/*****************************************************************************
 * Copyright (C) 2013-2020 MulticoreWare, Inc
 *
 * Authors: Gopu Govindaswamy <gopu@multicorewareinc.com>
 *          Steve Borho <steve@borho.org>
 *          Ashok Kumar Mishra <ashok@multicorewareinc.com>
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
#include "frame.h"
#include "framedata.h"
#include "picyuv.h"
#include "primitives.h"
#include "mv.h"

#include "slicetype.h"
#include "motion.h"

#if DETAILED_CU_STATS
#define ProfileLookaheadTime(elapsed, count) ScopedElapsedTime _scope(elapsed); count++
#else
#define ProfileLookaheadTime(elapsed, count)
#endif

using namespace X265_NS;

namespace {

uint32_t acEnergyVarHist(uint64_t sum_ssd, int shift)
{
    uint32_t sum = (uint32_t)sum_ssd;
    uint32_t ssd = (uint32_t)(sum_ssd >> 32);

    return ssd - ((uint64_t)sum * sum >> shift);
}

/* Compute variance to derive AC energy of each block */
inline uint32_t acEnergyVar(Frame *curFrame, uint64_t sum_ssd, int shift, int plane)
{
    uint32_t sum = (uint32_t)sum_ssd;
    uint32_t ssd = (uint32_t)(sum_ssd >> 32);

    //curFrame->m_lowres.wp_sum[plane] += sum;
    //curFrame->m_lowres.wp_ssd[plane] += ssd;
    return ssd - ((uint64_t)sum * sum >> shift);
}

/* Find the energy of each block in Y/Cb/Cr plane */
inline uint32_t acEnergyPlane(Frame *curFrame, pixel* src, intptr_t srcStride, int plane, int colorFormat, uint32_t qgSize)
{
    if ((colorFormat != X265_CSP_I444) && plane)
    {
        if (qgSize == 8)
        {
            ALIGN_VAR_4(pixel, pix[4 * 4]);
            primitives.cu[BLOCK_4x4].copy_pp(pix, 4, src, srcStride);
            return acEnergyVar(curFrame, primitives.cu[BLOCK_4x4].var(pix, 4), 4, plane);
        }
        else
        {
            ALIGN_VAR_8(pixel, pix[8 * 8]);
            primitives.cu[BLOCK_8x8].copy_pp(pix, 8, src, srcStride);
            return acEnergyVar(curFrame, primitives.cu[BLOCK_8x8].var(pix, 8), 6, plane);
        }
    }
    else
    {
        if (qgSize == 8)
            return acEnergyVar(curFrame, primitives.cu[BLOCK_8x8].var(src, srcStride), 6, plane);
        else
            return acEnergyVar(curFrame, primitives.cu[BLOCK_16x16].var(src, srcStride), 8, plane);
    }
}

} // end anonymous namespace

