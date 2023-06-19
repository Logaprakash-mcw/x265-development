/*****************************************************************************
 * Copyright (C) 2013-2020 MulticoreWare, Inc
 *
 * Authors: Steve Borho <steve@borho.org>
 *          Min Chen <chenm003@163.com>
 *          Praveen Kumar Tiwari <praveen@multicorewareinc.com>
 *          Aruna Matheswaran <aruna@multicorewareinc.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111, USA.
 *
 * This program is also available under a commercial proprietary license.
 * For more information, contact us at license @ x265.com.
 *****************************************************************************/

#include "common.h"
#include "primitives.h"
#include "threadpool.h"
#include "param.h"
#include "frame.h"
#include "framedata.h"
#include "picyuv.h"

#include "encoder.h"
#include "slicetype.h"
#include "frameencoder.h"
#include "x265.h"
#include "dpb.h"

#if _MSC_VER
#pragma warning(disable: 4996) // POSIX functions are just fine, thanks
#endif

using namespace X265_NS;

Encoder::Encoder()
{
    m_aborted = false;
    m_reconfigure = false;
    m_reconfigureRc = false;
    m_encodedFrameNum = 0;
    m_pocLast = -1;
    m_curEncoder = 0;

    m_lookahead = NULL;
    m_dpb = NULL;
    m_exportedPic = NULL;
    m_numDelayedPic = 0;
    m_outputCount = 0;
    m_param = NULL;
    m_latestParam = NULL;
    m_threadPool = NULL;
    for (int i = 0; i < X265_MAX_FRAME_THREADS; i++)
        m_frameEncoder[i] = NULL;

    m_origPicBuffer = 0;
}

inline char *strcatFilename(const char *input, const char *suffix)
{
    char *output = X265_MALLOC(char, strlen(input) + strlen(suffix) + 1);
    if (!output)
    {
        x265_log(X265_LOG_ERROR, "unable to allocate memory for filename\n");
        return NULL;
    }
    strcpy(output, input);
    strcat(output, suffix);
    return output;
}

void Encoder::create()
{
    if (!primitives.pu[0].sad)
    {
        // this should be an impossible condition when using our public API, and indicates a serious bug.
        x265_log( X265_LOG_ERROR, "Primitives must be initialized before encoder is created\n");
        abort();
    }

    x265_param* p = m_param;

    int rows = (p->sourceHeight + p->maxCUSize - 1) >> g_log2Size[p->maxCUSize];
    int cols = (p->sourceWidth  + p->maxCUSize - 1) >> g_log2Size[p->maxCUSize];

    // Do not allow WPP if only one row or fewer than 3 columns, it is pointless and unstable
    if (rows == 1 || cols < 3)
    {
        x265_log(X265_LOG_WARNING, "Too few rows/columns, --wpp disabled\n");
        p->bEnableWavefront = 0;
    }

    bool allowPools = !p->numaPools || strcmp(p->numaPools, "none");

    m_numPools = 0;
    if (allowPools)
        m_threadPool = ThreadPool::allocThreadPools(p, m_numPools, 0);

    for (int i = 0; i < m_param->frameNumThreads; i++)
    {
        m_frameEncoder[i] = new FrameEncoder;
    }

    if (m_numPools)
    {
        for (int i = 0; i < m_param->frameNumThreads; i++)
        {
            int pool = i % m_numPools;
            m_frameEncoder[i]->m_pool = &m_threadPool[pool];
            m_frameEncoder[i]->m_jpId = m_threadPool[pool].m_numProviders++;
            m_threadPool[pool].m_jpTable[m_frameEncoder[i]->m_jpId] = m_frameEncoder[i];
        }
        for (int i = 0; i < m_numPools; i++)
            m_threadPool[i].start();
    }
    else
    {
        /* CU stats and noise-reduction buffers are indexed by jpId, so it cannot be left as -1 */
        for (int i = 0; i < m_param->frameNumThreads; i++)
            m_frameEncoder[i]->m_jpId = 0;
    }

    int pools = m_numPools;
    ThreadPool* lookAheadThreadPool = 0;
    lookAheadThreadPool = m_threadPool;
    m_lookahead = new Lookahead(m_param, lookAheadThreadPool);
    if (pools)
    {
        m_lookahead->m_jpId = lookAheadThreadPool[0].m_numProviders++;
        lookAheadThreadPool[0].m_jpTable[m_lookahead->m_jpId] = m_lookahead;
    }

    m_dpb = new DPB();
    m_origPicBuffer = new OrigPicBuffer();

    int numRows = (m_param->sourceHeight + m_param->maxCUSize - 1) / m_param->maxCUSize;
    int numCols = (m_param->sourceWidth  + m_param->maxCUSize - 1) / m_param->maxCUSize;
    if (m_param->filmGrain)
    {
        m_filmGrainIn = x265_fopen(m_param->filmGrain, "wb");
        if (!m_filmGrainIn)
        {
            x265_log_file(NULL, X265_LOG_ERROR, "Failed to open film grain characteristics binary file %s\n", m_param->filmGrain);
        }
    }
    for (int i = 0; i < m_param->frameNumThreads; i++)
    {
        if (!m_frameEncoder[i]->init(this, numRows, numCols))
        {
            x265_log(X265_LOG_ERROR, "Unable to initialize frame encoder, aborting\n");
            m_aborted = true;
        }
    }

    for (int i = 0; i < m_param->frameNumThreads; i++)
    {
        m_frameEncoder[i]->start();
        m_frameEncoder[i]->m_done.wait(); /* wait for thread to initialize */
    }

    if (!m_lookahead->create())
        m_aborted = true;

    m_bZeroLatency = !m_param->lookaheadDepth && m_param->frameNumThreads == 1;

    m_encodeStartTime = x265_mdate();

}

