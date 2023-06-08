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

#ifndef X265_SLICETYPE_H
#define X265_SLICETYPE_H

#include "common.h"
#include "slice.h"
#include "motion.h"
#include "piclist.h"
#include "threadpool.h"

namespace X265_NS {
// private namespace

struct Lowres;
class Frame;
class Lookahead;

#define LOWRES_COST_MASK  ((1 << 14) - 1)
#define LOWRES_COST_SHIFT 14
#define AQ_EDGE_BIAS 0.5
#define EDGE_INCLINATION 45
#define TEMPORAL_SCENECUT_THRESHOLD 50

#define X265_ABS(a)                        (((a) < 0) ? (-(a)) : (a))

#define PICTURE_DIFF_VARIANCE_TH            390
#define PICTURE_VARIANCE_TH                 1500
#define LOW_VAR_SCENE_CHANGE_TH             2250
#define HIGH_VAR_SCENE_CHANGE_TH            3500

#define PICTURE_DIFF_VARIANCE_CHROMA_TH     10
#define PICTURE_VARIANCE_CHROMA_TH          20
#define LOW_VAR_SCENE_CHANGE_CHROMA_TH      2250/4
#define HIGH_VAR_SCENE_CHANGE_CHROMA_TH     3500/4

#define FLASH_TH                            1.5
#define FADE_TH                             4
#define INTENSITY_CHANGE_TH                 4

#define NUM64x64INPIC(w,h)                  ((w*h)>> (MAX_LOG2_CU_SIZE<<1))

#if HIGH_BIT_DEPTH
#define EDGE_THRESHOLD 1023.0
#else
#define EDGE_THRESHOLD 255.0
#endif

/* Thread local data for lookahead tasks */
struct LookaheadTLD
{
    MotionEstimate  me;
    pixel*          wbuffer[4];
    int             widthInCU;
    int             heightInCU;
    int             ncu;
    int             paddedLines;

    LookaheadTLD()
    {
        me.init(X265_CSP_I400);
        me.setQP(X265_LOOKAHEAD_QP);
        for (int i = 0; i < 4; i++)
            wbuffer[i] = NULL;
        widthInCU = heightInCU = ncu = paddedLines = 0;
    }

    void init(int w, int h, int n)
    {
        widthInCU = w;
        heightInCU = h;
        ncu = n;
    }

    ~LookaheadTLD() { X265_FREE(wbuffer[0]); }



    uint32_t calcVariance(pixel* src, intptr_t stride, intptr_t blockOffset, uint32_t plane);

protected:

    uint32_t acEnergyCu(Frame* curFrame, uint32_t blockX, uint32_t blockY, int csp, uint32_t qgSize);
    uint32_t edgeDensityCu(Frame* curFrame, uint32_t &avgAngle, uint32_t blockX, uint32_t blockY, uint32_t qgSize);

};

class Lookahead : public JobProvider
{
public:

    PicList       m_inputQueue;      // input pictures in order received
    PicList       m_outputQueue;     // pictures to be encoded, in encode order
    Lock          m_inputLock;
    Lock          m_outputLock;
    Event         m_outputSignal;
    LookaheadTLD* m_tld;
    x265_param*   m_param;
    Lowres*       m_lastNonB;
    int*          m_scratch;         // temp buffer for cutree propagate

    /* pre-lookahead */
    int           m_fullQueueSize;
    int           m_lastKeyframe;
    int           m_8x8Width;
    int           m_8x8Height;
    int           m_8x8Blocks;
    int           m_cuCount;
    int           m_numCoopSlices;
    int           m_numRowsPerSlice;
    int           m_inputCount;
    double        m_cuTreeStrength;

    /* HME */
    int           m_4x4Width;
    int           m_4x4Height;

    bool          m_isActive;
    bool          m_sliceTypeBusy;
    bool          m_bAdaptiveQuant;
    bool          m_outputSignalRequired;
    bool          m_bBatchMotionSearch;
    bool          m_bBatchFrameCosts;
    bool          m_filled;
    bool          m_isSceneTransition;
    int           m_numPools;
    bool          m_extendGopBoundary;
    double        m_frameVariance[X265_BFRAME_MAX + 4];

    bool          m_resetRunningAvg;
    uint32_t      m_segmentCountThreshold;

    int8_t                  m_gopId;

    Lookahead(x265_param *param, ThreadPool *pool);

    bool    create();
    void    destroy();
    void    stopJobs();

    //void    addPicture(Frame&, int sliceType);
    void    addPicture(Frame& curFrame);
    void    checkLookaheadQueue(int &frameCnt);
    void    flush();
    Frame*  getDecidedPicture();

    void    setLookaheadQueue();

protected:

    void    findJob(int workerThreadID);
    void    slicetypeDecide();


};

class PreLookaheadGroup : public BondedTaskGroup
{
public:

    Frame* m_preframes[X265_LOOKAHEAD_MAX];
    Lookahead& m_lookahead;

    PreLookaheadGroup(Lookahead& l) : m_lookahead(l) {}

    void processTasks(int workerThreadID);

protected:

    PreLookaheadGroup& operator=(const PreLookaheadGroup&);
};

}
#endif // ifndef X265_SLICETYPE_H
