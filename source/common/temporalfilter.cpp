/*****************************************************************************
* Copyright (C) 2013-2021 MulticoreWare, Inc
*
 * Authors: Ashok Kumar Mishra <ashok@multicorewareinc.com>
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
#include "temporalfilter.h"
#include "primitives.h"

#include "frame.h"
#include "slice.h"
#include "framedata.h"
#include "analysis.h"
#include "threadpool.h"

using namespace X265_NS;

void OrigPicBuffer::addPicture(Frame* inFrame)
{
    m_mcstfPicList.pushFrontMCSTF(*inFrame);
}

void OrigPicBuffer::addEncPicture(Frame* inFrame)
{
    m_mcstfOrigPicFreeList.pushFrontMCSTF(*inFrame);
}

void OrigPicBuffer::addEncPictureToPicList(Frame* inFrame)
{
    m_mcstfOrigPicList.pushFrontMCSTF(*inFrame);
}

OrigPicBuffer::~OrigPicBuffer()
{
    while (!m_mcstfOrigPicList.empty())
    {
        Frame* curFrame = m_mcstfOrigPicList.popBackMCSTF();
        curFrame->destroy();
        delete curFrame;
    }

    while (!m_mcstfOrigPicFreeList.empty())
    {
        Frame* curFrame = m_mcstfOrigPicFreeList.popBackMCSTF();
        curFrame->destroy();
        delete curFrame;
    }
}

void OrigPicBuffer::setOrigPicList(Frame* inFrame, int frameCnt)
{
    Slice* slice = inFrame->m_encData->m_slice;
    uint8_t j = 0;
    for (int iterPOC = (inFrame->m_poc - inFrame->m_mcstf->m_range);
        iterPOC <= (inFrame->m_poc + inFrame->m_mcstf->m_range); iterPOC++)
    {
        if (iterPOC != inFrame->m_poc)
        {
            if (iterPOC < 0)
                continue;
            if (iterPOC >= frameCnt)
                break;

            Frame *iterFrame = m_mcstfPicList.getPOCMCSTF(iterPOC);
            X265_CHECK(iterFrame, "Reference frame not found in OPB");
            if (iterFrame != NULL)
            {
                slice->m_mcstfRefFrameList[1][j] = iterFrame;
                iterFrame->m_refPicCnt[1]--;
            }

            iterFrame = m_mcstfOrigPicList.getPOCMCSTF(iterPOC);
            if (iterFrame != NULL)
            {

                slice->m_mcstfRefFrameList[1][j] = iterFrame;

                iterFrame->m_refPicCnt[1]--;
                Frame *cFrame = m_mcstfOrigPicList.getPOCMCSTF(inFrame->m_poc);
                X265_CHECK(cFrame, "Reference frame not found in encoded OPB");
                cFrame->m_refPicCnt[1]--;
            }
            j++;
        }
    }
}

void OrigPicBuffer::recycleOrigPicList()
{
    Frame *iterFrame = m_mcstfPicList.first();

    while (iterFrame)
    {
        Frame *curFrame = iterFrame;
        iterFrame = iterFrame->m_nextMCSTF;
        if (!curFrame->m_refPicCnt[1])
        {
            m_mcstfPicList.removeMCSTF(*curFrame);
            iterFrame = m_mcstfPicList.first();
        }
    }

    iterFrame = m_mcstfOrigPicList.first();

    while (iterFrame)
    {
        Frame *curFrame = iterFrame;
        iterFrame = iterFrame->m_nextMCSTF;
        if (!curFrame->m_refPicCnt[1])
        {
            m_mcstfOrigPicList.removeMCSTF(*curFrame);
            *curFrame->m_isSubSampled = false;
            m_mcstfOrigPicFreeList.pushFrontMCSTF(*curFrame);
            iterFrame = m_mcstfOrigPicList.first();
        }
    }
}

void OrigPicBuffer::addPictureToFreelist(Frame* inFrame)
{
    m_mcstfOrigPicFreeList.pushBack(*inFrame);
}

TemporalFilter::TemporalFilter()
{
    m_sourceWidth = 0;
    m_sourceHeight = 0,
    m_QP = 0;
    m_sliceTypeConfig = 3;
    m_numRef = 0;

    m_range = 2;
    m_chromaFactor = 0.55;
    m_sigmaMultiplier = 9.0;
    m_sigmaZeroPoint = 10.0;
}

TemporalFilter::~TemporalFilter()
{
    if (m_metld)
        delete m_metld;
}

void TemporalFilter::init(const x265_param* param)
{
    m_param = param;
    m_bitDepth = param->internalBitDepth;
    m_sourceWidth = param->sourceWidth;
    m_sourceHeight = param->sourceHeight;
    m_internalCsp = param->internalCsp;
    m_numComponents = (m_internalCsp != X265_CSP_I400) ? MAX_NUM_COMPONENT : 1;
    m_metld = new MotionEstimatorTLD;
}

int TemporalFilter::createRefPicInfo(TemporalFilterRefPicInfo* refFrame, x265_param* param)
{
    CHECKED_MALLOC_ZERO(refFrame->mvs, MV, sizeof(MV)* ((m_sourceWidth ) / 4) * ((m_sourceHeight ) / 4));
    refFrame->mvsStride = m_sourceWidth / 4;
    CHECKED_MALLOC_ZERO(refFrame->mvs0, MV, sizeof(MV)* ((m_sourceWidth ) / 16) * ((m_sourceHeight ) / 16));
    refFrame->mvsStride0 = m_sourceWidth / 16;
    CHECKED_MALLOC_ZERO(refFrame->mvs1, MV, sizeof(MV)* ((m_sourceWidth ) / 16) * ((m_sourceHeight ) / 16));
    refFrame->mvsStride1 = m_sourceWidth / 16;
    CHECKED_MALLOC_ZERO(refFrame->mvs2, MV, sizeof(MV)* ((m_sourceWidth ) / 16)*((m_sourceHeight ) / 16));
    refFrame->mvsStride2 = m_sourceWidth / 16;

    CHECKED_MALLOC_ZERO(refFrame->noise, int, sizeof(int) * ((m_sourceWidth) / 4) * ((m_sourceHeight) / 4));
    CHECKED_MALLOC_ZERO(refFrame->error, int, sizeof(int) * ((m_sourceWidth) / 4) * ((m_sourceHeight) / 4));

    refFrame->slicetype = X265_TYPE_AUTO;

    refFrame->compensatedPic = new PicYuv;
    refFrame->compensatedPic->create(param, true);

    return 1;
fail:
    return 0;
}

int MotionEstimatorTLD::motionErrorLumaSAD(MotionEstimatorTLD& m_metld,
    pixel* src,
    int stride,
    pixel* buf,
    int x,
    int y,
    int dx,
    int dy,
    int bs,
    int besterror)
{

    pixel* origOrigin = src;
    intptr_t origStride = stride;
    pixel *buffOrigin = buf;
    intptr_t buffStride = stride;
    int error = 0;// dx * 10 + dy * 10;
    if (((dx | dy) & 0xF) == 0)
    {
        dx /= m_motionVectorFactor;
        dy /= m_motionVectorFactor;

#if 1
        for (int y1 = 0; y1 < bs; y1++)
        {
            const pixel* origRowStart = origOrigin + (y + y1) * origStride + x;
            const pixel* bufferRowStart = buffOrigin + (y + y1 + dy) * buffStride + (x + dx);
            for (int x1 = 0; x1 < bs; x1+=2)
            {
                 int diff = origRowStart[x1] - bufferRowStart[x1];
                 error += diff * diff;
                diff = origRowStart[x1+1] - bufferRowStart[x1+1];
                error += diff * diff;
            }
            if(error > besterror)
            {
                return error;
            }

        }
#else
        int partEnum = partitionFromSizes(bs, bs);
        /* copy PU block into cache */
        primitives.pu[partEnum].copy_pp(predPUYuv.m_buf[0], FENC_STRIDE, bufferRowStart, buffStride);

        error = m_metld.me.bufSAD(predPUYuv.m_buf[0], FENC_STRIDE);
