/*****************************************************************************
* Copyright (C) 2013-2020 MulticoreWare, Inc
*
* Author: Steve Borho <steve@borho.org>
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

#ifndef X265_FRAMEDATA_H
#define X265_FRAMEDATA_H

#include "common.h"
#include "slice.h"
//#include "cudata.h"

namespace X265_NS {
// private namespace

class PicYuv;
class JobProvider;

#define INTER_MODES 4 // 2Nx2N, 2NxN, Nx2N, AMP modes
#define INTRA_MODES 3 // DC, Planar, Angular modes

/* Per-frame data that is used during encodes and referenced while the picture
 * is available for reference. A FrameData instance is attached to a Frame as it
 * comes out of the lookahead. Frames which are not being encoded do not have a
 * FrameData instance. These instances are re-used once the encoded frame has
 * no active references. They hold the Slice instance and the 'official' CTU
 * data structures. They are maintained in a free-list pool along together with
 * a reconstructed image PicYuv in order to conserve memory. */
class FrameData
{
public:

    Slice*         m_slice;
    const x265_param* m_param;

    FrameData*     m_freeListNext;
    PicYuv*        m_reconPic;
    bool           m_bHasReferences;   /* used during DPB/RPS updates */
    int            m_frameEncoderID;   /* the ID of the FrameEncoder encoding this frame */
    JobProvider*   m_jobProvider;

    //CUData*        m_picCTU;

    int            m_spsrpsIdx;
    //FrameStats     m_frameStats; // stats of current frame for multi-pass encodes

    int            m_picCsp;

    uint32_t*              m_meIntegral[INTEGRAL_PLANE_NUM];       // 12 integral planes for 32x32, 32x24, 32x8, 24x32, 16x16, 16x12, 16x4, 12x16, 8x32, 8x8, 4x16 and 4x4.
    uint32_t*              m_meBuffer[INTEGRAL_PLANE_NUM];

    FrameData();

    bool create(const x265_param& param, const SPS& sps, int csp);

    void destroy();
    //inline CUData* getPicCTU(uint32_t ctuAddr) { return &m_picCTU[ctuAddr]; }
};

}
#endif // ifndef X265_FRAMEDATA_H