void Encoder::stopJobs()
{

    if (m_lookahead)
        m_lookahead->stopJobs();
    
    for (int i = 0; i < m_param->frameNumThreads; i++)
    {
        if (m_frameEncoder[i])
        {
            m_frameEncoder[i]->getEncodedPicture();
            m_frameEncoder[i]->m_threadActive = false;
            m_frameEncoder[i]->m_enable.trigger();
            m_frameEncoder[i]->stop();
        }
    }

    if (m_threadPool)
    {
        for (int i = 0; i < m_numPools; i++)
            m_threadPool[i].stopWorkers();
    }
}

void Encoder::destroy()
{
    if (m_filmGrainIn)
        x265_fclose(m_filmGrainIn);
    if (m_exportedPic)
    {
        ATOMIC_DEC(&m_exportedPic->m_countRefEncoders);
        m_exportedPic = NULL;
    }

    for (int i = 0; i < m_param->frameNumThreads; i++)
    {
        if (m_frameEncoder[i])
        {
            m_frameEncoder[i]->destroy();
            delete m_frameEncoder[i];
        }
    }

    // thread pools can be cleaned up now that all the JobProviders are
    // known to be shutdown
    delete [] m_threadPool;

    if (m_lookahead)
    {
        m_lookahead->destroy();
        delete m_lookahead;
    }

    delete m_origPicBuffer;

    if (m_latestParam != NULL && m_latestParam != m_param)
    {
        PARAM_NS::x265_param_free(m_latestParam);
    }
    if (m_param)
    {
        PARAM_NS::x265_param_free(m_param);
    }
}




void Encoder::copyPicture(x265_picture *dest, const x265_picture *src)
{
    dest->poc = src->poc;
    dest->bitDepth = src->bitDepth;
    dest->framesize = src->framesize;
    dest->height = src->height;
    dest->width = src->width;
    dest->colorSpace = src->colorSpace;
    dest->stride[0] = src->stride[0];
    dest->stride[1] = src->stride[1];
    dest->stride[2] = src->stride[2];
    memcpy(dest->planes[0], src->planes[0], src->framesize * sizeof(char));
    dest->planes[1] = (char*)dest->planes[0] + src->stride[0] * src->height;
    dest->planes[2] = (char*)dest->planes[1] + src->stride[1] * (src->height >> x265_cli_csps[src->colorSpace].height[1]);
}

