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

#ifndef X265_CONSTANTS_H
#define X265_CONSTANTS_H

#include "common.h"

namespace X265_NS {
// private namespace

extern double x265_lambda_tab[QP_MAX_MAX + 1];
extern double x265_lambda2_tab[QP_MAX_MAX + 1];
extern const uint16_t x265_chroma_lambda2_offset_tab[MAX_CHROMA_LAMBDA_OFFSET + 1];


// flexible conversion from relative to absolute index
extern const uint32_t g_zscanToRaster[MAX_NUM_PARTITIONS];
extern const uint32_t g_rasterToZscan[MAX_NUM_PARTITIONS];

// conversion of partition index to picture pel position
extern const uint8_t g_zscanToPelX[MAX_NUM_PARTITIONS];
extern const uint8_t g_zscanToPelY[MAX_NUM_PARTITIONS];
extern const uint8_t g_log2Size[MAX_CU_SIZE + 1]; // from size to log2(size)

// global variable (CTU width/height, max. CU depth)
extern uint32_t g_maxLog2CUSize;
extern uint32_t g_maxCUSize;
extern uint32_t g_maxCUDepth;
extern uint32_t g_unitSizeDepth; // Depth at which 4x4 unit occurs from max CU size
extern uint32_t g_maxSlices; // number of Slices

extern const int16_t g_t4[4][4];
extern const int16_t g_t8[8][8];
extern const int16_t g_t16[16][16];
extern const int16_t g_t32[32][32];

// Subpel interpolation defines and constants

#define NTAPS_LUMA        8                            // Number of taps for luma
#define NTAPS_CHROMA      4                            // Number of taps for chroma
#define IF_INTERNAL_PREC 14                            // Number of bits for internal precision
#define IF_FILTER_PREC    6                            // Log2 of sum of filter taps
#define IF_INTERNAL_OFFS (1 << (IF_INTERNAL_PREC - 1)) // Offset used internally
#define SLFASE_CONSTANT  0x5f4e4a53


}

#endif
