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

#include "bitcost.h"
#include "encoder.h"
#include "slicetype.h"
#include "frameencoder.h"
#include "ratecontrol.h"
#include "dpb.h"
#include "nal.h"

#include "x265.h"

#if _MSC_VER
#pragma warning(disable: 4996) // POSIX functions are just fine, thanks
#endif

namespace X265_NS {
const char g_sliceTypeToChar[] = {'B', 'P', 'I'};

/* Dolby Vision profile specific settings */
typedef struct
{
    int bEmitHRDSEI;
    int bEnableVideoSignalTypePresentFlag;
    int bEnableColorDescriptionPresentFlag;
    int bEnableAccessUnitDelimiters;
    int bAnnexB;

    /* VUI parameters specific to Dolby Vision Profile */
    int videoFormat;
    int bEnableVideoFullRangeFlag;
    int transferCharacteristics;
    int colorPrimaries;
    int matrixCoeffs;

    int doviProfileId;
}DolbyVisionProfileSpec;

DolbyVisionProfileSpec dovi[] =
{
    { 1, 1, 1, 1, 1, 5, 1,  2, 2, 2, 50 },
    { 1, 1, 1, 1, 1, 5, 0, 16, 9, 9, 81 },
    { 1, 1, 1, 1, 1, 5, 0,  1, 1, 1, 82 },
    { 1, 1, 1, 1, 1, 5, 0, 18, 9, 9, 84 }
};

typedef struct
{
    int bEnableVideoSignalTypePresentFlag;
    int bEnableColorDescriptionPresentFlag;
    int bEnableChromaLocInfoPresentFlag;
    int colorPrimaries;
    int transferCharacteristics;
    int matrixCoeffs;
    int bEnableVideoFullRangeFlag;
    int chromaSampleLocTypeTopField;
    int chromaSampleLocTypeBottomField;
    const char* systemId;
}VideoSignalTypePresets;

VideoSignalTypePresets vstPresets[] =
{
    {1, 1, 1, 6, 6, 6, 0, 0, 0, "BT601_525"},
    {1, 1, 1, 5, 6, 5, 0, 0, 0, "BT601_626"},
    {1, 1, 1, 1, 1, 1, 0, 0, 0, "BT709_YCC"},
    {1, 1, 0, 1, 1, 0, 0, 0, 0, "BT709_RGB"},
    {1, 1, 1, 9, 14, 1, 0, 2, 2, "BT2020_YCC_NCL"},
    {1, 1, 0, 9, 16, 9, 0, 0, 0, "BT2020_RGB"},
    {1, 1, 1, 9, 16, 9, 0, 2, 2, "BT2100_PQ_YCC"},
    {1, 1, 1, 9, 16, 14, 0, 2, 2, "BT2100_PQ_ICTCP"},
    {1, 1, 0, 9, 16, 0, 0, 0, 0, "BT2100_PQ_RGB"},
    {1, 1, 1, 9, 18, 9, 0, 2, 2, "BT2100_HLG_YCC"},
    {1, 1, 0, 9, 18, 0, 0, 0, 0, "BT2100_HLG_RGB"},
    {1, 1, 0, 1, 1, 0, 1, 0, 0, "FR709_RGB"},
    {1, 1, 0, 9, 14, 0, 1, 0, 0, "FR2020_RGB"},
    {1, 1, 1, 12, 1, 6, 1, 1, 1, "FRP3D65_YCC"}
};
}

/* Threshold for motion vection, based on expermental result.
 * TODO: come up an algorithm for adoptive threshold */
#define MVTHRESHOLD (10*10)
#define PU_2Nx2N 1
#define MAX_CHROMA_QP_OFFSET 12
#define CONF_OFFSET_BYTES (2 * sizeof(int))
static const char* defaultAnalysisFileName = "x265_analysis.dat";

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
    m_rateControl = NULL;
    m_dpb = NULL;
    m_exportedPic = NULL;
    m_numDelayedPic = 0;
    m_outputCount = 0;
    m_param = NULL;
    m_latestParam = NULL;
    m_threadPool = NULL;
    m_iFrameNum = 0;
    m_iPPSQpMinus26 = 0;
    m_rpsInSpsCount = 0;
    m_cB = 1.0;
    m_cR = 1.0;
    for (int i = 0; i < X265_MAX_FRAME_THREADS; i++)
        m_frameEncoder[i] = NULL;
    for (uint32_t i = 0; i < DUP_BUFFER; i++)
        m_dupBuffer[i] = NULL;
    MotionEstimate::initScales();

#if ENABLE_HDR10_PLUS
    m_hdr10plus_api = hdr10plus_api_get();
    m_numCimInfo = 0;
    m_cim = NULL;
#endif

#if SVT_HEVC
    m_svtAppData = NULL;
#endif
    m_prevTonemapPayload.payload = NULL;
    m_startPoint = 0;
    m_saveCTUSize = 0;
    m_zoneIndex = 0;
    m_origPicBuffer = 0;
}

