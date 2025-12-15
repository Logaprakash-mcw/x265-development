/*****************************************************************************
* Copyright (C) 2013-2020 MulticoreWare, Inc
*
* Authors: Pooja Venkatesan <pooja@multicorewareinc.com>
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

#include "abrEncApp.h"
#include "mv.h"
#include "slice.h"
#include "param.h"

#include <signal.h>
#include <errno.h>

#include <queue>

using namespace X265_NS;

/* Ctrl-C handler */
static volatile sig_atomic_t b_ctrl_c /* = 0 */;
static void sigint_handler(int)
{
    b_ctrl_c = 1;
}

namespace X265_NS {
    // private namespace
#define X265_INPUT_QUEUE_SIZE 250

    AbrEncoder::AbrEncoder(CLIOptions cliopt[], uint8_t numEncodes, int &ret)
    {
        m_numEncodes = numEncodes;
        m_numActiveEncodes.set(numEncodes);
        m_queueSize = (numEncodes > 1) ? X265_INPUT_QUEUE_SIZE : 1;
        m_passEnc = X265_MALLOC(PassEncoder*, m_numEncodes);

        for (uint8_t i = 0; i < m_numEncodes; i++)
        {
            m_passEnc[i] = new PassEncoder(i, cliopt[i], this);
            if (!m_passEnc[i])
            {
                x265_log(X265_LOG_ERROR, "Unable to allocate memory for passEncoder\n");
                ret = 4;
            }
            m_passEnc[i]->init();
        }

        if (!allocBuffers())
        {
            x265_log(X265_LOG_ERROR, "Unable to allocate memory for buffers\n");
            ret = 4;
        }

        /* start passEncoder worker threads */
        for (uint8_t pass = 0; pass < m_numEncodes; pass++)
            m_passEnc[pass]->startThreads();
    }

    bool AbrEncoder::allocBuffers()
    {
        m_inputPicBuffer = X265_MALLOC(x265_picture**, m_numEncodes);
        m_denoisedInputPicBuffer = X265_MALLOC(x265_picture**, m_numEncodes);

        m_picWriteCnt = new ThreadSafeInteger[m_numEncodes];
        m_picReadCnt = new ThreadSafeInteger[m_numEncodes];

        m_picIdxReadCnt = X265_MALLOC(ThreadSafeInteger*, m_numEncodes);
        m_readFlag = X265_MALLOC(int*, m_numEncodes);

        for (uint8_t pass = 0; pass < m_numEncodes; pass++)
        {
            m_inputPicBuffer[pass] = X265_MALLOC(x265_picture*, m_queueSize);
            m_denoisedInputPicBuffer[pass] = X265_MALLOC(x265_picture*, m_queueSize);
            for (uint32_t idx = 0; idx < m_queueSize; idx++)
            {
                m_inputPicBuffer[pass][idx] = x265_picture_alloc();
                x265_picture_init(m_passEnc[pass]->m_param, m_inputPicBuffer[pass][idx]);

                m_denoisedInputPicBuffer[pass][idx] = x265_picture_alloc();
                x265_picture_init(m_passEnc[pass]->m_param, m_denoisedInputPicBuffer[pass][idx]);
            }

            m_picIdxReadCnt[pass] = new ThreadSafeInteger[m_queueSize];
            m_readFlag[pass] = X265_MALLOC(int, m_queueSize);
        }
        return true;
    //fail:
    //    return false;
    }

    void AbrEncoder::destroy()
    {
        x265_cleanup(); /* Free library singletons */
        for (uint8_t pass = 0; pass < m_numEncodes; pass++)
        {
            for (uint32_t index = 0; index < m_queueSize; index++)
            {
                X265_FREE(m_inputPicBuffer[pass][index]->planes[0]);
                x265_picture_free(m_inputPicBuffer[pass][index]);
                X265_FREE(m_denoisedInputPicBuffer[pass][index]->planes[0]);
                x265_picture_free(m_denoisedInputPicBuffer[pass][index]);
            }

            X265_FREE(m_inputPicBuffer[pass]);
            X265_FREE(m_denoisedInputPicBuffer[pass]);
            X265_FREE(m_readFlag[pass]);
            delete[] m_picIdxReadCnt[pass];
            m_passEnc[pass]->destroy();
            delete m_passEnc[pass];
        }
        X265_FREE(m_inputPicBuffer);
        X265_FREE(m_denoisedInputPicBuffer);
        X265_FREE(m_readFlag);

        delete[] m_picWriteCnt;
        delete[] m_picReadCnt;

        X265_FREE(m_picIdxReadCnt);

        X265_FREE(m_passEnc);
    }

