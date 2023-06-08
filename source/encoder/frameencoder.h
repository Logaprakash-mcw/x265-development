/*****************************************************************************
 * Copyright (C) 2013-2020 MulticoreWare, Inc
 *
 * Authors: Shin Yee <shinyee@multicorewareinc.com>
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

#ifndef X265_FRAMEENCODER_H
#define X265_FRAMEENCODER_H

#include "common.h"
#include "wavefront.h"
#include "frame.h"
#include "picyuv.h"
#include "md5.h"
#include "temporalfilter.h"
#include "fgm.h"

namespace X265_NS {
// private x265 namespace

class ThreadPool;
class Encoder;

#define ANGULAR_MODE_ID 2
#define AMP_ID 3


/*Film grain characteristics*/
struct FilmGrain
{
    bool    m_filmGrainCharacteristicsCancelFlag;
    bool    m_filmGrainCharacteristicsPersistenceFlag;
    bool    m_separateColourDescriptionPresentFlag;
    uint8_t m_filmGrainModelId;
    uint8_t m_blendingModeId;
    uint8_t m_log2ScaleFactor;
};

struct ColourDescription
{
    bool        m_filmGrainFullRangeFlag;
    uint8_t     m_filmGrainBitDepthLumaMinus8;
    uint8_t     m_filmGrainBitDepthChromaMinus8;
    uint8_t     m_filmGrainColourPrimaries;
    uint8_t     m_filmGrainTransferCharacteristics;
    uint8_t     m_filmGrainMatrixCoeffs;
};

struct FGPresent
{
    uint8_t     m_blendingModeId;
    uint8_t     m_log2ScaleFactor;
    bool        m_presentFlag[3];
};

// Manages the wave-front processing of a single encoding frame
class FrameEncoder : public WaveFront, public Thread
{
public:

    FrameEncoder();

    virtual ~FrameEncoder() {}

    virtual bool init(Encoder *top, int numRows, int numCols);

    void destroy();

    /* triggers encode of a new frame by the worker thread */
    bool startCompressFrame(Frame* curFrame);

    /* blocks until worker thread is done, returns access unit */
    Frame *getEncodedPicture();

    Event                    m_enable;
    Event                    m_done;
    Event                    m_completionEvent;
    int                      m_localTldIdx;
    bool                     m_reconfigure; /* reconfigure in progress */
    volatile bool            m_threadActive;

    uint32_t                 m_numRows;
    uint32_t                 m_numCols;
    //uint32_t                 m_filterRowDelay;
    //uint32_t                 m_filterRowDelayCus;
    //uint32_t                 m_refLagRows;

    //volatile int             m_activeWorkerCount;        // count of workers currently encoding or filtering CTUs
    //volatile int             m_totalActiveWorkerCount;   // sum of m_activeWorkerCount sampled at end of each CTU
    //volatile int             m_activeWorkerCountSamples; // count of times m_activeWorkerCount was sampled (think vbv restarts)
    //volatile int             m_countRowBlocks;           // count of workers forced to abandon a row because of top dependency
    int64_t                  m_startCompressTime;        // timestamp when frame encoder is given a frame
    //int64_t                  m_row0WaitTime;             // timestamp when row 0 is allowed to start
    //int64_t                  m_allRowsAvailableTime;     // timestamp when all reference dependencies are resolved
    int64_t                  m_endCompressTime;          // timestamp after all CTUs are compressed
    int64_t                  m_endFrameTime;             // timestamp after RCEnd, NR updates, etc
    //int64_t                  m_stallStartTime;           // timestamp when worker count becomes 0
    int64_t                  m_prevOutputTime;           // timestamp when prev frame was retrieved by API thread
    //int64_t                  m_slicetypeWaitTime;        // total elapsed time waiting for decided frame
    //int64_t                  m_totalWorkerElapsedTime;   // total elapsed time spent by worker threads processing CTUs
    //int64_t                  m_totalNoWorkerTime;        // total elapsed time without any active worker threads

    Encoder*                 m_top;
    x265_param*              m_param;
    Frame*                   m_frame;
    //ThreadLocalData*         m_tld; /* for --no-wpp */
    //uint32_t*                m_substreamSizes;

    //CUGeom*                  m_cuGeoms;
    //uint32_t*                m_ctuGeomMap;



    // initialization for mcstf
    TemporalFilter*          m_frameEncTF;
    TemporalFilterRefPicInfo m_mcstfRefList[MAX_MCSTF_TEMPORAL_WINDOW_LENGTH];

    FGAnalyser*              m_fg;

protected:


    /* analyze / compress frame, can be run in parallel within reference constraints */
    void compressFrame();


    void threadMain();

    ///* Called by WaveFront::findJob() */
    //virtual void processRow();
    //virtual void processRow(int row, int threadId);
    //virtual void processRowEncoder(int row);

    void enqueueRowEncoder(int row) { WaveFront::enqueueRow(row * 2 + 0); }
    void enqueueRowFilter(int row)  { WaveFront::enqueueRow(row * 2 + 1); }
    void enableRowEncoder(int row)  { WaveFront::enableRow(row * 2 + 0); }
    void enableRowFilter(int row)   { WaveFront::enableRow(row * 2 + 1); }
};
}

#endif // ifndef X265_FRAMEENCODER_H
