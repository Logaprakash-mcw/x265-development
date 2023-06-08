/*****************************************************************************
 * Copyright (C) 2013-2020 MulticoreWare, Inc
 *
 * Authors: Chung Shin Yee <shinyee@multicorewareinc.com>
 *          Min Chen <chenm003@163.com>
 *          Steve Borho <steve@borho.org>
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
#include "wavefront.h"
#include "param.h"

#include "encoder.h"
#include "frameencoder.h"
#include "common.h"
#include "slicetype.h"
#include "temporalfilter.h"

namespace X265_NS {

FrameEncoder::FrameEncoder()
{
    m_prevOutputTime = x265_mdate();
    m_reconfigure = false;
    m_isFrameEncoder = true;
    m_threadActive = true;
    m_top = NULL;
    m_param = NULL;
    m_frame = NULL;
    m_localTldIdx = 0;
}

void FrameEncoder::destroy()
{
    if (m_pool)
    {
        if (!m_jpId)
        {
            int numTLD = m_pool->m_numWorkers;
            if (!m_param->bEnableWavefront)
                numTLD += m_pool->m_numProviders;
            //delete [] m_tld;
        }
    }
    m_fg->destroy();
    delete m_frameEncTF->m_metld;

    for (int i = 0; i < (m_frameEncTF->m_range << 1); i++)
        m_frameEncTF->destroyRefPicInfo(&m_mcstfRefList[i]);

    delete m_frameEncTF;

}

bool FrameEncoder::init(Encoder *top, int numRows, int numCols)
{
    m_top = top;
    m_param = top->m_param;
    m_numRows = numRows;
    m_numCols = numCols;
    m_reconfigure = false;

    bool ok = !!m_numRows;



    m_frameEncTF = new TemporalFilter();
    if (m_frameEncTF)
        m_frameEncTF->init(m_param);

    for (int i = 0; i < (m_frameEncTF->m_range << 1); i++)
        ok &= !!m_frameEncTF->createRefPicInfo(&m_mcstfRefList[i], m_param);

    m_fg = new FGAnalyser;
    if(m_fg)
    {
        m_fg->init(m_param);
        m_fg->fout = this->m_top->m_filmGrainIn;
    }

    return ok;
}

/* Generate a complete list of unique geom sets for the current picture dimensions */
bool FrameEncoder::startCompressFrame(Frame* curFrame)
{

    m_frame = curFrame;
    curFrame->m_encData->m_frameEncoderID = m_jpId;
    curFrame->m_encData->m_jobProvider = this;
    m_enable.trigger();
    return true;
}

void FrameEncoder::threadMain()
{
    THREAD_NAME("Frame", m_jpId);

    m_done.trigger();     /* signal that thread is initialized */
    m_enable.wait();      /* Encoder::encode() triggers this event */

    while (m_threadActive)
    {
        compressFrame();
        m_done.trigger(); /* FrameEncoder::getEncodedPicture() blocks for this event */
        m_enable.wait();
    }
}

void FrameEncoder::compressFrame()
{
    ProfileScopeEvent(frameThread);

    m_startCompressTime = x265_mdate();

    PicYuv *original = new PicYuv();
    original->create(m_param);
    original->copyFromFrame(m_frame->m_fencPic);

    m_frameEncTF->m_QP = m_param->qp; // Keep qp is constant
    m_frameEncTF->bilateralFilter(m_frame, m_mcstfRefList, m_param->temporalFilterStrength);

    //m_fg->initBufs(original, m_frame->m_fencPic);

    m_fg->initBufs(original, m_frame->m_fencPic);

    m_fg->estimate_grain_parameters();

    //Reset the MCSTF context in Frame Encoder and Frame
    for (int i = 0; i < (m_frameEncTF->m_range << 1); i++)
    {
        memset(m_mcstfRefList[i].mvs0, 0, sizeof(MV) * ((m_param->sourceWidth / 16) * (m_param->sourceHeight / 16)));
        memset(m_mcstfRefList[i].mvs1, 0, sizeof(MV) * ((m_param->sourceWidth / 16) * (m_param->sourceHeight / 16)));
        memset(m_mcstfRefList[i].mvs2, 0, sizeof(MV) * ((m_param->sourceWidth / 16) * (m_param->sourceHeight / 16)));
        memset(m_mcstfRefList[i].mvs,  0, sizeof(MV) * ((m_param->sourceWidth / 4) * (m_param->sourceHeight / 4)));
        memset(m_mcstfRefList[i].noise, 0, sizeof(int) * ((m_param->sourceWidth / 4) * (m_param->sourceHeight / 4)));
        memset(m_mcstfRefList[i].error, 0, sizeof(int) * ((m_param->sourceWidth / 4) * (m_param->sourceHeight / 4)));

        m_frame->m_mcstf->m_numRef = 0;
    }

    m_fg->set_film_grain_parameters();
    if (m_top->m_filmGrainIn)
        m_fg->write_film_grain_parameters();
    m_endCompressTime = m_endFrameTime = x265_mdate();
}