#endif
        if (error > besterror)
        {
            return error;
        }
    }
    else
    {
        const int *xFilter = s_interpolationFilter[dx & 0xF];
        const int *yFilter = s_interpolationFilter[dy & 0xF];
        int tempArray[64 + 8][64];

        int iSum, iBase;
        for (int y1 = 1; y1 < bs + 7; y1++)
        {
            const int yOffset = y + y1 + (dy >> 4) - 3;
            const pixel *sourceRow = buffOrigin + (yOffset)*buffStride + 0;
            for (int x1 = 0; x1 < bs; x1++)
            {
                iSum = 0;
                iBase = x + x1 + (dx >> 4) - 3;
                const pixel *rowStart = sourceRow + iBase;

                iSum += xFilter[1] * rowStart[1];
                iSum += xFilter[2] * rowStart[2];
                iSum += xFilter[3] * rowStart[3];
                iSum += xFilter[4] * rowStart[4];
                iSum += xFilter[5] * rowStart[5];
                iSum += xFilter[6] * rowStart[6];

                tempArray[y1][x1] = iSum;
            }
        }

        const pixel maxSampleValue = (1 << m_bitDepth) - 1;
        for (int y1 = 0; y1 < bs; y1++)
        {
            const pixel *origRow = origOrigin + (y + y1)*origStride + 0;
            for (int x1 = 0; x1 < bs; x1++)
            {
                iSum = 0;
                iSum += yFilter[1] * tempArray[y1 + 1][x1];
                iSum += yFilter[2] * tempArray[y1 + 2][x1];
                iSum += yFilter[3] * tempArray[y1 + 3][x1];
                iSum += yFilter[4] * tempArray[y1 + 4][x1];
                iSum += yFilter[5] * tempArray[y1 + 5][x1];
                iSum += yFilter[6] * tempArray[y1 + 6][x1];

                iSum = (iSum + (1 << 11)) >> 12;
                iSum = iSum < 0 ? 0 : (iSum > maxSampleValue ? maxSampleValue : iSum);

                error += abs(iSum - origRow[x + x1]);
            }
            if (error > besterror)
            {
                return error;
            }
        }
    }
    return error;
}

int MotionEstimatorTLD::motionErrorLumaSSD(MotionEstimatorTLD& m_metld,
    pixel* src,
    int stride,
    pixel* buf,
    int x,
    int y,
    int dx,
    int dy,
    int bs,
    int besterror)
{

    pixel* origOrigin = src;
    intptr_t origStride = stride;
    pixel *buffOrigin = buf;
    intptr_t buffStride = stride;
    int error = 0;// dx * 10 + dy * 10;
    if (((dx | dy) & 0xF) == 0)
    {
        dx /= m_motionVectorFactor;
        dy /= m_motionVectorFactor;

#if 1
        for (int y1 = 0; y1 < bs; y1++)
        {
            const pixel* origRowStart = origOrigin + (y + y1) * origStride + x;
            const pixel* bufferRowStart = buffOrigin + (y + y1 + dy) * buffStride + (x + dx);
            for (int x1 = 0; x1 < bs; x1+=2)
            {
                int diff = origRowStart[x1] - bufferRowStart[x1];
                error += diff * diff;
                diff = origRowStart[x1+1] - bufferRowStart[x1+1];
                error += diff * diff;
            }
            if(error > besterror)
            {
                return error;
            }

        }
#else
        int partEnum = partitionFromSizes(bs, bs);
        /* copy PU block into cache */
        primitives.pu[partEnum].copy_pp(predPUYuv.m_buf[0], FENC_STRIDE, bufferRowStart, buffStride);

        error = (int)primitives.cu[partEnum].sse_pp(m_metld.me.fencPUYuv.m_buf[0], FENC_STRIDE, predPUYuv.m_buf[0], FENC_STRIDE);

#endif
        if (error > besterror)
        {
            return error;
        }
    }
    else
    {
        const int *xFilter = s_interpolationFilter[dx & 0xF];
        const int *yFilter = s_interpolationFilter[dy & 0xF];
        int tempArray[64 + 8][64];

        int iSum, iBase;
        for (int y1 = 1; y1 < bs + 7; y1++)
        {
            const int yOffset = y + y1 + (dy >> 4) - 3;
            const pixel *sourceRow = buffOrigin + (yOffset)*buffStride + 0;
            for (int x1 = 0; x1 < bs; x1++)
            {
                iSum = 0;
                iBase = x + x1 + (dx >> 4) - 3;
                const pixel *rowStart = sourceRow + iBase;

                iSum += xFilter[1] * rowStart[1];
                iSum += xFilter[2] * rowStart[2];
                iSum += xFilter[3] * rowStart[3];
                iSum += xFilter[4] * rowStart[4];
                iSum += xFilter[5] * rowStart[5];
                iSum += xFilter[6] * rowStart[6];

                tempArray[y1][x1] = iSum;
            }
        }

        const pixel maxSampleValue = (1 << m_bitDepth) - 1;
        for (int y1 = 0; y1 < bs; y1++)
        {
            const pixel *origRow = origOrigin + (y + y1)*origStride + 0;
            for (int x1 = 0; x1 < bs; x1++)
            {
                iSum = 0;
                iSum += yFilter[1] * tempArray[y1 + 1][x1];
                iSum += yFilter[2] * tempArray[y1 + 2][x1];
                iSum += yFilter[3] * tempArray[y1 + 3][x1];
                iSum += yFilter[4] * tempArray[y1 + 4][x1];
                iSum += yFilter[5] * tempArray[y1 + 5][x1];
                iSum += yFilter[6] * tempArray[y1 + 6][x1];

                iSum = (iSum + (1 << 11)) >> 12;
                iSum = iSum < 0 ? 0 : (iSum > maxSampleValue ? maxSampleValue : iSum);

                error += (iSum - origRow[x + x1]) * (iSum - origRow[x + x1]);
            }
            if (error > besterror)
            {
                return error;
            }
        }
    }
    return error;
}

void TemporalFilter::create(x265_param* param, ThreadPool* pool)
{
        int numTLD = 1 + (pool ? pool->m_numWorkers + 1 : 0);
        m_metld = new MotionEstimatorTLD[numTLD];
        //init(param);
}

void TemporalFilter::destroy()
{
    delete m_metld;
}

void TemporalFilter::runMCSTF(Frame* pic, ThreadPool* pool)
{
    const int numRef = pic->m_mcstf->m_numRef;
    if (numRef == 0)
        return;

    const int blockSize = 16;
    const int numBlockRows = (pic->m_fencPic->m_picHeight + blockSize - 1) / blockSize;
    {
        MCSTFMEGroup phase2(*this, pool);
        phase2.initRowSync(numRef, numBlockRows, blockSize);

        for (int j = 0; j < numRef; j++)
        {
            if (pic->m_lowres.lowresMcstfMvs[0][j][0].x != 0x7FFF)
                continue;

            for (int row = 0; row < numBlockRows; row++)
                phase2.add_row(j, pic->m_mcstfRefList[j].poc,
                    pic->m_poc, pic, row);
        }

        phase2.finishBatch();
    }
}