inline char *strcatFilename(const char *input, const char *suffix)
{
    char *output = X265_MALLOC(char, strlen(input) + strlen(suffix) + 1);
    if (!output)
    {
        x265_log(NULL, X265_LOG_ERROR, "unable to allocate memory for filename\n");
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
        x265_log(m_param, X265_LOG_ERROR, "Primitives must be initialized before encoder is created\n");
        abort();
    }

    x265_param* p = m_param;

    int rows = (p->sourceHeight + p->maxCUSize - 1) >> g_log2Size[p->maxCUSize];
    int cols = (p->sourceWidth  + p->maxCUSize - 1) >> g_log2Size[p->maxCUSize];

    // Do not allow WPP if only one row or fewer than 3 columns, it is pointless and unstable
    if (rows == 1 || cols < 3)
    {
        x265_log(p, X265_LOG_WARNING, "Too few rows/columns, --wpp disabled\n");
        p->bEnableWavefront = 0;
    }

    bool allowPools = !p->numaPools || strcmp(p->numaPools, "none");

    // Trim the thread pool if --wpp, --pme, and --pmode are disabled
    if (!p->bEnableWavefront && !p->bDistributeModeAnalysis && !p->bDistributeMotionEstimation && !p->lookaheadSlices)
        allowPools = false;

    m_numPools = 0;
    if (allowPools)
        m_threadPool = ThreadPool::allocThreadPools(p, m_numPools, 0);
    else
    {
        if (!p->frameNumThreads)
        {
            // auto-detect frame threads
            int cpuCount = ThreadPool::getCpuCount();
            ThreadPool::getFrameThreadsCount(p, cpuCount);
        }
    }

    for (int i = 0; i < m_param->frameNumThreads; i++)
    {
        m_frameEncoder[i] = new FrameEncoder;
        m_frameEncoder[i]->m_nalList.m_annexB = !!m_param->bAnnexB;
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
    if (m_param->lookaheadThreads > 0)
    {
        lookAheadThreadPool = ThreadPool::allocThreadPools(p, pools, 1);
    }
    else
        lookAheadThreadPool = m_threadPool;
    m_lookahead = new Lookahead(m_param, lookAheadThreadPool);
    if (pools)
    {
        m_lookahead->m_jpId = lookAheadThreadPool[0].m_numProviders++;
        lookAheadThreadPool[0].m_jpTable[m_lookahead->m_jpId] = m_lookahead;
    }
    if (m_param->lookaheadThreads > 0)
        for (int i = 0; i < pools; i++)
            lookAheadThreadPool[i].start();
    m_lookahead->m_numPools = pools;
    m_dpb = new DPB(m_param);

    if (m_param->bEnableTemporalFilter)
        m_origPicBuffer = new OrigPicBuffer();

    m_rateControl = new RateControl(*m_param, this);
    if (!m_param->bResetZoneConfig)
    {
        zoneReadCount = new ThreadSafeInteger[m_param->rc.zonefileCount];
        zoneWriteCount = new ThreadSafeInteger[m_param->rc.zonefileCount];
    }

   // initVPS(&m_vps);
   // initSPS(&m_sps);
    //initPPS(&m_pps);
   
    
    int numRows = (m_param->sourceHeight + m_param->maxCUSize - 1) / m_param->maxCUSize;
    int numCols = (m_param->sourceWidth  + m_param->maxCUSize - 1) / m_param->maxCUSize;
    for (int i = 0; i < m_param->frameNumThreads; i++)
    {
        if (!m_frameEncoder[i]->init(this, numRows, numCols))
        {
            x265_log(m_param, X265_LOG_ERROR, "Unable to initialize frame encoder, aborting\n");
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

    initRefIdx();

    m_bZeroLatency = !m_param->bframes && !m_param->lookaheadDepth && m_param->frameNumThreads == 1 && m_param->maxSlices == 1;
    m_aborted |= parseLambdaFile(m_param);

    m_encodeStartTime = x265_mdate();

    m_nalList.m_annexB = !!m_param->bAnnexB;

    if (m_param->naluFile)
    {
        m_naluFile = x265_fopen(m_param->naluFile, "r");
        if (!m_naluFile)
        {
            x265_log_file(NULL, X265_LOG_ERROR, "%s file not found or Failed to open\n", m_param->naluFile);
            m_aborted = true;
        }
        else
             m_enableNal = 1;
    }
    else
         m_enableNal = 0;

#if ENABLE_HDR10_PLUS
    if (m_bToneMap)
        m_numCimInfo = m_hdr10plus_api->hdr10plus_json_to_movie_cim(m_param->toneMapFile, m_cim);
#endif
    if (m_param->bDynamicRefine)
    {
        /* Allocate memory for 1 GOP and reuse it for the subsequent GOPs */
        int size = (m_param->keyframeMax + m_param->lookaheadDepth) * m_param->maxCUDepth * X265_REFINE_INTER_LEVELS;
        CHECKED_MALLOC_ZERO(m_variance, uint64_t, size);
        CHECKED_MALLOC_ZERO(m_rdCost, uint64_t, size);
        CHECKED_MALLOC_ZERO(m_trainingCount, uint32_t, size);
        return;
    fail:
        m_aborted = true;
    }
}

void Encoder::stopJobs()
{
    if (m_rateControl)
        m_rateControl->terminate(); // unblock all blocked RC calls

    if (m_lookahead)
        m_lookahead->stopJobs();
    
    for (int i = 0; i < m_param->frameNumThreads; i++)
    {
        if (m_frameEncoder[i])
        {
            m_frameEncoder[i]->getEncodedPicture(m_nalList);
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

int Encoder::copySlicetypePocAndSceneCut(int *slicetype, int *poc, int *sceneCut)
{
    Frame *FramePtr = m_dpb->m_picList.getCurFrame();
    if (FramePtr != NULL)
    {
        *slicetype = FramePtr->m_lowres.sliceType;
        *poc = FramePtr->m_encData->m_slice->m_poc;
        *sceneCut = FramePtr->m_lowres.bScenecut;
    }
    else
    {
        x265_log(NULL, X265_LOG_WARNING, "Frame is still in lookahead pipeline, this API must be called after (poc >= lookaheadDepth + bframes + 2) condition check\n");
        return -1;
    }
    return 0;
}

int Encoder::getRefFrameList(PicYuv** l0, PicYuv** l1, int sliceType, int poc, int* pocL0, int* pocL1)
{
    if (!(IS_X265_TYPE_I(sliceType)))
    {
        Frame *framePtr = m_dpb->m_picList.getPOC(poc);
        if (framePtr != NULL)
        {
            for (int j = 0; j < framePtr->m_encData->m_slice->m_numRefIdx[0]; j++)    // check only for --ref=n number of frames.
            {
                if (framePtr->m_encData->m_slice->m_refFrameList[0][j] && framePtr->m_encData->m_slice->m_refFrameList[0][j]->m_reconPic != NULL)
                {
                    int l0POC = framePtr->m_encData->m_slice->m_refFrameList[0][j]->m_poc;
                    pocL0[j] = l0POC;
                    Frame* l0Fp = m_dpb->m_picList.getPOC(l0POC);
                    while (l0Fp->m_reconRowFlag[l0Fp->m_numRows - 1].get() == 0)
                        l0Fp->m_reconRowFlag[l0Fp->m_numRows - 1].waitForChange(0); /* If recon is not ready, current frame encoder has to wait. */
                    l0[j] = l0Fp->m_reconPic;
                }
            }
            for (int j = 0; j < framePtr->m_encData->m_slice->m_numRefIdx[1]; j++)    // check only for --ref=n number of frames.
            {
                if (framePtr->m_encData->m_slice->m_refFrameList[1][j] && framePtr->m_encData->m_slice->m_refFrameList[1][j]->m_reconPic != NULL)
                {
                    int l1POC = framePtr->m_encData->m_slice->m_refFrameList[1][j]->m_poc;
                    pocL1[j] = l1POC;
                    Frame* l1Fp = m_dpb->m_picList.getPOC(l1POC);
                    while (l1Fp->m_reconRowFlag[l1Fp->m_numRows - 1].get() == 0)
                        l1Fp->m_reconRowFlag[l1Fp->m_numRows - 1].waitForChange(0); /* If recon is not ready, current frame encoder has to wait. */
                    l1[j] = l1Fp->m_reconPic;
                }
            }
        }
        else
        {
            x265_log(NULL, X265_LOG_WARNING, "Current frame is not in DPB piclist.\n");
            return 1;
        }
    }
    else
    {
        x265_log(NULL, X265_LOG_ERROR, "I frames does not have a refrence List\n");
        return -1;
    }
    return 0;
}


void Encoder::destroy()
{

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

    delete m_dpb;

    if (m_param->bEnableTemporalFilter)
        delete m_origPicBuffer;

    if (m_latestParam != NULL && m_latestParam != m_param)
    {
        if (m_latestParam->scalingLists != m_param->scalingLists)
            free((char*)m_latestParam->scalingLists);

        PARAM_NS::x265_param_free(m_latestParam);
    }
    if (m_param)
    {
        PARAM_NS::x265_param_free(m_param);
    }
}


//Find Sum of Squared Difference (SSD) between two pictures
uint64_t Encoder::computeSSD(pixel *fenc, pixel *rec, intptr_t stride, uint32_t width, uint32_t height, x265_param *param)
{
    uint64_t ssd = 0;

    if (!param->bEnableFrameDuplication || (width & 3))
    {
        if ((width | height) & 3)
        {
            /* Slow Path */
            for (uint32_t y = 0; y < height; y++)
            {
                for (uint32_t x = 0; x < width; x++)
                {
                    int diff = (int)(fenc[x] - rec[x]);
                    ssd += diff * diff;
                }

                fenc += stride;
                rec += stride;
            }

            return ssd;
        }
    }

    uint32_t y = 0;

    /* Consume rows in ever narrower chunks of height */
    for (int size = BLOCK_64x64; size >= BLOCK_4x4 && y < height; size--)
    {
        uint32_t rowHeight = 1 << (size + 2);

        for (; y + rowHeight <= height; y += rowHeight)
        {
            uint32_t y1, x = 0;

            /* Consume each row using the largest square blocks possible */
            if (size == BLOCK_64x64 && !(stride & 31))
                for (; x + 64 <= width; x += 64)
                    ssd += primitives.cu[BLOCK_64x64].sse_pp(fenc + x, stride, rec + x, stride);

            if (size >= BLOCK_32x32 && !(stride & 15))
                for (; x + 32 <= width; x += 32)
                    for (y1 = 0; y1 + 32 <= rowHeight; y1 += 32)
                        ssd += primitives.cu[BLOCK_32x32].sse_pp(fenc + y1 * stride + x, stride, rec + y1 * stride + x, stride);

            if (size >= BLOCK_16x16)
                for (; x + 16 <= width; x += 16)
                    for (y1 = 0; y1 + 16 <= rowHeight; y1 += 16)
                        ssd += primitives.cu[BLOCK_16x16].sse_pp(fenc + y1 * stride + x, stride, rec + y1 * stride + x, stride);

            if (size >= BLOCK_8x8)
                for (; x + 8 <= width; x += 8)
                    for (y1 = 0; y1 + 8 <= rowHeight; y1 += 8)
                        ssd += primitives.cu[BLOCK_8x8].sse_pp(fenc + y1 * stride + x, stride, rec + y1 * stride + x, stride);

            for (; x + 4 <= width; x += 4)
                for (y1 = 0; y1 + 4 <= rowHeight; y1 += 4)
                    ssd += primitives.cu[BLOCK_4x4].sse_pp(fenc + y1 * stride + x, stride, rec + y1 * stride + x, stride);

            fenc += stride * rowHeight;
            rec += stride * rowHeight;
        }
    }

    /* Handle last few rows of frames for videos 
    with height not divisble by 4 */
    uint32_t h = height % y;
    if (param->bEnableFrameDuplication && h)
    {
        for (uint32_t i = 0; i < h; i++)
        {
            for (uint32_t j = 0; j < width; j++)
            {
                int diff = (int)(fenc[j] - rec[j]);
                ssd += diff * diff;
            }

            fenc += stride;
            rec += stride;
        }
    }

    return ssd;
}

//Compute the PSNR weightage between two pictures
double Encoder::ComputePSNR(x265_picture *firstPic, x265_picture *secPic, x265_param *param)
{
    uint64_t ssdY = 0, ssdU = 0, ssdV = 0;
    intptr_t strideL, strideC;
    uint32_t widthL, heightL, widthC, heightC;
    double psnrY = 0, psnrU = 0, psnrV = 0, psnrWeight = 0;
    int width = firstPic->width;
    int height = firstPic->height;
    int hshift = CHROMA_H_SHIFT(firstPic->colorSpace);
    int vshift = CHROMA_V_SHIFT(firstPic->colorSpace);
    pixel *yFirstPic = NULL, *ySecPic = NULL;
    pixel *uFirstPic = NULL, *uSecPic = NULL;
    pixel *vFirstPic = NULL, *vSecPic = NULL;

    strideL = widthL = width;
    heightL = height;

    strideC = widthC = widthL >> hshift;
    heightC = heightL >> vshift;

    int size = width * height;
    int maxvalY = 255 << (X265_DEPTH - 8);
    int maxvalC = 255 << (X265_DEPTH - 8);
    double refValueY = (double)maxvalY * maxvalY * size;
    double refValueC = (double)maxvalC * maxvalC * size / 4.0;

    if (firstPic->bitDepth == 8 && X265_DEPTH == 8)
    {
        yFirstPic = (pixel*)firstPic->planes[0];
        ySecPic = (pixel*)secPic->planes[0];
        if (param->internalCsp != X265_CSP_I400)
        {
            uFirstPic = (pixel*)firstPic->planes[1];
            uSecPic = (pixel*)secPic->planes[1];
            vFirstPic = (pixel*)firstPic->planes[2];
            vSecPic = (pixel*)secPic->planes[2];
        }
    }
    else if (firstPic->bitDepth == 8 && X265_DEPTH > 8)
    {
        int shift = (X265_DEPTH - 8);
        uint8_t *yChar1, *yChar2, *uChar1, *uChar2, *vChar1, *vChar2;

        yChar1 = (uint8_t*)firstPic->planes[0];
        yChar2 = (uint8_t*)secPic->planes[0];

        primitives.planecopy_cp(yChar1, firstPic->stride[0] / sizeof(*yChar1), m_dupPicOne[0], firstPic->stride[0] / sizeof(*yChar1), width, height, shift);
        primitives.planecopy_cp(yChar2, secPic->stride[0] / sizeof(*yChar2), m_dupPicTwo[0], secPic->stride[0] / sizeof(*yChar2), width, height, shift);

        if (param->internalCsp != X265_CSP_I400)
        {
            uChar1 = (uint8_t*)firstPic->planes[1];
            uChar2 = (uint8_t*)secPic->planes[1];
            vChar1 = (uint8_t*)firstPic->planes[2];
            vChar2 = (uint8_t*)secPic->planes[2];

            primitives.planecopy_cp(uChar1, firstPic->stride[1] / sizeof(*uChar1), m_dupPicOne[1], firstPic->stride[1] / sizeof(*uChar1), widthC, heightC, shift);
            primitives.planecopy_cp(uChar2, secPic->stride[1] / sizeof(*uChar2), m_dupPicTwo[1], secPic->stride[1] / sizeof(*uChar2), widthC, heightC, shift);

            primitives.planecopy_cp(vChar1, firstPic->stride[2] / sizeof(*vChar1), m_dupPicOne[2], firstPic->stride[2] / sizeof(*vChar1), widthC, heightC, shift);
            primitives.planecopy_cp(vChar2, secPic->stride[2] / sizeof(*vChar2), m_dupPicTwo[2], secPic->stride[2] / sizeof(*vChar2), widthC, heightC, shift);
        }
    }
    else
    {
        uint16_t *yShort1, *yShort2, *uShort1, *uShort2, *vShort1, *vShort2;
        /* defensive programming, mask off bits that are supposed to be zero */
        uint16_t mask = (1 << X265_DEPTH) - 1;
        int shift = abs(firstPic->bitDepth - X265_DEPTH);

        yShort1 = (uint16_t*)firstPic->planes[0];
        yShort2 = (uint16_t*)secPic->planes[0];

        if (firstPic->bitDepth > X265_DEPTH)
        {
            /* shift right and mask pixels to final size */
            primitives.planecopy_sp(yShort1, firstPic->stride[0] / sizeof(*yShort1), m_dupPicOne[0], firstPic->stride[0] / sizeof(*yShort1), width, height, shift, mask);
            primitives.planecopy_sp(yShort2, secPic->stride[0] / sizeof(*yShort2), m_dupPicTwo[0], secPic->stride[0] / sizeof(*yShort2), width, height, shift, mask);
        }
        else /* Case for (pic.bitDepth <= X265_DEPTH) */
        {
            /* shift left and mask pixels to final size */
            primitives.planecopy_sp_shl(yShort1, firstPic->stride[0] / sizeof(*yShort1), m_dupPicOne[0], firstPic->stride[0] / sizeof(*yShort1), width, height, shift, mask);
            primitives.planecopy_sp_shl(yShort2, secPic->stride[0] / sizeof(*yShort2), m_dupPicTwo[0], secPic->stride[0] / sizeof(*yShort2), width, height, shift, mask);
        }

        if (param->internalCsp != X265_CSP_I400)
        {
            uShort1 = (uint16_t*)firstPic->planes[1];
            uShort2 = (uint16_t*)secPic->planes[1];
            vShort1 = (uint16_t*)firstPic->planes[2];
            vShort2 = (uint16_t*)secPic->planes[2];

            if (firstPic->bitDepth > X265_DEPTH)
            {
                primitives.planecopy_sp(uShort1, firstPic->stride[1] / sizeof(*uShort1), m_dupPicOne[1], firstPic->stride[1] / sizeof(*uShort1), widthC, heightC, shift, mask);
                primitives.planecopy_sp(uShort2, secPic->stride[1] / sizeof(*uShort2), m_dupPicTwo[1], secPic->stride[1] / sizeof(*uShort2), widthC, heightC, shift, mask);

                primitives.planecopy_sp(vShort1, firstPic->stride[2] / sizeof(*vShort1), m_dupPicOne[2], firstPic->stride[2] / sizeof(*vShort1), widthC, heightC, shift, mask);
                primitives.planecopy_sp(vShort2, secPic->stride[2] / sizeof(*vShort2), m_dupPicTwo[2], secPic->stride[2] / sizeof(*vShort2), widthC, heightC, shift, mask);
            }
            else /* Case for (pic.bitDepth <= X265_DEPTH) */
            {
                primitives.planecopy_sp_shl(uShort1, firstPic->stride[1] / sizeof(*uShort1), m_dupPicOne[1], firstPic->stride[1] / sizeof(*uShort1), widthC, heightC, shift, mask);
                primitives.planecopy_sp_shl(uShort2, secPic->stride[1] / sizeof(*uShort2), m_dupPicTwo[1], secPic->stride[1] / sizeof(*uShort2), widthC, heightC, shift, mask);

                primitives.planecopy_sp_shl(vShort1, firstPic->stride[2] / sizeof(*vShort1), m_dupPicOne[2], firstPic->stride[2] / sizeof(*vShort1), widthC, heightC, shift, mask);
                primitives.planecopy_sp_shl(vShort2, secPic->stride[2] / sizeof(*vShort2), m_dupPicTwo[2], secPic->stride[2] / sizeof(*vShort2), widthC, heightC, shift, mask);
            }
        }
    }

    if (!(firstPic->bitDepth == 8 && X265_DEPTH == 8))
    {
        yFirstPic = m_dupPicOne[0]; ySecPic = m_dupPicTwo[0];
        uFirstPic = m_dupPicOne[1]; uSecPic = m_dupPicTwo[1];
        vFirstPic = m_dupPicOne[2]; vSecPic = m_dupPicTwo[2];
    }

    //Compute SSD
    ssdY = computeSSD(yFirstPic, ySecPic, strideL, widthL, heightL, param);
    psnrY = (ssdY ? 10.0 * log10(refValueY / (double)ssdY) : 99.99);

    if (param->internalCsp != X265_CSP_I400)
    {
        ssdU = computeSSD(uFirstPic, uSecPic, strideC, widthC, heightC, param);
        ssdV = computeSSD(vFirstPic, vSecPic, strideC, widthC, heightC, param);
        psnrU = (ssdU ? 10.0 * log10(refValueC / (double)ssdU) : 99.99);
        psnrV = (ssdV ? 10.0 * log10(refValueC / (double)ssdV) : 99.99);
    }

    //Compute PSNR(picN,pic(N+1))
    return psnrWeight = (psnrY * 6 + psnrU + psnrV) / 8;
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

bool Encoder::isFilterThisframe(uint8_t sliceTypeConfig, int curSliceType)
{
    uint8_t newSliceType = 0;
    switch (curSliceType)
    {
    case 1: newSliceType |= 1 << 0;
        break;
    case 2: newSliceType |= 1 << 0;
        break;
    case 3: newSliceType |= 1 << 1;
        break;
    case 4: newSliceType |= 1 << 2;
        break;
    case 5: newSliceType |= 1 << 3;
        break;
    default: return 0;
    }
    return ((sliceTypeConfig & newSliceType) != 0);
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
        x265_log(m_param, X265_LOG_ERROR, "encoder aborting because of internal error\n");
        return -1;
    }
#endif
    if (m_aborted)
        return -1;

    const x265_picture* inputPic = NULL;
    static int written = 0, read = 0;
    bool dontRead = false;
    bool dropflag = false;

    if (m_exportedPic)
    {
        ATOMIC_DEC(&m_exportedPic->m_countRefEncoders);
        if (m_param->bEnableTemporalFilter)
            m_origPicBuffer->recycleOrigPicList();
    }

    if ((pic_in && (!m_param->chunkEnd || (m_encodedFrameNum < m_param->chunkEnd))) || (m_param->bEnableFrameDuplication && !pic_in && (read < written)))
    {

        inputPic = pic_in;

        Frame *inFrame;
        x265_param *p = (m_reconfigure || m_reconfigureRc) ? m_latestParam : m_param;
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
                        x265_log(m_param, X265_LOG_ERROR, "memory allocation failure, aborting encode\n");
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
                x265_log(m_param, X265_LOG_ERROR, "memory allocation failure, aborting encode\n");
                inFrame->destroy();
                delete inFrame;
                return -1;
            }
        }
        else
        {
            inFrame = m_dpb->m_freeList.popBack();
            inFrame->m_encodeStartTime = x265_mdate();
            /* Set lowres scencut and satdCost here to aovid overwriting ANALYSIS_READ
               decision by lowres init*/
            inFrame->m_lowres.bScenecut = false;
            inFrame->m_lowres.satdCost = (int64_t)-1;
            inFrame->m_lowresInit = false;
            inFrame->m_isInsideWindow = 0;
            inFrame->m_tempLayer = 0;
            inFrame->m_sameLayerRefPic = 0;
        }

        /* Copy input picture into a Frame and PicYuv, send to lookahead */
        inFrame->m_fencPic->copyFromPicture(*inputPic, *m_param, m_sps.conformanceWindow.rightOffset, m_sps.conformanceWindow.bottomOffset);

        inFrame->m_poc       = ++m_pocLast;
        inFrame->m_param     = m_param;

        if (m_pocLast == 0)
            m_firstPts = inFrame->m_pts;
        if (m_bframeDelay && m_pocLast == m_bframeDelay)
            m_bframeDelayTime = inFrame->m_pts - m_firstPts;

        /* Encoder holds a reference count until stats collection is finished */
        ATOMIC_INC(&inFrame->m_countRefEncoders);

        if (m_param->bEnableTemporalFilter)
        {
            if (!m_pocLast)
            {
                /*One shot allocation of frames in OriginalPictureBuffer*/
                int numFramesinOPB = X265_MAX(m_param->bframes, (inFrame->m_mcstf->m_range << 1)) + 1;
                for (int i = 0; i < numFramesinOPB; i++)
                {
                    Frame* dupFrame = new Frame;
                    if (!(dupFrame->create(m_param)))
                    {
                        m_aborted = true;
                        x265_log(m_param, X265_LOG_ERROR, "Memory allocation failure, aborting encode\n");
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
        }

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
            outFrame = curEncoder->getEncodedPicture(m_nalList);
        if (outFrame)
        {
            Slice *slice = outFrame->m_encData->m_slice;
            x265_frame_stats* frameData = NULL;

            if (pic_out)
            {
                PicYuv *recpic = outFrame->m_fencPic;
                pic_out->poc = outFrame->m_poc;
                pic_out->bitDepth = X265_DEPTH;
                pic_out->colorSpace = m_param->internalCsp;
                //pic_out->reorderedPts = outFrame->m_reorderedPts;
                //pic_out->sliceType = outFrame->m_lowres.sliceType;
                pic_out->planes[0] = recpic->m_picFil[0];
                pic_out->stride[0] = (int)(recpic->m_stride * sizeof(pixel));
                if (m_param->internalCsp != X265_CSP_I400)
                {
                    pic_out->planes[1] = recpic->m_picFil[1];
                    pic_out->stride[1] = (int)(recpic->m_strideC * sizeof(pixel));
                    pic_out->planes[2] = recpic->m_picFil[2];
                    pic_out->stride[2] = (int)(recpic->m_strideC * sizeof(pixel));
                }
            }

            if (m_aborted)
                return -1;

            if (m_param->bEnableTemporalFilter)
            {
                Frame *curFrame = m_origPicBuffer->m_mcstfPicList.getPOCMCSTF(outFrame->m_poc);
                X265_CHECK(curFrame, "Outframe not found in DPB's mcstfPicList");
                curFrame->m_refPicCnt[0]--;
                curFrame->m_refPicCnt[1]--;
                curFrame = m_origPicBuffer->m_mcstfOrigPicList.getPOCMCSTF(outFrame->m_poc);
                X265_CHECK(curFrame, "Outframe not found in OPB's mcstfOrigPicList");
                curFrame->m_refPicCnt[1]--;
            }

            /* Allow this frame to be recycled if no frame encoders are using it for reference */
            if (!pic_out)
            {
                ATOMIC_DEC(&outFrame->m_countRefEncoders);
                m_dpb->recycleUnreferenced();
                if (m_param->bEnableTemporalFilter)
                    m_origPicBuffer->recycleOrigPicList();
            }
            else
                m_exportedPic = outFrame;
            
            m_outputCount++;
            if (m_param->chunkEnd == m_outputCount)
                m_numDelayedPic = 0;
            else 
                m_numDelayedPic--;

            ret = 1;
        }

        /* pop a single frame from decided list, then provide to frame encoder
         * curEncoder is guaranteed to be idle at this point */
        if (!pass)
            frameEnc = m_lookahead->getDecidedPicture();
        if (frameEnc && !pass && (!m_param->chunkEnd || (m_encodedFrameNum < m_param->chunkEnd)))
        {
            frameEnc->allocEncodeData(m_reconfigure ? m_latestParam : m_param, m_sps);
            if (m_param->bEnableTemporalFilter)
            {
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
            }

            // Generate MCSTF References and perform HME
            if (m_param->bEnableTemporalFilter)
            {

                if (!generateMcstfRef(frameEnc, curEncoder))
                {
                    m_aborted = true;
                    x265_log(m_param, X265_LOG_ERROR, "Failed to initialize MCSTFReferencePicInfo at POC %d\n", frameEnc->m_poc);
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
            }

            /* Allow FrameEncoder::compressFrame() to start in the frame encoder thread */
            if (!curEncoder->startCompressFrame(frameEnc))
                m_aborted = true;
        }
        else if (m_encodedFrameNum)
            m_rateControl->setFinalFrameCount(m_encodedFrameNum);
    }
    while (m_bZeroLatency && ++pass < 2);

    return ret;
}

int Encoder::reconfigureParam(x265_param* encParam, x265_param* param)
{
    if (isReconfigureRc(encParam, param) && !param->rc.zonefileCount)
    {
        /* VBV can't be turned ON if it wasn't ON to begin with and can't be turned OFF if it was ON to begin with*/
        if (param->rc.vbvMaxBitrate > 0 && param->rc.vbvBufferSize > 0 &&
            encParam->rc.vbvMaxBitrate > 0 && encParam->rc.vbvBufferSize > 0)
        {
            m_reconfigureRc |= encParam->rc.vbvMaxBitrate != param->rc.vbvMaxBitrate;
            m_reconfigureRc |= encParam->rc.vbvBufferSize != param->rc.vbvBufferSize;
            if (m_reconfigureRc && m_param->bEmitHRDSEI)
                x265_log(m_param, X265_LOG_WARNING, "VBV parameters cannot be changed when HRD is in use.\n");
            else
            {
                encParam->rc.vbvMaxBitrate = param->rc.vbvMaxBitrate;
                encParam->rc.vbvBufferSize = param->rc.vbvBufferSize;
            }
        }
        m_reconfigureRc |= encParam->rc.bitrate != param->rc.bitrate;
        encParam->rc.bitrate = param->rc.bitrate;
        m_reconfigureRc |= encParam->rc.rfConstant != param->rc.rfConstant;
        encParam->rc.rfConstant = param->rc.rfConstant;
    }
    else
    {
        encParam->maxNumReferences = param->maxNumReferences; // never uses more refs than specified in stream headers
        encParam->bEnableFastIntra = param->bEnableFastIntra;
        encParam->bEnableEarlySkip = param->bEnableEarlySkip;
        encParam->recursionSkipMode = param->recursionSkipMode;
        encParam->searchMethod = param->searchMethod;
        /* Scratch buffer prevents me_range from being increased for esa/tesa */
        if (param->searchRange < encParam->searchRange)
            encParam->searchRange = param->searchRange;
        /* We can't switch out of subme=0 during encoding. */
        if (encParam->subpelRefine)
            encParam->subpelRefine = param->subpelRefine;
        encParam->rdoqLevel = param->rdoqLevel;
        encParam->rdLevel = param->rdLevel;
        encParam->bEnableRectInter = param->bEnableRectInter;
        encParam->maxNumMergeCand = param->maxNumMergeCand;
        encParam->bIntraInBFrames = param->bIntraInBFrames;
        if (param->scalingLists && !encParam->scalingLists)
            encParam->scalingLists = strdup(param->scalingLists);

        encParam->rc.aqMode = param->rc.aqMode;
        encParam->rc.aqStrength = param->rc.aqStrength;
        encParam->noiseReductionInter = param->noiseReductionInter;
        encParam->noiseReductionIntra = param->noiseReductionIntra;

        encParam->limitModes = param->limitModes;
        encParam->bEnableSplitRdSkip = param->bEnableSplitRdSkip;
        encParam->bCULossless = param->bCULossless;
        encParam->bEnableRdRefine = param->bEnableRdRefine;
        encParam->limitTU = param->limitTU;
        encParam->bEnableTSkipFast = param->bEnableTSkipFast;
        encParam->rdPenalty = param->rdPenalty;
        encParam->dynamicRd = param->dynamicRd;
        encParam->bEnableTransformSkip = param->bEnableTransformSkip;
        encParam->bEnableAMP = param->bEnableAMP;
        if (param->confWinBottomOffset == 0 && param->confWinRightOffset == 0)
        {
            encParam->confWinBottomOffset = param->confWinBottomOffset;
            encParam->confWinRightOffset = param->confWinRightOffset;
        }
        /* Resignal changes in params in Parameter Sets */
        m_sps.maxAMPDepth = (m_sps.bUseAMP = param->bEnableAMP && param->bEnableAMP) ? param->maxCUDepth : 0;
        m_pps.bTransformSkipEnabled = param->bEnableTransformSkip ? 1 : 0;

    }
    encParam->forceFlush = param->forceFlush;
    /* To add: Loop Filter/deblocking controls, transform skip, signhide require PPS to be resent */
    /* To add: SAO, temporal MVP, AMP, TU depths require SPS to be resent, at every CVS boundary */
    return x265_check_params(encParam);
}

bool Encoder::isReconfigureRc(x265_param* latestParam, x265_param* param_in)
{
    return (latestParam->rc.vbvMaxBitrate != param_in->rc.vbvMaxBitrate
        || latestParam->rc.vbvBufferSize != param_in->rc.vbvBufferSize
        || latestParam->rc.bitrate != param_in->rc.bitrate
        || latestParam->rc.rfConstant != param_in->rc.rfConstant);
}

void Encoder::copyCtuInfo(x265_ctu_info_t** frameCtuInfo, int poc)
{
    uint32_t widthInCU = (m_param->sourceWidth + m_param->maxCUSize - 1) >> m_param->maxLog2CUSize;
    uint32_t heightInCU = (m_param->sourceHeight + m_param->maxCUSize - 1) >> m_param->maxLog2CUSize;
    Frame* curFrame;
    Frame* prevFrame = NULL;
    int32_t* frameCTU;
    uint32_t numCUsInFrame = widthInCU * heightInCU;
    uint32_t maxNum8x8Partitions = 64;
    bool copied = false;
    do
    {
        curFrame = m_lookahead->m_inputQueue.getPOC(poc);
        if (!curFrame)
            curFrame = m_lookahead->m_outputQueue.getPOC(poc);

        if (poc > 0)
        {
            prevFrame = m_lookahead->m_inputQueue.getPOC(poc - 1);
            if (!prevFrame)
                prevFrame = m_lookahead->m_outputQueue.getPOC(poc - 1);
            if (!prevFrame)
            {
                FrameEncoder* prevEncoder;
                for (int i = 0; i < m_param->frameNumThreads; i++)
                {
                    prevEncoder = m_frameEncoder[i];
                    prevFrame = prevEncoder->m_frame;
                    if (prevFrame && (prevEncoder->m_frame->m_poc == poc - 1))
                    {
                        prevFrame = prevEncoder->m_frame;
                        break;
                    }
                }
            }
        }
        x265_ctu_info_t* ctuTemp, *prevCtuTemp;
        if (curFrame)
        {
            if (!curFrame->m_ctuInfo)
                CHECKED_MALLOC(curFrame->m_ctuInfo, x265_ctu_info_t*, 1);
            CHECKED_MALLOC(*curFrame->m_ctuInfo, x265_ctu_info_t, numCUsInFrame);
            CHECKED_MALLOC_ZERO(curFrame->m_prevCtuInfoChange, int, numCUsInFrame * maxNum8x8Partitions);
            for (uint32_t i = 0; i < numCUsInFrame; i++)
            {
                ctuTemp = *curFrame->m_ctuInfo + i;
                CHECKED_MALLOC(frameCTU, int32_t, maxNum8x8Partitions);
                ctuTemp->ctuInfo = (int32_t*)frameCTU;
                ctuTemp->ctuAddress = frameCtuInfo[i]->ctuAddress;
                memcpy(ctuTemp->ctuPartitions, frameCtuInfo[i]->ctuPartitions, sizeof(int32_t) * maxNum8x8Partitions);
                memcpy(ctuTemp->ctuInfo, frameCtuInfo[i]->ctuInfo, sizeof(int32_t) * maxNum8x8Partitions);
                if (prevFrame && curFrame->m_poc > 1)
                {
                    prevCtuTemp = *prevFrame->m_ctuInfo + i;
                    for (uint32_t j = 0; j < maxNum8x8Partitions; j++)
                        curFrame->m_prevCtuInfoChange[i * maxNum8x8Partitions + j] = (*((int32_t *)prevCtuTemp->ctuInfo + j) == 2) ? (poc - 1) : prevFrame->m_prevCtuInfoChange[i * maxNum8x8Partitions + j];
                }
            }
            copied = true;
            curFrame->m_copied.trigger();
        }
        else
        {
            FrameEncoder* curEncoder;
            for (int i = 0; i < m_param->frameNumThreads; i++)
            {
                curEncoder = m_frameEncoder[i];
                curFrame = curEncoder->m_frame;
                if (curFrame)
                {
                    if (poc == curFrame->m_poc)
                    {
                        if (!curFrame->m_ctuInfo)
                            CHECKED_MALLOC(curFrame->m_ctuInfo, x265_ctu_info_t*, 1);
                        CHECKED_MALLOC(*curFrame->m_ctuInfo, x265_ctu_info_t, numCUsInFrame);
                        CHECKED_MALLOC_ZERO(curFrame->m_prevCtuInfoChange, int, numCUsInFrame * maxNum8x8Partitions);
                        for (uint32_t l = 0; l < numCUsInFrame; l++)
                        {
                            ctuTemp = *curFrame->m_ctuInfo + l;
                            CHECKED_MALLOC(frameCTU, int32_t, maxNum8x8Partitions);
                            ctuTemp->ctuInfo = (int32_t*)frameCTU;
                            ctuTemp->ctuAddress = frameCtuInfo[l]->ctuAddress;
                            memcpy(ctuTemp->ctuPartitions, frameCtuInfo[l]->ctuPartitions, sizeof(int32_t) * maxNum8x8Partitions);
                            memcpy(ctuTemp->ctuInfo, frameCtuInfo[l]->ctuInfo, sizeof(int32_t) * maxNum8x8Partitions);
                            if (prevFrame && curFrame->m_poc > 1)
                            {
                                prevCtuTemp = *prevFrame->m_ctuInfo + l;
                                for (uint32_t j = 0; j < maxNum8x8Partitions; j++)
                                    curFrame->m_prevCtuInfoChange[l * maxNum8x8Partitions + j] = (*((int32_t *)prevCtuTemp->ctuInfo + j) == CTU_INFO_CHANGE) ? (poc - 1) : prevFrame->m_prevCtuInfoChange[l * maxNum8x8Partitions + j];
                            }
                        }
                        copied = true;
                        curFrame->m_copied.trigger();
                        break;
                    }
                }
            }
        }
    } while (!copied);
    return;
fail:
    for (uint32_t i = 0; i < numCUsInFrame; i++)
    {
        X265_FREE((*curFrame->m_ctuInfo + i)->ctuInfo);
        (*curFrame->m_ctuInfo + i)->ctuInfo = NULL;
    }
    X265_FREE(*curFrame->m_ctuInfo);
    *(curFrame->m_ctuInfo) = NULL;
    X265_FREE(curFrame->m_ctuInfo);
    curFrame->m_ctuInfo = NULL;
    X265_FREE(curFrame->m_prevCtuInfoChange);
    curFrame->m_prevCtuInfoChange = NULL;
}

void EncStats::addPsnr(double psnrY, double psnrU, double psnrV)
{
    m_psnrSumY += psnrY;
    m_psnrSumU += psnrU;
    m_psnrSumV += psnrV;
}

void EncStats::addBits(uint64_t bits)
{
    m_accBits += bits;
    m_numPics++;
}

void EncStats::addSsim(double ssim)
{
    m_globalSsim += ssim;
}

void EncStats::addQP(double aveQp)
{
    m_totalQp += aveQp;
}

char* Encoder::statsString(EncStats& stat, char* buffer)
{
    double fps = (double)m_param->fpsNum / m_param->fpsDenom;
    double scale = fps / 1000 / (double)stat.m_numPics;

    int len = sprintf(buffer, "%6u, ", stat.m_numPics);

    len += sprintf(buffer + len, "Avg QP:%2.2lf", stat.m_totalQp / (double)stat.m_numPics);
    len += sprintf(buffer + len, "  kb/s: %-8.2lf", stat.m_accBits * scale);
    if (m_param->bEnablePsnr)
    {
        len += sprintf(buffer + len, "  PSNR Mean: Y:%.3lf U:%.3lf V:%.3lf",
                       stat.m_psnrSumY / (double)stat.m_numPics,
                       stat.m_psnrSumU / (double)stat.m_numPics,
                       stat.m_psnrSumV / (double)stat.m_numPics);
    }
    if (m_param->bEnableSsim)
    {
        sprintf(buffer + len, "  SSIM Mean: %.6lf (%.3lfdB)",
                stat.m_globalSsim / (double)stat.m_numPics,
                x265_ssim2dB(stat.m_globalSsim / (double)stat.m_numPics));
    }
    return buffer;
}

#if defined(_MSC_VER)
#pragma warning(disable: 4800) // forcing int to bool
#pragma warning(disable: 4127) // conditional expression is constant
#endif

void Encoder::initRefIdx()
{
    int j = 0;

    for (j = 0; j < MAX_NUM_REF_IDX; j++)
    {
        m_refIdxLastGOP.numRefIdxl0[j] = 0;
        m_refIdxLastGOP.numRefIdxl1[j] = 0;
    }

    return;
}

void Encoder::analyseRefIdx(int *numRefIdx)
{
    int i_l0 = 0;
    int i_l1 = 0;

    i_l0 = numRefIdx[0];
    i_l1 = numRefIdx[1];

    if ((0 < i_l0) && (MAX_NUM_REF_IDX > i_l0))
        m_refIdxLastGOP.numRefIdxl0[i_l0]++;
    if ((0 < i_l1) && (MAX_NUM_REF_IDX > i_l1))
        m_refIdxLastGOP.numRefIdxl1[i_l1]++;

    return;
}

void Encoder::updateRefIdx()
{
    int i_max_l0 = 0;
    int i_max_l1 = 0;
    int j = 0;

    i_max_l0 = 0;
    i_max_l1 = 0;
    m_refIdxLastGOP.numRefIdxDefault[0] = 1;
    m_refIdxLastGOP.numRefIdxDefault[1] = 1;
    for (j = 0; j < MAX_NUM_REF_IDX; j++)
    {
        if (i_max_l0 < m_refIdxLastGOP.numRefIdxl0[j])
        {
            i_max_l0 = m_refIdxLastGOP.numRefIdxl0[j];
            m_refIdxLastGOP.numRefIdxDefault[0] = j;
        }
        if (i_max_l1 < m_refIdxLastGOP.numRefIdxl1[j])
        {
            i_max_l1 = m_refIdxLastGOP.numRefIdxl1[j];
            m_refIdxLastGOP.numRefIdxDefault[1] = j;
        }
    }

    m_pps.numRefIdxDefault[0] = m_refIdxLastGOP.numRefIdxDefault[0];
    m_pps.numRefIdxDefault[1] = m_refIdxLastGOP.numRefIdxDefault[1];
    initRefIdx();

    return;
}


void Encoder::getEndNalUnits(NALList& list, Bitstream& bs)
{
    NALList nalList;
    bs.resetBits();

    if (m_param->bEnableEndOfSequence)
        nalList.serialize(NAL_UNIT_EOS, bs);
    if (m_param->bEnableEndOfBitstream)
        nalList.serialize(NAL_UNIT_EOB, bs);

    list.takeContents(nalList);
}

void Encoder::initVPS(VPS *vps)
{
    /* Note that much of the VPS is initialized by determineLevel() */
    vps->ptl.progressiveSourceFlag = !m_param->interlaceMode;
    vps->ptl.interlacedSourceFlag = !!m_param->interlaceMode;
    vps->ptl.nonPackedConstraintFlag = false;
    vps->ptl.frameOnlyConstraintFlag = !m_param->interlaceMode;
}

void Encoder::initSPS(SPS *sps)
{
    sps->conformanceWindow = m_conformanceWindow;
    sps->chromaFormatIdc = m_param->internalCsp;
    sps->picWidthInLumaSamples = m_param->sourceWidth;
    sps->picHeightInLumaSamples = m_param->sourceHeight;
    sps->numCuInWidth = (m_param->sourceWidth + m_param->maxCUSize - 1) / m_param->maxCUSize;
    sps->numCuInHeight = (m_param->sourceHeight + m_param->maxCUSize - 1) / m_param->maxCUSize;
    sps->numCUsInFrame = sps->numCuInWidth * sps->numCuInHeight;
    sps->numPartitions = m_param->num4x4Partitions;
    sps->numPartInCUSize = 1 << m_param->unitSizeDepth;

    sps->log2MinCodingBlockSize = m_param->maxLog2CUSize - m_param->maxCUDepth;
    sps->log2DiffMaxMinCodingBlockSize = m_param->maxCUDepth;
    uint32_t maxLog2TUSize = (uint32_t)g_log2Size[m_param->maxTUSize];
    sps->quadtreeTULog2MaxSize = X265_MIN((uint32_t)m_param->maxLog2CUSize, maxLog2TUSize);
    sps->quadtreeTULog2MinSize = 2;
    sps->quadtreeTUMaxDepthInter = m_param->tuQTMaxInterDepth;
    sps->quadtreeTUMaxDepthIntra = m_param->tuQTMaxIntraDepth;

    sps->bUseSAO = m_param->bEnableSAO;

    sps->bUseAMP = m_param->bEnableAMP;
    sps->maxAMPDepth = m_param->bEnableAMP ? m_param->maxCUDepth : 0;

    sps->maxTempSubLayers = m_vps.maxTempSubLayers;// Getting the value from the user

    for(uint8_t i = 0; i < sps->maxTempSubLayers; i++)
    {
        sps->maxDecPicBuffering[i] = m_vps.maxDecPicBuffering[i];
        sps->numReorderPics[i] = m_vps.numReorderPics[i];
        sps->maxLatencyIncrease[i] = m_vps.maxLatencyIncrease[i] = m_param->bframes;
    }

    sps->bUseStrongIntraSmoothing = m_param->bEnableStrongIntraSmoothing;
    sps->bTemporalMVPEnabled = m_param->bEnableTemporalMvp;
    sps->bEmitVUITimingInfo = m_param->bEmitVUITimingInfo;
    sps->bEmitVUIHRDInfo = m_param->bEmitVUIHRDInfo;
    sps->log2MaxPocLsb = m_param->log2MaxPocLsb;
    int maxDeltaPOC = (m_param->bframes + 2) * (!!m_param->bBPyramid + 1) * 2;
    while ((1 << sps->log2MaxPocLsb) <= maxDeltaPOC * 2)
        sps->log2MaxPocLsb++;

    if (sps->log2MaxPocLsb != m_param->log2MaxPocLsb)
        x265_log(m_param, X265_LOG_WARNING, "Reset log2MaxPocLsb to %d to account for all POC values\n", sps->log2MaxPocLsb);

    VUI& vui = sps->vuiParameters;
    vui.aspectRatioInfoPresentFlag = !!m_param->vui.aspectRatioIdc;
    vui.aspectRatioIdc = m_param->vui.aspectRatioIdc;
    vui.sarWidth = m_param->vui.sarWidth;
    vui.sarHeight = m_param->vui.sarHeight;

    vui.overscanInfoPresentFlag = m_param->vui.bEnableOverscanInfoPresentFlag;
    vui.overscanAppropriateFlag = m_param->vui.bEnableOverscanAppropriateFlag;

    vui.videoSignalTypePresentFlag = m_param->vui.bEnableVideoSignalTypePresentFlag;
    vui.videoFormat = m_param->vui.videoFormat;
    vui.videoFullRangeFlag = m_param->vui.bEnableVideoFullRangeFlag;

    vui.colourDescriptionPresentFlag = m_param->vui.bEnableColorDescriptionPresentFlag;
    vui.colourPrimaries = m_param->vui.colorPrimaries;
    vui.transferCharacteristics = m_param->vui.transferCharacteristics;
    vui.matrixCoefficients = m_param->vui.matrixCoeffs;

    vui.chromaLocInfoPresentFlag = m_param->vui.bEnableChromaLocInfoPresentFlag;
    vui.chromaSampleLocTypeTopField = m_param->vui.chromaSampleLocTypeTopField;
    vui.chromaSampleLocTypeBottomField = m_param->vui.chromaSampleLocTypeBottomField;

    vui.defaultDisplayWindow.bEnabled = m_param->vui.bEnableDefaultDisplayWindowFlag;
    vui.defaultDisplayWindow.rightOffset = m_param->vui.defDispWinRightOffset;
    vui.defaultDisplayWindow.topOffset = m_param->vui.defDispWinTopOffset;
    vui.defaultDisplayWindow.bottomOffset = m_param->vui.defDispWinBottomOffset;
    vui.defaultDisplayWindow.leftOffset = m_param->vui.defDispWinLeftOffset;

    vui.frameFieldInfoPresentFlag = !!m_param->interlaceMode || (m_param->pictureStructure >= 0);
    vui.fieldSeqFlag = !!m_param->interlaceMode;

    vui.hrdParametersPresentFlag = m_param->bEmitHRDSEI;

    vui.timingInfo.numUnitsInTick = m_param->fpsDenom;
    vui.timingInfo.timeScale = m_param->fpsNum;
}

void Encoder::initPPS(PPS *pps)
{
    bool bIsVbv = m_param->rc.vbvBufferSize > 0 && m_param->rc.vbvMaxBitrate > 0;
    bool bEnableDistOffset = m_param->analysisMultiPassDistortion && m_param->rc.bStatRead;

    if (!m_param->bLossless && (m_param->rc.aqMode || bIsVbv || m_param->bAQMotion))
    {
        pps->bUseDQP = true;
        pps->maxCuDQPDepth = g_log2Size[m_param->maxCUSize] - g_log2Size[m_param->rc.qgSize];
        X265_CHECK(pps->maxCuDQPDepth <= 3, "max CU DQP depth cannot be greater than 3\n");
    }
    else if (!m_param->bLossless && bEnableDistOffset)
    {
        pps->bUseDQP = true;
        pps->maxCuDQPDepth = 0;
    }
    else
    {
        pps->bUseDQP = false;
        pps->maxCuDQPDepth = 0;
    }

    pps->chromaQpOffset[0] = m_param->cbQpOffset;
    pps->chromaQpOffset[1] = m_param->crQpOffset;
    pps->pps_slice_chroma_qp_offsets_present_flag = m_param->bHDR10Opt;

    pps->bConstrainedIntraPred = m_param->bEnableConstrainedIntra;
    pps->bUseWeightPred = m_param->bEnableWeightedPred;
    pps->bUseWeightedBiPred = m_param->bEnableWeightedBiPred;
    pps->bTransquantBypassEnabled = m_param->bCULossless || m_param->bLossless;
    pps->bTransformSkipEnabled = m_param->bEnableTransformSkip;
    pps->bSignHideEnabled = m_param->bEnableSignHiding;

    pps->bDeblockingFilterControlPresent = !m_param->bEnableLoopFilter || m_param->deblockingFilterBetaOffset || m_param->deblockingFilterTCOffset;
    pps->bPicDisableDeblockingFilter = !m_param->bEnableLoopFilter;
    pps->deblockingFilterBetaOffsetDiv2 = m_param->deblockingFilterBetaOffset;
    pps->deblockingFilterTcOffsetDiv2 = m_param->deblockingFilterTCOffset;

    pps->bEntropyCodingSyncEnabled = m_param->bEnableWavefront;

    pps->numRefIdxDefault[0] = 1;
    pps->numRefIdxDefault[1] = 1;
}

void Encoder::configureZone(x265_param *p, x265_param *zone)
{
    if (m_param->bResetZoneConfig)
    {
        p->maxNumReferences = zone->maxNumReferences;
        p->bEnableFastIntra = zone->bEnableFastIntra;
        p->bEnableEarlySkip = zone->bEnableEarlySkip;
        p->recursionSkipMode = zone->recursionSkipMode;
        p->searchMethod = zone->searchMethod;
        p->searchRange = zone->searchRange;
        p->subpelRefine = zone->subpelRefine;
        p->rdoqLevel = zone->rdoqLevel;
        p->rdLevel = zone->rdLevel;
        p->bEnableRectInter = zone->bEnableRectInter;
        p->maxNumMergeCand = zone->maxNumMergeCand;
        p->bIntraInBFrames = zone->bIntraInBFrames;
        if (zone->scalingLists)
            p->scalingLists = strdup(zone->scalingLists);

        p->rc.aqMode = zone->rc.aqMode;
        p->rc.aqStrength = zone->rc.aqStrength;
        p->noiseReductionInter = zone->noiseReductionInter;
        p->noiseReductionIntra = zone->noiseReductionIntra;

        p->limitModes = zone->limitModes;
        p->bEnableSplitRdSkip = zone->bEnableSplitRdSkip;
        p->bCULossless = zone->bCULossless;
        p->bEnableRdRefine = zone->bEnableRdRefine;
        p->limitTU = zone->limitTU;
        p->bEnableTSkipFast = zone->bEnableTSkipFast;
        p->rdPenalty = zone->rdPenalty;
        p->dynamicRd = zone->dynamicRd;
        p->bEnableTransformSkip = zone->bEnableTransformSkip;
        p->bEnableAMP = zone->bEnableAMP;

        if (m_param->rc.rateControlMode == X265_RC_ABR)
            p->rc.bitrate = zone->rc.bitrate;
        if (m_param->rc.rateControlMode == X265_RC_CRF)
            p->rc.rfConstant = zone->rc.rfConstant;
        if (m_param->rc.rateControlMode == X265_RC_CQP)
        {
            p->rc.qp = zone->rc.qp;
            p->rc.aqMode = X265_AQ_NONE;
            p->rc.hevcAq = 0;
        }
        p->radl = zone->radl;
    }
    memcpy(zone, p, sizeof(x265_param));
}


void Encoder::configure(x265_param *p)
{
    this->m_param = p;
}