inline int enqueueRefFrame(FrameEncoder* curframeEncoder, Frame* iterFrame, Frame* curFrame, bool isPreFiltered, int16_t i)
{
    TemporalFilterRefPicInfo* dest = &curframeEncoder->m_mcstfRefList[curFrame->m_mcstf->m_numRef];
    dest->picBuffer = iterFrame->m_fencPic;
    dest->picBufferSubSampled2 = iterFrame->m_fencPicSubsampled2;
    dest->picBufferSubSampled4 = iterFrame->m_fencPicSubsampled4;
    dest->isFilteredFrame = isPreFiltered;
    dest->isSubsampled = iterFrame->m_isSubSampled;
    dest->origOffset = i;
    curFrame->m_mcstf->m_numRef++;

    return 1;
}

bool Encoder::generateMcstfRef(Frame* frameEnc, FrameEncoder* currEncoder)
{
    frameEnc->m_mcstf->m_numRef = 0;

    for (int iterPOC = (frameEnc->m_poc - frameEnc->m_mcstf->m_range);
        iterPOC <= (frameEnc->m_poc + frameEnc->m_mcstf->m_range); iterPOC++)
    {
        bool isFound = false;
        if (iterPOC != frameEnc->m_poc)
        {
            //search for the reference frame in the Original Picture Buffer
            if (!isFound)
            {
                for (int j = 0; j < (2 * frameEnc->m_mcstf->m_range); j++)
                {
                    if (iterPOC < 0)
                        continue;
                    if (iterPOC >= m_pocLast)
                    {

                        TemporalFilter* mcstf = frameEnc->m_mcstf;
                        while (mcstf->m_numRef)
                        {
                            memset(currEncoder->m_mcstfRefList[mcstf->m_numRef].mvs0,  0, sizeof(MV) * ((mcstf->m_sourceWidth / 16) * (mcstf->m_sourceHeight / 16)));
                            memset(currEncoder->m_mcstfRefList[mcstf->m_numRef].mvs1,  0, sizeof(MV) * ((mcstf->m_sourceWidth / 16) * (mcstf->m_sourceHeight / 16)));
                            memset(currEncoder->m_mcstfRefList[mcstf->m_numRef].mvs2,  0, sizeof(MV) * ((mcstf->m_sourceWidth / 16) * (mcstf->m_sourceHeight / 16)));
                            memset(currEncoder->m_mcstfRefList[mcstf->m_numRef].mvs,   0, sizeof(MV) * ((mcstf->m_sourceWidth /  4) * (mcstf->m_sourceHeight /  4)));
                            memset(currEncoder->m_mcstfRefList[mcstf->m_numRef].noise, 0, sizeof(int) * ((mcstf->m_sourceWidth / 4) * (mcstf->m_sourceHeight / 4)));
                            memset(currEncoder->m_mcstfRefList[mcstf->m_numRef].error, 0, sizeof(int) * ((mcstf->m_sourceWidth / 4) * (mcstf->m_sourceHeight / 4)));

                            mcstf->m_numRef--;
                        }

                        break;
                    }
                    Frame* iterFrame = frameEnc->m_encData->m_slice->m_mcstfRefFrameList[1][j];
                    if (iterFrame->m_poc == iterPOC)
                    {
                        if (!enqueueRefFrame(currEncoder, iterFrame, frameEnc, false, (int16_t)(iterPOC - frameEnc->m_poc)))
                        {
                            return false;
                        };
                        break;
                    }
                }
            }
        }
    }

    return true;
}

/**
 * Feed one new input frame into the encoder, get one frame out. If pic_in is
 * NULL, a flush condition is implied and pic_in must be NULL for all subsequent
 * calls for this encoder instance.
 *
 * pic_in  input original YUV picture or NULL
 * pic_out pointer to reconstructed picture struct
 *
 * returns 0 if no frames are currently available for output
 *         1 if frame was output, m_nalList contains access unit
 *         negative on malloc error or abort */