void TemporalFilter::applyMotion(MV *mvs, uint32_t mvsStride, PicYuv *input, PicYuv *output)
{
    static const int lumaBlockSize = 8;
    int srcStride = 0;
    int dstStride = 0;
    int csx = 0, csy = 0;
    for (int c = 0; c < m_numComponents; c++)
    {
        const pixel maxValue = (1 << X265_DEPTH) - 1;

        const pixel *pSrcImage = input->m_picOrg[c];
        pixel *pDstImage = output->m_picOrg[c];

        if (!c)
        {
            srcStride = (int)input->m_stride;
            dstStride = (int)output->m_stride;
        }
        else
        {
            srcStride = (int)input->m_strideC;
            dstStride = (int)output->m_strideC;
            csx = CHROMA_H_SHIFT(m_internalCsp);
            csy = CHROMA_V_SHIFT(m_internalCsp);
        }
        const int blockSizeX = lumaBlockSize >> csx;
        const int blockSizeY = lumaBlockSize >> csy;
        const int height = input->m_picHeight >> csy;
        const int width = input->m_picWidth >> csx;

        for (int y = 0, blockNumY = 0; y + blockSizeY <= height; y += blockSizeY, blockNumY++)
        {
            for (int x = 0, blockNumX = 0; x + blockSizeX <= width; x += blockSizeX, blockNumX++)
            {
                int mvIdx = blockNumY * mvsStride + blockNumX;
                const MV &mv = mvs[mvIdx];
                const int dx = mv.x >> csx;
                const int dy = mv.y >> csy;
                const int xInt = mv.x >> (4 + csx);
                const int yInt = mv.y >> (4 + csy);

                const int *xFilter = s_interpolationFilter[dx & 0xf];
                const int *yFilter = s_interpolationFilter[dy & 0xf]; // will add 6 bit.
                const int numFilterTaps = 7;
                const int centreTapOffset = 3;

                int tempArray[lumaBlockSize + numFilterTaps][lumaBlockSize];

                for (int by = 1; by < blockSizeY + numFilterTaps; by++)
                {
                    const int yOffset = y + by + yInt - centreTapOffset;
                    const pixel *sourceRow = pSrcImage + yOffset * srcStride;
                    for (int bx = 0; bx < blockSizeX; bx++)
                    {
                        int iBase = x + bx + xInt - centreTapOffset;
                        const pixel *rowStart = sourceRow + iBase;

                        int iSum = 0;
                        iSum += xFilter[1] * rowStart[1];
                        iSum += xFilter[2] * rowStart[2];
                        iSum += xFilter[3] * rowStart[3];
                        iSum += xFilter[4] * rowStart[4];
                        iSum += xFilter[5] * rowStart[5];
                        iSum += xFilter[6] * rowStart[6];

                        tempArray[by][bx] = iSum;
                    }
                }

                pixel *pDstRow = pDstImage + y * dstStride;
                for (int by = 0; by < blockSizeY; by++, pDstRow += dstStride)
                {
                    pixel *pDstPel = pDstRow + x;
                    for (int bx = 0; bx < blockSizeX; bx++, pDstPel++)
                    {
                        int iSum = 0;

                        iSum += yFilter[1] * tempArray[by + 1][bx];
                        iSum += yFilter[2] * tempArray[by + 2][bx];
                        iSum += yFilter[3] * tempArray[by + 3][bx];
                        iSum += yFilter[4] * tempArray[by + 4][bx];
                        iSum += yFilter[5] * tempArray[by + 5][bx];
                        iSum += yFilter[6] * tempArray[by + 6][bx];

                        iSum = (iSum + (1 << 11)) >> 12;
                        iSum = iSum < 0 ? 0 : (iSum > maxValue ? maxValue : iSum);
                        *pDstPel = (pixel)iSum;
                    }
                }
            }
        }
    }
}

void MCSTFMEGroup::processTasks(int workerThreadID)
{
    ThreadPool* pool = m_pool;
    int id = workerThreadID;
    if (workerThreadID < 0)
    {
        // Fix: Clamp the master thread to the very last index of the pool array safely
        id = (pool && pool->m_numWorkers > 0) ? (pool->m_numWorkers) : 0;
    }

    MotionEstimatorTLD& m_metld = m_mcstf.m_metld[id];

    // 2. ELIMINATE THE LOCK: Use x265's atomic fetch-and-add tool instead.
    // Assuming m_tasksAllocated is a ThreadSafeInteger defined in your class header.
    int i = m_tasksAllocated.getIncr(1);

    while (i < m_jobTotal)
    {
        Estimate& e = m_estimates[i];

        printf("Job created by thread %d\n", workerThreadID);
        // This heavy ME calculation now runs completely lock-free and fully parallelized
        estimatelowresmotion_doubleres(m_metld, e.frame, e.p0, e.blockRow);

        // Atomically grab the next available job index without stopping other threads
        i = m_tasksAllocated.getIncr(1);
    }
}


