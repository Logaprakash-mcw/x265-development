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

#include "common.h"
#include "frame.h"
#include "framedata.h"
#include "picyuv.h"
#include "slice.h"

#include "dpb.h"

using namespace X265_NS;

DPB::~DPB()
{
    while (!m_freeList.empty())
    {
        Frame* curFrame = m_freeList.popFront();
        curFrame->destroy();
        delete curFrame;
    }

    while (!m_picList.empty())
    {
        Frame* curFrame = m_picList.popFront();
        curFrame->destroy();
        delete curFrame;
    }

    while (m_frameDataFreeList)
    {
        FrameData* next = m_frameDataFreeList->m_freeListNext;
        m_frameDataFreeList->destroy();

        m_frameDataFreeList->m_reconPic->destroy();
        delete m_frameDataFreeList->m_reconPic;

        delete m_frameDataFreeList;
        m_frameDataFreeList = next;
    }
}

// move unreferenced pictures from picList to freeList for recycle
void DPB::recycleUnreferenced()
{
    Frame *iterFrame = m_picList.first();

    while (iterFrame)
    {
        Frame *curFrame = iterFrame;
        iterFrame = iterFrame->m_next;
        bool isMCSTFReferenced = false;

        isMCSTFReferenced =!!(curFrame->m_refPicCnt[1]);

        if (!curFrame->m_encData->m_bHasReferences && !curFrame->m_countRefEncoders && !isMCSTFReferenced)
        {
            curFrame->m_bChromaExtended = false;

            *curFrame->m_isSubSampled = false;

            // Reset column counter
            X265_CHECK(curFrame->m_reconRowFlag != NULL, "curFrame->m_reconRowFlag check failure");
            X265_CHECK(curFrame->m_reconColCount != NULL, "curFrame->m_reconColCount check failure");
            X265_CHECK(curFrame->m_numRows > 0, "curFrame->m_numRows check failure");

            for(int32_t row = 0; row < curFrame->m_numRows; row++)
            {
                curFrame->m_reconRowFlag[row].set(0);
                curFrame->m_reconColCount[row].set(0);
            }

            // iterator is invalidated by remove, restart scan
            m_picList.remove(*curFrame);
            iterFrame = m_picList.first();

            m_freeList.pushBack(*curFrame);
            curFrame->m_encData->m_freeListNext = m_frameDataFreeList;
            m_frameDataFreeList = curFrame->m_encData;
            for (int i = 0; i < INTEGRAL_PLANE_NUM; i++)
            {
                if (curFrame->m_encData->m_meBuffer[i] != NULL)
                {
                    X265_FREE(curFrame->m_encData->m_meBuffer[i]);
                    curFrame->m_encData->m_meBuffer[i] = NULL;
                }
            }
            curFrame->m_encData = NULL;
            curFrame->m_reconPic = NULL;
        }
    }
}

void DPB::prepareEncode(Frame *newFrame)
{
    m_picList.pushFront(*newFrame);
}