    PassEncoder::PassEncoder(uint32_t id, CLIOptions cliopt, AbrEncoder *parent)
    {
        m_id = id;
        m_cliopt = cliopt;
        m_parent = parent;
        if(!(m_cliopt.enableScaler && m_id))
        {
            m_input = m_cliopt.input;
            m_denoisedInput = m_cliopt.denoisedInput;
        }
        m_param = cliopt.param;
        m_inputOver = false;
        m_lastIdx = -1;
        m_encoder = NULL;
        m_scaler = NULL;
        m_reader = NULL;
        m_ret = 0;
    }

    int PassEncoder::init()
    {

        m_reader = new Reader(m_id, this);


        /* note: we could try to acquire a different libx265 API here based on
        * the profile found during option parsing, but it must be done before
        * opening an encoder */

        if (m_param)
            m_encoder = m_cliopt.api->encoder_open(m_param);
        if (!m_encoder)
        {
            x265_log(X265_LOG_ERROR, "x265_encoder_open() failed for Enc, \n");
            m_ret = 2;
            return -1;
        }
        if (m_param->filmGrain)
        {
            m_cliopt.fgChar = x265_fopen(m_param->filmGrain, "wb");
            if (!m_cliopt.fgChar)
            {
                x265_log_file(NULL, X265_LOG_ERROR, "Failed to open film grain characteristics binary file %s\n", m_param->filmGrain);
            }
        }

        /* get the encoder parameters post-initialization */
        m_cliopt.api->encoder_parameters(m_encoder, m_param);

        return 1;
    }


    void PassEncoder::startThreads()
    {
        /* Start slave worker threads */
        m_threadActive = true;
        start();
        /* Start reader threads*/
        if (m_reader != NULL)
        {
            m_reader->m_threadActive = true;
            m_reader->start();
        }
    }

    

    bool PassEncoder::readPicture(x265_picture *dstPic, int denoised)
    {
        /*Check and wait if there any input frames to read*/
        int ipread = m_parent->m_picReadCnt[m_id].get();
        int ipwrite = m_parent->m_picWriteCnt[m_id].get();

        while (!m_inputOver && (ipread == ipwrite))
        {
            ipwrite = m_parent->m_picWriteCnt[m_id].waitForChange(ipwrite);
        }

        if (m_threadActive && ipread < ipwrite)
        {
            /*Get input index to read from inputQueue. If doesn't need analysis info, it need not wait to fetch poc from analysisQueue*/
            int readPos = ipread % m_parent->m_queueSize;
            x265_picture *srcPic;
            if(!denoised)
                srcPic = (x265_picture*)(m_parent->m_inputPicBuffer[m_id][readPos]);
            else
                srcPic = (x265_picture*)(m_parent->m_denoisedInputPicBuffer[m_id][readPos]);

            x265_picture *pic = (x265_picture*)(dstPic);
            pic->colorSpace = srcPic->colorSpace;
            pic->bitDepth = srcPic->bitDepth;
            pic->framesize = srcPic->framesize;
            pic->height = srcPic->height;
            pic->width = srcPic->width;
            pic->stride[0] = srcPic->stride[0];
            pic->stride[1] = srcPic->stride[1];
            pic->stride[2] = srcPic->stride[2];
            pic->planes[0] = srcPic->planes[0];
            pic->planes[1] = srcPic->planes[1];
            pic->planes[2] = srcPic->planes[2];
            return true;
        }
        else
            return false;
    }

