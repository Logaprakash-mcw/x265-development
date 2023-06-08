/*****************************************************************************
* Copyright (C) 2013-2020 MulticoreWare, Inc
*
* Author: Steve Borho <steve@borho.org>
*         Min Chen <chenm003@163.com>
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
#include "picyuv.h"
#include "framedata.h"

using namespace X265_NS;

Frame::Frame()
{
    m_bChromaExtended = false;
    m_reconRowFlag = NULL;
    m_reconColCount = NULL;
    m_countRefEncoders = 0;
    m_encData = NULL;
    m_reconPic = NULL;
    m_next = NULL;
    m_prev = NULL;
    m_param = NULL;
    m_encodeStartTime = 0;
    m_reconfigureRc = false;
    m_classifyFrame = false;
    m_fieldNum = 0;
    m_picStruct = 0;
    m_edgePic = NULL;
    m_gaussianPic = NULL;
    m_thetaPic = NULL;
    m_edgeBitPlane = NULL;
    m_edgeBitPic = NULL;
    m_isInsideWindow = 0;

    // mcstf
    m_isSubSampled = NULL;
    m_mcstf = NULL;
    m_refPicCnt[0] = 0;
    m_refPicCnt[1] = 0;
    m_nextMCSTF = NULL;
    m_prevMCSTF = NULL;

    m_tempLayer = 0;
    m_sameLayerRefPic = false;
}

bool Frame::create(x265_param *param)
{
    m_fencPic = new PicYuv;
    m_param = param;

    wp_sum[0] = 0;
    wp_sum[1] = 0;
    wp_sum[2] = 0;

    wp_ssd[0] = 0;
    wp_ssd[1] = 0;
    wp_ssd[2] = 0;

    m_mcstf = new TemporalFilter;
    m_mcstf->init(param);

    m_fencPicSubsampled2 = new PicYuv;
    m_fencPicSubsampled4 = new PicYuv;

    if (!m_fencPicSubsampled2->createScaledPicYUV(param, 2))
        return false;
    if (!m_fencPicSubsampled4->createScaledPicYUV(param, 4))
        return false;

    CHECKED_MALLOC_ZERO(m_isSubSampled, int, 1);


    if (m_fencPic->create(param, !!m_param->bCopyPicToFrame))
    {
        X265_CHECK((m_reconColCount == NULL), "m_reconColCount was initialized");
        m_numRows = (m_fencPic->m_picHeight + param->maxCUSize - 1)  / param->maxCUSize;
        m_reconRowFlag = new ThreadSafeInteger[m_numRows];
        m_reconColCount = new ThreadSafeInteger[m_numRows];

        return true;
    }
    return false;
fail:
    return false;
}

bool Frame::createSubSample()
{

    m_fencPicSubsampled2 = new PicYuv;
    m_fencPicSubsampled4 = new PicYuv;

    if (!m_fencPicSubsampled2->createScaledPicYUV(m_param, 2))
        return false;
    if (!m_fencPicSubsampled4->createScaledPicYUV(m_param, 4))
        return false;
    CHECKED_MALLOC_ZERO(m_isSubSampled, int, 1);
    return true;
fail:
    return false;
}

bool Frame::allocEncodeData(x265_param *param, const SPS& sps)
{
    m_encData = new FrameData;
    m_reconPic = new PicYuv;
    m_param = param;
    m_encData->m_reconPic = m_reconPic;
    bool ok = m_encData->create(*param, m_fencPic->m_picCsp) && m_reconPic->create(param);
    if (ok)
    {
        /* initialize right border of m_reconpicYuv as SAO may read beyond the
         * end of the picture accessing uninitialized pixels */
        int maxHeight = sps.numCuInHeight * param->maxCUSize;
        memset(m_reconPic->m_picOrg[0], 0, sizeof(pixel)* m_reconPic->m_stride * maxHeight);

        /* use pre-calculated cu/pu offsets cached in the SPS structure */
        m_reconPic->m_cuOffsetY = sps.cuOffsetY;
        m_reconPic->m_buOffsetY = sps.buOffsetY;

        if (param->internalCsp != X265_CSP_I400)
        {
            memset(m_reconPic->m_picOrg[1], 0, sizeof(pixel) * m_reconPic->m_strideC * (maxHeight >> m_reconPic->m_vChromaShift));
            memset(m_reconPic->m_picOrg[2], 0, sizeof(pixel) * m_reconPic->m_strideC * (maxHeight >> m_reconPic->m_vChromaShift));

            /* use pre-calculated cu/pu offsets cached in the SPS structure */
            m_reconPic->m_cuOffsetC = sps.cuOffsetC;
            m_reconPic->m_buOffsetC = sps.buOffsetC;
        }
    }
    return ok;
}

void Frame::destroy()
{
    if (m_encData)
    {
        m_encData->destroy();
        delete m_encData;
        m_encData = NULL;
    }

    if (m_fencPic)
    {
        if (m_param->bCopyPicToFrame)
            m_fencPic->destroy();
        delete m_fencPic;
        m_fencPic = NULL;
    }


    if (m_fencPicSubsampled2)
    {
        m_fencPicSubsampled2->destroy();
        delete m_fencPicSubsampled2;
        m_fencPicSubsampled2 = NULL;
    }

    if (m_fencPicSubsampled4)
    {
        m_fencPicSubsampled4->destroy();
        delete m_fencPicSubsampled4;
        m_fencPicSubsampled4 = NULL;
    }
    delete m_mcstf;
    X265_FREE(m_isSubSampled);

    if(m_reconPic)
        m_reconPic->destroy();
    delete m_reconPic;
    m_reconPic = NULL;


    if (m_reconRowFlag)
    {
        delete[] m_reconRowFlag;
        m_reconRowFlag = NULL;
    }

    if (m_reconColCount)
    {
        delete[] m_reconColCount;
        m_reconColCount = NULL;
    }


}
