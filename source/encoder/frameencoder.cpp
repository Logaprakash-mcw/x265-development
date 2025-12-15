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
        //m_fg->fout = this->m_top->m_filmgrainin;
    }

    return ok;
}

/* Generate a complete list of unique geom sets for the current picture dimensions */
bool FrameEncoder::startCompressFrame(Frame* curFrame, Frame* denoisedFrame)
{
    m_frame = curFrame;
    if (m_param->bEnableTemporalFilter)
    {
        curFrame->m_encData->m_frameEncoderID = m_jpId;
        curFrame->m_encData->m_jobProvider = this;
    }
    else
    {
        m_denoisedFrame = denoisedFrame;
    }
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

    if (m_param->bEnableTemporalFilter)
    {
        PicYuv *original = new PicYuv();
        original->create(m_param);
        original->copyFromFrame(m_frame->m_fencPic);

        m_frameEncTF->m_QP = m_param->qp; // Keep qp is constant
        m_frameEncTF->bilateralFilter(m_frame, m_mcstfRefList, m_param->temporalFilterStrength);
        m_fg->initBufs(original, m_frame->m_fencPic);
    }
    else
    {
        m_fg->initBufs(m_frame->m_fencPic, m_denoisedFrame->m_fencPic);
    }
    m_fg->estimate_grain_parameters();

    if (m_param->bEnableTemporalFilter)
    {
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
    }
    m_fg->set_film_grain_parameters();
    //if (m_top->m_filmGrainIn)
    //    m_fg->write_film_grain_parameters();

    m_frame->m_fencPic->m_fgChar = &m_fg->filmgrain;
    m_endCompressTime = m_endFrameTime = x265_mdate();
}

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