    void PassEncoder::threadMain()
    {
        THREAD_NAME("PassEncoder", m_id);

        while (m_threadActive)
        {

#if ENABLE_LIBVMAF
            x265_vmaf_data* vmafdata = m_cliopt.vmafData;
#endif
            /* This allows muxers to modify bitstream format */
            //m_cliopt.output->setParam(m_param);
            const x265_api* api = m_cliopt.api;
            ReconPlay* reconPlay = NULL;
            if (m_cliopt.reconPlayCmd)
                reconPlay = new ReconPlay(m_cliopt.reconPlayCmd, *m_param);
            char* profileName = m_cliopt.encName ? m_cliopt.encName : (char *)"x265";

            if (signal(SIGINT, sigint_handler) == SIG_ERR)
                x265_log(X265_LOG_ERROR, "Unable to register CTRL+C handler: %s in %s\n",
                    strerror(errno), profileName);

            x265_picture pic_orig, pic_denoised, pic_out;
            x265_picture *pic_in = &pic_orig;
            x265_picture *pic_denoised_in = (m_param->bEnableTemporalFilter) ? NULL : &pic_denoised;
            /* Allocate recon picture if analysis save/load is enabled */
            //std::priority_queue<int64_t>* pts_queue = m_cliopt.output->needPTS() ? new std::priority_queue<int64_t>() : NULL;
            x265_picture *pic_recon = (m_param->bEnableTemporalFilter) ? &pic_out : NULL;
            uint32_t inFrameCount = 0;
            uint32_t outFrameCount = 0;
            int16_t *errorBuf = NULL;
            uint8_t *rpuPayload = NULL;
            int inputPicNum = 1;

            api->picture_init(m_param, &pic_orig);
            if (!m_param->bEnableTemporalFilter)
                api->picture_init(m_param, &pic_denoised);

            // main encoder loop
            while (pic_in && (m_param->bEnableTemporalFilter || pic_denoised_in) && !b_ctrl_c)
            {
                pic_orig.poc = inFrameCount;

                if (!m_param->bEnableTemporalFilter)
                    pic_denoised.poc = inFrameCount;

                if (m_cliopt.framesToBeEncoded && inFrameCount >= m_cliopt.framesToBeEncoded)
                {
                    pic_in = NULL;
                    pic_denoised_in = NULL;
                }
                else
                {
                    int ret1 = readPicture(pic_in, 0);           // original
                    int ret2 = 1;
                    if(pic_denoised_in)
                        ret2 = readPicture(pic_denoised_in, 1);  // denoised (optional)

                    if (ret1 && ret2)
                    {
                        inFrameCount++; // count only when both successfully read
                    }
                    else
                    {
                        // stop feeding if one stream ends
                        pic_in = NULL;
                        pic_denoised_in = NULL;
                    }
                }

                if (pic_in )
                {
                    if (pic_in->bitDepth > m_param->internalBitDepth && m_cliopt.bDither)
                    {
                        x265_dither_image(pic_in, m_cliopt.input->getWidth(), m_cliopt.input->getHeight(), errorBuf, m_param->internalBitDepth);
                        pic_in->bitDepth = m_param->internalBitDepth;
                    }
                }
                if(pic_denoised_in)
                {
                    if (pic_denoised_in->bitDepth > m_param->internalBitDepth && m_cliopt.bDither)
                    {
                        x265_dither_image(pic_denoised_in, m_cliopt.denoisedInput->getWidth(), m_cliopt.denoisedInput->getHeight(), errorBuf, m_param->internalBitDepth);
                        pic_denoised_in->bitDepth = m_param->internalBitDepth;
                    }
                }
                if (pic_in)
                {
                    for (int inputNum = 0; inputNum < inputPicNum; inputNum++)
                    {
                        x265_picture *picInput = NULL;
                        x265_picture *picDenoisedInput = NULL;
                        picInput = pic_in;

                        if (pic_denoised_in)
                            picDenoisedInput = pic_denoised_in;
                        int numEncoded = api->encoder_encode(m_encoder, picInput, picDenoisedInput, pic_recon);

                        int idx = (inFrameCount - 1) % m_parent->m_queueSize;
                        m_parent->m_picIdxReadCnt[m_id][idx].incr();
                        m_parent->m_picReadCnt[m_id].incr();

                        if (numEncoded < 0)
                        {
                            b_ctrl_c = 1;
                            m_ret = 4;
                            break;
                        }

                        outFrameCount += numEncoded;

                        if (numEncoded && pic_recon && m_cliopt.recon)
                        {
                            if(m_param->bEnableTemporalFilter)
                                m_cliopt.recon->writePicture(pic_out);
                            m_cliopt.writeFG(pic_out.m_fg);
                        }
                        m_cliopt.printStatus(outFrameCount);
                    }
                }
            }

            /* Flush the encoder */
            while (!b_ctrl_c)
            {
                int numEncoded = api->encoder_encode(m_encoder, NULL, NULL, pic_recon);
                if (numEncoded < 0)
                {
                    m_ret = 4;
                    break;
                }

                outFrameCount += numEncoded;

                if (numEncoded && pic_recon && m_cliopt.recon)
                {
                    m_cliopt.recon->writePicture(pic_out);
                    m_cliopt.writeFG(pic_out.m_fg);
                }

                m_cliopt.printStatus(outFrameCount);

                if (!numEncoded)
                    break;
            }

            /* clear progress report */
            if (m_cliopt.bProgress)
                fprintf(stderr, "%*s\r", 80, " ");


            delete reconPlay;

            api->encoder_close(m_encoder);

            general_log( NULL, X265_LOG_INFO, "Encoded %d frames\n",
                    outFrameCount, profileName);
            if (b_ctrl_c)
                general_log( NULL, X265_LOG_INFO, "aborted at input frame %d\n",
                    m_cliopt.seek + inFrameCount, profileName);

            api->param_free(m_param);

            X265_FREE(errorBuf);
            X265_FREE(rpuPayload);

            m_threadActive = false;
            m_parent->m_numActiveEncodes.decr();
        }
    }