//void FrameEncoder::processRow()
//{
//}

//void FrameEncoder::processRow(int row, int threadId)
//{
//    //int64_t startTime = x265_mdate();
//    //if (ATOMIC_INC(&m_activeWorkerCount) == 1 && m_stallStartTime)
//    //    m_totalNoWorkerTime += x265_mdate() - m_stallStartTime;
//
//    //const uint32_t realRow = m_idx_to_row[row >> 1];
//    //const uint32_t typeNum = m_idx_to_row[row & 1];
//
//    ////if (!typeNum)
//    ////    processRowEncoder(realRow, m_tld[threadId]);
//    ////else
//    ////{
//    ////    m_frameFilter.processRow(realRow);
//
//    ////    // NOTE: Active next row
//    ////    if (realRow != m_sliceBaseRow[m_rows[realRow].sliceId + 1] - 1)
//    ////        enqueueRowFilter(m_row_to_idx[realRow + 1]);
//    ////}
//
//    //if (ATOMIC_DEC(&m_activeWorkerCount) == 0)
//    //    m_stallStartTime = x265_mdate();
//
//    //m_totalWorkerElapsedTime += x265_mdate() - startTime; // not thread safe, but good enough
//    return;
//}
//
// Called by worker threads
//void FrameEncoder::processRowEncoder(int intRow)
//{
//    const uint32_t row = (uint32_t)intRow;
//    CTURow& curRow = m_rows[row];
//
//    if (m_param->bEnableWavefront)
//    {
//        ScopedLock self(curRow.lock);
//        if (!curRow.active)
//            /* VBV restart is in progress, exit out */
//            return;
//        if (curRow.busy)
//        {
//            /* On multi-socket Windows servers, we have seen problems with
//             * ATOMIC_CAS which resulted in multiple worker threads processing
//             * the same CU row, which often resulted in bad pointer accesses. We
//             * believe the problem is fixed, but are leaving this check in place
//             * to prevent crashes in case it is not */
//            x265_log(m_param, X265_LOG_WARNING,
//                     "internal error - simultaneous row access detected. Please report HW to x265-devel@videolan.org\n");
//            return;
//        }
//        curRow.busy = true;
//    }
//
//    /* When WPP is enabled, every row has its own row coder instance. Otherwise
//     * they share row 0 */
//    //Entropy& rowCoder = m_param->bEnableWavefront ? curRow.rowGoOnCoder : m_rows[0].rowGoOnCoder;
//    FrameData& curEncData = *m_frame->m_encData;
//    Slice *slice = curEncData.m_slice;
//
//    const uint32_t numCols = m_numCols;
//    const uint32_t lineStartCUAddr = row * numCols;
//    bool bIsVbv = m_param->rc.vbvBufferSize > 0 && m_param->rc.vbvMaxBitrate > 0;
//
//    const uint32_t sliceId = curRow.sliceId;
//    uint32_t maxBlockCols = (m_frame->m_fencPic->m_picWidth + (16 - 1)) / 16;
//    uint32_t noOfBlocks = m_param->maxCUSize / 16;
//    const uint32_t bFirstRowInSlice = ((row == 0) || (m_rows[row - 1].sliceId != curRow.sliceId)) ? 1 : 0;
//    const uint32_t bLastRowInSlice = ((row == m_numRows - 1) || (m_rows[row + 1].sliceId != curRow.sliceId)) ? 1 : 0;
//    const uint32_t endRowInSlicePlus1 = m_sliceBaseRow[sliceId + 1];
//    const uint32_t rowInSlice = row - m_sliceBaseRow[sliceId];
//
//    // Load SBAC coder context from previous row and initialize row state.
//    if (bFirstRowInSlice && !curRow.completed)        
//        rowCoder.load(m_initSliceContext);     
//
//    // calculate mean QP for consistent deltaQP signalling calculation
//    if (m_param->bOptCUDeltaQP)
//    {
//        ScopedLock self(curRow.lock);
//        if (!curRow.avgQPComputed)
//        {
//            if (m_param->bEnableWavefront || !row)
//            {
//                double meanQPOff = 0;
//                bool isReferenced = IS_REFERENCED(m_frame);
//                double *qpoffs = (isReferenced && m_param->rc.cuTree) ? m_frame->m_lowres.qpCuTreeOffset : m_frame->m_lowres.qpAqOffset;
//                if (qpoffs)
//                {
//                    uint32_t loopIncr = (m_param->rc.qgSize == 8) ? 8 : 16;
//
//                    uint32_t cuYStart = 0, height = m_frame->m_fencPic->m_picHeight;
//                    if (m_param->bEnableWavefront)
//                    {
//                        cuYStart = intRow * m_param->maxCUSize;
//                        height = cuYStart + m_param->maxCUSize;
//                    }
//
//                    uint32_t qgSize = m_param->rc.qgSize, width = m_frame->m_fencPic->m_picWidth;
//                    uint32_t maxOffsetCols = (m_frame->m_fencPic->m_picWidth + (loopIncr - 1)) / loopIncr;
//                    uint32_t count = 0;
//                    for (uint32_t cuY = cuYStart; cuY < height && (cuY < m_frame->m_fencPic->m_picHeight); cuY += qgSize)
//                    {
//                        for (uint32_t cuX = 0; cuX < width; cuX += qgSize)
//                        {
//                            double qp_offset = 0;
//                            uint32_t cnt = 0;
//
//                            for (uint32_t block_yy = cuY; block_yy < cuY + qgSize && block_yy < m_frame->m_fencPic->m_picHeight; block_yy += loopIncr)
//                            {
//                                for (uint32_t block_xx = cuX; block_xx < cuX + qgSize && block_xx < width; block_xx += loopIncr)
//                                {
//                                    int idx = ((block_yy / loopIncr) * (maxOffsetCols)) + (block_xx / loopIncr);
//                                    qp_offset += qpoffs[idx];
//                                    cnt++;
//                                }
//                            }
//                            qp_offset /= cnt;
//                            meanQPOff += qp_offset;
//                            count++;
//                        }
//                    }
//                    meanQPOff /= count;
//                }
//                rowCoder.m_meanQP = slice->m_sliceQp + meanQPOff;
//            }
//            else
//            {
//                rowCoder.m_meanQP = m_rows[0].rowGoOnCoder.m_meanQP;
//            }
//            curRow.avgQPComputed = 1;
//        }
//    }
//
//    // Initialize restrict on MV range in slices
//    //tld.analysis.m_sliceMinY = -(int32_t)(rowInSlice * m_param->maxCUSize * 4) + 3 * 4;
//    //tld.analysis.m_sliceMaxY = (int32_t)((endRowInSlicePlus1 - 1 - row) * (m_param->maxCUSize * 4) - 4 * 4);
//
//    //// Handle single row slice
//    //if (tld.analysis.m_sliceMaxY < tld.analysis.m_sliceMinY)
//    //    tld.analysis.m_sliceMaxY = tld.analysis.m_sliceMinY = 0;
//
//
//    while (curRow.completed < numCols)
//    {
//        ProfileScopeEvent(encodeCTU);
//
//        const uint32_t col = curRow.completed;
//        const uint32_t cuAddr = lineStartCUAddr + col;
//        CUData* ctu = curEncData.getPicCTU(cuAddr);
//        const uint32_t bLastCuInSlice = (bLastRowInSlice & (col == numCols - 1)) ? 1 : 0;
//        ctu->initCTU(*m_frame, cuAddr, slice->m_sliceQp, bFirstRowInSlice, bLastRowInSlice, bLastCuInSlice);
//
//        if (bIsVbv)
//        {
//            if (col == 0 && !m_param->bEnableWavefront)
//            {
//                m_backupStreams[0].copyBits(&m_outStreams[0]);
//                curRow.bufferedEntropy.copyState(rowCoder);
//                curRow.bufferedEntropy.loadContexts(rowCoder);
//            }
//            if (bFirstRowInSlice && m_vbvResetTriggerRow[curRow.sliceId] != intRow)
//            {
//                curEncData.m_rowStat[row].rowQp = curEncData.m_avgQpRc;
//                curEncData.m_rowStat[row].rowQpScale = x265_qp2qScale(curEncData.m_avgQpRc);
//            }
//
//            FrameData::RCStatCU& cuStat = curEncData.m_cuStat[cuAddr];
//            if (m_param->bEnableWavefront && rowInSlice >= col && !bFirstRowInSlice && m_vbvResetTriggerRow[curRow.sliceId] != intRow)
//                cuStat.baseQp = curEncData.m_cuStat[cuAddr - numCols + 1].baseQp;
//            else if (!m_param->bEnableWavefront && !bFirstRowInSlice && m_vbvResetTriggerRow[curRow.sliceId] != intRow)
//                cuStat.baseQp = curEncData.m_rowStat[row - 1].rowQp;
//            else
//                cuStat.baseQp = curEncData.m_rowStat[row].rowQp;
//
//            /* TODO: use defines from slicetype.h for lowres block size */
//            uint32_t block_y = (ctu->m_cuPelY >> m_param->maxLog2CUSize) * noOfBlocks;
//            uint32_t block_x = (ctu->m_cuPelX >> m_param->maxLog2CUSize) * noOfBlocks;
//            if (!m_param->analysisLoad || !m_param->bDisableLookahead)
//            {
//                cuStat.vbvCost = 0;
//                cuStat.intraVbvCost = 0;
//
//                for (uint32_t h = 0; h < noOfBlocks && block_y < m_sliceMaxBlockRow[sliceId + 1]; h++, block_y++)
//                {
//                    uint32_t idx = block_x + (block_y * maxBlockCols);
//
//                    for (uint32_t w = 0; w < noOfBlocks && (block_x + w) < maxBlockCols; w++, idx++)
//                    {
//                        cuStat.vbvCost += m_frame->m_lowres.lowresCostForRc[idx] & LOWRES_COST_MASK;
//                        cuStat.intraVbvCost += m_frame->m_lowres.intraCost[idx];
//                    }
//                }
//            }
//        }
//        else
//            curEncData.m_cuStat[cuAddr].baseQp = curEncData.m_avgQpRc;
//
//        if (m_param->bEnableWavefront && !col && !bFirstRowInSlice)
//        {
//            // Load SBAC coder context from previous row and initialize row state.
//            rowCoder.copyState(m_initSliceContext);
//            rowCoder.loadContexts(m_rows[row - 1].bufferedEntropy);
//        }
//
//        // take a sample of the current active worker count
//        ATOMIC_ADD(&m_totalActiveWorkerCount, m_activeWorkerCount);
//        ATOMIC_INC(&m_activeWorkerCountSamples);
//
//        /* advance top-level row coder to include the context of this CTU.
//         * if SAO is disabled, rowCoder writes the final CTU bitstream */
//        rowCoder.encodeCTU(*ctu, m_cuGeoms[m_ctuGeomMap[cuAddr]]);
//
//        if (m_param->bEnableWavefront && col == 1)
//            // Save CABAC state for next row
//            curRow.bufferedEntropy.loadContexts(rowCoder);
//
//
//
//        // Completed CU processing
//        curRow.completed++;
//
//        FrameStats frameLog;
//
//        // copy number of intra, inter cu per row into frame stats for 2 pass
//
//        //curRow.rowStats.totalCtu++;
//        //curRow.rowStats.lumaDistortion   += best.lumaDistortion;
//        //curRow.rowStats.chromaDistortion += best.chromaDistortion;
//        //curRow.rowStats.psyEnergy        += best.psyEnergy;
//        //curRow.rowStats.ssimEnergy       += best.ssimEnergy;
//        //curRow.rowStats.resEnergy        += best.resEnergy;
//        curRow.rowStats.cntIntraNxN      += frameLog.cntIntraNxN;
//        curRow.rowStats.totalCu          += frameLog.totalCu;
//        for (uint32_t depth = 0; depth <= m_param->maxCUDepth; depth++)
//        {
//            curRow.rowStats.cntSkipCu[depth] += frameLog.cntSkipCu[depth];
//            curRow.rowStats.cntMergeCu[depth] += frameLog.cntMergeCu[depth];
//            for (int m = 0; m < INTER_MODES; m++)
//                curRow.rowStats.cuInterDistribution[depth][m] += frameLog.cuInterDistribution[depth][m];
//            for (int n = 0; n < INTRA_MODES; n++)
//                curRow.rowStats.cuIntraDistribution[depth][n] += frameLog.cuIntraDistribution[depth][n];
//        }
//
//        //curEncData.m_cuStat[cuAddr].totalBits = best.totalBits;
//        x265_emms();
//
//        if (bIsVbv)
//        {   
//            // Update encoded bits, satdCost, baseQP for each CU if tune grain is disabled
//            FrameData::RCStatCU& cuStat = curEncData.m_cuStat[cuAddr];    
//            if ((m_param->bEnableWavefront && ((cuAddr == m_sliceBaseRow[sliceId] * numCols) || !m_param->rc.bEnableConstVbv)) || !m_param->bEnableWavefront)
//            {
//                curEncData.m_rowStat[row].rowSatd += cuStat.vbvCost;
//                curEncData.m_rowStat[row].rowIntraSatd += cuStat.intraVbvCost;
//                curEncData.m_rowStat[row].encodedBits += cuStat.totalBits;
//                curEncData.m_rowStat[row].sumQpRc += cuStat.baseQp;
//                curEncData.m_rowStat[row].numEncodedCUs = cuAddr;
//            }
//            
//            // If current block is at row end checkpoint, call vbv ratecontrol.
//            if (!m_param->bEnableWavefront && col == numCols - 1)
//            {
//                double qpBase = curEncData.m_cuStat[cuAddr].baseQp;
//                qpBase = x265_clip3((double)m_param->rc.qpMin, (double)m_param->rc.qpMax, qpBase);
//                curEncData.m_rowStat[row].rowQp = qpBase;
//                curEncData.m_rowStat[row].rowQpScale = x265_qp2qScale(qpBase);
//                if (curRow.reEncode < 0)
//                {
//                    x265_log(m_param, X265_LOG_DEBUG, "POC %d row %d - encode restart required for VBV, to %.2f from %.2f\n",
//                        m_frame->m_poc, row, qpBase, curEncData.m_cuStat[cuAddr].baseQp);
//
//                    m_vbvResetTriggerRow[curRow.sliceId] = row;
//                    m_outStreams[0].copyBits(&m_backupStreams[0]);
//
//                    rowCoder.copyState(curRow.bufferedEntropy);
//                    rowCoder.loadContexts(curRow.bufferedEntropy);
//
//                    curRow.completed = 0;
//                    memset(&curRow.rowStats, 0, sizeof(curRow.rowStats));
//                    curEncData.m_rowStat[row].numEncodedCUs = 0;
//                    curEncData.m_rowStat[row].encodedBits = 0;
//                    curEncData.m_rowStat[row].rowSatd = 0;
//                    curEncData.m_rowStat[row].rowIntraSatd = 0;
//                    curEncData.m_rowStat[row].sumQpRc = 0;
//                    curEncData.m_rowStat[row].sumQpAq = 0;
//                }
//            }
//            // If current block is at row diagonal checkpoint, call vbv ratecontrol.
//            else if (m_param->bEnableWavefront && rowInSlice == col && !bFirstRowInSlice)
//            {
//                if (m_param->rc.bEnableConstVbv)
//                {
//                    uint32_t startCuAddr = numCols * row;
//                    uint32_t EndCuAddr = startCuAddr + col;
//
//                    for (int32_t r = row; r >= (int32_t)m_sliceBaseRow[sliceId]; r--)
//                    {
//                        for (uint32_t c = startCuAddr; c <= EndCuAddr && c <= numCols * (r + 1) - 1; c++)
//                        {
//                            curEncData.m_rowStat[r].rowSatd += curEncData.m_cuStat[c].vbvCost;
//                            curEncData.m_rowStat[r].rowIntraSatd += curEncData.m_cuStat[c].intraVbvCost;
//                            curEncData.m_rowStat[r].encodedBits += curEncData.m_cuStat[c].totalBits;
//                            curEncData.m_rowStat[r].sumQpRc += curEncData.m_cuStat[c].baseQp;
//                            curEncData.m_rowStat[r].numEncodedCUs = c;
//                        }
//                        if (curRow.reEncode < 0)
//                            break;
//                        startCuAddr = EndCuAddr - numCols;
//                        EndCuAddr = startCuAddr + 1;
//                    }
//                }
//                double qpBase = curEncData.m_cuStat[cuAddr].baseQp;
//                qpBase = x265_clip3((double)m_param->rc.qpMin, (double)m_param->rc.qpMax, qpBase);
//                curEncData.m_rowStat[row].rowQp = qpBase;
//                curEncData.m_rowStat[row].rowQpScale = x265_qp2qScale(qpBase);
//
//                if (curRow.reEncode < 0)
//                {
//                    x265_log(m_param, X265_LOG_DEBUG, "POC %d row %d - encode restart required for VBV, to %.2f from %.2f\n",
//                             m_frame->m_poc, row, qpBase, curEncData.m_cuStat[cuAddr].baseQp);
//
//                    // prevent the WaveFront::findJob() method from providing new jobs
//                    m_vbvResetTriggerRow[curRow.sliceId] = row;
//                    m_bAllRowsStop[curRow.sliceId] = true;
//
//                    for (uint32_t r = m_sliceBaseRow[sliceId + 1] - 1; r >= row; r--)
//                    {
//                        CTURow& stopRow = m_rows[r];
//
//                        if (r != row)
//                        {
//                            /* if row was active (ready to be run) clear active bit and bitmap bit for this row */
//                            stopRow.lock.acquire();
//                            while (stopRow.active)
//                            {
//                                if (dequeueRow(m_row_to_idx[r] * 2))
//                                    stopRow.active = false;
//                                else
//                                {
//                                    /* we must release the row lock to allow the thread to exit */
//                                    stopRow.lock.release();
//                                    GIVE_UP_TIME();
//                                    stopRow.lock.acquire();
//                                }
//                            }
//                            stopRow.lock.release();
//
//                            bool bRowBusy = true;
//                            do
//                            {
//                                stopRow.lock.acquire();
//                                bRowBusy = stopRow.busy;
//                                stopRow.lock.release();
//
//                                if (bRowBusy)
//                                {
//                                    GIVE_UP_TIME();
//                                }
//                            }
//                            while (bRowBusy);
//                        }
//
//                        m_outStreams[r].resetBits();
//                        stopRow.completed = 0;
//                        memset(&stopRow.rowStats, 0, sizeof(stopRow.rowStats));
//                        curEncData.m_rowStat[r].numEncodedCUs = 0;
//                        curEncData.m_rowStat[r].encodedBits = 0;
//                        curEncData.m_rowStat[r].rowSatd = 0;
//                        curEncData.m_rowStat[r].rowIntraSatd = 0;
//                        curEncData.m_rowStat[r].sumQpRc = 0;
//                        curEncData.m_rowStat[r].sumQpAq = 0;
//                    }
//
//                    m_bAllRowsStop[curRow.sliceId] = false;
//                }
//            }
//        }
//
//        if (m_param->bEnableWavefront && curRow.completed >= 2 && !bLastRowInSlice &&
//            (!m_bAllRowsStop[curRow.sliceId] || intRow + 1 < m_vbvResetTriggerRow[curRow.sliceId]))
//        {
//            /* activate next row */
//            ScopedLock below(m_rows[row + 1].lock);
//
//            if (m_rows[row + 1].active == false &&
//                m_rows[row + 1].completed + 2 <= curRow.completed)
//            {
//                m_rows[row + 1].active = true;
//                enqueueRowEncoder(m_row_to_idx[row + 1]);
//                tryWakeOne(); /* wake up a sleeping thread or set the help wanted flag */
//            }
//        }
//
//        ScopedLock self(curRow.lock);
//        if ((m_bAllRowsStop[curRow.sliceId] && intRow > m_vbvResetTriggerRow[curRow.sliceId]) ||
//            (!bFirstRowInSlice && ((curRow.completed < numCols - 1) || (m_rows[row - 1].completed < numCols)) && m_rows[row - 1].completed < curRow.completed + 2))
//        {
//            curRow.active = false;
//            curRow.busy = false;
//            ATOMIC_INC(&m_countRowBlocks);
//            return;
//        }
//    }
//
//    /* this row of CTUs has been compressed */
//    if (m_param->bEnableWavefront && m_param->rc.bEnableConstVbv)
//    {
//        if (bLastRowInSlice)       
//        {
//            for (uint32_t r = m_sliceBaseRow[sliceId]; r < m_sliceBaseRow[sliceId + 1]; r++)
//            {
//                for (uint32_t c = curEncData.m_rowStat[r].numEncodedCUs + 1; c < numCols * (r + 1); c++)
//                {
//                    curEncData.m_rowStat[r].rowSatd += curEncData.m_cuStat[c].vbvCost;
//                    curEncData.m_rowStat[r].rowIntraSatd += curEncData.m_cuStat[c].intraVbvCost;
//                    curEncData.m_rowStat[r].encodedBits += curEncData.m_cuStat[c].totalBits;
//                    curEncData.m_rowStat[r].sumQpRc += curEncData.m_cuStat[c].baseQp;
//                    curEncData.m_rowStat[r].numEncodedCUs = c;
//                }
//            }
//        }
//    }
//
//    /* If encoding with ABR, update update bits and complexity in rate control
//     * after a number of rows so the next frame's rateControlStart has more
//     * accurate data for estimation. At the start of the encode we update stats
//     * after half the frame is encoded, but after this initial period we update
//     * after refLagRows (the number of rows reference frames must have completed
//     * before referencees may begin encoding) */
//    if (m_param->rc.rateControlMode == X265_RC_ABR || bIsVbv)
//    {
//        uint32_t rowCount = 0;
//        uint32_t maxRows = m_sliceBaseRow[sliceId + 1] - m_sliceBaseRow[sliceId];
//
//        if (rowInSlice == rowCount)
//        {
//            m_rowSliceTotalBits[sliceId] = 0;
//            if (bIsVbv && !(m_param->rc.bEnableConstVbv && m_param->bEnableWavefront))
//            {
//                for (uint32_t i = m_sliceBaseRow[sliceId]; i < rowCount + m_sliceBaseRow[sliceId]; i++)
//                    m_rowSliceTotalBits[sliceId] += curEncData.m_rowStat[i].encodedBits;
//            }
//            else
//            {
//                uint32_t startAddr = m_sliceBaseRow[sliceId] * numCols;
//                uint32_t finishAddr = startAddr + rowCount * numCols;
//                
//                for (uint32_t cuAddr = startAddr; cuAddr < finishAddr; cuAddr++)
//                    m_rowSliceTotalBits[sliceId] += curEncData.m_cuStat[cuAddr].totalBits;
//            }
//
//            if (ATOMIC_INC(&m_sliceCnt) == (int)m_param->maxSlices)
//            {
//                //m_rce.rowTotalBits = 0;
//                //for (uint32_t i = 0; i < m_param->maxSlices; i++)
//                 //   m_rce.rowTotalBits += m_rowSliceTotalBits[i];
//                //m_top->m_rateControl->rateControlUpdateStats(&m_rce);
//            }
//        }
//    }
//
//    /* flush row bitstream (if WPP and no SAO) or flush frame if no WPP and no SAO */
//    /* end_of_sub_stream_one_bit / end_of_slice_segment_flag */
//       if (!slice->m_bUseSao && (m_param->bEnableWavefront || bLastRowInSlice))
//               rowCoder.finishSlice();
//
//
//    /* trigger row-wise loop filters */
//    if (m_param->bEnableWavefront)
//    {
//        if (rowInSlice >= m_filterRowDelay)
//        {
//            enableRowFilter(m_row_to_idx[row - m_filterRowDelay]);
//
//            /* NOTE: Activate filter if first row (row 0) */
//            if (rowInSlice == m_filterRowDelay)
//                enqueueRowFilter(m_row_to_idx[row - m_filterRowDelay]);
//            tryWakeOne();
//        }
//
//        if (bLastRowInSlice)
//        {
//            for (uint32_t i = endRowInSlicePlus1 - m_filterRowDelay; i < endRowInSlicePlus1; i++)
//            {
//                enableRowFilter(m_row_to_idx[i]);
//            }
//            tryWakeOne();
//        }
//
//        // handle specially case - single row slice
//        if  (bFirstRowInSlice & bLastRowInSlice)
//        {
//            enqueueRowFilter(m_row_to_idx[row]);
//            tryWakeOne();
//        }
//    }
//
//    curRow.busy = false;
//
//    // CHECK_ME: Does it always FALSE condition?
//    if (ATOMIC_INC(&m_completionCount) == 2 * (int)m_numRows)
//        m_completionEvent.trigger();
//    return;
//}

Frame *FrameEncoder::getEncodedPicture()
{
    if (m_frame)
    {
        /* block here until worker thread completes */
        m_done.wait();

        Frame *ret = m_frame;
        m_frame = NULL;
        m_prevOutputTime = x265_mdate();
        return ret;
    }

    return NULL;
}
}
