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
#include "primitives.h"
#include "motion.h"
#include "x265.h"

#if _MSC_VER
#pragma warning(disable: 4127) // conditional  expression is constant (macros use this construct)
#endif

using namespace X265_NS;

namespace {

struct SubpelWorkload
{
    int hpel_iters;
    int hpel_dirs;
    int qpel_iters;
    int qpel_dirs;
    bool hpel_satd;
};

const SubpelWorkload workload[X265_MAX_SUBPEL_LEVEL + 1] =
{
    { 1, 4, 0, 4, false }, // 4 SAD HPEL only
    { 1, 4, 1, 4, false }, // 4 SAD HPEL + 4 SATD QPEL
    { 1, 4, 1, 4, true },  // 4 SATD HPEL + 4 SATD QPEL
    { 2, 4, 1, 4, true },  // 2x4 SATD HPEL + 4 SATD QPEL
    { 2, 4, 2, 4, true },  // 2x4 SATD HPEL + 2x4 SATD QPEL
    { 1, 8, 1, 8, true },  // 8 SATD HPEL + 8 SATD QPEL (default)
    { 2, 8, 1, 8, true },  // 2x8 SATD HPEL + 8 SATD QPEL
    { 2, 8, 2, 8, true },  // 2x8 SATD HPEL + 2x8 SATD QPEL
};

#define SAD_THRESH(v) (bcost < (((v >> 4) * sizeScale[partEnum])))

/* radius 2 hexagon. repeated entries are to avoid having to compute mod6 every time. */
const MV hex2[8] = { MV(-1, -2), MV(-2, 0), MV(-1, 2), MV(1, 2), MV(2, 0), MV(1, -2), MV(-1, -2), MV(-2, 0) };
const uint8_t mod6m1[8] = { 5, 0, 1, 2, 3, 4, 5, 0 };  /* (x-1)%6 */
const MV square1[9] = { MV(0, 0), MV(0, -1), MV(0, 1), MV(-1, 0), MV(1, 0), MV(-1, -1), MV(-1, 1), MV(1, -1), MV(1, 1) };
const MV hex4[16] =
{
    MV(0, -4), MV(0, 4), MV(-2, -3), MV(2, -3),
    MV(-4, -2), MV(4, -2), MV(-4, -1), MV(4, -1),
    MV(-4, 0), MV(4, 0), MV(-4, 1), MV(4, 1),
    MV(-4, 2), MV(4, 2), MV(-2, 3), MV(2, 3),
};
const MV offsets[] =
{
    MV(-1, 0), MV(0, -1),
    MV(-1, -1), MV(1, -1),
    MV(-1, 0), MV(1, 0),
    MV(-1, 1), MV(-1, -1),
    MV(1, -1), MV(1, 1),
    MV(-1, 0), MV(0, 1),
    MV(-1, 1), MV(1, 1),
    MV(1, 0), MV(0, 1),
}; // offsets for Two Point Search

/* sum of absolute differences between MV candidates, used for adaptive ME range */
inline int predictorDifference(const MV *mvc, intptr_t numCandidates)
{
    int sum = 0;

    for (int i = 0; i < numCandidates - 1; i++)
    {
        sum += abs(mvc[i].x - mvc[i + 1].x)
            +  abs(mvc[i].y - mvc[i + 1].y);
    }

    return sum;
}

}

MotionEstimate::MotionEstimate()
{
    ctuAddr = -1;
    absPartIdx = -1;
    searchMethod = X265_HEX_SEARCH;
    subpelRefine = 2;
    blockwidth = blockheight = 0;
    blockOffset = 0;
    bChromaSATD = false;
    chromaSatd = NULL;
}

void MotionEstimate::init(int csp)
{
    fencPUYuv.create(FENC_STRIDE, csp);
}

MotionEstimate::~MotionEstimate()
{
    fencPUYuv.destroy();
}

/* Called by lookahead, luma only, no use of PicYuv */
void MotionEstimate::setSourcePU(pixel *fencY, intptr_t stride, intptr_t offset, int pwidth, int pheight, const int method, const int refine)
{
    partEnum = partitionFromSizes(pwidth, pheight);
    X265_CHECK(LUMA_4x4 != partEnum, "4x4 inter partition detected!\n");
    sad = primitives.pu[partEnum].sad;
    ads = primitives.pu[partEnum].ads;
    satd = primitives.pu[partEnum].satd;
    sad_x3 = primitives.pu[partEnum].sad_x3;
    sad_x4 = primitives.pu[partEnum].sad_x4;


    blockwidth = pwidth;
    blockOffset = offset;
    absPartIdx = ctuAddr = -1;

    /* Search params */
    searchMethod = method;
    subpelRefine = refine;

    /* copy PU block into cache */
    primitives.pu[partEnum].copy_pp(fencPUYuv.m_buf[0], FENC_STRIDE, fencY + offset, stride);
    X265_CHECK(!bChromaSATD, "chroma distortion measurements impossible in this code path\n");
}