int Encoder::encode(const x265_picture* pic_in, x265_picture* pic_out)
{
#if CHECKED_BUILD || _DEBUG
    if (g_checkFailures)
    {
        x265_log(X265_LOG_ERROR, "encoder aborting because of internal error\n");
        return -1;
    }
#endif
    if (m_aborted)
        return -1;

    const x265_picture* inputPic = NULL;
    static int written = 0, read = 0;

    if (m_exportedPic)
    {
        ATOMIC_DEC(&m_exportedPic->m_countRefEncoders);
        m_origPicBuffer->recycleOrigPicList();
    }

    if ((pic_in) || (!pic_in && (read < written)))
    {

        inputPic = pic_in;

        Frame *inFrame;
        x265_param *p = m_param;
        if (m_dpb->m_freeList.empty())
        {
            inFrame = new Frame;
            inFrame->m_encodeStartTime = x265_mdate();
            if (inFrame->create(p))
            {
                /* the first PicYuv created is asked to generate the CU and block unit offset
                 * arrays which are then shared with all subsequent PicYuv (orig and recon) 
                 * allocated by this top level encoder */
                if (m_sps.cuOffsetY)
                {
                    inFrame->m_fencPic->m_cuOffsetY = m_sps.cuOffsetY;
                    inFrame->m_fencPic->m_buOffsetY = m_sps.buOffsetY;
                    if (m_param->internalCsp != X265_CSP_I400)
                    {
                        inFrame->m_fencPic->m_cuOffsetC = m_sps.cuOffsetC;
                        inFrame->m_fencPic->m_buOffsetC = m_sps.buOffsetC;
                    }
                }
                else
                {
                    if (!inFrame->m_fencPic->createOffsets(m_sps))
                    {
                        m_aborted = true;
                        x265_log(X265_LOG_ERROR, "memory allocation failure, aborting encode\n");
                        inFrame->destroy();
                        delete inFrame;
                        return -1;
                    }
                    else
                    {
                        m_sps.cuOffsetY = inFrame->m_fencPic->m_cuOffsetY;
                        m_sps.buOffsetY = inFrame->m_fencPic->m_buOffsetY;
                        if (m_param->internalCsp != X265_CSP_I400)
                        {
                            m_sps.cuOffsetC = inFrame->m_fencPic->m_cuOffsetC;
                            m_sps.cuOffsetY = inFrame->m_fencPic->m_cuOffsetY;
                            m_sps.buOffsetC = inFrame->m_fencPic->m_buOffsetC;
                            m_sps.buOffsetY = inFrame->m_fencPic->m_buOffsetY;
                        }
                    }
                }
            }
            else
            {
                m_aborted = true;
                x265_log(X265_LOG_ERROR, "memory allocation failure, aborting encode\n");
                inFrame->destroy();
                delete inFrame;
                return -1;
            }
        }
        else
        {
            inFrame = m_dpb->m_freeList.popBack();
            inFrame->m_encodeStartTime = x265_mdate();
        }

        /* Copy input picture into a Frame and PicYuv, send to lookahead */
        inFrame->m_fencPic->copyFromPicture(*inputPic, *m_param, m_sps.conformanceWindow.rightOffset, m_sps.conformanceWindow.bottomOffset);

        inFrame->m_poc       = ++m_pocLast;
        inFrame->m_param     = m_param;

        /* Encoder holds a reference count until stats collection is finished */
        ATOMIC_INC(&inFrame->m_countRefEncoders);

        if (!m_pocLast)
        {
            /*One shot allocation of frames in OriginalPictureBuffer*/
            int numFramesinOPB = X265_MAX(0, (inFrame->m_mcstf->m_range << 1)) + 1;
            for (int i = 0; i < numFramesinOPB; i++)
            {
                Frame* dupFrame = new Frame;
                if (!(dupFrame->create(m_param)))
                {
                    m_aborted = true;
                    x265_log(X265_LOG_ERROR, "Memory allocation failure, aborting encode\n");
                    fflush(stderr);
                    dupFrame->destroy();
                    delete dupFrame;
                    return -1;
                }
                else
                {
                    if (m_sps.cuOffsetY)
                    {
                        dupFrame->m_fencPic->m_cuOffsetC = m_sps.cuOffsetC;
                        dupFrame->m_fencPic->m_buOffsetC = m_sps.buOffsetC;
                        dupFrame->m_fencPic->m_cuOffsetY = m_sps.cuOffsetY;
                        dupFrame->m_fencPic->m_buOffsetY = m_sps.buOffsetY;
                        if (m_param->internalCsp != X265_CSP_I400)
                        {
                            dupFrame->m_fencPic->m_cuOffsetC = m_sps.cuOffsetC;
                            dupFrame->m_fencPic->m_buOffsetC = m_sps.buOffsetC;
                        }
                        m_origPicBuffer->addEncPicture(dupFrame);
                    }
                }
            }
        }

        inFrame->m_refPicCnt[1] = 2 * inFrame->m_mcstf->m_range + 1;
        if (inFrame->m_poc < inFrame->m_mcstf->m_range)
            inFrame->m_refPicCnt[1] -= (uint8_t)(inFrame->m_mcstf->m_range - inFrame->m_poc);
        if (m_param->totalFrames && (inFrame->m_poc >= (m_param->totalFrames - inFrame->m_mcstf->m_range)))
            inFrame->m_refPicCnt[1] -= (uint8_t)(inFrame->m_poc + inFrame->m_mcstf->m_range - m_param->totalFrames + 1);

        //Extend full-res original picture border
        PicYuv *orig = inFrame->m_fencPic;
        extendPicBorder(orig->m_picOrg[0], orig->m_stride, orig->m_picWidth, orig->m_picHeight, orig->m_lumaMarginX, orig->m_lumaMarginY);
        extendPicBorder(orig->m_picOrg[1], orig->m_strideC, orig->m_picWidth >> orig->m_hChromaShift, orig->m_picHeight >> orig->m_vChromaShift, orig->m_chromaMarginX, orig->m_chromaMarginY);
        extendPicBorder(orig->m_picOrg[2], orig->m_strideC, orig->m_picWidth >> orig->m_hChromaShift, orig->m_picHeight >> orig->m_vChromaShift, orig->m_chromaMarginX, orig->m_chromaMarginY);

        //TODO: Add subsampling here if required
        m_origPicBuffer->addPicture(inFrame);


        m_lookahead->addPicture(*inFrame);
        m_numDelayedPic++;
    }
    else
        m_lookahead->flush();

    FrameEncoder *curEncoder = m_frameEncoder[m_curEncoder];
    m_curEncoder = (m_curEncoder + 1) % m_param->frameNumThreads;
    int ret = 0;

    /* Normal operation is to wait for the current frame encoder to complete its current frame
     * and then to give it a new frame to work on.  In zero-latency mode, we must encode this
     * input picture before returning so the order must be reversed. This do/while() loop allows
     * us to alternate the order of the calls without ugly code replication */
    Frame* outFrame = NULL;
    Frame* frameEnc = NULL;
    int pass = 0;
    do
    {
        /* getEncodedPicture() should block until the FrameEncoder has completed
         * encoding the frame.  This is how back-pressure through the API is
         * accomplished when the encoder is full */
        if (!m_bZeroLatency || pass)
            outFrame = curEncoder->getEncodedPicture();
        if (outFrame)
        {
            if (pic_out)
            {
                PicYuv *recpic = outFrame->m_fencPic;
                pic_out->poc = outFrame->m_poc;
                pic_out->bitDepth = X265_DEPTH;
                pic_out->colorSpace = m_param->internalCsp;
                pic_out->planes[0] = recpic->m_picOrg[0];
                pic_out->stride[0] = (int)(recpic->m_stride * sizeof(pixel));
                if (m_param->internalCsp != X265_CSP_I400)
                {
                    pic_out->planes[1] = recpic->m_picOrg[1];
                    pic_out->stride[1] = (int)(recpic->m_strideC * sizeof(pixel));
                    pic_out->planes[2] = recpic->m_picOrg[2];
                    pic_out->stride[2] = (int)(recpic->m_strideC * sizeof(pixel));
                }
            }

            if (m_aborted)
                return -1;


            Frame *curFrame = m_origPicBuffer->m_mcstfPicList.getPOCMCSTF(outFrame->m_poc);
            X265_CHECK(curFrame, "Outframe not found in DPB's mcstfPicList");
            curFrame->m_refPicCnt[0]--;
            curFrame->m_refPicCnt[1]--;
            curFrame = m_origPicBuffer->m_mcstfOrigPicList.getPOCMCSTF(outFrame->m_poc);
            X265_CHECK(curFrame, "Outframe not found in OPB's mcstfOrigPicList");
            curFrame->m_refPicCnt[1]--;


            /* Allow this frame to be recycled if no frame encoders are using it for reference */
            if (!pic_out)
            {
                ATOMIC_DEC(&outFrame->m_countRefEncoders);
                m_origPicBuffer->recycleOrigPicList();
            }
            else
                m_exportedPic = outFrame;

            m_outputCount++;
            m_numDelayedPic--;

            ret = 1;
        }

        /* pop a single frame from decided list, then provide to frame encoder
         * curEncoder is guaranteed to be idle at this point */
        if (!pass)
            frameEnc = m_lookahead->getDecidedPicture();
        if (frameEnc && !pass)
        {
            frameEnc->allocEncodeData(m_reconfigure ? m_latestParam : m_param, m_sps);

            X265_CHECK(!m_origPicBuffer->m_mcstfOrigPicFreeList.empty(), "Frames not available in Encoded OPB");

            Frame *dupFrame = m_origPicBuffer->m_mcstfOrigPicFreeList.popBackMCSTF();
            dupFrame->m_fencPic->copyFromFrame(frameEnc->m_fencPic);
            dupFrame->m_poc = frameEnc->m_poc;
            dupFrame->m_encodeOrder = frameEnc->m_encodeOrder;
            dupFrame->m_refPicCnt[1] = 2 * dupFrame->m_mcstf->m_range + 1;

            if (dupFrame->m_poc < dupFrame->m_mcstf->m_range)
                dupFrame->m_refPicCnt[1] -= (uint8_t)(dupFrame->m_mcstf->m_range - dupFrame->m_poc);
            if (m_param->totalFrames && (dupFrame->m_poc >= (m_param->totalFrames - dupFrame->m_mcstf->m_range)))
                dupFrame->m_refPicCnt[1] -= (uint8_t)(dupFrame->m_poc + dupFrame->m_mcstf->m_range - m_param->totalFrames + 1);

            m_origPicBuffer->addEncPictureToPicList(dupFrame);
            m_origPicBuffer->setOrigPicList(frameEnc, m_pocLast);

            if (!generateMcstfRef(frameEnc, curEncoder))
            {
                m_aborted = true;
                x265_log(X265_LOG_ERROR, "Failed to initialize MCSTFReferencePicInfo at POC %d\n", frameEnc->m_poc);
                fflush(stderr);
                return -1;
            }


            if (!*frameEnc->m_isSubSampled)
            {
                primitives.frameSubSampleLuma((const pixel *)frameEnc->m_fencPic->m_picOrg[0],frameEnc->m_fencPicSubsampled2->m_picOrg[0], frameEnc->m_fencPic->m_stride, frameEnc->m_fencPicSubsampled2->m_stride, frameEnc->m_fencPicSubsampled2->m_picWidth, frameEnc->m_fencPicSubsampled2->m_picHeight);
                extendPicBorder(frameEnc->m_fencPicSubsampled2->m_picOrg[0], frameEnc->m_fencPicSubsampled2->m_stride, frameEnc->m_fencPicSubsampled2->m_picWidth, frameEnc->m_fencPicSubsampled2->m_picHeight, frameEnc->m_fencPicSubsampled2->m_lumaMarginX, frameEnc->m_fencPicSubsampled2->m_lumaMarginY);
                primitives.frameSubSampleLuma((const pixel *)frameEnc->m_fencPicSubsampled2->m_picOrg[0],frameEnc->m_fencPicSubsampled4->m_picOrg[0], frameEnc->m_fencPicSubsampled2->m_stride, frameEnc->m_fencPicSubsampled4->m_stride, frameEnc->m_fencPicSubsampled4->m_picWidth, frameEnc->m_fencPicSubsampled4->m_picHeight);
                extendPicBorder(frameEnc->m_fencPicSubsampled4->m_picOrg[0], frameEnc->m_fencPicSubsampled4->m_stride, frameEnc->m_fencPicSubsampled4->m_picWidth, frameEnc->m_fencPicSubsampled4->m_picHeight, frameEnc->m_fencPicSubsampled4->m_lumaMarginX, frameEnc->m_fencPicSubsampled4->m_lumaMarginY);
                *frameEnc->m_isSubSampled = true;
            }

            for (uint8_t i = 1; i <= frameEnc->m_mcstf->m_numRef; i++)
            {
                TemporalFilterRefPicInfo *ref = &curEncoder->m_mcstfRefList[i - 1];
                if (!*ref->isSubsampled)
                {
                    primitives.frameSubSampleLuma((const pixel *)ref->picBuffer->m_picOrg[0], ref->picBufferSubSampled2->m_picOrg[0], ref->picBuffer->m_stride, ref->picBufferSubSampled2->m_stride, ref->picBufferSubSampled2->m_picWidth, ref->picBufferSubSampled2->m_picHeight);
                    extendPicBorder(ref->picBufferSubSampled2->m_picOrg[0], ref->picBufferSubSampled2->m_stride, ref->picBufferSubSampled2->m_picWidth, ref->picBufferSubSampled2->m_picHeight, ref->picBufferSubSampled2->m_lumaMarginX, ref->picBufferSubSampled2->m_lumaMarginY);
                    primitives.frameSubSampleLuma((const pixel *)ref->picBufferSubSampled2->m_picOrg[0],ref->picBufferSubSampled4->m_picOrg[0], ref->picBufferSubSampled2->m_stride, ref->picBufferSubSampled4->m_stride, ref->picBufferSubSampled4->m_picWidth, ref->picBufferSubSampled4->m_picHeight);
                    extendPicBorder(ref->picBufferSubSampled4->m_picOrg[0], ref->picBufferSubSampled4->m_stride, ref->picBufferSubSampled4->m_picWidth, ref->picBufferSubSampled4->m_picHeight, ref->picBufferSubSampled4->m_lumaMarginX, ref->picBufferSubSampled4->m_lumaMarginY);
                    *ref->isSubsampled = true;
                }
            }

            for (uint8_t i = 1; i <= frameEnc->m_mcstf->m_numRef; i++)
            {
                TemporalFilterRefPicInfo *ref = &curEncoder->m_mcstfRefList[i - 1];

                curEncoder->m_frameEncTF->motionEstimationLuma(ref->mvs0, ref->mvsStride0, frameEnc->m_fencPicSubsampled4, ref->picBufferSubSampled4, 16);
                curEncoder->m_frameEncTF->motionEstimationLuma(ref->mvs1, ref->mvsStride1, frameEnc->m_fencPicSubsampled2, ref->picBufferSubSampled2, 16, ref->mvs0, ref->mvsStride0, 2);
                curEncoder->m_frameEncTF->motionEstimationLuma(ref->mvs2, ref->mvsStride2, frameEnc->m_fencPic, ref->picBuffer, 16, ref->mvs1, ref->mvsStride1, 2);
                curEncoder->m_frameEncTF->motionEstimationLumaDoubleRes(ref->mvs,  ref->mvsStride, frameEnc->m_fencPic, ref->picBuffer, 8, ref->mvs2, ref->mvsStride2, 1, ref->error);
            }


            /* Allow FrameEncoder::compressFrame() to start in the frame encoder thread */
            if (!curEncoder->startCompressFrame(frameEnc))
                m_aborted = true;
        }
    }
    while (m_bZeroLatency && ++pass < 2);

    return ret;
}

#if defined(_MSC_VER)
#pragma warning(disable: 4800) // forcing int to bool
#pragma warning(disable: 4127) // conditional expression is constant
#endif

void Encoder::configure(x265_param *p)
{
    this->m_param = p;
}