namespace X265_NS {

bool computeEdge(pixel* edgePic, pixel* refPic, pixel* edgeTheta, intptr_t stride, int height, int width, bool bcalcTheta, pixel whitePixel)
{
    intptr_t rowOne = 0, rowTwo = 0, rowThree = 0, colOne = 0, colTwo = 0, colThree = 0;
    intptr_t middle = 0, topLeft = 0, topRight = 0, bottomLeft = 0, bottomRight = 0;

    const int startIndex = 1;

    if (!edgePic || !refPic || (!edgeTheta && bcalcTheta))
    {
        return false;
    }
    else
    {
        float gradientH = 0, gradientV = 0, radians = 0, theta = 0;
        float gradientMagnitude = 0;
        pixel blackPixel = 0;

        //Applying Sobel filter expect for border pixels
        height = height - startIndex;
        width = width - startIndex;
        for (int rowNum = startIndex; rowNum < height; rowNum++)
        {
            rowTwo = rowNum * stride;
            rowOne = rowTwo - stride;
            rowThree = rowTwo + stride;

            for (int colNum = startIndex; colNum < width; colNum++)
            {

                 /*  Horizontal and vertical gradients
                     [ -3   0   3 ]        [-3   -10  -3 ]
                 gH =[ -10  0   10]   gV = [ 0    0    0 ]
                     [ -3   0   3 ]        [ 3    10   3 ] */

                colTwo = colNum;
                colOne = colTwo - startIndex;
                colThree = colTwo + startIndex;
                middle = rowTwo + colTwo;
                topLeft = rowOne + colOne;
                topRight = rowOne + colThree;
                bottomLeft = rowThree + colOne;
                bottomRight = rowThree + colThree;
                gradientH = (float)(-3 * refPic[topLeft] + 3 * refPic[topRight] - 10 * refPic[rowTwo + colOne] + 10 * refPic[rowTwo + colThree] - 3 * refPic[bottomLeft] + 3 * refPic[bottomRight]);
                gradientV = (float)(-3 * refPic[topLeft] - 10 * refPic[rowOne + colTwo] - 3 * refPic[topRight] + 3 * refPic[bottomLeft] + 10 * refPic[rowThree + colTwo] + 3 * refPic[bottomRight]);
                gradientMagnitude = sqrtf(gradientH * gradientH + gradientV * gradientV);
                if(bcalcTheta) 
                {
                    edgeTheta[middle] = 0;
                    radians = atan2(gradientV, gradientH);
                    theta = (float)((radians * 180) / PI);
                    if (theta < 0)
                       theta = 180 + theta;
                    edgeTheta[middle] = (pixel)theta;
                }
                edgePic[middle] = (pixel)(gradientMagnitude >= EDGE_THRESHOLD ? whitePixel : blackPixel);
            }
        }
        return true;
    }
}

void edgeFilter(Frame *curFrame, x265_param* param)
{
    int height = curFrame->m_fencPic->m_picHeight;
    int width = curFrame->m_fencPic->m_picWidth;
    intptr_t stride = curFrame->m_fencPic->m_stride;
    uint32_t numCuInHeight = (height + param->maxCUSize - 1) / param->maxCUSize;
    int maxHeight = numCuInHeight * param->maxCUSize;

    memset(curFrame->m_edgePic, 0, stride * (maxHeight + (curFrame->m_fencPic->m_lumaMarginY * 2)) * sizeof(pixel));
    memset(curFrame->m_gaussianPic, 0, stride * (maxHeight + (curFrame->m_fencPic->m_lumaMarginY * 2)) * sizeof(pixel));
    memset(curFrame->m_thetaPic, 0, stride * (maxHeight + (curFrame->m_fencPic->m_lumaMarginY * 2)) * sizeof(pixel));

    pixel *src = (pixel*)curFrame->m_fencPic->m_picOrg[0];
    pixel *edgePic = curFrame->m_edgePic + curFrame->m_fencPic->m_lumaMarginY * stride + curFrame->m_fencPic->m_lumaMarginX;
    pixel *refPic = curFrame->m_gaussianPic + curFrame->m_fencPic->m_lumaMarginY * stride + curFrame->m_fencPic->m_lumaMarginX;
    pixel *edgeTheta = curFrame->m_thetaPic + curFrame->m_fencPic->m_lumaMarginY * stride + curFrame->m_fencPic->m_lumaMarginX;

    for (int i = 0; i < height; i++)
    {
        memcpy(edgePic, src, width * sizeof(pixel));
        memcpy(refPic, src, width * sizeof(pixel));
        src += stride;
        edgePic += stride;
        refPic += stride;
    }

    //Applying Gaussian filter on the picture
    src = (pixel*)curFrame->m_fencPic->m_picOrg[0];
    refPic = curFrame->m_gaussianPic + curFrame->m_fencPic->m_lumaMarginY * stride + curFrame->m_fencPic->m_lumaMarginX;
    edgePic = curFrame->m_edgePic + curFrame->m_fencPic->m_lumaMarginY * stride + curFrame->m_fencPic->m_lumaMarginX;
    pixel pixelValue = 0;

    for (int rowNum = 0; rowNum < height; rowNum++)
    {
        for (int colNum = 0; colNum < width; colNum++)
        {
            if ((rowNum >= 2) && (colNum >= 2) && (rowNum < height - 2) && (colNum < width - 2)) //Ignoring the border pixels of the picture
            {
                /*  5x5 Gaussian filter
                    [2   4   5   4   2]
                 1  [4   9   12  9   4]
                --- [5   12  15  12  5]
                159 [4   9   12  9   4]
                    [2   4   5   4   2]*/

                const intptr_t rowOne = (rowNum - 2)*stride, colOne = colNum - 2;
                const intptr_t rowTwo = (rowNum - 1)*stride, colTwo = colNum - 1;
                const intptr_t rowThree = rowNum * stride, colThree = colNum;
                const intptr_t rowFour = (rowNum + 1)*stride, colFour = colNum + 1;
                const intptr_t rowFive = (rowNum + 2)*stride, colFive = colNum + 2;
                const intptr_t index = (rowNum*stride) + colNum;

                pixelValue = ((2 * src[rowOne + colOne] + 4 * src[rowOne + colTwo] + 5 * src[rowOne + colThree] + 4 * src[rowOne + colFour] + 2 * src[rowOne + colFive] +
                    4 * src[rowTwo + colOne] + 9 * src[rowTwo + colTwo] + 12 * src[rowTwo + colThree] + 9 * src[rowTwo + colFour] + 4 * src[rowTwo + colFive] +
                    5 * src[rowThree + colOne] + 12 * src[rowThree + colTwo] + 15 * src[rowThree + colThree] + 12 * src[rowThree + colFour] + 5 * src[rowThree + colFive] +
                    4 * src[rowFour + colOne] + 9 * src[rowFour + colTwo] + 12 * src[rowFour + colThree] + 9 * src[rowFour + colFour] + 4 * src[rowFour + colFive] +
                    2 * src[rowFive + colOne] + 4 * src[rowFive + colTwo] + 5 * src[rowFive + colThree] + 4 * src[rowFive + colFour] + 2 * src[rowFive + colFive]) / 159);
                refPic[index] = pixelValue;
            }
        }
    }

    if(!computeEdge(edgePic, refPic, edgeTheta, stride, height, width, true))
        x265_log(NULL, X265_LOG_ERROR, "Failed edge computation!");
}

//Find the angle of a block by averaging the pixel angles 
inline void findAvgAngle(const pixel* block, intptr_t stride, uint32_t size, uint32_t &angle)
{
    int sum = 0;
    for (uint32_t y = 0; y < size; y++)
    {
        for (uint32_t x = 0; x < size; x++)
        {
            sum += block[x];
        }
        block += stride;
    }
    angle = sum / (size*size);
}

uint32_t LookaheadTLD::edgeDensityCu(Frame* curFrame, uint32_t &avgAngle, uint32_t blockX, uint32_t blockY, uint32_t qgSize)
{
    pixel *edgeImage = curFrame->m_edgePic + curFrame->m_fencPic->m_lumaMarginY * curFrame->m_fencPic->m_stride + curFrame->m_fencPic->m_lumaMarginX;
    pixel *edgeTheta = curFrame->m_thetaPic + curFrame->m_fencPic->m_lumaMarginY * curFrame->m_fencPic->m_stride + curFrame->m_fencPic->m_lumaMarginX;
    intptr_t srcStride = curFrame->m_fencPic->m_stride;
    intptr_t blockOffsetLuma = blockX + (blockY * srcStride);
    int plane = 0; // Sobel filter is applied only on Y component
    uint32_t var;

    if (qgSize == 8)
    {
        findAvgAngle(edgeTheta + blockOffsetLuma, srcStride, qgSize, avgAngle);
        var = acEnergyVar(curFrame, primitives.cu[BLOCK_8x8].var(edgeImage + blockOffsetLuma, srcStride), 6, plane);
    }
    else
    {
        findAvgAngle(edgeTheta + blockOffsetLuma, srcStride, 16, avgAngle);
        var = acEnergyVar(curFrame, primitives.cu[BLOCK_16x16].var(edgeImage + blockOffsetLuma, srcStride), 8, plane);
    }
    x265_emms();
    return var;
}

/* Find the total AC energy of each block in all planes */
uint32_t LookaheadTLD::acEnergyCu(Frame* curFrame, uint32_t blockX, uint32_t blockY, int csp, uint32_t qgSize)
{
    intptr_t stride = curFrame->m_fencPic->m_stride;
    intptr_t cStride = curFrame->m_fencPic->m_strideC;
    intptr_t blockOffsetLuma = blockX + (blockY * stride);
    int hShift = CHROMA_H_SHIFT(csp);
    int vShift = CHROMA_V_SHIFT(csp);
    intptr_t blockOffsetChroma = (blockX >> hShift) + ((blockY >> vShift) * cStride);

    uint32_t var;

    var  = acEnergyPlane(curFrame, curFrame->m_fencPic->m_picOrg[0] + blockOffsetLuma, stride, 0, csp, qgSize);
    if (csp != X265_CSP_I400 && curFrame->m_fencPic->m_picCsp != X265_CSP_I400)
    {
        var += acEnergyPlane(curFrame, curFrame->m_fencPic->m_picOrg[1] + blockOffsetChroma, cStride, 1, csp, qgSize);
        var += acEnergyPlane(curFrame, curFrame->m_fencPic->m_picOrg[2] + blockOffsetChroma, cStride, 2, csp, qgSize);
    }
    x265_emms();
    return var;
}

Lookahead::Lookahead(x265_param *param, ThreadPool* pool)
{
    m_param = param;
    m_pool  = pool;

    m_tld      = NULL;
    m_filled   = false;
    m_outputSignalRequired = false;
    m_isActive = true;
    m_inputCount = 0;
    m_extendGopBoundary = false;
    //m_8x8Height = ((m_param->sourceHeight / 2) + X265_LOWRES_CU_SIZE - 1) >> X265_LOWRES_CU_BITS;
    //m_8x8Width = ((m_param->sourceWidth / 2) + X265_LOWRES_CU_SIZE - 1) >> X265_LOWRES_CU_BITS;
    //m_4x4Height = ((m_param->sourceHeight / 4) + X265_LOWRES_CU_SIZE - 1) >> X265_LOWRES_CU_BITS;
    //m_4x4Width = ((m_param->sourceWidth / 4) + X265_LOWRES_CU_SIZE - 1) >> X265_LOWRES_CU_BITS;
    //m_cuCount = m_8x8Width * m_8x8Height;
    //m_8x8Blocks = m_8x8Width > 2 && m_8x8Height > 2 ? (m_cuCount + 4 - 2 * (m_8x8Width + m_8x8Height)) : m_cuCount;


    m_sliceTypeBusy = false;


    /* It is also beneficial to pre-calculate all possible frame cost estimates
     * using worker threads bonded to the worker thread running
     * slicetypeDecide(). This creates bframes * bframes jobs which take less
     * time than the motion search batches but there are many of them. This may
     * do much unnecessary work, some frame cost estimates are not needed, so if
     * the thread pool is small we disable this feature after the initial burst
     * of work */
    m_bBatchFrameCosts = m_bBatchMotionSearch;


    m_resetRunningAvg = true;

}


bool Lookahead::create()
{
    int numTLD = 1 + (m_pool ? m_pool->m_numWorkers : 0);
    m_tld = new LookaheadTLD[numTLD];
    //for (int i = 0; i < numTLD; i++)
    //    m_tld[i].init(m_8x8Width, m_8x8Height, m_8x8Blocks);
    //m_scratch = X265_MALLOC(int, m_tld[0].widthInCU);

    return m_tld ; //&& m_scratch;
}

void Lookahead::stopJobs()
{
    if (m_pool && !m_inputQueue.empty())
    {
        m_inputLock.acquire();
        m_isActive = false;
        bool wait = m_outputSignalRequired = m_sliceTypeBusy;
        m_inputLock.release();

        if (wait)
            m_outputSignal.wait();
    }
}

void Lookahead::destroy()
{
    // these two queues will be empty unless the encode was aborted
    while (!m_inputQueue.empty())
    {
        Frame* curFrame = m_inputQueue.popFront();
        curFrame->destroy();
        delete curFrame;
    }

    while (!m_outputQueue.empty())
    {
        Frame* curFrame = m_outputQueue.popFront();
        curFrame->destroy();
        delete curFrame;
    }

    delete [] m_tld;
}

/* The synchronization of slicetypeDecide is managed here.  The findJob() method
 * polls the occupancy of the input queue. If the queue is
 * full, it will run slicetypeDecide() and output a mini-gop of frames to the
 * output queue. If the flush() method has been called (implying no new pictures
 * will be received) then the input queue is considered full if it has even one
 * picture left. getDecidedPicture() removes pictures from the output queue and
 * only blocks as a last resort. It does not start removing pictures until
 * m_filled is true, which occurs after *more than* the lookahead depth of
 * pictures have been input so slicetypeDecide() should have started prior to
 * output pictures being withdrawn. The first slicetypeDecide() will obviously
 * still require a blocking wait, but after this slicetypeDecide() will maintain
 * its lead over the encoder (because one picture is added to the input queue
 * each time one is removed from the output) and decides slice types of pictures
 * just ahead of when the encoder needs them */


void Lookahead::addPicture(Frame& curFrame)
{
    m_inputLock.acquire();
    m_inputQueue.pushBack(curFrame);
    m_inputLock.release();
    m_inputCount++;
}

/* Called by API thread */
void Lookahead::flush()
{
    /* force slicetypeDecide to run until the input queue is empty */
    m_fullQueueSize = 1;
    m_filled = true;
}

void Lookahead::setLookaheadQueue()
{
    m_filled = false;
    m_fullQueueSize = X265_MAX(1, m_param->lookaheadDepth);
}

void Lookahead::findJob(int /*workerThreadID*/)
{
    bool doDecide;

    m_inputLock.acquire();
    if (m_inputQueue.size() >= m_fullQueueSize && !m_sliceTypeBusy && m_isActive)
        doDecide = m_sliceTypeBusy = true;
    else
        doDecide = m_helpWanted = false;
    m_inputLock.release();

    if (!doDecide)
        return;

    ProfileLookaheadTime(m_slicetypeDecideElapsedTime, m_countSlicetypeDecide);
    ProfileScopeEvent(slicetypeDecideEV);

    slicetypeDecide();

    m_inputLock.acquire();
    if (m_outputSignalRequired)
    {
        m_outputSignal.trigger();
        m_outputSignalRequired = false;
    }
    m_sliceTypeBusy = false;
    m_inputLock.release();
}

/* Called by API thread */
Frame* Lookahead::getDecidedPicture()
{
    if (m_filled)
    {
        m_outputLock.acquire();
        Frame *out = m_outputQueue.popFront();
        m_outputLock.release();

        if (out)
        {
            m_inputCount--;
            return out;
        }

        findJob(-1); /* run slicetypeDecide() if necessary */

        m_inputLock.acquire();
        bool wait = m_outputSignalRequired = m_sliceTypeBusy;
        m_inputLock.release();

        if (wait)
            m_outputSignal.wait();

        out = m_outputQueue.popFront();
        if (out)
            m_inputCount--;
        return out;
    }
    else
        return NULL;
}


uint32_t LookaheadTLD::calcVariance(pixel* inpSrc, intptr_t stride, intptr_t blockOffset, uint32_t plane)
{
    pixel* src = inpSrc + blockOffset;

    uint32_t var;
    if (!plane)
        var = acEnergyVarHist(primitives.cu[BLOCK_8x8].var(src, stride), 6);
    else
        var = acEnergyVarHist(primitives.cu[BLOCK_4x4].var(src, stride), 4);

    x265_emms();
    return var;
}

void PreLookaheadGroup::processTasks(int workerThreadID)
{
    if (workerThreadID < 0)
        workerThreadID = m_lookahead.m_pool ? m_lookahead.m_pool->m_numWorkers : 0;
    LookaheadTLD& tld = m_lookahead.m_tld[workerThreadID];

    m_lock.acquire();
    while (m_jobAcquired < m_jobTotal)
    {
        Frame* preFrame = m_preframes[m_jobAcquired++];
        ProfileLookaheadTime(m_lookahead.m_preLookaheadElapsedTime, m_lookahead.m_countPreLookahead);
        ProfileScopeEvent(prelookahead);
        m_lock.release();
        m_lock.acquire();
    }
    m_lock.release();
}




/* called by API thread or worker thread with inputQueueLock acquired */
void Lookahead::slicetypeDecide()
{
    PreLookaheadGroup pre(*this);
    Lowres* frames[X265_LOOKAHEAD_MAX + X265_BFRAME_MAX + 4];
    Frame*  list[X265_BFRAME_MAX + 4];
    memset(frames, 0, sizeof(frames));
    memset(list, 0, sizeof(list));
    int maxSearch = X265_MIN(m_param->lookaheadDepth, X265_LOOKAHEAD_MAX);
    maxSearch = X265_MAX(1, maxSearch);

    {
        ScopedLock lock(m_inputLock);

        Frame *curFrame = m_inputQueue.first();
        int j;
        for (j = 0; j < 2; j++)
        {
            if (!curFrame) break;
            list[j] = curFrame;
            curFrame = curFrame->m_next;
        }
    }
        m_inputLock.acquire();
        /* dequeue all frames from inputQueue that are about to be enqueued
         * in the output queue. The order is important because Frame can
         * only be in one list at a time */
        int64_t pts[X265_BFRAME_MAX + 1];
        for (int i = 0; i <= 0; i++)
        {
            Frame *curFrame;
            curFrame = m_inputQueue.popFront();
            pts[i] = curFrame->m_pts;
            maxSearch--;
        }
        m_inputLock.release();

        m_outputLock.acquire();

        m_outputQueue.pushBack(*list[0]);

        m_outputLock.release();

}

}