void MCSTFMEGroup::estimatelowresmotion_doubleres(MotionEstimatorTLD& metld, Frame* curFrame, int refId, int row)
{
    metld.m_bitDepth = curFrame->m_param->internalBitDepth;

    TemporalFilterRefPicInfo* ref = &curFrame->m_mcstfRefList[refId];

    // All pointers come straight from ref/curFrame — no allocation.
    PicYuv* orig = curFrame->m_fencPic;
    PicYuv* buffer = ref->picBuffer;
    MV* mvs = ref->mvs;
    uint32_t mvStride = ref->mvsStride;
    MV* previous = ref->mvs2;       // integer-pel seeds from Phase-1
    uint32_t prevStride = ref->mvsStride2;
    int* minError = ref->error;
    const int rowSize = m_mctfUnitSize; // e.g. 16
    const int blockSize = 8;
    const int stepSize = blockSize;
    const int factor = 1;

    const int origWidth = orig->m_picWidth;
    const int origHeight = orig->m_picHeight;
    int rowStart = row * rowSize;

    if (row * rowSize > origHeight)
        return;   // row beyond frame edge — nothing to do

    // if (blockRow > 0)
    // {
    //     while (m_rowDone[refId][blockRow - 1] == 0)
    //     {
    //         GIVE_UP_TIME();
    //     }
    // }
    int rowEnd = X265_MIN(rowStart + rowSize, origHeight);
    for (int blockY = rowStart; blockY + blockSize <= rowEnd; blockY += stepSize)
    {
        for (int blockX = 0; blockX + blockSize <= origWidth; blockX += stepSize)
        {
            const intptr_t pelOffset = blockY * orig->m_stride + blockX;
            metld.me.setSourcePU(orig->m_picOrg[0], orig->m_stride,
                pelOffset, blockSize, blockSize,
                X265_HEX_SEARCH, 1);

            MV  best(0, 0);
            int leastError = INT_MAX;
            int range = 0;
            int error;

            if (previous == NULL)
            {
                range = 8;
            }
            else
            {
                // 3×3 neighbourhood from lower-res seed MVs
                for (int py = -1; py <= 1; py++)
                {
                    int testy = blockY / (2 * blockSize) + py;
                    for (int px = -1; px <= 1; px++)
                    {
                        int testx = blockX / (2 * blockSize) + px;
                        if (testx >= 0 && testx < origWidth / (2 * blockSize) &&
                            testy >= 0 && testy < origHeight / (2 * blockSize))
                        {
                            MV old = previous[testy * prevStride + testx];
                            error = metld.m_useSADinME
                                ? metld.motionErrorLumaSAD(metld,
                                    orig->m_picOrg[0], (int)orig->m_stride,
                                    buffer->m_picOrg[0], blockX, blockY,
                                    old.x * factor, old.y * factor,
                                    blockSize, leastError)
                                : metld.motionErrorLumaSSD(metld,
                                    orig->m_picOrg[0], (int)orig->m_stride,
                                    buffer->m_picOrg[0], blockX, blockY,
                                    old.x * factor, old.y * factor,
                                    blockSize, leastError);

                            if (error < leastError)
                            {
                                best.set(old.x * factor, old.y * factor);
                                leastError = error;
                            }
                        }
                    }
                }

                // zero-MV candidate
                error = metld.m_useSADinME
                    ? metld.motionErrorLumaSAD(metld,
                        orig->m_picOrg[0], (int)orig->m_stride,
                        buffer->m_picOrg[0], blockX, blockY,
                        0, 0, blockSize, leastError)
                    : metld.motionErrorLumaSSD(metld,
                        orig->m_picOrg[0], (int)orig->m_stride,
                        buffer->m_picOrg[0], blockX, blockY,
                        0, 0, blockSize, leastError);

                if (error < leastError)
                {
                    best.set(0, 0);
                    leastError = error;
                }
            }

            // Integer search around best
            MV prevBest = best;
            for (int y2 = prevBest.y / metld.m_motionVectorFactor - range;
                y2 <= prevBest.y / metld.m_motionVectorFactor + range; y2++)
            {
                for (int x2 = prevBest.x / metld.m_motionVectorFactor - range;
                    x2 <= prevBest.x / metld.m_motionVectorFactor + range; x2++)
                {
                    error = metld.m_useSADinME
                        ? metld.motionErrorLumaSAD(metld,
                            orig->m_picOrg[0], (int)orig->m_stride,
                            buffer->m_picOrg[0], blockX, blockY,
                            x2 * metld.m_motionVectorFactor,
                            y2 * metld.m_motionVectorFactor,
                            blockSize, leastError)
                        : metld.motionErrorLumaSSD(metld,
                            orig->m_picOrg[0], (int)orig->m_stride,
                            buffer->m_picOrg[0], blockX, blockY,
                            x2 * metld.m_motionVectorFactor,
                            y2 * metld.m_motionVectorFactor,
                            blockSize, leastError);

                    if (error < leastError)
                    {
                        best.set(x2 * metld.m_motionVectorFactor,
                            y2 * metld.m_motionVectorFactor);
                        leastError = error;
                    }
                }
            }

            // Sub-pel refinement
            prevBest = best;
            int doubleRange = 3 * 4;
            for (int y2 = prevBest.y - doubleRange;
                y2 <= prevBest.y + doubleRange; y2 += 4)
            {
                for (int x2 = prevBest.x - doubleRange;
                    x2 <= prevBest.x + doubleRange; x2 += 4)
                {
                    error = metld.m_useSADinME
                        ? metld.motionErrorLumaSAD(metld,
                            orig->m_picOrg[0], (int)orig->m_stride,
                            buffer->m_picOrg[0], blockX, blockY,
                            x2, y2, blockSize, leastError)
                        : metld.motionErrorLumaSSD(metld,
                            orig->m_picOrg[0], (int)orig->m_stride,
                            buffer->m_picOrg[0], blockX, blockY,
                            x2, y2, blockSize, leastError);

                    if (error < leastError)
                    {
                        best.set(x2, y2);
                        leastError = error;
                    }
                }
            }
            prevBest = best;
            doubleRange = 3;
            for (int y2 = prevBest.y - doubleRange;
                y2 <= prevBest.y + doubleRange; y2++)
            {
                for (int x2 = prevBest.x - doubleRange;
                    x2 <= prevBest.x + doubleRange; x2++)
                {
                    error = metld.m_useSADinME
                        ? metld.motionErrorLumaSAD(metld,
                            orig->m_picOrg[0], (int)orig->m_stride,
                            buffer->m_picOrg[0], blockX, blockY,
                            x2, y2, blockSize, leastError)
                        : metld.motionErrorLumaSSD(metld,
                            orig->m_picOrg[0], (int)orig->m_stride,
                            buffer->m_picOrg[0], blockX, blockY,
                            x2, y2, blockSize, leastError);

                    if (error < leastError)
                    {
                        best.set(x2, y2);
                        leastError = error;
                    }
                }
            }

            if (blockY > 0)
            {
                int idx = ((blockY - stepSize) / stepSize) * mvStride + (blockX / stepSize);
                MV aboveMV = mvs[idx];
                error = metld.m_useSADinME
                    ? metld.motionErrorLumaSAD(metld,
                        orig->m_picOrg[0], (int)orig->m_stride,
                        buffer->m_picOrg[0], blockX, blockY,
                        aboveMV.x, aboveMV.y, blockSize, leastError)
                    : metld.motionErrorLumaSSD(metld,
                        orig->m_picOrg[0], (int)orig->m_stride,
                        buffer->m_picOrg[0], blockX, blockY,
                        aboveMV.x, aboveMV.y, blockSize, leastError);

                if (error < leastError)
                {
                    best.set(aboveMV.x, aboveMV.y);
                    leastError = error;
                }
            }

            // Left MV — safe because we process left-to-right
            if (blockX > 0)
            {
                int idx = ((blockY / stepSize) * mvStride + (blockX - stepSize) / stepSize);
                MV leftMV = mvs[idx];

                error = metld.m_useSADinME
                    ? metld.motionErrorLumaSAD(metld,
                        orig->m_picOrg[0], (int)orig->m_stride,
                        buffer->m_picOrg[0], blockX, blockY,
                        leftMV.x, leftMV.y, blockSize, leastError)
                    : metld.motionErrorLumaSSD(metld,
                        orig->m_picOrg[0], (int)orig->m_stride,
                        buffer->m_picOrg[0], blockX, blockY,
                        leftMV.x, leftMV.y, blockSize, leastError);

                if (error < leastError)
                {
                    best.set(leftMV.x, leftMV.y);
                    leastError = error;
                }
            }

            // Variance normalisation (unchanged from motionEstimationLumaDoubleRes)
            double avg = 0.0;
            for (int x1 = 0; x1 < blockSize; x1++)
            {
                for (int y1 = 0; y1 < blockSize; y1++)
                {
                    avg = avg + *(orig->m_picOrg[0] + (blockX + x1 + orig->m_stride * (blockY + y1)));
                }
            }
            avg = avg / (blockSize * blockSize);

            // calculate variance
            double variance = 0;
            for (int x1 = 0; x1 < blockSize; x1++)
            {
                for (int y1 = 0; y1 < blockSize; y1++)
                {
                    int pix = *(orig->m_picOrg[0] + (blockX + x1 + orig->m_stride * (blockY + y1)));
                    variance = variance + (pix - avg) * (pix - avg);
                }
            }

            leastError = (int)(20 * ((leastError + 5.0) / (variance + 5.0)) + (leastError / (blockSize * blockSize)) / 50);

            int mvIdx = (blockY / stepSize) * mvStride + (blockX / stepSize);
            mvs[mvIdx] = best;
            minError[mvIdx] = leastError;
        }
    }

    // m_rowDone[refId][row] = 1;

    // Mark the whole reference done after the last row
    if (row == m_numBlockRows - 1)
        curFrame->m_lowres.lowresMcstfMvs[0][refId][0].x = 1;
}

void MCSTFMEGroup::add_row(int refIdx, int poc, int curPoc,
    Frame* pic, int blockRow)
{
    /*X265_CHECK(m_jobTotal,
        "Cannot do MCSTF-ME in batch modes\n");*/

    Estimate& e = m_estimates[m_jobTotal++];
    e.p0 = refIdx;
    e.p1 = poc;
    e.b = curPoc;
    e.frame = pic;
    e.blockRow = blockRow;

    if (m_jobTotal == MAX_BATCH_SIZE)
        finishBatch();
}

void MCSTFMEGroup::initRowSync(int numRefs, int numBlockRows,
    int mctfUnitSize)
{

    m_numBlockRows = numBlockRows;
    m_mctfUnitSize = mctfUnitSize;

    // for (int r = 0; r < numRefs; r++)
    //     for (int row = 0; row < numBlockRows; row++)
    //         m_rowDone[r][row] = 0;
}

void MCSTFMEGroup::finishBatch()
{
    m_tasksAllocated.set(0);
    if (m_pool)
    {
        int num = tryBondPeers(*m_pool, m_jobTotal);
        printf("num=%d\n", num);
    }
    processTasks(-1);
    waitForExit();
    m_jobTotal = m_jobAcquired = 0;
}

