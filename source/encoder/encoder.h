/*****************************************************************************
 * Copyright (C) 2013-2020 MulticoreWare, Inc
 *
 * Authors: Steve Borho <steve@borho.org>
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

#ifndef X265_ENCODER_H
#define X265_ENCODER_H

#include "common.h"
#include "slice.h"
#include "threading.h"
#include "x265.h"
#include "framedata.h"
#include "temporalfilter.h"
struct x265_encoder {};
namespace X265_NS {
// private namespace
extern const char g_sliceTypeToChar[3];

class Entropy;

#define MAX_NUM_REF_IDX 64
#define DUP_BUFFER 2
#define doubling 7
#define tripling 8

struct RefIdxLastGOP
{
    int numRefIdxDefault[2];
    int numRefIdxl0[MAX_NUM_REF_IDX];
    int numRefIdxl1[MAX_NUM_REF_IDX];
};


struct cuLocation
{
    bool skipWidth;
    bool skipHeight;
    uint32_t heightInCU;
    uint32_t widthInCU;
    uint32_t oddRowIndex;
    uint32_t evenRowIndex;
    uint32_t switchCondition;

    void init(x265_param* param)
    {
        skipHeight = false;
        skipWidth = false;
        heightInCU = (param->sourceHeight + param->maxCUSize - 1) >> param->maxLog2CUSize;
        widthInCU = (param->sourceWidth + param->maxCUSize - 1) >> param->maxLog2CUSize;
        evenRowIndex = 0;
        oddRowIndex = param->num4x4Partitions * widthInCU;
        switchCondition = 0; // To switch between odd and even rows
    }
};

struct puOrientation
{
    bool isVert;
    bool isRect;
    bool isAmp;

    void init()
    {
        isRect = false;
        isAmp = false;
        isVert = false;
    }
};

struct AdaptiveFrameDuplication
{
    x265_picture* dupPic;
    char* dupPlane;

    //Flag to denote the availability of the picture buffer.
    bool bOccupied;

    //Flag to check whether the picture has duplicated.
    bool bDup;
};

class FrameEncoder;
class DPB;
class Lookahead;
class ThreadPool;
class FrameData;

class Encoder : public x265_encoder
{
public:

    int64_t            m_firstPts;
    int64_t            m_encodeStartTime;
    int64_t            m_bframeDelayTime;

    int                m_pocLast;         // time index (POC)
    int                m_encodedFrameNum;
    int                m_outputCount;
    int                m_bframeDelay;
    int                m_numPools;
    int                m_curEncoder;
    uint32_t           m_numDelayedPic;

    ThreadPool*        m_threadPool;
    FrameEncoder*      m_frameEncoder[X265_MAX_FRAME_THREADS];
    DPB*               m_dpb;
    Frame*             m_exportedPic;
    x265_param*        m_param;
    x265_param*        m_latestParam;     // Holds latest param during a reconfigure
    Lookahead*         m_lookahead;

    SPS                m_sps;
    Window             m_conformanceWindow;

    bool               m_bZeroLatency;     // x265_encoder_encode() returns NALs for the input picture, zero lag
    bool               m_aborted;          // fatal error detected
    bool               m_reconfigure;      // Encoder reconfigure in progress
    bool               m_reconfigureRc;
    bool               m_reconfigureZone;

    int                m_saveCtuDistortionLevel;

    /* Begin intra refresh when one not in progress or else begin one as soon as the current 
     * one is done. Requires bIntraRefresh to be set.*/
    int                m_bQueuedIntraRefresh;


    ThreadSafeInteger* zoneReadCount;
    ThreadSafeInteger* zoneWriteCount;

    /* Film grain model file */
    FILE* m_filmGrainIn;
    OrigPicBuffer* m_origPicBuffer;

    Encoder();
    ~Encoder(){};

    void create();
    void stopJobs();
    void destroy();

    int encode(const x265_picture* pic, x265_picture *pic_out);

    void configure(x265_param *param);

    void copyPicture(x265_picture *dest, const x265_picture *src);

    bool generateMcstfRef(Frame* frameEnc, FrameEncoder* currEncoder);


};
}

#endif // ifndef X265_ENCODER_H