    void PassEncoder::destroy()
    {
        stop();
        if (m_reader)
        {
            m_reader->stop();
            delete m_reader;
        }
    }

    Reader::Reader(int id, PassEncoder *parentEnc)
    {
        m_parentEnc = parentEnc;
        m_id = id;
        m_input = parentEnc->m_input;
        m_denoisedInput = parentEnc->m_denoisedInput;
    }

    void Reader::threadMain()
    {
        THREAD_NAME("Reader", m_id);

        int QDepth = m_parentEnc->m_parent->m_queueSize;
        x265_picture* src = x265_picture_alloc();
        x265_picture_init(m_parentEnc->m_param, src);
        x265_picture* denoised_src = NULL;
        if (!m_parentEnc->m_param->bEnableTemporalFilter)
        {
            denoised_src = x265_picture_alloc();
            x265_picture_init(m_parentEnc->m_param, denoised_src);
        }

        while (m_threadActive)
        {
            uint32_t written = m_parentEnc->m_parent->m_picWriteCnt[m_id].get();
            uint32_t writeIdx = written % QDepth;
            uint32_t read = m_parentEnc->m_parent->m_picIdxReadCnt[m_id][writeIdx].get();
            uint32_t overWritePicBuffer = written / QDepth;

            if (m_parentEnc->m_cliopt.framesToBeEncoded && written >= m_parentEnc->m_cliopt.framesToBeEncoded)
                break;

            while (overWritePicBuffer && read < overWritePicBuffer)
            {
                read = m_parentEnc->m_parent->m_picIdxReadCnt[m_id][writeIdx].waitForChange(read);
            }

            x265_picture* dest = m_parentEnc->m_parent->m_inputPicBuffer[m_id][writeIdx];
            x265_picture* denoised_dest = NULL;
            if (!m_parentEnc->m_param->bEnableTemporalFilter)
            {
                denoised_dest = m_parentEnc->m_parent->m_denoisedInputPicBuffer[m_id][writeIdx];
            }
            if (m_input->readPicture(*src))
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

                if (!dest->planes[0])
                    dest->planes[0] = X265_MALLOC(char, dest->framesize);

                memcpy(dest->planes[0], src->planes[0], src->framesize * sizeof(char));
                dest->planes[1] = (char*)dest->planes[0] + src->stride[0] * src->height;
                dest->planes[2] = (char*)dest->planes[1] + src->stride[1] * (src->height >> x265_cli_csps[src->colorSpace].height[1]);
                if (!m_parentEnc->m_param->bEnableTemporalFilter && m_denoisedInput->readPicture(*denoised_src))
                {
                    denoised_dest->poc = denoised_src->poc;
                    denoised_dest->bitDepth = denoised_src->bitDepth;
                    denoised_dest->framesize = denoised_src->framesize;
                    denoised_dest->height = denoised_src->height;
                    denoised_dest->width = denoised_src->width;
                    denoised_dest->colorSpace = denoised_src->colorSpace;
                    denoised_dest->stride[0] = denoised_src->stride[0];
                    denoised_dest->stride[1] = denoised_src->stride[1];
                    denoised_dest->stride[2] = denoised_src->stride[2];

                    if (!denoised_dest->planes[0])
                        denoised_dest->planes[0] = X265_MALLOC(char, denoised_dest->framesize);

                    memcpy(denoised_dest->planes[0], denoised_src->planes[0], denoised_src->framesize * sizeof(char));
                    denoised_dest->planes[1] = (char*)denoised_dest->planes[0] + denoised_src->stride[0] * denoised_src->height;
                    denoised_dest->planes[2] = (char*)denoised_dest->planes[1] + denoised_src->stride[1] * (denoised_src->height >> x265_cli_csps[denoised_src->colorSpace].height[1]);
                }

                m_parentEnc->m_parent->m_picWriteCnt[m_id].incr();
            }
            else
            {
                m_threadActive = false;
                m_parentEnc->m_inputOver = true;
                m_parentEnc->m_parent->m_picWriteCnt[m_id].poke();
            }
        }
        x265_picture_free(src);
        x265_picture_free(denoised_src);
    }
}