void TemporalFilter::bilateralFilter_core(Frame* frame,
    TemporalFilterRefPicInfo* m_mcstfRefList,
    double overallStrength)
{

    const int numRefs = frame->m_mcstf->m_numRef;

    for (int i = 0; i < numRefs; i++)
    {
        TemporalFilterRefPicInfo *ref = &m_mcstfRefList[i];
        applyMotion(m_mcstfRefList[i].mvs, m_mcstfRefList[i].mvsStride, m_mcstfRefList[i].picBuffer, ref->compensatedPic);
    }

    int refStrengthRow = 0;

    if (frame->m_poc % 16 == 0)
        overallStrength = 1.5;
    const double lumaSigmaSq = (m_QP - m_sigmaZeroPoint) * (m_QP - m_sigmaZeroPoint) * m_sigmaMultiplier;
    const double chromaSigmaSq = 30 * 30;

    PicYuv* orgPic = frame->m_fencPic;

    for (int c = 0; c < m_numComponents; c++)
    {
        int height, width;
        pixel *srcPelRow = NULL;
        intptr_t srcStride, correctedPicsStride = 0;

        if (!c)
        {
            height = orgPic->m_picHeight;
            width = orgPic->m_picWidth;
            srcPelRow = orgPic->m_picOrg[c];
            srcStride = orgPic->m_stride;
        }
        else
        {
            int csx = CHROMA_H_SHIFT(m_internalCsp);
            int csy = CHROMA_V_SHIFT(m_internalCsp);

            height = orgPic->m_picHeight >> csy;
            width = orgPic->m_picWidth >> csx;
            srcPelRow = orgPic->m_picOrg[c];
            srcStride = (int)orgPic->m_strideC;
        }

        const double sigmaSq = (!c)  ? lumaSigmaSq : chromaSigmaSq;
        const double weightScaling = overallStrength * ( (!c) ? 0.4 : m_chromaFactor);

        const double maxSampleValue = (1 << m_bitDepth) - 1;
        const double bitDepthDiffWeighting = 1024.0 / (maxSampleValue + 1);

        const int blkSize = (!c) ? 8 : 4;

        for (int y = 0; y < height; y++, srcPelRow += srcStride)
        {
            pixel *srcPel = srcPelRow;

            for (int x = 0; x < width; x++, srcPel++)
            {
                const int orgVal = (int)*srcPel;
                double temporalWeightSum = 1.0;
                double newVal = (double)orgVal;

                if ((y % blkSize == 0) && (x % blkSize == 0))
                {
                    for (int i = 0; i < numRefs; i++)
                    {
                        TemporalFilterRefPicInfo *refPicInfo = &m_mcstfRefList[i];

                        if (!c)
                            correctedPicsStride = refPicInfo->compensatedPic->m_stride;
                        else
                            correctedPicsStride = refPicInfo->compensatedPic->m_strideC;

                        //const intptr_t pelOffset = y * correctedPicsStride + x;
                        // primitives.pu[1].copy_pp(m_metld->me.fencPUYuv.m_buf[0], FENC_STRIDE, refPicInfo->compensatedPic->m_picOrg[c] + pelOffset, correctedPicsStride);

                        double variance = 0, diffsum = 0;
                        const pixel *refPel = refPicInfo->compensatedPic->m_picOrg[c] +y * correctedPicsStride + x;
                        for (int y1 = 0; y1 < blkSize; y1++)
                        {
                            for (int x1 = 0; x1 < blkSize; x1++)
                            {
                                int pix  = *(srcPel + srcStride * y1 + x1);
                                int ref  = *(refPel + correctedPicsStride * y1 + x1);
                                int diff = pix - ref;

                                variance += diff * diff;

                                if (x1 != blkSize - 1)
                                {
                                    int pixR  = *(srcPel + srcStride * y1 + x1 + 1);
                                    int refR  = *(refPel + correctedPicsStride * y1 + x1 + 1);
                                    int diffR = pixR - refR;
                                    diffsum += (diffR - diff) * (diffR - diff);
                                }
                                if (y1 != blkSize - 1)
                                {
                                    int pixD  = *(srcPel + srcStride * y1 + x1 + srcStride);
                                    int refD  = *(refPel + correctedPicsStride * y1 + x1 + correctedPicsStride);
                                    int diffD = pixD - refD;
                                    diffsum += (diffD - diff) * (diffD - diff);
                                }
                            }
                        }
                        const int cntV = blkSize * blkSize;
                        const int cntD = 2 * cntV - blkSize - blkSize;
                        refPicInfo->noise[(y / blkSize) * refPicInfo->mvsStride + (x / blkSize)] = (int)round((15.0 * cntD / cntV * variance + 5.0) / (diffsum + 5.0));
                    }
                }

                double minError = 9999999;
                for (int i = 0; i < numRefs; i++)
                {
                    TemporalFilterRefPicInfo *refPicInfo = &m_mcstfRefList[i];
                    minError = X265_MIN(minError, (double)refPicInfo->error[(y / blkSize) * refPicInfo->mvsStride + (x / blkSize)]);
                }

                for (int i = 0; i < numRefs; i++)
                {
                    TemporalFilterRefPicInfo *refPicInfo = &m_mcstfRefList[i];

                    const int error = refPicInfo->error[(y / blkSize) * refPicInfo->mvsStride + (x / blkSize)];
                    const int noise = refPicInfo->noise[(y / blkSize) * refPicInfo->mvsStride + (x / blkSize)];

                    const pixel *pCorrectedPelPtr = refPicInfo->compensatedPic->m_picOrg[c] + (y * correctedPicsStride + x);
                    const int refVal = (int)*pCorrectedPelPtr;
                    double diff = (double)(refVal - orgVal);
                    diff *= bitDepthDiffWeighting;
                    double diffSq = diff * diff;

                    const int index = X265_MIN(3, std::abs(refPicInfo->origOffset) - 1);
                    double ww = 1, sw = 1;
                    ww *= (noise < 25) ? 1 : 1.2;
                    sw *= (noise < 25) ? 1.3 : 0.8;
                    ww *= (error < 50) ? 1.2 : ((error > 100) ? 0.8 : 1);
                    sw *= (error < 50) ? 1.3 : 1;
                    ww *= ((minError + 1) / (error + 1));
                    const double weight = weightScaling * s_refStrengths[refStrengthRow][index] * ww * exp(-diffSq / (2 * sw * sigmaSq));

                    newVal += weight * refVal;
                    temporalWeightSum += weight;
                }
                newVal /= temporalWeightSum;
                double sampleVal = round(newVal);
                sampleVal = (sampleVal < 0 ? 0 : (sampleVal > maxSampleValue ? maxSampleValue : sampleVal));
                *srcPel = (pixel)sampleVal;
            }
        }
    }
}

void TemporalFilter::applyMotionBlock(const pixel *pSrc, const int srcStride, pixel *dst, const intptr_t dstStride, const int w, const int h, const int *xFilter, const int *yFilter)
{
    
    const int numFilterTaps = 7;
    const int centreTapOffset = 3;

    int tempArray[8 + numFilterTaps][8];
    const pixel maxValue = (1 << X265_DEPTH) - 1;

    for (int y = 1; y < h + numFilterTaps; y++)
    {
        const int yOffset = y - centreTapOffset;
        const pixel *sourceRow = pSrc + yOffset * srcStride;
        for (int x = 0; x < w; x++)
        {
            int iBase = x - centreTapOffset;
            const pixel *rowStart = sourceRow + iBase;

            int iSum = 0;
            iSum += xFilter[1] * rowStart[1];
            iSum += xFilter[2] * rowStart[2];
            iSum += xFilter[3] * rowStart[3];
            iSum += xFilter[4] * rowStart[4];
            iSum += xFilter[5] * rowStart[5];
            iSum += xFilter[6] * rowStart[6];

            tempArray[y][x] = iSum;
        }
    }

    for (int y = 0; y < h; y++, dst += dstStride)
    {
        pixel *pDstPel = dst;
        for (int x = 0; x < w; x++, pDstPel++)
        {
            int iSum = 0;

            iSum += yFilter[1] * tempArray[y + 1][x];
            iSum += yFilter[2] * tempArray[y + 2][x];
            iSum += yFilter[3] * tempArray[y + 3][x];
            iSum += yFilter[4] * tempArray[y + 4][x];
            iSum += yFilter[5] * tempArray[y + 5][x];
            iSum += yFilter[6] * tempArray[y + 6][x];

            iSum = (iSum + (1 << 11)) >> 12;
            iSum = iSum < 0 ? 0 : (iSum > maxValue ? maxValue : iSum);
            *pDstPel = (pixel)iSum;
        }
    }
}

