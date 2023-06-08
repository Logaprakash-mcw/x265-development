/*****************************************************************************
 * Copyright (C) 2013-2020 MulticoreWare, Inc
 *
 * Authors: Steve Borho <steve@borho.org>
 *          Min Chen <chenm003@163.com>
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

#ifndef X265_SLICE_H
#define X265_SLICE_H

#include "common.h"

namespace X265_NS {
// private namespace

class Frame;
class PicList;
class PicYuv;
class MotionReference;

struct TimingInfo
{
    uint32_t numUnitsInTick;
    uint32_t timeScale;
};


struct Window
{
    int  leftOffset;
    int  rightOffset;
    int  topOffset;
    int  bottomOffset;
    bool bEnabled;

    Window()
    {
        bEnabled = false;
    }
};


struct SPS
{
    /* cached PicYuv offset arrays, shared by all instances of
     * PicYuv created by this encoder */
    intptr_t* cuOffsetY;
    intptr_t* cuOffsetC;
    intptr_t* buOffsetY;
    intptr_t* buOffsetC;

    int      chromaFormatIdc;        // use param
    uint32_t picWidthInLumaSamples;  // use param
    uint32_t picHeightInLumaSamples; // use param

    uint32_t numCuInWidth;
    uint32_t numCuInHeight;
    uint32_t numCUsInFrame;
    uint32_t numPartitions;
    uint32_t numPartInCUSize;

    int      log2MinCodingBlockSize;
    int      log2DiffMaxMinCodingBlockSize;
    int      log2MaxPocLsb;

    uint32_t quadtreeTULog2MaxSize;
    uint32_t quadtreeTULog2MinSize;

    uint32_t quadtreeTUMaxDepthInter; // use param
    uint32_t quadtreeTUMaxDepthIntra; // use param

    uint32_t maxAMPDepth;

    int      numGOPBegin;

    bool     bUseSAO; // use param
    bool     bUseAMP; // use param
    bool     bUseStrongIntraSmoothing; // use param
    bool     bTemporalMVPEnabled;
    bool     bEmitVUITimingInfo;
    bool     bEmitVUIHRDInfo;

    Window   conformanceWindow;

    SPS()
    {
        memset(this, 0, sizeof(*this));
    }

    ~SPS()
    {
        X265_FREE(cuOffsetY);
        X265_FREE(cuOffsetC);
        X265_FREE(buOffsetY);
        X265_FREE(buOffsetC);
    }
};


#define SET_WEIGHT(w, b, s, d, o) \
    { \
        (w).inputWeight = (s); \
        (w).log2WeightDenom = (d); \
        (w).inputOffset = (o); \
        (w).wtPresent = (b); \
    }

class Slice
{
public:

    const SPS*  m_sps;
    Frame*      m_refFrameList[2][MAX_NUM_REF + 1];
    PicYuv*     m_refReconPicList[2][MAX_NUM_REF + 1];
    MotionReference (*m_mref)[MAX_NUM_REF + 1];
    int         m_sliceQp;
    int         m_chromaQpOffset[2];
    int         m_poc;
    int         m_lastIDR;
    int         m_rpsIdx;


    int         m_numRefIdx[2];
    int         m_refPOCList[2][MAX_NUM_REF + 1];

    uint32_t    m_maxNumMergeCand; // use param
    uint32_t    m_endCUAddr;

    int         m_iPPSQpMinus26;
    int         numRefIdxDefault[2];

    const x265_param *m_param;
    int         m_fieldNum;
    Frame*      m_mcstfRefFrameList[2][MAX_MCSTF_TEMPORAL_WINDOW_LENGTH];

    Slice()
    {
        m_lastIDR = 0;
        m_numRefIdx[0] = m_numRefIdx[1] = 0;
        memset(m_refFrameList, 0, sizeof(m_refFrameList));
        memset(m_refReconPicList, 0, sizeof(m_refReconPicList));
        memset(m_refPOCList, 0, sizeof(m_refPOCList));
        m_iPPSQpMinus26 = 0;
        numRefIdxDefault[0] = 1;
        numRefIdxDefault[1] = 1;
        m_rpsIdx = -1;
        m_chromaQpOffset[0] = m_chromaQpOffset[1] = 0;
        m_fieldNum = 0;
    }

};

}

#endif // ifndef X265_SLICE_H
