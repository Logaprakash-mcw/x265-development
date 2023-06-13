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

namespace X265_NS {

Lookahead::Lookahead(x265_param *param, ThreadPool* pool)
{
    m_param = param;
    m_pool  = pool;
    m_tld      = NULL;
    m_filled   = false;
    m_outputSignalRequired = false;
    m_isActive = true;
    m_inputCount = 0;
    m_sliceTypeBusy = false;
}


bool Lookahead::create()
{
    int numTLD = 1 + (m_pool ? m_pool->m_numWorkers : 0);
    m_tld = new LookaheadTLD[numTLD];
    return m_tld ;
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

void PreLookaheadGroup::processTasks(int workerThreadID)
{
    if (workerThreadID < 0)
        workerThreadID = m_lookahead.m_pool ? m_lookahead.m_pool->m_numWorkers : 0;

    m_lock.acquire();
    while (m_jobAcquired < m_jobTotal)
    {
        m_jobAcquired++;
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
   // Lowres* frames[X265_LOOKAHEAD_MAX + 1 + 4];
    Frame*  list[1 + 4];
   // memset(frames, 0, sizeof(frames));
    memset(list, 0, sizeof(list));

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

        m_inputQueue.popFront();

        m_inputLock.release();

        m_outputLock.acquire();

        m_outputQueue.pushBack(*list[0]);

        m_outputLock.release();

}

}