void TemporalFilter::bilateralFilterBlock(
    Frame*                    frame,
    TemporalFilterRefPicInfo* m_mcstfRefList,
    int                       numRefs,
    int                       blockRow,
    int                       blockSize,
    double                    overallStrength)
{

    int refStrengthRow = 0;

    if (frame->m_poc % 16 == 0)
        overallStrength = 1.5;
    const double lumaSigmaSq = (m_QP - m_sigmaZeroPoint) * (m_QP - m_sigmaZeroPoint) * m_sigmaMultiplier;
    const double chromaSigmaSq = 30 * 30;

    PicYuv* orgPic = frame->m_fencPic;

    for (int c = 0; c < m_numComponents; c++)
    {
        int height, width;
        pixel *srcPelPlane = NULL;
        intptr_t srcStride, correctedPicsStride = 0;

        int csx = (!c) ? 0 : CHROMA_H_SHIFT(m_internalCsp);
        int csy = (!c) ? 0 : CHROMA_V_SHIFT(m_internalCsp);
        if (!c)
        {
            height = orgPic->m_picHeight;
            width = orgPic->m_picWidth;
            srcPelPlane = orgPic->m_picOrg[c];
            srcStride = orgPic->m_stride;
        }
        else
        {

            height = orgPic->m_picHeight >> csy;
            width = orgPic->m_picWidth >> csx;
            srcPelPlane = orgPic->m_picOrg[c];
            srcStride = (int)orgPic->m_strideC;
        }

        const double sigmaSq = (!c)  ? lumaSigmaSq : chromaSigmaSq;
        const double weightScaling = overallStrength * ( (!c) ? 0.4 : m_chromaFactor);

        const double maxSampleValue = (1 << m_bitDepth) - 1;
        const double bitDepthDiffWeighting = 1024.0 / (maxSampleValue + 1);

        const int blkSize = (!c) ? 8 : 4;

        const int vShift       = (!c) ? 0 : csy;
        const int planeRowStart = (blockRow * blockSize) >> vShift;
        const int planeRowEnd   = X265_MIN((blockRow * blockSize + blockSize) >> vShift, height);

        const int blkRowStart = (planeRowStart / blkSize) * blkSize;
        const int blkRowEnd   = X265_MIN(((planeRowEnd + blkSize - 1) / blkSize) * blkSize, height);

        for (int by = blkRowStart, blockNumY = planeRowStart / blkSize; by + blkSize <= blkRowEnd; by += blkSize, blockNumY++)
        {
            for (int bx = 0, blockNumX = 0; bx + blkSize <= width; bx += blkSize, blockNumX++)
            {
                double vww   [16] = {};
                double vsw   [16] = {};

                const pixel* srcPel = srcPelPlane + by * srcStride + bx;

                double minError = 9999999;

                for (int i = 0; i < numRefs; i++)
                {
                    TemporalFilterRefPicInfo *refPicInfo = &m_mcstfRefList[i];

                    if (!c)
                        correctedPicsStride = refPicInfo->compensatedPic->m_stride;
                    else
                        correctedPicsStride = refPicInfo->compensatedPic->m_strideC;

                    // const intptr_t pelOffset = by * correctedPicsStride + bx;
                    // primitives.pu[1].copy_pp(m_metld->me.fencPUYuv.m_buf[0], FENC_STRIDE, refPicInfo->compensatedPic->m_picOrg[c] + pelOffset, correctedPicsStride);

                    
                    int mvIdx = blockNumY * m_mcstfRefList[i].mvsStride + blockNumX;
                    const MV &mv = m_mcstfRefList[i].mvs[mvIdx];
                    const int dx = mv.x >> csx;
                    const int dy = mv.y >> csy;
                    const int xInt = mv.x >> (4 + csx);
                    const int yInt = mv.y >> (4 + csy);

                    const int yOffset = by + yInt;
                    const int xOffset = bx + xInt;

                    const pixel* src = refPicInfo->picBuffer->m_picOrg[c] + yOffset * srcStride + xOffset;
                    const int    refStride = (int) ((!c) ? refPicInfo->picBuffer->m_stride : refPicInfo->picBuffer->m_strideC);
                    pixel* dst = refPicInfo->compensatedPic->m_picOrg[c] + by * correctedPicsStride + bx;
                    const int *xFilter = s_interpolationFilter[dx & 0xf];
                    const int *yFilter = s_interpolationFilter[dy & 0xf];

                    applyMotionBlock(src, refStride, dst, correctedPicsStride, blkSize, blkSize, xFilter, yFilter);

                    double variance = 0, diffsum = 0;
                    const pixel *refPel = refPicInfo->compensatedPic->m_picOrg[c] + by * correctedPicsStride + bx;
                    for (int y1 = 0; y1 < blkSize; y1++)
                    {
                        for (int x1 = 0; x1 < blkSize; x1++)
                        {
                            int pix  = *(srcPel + srcStride * y1 + x1);
                            int ref  = *(refPel + correctedPicsStride * y1 + x1);
                            int diff = pix - ref;

                            variance += diff * diff;

                            if (x1 != blkSize - 1)
                            {
                                int pixR  = *(srcPel + srcStride * y1 + x1 + 1);
                                int refR  = *(refPel + correctedPicsStride * y1 + x1 + 1);
                                int diffR = pixR - refR;
                                diffsum += (diffR - diff) * (diffR - diff);
                            }
                            if (y1 != blkSize - 1)
                            {
                                int pixD  = *(srcPel + srcStride * y1 + x1 + srcStride);
                                int refD  = *(refPel + correctedPicsStride * y1 + x1 + correctedPicsStride);
                                int diffD = pixD - refD;
                                diffsum += (diffD - diff) * (diffD - diff);
                            }
                        }
                    }
                    const int cntV = blkSize * blkSize;
                    const int cntD = 2 * cntV - blkSize - blkSize;
                    refPicInfo->noise[(by / blkSize) * refPicInfo->mvsStride + (bx / blkSize)] = (int)round((15.0 * cntD / cntV * variance + 5.0) / (diffsum + 5.0));
                    minError = X265_MIN(minError, (double)refPicInfo->error[(by / blkSize) * refPicInfo->mvsStride + (bx / blkSize)]);
                }

                // ── Step 2: pre-compute vww / vsw per reference (block-level) ─
                for (int i = 0; i < numRefs; i++)
                {
                    TemporalFilterRefPicInfo* refPicInfo = &m_mcstfRefList[i];

                    const int error = refPicInfo->error[(by / blkSize) * refPicInfo->mvsStride + (bx / blkSize)];
                    const int noise = refPicInfo->noise[(by / blkSize) * refPicInfo->mvsStride + (bx / blkSize)];

                    const int index = X265_MIN(3, std::abs(refPicInfo->origOffset) - 1);

                    double ww = 1, sw = 1;
                    ww *= (noise < 25) ? 1   : 1.2;
                    sw *= (noise < 25) ? 1.3 : 0.8;
                    ww *= (error < 50) ? 1.2 : ((error > 100) ? 0.8 : 1);
                    sw *= (error < 50) ? 1.3 : 1;
                    ww *= ((minError + 1) / (error + 1));

                    vww[i] = weightScaling * s_refStrengths[refStrengthRow][index] * ww;
                    vsw[i] = 2 * sw * sigmaSq;
                }

                // ── Step 3: pixel loop uses pre-computed vww / vsw ──────
                for (int y = by; y < X265_MIN(by + blkSize, height); y++)
                {
                    for (int x = bx; x < X265_MIN(bx + blkSize, width); x++)
                    {
                        const int orgVal = (int)srcPelPlane[y * srcStride + x];
                        double temporalWeightSum = 1.0;
                        double newVal = (double)orgVal;

                        for (int i = 0; i < numRefs; i++)
                        {
                            TemporalFilterRefPicInfo* refPicInfo = &m_mcstfRefList[i];

                            correctedPicsStride = (!c) ? refPicInfo->compensatedPic->m_stride
                                                                : refPicInfo->compensatedPic->m_strideC;

                            const pixel* pCorrectedPelPtr = refPicInfo->compensatedPic->m_picOrg[c]
                                                          + y * correctedPicsStride + x;
                            const int refVal = (int)*pCorrectedPelPtr;

                            double diff   = (double)(refVal - orgVal);
                            diff         *= bitDepthDiffWeighting;
                            double diffSq = diff * diff;

                            const double weight = vww[i] * exp(-diffSq / vsw[i]);

                            newVal            += weight * refVal;
                            temporalWeightSum += weight;
                        }

                        newVal /= temporalWeightSum;
                        double sampleVal = round(newVal);
                        sampleVal = (sampleVal < 0 ? 0 : (sampleVal > maxSampleValue ? maxSampleValue : sampleVal));
                        srcPelPlane[y * srcStride + x] = (pixel)sampleVal;
                    }
                }

            } // bx
        } // by
    } // c
}


// -----------------------------------------------------------------------
//  Top-level bilateral filter.
//  Splits the frame into 16-row blocks, dispatches jobs to the thread
//  pool via BilateralFilterGroup, then returns.
//  pool may be nullptr (single-threaded fallback).
// -----------------------------------------------------------------------
void TemporalFilter::bilateralFilter(
    Frame*                    curFrame,
    TemporalFilterRefPicInfo* mctfRefList,
    double                    overallStrength,
    ThreadPool*               pool)
{
    const int numRef       = curFrame->m_mcstf->m_numRef;
    const int blockSize    = 16;
    const int frameHeight  = curFrame->m_fencPic->m_picHeight;
    const int numBlockRows = (frameHeight + blockSize - 1) / blockSize;

    if (numRef == 0)
        return;

    BilateralFilterGroup filterGroup(*this, pool);

    for (int row = 0; row < numBlockRows; row++)
        filterGroup.add(curFrame, mctfRefList, numRef, row, blockSize, overallStrength);

    filterGroup.finishBatch();
}

void MotionEstimatorTLD::motionEstimationLuma(MotionEstimatorTLD& m_metld, MV *mvs, uint32_t mvStride, pixel* src,int stride, int height, int width, pixel* buf, int blockSize,
    int sRange, MV* previous, uint32_t prevMvStride, int factor)
{

    int range = sRange;


    const int stepSize = blockSize;

    const int origWidth = width;
    const int origHeight = height;

    int error;

    for (int blockY = 0; blockY + blockSize <= origHeight; blockY += stepSize)
    {
        for (int blockX = 0; blockX + blockSize <= origWidth; blockX += stepSize)
        {
            const intptr_t pelOffset = blockY * stride + blockX;
            m_metld.me.setSourcePU(src, stride, pelOffset, blockSize, blockSize, X265_HEX_SEARCH, 1);


            MV best(0, 0);
            int leastError = INT_MAX;

            if (previous == NULL)
            {
                range = sRange;
            }
            else
            {

                for (int py = -1; py <= 1; py++)
                {
                    int testy = blockY / (2 * blockSize) + py;

                    for (int px = -1; px <= 1; px++)
                    {

                        int testx = blockX / (2 * blockSize) + px;
                        if ((testx >= 0) && (testx < origWidth / (2 * blockSize)) && (testy >= 0) && (testy < origHeight / (2 * blockSize)))
                        {
                            int mvIdx = testy * prevMvStride + testx;
                            MV old = previous[mvIdx];

                            if (m_useSADinME)
                                error = motionErrorLumaSAD(m_metld, src, stride, buf, blockX, blockY, old.x * factor, old.y * factor, blockSize, leastError);
                            else
                                error = motionErrorLumaSSD(m_metld, src, stride, buf, blockX, blockY, old.x * factor, old.y * factor, blockSize, leastError);

                            if (error < leastError)
                            {
                                best.set(old.x * factor, old.y * factor);
                                leastError = error;
                            }
                        }
                    }
                }

                if (m_useSADinME)
                    error = motionErrorLumaSAD(m_metld, src, stride, buf, blockX, blockY, 0, 0, blockSize, leastError);
                else
                    error = motionErrorLumaSSD(m_metld, src, stride, buf, blockX, blockY, 0, 0, blockSize, leastError);

                if (error < leastError)
                {
                    best.set(0, 0);
                    leastError = error;
                }

            }

            MV prevBest = best;
            for (int y2 = prevBest.y / m_motionVectorFactor - range; y2 <= prevBest.y / m_motionVectorFactor + range; y2++)
            {
                for (int x2 = prevBest.x / m_motionVectorFactor - range; x2 <= prevBest.x / m_motionVectorFactor + range; x2++)
                {
                    if (m_useSADinME)
                        error = motionErrorLumaSAD(m_metld, src, stride, buf, blockX, blockY, x2 * m_motionVectorFactor, y2 * m_motionVectorFactor, blockSize, leastError);
                    else
                        error = motionErrorLumaSSD(m_metld, src, stride, buf, blockX, blockY, x2 * m_motionVectorFactor, y2 * m_motionVectorFactor, blockSize, leastError);
                    if (error < leastError)
                    {
                        best.set(x2 * m_motionVectorFactor, y2 * m_motionVectorFactor);
                        leastError = error;
                    }
                }
            }

            if (blockY > 0)
            {
                int idx = ((blockY - stepSize) / stepSize) * mvStride + (blockX / stepSize);
                MV aboveMV = mvs[idx];

                if (m_useSADinME)
                    error = motionErrorLumaSAD(m_metld, src, stride, buf, blockX, blockY, aboveMV.x, aboveMV.y, blockSize, leastError);
                else
                    error = motionErrorLumaSSD(m_metld, src, stride, buf, blockX, blockY, aboveMV.x, aboveMV.y, blockSize, leastError);

                if (error < leastError)
                {
                    best.set(aboveMV.x, aboveMV.y);
                    leastError = error;
                }
            }

            if (blockX > 0)
            {
                int idx = ((blockY / stepSize) * mvStride + (blockX - stepSize) / stepSize);
                MV leftMV = mvs[idx];

                if (m_useSADinME)
                    error = motionErrorLumaSAD(m_metld, src, stride, buf, blockX, blockY, leftMV.x, leftMV.y, blockSize, leastError);
                else
                    error = motionErrorLumaSSD(m_metld, src, stride, buf, blockX, blockY, leftMV.x, leftMV.y, blockSize, leastError);

                if (error < leastError)
                {
                    best.set(leftMV.x, leftMV.y);
                    leastError = error;
                }
            }

            // calculate average
            double avg = 0.0;
            for (int x1 = 0; x1 < blockSize; x1++)
            {
                for (int y1 = 0; y1 < blockSize; y1++)
                {
                    avg = avg + *(src + (blockX + x1 + stride * (blockY + y1)));
                }
            }
            avg = avg / (blockSize * blockSize);

            // calculate variance
            double variance = 0;
            for (int x1 = 0; x1 < blockSize; x1++)
            {
                for (int y1 = 0; y1 < blockSize; y1++)
                {
                    int pix = *(src + (blockX + x1 + stride * (blockY + y1)));
                    variance = variance + (pix - avg) * (pix - avg);
                }
            }

            leastError = (int)(20 * ((leastError + 5.0) / (variance + 5.0)) + (leastError / (blockSize * blockSize)) / 50);

            int mvIdx = (blockY / stepSize) * mvStride + (blockX / stepSize);
            mvs[mvIdx] = best;
        }
    }
}


void MotionEstimatorTLD::motionEstimationLumaDoubleRes(MotionEstimatorTLD& m_metld, MV *mvs, uint32_t mvStride, PicYuv *orig, PicYuv *buffer, int blockSize,
    MV *previous, uint32_t prevMvStride, int factor, int* minError)
{

    int range = 0;


    const int stepSize = blockSize;

    const int origWidth = orig->m_picWidth;
    const int origHeight = orig->m_picHeight;

    int error;

    for (int blockY = 0; blockY + blockSize <= origHeight; blockY += stepSize)
    {
        for (int blockX = 0; blockX + blockSize <= origWidth; blockX += stepSize)
        {

            const intptr_t pelOffset = blockY * orig->m_stride + blockX;
            m_metld.me.setSourcePU(orig->m_picOrg[0], orig->m_stride, pelOffset, blockSize, blockSize, X265_HEX_SEARCH, 1);

            MV best(0, 0);
            int leastError = INT_MAX;

            if (previous == NULL)
            {
                range = 8;
            }
            else
            {

                for (int py = -1; py <= 1; py++)
                {
                    int testy = blockY / (2 * blockSize) + py;

                    for (int px = -1; px <= 1; px++)
                    {

                        int testx = blockX / (2 * blockSize) + px;
                        if ((testx >= 0) && (testx < origWidth / (2 * blockSize)) && (testy >= 0) && (testy < origHeight / (2 * blockSize)))
                        {
                            int mvIdx = testy * prevMvStride + testx;
                            MV old = previous[mvIdx];

                            if (m_useSADinME)
                                error = motionErrorLumaSAD(m_metld, orig->m_picOrg[0], (int)orig->m_stride, buffer->m_picOrg[0], blockX, blockY, old.x * factor, old.y * factor, blockSize, leastError);
                            else
                                error = motionErrorLumaSSD(m_metld, orig->m_picOrg[0], (int)orig->m_stride, buffer->m_picOrg[0], blockX, blockY, old.x * factor, old.y * factor, blockSize, leastError);

                            if (error < leastError)
                            {
                                best.set(old.x * factor, old.y * factor);
                                leastError = error;
                            }
                        }
                    }
                }

                if (m_useSADinME)
                    error = motionErrorLumaSAD(m_metld, orig->m_picOrg[0], (int)orig->m_stride, buffer->m_picOrg[0], blockX, blockY, 0, 0, blockSize, leastError);
                else
                    error = motionErrorLumaSSD(m_metld, orig->m_picOrg[0], (int)orig->m_stride, buffer->m_picOrg[0], blockX, blockY, 0, 0, blockSize, leastError);

                if (error < leastError)
                {
                    best.set(0, 0);
                    leastError = error;
                }

            }

            MV prevBest = best;
            for (int y2 = prevBest.y / m_motionVectorFactor - range; y2 <= prevBest.y / m_motionVectorFactor + range; y2++)
            {
                for (int x2 = prevBest.x / m_motionVectorFactor - range; x2 <= prevBest.x / m_motionVectorFactor + range; x2++)
                {
                    if (m_useSADinME)
                        error = motionErrorLumaSAD(m_metld, orig->m_picOrg[0], (int)orig->m_stride, buffer->m_picOrg[0], blockX, blockY, x2 * m_motionVectorFactor, y2 * m_motionVectorFactor, blockSize, leastError);
                    else
                        error = motionErrorLumaSSD(m_metld, orig->m_picOrg[0], (int)orig->m_stride, buffer->m_picOrg[0], blockX, blockY, x2 * m_motionVectorFactor, y2 * m_motionVectorFactor, blockSize, leastError);

                    if (error < leastError)
                    {
                        best.set(x2 * m_motionVectorFactor, y2 * m_motionVectorFactor);
                        leastError = error;
                    }
                }
            }

            prevBest = best;
            int doubleRange = 3 * 4;
            for (int y2 = prevBest.y - doubleRange; y2 <= prevBest.y + doubleRange; y2+=4)
            {
                for (int x2 = prevBest.x - doubleRange; x2 <= prevBest.x + doubleRange; x2+=4)
                {
                    if (m_useSADinME)
                        error = motionErrorLumaSAD(m_metld, orig->m_picOrg[0], (int)orig->m_stride, buffer->m_picOrg[0], blockX, blockY, x2, y2, blockSize, leastError);
                    else
                        error = motionErrorLumaSSD(m_metld, orig->m_picOrg[0], (int)orig->m_stride, buffer->m_picOrg[0], blockX, blockY, x2, y2, blockSize, leastError);

                    if (error < leastError)
                    {
                        best.set(x2, y2);
                        leastError = error;
                    }
                }
            }

            prevBest = best;
            doubleRange = 3;
            for (int y2 = prevBest.y - doubleRange; y2 <= prevBest.y + doubleRange; y2++)
            {
                for (int x2 = prevBest.x - doubleRange; x2 <= prevBest.x + doubleRange; x2++)
                {
                    if (m_useSADinME)
                        error = motionErrorLumaSAD(m_metld, orig->m_picOrg[0], (int)orig->m_stride, buffer->m_picOrg[0], blockX, blockY, x2, y2, blockSize, leastError);
                    else
                        error = motionErrorLumaSSD(m_metld, orig->m_picOrg[0], (int)orig->m_stride, buffer->m_picOrg[0], blockX, blockY, x2, y2, blockSize, leastError);

                    if (error < leastError)
                    {
                        best.set(x2, y2);
                        leastError = error;
                    }
                }
            }


            if (blockY > 0)
            {
                int idx = ((blockY - stepSize) / stepSize) * mvStride + (blockX / stepSize);
                MV aboveMV = mvs[idx];

                if (m_useSADinME)
                    error = motionErrorLumaSAD(m_metld, orig->m_picOrg[0], (int)orig->m_stride, buffer->m_picOrg[0], blockX, blockY, aboveMV.x, aboveMV.y, blockSize, leastError);
                else
                    error = motionErrorLumaSSD(m_metld, orig->m_picOrg[0], (int)orig->m_stride, buffer->m_picOrg[0], blockX, blockY, aboveMV.x, aboveMV.y, blockSize, leastError);

                if (error < leastError)
                {
                    best.set(aboveMV.x, aboveMV.y);
                    leastError = error;
                }
            }

            if (blockX > 0)
            {
                int idx = ((blockY / stepSize) * mvStride + (blockX - stepSize) / stepSize);
                MV leftMV = mvs[idx];

                if (m_useSADinME)
                    error = motionErrorLumaSAD(m_metld, orig->m_picOrg[0], (int)orig->m_stride, buffer->m_picOrg[0], blockX, blockY, leftMV.x, leftMV.y, blockSize, leastError);
                else
                    error = motionErrorLumaSSD(m_metld, orig->m_picOrg[0], (int)orig->m_stride, buffer->m_picOrg[0], blockX, blockY, leftMV.x, leftMV.y, blockSize, leastError);

                if (error < leastError)
                {
                    best.set(leftMV.x, leftMV.y);
                    leastError = error;
                }
            }

            // calculate average
            double avg = 0.0;
            for (int x1 = 0; x1 < blockSize; x1++)
            {
                for (int y1 = 0; y1 < blockSize; y1++)
                {
                    avg = avg + *(orig->m_picOrg[0] + (blockX + x1 + orig->m_stride * (blockY + y1)));
                }
            }
            avg = avg / (blockSize * blockSize);

            // calculate variance
            double variance = 0;
            for (int x1 = 0; x1 < blockSize; x1++)
            {
                for (int y1 = 0; y1 < blockSize; y1++)
                {
                    int pix = *(orig->m_picOrg[0] + (blockX + x1 + orig->m_stride * (blockY + y1)));
                    variance = variance + (pix - avg) * (pix - avg);
                }
            }

            leastError = (int)(20 * ((leastError + 5.0) / (variance + 5.0)) + (leastError / (blockSize * blockSize)) / 50);

            int mvIdx = (blockY / stepSize) * mvStride + (blockX / stepSize);
            mvs[mvIdx] = best;
            minError[mvIdx] = leastError;
        }
    }
}

void TemporalFilter::destroyRefPicInfo(TemporalFilterRefPicInfo* curFrame)
{
    if (curFrame)
    {
        if (curFrame->compensatedPic)
        {
            curFrame->compensatedPic->destroy();
            delete curFrame->compensatedPic;
        }

        if (curFrame->mvs)
            X265_FREE(curFrame->mvs);
        if (curFrame->mvs0)
            X265_FREE(curFrame->mvs0);
        if (curFrame->mvs1)
            X265_FREE(curFrame->mvs1);
        if (curFrame->mvs2)
            X265_FREE(curFrame->mvs2);
        if (curFrame->noise)
            X265_FREE(curFrame->noise);
        if (curFrame->error)
            X265_FREE(curFrame->error);
    }
}
