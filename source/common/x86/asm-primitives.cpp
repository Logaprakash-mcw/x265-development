/*****************************************************************************
 * Copyright (C) 2013-2020 MulticoreWare, Inc
 *
 * Authors: Steve Borho <steve@borho.org>
 *          Praveen Kumar Tiwari <praveen@multicorewareinc.com>
 *          Min Chen <chenm003@163.com> <min.chen@multicorewareinc.com>
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
#include "x265.h"
#include "cpu.h"

#define FUNCDEF_TU(ret, name, cpu, ...) \
    ret PFX(name ## _4x4_ ## cpu(__VA_ARGS__)); \
    ret PFX(name ## _8x8_ ## cpu(__VA_ARGS__)); \
    ret PFX(name ## _16x16_ ## cpu(__VA_ARGS__)); \
    ret PFX(name ## _32x32_ ## cpu(__VA_ARGS__)); \
    ret PFX(name ## _64x64_ ## cpu(__VA_ARGS__))

#define FUNCDEF_TU_S(ret, name, cpu, ...) \
    ret PFX(name ## _4_ ## cpu(__VA_ARGS__)); \
    ret PFX(name ## _8_ ## cpu(__VA_ARGS__)); \
    ret PFX(name ## _16_ ## cpu(__VA_ARGS__)); \
    ret PFX(name ## _32_ ## cpu(__VA_ARGS__)); \
    ret PFX(name ## _64_ ## cpu(__VA_ARGS__))

#define FUNCDEF_TU_S2(ret, name, cpu, ...) \
    ret PFX(name ## 4_ ## cpu(__VA_ARGS__)); \
    ret PFX(name ## 8_ ## cpu(__VA_ARGS__)); \
    ret PFX(name ## 16_ ## cpu(__VA_ARGS__)); \
    ret PFX(name ## 32_ ## cpu(__VA_ARGS__)); \
    ret PFX(name ## 64_ ## cpu(__VA_ARGS__))

#define FUNCDEF_PU(ret, name, cpu, ...) \
    ret PFX(name ## _4x4_   ## cpu)(__VA_ARGS__); \
    ret PFX(name ## _8x8_   ## cpu)(__VA_ARGS__); \
    ret PFX(name ## _16x16_ ## cpu)(__VA_ARGS__); \
    ret PFX(name ## _32x32_ ## cpu)(__VA_ARGS__); \
    ret PFX(name ## _64x64_ ## cpu)(__VA_ARGS__); \
    ret PFX(name ## _8x4_   ## cpu)(__VA_ARGS__); \
    ret PFX(name ## _4x8_   ## cpu)(__VA_ARGS__); \
    ret PFX(name ## _16x8_  ## cpu)(__VA_ARGS__); \
    ret PFX(name ## _8x16_  ## cpu)(__VA_ARGS__); \
    ret PFX(name ## _16x32_ ## cpu)(__VA_ARGS__); \
    ret PFX(name ## _32x16_ ## cpu)(__VA_ARGS__); \
    ret PFX(name ## _64x32_ ## cpu)(__VA_ARGS__); \
    ret PFX(name ## _32x64_ ## cpu)(__VA_ARGS__); \
    ret PFX(name ## _16x12_ ## cpu)(__VA_ARGS__); \
    ret PFX(name ## _12x16_ ## cpu)(__VA_ARGS__); \
    ret PFX(name ## _16x4_  ## cpu)(__VA_ARGS__); \
    ret PFX(name ## _4x16_  ## cpu)(__VA_ARGS__); \
    ret PFX(name ## _32x24_ ## cpu)(__VA_ARGS__); \
    ret PFX(name ## _24x32_ ## cpu)(__VA_ARGS__); \
    ret PFX(name ## _32x8_  ## cpu)(__VA_ARGS__); \
    ret PFX(name ## _8x32_  ## cpu)(__VA_ARGS__); \
    ret PFX(name ## _64x48_ ## cpu)(__VA_ARGS__); \
    ret PFX(name ## _48x64_ ## cpu)(__VA_ARGS__); \
    ret PFX(name ## _64x16_ ## cpu)(__VA_ARGS__); \
    ret PFX(name ## _16x64_ ## cpu)(__VA_ARGS__)

#define FUNCDEF_CHROMA_PU(ret, name, cpu, ...) \
    FUNCDEF_PU(ret, name, cpu, __VA_ARGS__); \
    ret PFX(name ## _4x2_ ## cpu)(__VA_ARGS__); \
    ret PFX(name ## _2x4_ ## cpu)(__VA_ARGS__); \
    ret PFX(name ## _8x2_ ## cpu)(__VA_ARGS__); \
    ret PFX(name ## _2x8_ ## cpu)(__VA_ARGS__); \
    ret PFX(name ## _8x6_ ## cpu)(__VA_ARGS__); \
    ret PFX(name ## _6x8_ ## cpu)(__VA_ARGS__); \
    ret PFX(name ## _8x12_ ## cpu)(__VA_ARGS__); \
    ret PFX(name ## _12x8_ ## cpu)(__VA_ARGS__); \
    ret PFX(name ## _6x16_ ## cpu)(__VA_ARGS__); \
    ret PFX(name ## _16x6_ ## cpu)(__VA_ARGS__); \
    ret PFX(name ## _2x16_ ## cpu)(__VA_ARGS__); \
    ret PFX(name ## _16x2_ ## cpu)(__VA_ARGS__); \
    ret PFX(name ## _4x12_ ## cpu)(__VA_ARGS__); \
    ret PFX(name ## _12x4_ ## cpu)(__VA_ARGS__); \
    ret PFX(name ## _32x12_ ## cpu)(__VA_ARGS__); \
    ret PFX(name ## _12x32_ ## cpu)(__VA_ARGS__); \
    ret PFX(name ## _32x4_ ## cpu)(__VA_ARGS__); \
    ret PFX(name ## _4x32_ ## cpu)(__VA_ARGS__); \
    ret PFX(name ## _32x48_ ## cpu)(__VA_ARGS__); \
    ret PFX(name ## _48x32_ ## cpu)(__VA_ARGS__); \
    ret PFX(name ## _16x24_ ## cpu)(__VA_ARGS__); \
    ret PFX(name ## _24x16_ ## cpu)(__VA_ARGS__); \
    ret PFX(name ## _8x64_ ## cpu)(__VA_ARGS__); \
    ret PFX(name ## _64x8_ ## cpu)(__VA_ARGS__); \
    ret PFX(name ## _64x24_ ## cpu)(__VA_ARGS__); \
    ret PFX(name ## _24x64_ ## cpu)(__VA_ARGS__);

extern "C" {
#include "pixel.h"
#include "pixel-util.h"
#include "mc.h"
//#include "loopfilter.h"
#include "blockcopy8.h"
//#include "intrapred.h"
#include "dct8.h"
//#include "seaintegral.h"
}
#define ALL_LUMA_CU_TYPED(prim, fncdef, fname, cpu) \
    p.cu[BLOCK_8x8].prim   = fncdef PFX(fname ## _8x8_ ## cpu); \
    p.cu[BLOCK_16x16].prim = fncdef PFX(fname ## _16x16_ ## cpu); \
    p.cu[BLOCK_32x32].prim = fncdef PFX(fname ## _32x32_ ## cpu); \
    p.cu[BLOCK_64x64].prim = fncdef PFX(fname ## _64x64_ ## cpu)
#define ALL_LUMA_CU_TYPED_S(prim, fncdef, fname, cpu) \
    p.cu[BLOCK_8x8].prim   = fncdef PFX(fname ## 8_ ## cpu); \
    p.cu[BLOCK_16x16].prim = fncdef PFX(fname ## 16_ ## cpu); \
    p.cu[BLOCK_32x32].prim = fncdef PFX(fname ## 32_ ## cpu); \
    p.cu[BLOCK_64x64].prim = fncdef PFX(fname ## 64_ ## cpu)
#define ALL_LUMA_TU_TYPED(prim, fncdef, fname, cpu) \
    p.cu[BLOCK_4x4].prim   = fncdef PFX(fname ## _4x4_ ## cpu); \
    p.cu[BLOCK_8x8].prim   = fncdef PFX(fname ## _8x8_ ## cpu); \
    p.cu[BLOCK_16x16].prim = fncdef PFX(fname ## _16x16_ ## cpu); \
    p.cu[BLOCK_32x32].prim = fncdef PFX(fname ## _32x32_ ## cpu)
#define ALL_LUMA_TU_TYPED_S(prim, fncdef, fname, cpu) \
    p.cu[BLOCK_4x4].prim   = fncdef PFX(fname ## 4_ ## cpu); \
    p.cu[BLOCK_8x8].prim   = fncdef PFX(fname ## 8_ ## cpu); \
    p.cu[BLOCK_16x16].prim = fncdef PFX(fname ## 16_ ## cpu); \
    p.cu[BLOCK_32x32].prim = fncdef PFX(fname ## 32_ ## cpu)
#define ALL_LUMA_BLOCKS_TYPED(prim, fncdef, fname, cpu) \
    p.cu[BLOCK_4x4].prim   = fncdef PFX(fname ## _4x4_ ## cpu); \
    p.cu[BLOCK_8x8].prim   = fncdef PFX(fname ## _8x8_ ## cpu); \
    p.cu[BLOCK_16x16].prim = fncdef PFX(fname ## _16x16_ ## cpu); \
    p.cu[BLOCK_32x32].prim = fncdef PFX(fname ## _32x32_ ## cpu); \
    p.cu[BLOCK_64x64].prim = fncdef PFX(fname ## _64x64_ ## cpu);
#define ALL_LUMA_CU(prim, fname, cpu)      ALL_LUMA_CU_TYPED(prim, , fname, cpu)
#define ALL_LUMA_CU_S(prim, fname, cpu)    ALL_LUMA_CU_TYPED_S(prim, , fname, cpu)
#define ALL_LUMA_TU(prim, fname, cpu)      ALL_LUMA_TU_TYPED(prim, , fname, cpu)
#define ALL_LUMA_BLOCKS(prim, fname, cpu)  ALL_LUMA_BLOCKS_TYPED(prim, , fname, cpu)
#define ALL_LUMA_TU_S(prim, fname, cpu)    ALL_LUMA_TU_TYPED_S(prim, , fname, cpu)

#define ALL_LUMA_PU_TYPED(prim, fncdef, fname, cpu) \
    p.pu[LUMA_8x8].prim   = fncdef PFX(fname ## _8x8_ ## cpu); \
    p.pu[LUMA_16x16].prim = fncdef PFX(fname ## _16x16_ ## cpu); \
    p.pu[LUMA_32x32].prim = fncdef PFX(fname ## _32x32_ ## cpu); \
    p.pu[LUMA_64x64].prim = fncdef PFX(fname ## _64x64_ ## cpu); \
    p.pu[LUMA_8x4].prim   = fncdef PFX(fname ## _8x4_ ## cpu); \
    p.pu[LUMA_4x8].prim   = fncdef PFX(fname ## _4x8_ ## cpu); \
    p.pu[LUMA_16x8].prim  = fncdef PFX(fname ## _16x8_ ## cpu); \
    p.pu[LUMA_8x16].prim  = fncdef PFX(fname ## _8x16_ ## cpu); \
    p.pu[LUMA_16x32].prim = fncdef PFX(fname ## _16x32_ ## cpu); \
    p.pu[LUMA_32x16].prim = fncdef PFX(fname ## _32x16_ ## cpu); \
    p.pu[LUMA_64x32].prim = fncdef PFX(fname ## _64x32_ ## cpu); \
    p.pu[LUMA_32x64].prim = fncdef PFX(fname ## _32x64_ ## cpu); \
    p.pu[LUMA_16x12].prim = fncdef PFX(fname ## _16x12_ ## cpu); \
    p.pu[LUMA_12x16].prim = fncdef PFX(fname ## _12x16_ ## cpu); \
    p.pu[LUMA_16x4].prim  = fncdef PFX(fname ## _16x4_ ## cpu); \
    p.pu[LUMA_4x16].prim  = fncdef PFX(fname ## _4x16_ ## cpu); \
    p.pu[LUMA_32x24].prim = fncdef PFX(fname ## _32x24_ ## cpu); \
    p.pu[LUMA_24x32].prim = fncdef PFX(fname ## _24x32_ ## cpu); \
    p.pu[LUMA_32x8].prim  = fncdef PFX(fname ## _32x8_ ## cpu); \
    p.pu[LUMA_8x32].prim  = fncdef PFX(fname ## _8x32_ ## cpu); \
    p.pu[LUMA_64x48].prim = fncdef PFX(fname ## _64x48_ ## cpu); \
    p.pu[LUMA_48x64].prim = fncdef PFX(fname ## _48x64_ ## cpu); \
    p.pu[LUMA_64x16].prim = fncdef PFX(fname ## _64x16_ ## cpu); \
    p.pu[LUMA_16x64].prim = fncdef PFX(fname ## _16x64_ ## cpu)
#define ALL_LUMA_PU(prim, fname, cpu) ALL_LUMA_PU_TYPED(prim, , fname, cpu)

#define ALL_LUMA_PU_T(prim, fname) \
    p.pu[LUMA_8x8].prim   = fname<LUMA_8x8>; \
    p.pu[LUMA_16x16].prim = fname<LUMA_16x16>; \
    p.pu[LUMA_32x32].prim = fname<LUMA_32x32>; \
    p.pu[LUMA_64x64].prim = fname<LUMA_64x64>; \
    p.pu[LUMA_8x4].prim   = fname<LUMA_8x4>; \
    p.pu[LUMA_4x8].prim   = fname<LUMA_4x8>; \
    p.pu[LUMA_16x8].prim  = fname<LUMA_16x8>; \
    p.pu[LUMA_8x16].prim  = fname<LUMA_8x16>; \
    p.pu[LUMA_16x32].prim = fname<LUMA_16x32>; \
    p.pu[LUMA_32x16].prim = fname<LUMA_32x16>; \
    p.pu[LUMA_64x32].prim = fname<LUMA_64x32>; \
    p.pu[LUMA_32x64].prim = fname<LUMA_32x64>; \
    p.pu[LUMA_16x12].prim = fname<LUMA_16x12>; \
    p.pu[LUMA_12x16].prim = fname<LUMA_12x16>; \
    p.pu[LUMA_16x4].prim  = fname<LUMA_16x4>; \
    p.pu[LUMA_4x16].prim  = fname<LUMA_4x16>; \
    p.pu[LUMA_32x24].prim = fname<LUMA_32x24>; \
    p.pu[LUMA_24x32].prim = fname<LUMA_24x32>; \
    p.pu[LUMA_32x8].prim  = fname<LUMA_32x8>; \
    p.pu[LUMA_8x32].prim  = fname<LUMA_8x32>; \
    p.pu[LUMA_64x48].prim = fname<LUMA_64x48>; \
    p.pu[LUMA_48x64].prim = fname<LUMA_48x64>; \
    p.pu[LUMA_64x16].prim = fname<LUMA_64x16>; \
    p.pu[LUMA_16x64].prim = fname<LUMA_16x64>

#define ALL_CHROMA_420_CU_TYPED(prim, fncdef, fname, cpu) \
    p.chroma[X265_CSP_I420].cu[BLOCK_420_4x4].prim   = fncdef PFX(fname ## _4x4_ ## cpu); \
    p.chroma[X265_CSP_I420].cu[BLOCK_420_8x8].prim   = fncdef PFX(fname ## _8x8_ ## cpu); \
    p.chroma[X265_CSP_I420].cu[BLOCK_420_16x16].prim = fncdef PFX(fname ## _16x16_ ## cpu); \
    p.chroma[X265_CSP_I420].cu[BLOCK_420_32x32].prim = fncdef PFX(fname ## _32x32_ ## cpu)
#define ALL_CHROMA_420_CU_TYPED_S(prim, fncdef, fname, cpu) \
    p.chroma[X265_CSP_I420].cu[BLOCK_420_4x4].prim   = fncdef PFX(fname ## _4_ ## cpu); \
    p.chroma[X265_CSP_I420].cu[BLOCK_420_8x8].prim   = fncdef PFX(fname ## _8_ ## cpu); \
    p.chroma[X265_CSP_I420].cu[BLOCK_420_16x16].prim = fncdef PFX(fname ## _16_ ## cpu); \
    p.chroma[X265_CSP_I420].cu[BLOCK_420_32x32].prim = fncdef PFX(fname ## _32_ ## cpu)
#define ALL_CHROMA_420_CU(prim, fname, cpu) ALL_CHROMA_420_CU_TYPED(prim, , fname, cpu)
#define ALL_CHROMA_420_CU_S(prim, fname, cpu) ALL_CHROMA_420_CU_TYPED_S(prim, , fname, cpu)

#define ALL_CHROMA_420_PU_TYPED(prim, fncdef, fname, cpu) \
    p.chroma[X265_CSP_I420].pu[CHROMA_420_4x4].prim   = fncdef PFX(fname ## _4x4_ ## cpu); \
    p.chroma[X265_CSP_I420].pu[CHROMA_420_8x8].prim   = fncdef PFX(fname ## _8x8_ ## cpu); \
    p.chroma[X265_CSP_I420].pu[CHROMA_420_16x16].prim = fncdef PFX(fname ## _16x16_ ## cpu); \
    p.chroma[X265_CSP_I420].pu[CHROMA_420_32x32].prim = fncdef PFX(fname ## _32x32_ ## cpu); \
    p.chroma[X265_CSP_I420].pu[CHROMA_420_4x2].prim   = fncdef PFX(fname ## _4x2_ ## cpu); \
    p.chroma[X265_CSP_I420].pu[CHROMA_420_2x4].prim   = fncdef PFX(fname ## _2x4_ ## cpu); \
    p.chroma[X265_CSP_I420].pu[CHROMA_420_8x4].prim   = fncdef PFX(fname ## _8x4_ ## cpu); \
    p.chroma[X265_CSP_I420].pu[CHROMA_420_4x8].prim   = fncdef PFX(fname ## _4x8_ ## cpu); \
    p.chroma[X265_CSP_I420].pu[CHROMA_420_16x8].prim  = fncdef PFX(fname ## _16x8_ ## cpu); \
    p.chroma[X265_CSP_I420].pu[CHROMA_420_8x16].prim  = fncdef PFX(fname ## _8x16_ ## cpu); \
    p.chroma[X265_CSP_I420].pu[CHROMA_420_32x16].prim = fncdef PFX(fname ## _32x16_ ## cpu); \
    p.chroma[X265_CSP_I420].pu[CHROMA_420_16x32].prim = fncdef PFX(fname ## _16x32_ ## cpu); \
    p.chroma[X265_CSP_I420].pu[CHROMA_420_8x6].prim   = fncdef PFX(fname ## _8x6_ ## cpu); \
    p.chroma[X265_CSP_I420].pu[CHROMA_420_6x8].prim   = fncdef PFX(fname ## _6x8_ ## cpu); \
    p.chroma[X265_CSP_I420].pu[CHROMA_420_8x2].prim   = fncdef PFX(fname ## _8x2_ ## cpu); \
    p.chroma[X265_CSP_I420].pu[CHROMA_420_2x8].prim   = fncdef PFX(fname ## _2x8_ ## cpu); \
    p.chroma[X265_CSP_I420].pu[CHROMA_420_16x12].prim = fncdef PFX(fname ## _16x12_ ## cpu); \
    p.chroma[X265_CSP_I420].pu[CHROMA_420_12x16].prim = fncdef PFX(fname ## _12x16_ ## cpu); \
    p.chroma[X265_CSP_I420].pu[CHROMA_420_16x4].prim  = fncdef PFX(fname ## _16x4_ ## cpu); \
    p.chroma[X265_CSP_I420].pu[CHROMA_420_4x16].prim  = fncdef PFX(fname ## _4x16_ ## cpu); \
    p.chroma[X265_CSP_I420].pu[CHROMA_420_32x24].prim = fncdef PFX(fname ## _32x24_ ## cpu); \
    p.chroma[X265_CSP_I420].pu[CHROMA_420_24x32].prim = fncdef PFX(fname ## _24x32_ ## cpu); \
    p.chroma[X265_CSP_I420].pu[CHROMA_420_32x8].prim  = fncdef PFX(fname ## _32x8_ ## cpu); \
    p.chroma[X265_CSP_I420].pu[CHROMA_420_8x32].prim  = fncdef PFX(fname ## _8x32_ ## cpu)
#define ALL_CHROMA_420_PU(prim, fname, cpu) ALL_CHROMA_420_PU_TYPED(prim, , fname, cpu)

#define ALL_CHROMA_420_4x4_PU_TYPED(prim, fncdef, fname, cpu) \
    p.chroma[X265_CSP_I420].pu[CHROMA_420_4x4].prim   = fncdef PFX(fname ## _4x4_ ## cpu); \
    p.chroma[X265_CSP_I420].pu[CHROMA_420_8x8].prim   = fncdef PFX(fname ## _8x8_ ## cpu); \
    p.chroma[X265_CSP_I420].pu[CHROMA_420_16x16].prim = fncdef PFX(fname ## _16x16_ ## cpu); \
    p.chroma[X265_CSP_I420].pu[CHROMA_420_32x32].prim = fncdef PFX(fname ## _32x32_ ## cpu); \
    p.chroma[X265_CSP_I420].pu[CHROMA_420_8x4].prim   = fncdef PFX(fname ## _8x4_ ## cpu); \
    p.chroma[X265_CSP_I420].pu[CHROMA_420_4x8].prim   = fncdef PFX(fname ## _4x8_ ## cpu); \
    p.chroma[X265_CSP_I420].pu[CHROMA_420_16x8].prim  = fncdef PFX(fname ## _16x8_ ## cpu); \
    p.chroma[X265_CSP_I420].pu[CHROMA_420_8x16].prim  = fncdef PFX(fname ## _8x16_ ## cpu); \
    p.chroma[X265_CSP_I420].pu[CHROMA_420_32x16].prim = fncdef PFX(fname ## _32x16_ ## cpu); \
    p.chroma[X265_CSP_I420].pu[CHROMA_420_16x32].prim = fncdef PFX(fname ## _16x32_ ## cpu); \
    p.chroma[X265_CSP_I420].pu[CHROMA_420_16x12].prim = fncdef PFX(fname ## _16x12_ ## cpu); \
    p.chroma[X265_CSP_I420].pu[CHROMA_420_12x16].prim = fncdef PFX(fname ## _12x16_ ## cpu); \
    p.chroma[X265_CSP_I420].pu[CHROMA_420_16x4].prim  = fncdef PFX(fname ## _16x4_ ## cpu); \
    p.chroma[X265_CSP_I420].pu[CHROMA_420_4x16].prim  = fncdef PFX(fname ## _4x16_ ## cpu); \
    p.chroma[X265_CSP_I420].pu[CHROMA_420_32x24].prim = fncdef PFX(fname ## _32x24_ ## cpu); \
    p.chroma[X265_CSP_I420].pu[CHROMA_420_24x32].prim = fncdef PFX(fname ## _24x32_ ## cpu); \
    p.chroma[X265_CSP_I420].pu[CHROMA_420_32x8].prim  = fncdef PFX(fname ## _32x8_ ## cpu); \
    p.chroma[X265_CSP_I420].pu[CHROMA_420_8x32].prim  = fncdef PFX(fname ## _8x32_ ## cpu)
#define ALL_CHROMA_420_4x4_PU(prim, fname, cpu) ALL_CHROMA_420_4x4_PU_TYPED(prim, , fname, cpu)

#define ALL_CHROMA_422_CU_TYPED(prim, fncdef, fname, cpu) \
    p.chroma[X265_CSP_I422].cu[BLOCK_422_4x8].prim   = fncdef PFX(fname ## _4x8_ ## cpu); \
    p.chroma[X265_CSP_I422].cu[BLOCK_422_8x16].prim  = fncdef PFX(fname ## _8x16_ ## cpu); \
    p.chroma[X265_CSP_I422].cu[BLOCK_422_16x32].prim = fncdef PFX(fname ## _16x32_ ## cpu); \
    p.chroma[X265_CSP_I422].cu[BLOCK_422_32x64].prim = fncdef PFX(fname ## _32x64_ ## cpu)
#define ALL_CHROMA_422_CU(prim, fname, cpu) ALL_CHROMA_422_CU_TYPED(prim, , fname, cpu)

#define ALL_CHROMA_422_PU_TYPED(prim, fncdef, fname, cpu) \
    p.chroma[X265_CSP_I422].pu[CHROMA_422_4x8].prim   = fncdef PFX(fname ## _4x8_ ## cpu); \
    p.chroma[X265_CSP_I422].pu[CHROMA_422_8x16].prim  = fncdef PFX(fname ## _8x16_ ## cpu); \
    p.chroma[X265_CSP_I422].pu[CHROMA_422_16x32].prim = fncdef PFX(fname ## _16x32_ ## cpu); \
    p.chroma[X265_CSP_I422].pu[CHROMA_422_32x64].prim = fncdef PFX(fname ## _32x64_ ## cpu); \
    p.chroma[X265_CSP_I422].pu[CHROMA_422_4x4].prim   = fncdef PFX(fname ## _4x4_ ## cpu); \
    p.chroma[X265_CSP_I422].pu[CHROMA_422_2x8].prim   = fncdef PFX(fname ## _2x8_ ## cpu); \
    p.chroma[X265_CSP_I422].pu[CHROMA_422_8x8].prim   = fncdef PFX(fname ## _8x8_ ## cpu); \
    p.chroma[X265_CSP_I422].pu[CHROMA_422_4x16].prim  = fncdef PFX(fname ## _4x16_ ## cpu); \
    p.chroma[X265_CSP_I422].pu[CHROMA_422_16x16].prim = fncdef PFX(fname ## _16x16_ ## cpu); \
    p.chroma[X265_CSP_I422].pu[CHROMA_422_8x32].prim  = fncdef PFX(fname ## _8x32_ ## cpu); \
    p.chroma[X265_CSP_I422].pu[CHROMA_422_32x32].prim = fncdef PFX(fname ## _32x32_ ## cpu); \
    p.chroma[X265_CSP_I422].pu[CHROMA_422_16x64].prim = fncdef PFX(fname ## _16x64_ ## cpu); \
    p.chroma[X265_CSP_I422].pu[CHROMA_422_8x12].prim  = fncdef PFX(fname ## _8x12_ ## cpu); \
    p.chroma[X265_CSP_I422].pu[CHROMA_422_6x16].prim  = fncdef PFX(fname ## _6x16_ ## cpu); \
    p.chroma[X265_CSP_I422].pu[CHROMA_422_8x4].prim   = fncdef PFX(fname ## _8x4_ ## cpu); \
    p.chroma[X265_CSP_I422].pu[CHROMA_422_2x16].prim  = fncdef PFX(fname ## _2x16_ ## cpu); \
    p.chroma[X265_CSP_I422].pu[CHROMA_422_16x24].prim = fncdef PFX(fname ## _16x24_ ## cpu); \
    p.chroma[X265_CSP_I422].pu[CHROMA_422_12x32].prim = fncdef PFX(fname ## _12x32_ ## cpu); \
    p.chroma[X265_CSP_I422].pu[CHROMA_422_16x8].prim  = fncdef PFX(fname ## _16x8_ ## cpu); \
    p.chroma[X265_CSP_I422].pu[CHROMA_422_4x32].prim  = fncdef PFX(fname ## _4x32_ ## cpu); \
    p.chroma[X265_CSP_I422].pu[CHROMA_422_32x48].prim = fncdef PFX(fname ## _32x48_ ## cpu); \
    p.chroma[X265_CSP_I422].pu[CHROMA_422_24x64].prim = fncdef PFX(fname ## _24x64_ ## cpu); \
    p.chroma[X265_CSP_I422].pu[CHROMA_422_32x16].prim = fncdef PFX(fname ## _32x16_ ## cpu); \
    p.chroma[X265_CSP_I422].pu[CHROMA_422_8x64].prim  = fncdef PFX(fname ## _8x64_ ## cpu)
#define ALL_CHROMA_422_PU(prim, fname, cpu) ALL_CHROMA_422_PU_TYPED(prim, , fname, cpu)

#define ALL_CHROMA_444_PU_TYPED(prim, fncdef, fname, cpu) \
    p.chroma[X265_CSP_I444].pu[LUMA_4x4].prim   = fncdef PFX(fname ## _4x4_ ## cpu); \
    p.chroma[X265_CSP_I444].pu[LUMA_8x8].prim   = fncdef PFX(fname ## _8x8_ ## cpu); \
    p.chroma[X265_CSP_I444].pu[LUMA_16x16].prim = fncdef PFX(fname ## _16x16_ ## cpu); \
    p.chroma[X265_CSP_I444].pu[LUMA_32x32].prim = fncdef PFX(fname ## _32x32_ ## cpu); \
    p.chroma[X265_CSP_I444].pu[LUMA_64x64].prim = fncdef PFX(fname ## _64x64_ ## cpu); \
    p.chroma[X265_CSP_I444].pu[LUMA_8x4].prim   = fncdef PFX(fname ## _8x4_ ## cpu); \
    p.chroma[X265_CSP_I444].pu[LUMA_4x8].prim   = fncdef PFX(fname ## _4x8_ ## cpu); \
    p.chroma[X265_CSP_I444].pu[LUMA_16x8].prim  = fncdef PFX(fname ## _16x8_ ## cpu); \
    p.chroma[X265_CSP_I444].pu[LUMA_8x16].prim  = fncdef PFX(fname ## _8x16_ ## cpu); \
    p.chroma[X265_CSP_I444].pu[LUMA_16x32].prim = fncdef PFX(fname ## _16x32_ ## cpu); \
    p.chroma[X265_CSP_I444].pu[LUMA_32x16].prim = fncdef PFX(fname ## _32x16_ ## cpu); \
    p.chroma[X265_CSP_I444].pu[LUMA_64x32].prim = fncdef PFX(fname ## _64x32_ ## cpu); \
    p.chroma[X265_CSP_I444].pu[LUMA_32x64].prim = fncdef PFX(fname ## _32x64_ ## cpu); \
    p.chroma[X265_CSP_I444].pu[LUMA_16x12].prim = fncdef PFX(fname ## _16x12_ ## cpu); \
    p.chroma[X265_CSP_I444].pu[LUMA_12x16].prim = fncdef PFX(fname ## _12x16_ ## cpu); \
    p.chroma[X265_CSP_I444].pu[LUMA_16x4].prim  = fncdef PFX(fname ## _16x4_ ## cpu); \
    p.chroma[X265_CSP_I444].pu[LUMA_4x16].prim  = fncdef PFX(fname ## _4x16_ ## cpu); \
    p.chroma[X265_CSP_I444].pu[LUMA_32x24].prim = fncdef PFX(fname ## _32x24_ ## cpu); \
    p.chroma[X265_CSP_I444].pu[LUMA_24x32].prim = fncdef PFX(fname ## _24x32_ ## cpu); \
    p.chroma[X265_CSP_I444].pu[LUMA_32x8].prim  = fncdef PFX(fname ## _32x8_ ## cpu); \
    p.chroma[X265_CSP_I444].pu[LUMA_8x32].prim  = fncdef PFX(fname ## _8x32_ ## cpu); \
    p.chroma[X265_CSP_I444].pu[LUMA_64x48].prim = fncdef PFX(fname ## _64x48_ ## cpu); \
    p.chroma[X265_CSP_I444].pu[LUMA_48x64].prim = fncdef PFX(fname ## _48x64_ ## cpu); \
    p.chroma[X265_CSP_I444].pu[LUMA_64x16].prim = fncdef PFX(fname ## _64x16_ ## cpu); \
    p.chroma[X265_CSP_I444].pu[LUMA_16x64].prim = fncdef PFX(fname ## _16x64_ ## cpu)
#define ALL_CHROMA_444_PU(prim, fname, cpu) ALL_CHROMA_444_PU_TYPED(prim, , fname, cpu)

#define AVC_LUMA_PU(name, cpu) \
    p.pu[LUMA_16x16].name = PFX(pixel_ ## name ## _16x16_ ## cpu); \
    p.pu[LUMA_16x8].name  = PFX(pixel_ ## name ## _16x8_ ## cpu); \
    p.pu[LUMA_8x16].name  = PFX(pixel_ ## name ## _8x16_ ## cpu); \
    p.pu[LUMA_8x8].name   = PFX(pixel_ ## name ## _8x8_ ## cpu); \
    p.pu[LUMA_8x4].name   = PFX(pixel_ ## name ## _8x4_ ## cpu); \
    p.pu[LUMA_4x8].name   = PFX(pixel_ ## name ## _4x8_ ## cpu); \
    p.pu[LUMA_4x4].name   = PFX(pixel_ ## name ## _4x4_ ## cpu); \
    p.pu[LUMA_4x16].name  = PFX(pixel_ ## name ## _4x16_ ## cpu)

#define HEVC_SAD(cpu) \
    p.pu[LUMA_8x32].sad  = PFX(pixel_sad_8x32_ ## cpu); \
    p.pu[LUMA_16x4].sad  = PFX(pixel_sad_16x4_ ## cpu); \
    p.pu[LUMA_16x12].sad = PFX(pixel_sad_16x12_ ## cpu); \
    p.pu[LUMA_16x32].sad = PFX(pixel_sad_16x32_ ## cpu); \
    p.pu[LUMA_16x64].sad = PFX(pixel_sad_16x64_ ## cpu); \
    p.pu[LUMA_32x8].sad  = PFX(pixel_sad_32x8_ ## cpu); \
    p.pu[LUMA_32x16].sad = PFX(pixel_sad_32x16_ ## cpu); \
    p.pu[LUMA_32x24].sad = PFX(pixel_sad_32x24_ ## cpu); \
    p.pu[LUMA_32x32].sad = PFX(pixel_sad_32x32_ ## cpu); \
    p.pu[LUMA_32x64].sad = PFX(pixel_sad_32x64_ ## cpu); \
    p.pu[LUMA_64x16].sad = PFX(pixel_sad_64x16_ ## cpu); \
    p.pu[LUMA_64x32].sad = PFX(pixel_sad_64x32_ ## cpu); \
    p.pu[LUMA_64x48].sad = PFX(pixel_sad_64x48_ ## cpu); \
    p.pu[LUMA_64x64].sad = PFX(pixel_sad_64x64_ ## cpu); \
    p.pu[LUMA_48x64].sad = PFX(pixel_sad_48x64_ ## cpu); \
    p.pu[LUMA_24x32].sad = PFX(pixel_sad_24x32_ ## cpu); \
    p.pu[LUMA_12x16].sad = PFX(pixel_sad_12x16_ ## cpu)

#define HEVC_SAD_X3(cpu) \
    p.pu[LUMA_16x8].sad_x3  = PFX(pixel_sad_x3_16x8_ ## cpu); \
    p.pu[LUMA_16x12].sad_x3 = PFX(pixel_sad_x3_16x12_ ## cpu); \
    p.pu[LUMA_16x16].sad_x3 = PFX(pixel_sad_x3_16x16_ ## cpu); \
    p.pu[LUMA_16x32].sad_x3 = PFX(pixel_sad_x3_16x32_ ## cpu); \
    p.pu[LUMA_16x64].sad_x3 = PFX(pixel_sad_x3_16x64_ ## cpu); \
    p.pu[LUMA_32x8].sad_x3  = PFX(pixel_sad_x3_32x8_ ## cpu); \
    p.pu[LUMA_32x16].sad_x3 = PFX(pixel_sad_x3_32x16_ ## cpu); \
    p.pu[LUMA_32x24].sad_x3 = PFX(pixel_sad_x3_32x24_ ## cpu); \
    p.pu[LUMA_32x32].sad_x3 = PFX(pixel_sad_x3_32x32_ ## cpu); \
    p.pu[LUMA_32x64].sad_x3 = PFX(pixel_sad_x3_32x64_ ## cpu); \
    p.pu[LUMA_24x32].sad_x3 = PFX(pixel_sad_x3_24x32_ ## cpu); \
    p.pu[LUMA_48x64].sad_x3 = PFX(pixel_sad_x3_48x64_ ## cpu); \
    p.pu[LUMA_64x16].sad_x3 = PFX(pixel_sad_x3_64x16_ ## cpu); \
    p.pu[LUMA_64x32].sad_x3 = PFX(pixel_sad_x3_64x32_ ## cpu); \
    p.pu[LUMA_64x48].sad_x3 = PFX(pixel_sad_x3_64x48_ ## cpu); \
    p.pu[LUMA_64x64].sad_x3 = PFX(pixel_sad_x3_64x64_ ## cpu)

#define HEVC_SAD_X4(cpu) \
    p.pu[LUMA_16x8].sad_x4  = PFX(pixel_sad_x4_16x8_ ## cpu); \
    p.pu[LUMA_16x12].sad_x4 = PFX(pixel_sad_x4_16x12_ ## cpu); \
    p.pu[LUMA_16x16].sad_x4 = PFX(pixel_sad_x4_16x16_ ## cpu); \
    p.pu[LUMA_16x32].sad_x4 = PFX(pixel_sad_x4_16x32_ ## cpu); \
    p.pu[LUMA_16x64].sad_x4 = PFX(pixel_sad_x4_16x64_ ## cpu); \
    p.pu[LUMA_32x8].sad_x4  = PFX(pixel_sad_x4_32x8_ ## cpu); \
    p.pu[LUMA_32x16].sad_x4 = PFX(pixel_sad_x4_32x16_ ## cpu); \
    p.pu[LUMA_32x24].sad_x4 = PFX(pixel_sad_x4_32x24_ ## cpu); \
    p.pu[LUMA_32x32].sad_x4 = PFX(pixel_sad_x4_32x32_ ## cpu); \
    p.pu[LUMA_32x64].sad_x4 = PFX(pixel_sad_x4_32x64_ ## cpu); \
    p.pu[LUMA_24x32].sad_x4 = PFX(pixel_sad_x4_24x32_ ## cpu); \
    p.pu[LUMA_48x64].sad_x4 = PFX(pixel_sad_x4_48x64_ ## cpu); \
    p.pu[LUMA_64x16].sad_x4 = PFX(pixel_sad_x4_64x16_ ## cpu); \
    p.pu[LUMA_64x32].sad_x4 = PFX(pixel_sad_x4_64x32_ ## cpu); \
    p.pu[LUMA_64x48].sad_x4 = PFX(pixel_sad_x4_64x48_ ## cpu); \
    p.pu[LUMA_64x64].sad_x4 = PFX(pixel_sad_x4_64x64_ ## cpu)

#define ASSIGN_SSE_PP(cpu) \
    p.cu[BLOCK_8x8].sse_pp   = PFX(pixel_ssd_8x8_ ## cpu); \
    p.cu[BLOCK_16x16].sse_pp = PFX(pixel_ssd_16x16_ ## cpu); \
    p.cu[BLOCK_32x32].sse_pp = PFX(pixel_ssd_32x32_ ## cpu); \
    p.chroma[X265_CSP_I422].cu[BLOCK_422_8x16].sse_pp = PFX(pixel_ssd_8x16_ ## cpu); \
    p.chroma[X265_CSP_I422].cu[BLOCK_422_16x32].sse_pp = PFX(pixel_ssd_16x32_ ## cpu); \
    p.chroma[X265_CSP_I422].cu[BLOCK_422_32x64].sse_pp = PFX(pixel_ssd_32x64_ ## cpu);

#define ASSIGN_SSE_SS(cpu) ALL_LUMA_BLOCKS(sse_ss, pixel_ssd_ss, cpu)

#define ASSIGN_SA8D(cpu) \
    ALL_LUMA_CU(sa8d, pixel_sa8d, cpu); \
    p.chroma[X265_CSP_I422].cu[BLOCK_422_8x16].sa8d = PFX(pixel_sa8d_8x16_ ## cpu); \
    p.chroma[X265_CSP_I422].cu[BLOCK_422_16x32].sa8d = PFX(pixel_sa8d_16x32_ ## cpu); \
    p.chroma[X265_CSP_I422].cu[BLOCK_422_32x64].sa8d = PFX(pixel_sa8d_32x64_ ## cpu)
#define PIXEL_AVG(cpu) \
    p.pu[LUMA_64x64].pixelavg_pp[NONALIGNED] = PFX(pixel_avg_64x64_ ## cpu); \
    p.pu[LUMA_64x48].pixelavg_pp[NONALIGNED] = PFX(pixel_avg_64x48_ ## cpu); \
    p.pu[LUMA_64x32].pixelavg_pp[NONALIGNED] = PFX(pixel_avg_64x32_ ## cpu); \
    p.pu[LUMA_64x16].pixelavg_pp[NONALIGNED] = PFX(pixel_avg_64x16_ ## cpu); \
    p.pu[LUMA_48x64].pixelavg_pp[NONALIGNED] = PFX(pixel_avg_48x64_ ## cpu); \
    p.pu[LUMA_32x64].pixelavg_pp[NONALIGNED] = PFX(pixel_avg_32x64_ ## cpu); \
    p.pu[LUMA_32x32].pixelavg_pp[NONALIGNED] = PFX(pixel_avg_32x32_ ## cpu); \
    p.pu[LUMA_32x24].pixelavg_pp[NONALIGNED] = PFX(pixel_avg_32x24_ ## cpu); \
    p.pu[LUMA_32x16].pixelavg_pp[NONALIGNED] = PFX(pixel_avg_32x16_ ## cpu); \
    p.pu[LUMA_32x8].pixelavg_pp[NONALIGNED]  = PFX(pixel_avg_32x8_ ## cpu); \
    p.pu[LUMA_24x32].pixelavg_pp[NONALIGNED] = PFX(pixel_avg_24x32_ ## cpu); \
    p.pu[LUMA_16x64].pixelavg_pp[NONALIGNED] = PFX(pixel_avg_16x64_ ## cpu); \
    p.pu[LUMA_16x32].pixelavg_pp[NONALIGNED] = PFX(pixel_avg_16x32_ ## cpu); \
    p.pu[LUMA_16x16].pixelavg_pp[NONALIGNED] = PFX(pixel_avg_16x16_ ## cpu); \
    p.pu[LUMA_16x12].pixelavg_pp[NONALIGNED] = PFX(pixel_avg_16x12_ ## cpu); \
    p.pu[LUMA_16x8].pixelavg_pp[NONALIGNED]  = PFX(pixel_avg_16x8_ ## cpu); \
    p.pu[LUMA_16x4].pixelavg_pp[NONALIGNED]  = PFX(pixel_avg_16x4_ ## cpu); \
    p.pu[LUMA_12x16].pixelavg_pp[NONALIGNED] = PFX(pixel_avg_12x16_ ## cpu); \
    p.pu[LUMA_8x32].pixelavg_pp[NONALIGNED]  = PFX(pixel_avg_8x32_ ## cpu); \
    p.pu[LUMA_8x16].pixelavg_pp[NONALIGNED]  = PFX(pixel_avg_8x16_ ## cpu); \
    p.pu[LUMA_8x8].pixelavg_pp[NONALIGNED]   = PFX(pixel_avg_8x8_ ## cpu); \
    p.pu[LUMA_8x4].pixelavg_pp[NONALIGNED]   = PFX(pixel_avg_8x4_ ## cpu); \
    p.pu[LUMA_64x64].pixelavg_pp[ALIGNED] = PFX(pixel_avg_64x64_ ## cpu); \
    p.pu[LUMA_64x48].pixelavg_pp[ALIGNED] = PFX(pixel_avg_64x48_ ## cpu); \
    p.pu[LUMA_64x32].pixelavg_pp[ALIGNED] = PFX(pixel_avg_64x32_ ## cpu); \
    p.pu[LUMA_64x16].pixelavg_pp[ALIGNED] = PFX(pixel_avg_64x16_ ## cpu); \
    p.pu[LUMA_48x64].pixelavg_pp[ALIGNED] = PFX(pixel_avg_48x64_ ## cpu); \
    p.pu[LUMA_32x64].pixelavg_pp[ALIGNED] = PFX(pixel_avg_32x64_ ## cpu); \
    p.pu[LUMA_32x32].pixelavg_pp[ALIGNED] = PFX(pixel_avg_32x32_ ## cpu); \
    p.pu[LUMA_32x24].pixelavg_pp[ALIGNED] = PFX(pixel_avg_32x24_ ## cpu); \
    p.pu[LUMA_32x16].pixelavg_pp[ALIGNED] = PFX(pixel_avg_32x16_ ## cpu); \
    p.pu[LUMA_32x8].pixelavg_pp[ALIGNED]  = PFX(pixel_avg_32x8_ ## cpu); \
    p.pu[LUMA_24x32].pixelavg_pp[ALIGNED] = PFX(pixel_avg_24x32_ ## cpu); \
    p.pu[LUMA_16x64].pixelavg_pp[ALIGNED] = PFX(pixel_avg_16x64_ ## cpu); \
    p.pu[LUMA_16x32].pixelavg_pp[ALIGNED] = PFX(pixel_avg_16x32_ ## cpu); \
    p.pu[LUMA_16x16].pixelavg_pp[ALIGNED] = PFX(pixel_avg_16x16_ ## cpu); \
    p.pu[LUMA_16x12].pixelavg_pp[ALIGNED] = PFX(pixel_avg_16x12_ ## cpu); \
    p.pu[LUMA_16x8].pixelavg_pp[ALIGNED]  = PFX(pixel_avg_16x8_ ## cpu); \
    p.pu[LUMA_16x4].pixelavg_pp[ALIGNED]  = PFX(pixel_avg_16x4_ ## cpu); \
    p.pu[LUMA_12x16].pixelavg_pp[ALIGNED] = PFX(pixel_avg_12x16_ ## cpu); \
    p.pu[LUMA_8x32].pixelavg_pp[ALIGNED]  = PFX(pixel_avg_8x32_ ## cpu); \
    p.pu[LUMA_8x16].pixelavg_pp[ALIGNED]  = PFX(pixel_avg_8x16_ ## cpu); \
    p.pu[LUMA_8x8].pixelavg_pp[ALIGNED]   = PFX(pixel_avg_8x8_ ## cpu); \
    p.pu[LUMA_8x4].pixelavg_pp[ALIGNED]   = PFX(pixel_avg_8x4_ ## cpu);
#define PIXEL_AVG_W4(cpu) \
    p.pu[LUMA_4x4].pixelavg_pp[NONALIGNED]  = PFX(pixel_avg_4x4_ ## cpu); \
    p.pu[LUMA_4x8].pixelavg_pp[NONALIGNED]  = PFX(pixel_avg_4x8_ ## cpu); \
    p.pu[LUMA_4x16].pixelavg_pp[NONALIGNED] = PFX(pixel_avg_4x16_ ## cpu); \
    p.pu[LUMA_4x4].pixelavg_pp[ALIGNED]  = PFX(pixel_avg_4x4_ ## cpu); \
    p.pu[LUMA_4x8].pixelavg_pp[ALIGNED]  = PFX(pixel_avg_4x8_ ## cpu); \
    p.pu[LUMA_4x16].pixelavg_pp[ALIGNED] = PFX(pixel_avg_4x16_ ## cpu);
#define CHROMA_420_FILTERS(cpu) \
    ALL_CHROMA_420_PU(filter_hpp, interp_4tap_horiz_pp, cpu); \
    ALL_CHROMA_420_PU(filter_hps, interp_4tap_horiz_ps, cpu); \
    ALL_CHROMA_420_PU(filter_vpp, interp_4tap_vert_pp, cpu); \
    ALL_CHROMA_420_PU(filter_vps, interp_4tap_vert_ps, cpu);

#define CHROMA_422_FILTERS(cpu) \
    ALL_CHROMA_422_PU(filter_hpp, interp_4tap_horiz_pp, cpu); \
    ALL_CHROMA_422_PU(filter_hps, interp_4tap_horiz_ps, cpu); \
    ALL_CHROMA_422_PU(filter_vpp, interp_4tap_vert_pp, cpu); \
    ALL_CHROMA_422_PU(filter_vps, interp_4tap_vert_ps, cpu);

#define CHROMA_444_FILTERS(cpu) \
    ALL_CHROMA_444_PU(filter_hpp, interp_4tap_horiz_pp, cpu); \
    ALL_CHROMA_444_PU(filter_hps, interp_4tap_horiz_ps, cpu); \
    ALL_CHROMA_444_PU(filter_vpp, interp_4tap_vert_pp, cpu); \
    ALL_CHROMA_444_PU(filter_vps, interp_4tap_vert_ps, cpu);

#define SETUP_CHROMA_420_VSP_FUNC_DEF(W, H, cpu) \
    p.chroma[X265_CSP_I420].pu[CHROMA_420_ ## W ## x ## H].filter_vsp = PFX(interp_4tap_vert_sp_ ## W ## x ## H ## cpu);

#define CHROMA_420_VSP_FILTERS_SSE4(cpu) \
    SETUP_CHROMA_420_VSP_FUNC_DEF(4, 4, cpu); \
    SETUP_CHROMA_420_VSP_FUNC_DEF(4, 2, cpu); \
    SETUP_CHROMA_420_VSP_FUNC_DEF(2, 4, cpu); \
    SETUP_CHROMA_420_VSP_FUNC_DEF(4, 8, cpu); \
    SETUP_CHROMA_420_VSP_FUNC_DEF(6, 8, cpu); \
    SETUP_CHROMA_420_VSP_FUNC_DEF(2, 8, cpu); \
    SETUP_CHROMA_420_VSP_FUNC_DEF(16, 16, cpu); \
    SETUP_CHROMA_420_VSP_FUNC_DEF(16, 8, cpu); \
    SETUP_CHROMA_420_VSP_FUNC_DEF(16, 12, cpu); \
    SETUP_CHROMA_420_VSP_FUNC_DEF(12, 16, cpu); \
    SETUP_CHROMA_420_VSP_FUNC_DEF(16, 4, cpu); \
    SETUP_CHROMA_420_VSP_FUNC_DEF(4, 16, cpu); \
    SETUP_CHROMA_420_VSP_FUNC_DEF(32, 32, cpu); \
    SETUP_CHROMA_420_VSP_FUNC_DEF(32, 16, cpu); \
    SETUP_CHROMA_420_VSP_FUNC_DEF(16, 32, cpu); \
    SETUP_CHROMA_420_VSP_FUNC_DEF(32, 24, cpu); \
    SETUP_CHROMA_420_VSP_FUNC_DEF(24, 32, cpu); \
    SETUP_CHROMA_420_VSP_FUNC_DEF(32, 8, cpu);

#define CHROMA_420_VSP_FILTERS(cpu) \
    SETUP_CHROMA_420_VSP_FUNC_DEF(8, 2, cpu); \
    SETUP_CHROMA_420_VSP_FUNC_DEF(8, 4, cpu); \
    SETUP_CHROMA_420_VSP_FUNC_DEF(8, 6, cpu); \
    SETUP_CHROMA_420_VSP_FUNC_DEF(8, 8, cpu); \
    SETUP_CHROMA_420_VSP_FUNC_DEF(8, 16, cpu); \
    SETUP_CHROMA_420_VSP_FUNC_DEF(8, 32, cpu);

#define SETUP_CHROMA_422_VSP_FUNC_DEF(W, H, cpu) \
    p.chroma[X265_CSP_I422].pu[CHROMA_422_ ## W ## x ## H].filter_vsp = PFX(interp_4tap_vert_sp_ ## W ## x ## H ## cpu);

#define CHROMA_422_VSP_FILTERS_SSE4(cpu) \
    SETUP_CHROMA_422_VSP_FUNC_DEF(4, 8, cpu); \
    SETUP_CHROMA_422_VSP_FUNC_DEF(4, 4, cpu); \
    SETUP_CHROMA_422_VSP_FUNC_DEF(2, 8, cpu); \
    SETUP_CHROMA_422_VSP_FUNC_DEF(4, 16, cpu); \
    SETUP_CHROMA_422_VSP_FUNC_DEF(6, 16, cpu); \
    SETUP_CHROMA_422_VSP_FUNC_DEF(2, 16, cpu); \
    SETUP_CHROMA_422_VSP_FUNC_DEF(16, 32, cpu); \
    SETUP_CHROMA_422_VSP_FUNC_DEF(16, 16, cpu); \
    SETUP_CHROMA_422_VSP_FUNC_DEF(16, 24, cpu); \
    SETUP_CHROMA_422_VSP_FUNC_DEF(12, 32, cpu); \
    SETUP_CHROMA_422_VSP_FUNC_DEF(16, 8, cpu); \
    SETUP_CHROMA_422_VSP_FUNC_DEF(4, 32, cpu); \
    SETUP_CHROMA_422_VSP_FUNC_DEF(32, 64, cpu); \
    SETUP_CHROMA_422_VSP_FUNC_DEF(32, 32, cpu); \
    SETUP_CHROMA_422_VSP_FUNC_DEF(16, 64, cpu); \
    SETUP_CHROMA_422_VSP_FUNC_DEF(32, 48, cpu); \
    SETUP_CHROMA_422_VSP_FUNC_DEF(24, 64, cpu); \
    SETUP_CHROMA_422_VSP_FUNC_DEF(32, 16, cpu);

#define CHROMA_422_VSP_FILTERS(cpu) \
    SETUP_CHROMA_422_VSP_FUNC_DEF(8, 4, cpu); \
    SETUP_CHROMA_422_VSP_FUNC_DEF(8, 8, cpu); \
    SETUP_CHROMA_422_VSP_FUNC_DEF(8, 12, cpu); \
    SETUP_CHROMA_422_VSP_FUNC_DEF(8, 16, cpu); \
    SETUP_CHROMA_422_VSP_FUNC_DEF(8, 32, cpu); \
    SETUP_CHROMA_422_VSP_FUNC_DEF(8, 64, cpu);

#define SETUP_CHROMA_444_VSP_FUNC_DEF(W, H, cpu) \
    p.chroma[X265_CSP_I444].pu[LUMA_ ## W ## x ## H].filter_vsp = PFX(interp_4tap_vert_sp_ ## W ## x ## H ## cpu);

#define CHROMA_444_VSP_FILTERS_SSE4(cpu) \
    SETUP_CHROMA_444_VSP_FUNC_DEF(4, 4, cpu); \
    SETUP_CHROMA_444_VSP_FUNC_DEF(4, 8, cpu); \
    SETUP_CHROMA_444_VSP_FUNC_DEF(16, 16, cpu); \
    SETUP_CHROMA_444_VSP_FUNC_DEF(16, 8, cpu); \
    SETUP_CHROMA_444_VSP_FUNC_DEF(16, 12, cpu); \
    SETUP_CHROMA_444_VSP_FUNC_DEF(12, 16, cpu); \
    SETUP_CHROMA_444_VSP_FUNC_DEF(16, 4, cpu); \
    SETUP_CHROMA_444_VSP_FUNC_DEF(4, 16, cpu); \
    SETUP_CHROMA_444_VSP_FUNC_DEF(32, 32, cpu); \
    SETUP_CHROMA_444_VSP_FUNC_DEF(32, 16, cpu); \
    SETUP_CHROMA_444_VSP_FUNC_DEF(16, 32, cpu); \
    SETUP_CHROMA_444_VSP_FUNC_DEF(32, 24, cpu); \
    SETUP_CHROMA_444_VSP_FUNC_DEF(24, 32, cpu); \
    SETUP_CHROMA_444_VSP_FUNC_DEF(32, 8, cpu); \
    SETUP_CHROMA_444_VSP_FUNC_DEF(64, 64, cpu); \
    SETUP_CHROMA_444_VSP_FUNC_DEF(64, 32, cpu); \
    SETUP_CHROMA_444_VSP_FUNC_DEF(32, 64, cpu); \
    SETUP_CHROMA_444_VSP_FUNC_DEF(64, 48, cpu); \
    SETUP_CHROMA_444_VSP_FUNC_DEF(48, 64, cpu); \
    SETUP_CHROMA_444_VSP_FUNC_DEF(64, 16, cpu); \
    SETUP_CHROMA_444_VSP_FUNC_DEF(16, 64, cpu);

#define CHROMA_444_VSP_FILTERS(cpu) \
    SETUP_CHROMA_444_VSP_FUNC_DEF(8, 8, cpu); \
    SETUP_CHROMA_444_VSP_FUNC_DEF(8, 4, cpu); \
    SETUP_CHROMA_444_VSP_FUNC_DEF(8, 16, cpu); \
    SETUP_CHROMA_444_VSP_FUNC_DEF(8, 32, cpu);

#define SETUP_CHROMA_420_VSS_FUNC_DEF(W, H, cpu) \
    p.chroma[X265_CSP_I420].pu[CHROMA_420_ ## W ## x ## H].filter_vss = PFX(interp_4tap_vert_ss_ ## W ## x ## H ## cpu);

#define CHROMA_420_VSS_FILTERS(cpu) \
    SETUP_CHROMA_420_VSS_FUNC_DEF(4, 4, cpu); \
    SETUP_CHROMA_420_VSS_FUNC_DEF(4, 2, cpu); \
    SETUP_CHROMA_420_VSS_FUNC_DEF(8, 8, cpu); \
    SETUP_CHROMA_420_VSS_FUNC_DEF(8, 4, cpu); \
    SETUP_CHROMA_420_VSS_FUNC_DEF(4, 8, cpu); \
    SETUP_CHROMA_420_VSS_FUNC_DEF(8, 6, cpu); \
    SETUP_CHROMA_420_VSS_FUNC_DEF(8, 2, cpu); \
    SETUP_CHROMA_420_VSS_FUNC_DEF(16, 16, cpu); \
    SETUP_CHROMA_420_VSS_FUNC_DEF(16, 8, cpu); \
    SETUP_CHROMA_420_VSS_FUNC_DEF(8, 16, cpu); \
    SETUP_CHROMA_420_VSS_FUNC_DEF(16, 12, cpu); \
    SETUP_CHROMA_420_VSS_FUNC_DEF(12, 16, cpu); \
    SETUP_CHROMA_420_VSS_FUNC_DEF(16, 4, cpu); \
    SETUP_CHROMA_420_VSS_FUNC_DEF(4, 16, cpu); \
    SETUP_CHROMA_420_VSS_FUNC_DEF(32, 32, cpu); \
    SETUP_CHROMA_420_VSS_FUNC_DEF(32, 16, cpu); \
    SETUP_CHROMA_420_VSS_FUNC_DEF(16, 32, cpu); \
    SETUP_CHROMA_420_VSS_FUNC_DEF(32, 24, cpu); \
    SETUP_CHROMA_420_VSS_FUNC_DEF(24, 32, cpu); \
    SETUP_CHROMA_420_VSS_FUNC_DEF(32, 8, cpu); \
    SETUP_CHROMA_420_VSS_FUNC_DEF(8, 32, cpu);

#define CHROMA_420_VSS_FILTERS_SSE4(cpu) \
    SETUP_CHROMA_420_VSS_FUNC_DEF(2, 4, cpu); \
    SETUP_CHROMA_420_VSS_FUNC_DEF(2, 8, cpu); \
    SETUP_CHROMA_420_VSS_FUNC_DEF(6, 8, cpu);

#define SETUP_CHROMA_422_VSS_FUNC_DEF(W, H, cpu) \
    p.chroma[X265_CSP_I422].pu[CHROMA_422_ ## W ## x ## H].filter_vss = PFX(interp_4tap_vert_ss_ ## W ## x ## H ## cpu);

#define CHROMA_422_VSS_FILTERS(cpu) \
    SETUP_CHROMA_422_VSS_FUNC_DEF(4, 8, cpu); \
    SETUP_CHROMA_422_VSS_FUNC_DEF(4, 4, cpu); \
    SETUP_CHROMA_422_VSS_FUNC_DEF(8, 16, cpu); \
    SETUP_CHROMA_422_VSS_FUNC_DEF(8, 8, cpu); \
    SETUP_CHROMA_422_VSS_FUNC_DEF(4, 16, cpu); \
    SETUP_CHROMA_422_VSS_FUNC_DEF(8, 12, cpu); \
    SETUP_CHROMA_422_VSS_FUNC_DEF(8, 4, cpu); \
    SETUP_CHROMA_422_VSS_FUNC_DEF(16, 32, cpu); \
    SETUP_CHROMA_422_VSS_FUNC_DEF(16, 16, cpu); \
    SETUP_CHROMA_422_VSS_FUNC_DEF(8, 32, cpu); \
    SETUP_CHROMA_422_VSS_FUNC_DEF(16, 24, cpu); \
    SETUP_CHROMA_422_VSS_FUNC_DEF(12, 32, cpu); \
    SETUP_CHROMA_422_VSS_FUNC_DEF(16, 8, cpu); \
    SETUP_CHROMA_422_VSS_FUNC_DEF(4, 32, cpu); \
    SETUP_CHROMA_422_VSS_FUNC_DEF(32, 64, cpu); \
    SETUP_CHROMA_422_VSS_FUNC_DEF(32, 32, cpu); \
    SETUP_CHROMA_422_VSS_FUNC_DEF(16, 64, cpu); \
    SETUP_CHROMA_422_VSS_FUNC_DEF(32, 48, cpu); \
    SETUP_CHROMA_422_VSS_FUNC_DEF(24, 64, cpu); \
    SETUP_CHROMA_422_VSS_FUNC_DEF(32, 16, cpu); \
    SETUP_CHROMA_422_VSS_FUNC_DEF(8, 64, cpu);

#define CHROMA_422_VSS_FILTERS_SSE4(cpu) \
    SETUP_CHROMA_422_VSS_FUNC_DEF(2, 8, cpu); \
    SETUP_CHROMA_422_VSS_FUNC_DEF(2, 16, cpu); \
    SETUP_CHROMA_422_VSS_FUNC_DEF(6, 16, cpu);

#define CHROMA_444_VSS_FILTERS(cpu) ALL_CHROMA_444_PU(filter_vss, interp_4tap_vert_ss, cpu)

#define LUMA_FILTERS(cpu) \
    ALL_LUMA_PU(luma_hpp, interp_8tap_horiz_pp, cpu); p.pu[LUMA_4x4].luma_hpp = PFX(interp_8tap_horiz_pp_4x4_ ## cpu); \
    ALL_LUMA_PU(luma_hps, interp_8tap_horiz_ps, cpu); p.pu[LUMA_4x4].luma_hps = PFX(interp_8tap_horiz_ps_4x4_ ## cpu); \
    ALL_LUMA_PU(luma_vpp, interp_8tap_vert_pp, cpu); p.pu[LUMA_4x4].luma_vpp = PFX(interp_8tap_vert_pp_4x4_ ## cpu); \
    ALL_LUMA_PU(luma_vps, interp_8tap_vert_ps, cpu); p.pu[LUMA_4x4].luma_vps = PFX(interp_8tap_vert_ps_4x4_ ## cpu); \
    ALL_LUMA_PU(luma_vsp, interp_8tap_vert_sp, cpu); p.pu[LUMA_4x4].luma_vsp = PFX(interp_8tap_vert_sp_4x4_ ## cpu); \
    ALL_LUMA_PU_T(luma_hvpp, interp_8tap_hv_pp_cpu); p.pu[LUMA_4x4].luma_hvpp = interp_8tap_hv_pp_cpu<LUMA_4x4>;

#define LUMA_VSS_FILTERS(cpu) ALL_LUMA_PU(luma_vss, interp_8tap_vert_ss, cpu); p.pu[LUMA_4x4].luma_vss = PFX(interp_8tap_vert_ss_4x4_ ## cpu)

#define LUMA_CU_BLOCKCOPY(type, cpu) \
    p.cu[BLOCK_4x4].copy_ ## type = PFX(blockcopy_ ## type ## _4x4_ ## cpu); \
    ALL_LUMA_CU(copy_ ## type, blockcopy_ ## type, cpu);

#define CHROMA_420_CU_BLOCKCOPY(type, cpu) ALL_CHROMA_420_CU(copy_ ## type, blockcopy_ ## type, cpu)
#define CHROMA_422_CU_BLOCKCOPY(type, cpu) ALL_CHROMA_422_CU(copy_ ## type, blockcopy_ ## type, cpu)

#define LUMA_PU_BLOCKCOPY(type, cpu)       ALL_LUMA_PU(copy_ ## type, blockcopy_ ## type, cpu); p.pu[LUMA_4x4].copy_ ## type = PFX(blockcopy_ ## type ## _4x4_ ## cpu)
#define CHROMA_420_PU_BLOCKCOPY(type, cpu) ALL_CHROMA_420_PU(copy_ ## type, blockcopy_ ## type, cpu)
#define CHROMA_422_PU_BLOCKCOPY(type, cpu) ALL_CHROMA_422_PU(copy_ ## type, blockcopy_ ## type, cpu)

#define LUMA_PIXELSUB(cpu) \
    p.cu[BLOCK_4x4].sub_ps = PFX(pixel_sub_ps_4x4_ ## cpu); \
    p.cu[BLOCK_4x4].add_ps[NONALIGNED] = PFX(pixel_add_ps_4x4_ ## cpu); \
    p.cu[BLOCK_4x4].add_ps[ALIGNED] = PFX(pixel_add_ps_4x4_ ## cpu); \
    ALL_LUMA_CU(sub_ps, pixel_sub_ps, cpu); \
    ALL_LUMA_CU(add_ps[NONALIGNED], pixel_add_ps, cpu); \
    ALL_LUMA_CU(add_ps[ALIGNED], pixel_add_ps, cpu);

#define CHROMA_420_PIXELSUB_PS(cpu) \
    ALL_CHROMA_420_CU(sub_ps, pixel_sub_ps, cpu); \
    ALL_CHROMA_420_CU(add_ps[NONALIGNED], pixel_add_ps, cpu); \
    ALL_CHROMA_420_CU(add_ps[ALIGNED], pixel_add_ps, cpu);

#define CHROMA_422_PIXELSUB_PS(cpu) \
    ALL_CHROMA_422_CU(sub_ps, pixel_sub_ps, cpu); \
    ALL_CHROMA_422_CU(add_ps[NONALIGNED], pixel_add_ps, cpu); \
    ALL_CHROMA_422_CU(add_ps[ALIGNED], pixel_add_ps, cpu);

#define LUMA_VAR(cpu)          ALL_LUMA_CU(var, pixel_var, cpu)

#define LUMA_ADDAVG(cpu)       ALL_LUMA_PU(addAvg[NONALIGNED], addAvg, cpu); \
                               p.pu[LUMA_4x4].addAvg[NONALIGNED] = PFX(addAvg_4x4_ ## cpu); \
                               ALL_LUMA_PU(addAvg[ALIGNED], addAvg, cpu); \
                               p.pu[LUMA_4x4].addAvg[ALIGNED] = PFX(addAvg_4x4_ ## cpu)
#define CHROMA_420_ADDAVG(cpu) ALL_CHROMA_420_PU(addAvg[NONALIGNED], addAvg, cpu); \
                               ALL_CHROMA_420_PU(addAvg[ALIGNED], addAvg, cpu)
#define CHROMA_422_ADDAVG(cpu) ALL_CHROMA_422_PU(addAvg[NONALIGNED], addAvg, cpu); \
                               ALL_CHROMA_422_PU(addAvg[ALIGNED], addAvg, cpu)

#define SETUP_INTRA_ANG_COMMON(mode, fno, cpu) \
    p.cu[BLOCK_4x4].intra_pred[mode] = PFX(intra_pred_ang4_ ## fno ## _ ## cpu); \
    p.cu[BLOCK_8x8].intra_pred[mode] = PFX(intra_pred_ang8_ ## fno ## _ ## cpu); \
    p.cu[BLOCK_16x16].intra_pred[mode] = PFX(intra_pred_ang16_ ## fno ## _ ## cpu); \
    p.cu[BLOCK_32x32].intra_pred[mode] = PFX(intra_pred_ang32_ ## fno ## _ ## cpu);

#define SETUP_INTRA_ANG4(mode, fno, cpu) \
    p.cu[BLOCK_4x4].intra_pred[mode] = PFX(intra_pred_ang4_ ## fno ## _ ## cpu);

#define SETUP_INTRA_ANG16_32(mode, fno, cpu) \
    p.cu[BLOCK_16x16].intra_pred[mode] = PFX(intra_pred_ang16_ ## fno ## _ ## cpu); \
    p.cu[BLOCK_32x32].intra_pred[mode] = PFX(intra_pred_ang32_ ## fno ## _ ## cpu);

#define SETUP_INTRA_ANG4_8(mode, fno, cpu) \
    p.cu[BLOCK_4x4].intra_pred[mode] = PFX(intra_pred_ang4_ ## fno ## _ ## cpu); \
    p.cu[BLOCK_8x8].intra_pred[mode] = PFX(intra_pred_ang8_ ## fno ## _ ## cpu);

#define SETUP_INTRA_ANG_HIGH(mode, fno, cpu) \
    p.cu[BLOCK_8x8].intra_pred[mode] = PFX(intra_pred_ang8_ ## fno ## _ ## cpu); \
    p.cu[BLOCK_16x16].intra_pred[mode] = PFX(intra_pred_ang16_ ## fno ## _ ## cpu); \
    p.cu[BLOCK_32x32].intra_pred[mode] = PFX(intra_pred_ang32_ ## fno ## _ ## cpu);

#define CHROMA_420_VERT_FILTERS(cpu) \
    ALL_CHROMA_420_4x4_PU(filter_vss, interp_4tap_vert_ss, cpu); \
    ALL_CHROMA_420_4x4_PU(filter_vpp, interp_4tap_vert_pp, cpu); \
    ALL_CHROMA_420_4x4_PU(filter_vps, interp_4tap_vert_ps, cpu); \
    ALL_CHROMA_420_4x4_PU(filter_vsp, interp_4tap_vert_sp, cpu)

#define SETUP_CHROMA_420_VERT_FUNC_DEF(W, H, cpu) \
    p.chroma[X265_CSP_I420].pu[CHROMA_420_ ## W ## x ## H].filter_vss = PFX(interp_4tap_vert_ss_ ## W ## x ## H ## cpu); \
    p.chroma[X265_CSP_I420].pu[CHROMA_420_ ## W ## x ## H].filter_vpp = PFX(interp_4tap_vert_pp_ ## W ## x ## H ## cpu); \
    p.chroma[X265_CSP_I420].pu[CHROMA_420_ ## W ## x ## H].filter_vps = PFX(interp_4tap_vert_ps_ ## W ## x ## H ## cpu); \
    p.chroma[X265_CSP_I420].pu[CHROMA_420_ ## W ## x ## H].filter_vsp = PFX(interp_4tap_vert_sp_ ## W ## x ## H ## cpu);

#define CHROMA_420_VERT_FILTERS_SSE4(cpu) \
    SETUP_CHROMA_420_VERT_FUNC_DEF(2, 4, cpu); \
    SETUP_CHROMA_420_VERT_FUNC_DEF(2, 8, cpu); \
    SETUP_CHROMA_420_VERT_FUNC_DEF(4, 2, cpu); \
    SETUP_CHROMA_420_VERT_FUNC_DEF(6, 8, cpu);

#define SETUP_CHROMA_422_VERT_FUNC_DEF(W, H, cpu) \
    p.chroma[X265_CSP_I422].pu[CHROMA_422_ ## W ## x ## H].filter_vss = PFX(interp_4tap_vert_ss_ ## W ## x ## H ## cpu); \
    p.chroma[X265_CSP_I422].pu[CHROMA_422_ ## W ## x ## H].filter_vpp = PFX(interp_4tap_vert_pp_ ## W ## x ## H ## cpu); \
    p.chroma[X265_CSP_I422].pu[CHROMA_422_ ## W ## x ## H].filter_vps = PFX(interp_4tap_vert_ps_ ## W ## x ## H ## cpu); \
    p.chroma[X265_CSP_I422].pu[CHROMA_422_ ## W ## x ## H].filter_vsp = PFX(interp_4tap_vert_sp_ ## W ## x ## H ## cpu);

#define CHROMA_422_VERT_FILTERS(cpu) \
    SETUP_CHROMA_422_VERT_FUNC_DEF(4, 8, cpu); \
    SETUP_CHROMA_422_VERT_FUNC_DEF(8, 16, cpu); \
    SETUP_CHROMA_422_VERT_FUNC_DEF(8, 8, cpu); \
    SETUP_CHROMA_422_VERT_FUNC_DEF(4, 16, cpu); \
    SETUP_CHROMA_422_VERT_FUNC_DEF(8, 12, cpu); \
    SETUP_CHROMA_422_VERT_FUNC_DEF(8, 4, cpu); \
    SETUP_CHROMA_422_VERT_FUNC_DEF(16, 32, cpu); \
    SETUP_CHROMA_422_VERT_FUNC_DEF(16, 16, cpu); \
    SETUP_CHROMA_422_VERT_FUNC_DEF(8, 32, cpu); \
    SETUP_CHROMA_422_VERT_FUNC_DEF(16, 24, cpu); \
    SETUP_CHROMA_422_VERT_FUNC_DEF(12, 32, cpu); \
    SETUP_CHROMA_422_VERT_FUNC_DEF(16, 8, cpu); \
    SETUP_CHROMA_422_VERT_FUNC_DEF(4, 32, cpu); \
    SETUP_CHROMA_422_VERT_FUNC_DEF(32, 64, cpu); \
    SETUP_CHROMA_422_VERT_FUNC_DEF(32, 32, cpu); \
    SETUP_CHROMA_422_VERT_FUNC_DEF(16, 64, cpu); \
    SETUP_CHROMA_422_VERT_FUNC_DEF(32, 48, cpu); \
    SETUP_CHROMA_422_VERT_FUNC_DEF(24, 64, cpu); \
    SETUP_CHROMA_422_VERT_FUNC_DEF(32, 16, cpu); \
    SETUP_CHROMA_422_VERT_FUNC_DEF(8, 64, cpu);

#define CHROMA_422_VERT_FILTERS_SSE4(cpu) \
    SETUP_CHROMA_422_VERT_FUNC_DEF(2, 8, cpu); \
    SETUP_CHROMA_422_VERT_FUNC_DEF(2, 16, cpu); \
    SETUP_CHROMA_422_VERT_FUNC_DEF(4, 4, cpu); \
    SETUP_CHROMA_422_VERT_FUNC_DEF(6, 16, cpu);

#define CHROMA_444_VERT_FILTERS(cpu) \
    ALL_CHROMA_444_PU(filter_vss, interp_4tap_vert_ss, cpu); \
    ALL_CHROMA_444_PU(filter_vpp, interp_4tap_vert_pp, cpu); \
    ALL_CHROMA_444_PU(filter_vps, interp_4tap_vert_ps, cpu); \
    ALL_CHROMA_444_PU(filter_vsp, interp_4tap_vert_sp, cpu)

#define CHROMA_420_HORIZ_FILTERS(cpu) \
    ALL_CHROMA_420_PU(filter_hpp, interp_4tap_horiz_pp, cpu); \
    ALL_CHROMA_420_PU(filter_hps, interp_4tap_horiz_ps, cpu);

#define SETUP_CHROMA_422_HORIZ_FUNC_DEF(W, H, cpu) \
    p.chroma[X265_CSP_I422].pu[CHROMA_422_ ## W ## x ## H].filter_hpp = PFX(interp_4tap_horiz_pp_ ## W ## x ## H ## cpu); \
    p.chroma[X265_CSP_I422].pu[CHROMA_422_ ## W ## x ## H].filter_hps = PFX(interp_4tap_horiz_ps_ ## W ## x ## H ## cpu);

#define CHROMA_422_HORIZ_FILTERS(cpu) \
    SETUP_CHROMA_422_HORIZ_FUNC_DEF(4, 8, cpu); \
    SETUP_CHROMA_422_HORIZ_FUNC_DEF(4, 4, cpu); \
    SETUP_CHROMA_422_HORIZ_FUNC_DEF(2, 8, cpu); \
    SETUP_CHROMA_422_HORIZ_FUNC_DEF(8, 16, cpu); \
    SETUP_CHROMA_422_HORIZ_FUNC_DEF(8, 8, cpu); \
    SETUP_CHROMA_422_HORIZ_FUNC_DEF(4, 16, cpu); \
    SETUP_CHROMA_422_HORIZ_FUNC_DEF(8, 12, cpu); \
    SETUP_CHROMA_422_HORIZ_FUNC_DEF(6, 16, cpu); \
    SETUP_CHROMA_422_HORIZ_FUNC_DEF(8, 4, cpu); \
    SETUP_CHROMA_422_HORIZ_FUNC_DEF(2, 16, cpu); \
    SETUP_CHROMA_422_HORIZ_FUNC_DEF(16, 32, cpu); \
    SETUP_CHROMA_422_HORIZ_FUNC_DEF(16, 16, cpu); \
    SETUP_CHROMA_422_HORIZ_FUNC_DEF(8, 32, cpu); \
    SETUP_CHROMA_422_HORIZ_FUNC_DEF(16, 24, cpu); \
    SETUP_CHROMA_422_HORIZ_FUNC_DEF(12, 32, cpu); \
    SETUP_CHROMA_422_HORIZ_FUNC_DEF(16, 8, cpu); \
    SETUP_CHROMA_422_HORIZ_FUNC_DEF(4, 32, cpu); \
    SETUP_CHROMA_422_HORIZ_FUNC_DEF(32, 64, cpu); \
    SETUP_CHROMA_422_HORIZ_FUNC_DEF(32, 32, cpu); \
    SETUP_CHROMA_422_HORIZ_FUNC_DEF(16, 64, cpu); \
    SETUP_CHROMA_422_HORIZ_FUNC_DEF(32, 48, cpu); \
    SETUP_CHROMA_422_HORIZ_FUNC_DEF(24, 64, cpu); \
    SETUP_CHROMA_422_HORIZ_FUNC_DEF(32, 16, cpu); \
    SETUP_CHROMA_422_HORIZ_FUNC_DEF(8, 64, cpu);

#define CHROMA_444_HORIZ_FILTERS(cpu) \
    ALL_CHROMA_444_PU(filter_hpp, interp_4tap_horiz_pp, cpu); \
    ALL_CHROMA_444_PU(filter_hps, interp_4tap_horiz_ps, cpu);

#define ASSIGN2(func, fname) \
    func[ALIGNED] = PFX(fname); \
    func[NONALIGNED] = PFX(fname)

namespace X265_NS {
// private x265 namespace

#if HIGH_BIT_DEPTH

void setupAssemblyPrimitives(EncoderPrimitives &p, int cpuMask) // Main10
{

    if (cpuMask & X265_CPU_SSE2)
    {
        /* We do not differentiate CPUs which support MMX and not SSE2. We only check
         * for SSE2 and then use both MMX and SSE2 functions */
        AVC_LUMA_PU(sad, mmx2);

        p.pu[LUMA_16x16].sad = PFX(pixel_sad_16x16_sse2);
        p.pu[LUMA_16x8].sad  = PFX(pixel_sad_16x8_sse2);
        p.pu[LUMA_8x16].sad  = PFX(pixel_sad_8x16_sse2);
        HEVC_SAD(sse2);

        p.pu[LUMA_4x4].sad_x3   = PFX(pixel_sad_x3_4x4_mmx2);
        p.pu[LUMA_4x8].sad_x3   = PFX(pixel_sad_x3_4x8_mmx2);
        p.pu[LUMA_4x16].sad_x3  = PFX(pixel_sad_x3_4x16_mmx2);
        p.pu[LUMA_8x4].sad_x3   = PFX(pixel_sad_x3_8x4_sse2);
        p.pu[LUMA_8x8].sad_x3   = PFX(pixel_sad_x3_8x8_sse2);
        p.pu[LUMA_8x16].sad_x3  = PFX(pixel_sad_x3_8x16_sse2);
        p.pu[LUMA_8x32].sad_x3  = PFX(pixel_sad_x3_8x32_sse2);
        p.pu[LUMA_16x4].sad_x3  = PFX(pixel_sad_x3_16x4_sse2);
        p.pu[LUMA_12x16].sad_x3 = PFX(pixel_sad_x3_12x16_mmx2);
        HEVC_SAD_X3(sse2);

        p.pu[LUMA_4x4].sad_x4   = PFX(pixel_sad_x4_4x4_mmx2);
        p.pu[LUMA_4x8].sad_x4   = PFX(pixel_sad_x4_4x8_mmx2);
        p.pu[LUMA_4x16].sad_x4  = PFX(pixel_sad_x4_4x16_mmx2);
        p.pu[LUMA_8x4].sad_x4   = PFX(pixel_sad_x4_8x4_sse2);
        p.pu[LUMA_8x8].sad_x4   = PFX(pixel_sad_x4_8x8_sse2);
        p.pu[LUMA_8x16].sad_x4  = PFX(pixel_sad_x4_8x16_sse2);
        p.pu[LUMA_8x32].sad_x4  = PFX(pixel_sad_x4_8x32_sse2);
        p.pu[LUMA_16x4].sad_x4  = PFX(pixel_sad_x4_16x4_sse2);
        p.pu[LUMA_12x16].sad_x4 = PFX(pixel_sad_x4_12x16_mmx2);
        HEVC_SAD_X4(sse2);

        p.pu[LUMA_4x4].satd = p.cu[BLOCK_4x4].sa8d = PFX(pixel_satd_4x4_mmx2);
        ALL_LUMA_PU(satd, pixel_satd, sse2);

#if X265_DEPTH <= 10
        ASSIGN_SA8D(sse2);
#endif /* X265_DEPTH <= 10 */
        LUMA_PIXELSUB(sse2);
        CHROMA_420_PIXELSUB_PS(sse2);
        CHROMA_422_PIXELSUB_PS(sse2);

        LUMA_CU_BLOCKCOPY(ss, sse2);
        CHROMA_420_CU_BLOCKCOPY(ss, sse2);
        CHROMA_422_CU_BLOCKCOPY(ss, sse2);

        p.pu[LUMA_4x4].copy_pp = (copy_pp_t)PFX(blockcopy_ss_4x4_sse2);
        ALL_LUMA_PU_TYPED(copy_pp, (copy_pp_t), blockcopy_ss, sse2);
        ALL_CHROMA_420_PU_TYPED(copy_pp, (copy_pp_t), blockcopy_ss, sse2);
        ALL_CHROMA_422_PU_TYPED(copy_pp, (copy_pp_t), blockcopy_ss, sse2);

        ASSIGN2(p.pu[LUMA_64x64].pixelavg_pp, pixel_avg_64x64_sse2);
        ASSIGN2(p.pu[LUMA_64x48].pixelavg_pp, pixel_avg_64x48_sse2);
        ASSIGN2(p.pu[LUMA_64x32].pixelavg_pp, pixel_avg_64x32_sse2);
        ASSIGN2(p.pu[LUMA_64x16].pixelavg_pp, pixel_avg_64x16_sse2);
        ASSIGN2(p.pu[LUMA_48x64].pixelavg_pp, pixel_avg_48x64_sse2);
        ASSIGN2(p.pu[LUMA_32x64].pixelavg_pp, pixel_avg_32x64_sse2);
        ASSIGN2(p.pu[LUMA_32x32].pixelavg_pp, pixel_avg_32x32_sse2);
        ASSIGN2(p.pu[LUMA_32x24].pixelavg_pp, pixel_avg_32x24_sse2);
        ASSIGN2(p.pu[LUMA_32x16].pixelavg_pp, pixel_avg_32x16_sse2);
        ASSIGN2(p.pu[LUMA_32x8].pixelavg_pp, pixel_avg_32x8_sse2);
        ASSIGN2(p.pu[LUMA_24x32].pixelavg_pp, pixel_avg_24x32_sse2);
        ASSIGN2(p.pu[LUMA_16x64].pixelavg_pp, pixel_avg_16x64_sse2);
        ASSIGN2(p.pu[LUMA_16x32].pixelavg_pp, pixel_avg_16x32_sse2);
        ASSIGN2(p.pu[LUMA_16x16].pixelavg_pp, pixel_avg_16x16_sse2);
        ASSIGN2(p.pu[LUMA_16x12].pixelavg_pp, pixel_avg_16x12_sse2);
        ASSIGN2(p.pu[LUMA_16x8].pixelavg_pp, pixel_avg_16x8_sse2);
        ASSIGN2(p.pu[LUMA_16x4].pixelavg_pp, pixel_avg_16x4_sse2);
        ASSIGN2(p.pu[LUMA_12x16].pixelavg_pp, pixel_avg_12x16_sse2);
#if X86_64
        ASSIGN2(p.pu[LUMA_8x32].pixelavg_pp, pixel_avg_8x32_sse2);
        ASSIGN2(p.pu[LUMA_8x16].pixelavg_pp, pixel_avg_8x16_sse2);
        ASSIGN2(p.pu[LUMA_8x8].pixelavg_pp, pixel_avg_8x8_sse2);
        ASSIGN2(p.pu[LUMA_8x4].pixelavg_pp, pixel_avg_8x4_sse2);
#endif
        PIXEL_AVG_W4(mmx2);
        LUMA_VAR(sse2);


        ALL_LUMA_TU(blockfill_s[ALIGNED], blockfill_s, sse2);
        ALL_LUMA_TU(blockfill_s[NONALIGNED], blockfill_s, sse2);
        ALL_LUMA_TU_S(cpy1Dto2D_shr, cpy1Dto2D_shr_, sse2);
        ALL_LUMA_TU_S(cpy1Dto2D_shl[ALIGNED], cpy1Dto2D_shl_, sse2);
        ALL_LUMA_TU_S(cpy1Dto2D_shl[NONALIGNED], cpy1Dto2D_shl_, sse2);
        ALL_LUMA_TU_S(cpy2Dto1D_shr, cpy2Dto1D_shr_, sse2);
        ALL_LUMA_TU_S(cpy2Dto1D_shl, cpy2Dto1D_shl_, sse2);
#if X86_64
        ASSIGN2(p.cu[BLOCK_4x4].ssd_s,pixel_ssd_s_4_sse2 );
        ASSIGN2(p.cu[BLOCK_8x8].ssd_s,pixel_ssd_s_8_sse2);
        ASSIGN2(p.cu[BLOCK_16x16].ssd_s,pixel_ssd_s_16_sse2);
        ASSIGN2(p.cu[BLOCK_32x32].ssd_s,pixel_ssd_s_32_sse2 );
#endif

#if X86_64 && X265_DEPTH <= 10
        p.cu[BLOCK_4x4].sse_ss = PFX(pixel_ssd_ss_4x4_mmx2);
        p.chroma[X265_CSP_I422].cu[BLOCK_422_32x64].sse_pp = (pixel_sse_t)PFX(pixel_ssd_ss_32x64_sse2);
        p.chroma[X265_CSP_I422].cu[BLOCK_422_4x8].sse_pp = (pixel_sse_t)PFX(pixel_ssd_ss_4x8_mmx2);
        p.chroma[X265_CSP_I422].cu[BLOCK_422_8x16].sse_pp = (pixel_sse_t)PFX(pixel_ssd_ss_8x16_sse2);
        p.chroma[X265_CSP_I422].cu[BLOCK_422_16x32].sse_pp = (pixel_sse_t)PFX(pixel_ssd_ss_16x32_sse2);

        p.cu[BLOCK_8x8].sse_ss = PFX(pixel_ssd_ss_8x8_sse2);
        p.cu[BLOCK_16x16].sse_ss = PFX(pixel_ssd_ss_16x16_sse2);
        p.cu[BLOCK_32x32].sse_ss = PFX(pixel_ssd_ss_32x32_sse2);
        p.cu[BLOCK_64x64].sse_ss = PFX(pixel_ssd_ss_64x64_sse2);
#endif
        p.cu[BLOCK_4x4].dct = PFX(dct4_sse2);
        p.cu[BLOCK_8x8].dct = PFX(dct8_sse2);
        p.cu[BLOCK_4x4].idct = PFX(idct4_sse2);
#if X86_64
        p.cu[BLOCK_8x8].idct = PFX(idct8_sse2);
#endif
        p.idst4x4 = PFX(idst4_sse2);
        p.dst4x4 = PFX(dst4_sse2);

        p.frameInitLowres = PFX(frame_init_lowres_core_sse2);
        p.frameInitLowerRes = PFX(frame_init_lowres_core_sse2);
        p.frameSubSampleLuma = PFX(frame_subsample_luma_sse2);
        p.planecopy_sp_shl = PFX(upShift_16_sse2);
    }
    if (cpuMask & X265_CPU_SSSE3)
    {
        p.frameSubSampleLuma = PFX(frame_subsample_luma_ssse3);

        // p.pu[LUMA_4x4].satd = p.cu[BLOCK_4x4].sa8d = PFX(pixel_satd_4x4_ssse3); this one is broken
        ALL_LUMA_PU(satd, pixel_satd, ssse3);

#if X265_DEPTH <= 10
        ASSIGN_SA8D(ssse3);
#endif

        p.dst4x4 = PFX(dst4_ssse3);
        p.cu[BLOCK_8x8].idct = PFX(idct8_ssse3);

        p.frameInitLowres = PFX(frame_init_lowres_core_ssse3);
        p.frameInitLowerRes = PFX(frame_init_lowres_core_ssse3);
    }
    if (cpuMask & X265_CPU_SSE4)
    {
        LUMA_ADDAVG(sse4);
        CHROMA_420_ADDAVG(sse4);
        CHROMA_422_ADDAVG(sse4);

        p.cu[BLOCK_8x8].dct = PFX(dct8_sse4);

        // p.pu[LUMA_4x4].satd = p.cu[BLOCK_4x4].sa8d = PFX(pixel_satd_4x4_sse4); fails tests
        ALL_LUMA_PU(satd, pixel_satd, sse4);
#if X265_DEPTH <= 10
        ASSIGN_SA8D(sse4);
#endif
        p.planecopy_cp = PFX(upShift_8_sse4);
    }
#if X86_64
    if (cpuMask & X265_CPU_AVX)
    {
        // p.pu[LUMA_4x4].satd = p.cu[BLOCK_4x4].sa8d = PFX(pixel_satd_4x4_avx); fails tests
        p.chroma[X265_CSP_I422].pu[CHROMA_422_16x24].satd = PFX(pixel_satd_16x24_avx);
        p.chroma[X265_CSP_I422].pu[CHROMA_422_32x48].satd = PFX(pixel_satd_32x48_avx);
        p.chroma[X265_CSP_I422].pu[CHROMA_422_24x64].satd = PFX(pixel_satd_24x64_avx);
        p.chroma[X265_CSP_I422].pu[CHROMA_422_8x64].satd = PFX(pixel_satd_8x64_avx);
        p.chroma[X265_CSP_I422].pu[CHROMA_422_8x12].satd = PFX(pixel_satd_8x12_avx);
        p.chroma[X265_CSP_I422].pu[CHROMA_422_12x32].satd = PFX(pixel_satd_12x32_avx);
        p.chroma[X265_CSP_I422].pu[CHROMA_422_4x32].satd = PFX(pixel_satd_4x32_avx);
        p.chroma[X265_CSP_I422].pu[CHROMA_422_4x8].satd = PFX(pixel_satd_4x8_avx);
        p.chroma[X265_CSP_I422].pu[CHROMA_422_8x16].satd = PFX(pixel_satd_8x16_avx);
        // p.chroma[X265_CSP_I422].pu[CHROMA_422_4x4].satd = PFX(pixel_satd_4x4_avx);
        p.chroma[X265_CSP_I422].pu[CHROMA_422_8x8].satd = PFX(pixel_satd_8x8_avx);
        p.chroma[X265_CSP_I422].pu[CHROMA_422_4x16].satd = PFX(pixel_satd_4x16_avx);
        p.chroma[X265_CSP_I422].pu[CHROMA_422_8x32].satd = PFX(pixel_satd_8x32_avx);
        p.chroma[X265_CSP_I422].pu[CHROMA_422_8x4].satd = PFX(pixel_satd_8x4_avx);

        ALL_LUMA_PU(satd, pixel_satd, avx);
        p.chroma[X265_CSP_I420].pu[CHROMA_420_8x8].satd = PFX(pixel_satd_8x8_avx);
        p.chroma[X265_CSP_I420].pu[CHROMA_420_8x4].satd = PFX(pixel_satd_8x4_avx);
        p.chroma[X265_CSP_I420].pu[CHROMA_420_8x16].satd = PFX(pixel_satd_8x16_avx);
        p.chroma[X265_CSP_I420].pu[CHROMA_420_8x32].satd = PFX(pixel_satd_8x32_avx);
        p.chroma[X265_CSP_I420].pu[CHROMA_420_12x16].satd = PFX(pixel_satd_12x16_avx);
        p.chroma[X265_CSP_I420].pu[CHROMA_420_24x32].satd = PFX(pixel_satd_24x32_avx);
        p.chroma[X265_CSP_I420].pu[CHROMA_420_4x16].satd = PFX(pixel_satd_4x16_avx);
        p.chroma[X265_CSP_I420].pu[CHROMA_420_4x8].satd = PFX(pixel_satd_4x8_avx);
#if X265_DEPTH <= 10
        ASSIGN_SA8D(avx);
#endif
        p.chroma[X265_CSP_I420].cu[BLOCK_420_8x8].sa8d = PFX(pixel_sa8d_8x8_avx);
        p.chroma[X265_CSP_I420].cu[BLOCK_420_16x16].sa8d = PFX(pixel_sa8d_16x16_avx);
        p.chroma[X265_CSP_I420].cu[BLOCK_420_32x32].sa8d = PFX(pixel_sa8d_32x32_avx);
        LUMA_VAR(avx);

        // copy_pp primitives
        // 16 x N
        p.pu[LUMA_64x64].copy_pp = (copy_pp_t)PFX(blockcopy_ss_64x64_avx);
        p.pu[LUMA_16x4].copy_pp = (copy_pp_t)PFX(blockcopy_ss_16x4_avx);
        p.pu[LUMA_16x8].copy_pp = (copy_pp_t)PFX(blockcopy_ss_16x8_avx);
        p.pu[LUMA_16x12].copy_pp = (copy_pp_t)PFX(blockcopy_ss_16x12_avx);
        p.pu[LUMA_16x16].copy_pp = (copy_pp_t)PFX(blockcopy_ss_16x16_avx);
        p.pu[LUMA_16x32].copy_pp = (copy_pp_t)PFX(blockcopy_ss_16x32_avx);
        p.pu[LUMA_16x64].copy_pp = (copy_pp_t)PFX(blockcopy_ss_16x64_avx);
        p.pu[LUMA_64x16].copy_pp = (copy_pp_t)PFX(blockcopy_ss_64x16_avx);
        p.pu[LUMA_64x32].copy_pp = (copy_pp_t)PFX(blockcopy_ss_64x32_avx);
        p.pu[LUMA_64x48].copy_pp = (copy_pp_t)PFX(blockcopy_ss_64x48_avx);
        p.pu[LUMA_64x64].copy_pp = (copy_pp_t)PFX(blockcopy_ss_64x64_avx);
        p.chroma[X265_CSP_I420].pu[CHROMA_420_16x4].copy_pp = (copy_pp_t)PFX(blockcopy_ss_16x4_avx);
        p.chroma[X265_CSP_I420].pu[CHROMA_420_16x8].copy_pp = (copy_pp_t)PFX(blockcopy_ss_16x8_avx);
        p.chroma[X265_CSP_I420].pu[CHROMA_420_16x12].copy_pp = (copy_pp_t)PFX(blockcopy_ss_16x12_avx);
        p.chroma[X265_CSP_I420].pu[CHROMA_420_16x16].copy_pp = (copy_pp_t)PFX(blockcopy_ss_16x16_avx);
        p.chroma[X265_CSP_I420].pu[CHROMA_420_16x32].copy_pp = (copy_pp_t)PFX(blockcopy_ss_16x32_avx);
        p.chroma[X265_CSP_I422].pu[CHROMA_422_16x16].copy_pp = (copy_pp_t)PFX(blockcopy_ss_16x16_avx);
        p.chroma[X265_CSP_I422].pu[CHROMA_422_16x24].copy_pp = (copy_pp_t)PFX(blockcopy_ss_16x24_avx);
        p.chroma[X265_CSP_I422].pu[CHROMA_422_16x32].copy_pp = (copy_pp_t)PFX(blockcopy_ss_16x32_avx);
        p.chroma[X265_CSP_I422].pu[CHROMA_422_16x64].copy_pp = (copy_pp_t)PFX(blockcopy_ss_16x64_avx);
        p.chroma[X265_CSP_I422].pu[CHROMA_422_16x8].copy_pp = (copy_pp_t)PFX(blockcopy_ss_16x8_avx);

        // 24 X N
        p.pu[LUMA_24x32].copy_pp = (copy_pp_t)PFX(blockcopy_ss_24x32_avx);
        p.chroma[X265_CSP_I420].pu[CHROMA_420_24x32].copy_pp = (copy_pp_t)PFX(blockcopy_ss_24x32_avx);
        p.chroma[X265_CSP_I422].pu[CHROMA_422_24x64].copy_pp = (copy_pp_t)PFX(blockcopy_ss_24x64_avx);

        // 32 x N
        p.pu[LUMA_32x8].copy_pp = (copy_pp_t)PFX(blockcopy_ss_32x8_avx);
        p.pu[LUMA_32x16].copy_pp = (copy_pp_t)PFX(blockcopy_ss_32x16_avx);
        p.pu[LUMA_32x24].copy_pp = (copy_pp_t)PFX(blockcopy_ss_32x24_avx);
        p.pu[LUMA_32x32].copy_pp = (copy_pp_t)PFX(blockcopy_ss_32x32_avx);
        p.pu[LUMA_32x64].copy_pp = (copy_pp_t)PFX(blockcopy_ss_32x64_avx);
        p.chroma[X265_CSP_I420].pu[CHROMA_420_32x8].copy_pp = (copy_pp_t)PFX(blockcopy_ss_32x8_avx);
        p.chroma[X265_CSP_I420].pu[CHROMA_420_32x16].copy_pp = (copy_pp_t)PFX(blockcopy_ss_32x16_avx);
        p.chroma[X265_CSP_I420].pu[CHROMA_420_32x24].copy_pp = (copy_pp_t)PFX(blockcopy_ss_32x24_avx);
        p.chroma[X265_CSP_I420].pu[CHROMA_420_32x32].copy_pp = (copy_pp_t)PFX(blockcopy_ss_32x32_avx);
        p.chroma[X265_CSP_I422].pu[CHROMA_422_32x16].copy_pp = (copy_pp_t)PFX(blockcopy_ss_32x16_avx);
        p.chroma[X265_CSP_I422].pu[CHROMA_422_32x32].copy_pp = (copy_pp_t)PFX(blockcopy_ss_32x32_avx);
        p.chroma[X265_CSP_I422].pu[CHROMA_422_32x48].copy_pp = (copy_pp_t)PFX(blockcopy_ss_32x48_avx);
        p.chroma[X265_CSP_I422].pu[CHROMA_422_32x64].copy_pp = (copy_pp_t)PFX(blockcopy_ss_32x64_avx);

        // 48 X 64
        p.pu[LUMA_48x64].copy_pp = (copy_pp_t)PFX(blockcopy_ss_48x64_avx);

        // copy_ss primitives
        // 16 X N
        p.cu[BLOCK_16x16].copy_ss = PFX(blockcopy_ss_16x16_avx);
        p.chroma[X265_CSP_I420].cu[BLOCK_420_16x16].copy_ss = PFX(blockcopy_ss_16x16_avx);
        p.chroma[X265_CSP_I422].cu[BLOCK_422_16x32].copy_ss = PFX(blockcopy_ss_16x32_avx);

        // 32 X N
        p.cu[BLOCK_32x32].copy_ss = PFX(blockcopy_ss_32x32_avx);
        p.chroma[X265_CSP_I420].cu[BLOCK_420_32x32].copy_ss = PFX(blockcopy_ss_32x32_avx);
        p.chroma[X265_CSP_I422].cu[BLOCK_422_32x64].copy_ss = PFX(blockcopy_ss_32x64_avx);

        // 64 X N
        p.cu[BLOCK_64x64].copy_ss = PFX(blockcopy_ss_64x64_avx);

        // copy_ps primitives
        // 16 X N
        p.cu[BLOCK_16x16].copy_ps = (copy_ps_t)PFX(blockcopy_ss_16x16_avx);
        p.chroma[X265_CSP_I420].cu[BLOCK_420_16x16].copy_ps = (copy_ps_t)PFX(blockcopy_ss_16x16_avx);
        p.chroma[X265_CSP_I422].cu[BLOCK_422_16x32].copy_ps = (copy_ps_t)PFX(blockcopy_ss_16x32_avx);

        // 32 X N
        p.cu[BLOCK_32x32].copy_ps = (copy_ps_t)PFX(blockcopy_ss_32x32_avx);
        p.chroma[X265_CSP_I420].cu[BLOCK_420_32x32].copy_ps = (copy_ps_t)PFX(blockcopy_ss_32x32_avx);
        p.chroma[X265_CSP_I422].cu[BLOCK_422_32x64].copy_ps = (copy_ps_t)PFX(blockcopy_ss_32x64_avx);

        // 64 X N
        p.cu[BLOCK_64x64].copy_ps = (copy_ps_t)PFX(blockcopy_ss_64x64_avx);

        // copy_sp primitives
        // 16 X N
        p.cu[BLOCK_16x16].copy_sp = (copy_sp_t)PFX(blockcopy_ss_16x16_avx);
        p.chroma[X265_CSP_I420].cu[BLOCK_420_16x16].copy_sp = (copy_sp_t)PFX(blockcopy_ss_16x16_avx);
        p.chroma[X265_CSP_I422].cu[BLOCK_422_16x32].copy_sp = (copy_sp_t)PFX(blockcopy_ss_16x32_avx);

        // 32 X N
        p.cu[BLOCK_32x32].copy_sp = (copy_sp_t)PFX(blockcopy_ss_32x32_avx);
        p.chroma[X265_CSP_I420].cu[BLOCK_420_32x32].copy_sp = (copy_sp_t)PFX(blockcopy_ss_32x32_avx);
        p.chroma[X265_CSP_I422].cu[BLOCK_422_32x64].copy_sp = (copy_sp_t)PFX(blockcopy_ss_32x64_avx);

        // 64 X N
        p.cu[BLOCK_64x64].copy_sp = (copy_sp_t)PFX(blockcopy_ss_64x64_avx);

        p.frameInitLowres = PFX(frame_init_lowres_core_avx);
        p.frameInitLowerRes = PFX(frame_init_lowres_core_avx);

        p.pu[LUMA_64x16].copy_pp = (copy_pp_t)PFX(blockcopy_ss_64x16_avx);
        p.pu[LUMA_64x32].copy_pp = (copy_pp_t)PFX(blockcopy_ss_64x32_avx);
        p.pu[LUMA_64x48].copy_pp = (copy_pp_t)PFX(blockcopy_ss_64x48_avx);
        p.pu[LUMA_64x64].copy_pp = (copy_pp_t)PFX(blockcopy_ss_64x64_avx);
        p.frameSubSampleLuma = PFX(frame_subsample_luma_avx);
    }
    if (cpuMask & X265_CPU_XOP)
    {
        //p.pu[LUMA_4x4].satd = p.cu[BLOCK_4x4].sa8d = PFX(pixel_satd_4x4_xop); this one is broken
        ALL_LUMA_PU(satd, pixel_satd, xop);
#if X265_DEPTH <= 10
        ASSIGN_SA8D(xop);
#endif
        LUMA_VAR(xop);
        p.frameInitLowres = PFX(frame_init_lowres_core_xop);
        p.frameInitLowerRes = PFX(frame_init_lowres_core_xop);
        p.frameSubSampleLuma = PFX(frame_subsample_luma_xop);
    }
    if (cpuMask & X265_CPU_AVX2)
    {
#if X265_DEPTH == 12
        ASSIGN_SA8D(avx2);
#endif

        // TODO: the planecopy_sp is really planecopy_SC now, must be fix it
        //p.planecopy_sp = PFX(downShift_16_avx2);
        p.planecopy_sp_shl = PFX(upShift_16_avx2);

        ASSIGN2(p.pu[LUMA_12x16].pixelavg_pp, pixel_avg_12x16_avx2);
        ASSIGN2(p.pu[LUMA_16x4].pixelavg_pp, pixel_avg_16x4_avx2);
        ASSIGN2(p.pu[LUMA_16x8].pixelavg_pp, pixel_avg_16x8_avx2);
        ASSIGN2(p.pu[LUMA_16x12].pixelavg_pp, pixel_avg_16x12_avx2);
        ASSIGN2(p.pu[LUMA_16x16].pixelavg_pp, pixel_avg_16x16_avx2);
        ASSIGN2(p.pu[LUMA_16x32].pixelavg_pp, pixel_avg_16x32_avx2);
        ASSIGN2(p.pu[LUMA_16x64].pixelavg_pp, pixel_avg_16x64_avx2);
        ASSIGN2(p.pu[LUMA_24x32].pixelavg_pp, pixel_avg_24x32_avx2);
        ASSIGN2(p.pu[LUMA_32x8].pixelavg_pp, pixel_avg_32x8_avx2);
        ASSIGN2(p.pu[LUMA_32x16].pixelavg_pp, pixel_avg_32x16_avx2);
        ASSIGN2(p.pu[LUMA_32x24].pixelavg_pp, pixel_avg_32x24_avx2);
        ASSIGN2(p.pu[LUMA_32x32].pixelavg_pp, pixel_avg_32x32_avx2);
        ASSIGN2(p.pu[LUMA_32x64].pixelavg_pp, pixel_avg_32x64_avx2);
        ASSIGN2(p.pu[LUMA_64x16].pixelavg_pp, pixel_avg_64x16_avx2);
        ASSIGN2(p.pu[LUMA_64x32].pixelavg_pp, pixel_avg_64x32_avx2);
        ASSIGN2(p.pu[LUMA_64x48].pixelavg_pp, pixel_avg_64x48_avx2);
        ASSIGN2(p.pu[LUMA_64x64].pixelavg_pp, pixel_avg_64x64_avx2);
        ASSIGN2(p.pu[LUMA_48x64].pixelavg_pp, pixel_avg_48x64_avx2);
        ASSIGN2(p.pu[LUMA_8x4].addAvg, addAvg_8x4_avx2);
        ASSIGN2(p.pu[LUMA_8x8].addAvg, addAvg_8x8_avx2);
        ASSIGN2(p.pu[LUMA_8x16].addAvg, addAvg_8x16_avx2);
        ASSIGN2(p.pu[LUMA_8x32].addAvg, addAvg_8x32_avx2);
        ASSIGN2(p.pu[LUMA_12x16].addAvg, addAvg_12x16_avx2);
        ASSIGN2(p.pu[LUMA_16x4].addAvg, addAvg_16x4_avx2);
        ASSIGN2(p.pu[LUMA_16x8].addAvg, addAvg_16x8_avx2);
        ASSIGN2(p.pu[LUMA_16x12].addAvg, addAvg_16x12_avx2);
        ASSIGN2(p.pu[LUMA_16x16].addAvg, addAvg_16x16_avx2);
        ASSIGN2(p.pu[LUMA_16x32].addAvg, addAvg_16x32_avx2);
        ASSIGN2(p.pu[LUMA_16x64].addAvg, addAvg_16x64_avx2);
        ASSIGN2(p.pu[LUMA_24x32].addAvg, addAvg_24x32_avx2);
        ASSIGN2(p.pu[LUMA_32x8].addAvg, addAvg_32x8_avx2);
        ASSIGN2(p.pu[LUMA_32x16].addAvg, addAvg_32x16_avx2);
        ASSIGN2(p.pu[LUMA_32x24].addAvg, addAvg_32x24_avx2);
        ASSIGN2(p.pu[LUMA_32x32].addAvg, addAvg_32x32_avx2);
        ASSIGN2(p.pu[LUMA_32x64].addAvg, addAvg_32x64_avx2);
        ASSIGN2(p.pu[LUMA_48x64].addAvg, addAvg_48x64_avx2);
        ASSIGN2(p.pu[LUMA_64x16].addAvg, addAvg_64x16_avx2);
        ASSIGN2(p.pu[LUMA_64x32].addAvg, addAvg_64x32_avx2);
        ASSIGN2(p.pu[LUMA_64x48].addAvg, addAvg_64x48_avx2);
        ASSIGN2(p.pu[LUMA_64x64].addAvg, addAvg_64x64_avx2);

        ASSIGN2(p.chroma[X265_CSP_I420].pu[CHROMA_420_8x2].addAvg, addAvg_8x2_avx2);
        ASSIGN2(p.chroma[X265_CSP_I420].pu[CHROMA_420_8x4].addAvg, addAvg_8x4_avx2);
        ASSIGN2(p.chroma[X265_CSP_I420].pu[CHROMA_420_8x6].addAvg, addAvg_8x6_avx2);
        ASSIGN2(p.chroma[X265_CSP_I420].pu[CHROMA_420_8x8].addAvg, addAvg_8x8_avx2);
        ASSIGN2(p.chroma[X265_CSP_I420].pu[CHROMA_420_8x16].addAvg, addAvg_8x16_avx2);
        ASSIGN2(p.chroma[X265_CSP_I420].pu[CHROMA_420_8x32].addAvg, addAvg_8x32_avx2);
        ASSIGN2(p.chroma[X265_CSP_I420].pu[CHROMA_420_12x16].addAvg, addAvg_12x16_avx2);
        ASSIGN2(p.chroma[X265_CSP_I420].pu[CHROMA_420_16x4].addAvg, addAvg_16x4_avx2);
        ASSIGN2(p.chroma[X265_CSP_I420].pu[CHROMA_420_16x8].addAvg, addAvg_16x8_avx2);
        ASSIGN2(p.chroma[X265_CSP_I420].pu[CHROMA_420_16x12].addAvg, addAvg_16x12_avx2);
        ASSIGN2(p.chroma[X265_CSP_I420].pu[CHROMA_420_16x16].addAvg, addAvg_16x16_avx2);
        ASSIGN2(p.chroma[X265_CSP_I420].pu[CHROMA_420_16x32].addAvg, addAvg_16x32_avx2);
        ASSIGN2(p.chroma[X265_CSP_I420].pu[CHROMA_420_32x8].addAvg, addAvg_32x8_avx2);
        ASSIGN2(p.chroma[X265_CSP_I420].pu[CHROMA_420_32x16].addAvg, addAvg_32x16_avx2);
        ASSIGN2(p.chroma[X265_CSP_I420].pu[CHROMA_420_32x24].addAvg, addAvg_32x24_avx2);
        ASSIGN2(p.chroma[X265_CSP_I420].pu[CHROMA_420_32x32].addAvg, addAvg_32x32_avx2);

        ASSIGN2(p.chroma[X265_CSP_I422].pu[CHROMA_422_8x16].addAvg, addAvg_8x16_avx2);
        ASSIGN2(p.chroma[X265_CSP_I422].pu[CHROMA_422_16x32].addAvg, addAvg_16x32_avx2);
        ASSIGN2(p.chroma[X265_CSP_I422].pu[CHROMA_422_32x64].addAvg, addAvg_32x64_avx2);
        ASSIGN2(p.chroma[X265_CSP_I422].pu[CHROMA_422_8x8].addAvg, addAvg_8x8_avx2);
        ASSIGN2(p.chroma[X265_CSP_I422].pu[CHROMA_422_16x16].addAvg,addAvg_16x16_avx2);
        ASSIGN2(p.chroma[X265_CSP_I422].pu[CHROMA_422_8x32].addAvg, addAvg_8x32_avx2);
        ASSIGN2(p.chroma[X265_CSP_I422].pu[CHROMA_422_32x32].addAvg, addAvg_32x32_avx2);
        ASSIGN2(p.chroma[X265_CSP_I422].pu[CHROMA_422_16x64].addAvg, addAvg_16x64_avx2);
        ASSIGN2(p.chroma[X265_CSP_I422].pu[CHROMA_422_8x12].addAvg, addAvg_8x12_avx2);
        ASSIGN2(p.chroma[X265_CSP_I422].pu[CHROMA_422_8x4].addAvg, addAvg_8x4_avx2);
        ASSIGN2(p.chroma[X265_CSP_I422].pu[CHROMA_422_16x24].addAvg, addAvg_16x24_avx2);
        ASSIGN2(p.chroma[X265_CSP_I422].pu[CHROMA_422_16x8].addAvg, addAvg_16x8_avx2);
        ASSIGN2(p.chroma[X265_CSP_I422].pu[CHROMA_422_8x64].addAvg, addAvg_8x64_avx2);
        ASSIGN2(p.chroma[X265_CSP_I422].pu[CHROMA_422_24x64].addAvg, addAvg_24x64_avx2);
        ASSIGN2(p.chroma[X265_CSP_I422].pu[CHROMA_422_12x32].addAvg, addAvg_12x32_avx2);
        ASSIGN2(p.chroma[X265_CSP_I422].pu[CHROMA_422_32x16].addAvg, addAvg_32x16_avx2);
        ASSIGN2(p.chroma[X265_CSP_I422].pu[CHROMA_422_32x48].addAvg, addAvg_32x48_avx2);

        p.pu[LUMA_48x64].satd = PFX(pixel_satd_48x64_avx2);

        p.pu[LUMA_64x16].satd = PFX(pixel_satd_64x16_avx2);
        p.pu[LUMA_64x32].satd = PFX(pixel_satd_64x32_avx2);
        p.pu[LUMA_64x48].satd = PFX(pixel_satd_64x48_avx2);
        p.pu[LUMA_64x64].satd = PFX(pixel_satd_64x64_avx2);

        p.pu[LUMA_32x8].satd = PFX(pixel_satd_32x8_avx2);
        p.pu[LUMA_32x16].satd = PFX(pixel_satd_32x16_avx2);
        p.pu[LUMA_32x24].satd = PFX(pixel_satd_32x24_avx2);
        p.pu[LUMA_32x32].satd = PFX(pixel_satd_32x32_avx2);
        p.pu[LUMA_32x64].satd = PFX(pixel_satd_32x64_avx2);

        p.pu[LUMA_16x4].satd = PFX(pixel_satd_16x4_avx2);
        p.pu[LUMA_16x8].satd = PFX(pixel_satd_16x8_avx2);
        p.pu[LUMA_16x12].satd = PFX(pixel_satd_16x12_avx2);
        p.pu[LUMA_16x16].satd = PFX(pixel_satd_16x16_avx2);
        p.pu[LUMA_16x32].satd = PFX(pixel_satd_16x32_avx2);
        p.pu[LUMA_16x64].satd = PFX(pixel_satd_16x64_avx2);
        p.chroma[X265_CSP_I420].pu[CHROMA_420_16x16].satd = PFX(pixel_satd_16x16_avx2);
        p.chroma[X265_CSP_I420].pu[CHROMA_420_16x8].satd = PFX(pixel_satd_16x8_avx2);
        p.chroma[X265_CSP_I420].pu[CHROMA_420_16x32].satd = PFX(pixel_satd_16x32_avx2);
        p.chroma[X265_CSP_I420].pu[CHROMA_420_16x12].satd = PFX(pixel_satd_16x12_avx2);
        p.chroma[X265_CSP_I420].pu[CHROMA_420_16x4].satd = PFX(pixel_satd_16x4_avx2);
        p.chroma[X265_CSP_I420].pu[CHROMA_420_32x32].satd = PFX(pixel_satd_32x32_avx2);
        p.chroma[X265_CSP_I420].pu[CHROMA_420_32x16].satd = PFX(pixel_satd_32x16_avx2);
        p.chroma[X265_CSP_I420].pu[CHROMA_420_32x24].satd = PFX(pixel_satd_32x24_avx2);
        p.chroma[X265_CSP_I420].pu[CHROMA_420_32x8].satd = PFX(pixel_satd_32x8_avx2);
        p.chroma[X265_CSP_I422].pu[CHROMA_422_16x32].satd = PFX(pixel_satd_16x32_avx2);
        p.chroma[X265_CSP_I422].pu[CHROMA_422_32x64].satd = PFX(pixel_satd_32x64_avx2);
        p.chroma[X265_CSP_I422].pu[CHROMA_422_16x16].satd = PFX(pixel_satd_16x16_avx2);
        p.chroma[X265_CSP_I422].pu[CHROMA_422_32x32].satd = PFX(pixel_satd_32x32_avx2);
        p.chroma[X265_CSP_I422].pu[CHROMA_422_16x64].satd = PFX(pixel_satd_16x64_avx2);
        p.chroma[X265_CSP_I422].pu[CHROMA_422_16x8].satd = PFX(pixel_satd_16x8_avx2);
        p.chroma[X265_CSP_I422].pu[CHROMA_422_32x16].satd = PFX(pixel_satd_32x16_avx2);

        ASSIGN2( p.cu[BLOCK_16x16].ssd_s,pixel_ssd_s_16_avx2);
        ASSIGN2( p.cu[BLOCK_32x32].ssd_s,pixel_ssd_s_32_avx2);
        p.cu[BLOCK_16x16].sse_ss = (pixel_sse_ss_t)PFX(pixel_ssd_16x16_avx2);
        p.cu[BLOCK_32x32].sse_ss = (pixel_sse_ss_t)PFX(pixel_ssd_32x32_avx2);
        p.cu[BLOCK_64x64].sse_ss = (pixel_sse_ss_t)PFX(pixel_ssd_64x64_avx2);
        p.chroma[X265_CSP_I420].cu[BLOCK_420_16x16].sse_pp = (pixel_sse_t)PFX(pixel_ssd_16x16_avx2);
        p.chroma[X265_CSP_I420].cu[BLOCK_420_32x32].sse_pp = (pixel_sse_t)PFX(pixel_ssd_32x32_avx2);
        p.chroma[X265_CSP_I422].cu[BLOCK_422_16x32].sse_pp = (pixel_sse_t)PFX(pixel_ssd_ss_16x32_avx2);
        p.chroma[X265_CSP_I422].cu[BLOCK_422_32x64].sse_pp = (pixel_sse_t)PFX(pixel_ssd_ss_32x64_avx2);
        p.dst4x4 = PFX(dst4_avx2);
        p.idst4x4 = PFX(idst4_avx2);
        p.denoiseDct = PFX(denoise_dct_avx2);

        //p.sign = PFX(calSign_avx2);
        p.planecopy_cp = PFX(upShift_8_avx2);


        ASSIGN2(p.cu[BLOCK_16x16].blockfill_s, blockfill_s_16x16_avx2);
        ASSIGN2(p.cu[BLOCK_32x32].blockfill_s, blockfill_s_32x32_avx2);
        ALL_LUMA_TU_S(cpy1Dto2D_shl[ALIGNED], cpy1Dto2D_shl_, avx2);
        ALL_LUMA_TU_S(cpy1Dto2D_shl[NONALIGNED], cpy1Dto2D_shl_, avx2);
        ALL_LUMA_TU_S(cpy1Dto2D_shr, cpy1Dto2D_shr_, avx2);
        p.cu[BLOCK_8x8].copy_cnt = PFX(copy_cnt_8_avx2);
        p.cu[BLOCK_16x16].copy_cnt = PFX(copy_cnt_16_avx2);
        p.cu[BLOCK_32x32].copy_cnt = PFX(copy_cnt_32_avx2);

        p.cu[BLOCK_8x8].cpy2Dto1D_shl = PFX(cpy2Dto1D_shl_8_avx2);
        p.cu[BLOCK_16x16].cpy2Dto1D_shl = PFX(cpy2Dto1D_shl_16_avx2);
        p.cu[BLOCK_32x32].cpy2Dto1D_shl = PFX(cpy2Dto1D_shl_32_avx2);

        p.cu[BLOCK_8x8].cpy2Dto1D_shr = PFX(cpy2Dto1D_shr_8_avx2);
        p.cu[BLOCK_16x16].cpy2Dto1D_shr = PFX(cpy2Dto1D_shr_16_avx2);
        p.cu[BLOCK_32x32].cpy2Dto1D_shr = PFX(cpy2Dto1D_shr_32_avx2);

        ALL_LUMA_TU_S(idct, idct, avx2);
        ALL_LUMA_TU_S(dct, dct, avx2);

        ASSIGN2(p.cu[BLOCK_16x16].add_ps, pixel_add_ps_16x16_avx2);
        ASSIGN2(p.cu[BLOCK_32x32].add_ps, pixel_add_ps_32x32_avx2);
        ASSIGN2(p.cu[BLOCK_64x64].add_ps, pixel_add_ps_64x64_avx2);
        ASSIGN2(p.chroma[X265_CSP_I420].cu[BLOCK_420_16x16].add_ps, pixel_add_ps_16x16_avx2);
        ASSIGN2(p.chroma[X265_CSP_I420].cu[BLOCK_420_32x32].add_ps, pixel_add_ps_32x32_avx2);
        ASSIGN2(p.chroma[X265_CSP_I422].cu[BLOCK_422_16x32].add_ps, pixel_add_ps_16x32_avx2);
        ASSIGN2(p.chroma[X265_CSP_I422].cu[BLOCK_422_32x64].add_ps, pixel_add_ps_32x64_avx2);

        p.cu[BLOCK_16x16].sub_ps = PFX(pixel_sub_ps_16x16_avx2);
        p.cu[BLOCK_32x32].sub_ps = PFX(pixel_sub_ps_32x32_avx2);
        p.cu[BLOCK_64x64].sub_ps = PFX(pixel_sub_ps_64x64_avx2);
        p.chroma[X265_CSP_I420].cu[BLOCK_420_16x16].sub_ps = PFX(pixel_sub_ps_16x16_avx2);
        p.chroma[X265_CSP_I420].cu[BLOCK_420_32x32].sub_ps = PFX(pixel_sub_ps_32x32_avx2);
        p.chroma[X265_CSP_I422].cu[BLOCK_422_16x32].sub_ps = PFX(pixel_sub_ps_16x32_avx2);
        p.chroma[X265_CSP_I422].cu[BLOCK_422_32x64].sub_ps = PFX(pixel_sub_ps_32x64_avx2);

        p.pu[LUMA_16x4].sad = PFX(pixel_sad_16x4_avx2);
        p.pu[LUMA_16x8].sad = PFX(pixel_sad_16x8_avx2);
        p.pu[LUMA_16x12].sad = PFX(pixel_sad_16x12_avx2);
        p.pu[LUMA_16x16].sad = PFX(pixel_sad_16x16_avx2);
        p.pu[LUMA_16x32].sad = PFX(pixel_sad_16x32_avx2);
        p.pu[LUMA_16x64].sad = PFX(pixel_sad_16x64_avx2);
        p.pu[LUMA_32x8].sad = PFX(pixel_sad_32x8_avx2);
        p.pu[LUMA_32x16].sad = PFX(pixel_sad_32x16_avx2);
        p.pu[LUMA_32x24].sad = PFX(pixel_sad_32x24_avx2);
        p.pu[LUMA_32x32].sad = PFX(pixel_sad_32x32_avx2);
        p.pu[LUMA_32x64].sad = PFX(pixel_sad_32x64_avx2);
        p.pu[LUMA_48x64].sad = PFX(pixel_sad_48x64_avx2);
        p.pu[LUMA_64x16].sad = PFX(pixel_sad_64x16_avx2);
        p.pu[LUMA_64x32].sad = PFX(pixel_sad_64x32_avx2);
        p.pu[LUMA_64x48].sad = PFX(pixel_sad_64x48_avx2);
        p.pu[LUMA_64x64].sad = PFX(pixel_sad_64x64_avx2);

        p.pu[LUMA_16x4].sad_x3 = PFX(pixel_sad_x3_16x4_avx2);
        p.pu[LUMA_16x8].sad_x3 = PFX(pixel_sad_x3_16x8_avx2);
        p.pu[LUMA_16x12].sad_x3 = PFX(pixel_sad_x3_16x12_avx2);
        p.pu[LUMA_16x16].sad_x3 = PFX(pixel_sad_x3_16x16_avx2);
        p.pu[LUMA_16x32].sad_x3 = PFX(pixel_sad_x3_16x32_avx2);
        p.pu[LUMA_16x64].sad_x3 = PFX(pixel_sad_x3_16x64_avx2);
        p.pu[LUMA_32x8].sad_x3 = PFX(pixel_sad_x3_32x8_avx2);
        p.pu[LUMA_32x16].sad_x3 = PFX(pixel_sad_x3_32x16_avx2);
        p.pu[LUMA_32x24].sad_x3 = PFX(pixel_sad_x3_32x24_avx2);
        p.pu[LUMA_32x32].sad_x3 = PFX(pixel_sad_x3_32x32_avx2);
        p.pu[LUMA_32x64].sad_x3 = PFX(pixel_sad_x3_32x64_avx2);
        p.pu[LUMA_48x64].sad_x3 = PFX(pixel_sad_x3_48x64_avx2);
        p.pu[LUMA_64x16].sad_x3 = PFX(pixel_sad_x3_64x16_avx2);
        p.pu[LUMA_64x32].sad_x3 = PFX(pixel_sad_x3_64x32_avx2);
        p.pu[LUMA_64x48].sad_x3 = PFX(pixel_sad_x3_64x48_avx2);
        p.pu[LUMA_64x64].sad_x3 = PFX(pixel_sad_x3_64x64_avx2);

        p.pu[LUMA_16x4].sad_x4 = PFX(pixel_sad_x4_16x4_avx2);
        p.pu[LUMA_16x8].sad_x4 = PFX(pixel_sad_x4_16x8_avx2);
        p.pu[LUMA_16x12].sad_x4 = PFX(pixel_sad_x4_16x12_avx2);
        p.pu[LUMA_16x16].sad_x4 = PFX(pixel_sad_x4_16x16_avx2);
        p.pu[LUMA_16x32].sad_x4 = PFX(pixel_sad_x4_16x32_avx2);
        p.pu[LUMA_16x64].sad_x4 = PFX(pixel_sad_x4_16x64_avx2);
        p.pu[LUMA_32x8].sad_x4 = PFX(pixel_sad_x4_32x8_avx2);
        p.pu[LUMA_32x16].sad_x4 = PFX(pixel_sad_x4_32x16_avx2);
        p.pu[LUMA_32x24].sad_x4 = PFX(pixel_sad_x4_32x24_avx2);
        p.pu[LUMA_32x32].sad_x4 = PFX(pixel_sad_x4_32x32_avx2);
        p.pu[LUMA_32x64].sad_x4 = PFX(pixel_sad_x4_32x64_avx2);
        p.pu[LUMA_48x64].sad_x4 = PFX(pixel_sad_x4_48x64_avx2);
        p.pu[LUMA_64x16].sad_x4 = PFX(pixel_sad_x4_64x16_avx2);
        p.pu[LUMA_64x32].sad_x4 = PFX(pixel_sad_x4_64x32_avx2);
        p.pu[LUMA_64x48].sad_x4 = PFX(pixel_sad_x4_64x48_avx2);
        p.pu[LUMA_64x64].sad_x4 = PFX(pixel_sad_x4_64x64_avx2);

        p.frameInitLowres = PFX(frame_init_lowres_core_avx2);
        p.frameInitLowerRes = PFX(frame_init_lowres_core_avx2);
        p.frameSubSampleLuma = PFX(frame_subsample_luma_avx2);

#if X265_DEPTH == 10
        p.pu[LUMA_8x8].satd = PFX(pixel_satd_8x8_avx2);
        p.cu[LUMA_8x8].sa8d = PFX(pixel_sa8d_8x8_avx2);
        p.cu[LUMA_16x16].sa8d = PFX(pixel_sa8d_16x16_avx2);
        p.cu[LUMA_32x32].sa8d = PFX(pixel_sa8d_32x32_avx2);
#endif

    }
    if (cpuMask & X265_CPU_AVX512)
    {
        p.cu[BLOCK_16x16].var = PFX(pixel_var_16x16_avx512);
        p.cu[BLOCK_64x64].sub_ps = PFX(pixel_sub_ps_64x64_avx512);
        p.cu[BLOCK_32x32].sub_ps = PFX(pixel_sub_ps_32x32_avx512);
        p.chroma[X265_CSP_I420].cu[BLOCK_420_32x32].sub_ps = PFX(pixel_sub_ps_32x32_avx512);
        p.chroma[X265_CSP_I422].cu[BLOCK_422_32x64].sub_ps = PFX(pixel_sub_ps_32x64_avx512);

        p.cu[BLOCK_64x64].add_ps[NONALIGNED] = PFX(pixel_add_ps_64x64_avx512);
        p.cu[BLOCK_32x32].add_ps[NONALIGNED] = PFX(pixel_add_ps_32x32_avx512);
        p.chroma[X265_CSP_I420].cu[BLOCK_420_32x32].add_ps[NONALIGNED] = PFX(pixel_add_ps_32x32_avx512);
        p.chroma[X265_CSP_I422].cu[BLOCK_422_32x64].add_ps[NONALIGNED] = PFX(pixel_add_ps_32x64_avx512);

        p.cu[BLOCK_32x32].add_ps[ALIGNED] = PFX(pixel_add_ps_aligned_32x32_avx512);
        p.cu[BLOCK_64x64].add_ps[ALIGNED] = PFX(pixel_add_ps_aligned_64x64_avx512);
        p.chroma[X265_CSP_I420].cu[BLOCK_420_32x32].add_ps[ALIGNED] = PFX(pixel_add_ps_aligned_32x32_avx512);
        p.chroma[X265_CSP_I422].cu[BLOCK_422_32x64].add_ps[ALIGNED] = PFX(pixel_add_ps_aligned_32x64_avx512);

        // 64 X N
        p.cu[BLOCK_64x64].copy_ss = PFX(blockcopy_ss_64x64_avx512);
        p.pu[LUMA_64x64].copy_pp = (copy_pp_t)PFX(blockcopy_ss_64x64_avx512);
        p.pu[LUMA_64x48].copy_pp = (copy_pp_t)PFX(blockcopy_ss_64x48_avx512);
        p.pu[LUMA_64x32].copy_pp = (copy_pp_t)PFX(blockcopy_ss_64x32_avx512);
        p.pu[LUMA_64x16].copy_pp = (copy_pp_t)PFX(blockcopy_ss_64x16_avx512);
        p.cu[BLOCK_64x64].copy_ps = (copy_ps_t)PFX(blockcopy_ss_64x64_avx512);
        p.cu[BLOCK_64x64].copy_sp = (copy_sp_t)PFX(blockcopy_ss_64x64_avx512);

        // 32 X N
        p.cu[BLOCK_32x32].copy_ss = PFX(blockcopy_ss_32x32_avx512);
        p.pu[LUMA_32x64].copy_pp = (copy_pp_t)PFX(blockcopy_ss_32x64_avx512);
        p.pu[LUMA_32x32].copy_pp = (copy_pp_t)PFX(blockcopy_ss_32x32_avx512);
        p.pu[LUMA_32x24].copy_pp = (copy_pp_t)PFX(blockcopy_ss_32x24_avx512);
        p.pu[LUMA_32x16].copy_pp = (copy_pp_t)PFX(blockcopy_ss_32x16_avx512);
        p.pu[LUMA_32x8].copy_pp = (copy_pp_t)PFX(blockcopy_ss_32x8_avx512);
        p.chroma[X265_CSP_I420].pu[CHROMA_420_32x8].copy_pp = (copy_pp_t)PFX(blockcopy_ss_32x8_avx512);
        p.chroma[X265_CSP_I420].pu[CHROMA_420_32x16].copy_pp = (copy_pp_t)PFX(blockcopy_ss_32x16_avx512);
        p.chroma[X265_CSP_I420].pu[CHROMA_420_32x24].copy_pp = (copy_pp_t)PFX(blockcopy_ss_32x24_avx512);
        p.chroma[X265_CSP_I420].pu[CHROMA_420_32x32].copy_pp = (copy_pp_t)PFX(blockcopy_ss_32x32_avx512);
        p.chroma[X265_CSP_I422].pu[CHROMA_422_32x16].copy_pp = (copy_pp_t)PFX(blockcopy_ss_32x16_avx512);
        p.chroma[X265_CSP_I422].pu[CHROMA_422_32x32].copy_pp = (copy_pp_t)PFX(blockcopy_ss_32x32_avx512);
        p.chroma[X265_CSP_I422].pu[CHROMA_422_32x48].copy_pp = (copy_pp_t)PFX(blockcopy_ss_32x48_avx512);
        p.chroma[X265_CSP_I422].pu[CHROMA_422_32x64].copy_pp = (copy_pp_t)PFX(blockcopy_ss_32x64_avx512);
        p.chroma[X265_CSP_I420].cu[BLOCK_420_32x32].copy_ss = PFX(blockcopy_ss_32x32_avx512);
        p.chroma[X265_CSP_I422].cu[BLOCK_422_32x64].copy_ss = PFX(blockcopy_ss_32x64_avx512);
        p.cu[BLOCK_32x32].copy_ps = (copy_ps_t)PFX(blockcopy_ss_32x32_avx512);
        p.chroma[X265_CSP_I420].cu[BLOCK_420_32x32].copy_ps = (copy_ps_t)PFX(blockcopy_ss_32x32_avx512);
        p.chroma[X265_CSP_I422].cu[BLOCK_422_32x64].copy_ps = (copy_ps_t)PFX(blockcopy_ss_32x64_avx512);
        p.cu[BLOCK_32x32].copy_sp = (copy_sp_t)PFX(blockcopy_ss_32x32_avx512);
        p.chroma[X265_CSP_I420].cu[BLOCK_420_32x32].copy_sp = (copy_sp_t)PFX(blockcopy_ss_32x32_avx512);
        p.chroma[X265_CSP_I422].cu[BLOCK_422_32x64].copy_sp = (copy_sp_t)PFX(blockcopy_ss_32x64_avx512);

        p.cu[BLOCK_32x32].ssd_s[NONALIGNED] = PFX(pixel_ssd_s_32_avx512);
        p.cu[BLOCK_32x32].ssd_s[ALIGNED] = PFX(pixel_ssd_s_aligned_32_avx512);
        p.cu[BLOCK_16x16].ssd_s[NONALIGNED] = PFX(pixel_ssd_s_16_avx512);
        p.cu[BLOCK_16x16].ssd_s[ALIGNED] = PFX(pixel_ssd_s_aligned_16_avx512);
        p.pu[LUMA_16x32].sad = PFX(pixel_sad_16x32_avx512);
        p.pu[LUMA_16x64].sad = PFX(pixel_sad_16x64_avx512);
        p.pu[LUMA_32x8].sad = PFX(pixel_sad_32x8_avx512);
        p.pu[LUMA_32x16].sad = PFX(pixel_sad_32x16_avx512);
        p.pu[LUMA_32x24].sad = PFX(pixel_sad_32x24_avx512);
        p.pu[LUMA_32x32].sad = PFX(pixel_sad_32x32_avx512);
        p.pu[LUMA_32x64].sad = PFX(pixel_sad_32x64_avx512);
        p.pu[LUMA_48x64].sad = PFX(pixel_sad_48x64_avx512);
        p.pu[LUMA_64x16].sad = PFX(pixel_sad_64x16_avx512);
        p.pu[LUMA_64x32].sad = PFX(pixel_sad_64x32_avx512);
        p.pu[LUMA_64x48].sad = PFX(pixel_sad_64x48_avx512);
        p.pu[LUMA_64x64].sad = PFX(pixel_sad_64x64_avx512);

        p.pu[LUMA_64x16].addAvg[NONALIGNED] = PFX(addAvg_64x16_avx512);
        p.pu[LUMA_64x32].addAvg[NONALIGNED] = PFX(addAvg_64x32_avx512);
        p.pu[LUMA_64x48].addAvg[NONALIGNED] = PFX(addAvg_64x48_avx512);
        p.pu[LUMA_64x64].addAvg[NONALIGNED] = PFX(addAvg_64x64_avx512);
        p.pu[LUMA_32x8].addAvg[NONALIGNED] = PFX(addAvg_32x8_avx512);
        p.pu[LUMA_32x16].addAvg[NONALIGNED] = PFX(addAvg_32x16_avx512);
        p.pu[LUMA_32x24].addAvg[NONALIGNED] = PFX(addAvg_32x24_avx512);
        p.pu[LUMA_32x32].addAvg[NONALIGNED] = PFX(addAvg_32x32_avx512);
        p.pu[LUMA_32x64].addAvg[NONALIGNED] = PFX(addAvg_32x64_avx512);
        p.pu[LUMA_16x4].addAvg[NONALIGNED] = PFX(addAvg_16x4_avx512);
        p.pu[LUMA_16x8].addAvg[NONALIGNED] = PFX(addAvg_16x8_avx512);
        p.pu[LUMA_16x12].addAvg[NONALIGNED] = PFX(addAvg_16x12_avx512);
        p.pu[LUMA_16x16].addAvg[NONALIGNED] = PFX(addAvg_16x16_avx512);
        p.pu[LUMA_16x32].addAvg[NONALIGNED] = PFX(addAvg_16x32_avx512);
        p.pu[LUMA_16x64].addAvg[NONALIGNED] = PFX(addAvg_16x64_avx512);
        p.pu[LUMA_48x64].addAvg[NONALIGNED] = PFX(addAvg_48x64_avx512);

        p.chroma[X265_CSP_I420].pu[CHROMA_420_32x8].addAvg[NONALIGNED] = PFX(addAvg_32x8_avx512);
        p.chroma[X265_CSP_I420].pu[CHROMA_420_32x16].addAvg[NONALIGNED] = PFX(addAvg_32x16_avx512);
        p.chroma[X265_CSP_I420].pu[CHROMA_420_32x24].addAvg[NONALIGNED] = PFX(addAvg_32x24_avx512);
        p.chroma[X265_CSP_I420].pu[CHROMA_420_32x32].addAvg[NONALIGNED] = PFX(addAvg_32x32_avx512);
        p.chroma[X265_CSP_I420].pu[CHROMA_420_16x4].addAvg[NONALIGNED] = PFX(addAvg_16x4_avx512);
        p.chroma[X265_CSP_I420].pu[CHROMA_420_16x8].addAvg[NONALIGNED] = PFX(addAvg_16x8_avx512);
        p.chroma[X265_CSP_I420].pu[CHROMA_420_16x12].addAvg[NONALIGNED] = PFX(addAvg_16x12_avx512);
        p.chroma[X265_CSP_I420].pu[CHROMA_420_16x16].addAvg[NONALIGNED] = PFX(addAvg_16x16_avx512);
        p.chroma[X265_CSP_I420].pu[CHROMA_420_16x32].addAvg[NONALIGNED] = PFX(addAvg_16x32_avx512);

        p.chroma[X265_CSP_I422].pu[CHROMA_422_32x16].addAvg[NONALIGNED] = PFX(addAvg_32x16_avx512);
        p.chroma[X265_CSP_I422].pu[CHROMA_422_32x32].addAvg[NONALIGNED] = PFX(addAvg_32x32_avx512);
        p.chroma[X265_CSP_I422].pu[CHROMA_422_32x48].addAvg[NONALIGNED] = PFX(addAvg_32x48_avx512);
        p.chroma[X265_CSP_I422].pu[CHROMA_422_32x64].addAvg[NONALIGNED] = PFX(addAvg_32x64_avx512);
        p.chroma[X265_CSP_I422].pu[CHROMA_422_16x32].addAvg[NONALIGNED] = PFX(addAvg_16x32_avx512);
        p.chroma[X265_CSP_I422].pu[CHROMA_422_16x16].addAvg[NONALIGNED] = PFX(addAvg_16x16_avx512);
        p.chroma[X265_CSP_I422].pu[CHROMA_422_16x64].addAvg[NONALIGNED] = PFX(addAvg_16x64_avx512);
        p.chroma[X265_CSP_I422].pu[CHROMA_422_16x24].addAvg[NONALIGNED] = PFX(addAvg_16x24_avx512);
        p.chroma[X265_CSP_I422].pu[CHROMA_422_16x8].addAvg[NONALIGNED] = PFX(addAvg_16x8_avx512);
        p.pu[LUMA_32x8].pixelavg_pp[NONALIGNED] = PFX(pixel_avg_32x8_avx512);
        p.pu[LUMA_32x16].pixelavg_pp[NONALIGNED] = PFX(pixel_avg_32x16_avx512);
        p.pu[LUMA_32x24].pixelavg_pp[NONALIGNED] = PFX(pixel_avg_32x24_avx512);
        p.pu[LUMA_32x32].pixelavg_pp[NONALIGNED] = PFX(pixel_avg_32x32_avx512);
        p.pu[LUMA_32x64].pixelavg_pp[NONALIGNED] = PFX(pixel_avg_32x64_avx512);
        p.pu[LUMA_64x16].pixelavg_pp[NONALIGNED] = PFX(pixel_avg_64x16_avx512);
        p.pu[LUMA_64x32].pixelavg_pp[NONALIGNED] = PFX(pixel_avg_64x32_avx512);
        p.pu[LUMA_64x48].pixelavg_pp[NONALIGNED] = PFX(pixel_avg_64x48_avx512);
        p.pu[LUMA_64x64].pixelavg_pp[NONALIGNED] = PFX(pixel_avg_64x64_avx512);
        p.pu[LUMA_48x64].pixelavg_pp[NONALIGNED] = PFX(pixel_avg_48x64_avx512);

        p.pu[LUMA_32x8].pixelavg_pp[ALIGNED] = PFX(pixel_avg_aligned_32x8_avx512);
        p.pu[LUMA_32x16].pixelavg_pp[ALIGNED] = PFX(pixel_avg_aligned_32x16_avx512);
        p.pu[LUMA_32x24].pixelavg_pp[ALIGNED] = PFX(pixel_avg_aligned_32x24_avx512);
        p.pu[LUMA_32x32].pixelavg_pp[ALIGNED] = PFX(pixel_avg_aligned_32x32_avx512);
        p.pu[LUMA_32x64].pixelavg_pp[ALIGNED] = PFX(pixel_avg_aligned_32x64_avx512);
        //p.pu[LUMA_48x64].pixelavg_pp[ALIGNED] = PFX(pixel_avg_aligned_48x64_avx512);
        p.pu[LUMA_64x16].pixelavg_pp[ALIGNED] = PFX(pixel_avg_aligned_64x16_avx512);
        p.pu[LUMA_64x32].pixelavg_pp[ALIGNED] = PFX(pixel_avg_aligned_64x32_avx512);
        p.pu[LUMA_64x48].pixelavg_pp[ALIGNED] = PFX(pixel_avg_aligned_64x48_avx512);
        p.pu[LUMA_64x64].pixelavg_pp[ALIGNED] = PFX(pixel_avg_aligned_64x64_avx512);
        p.pu[LUMA_16x8].sad_x3 = PFX(pixel_sad_x3_16x8_avx512);
        p.pu[LUMA_16x12].sad_x3 = PFX(pixel_sad_x3_16x12_avx512);
        p.pu[LUMA_16x16].sad_x3 = PFX(pixel_sad_x3_16x16_avx512);
        p.pu[LUMA_16x32].sad_x3 = PFX(pixel_sad_x3_16x32_avx512);
        p.pu[LUMA_16x64].sad_x3 = PFX(pixel_sad_x3_16x64_avx512);
        p.pu[LUMA_32x8].sad_x3 = PFX(pixel_sad_x3_32x8_avx512);
        p.pu[LUMA_32x16].sad_x3 = PFX(pixel_sad_x3_32x16_avx512);
        p.pu[LUMA_32x24].sad_x3 = PFX(pixel_sad_x3_32x24_avx512);
        p.pu[LUMA_32x32].sad_x3 = PFX(pixel_sad_x3_32x32_avx512);
        p.pu[LUMA_32x64].sad_x3 = PFX(pixel_sad_x3_32x64_avx512);
        //p.pu[LUMA_48x64].sad_x3 = PFX(pixel_sad_x3_48x64_avx512);
        p.pu[LUMA_64x16].sad_x3 = PFX(pixel_sad_x3_64x16_avx512);
        p.pu[LUMA_64x32].sad_x3 = PFX(pixel_sad_x3_64x32_avx512);
        p.pu[LUMA_64x48].sad_x3 = PFX(pixel_sad_x3_64x48_avx512);
        p.pu[LUMA_64x64].sad_x3 = PFX(pixel_sad_x3_64x64_avx512);

        p.pu[LUMA_16x8].sad_x4 = PFX(pixel_sad_x4_16x8_avx512);
        p.pu[LUMA_16x12].sad_x4 = PFX(pixel_sad_x4_16x12_avx512);
        p.pu[LUMA_16x16].sad_x4 = PFX(pixel_sad_x4_16x16_avx512);
        p.pu[LUMA_16x32].sad_x4 = PFX(pixel_sad_x4_16x32_avx512);
        p.pu[LUMA_16x64].sad_x4 = PFX(pixel_sad_x4_16x64_avx512);
        p.pu[LUMA_32x8].sad_x4 = PFX(pixel_sad_x4_32x8_avx512);
        p.pu[LUMA_32x16].sad_x4 = PFX(pixel_sad_x4_32x16_avx512);
        p.pu[LUMA_32x24].sad_x4 = PFX(pixel_sad_x4_32x24_avx512);
        p.pu[LUMA_32x32].sad_x4 = PFX(pixel_sad_x4_32x32_avx512);
        p.pu[LUMA_32x64].sad_x4 = PFX(pixel_sad_x4_32x64_avx512);
        //p.pu[LUMA_48x64].sad_x4 = PFX(pixel_sad_x4_48x64_avx512);
        p.pu[LUMA_64x16].sad_x4 = PFX(pixel_sad_x4_64x16_avx512);
        p.pu[LUMA_64x32].sad_x4 = PFX(pixel_sad_x4_64x32_avx512);
        p.pu[LUMA_64x48].sad_x4 = PFX(pixel_sad_x4_64x48_avx512);
        p.pu[LUMA_64x64].sad_x4 = PFX(pixel_sad_x4_64x64_avx512);
        p.cu[BLOCK_16x16].cpy2Dto1D_shl = PFX(cpy2Dto1D_shl_16_avx512);
        p.cu[BLOCK_32x32].cpy2Dto1D_shl = PFX(cpy2Dto1D_shl_32_avx512);
        p.cu[BLOCK_32x32].cpy1Dto2D_shl[NONALIGNED] = PFX(cpy1Dto2D_shl_32_avx512);
        p.cu[BLOCK_32x32].cpy1Dto2D_shl[ALIGNED] = PFX(cpy1Dto2D_shl_aligned_32_avx512);
        p.cu[BLOCK_16x16].cpy1Dto2D_shr = PFX(cpy1Dto2D_shr_16_avx512);
        p.cu[BLOCK_32x32].cpy1Dto2D_shr = PFX(cpy1Dto2D_shr_32_avx512);

        p.cu[BLOCK_16x16].cpy2Dto1D_shr = PFX(cpy2Dto1D_shr_16_avx512);
        p.cu[BLOCK_32x32].cpy2Dto1D_shr = PFX(cpy2Dto1D_shr_32_avx512);

        p.cu[BLOCK_32x32].copy_cnt = PFX(copy_cnt_32_avx512);
        p.cu[BLOCK_16x16].copy_cnt = PFX(copy_cnt_16_avx512);

        p.pu[LUMA_16x4].addAvg[ALIGNED] = PFX(addAvg_aligned_16x4_avx512);
        p.pu[LUMA_16x8].addAvg[ALIGNED] = PFX(addAvg_aligned_16x8_avx512);
        p.pu[LUMA_16x12].addAvg[ALIGNED] = PFX(addAvg_aligned_16x12_avx512);
        p.pu[LUMA_16x16].addAvg[ALIGNED] = PFX(addAvg_aligned_16x16_avx512);
        p.pu[LUMA_16x32].addAvg[ALIGNED] = PFX(addAvg_aligned_16x32_avx512);
        p.pu[LUMA_16x64].addAvg[ALIGNED] = PFX(addAvg_aligned_16x64_avx512);
        p.pu[LUMA_48x64].addAvg[ALIGNED] = PFX(addAvg_aligned_48x64_avx512);
        p.pu[LUMA_32x8].addAvg[ALIGNED] = PFX(addAvg_aligned_32x8_avx512);
        p.pu[LUMA_32x16].addAvg[ALIGNED] = PFX(addAvg_aligned_32x16_avx512);
        p.pu[LUMA_32x24].addAvg[ALIGNED] = PFX(addAvg_aligned_32x24_avx512);
        p.pu[LUMA_32x32].addAvg[ALIGNED] = PFX(addAvg_aligned_32x32_avx512);
        p.pu[LUMA_32x64].addAvg[ALIGNED] = PFX(addAvg_aligned_32x64_avx512);
        p.pu[LUMA_64x16].addAvg[ALIGNED] = PFX(addAvg_aligned_64x16_avx512);
        p.pu[LUMA_64x32].addAvg[ALIGNED] = PFX(addAvg_aligned_64x32_avx512);
        p.pu[LUMA_64x48].addAvg[ALIGNED] = PFX(addAvg_aligned_64x48_avx512);
        p.pu[LUMA_64x64].addAvg[ALIGNED] = PFX(addAvg_aligned_64x64_avx512);

        p.chroma[X265_CSP_I420].pu[CHROMA_420_16x4].addAvg[ALIGNED] = PFX(addAvg_aligned_16x4_avx512);
        p.chroma[X265_CSP_I420].pu[CHROMA_420_16x8].addAvg[ALIGNED] = PFX(addAvg_aligned_16x8_avx512);
        p.chroma[X265_CSP_I420].pu[CHROMA_420_16x12].addAvg[ALIGNED] = PFX(addAvg_aligned_16x12_avx512);
        p.chroma[X265_CSP_I420].pu[CHROMA_420_16x16].addAvg[ALIGNED] = PFX(addAvg_aligned_16x16_avx512);
        p.chroma[X265_CSP_I420].pu[CHROMA_420_16x32].addAvg[ALIGNED] = PFX(addAvg_aligned_16x32_avx512);
        p.chroma[X265_CSP_I420].pu[CHROMA_420_32x8].addAvg[ALIGNED] = PFX(addAvg_aligned_32x8_avx512);
        p.chroma[X265_CSP_I420].pu[CHROMA_420_32x16].addAvg[ALIGNED] = PFX(addAvg_aligned_32x16_avx512);
        p.chroma[X265_CSP_I420].pu[CHROMA_420_32x24].addAvg[ALIGNED] = PFX(addAvg_aligned_32x24_avx512);
        p.chroma[X265_CSP_I420].pu[CHROMA_420_32x32].addAvg[ALIGNED] = PFX(addAvg_aligned_32x32_avx512);

        p.chroma[X265_CSP_I422].pu[CHROMA_422_16x32].addAvg[ALIGNED] = PFX(addAvg_aligned_16x32_avx512);
        p.chroma[X265_CSP_I422].pu[CHROMA_422_16x16].addAvg[ALIGNED] = PFX(addAvg_aligned_16x16_avx512);
        p.chroma[X265_CSP_I422].pu[CHROMA_422_16x64].addAvg[ALIGNED] = PFX(addAvg_aligned_16x64_avx512);
        p.chroma[X265_CSP_I422].pu[CHROMA_422_16x24].addAvg[ALIGNED] = PFX(addAvg_aligned_16x24_avx512);
        p.chroma[X265_CSP_I422].pu[CHROMA_422_16x8].addAvg[ALIGNED] = PFX(addAvg_aligned_16x8_avx512);
        p.chroma[X265_CSP_I422].pu[CHROMA_422_32x16].addAvg[ALIGNED] = PFX(addAvg_aligned_32x16_avx512);
        p.chroma[X265_CSP_I422].pu[CHROMA_422_32x32].addAvg[ALIGNED] = PFX(addAvg_aligned_32x32_avx512);
        p.chroma[X265_CSP_I422].pu[CHROMA_422_32x48].addAvg[ALIGNED] = PFX(addAvg_aligned_32x48_avx512);
        p.chroma[X265_CSP_I422].pu[CHROMA_422_32x64].addAvg[ALIGNED] = PFX(addAvg_aligned_32x64_avx512);
        p.cu[BLOCK_32x32].blockfill_s[NONALIGNED] = PFX(blockfill_s_32x32_avx512);
        p.cu[BLOCK_32x32].blockfill_s[ALIGNED] = PFX(blockfill_s_aligned_32x32_avx512);

        p.cu[BLOCK_8x8].dct = PFX(dct8_avx512);
        /* TODO: Currently these kernels performance are similar to AVX2 version, we need a to improve them further to ebable
        * it. Probably a Vtune analysis will help here.

        * p.cu[BLOCK_16x16].dct  = PFX(dct16_avx512);
        * p.cu[BLOCK_32x32].dct  = PFX(dct32_avx512); */

        p.cu[BLOCK_8x8].idct = PFX(idct8_avx512);
        p.cu[BLOCK_16x16].idct = PFX(idct16_avx512);
        p.cu[BLOCK_32x32].idct = PFX(idct32_avx512);
        p.denoiseDct = PFX(denoise_dct_avx512);
        p.pu[LUMA_16x8].satd = PFX(pixel_satd_16x8_avx512);
        p.pu[LUMA_16x16].satd = PFX(pixel_satd_16x16_avx512);
        p.pu[LUMA_16x32].satd = PFX(pixel_satd_16x32_avx512);
        p.pu[LUMA_16x64].satd = PFX(pixel_satd_16x64_avx512);
        p.pu[LUMA_32x8].satd = PFX(pixel_satd_32x8_avx512);
        p.pu[LUMA_32x16].satd = PFX(pixel_satd_32x16_avx512);
        p.pu[LUMA_32x24].satd = PFX(pixel_satd_32x24_avx512);
        p.pu[LUMA_32x32].satd = PFX(pixel_satd_32x32_avx512);
        p.pu[LUMA_32x64].satd = PFX(pixel_satd_32x64_avx512);
        p.pu[LUMA_64x16].satd = PFX(pixel_satd_64x16_avx512);
        p.pu[LUMA_64x32].satd = PFX(pixel_satd_64x32_avx512);
        p.pu[LUMA_64x48].satd = PFX(pixel_satd_64x48_avx512);
        p.pu[LUMA_64x64].satd = PFX(pixel_satd_64x64_avx512);
        p.pu[LUMA_48x64].satd = PFX(pixel_satd_48x64_avx512);
        p.chroma[X265_CSP_I420].pu[CHROMA_420_16x32].satd = PFX(pixel_satd_16x32_avx512);
        p.chroma[X265_CSP_I420].pu[CHROMA_420_16x16].satd = PFX(pixel_satd_16x16_avx512);
        p.chroma[X265_CSP_I420].pu[CHROMA_420_16x8].satd = PFX(pixel_satd_16x8_avx512);
        p.chroma[X265_CSP_I420].pu[CHROMA_420_32x32].satd = PFX(pixel_satd_32x32_avx512);
        p.chroma[X265_CSP_I420].pu[CHROMA_420_32x16].satd = PFX(pixel_satd_32x16_avx512);
        p.chroma[X265_CSP_I420].pu[CHROMA_420_32x24].satd = PFX(pixel_satd_32x24_avx512);
        p.chroma[X265_CSP_I420].pu[CHROMA_420_32x8].satd = PFX(pixel_satd_32x8_avx512);
        p.chroma[X265_CSP_I422].pu[CHROMA_422_16x64].satd = PFX(pixel_satd_16x64_avx512);
        p.chroma[X265_CSP_I422].pu[CHROMA_422_16x32].satd = PFX(pixel_satd_16x32_avx512);
        p.chroma[X265_CSP_I422].pu[CHROMA_422_16x16].satd = PFX(pixel_satd_16x16_avx512);
        p.chroma[X265_CSP_I422].pu[CHROMA_422_16x8].satd = PFX(pixel_satd_16x8_avx512);
        p.chroma[X265_CSP_I422].pu[CHROMA_422_32x64].satd = PFX(pixel_satd_32x64_avx512);
        p.chroma[X265_CSP_I422].pu[CHROMA_422_32x32].satd = PFX(pixel_satd_32x32_avx512);
        p.chroma[X265_CSP_I422].pu[CHROMA_422_32x16].satd = PFX(pixel_satd_32x16_avx512);

        p.cu[BLOCK_32x32].sse_ss = (pixel_sse_ss_t)PFX(pixel_ssd_32x32_avx512);
        p.cu[BLOCK_64x64].sse_ss = (pixel_sse_ss_t)PFX(pixel_ssd_64x64_avx512);
        p.cu[BLOCK_32x32].sse_pp = PFX(pixel_ssd_32x32_avx512);
        p.cu[BLOCK_64x64].sse_pp = PFX(pixel_ssd_64x64_avx512);
        p.chroma[X265_CSP_I420].cu[BLOCK_420_32x32].sse_pp = (pixel_sse_t)PFX(pixel_ssd_32x32_avx512);
        p.chroma[X265_CSP_I422].cu[BLOCK_422_32x64].sse_pp = (pixel_sse_t)PFX(pixel_ssd_32x64_avx512);
        p.planecopy_sp_shl = PFX(upShift_16_avx512);

    }
#endif
}
#else // if HIGH_BIT_DEPTH

void setupAssemblyPrimitives(EncoderPrimitives &p, int cpuMask) // Main
{
//#if X86_64
//    p.scanPosLast = PFX(scanPosLast_x64);
//#endif

    if (cpuMask & X265_CPU_SSE2)
    {
        /* We do not differentiate CPUs which support MMX and not SSE2. We only check
         * for SSE2 and then use both MMX and SSE2 functions */
        AVC_LUMA_PU(sad, mmx2);
        AVC_LUMA_PU(sad_x3, mmx2);
        AVC_LUMA_PU(sad_x4, mmx2);

        p.pu[LUMA_16x16].sad = PFX(pixel_sad_16x16_sse2);
        p.pu[LUMA_16x16].sad_x3 = PFX(pixel_sad_x3_16x16_sse2);
        p.pu[LUMA_16x16].sad_x4 = PFX(pixel_sad_x4_16x16_sse2);
        p.pu[LUMA_16x8].sad  = PFX(pixel_sad_16x8_sse2);
        p.pu[LUMA_16x8].sad_x3  = PFX(pixel_sad_x3_16x8_sse2);
        p.pu[LUMA_16x8].sad_x4  = PFX(pixel_sad_x4_16x8_sse2);
        HEVC_SAD(sse2);

        p.pu[LUMA_4x4].satd = p.cu[BLOCK_4x4].sa8d = PFX(pixel_satd_4x4_mmx2);
        ALL_LUMA_PU(satd, pixel_satd, sse2);

        p.cu[BLOCK_4x4].sse_pp = PFX(pixel_ssd_4x4_mmx);
        p.cu[BLOCK_8x8].sse_pp = PFX(pixel_ssd_8x8_mmx);
        p.cu[BLOCK_16x16].sse_pp = PFX(pixel_ssd_16x16_mmx);

        PIXEL_AVG_W4(mmx2);
        PIXEL_AVG(sse2);
        LUMA_VAR(sse2);

        ASSIGN_SA8D(sse2);
        p.chroma[X265_CSP_I422].cu[BLOCK_422_4x8].sse_pp = PFX(pixel_ssd_4x8_mmx);
        ASSIGN_SSE_PP(sse2);
        ASSIGN_SSE_SS(sse2);

        LUMA_PU_BLOCKCOPY(pp, sse2);
        CHROMA_420_PU_BLOCKCOPY(pp, sse2);
        CHROMA_422_PU_BLOCKCOPY(pp, sse2);

        LUMA_CU_BLOCKCOPY(ss, sse2);
        LUMA_CU_BLOCKCOPY(sp, sse2);
        CHROMA_420_CU_BLOCKCOPY(ss, sse2);
        CHROMA_422_CU_BLOCKCOPY(ss, sse2);
        CHROMA_420_CU_BLOCKCOPY(sp, sse2);
        CHROMA_422_CU_BLOCKCOPY(sp, sse2);

        p.frameInitLowres = PFX(frame_init_lowres_core_mmx2);
        p.frameInitLowres = PFX(frame_init_lowres_core_sse2);
        p.frameInitLowerRes = PFX(frame_init_lowres_core_sse2);
        p.frameSubSampleLuma = PFX(frame_subsample_luma_sse2);

        ALL_LUMA_TU(blockfill_s[NONALIGNED], blockfill_s, sse2);
        ALL_LUMA_TU(blockfill_s[ALIGNED], blockfill_s, sse2);
        ALL_LUMA_TU_S(cpy2Dto1D_shl, cpy2Dto1D_shl_, sse2);
        ALL_LUMA_TU_S(cpy2Dto1D_shr, cpy2Dto1D_shr_, sse2);
        ALL_LUMA_TU_S(cpy1Dto2D_shl[ALIGNED], cpy1Dto2D_shl_, sse2);
        ALL_LUMA_TU_S(cpy1Dto2D_shl[NONALIGNED], cpy1Dto2D_shl_, sse2);
        ALL_LUMA_TU_S(cpy1Dto2D_shr, cpy1Dto2D_shr_, sse2);
        ALL_LUMA_TU_S(ssd_s[NONALIGNED], pixel_ssd_s_, sse2);

        ASSIGN2(p.cu[BLOCK_4x4].calcresidual, getResidual4_sse2);
        ASSIGN2(p.cu[BLOCK_8x8].calcresidual, getResidual8_sse2);

        ALL_LUMA_TU_S(transpose, transpose, sse2);
        p.cu[BLOCK_64x64].transpose = PFX(transpose64_sse2);

        p.cu[BLOCK_4x4].dct = PFX(dct4_sse2);
        p.cu[BLOCK_8x8].dct = PFX(dct8_sse2);
        p.cu[BLOCK_4x4].idct = PFX(idct4_sse2);
#if X86_64
        p.cu[BLOCK_8x8].idct = PFX(idct8_sse2);
#endif
        p.idst4x4 = PFX(idst4_sse2);
        p.dst4x4 = PFX(dst4_sse2);

        p.planecopy_sp = PFX(downShift_16_sse2);
        ALL_LUMA_TU(count_nonzero, count_nonzero, sse2);
    }
    if (cpuMask & X265_CPU_SSSE3)
    {
        p.pu[LUMA_8x16].sad_x3 = PFX(pixel_sad_x3_8x16_ssse3);
        p.pu[LUMA_8x32].sad_x3 = PFX(pixel_sad_x3_8x32_ssse3);
        p.pu[LUMA_12x16].sad_x3 = PFX(pixel_sad_x3_12x16_ssse3);
        HEVC_SAD_X3(ssse3);

        p.pu[LUMA_8x4].sad_x4  = PFX(pixel_sad_x4_8x4_ssse3);
        p.pu[LUMA_8x8].sad_x4  = PFX(pixel_sad_x4_8x8_ssse3);
        p.pu[LUMA_8x16].sad_x4 = PFX(pixel_sad_x4_8x16_ssse3);
        p.pu[LUMA_8x32].sad_x4 = PFX(pixel_sad_x4_8x32_ssse3);
        p.pu[LUMA_12x16].sad_x4 = PFX(pixel_sad_x4_12x16_ssse3);
        HEVC_SAD_X4(ssse3);

        p.pu[LUMA_4x4].satd = p.cu[BLOCK_4x4].sa8d = PFX(pixel_satd_4x4_ssse3);
        ALL_LUMA_PU(satd, pixel_satd, ssse3);

        ASSIGN_SA8D(ssse3);
        PIXEL_AVG(ssse3);
        PIXEL_AVG_W4(ssse3);

        ASSIGN_SSE_PP(ssse3);
        p.cu[BLOCK_4x4].sse_pp = PFX(pixel_ssd_4x4_ssse3);
        p.chroma[X265_CSP_I422].cu[BLOCK_422_4x8].sse_pp = PFX(pixel_ssd_4x8_ssse3);

        p.dst4x4 = PFX(dst4_ssse3);
        p.cu[BLOCK_8x8].idct = PFX(idct8_ssse3);

        // MUST be done after LUMA_FILTERS() to overwrite default version
        //p.pu[LUMA_8x8].luma_hvpp = PFX(interp_8tap_hv_pp_8x8_ssse3);

        p.frameInitLowres = PFX(frame_init_lowres_core_ssse3);
        p.frameInitLowerRes = PFX(frame_init_lowres_core_ssse3);

        p.frameSubSampleLuma = PFX(frame_subsample_luma_ssse3);
    }
    if (cpuMask & X265_CPU_SSE4)
    {

        LUMA_ADDAVG(sse4);
        CHROMA_420_ADDAVG(sse4);
        CHROMA_422_ADDAVG(sse4);

        // TODO: check POPCNT flag!
        ALL_LUMA_TU_S(copy_cnt, copy_cnt_, sse4);

        p.pu[LUMA_4x4].satd = p.cu[BLOCK_4x4].sa8d = PFX(pixel_satd_4x4_sse4);
        ALL_LUMA_PU(satd, pixel_satd, sse4);
        ASSIGN_SA8D(sse4);
        ASSIGN_SSE_SS(sse4);
        p.cu[BLOCK_64x64].sse_pp = PFX(pixel_ssd_64x64_sse4);

        LUMA_PIXELSUB(sse4);
        CHROMA_420_PIXELSUB_PS(sse4);
        CHROMA_422_PIXELSUB_PS(sse4);

        // MUST be done after LUMA_FILTERS() to overwrite default version
        //p.pu[LUMA_8x8].luma_hvpp = PFX(interp_8tap_hv_pp_8x8_ssse3);

        LUMA_CU_BLOCKCOPY(ps, sse4);
        CHROMA_420_CU_BLOCKCOPY(ps, sse4);
        CHROMA_422_CU_BLOCKCOPY(ps, sse4);

        ASSIGN2(p.cu[BLOCK_16x16].calcresidual, getResidual16_sse4);
        ASSIGN2(p.cu[BLOCK_32x32].calcresidual, getResidual32_sse4);
        p.cu[BLOCK_8x8].dct = PFX(dct8_sse4);
        p.denoiseDct = PFX(denoise_dct_sse4);
        p.quant = PFX(quant_sse4);
        p.nquant = PFX(nquant_sse4);
        p.dequant_normal = PFX(dequant_normal_sse4);
        p.dequant_scaling = PFX(dequant_scaling_sse4);

        p.weight_pp = PFX(weight_pp_sse4);
        p.weight_sp = PFX(weight_sp_sse4);

    }
    if (cpuMask & X265_CPU_AVX)
    {
        p.pu[LUMA_4x4].satd = p.cu[BLOCK_4x4].sa8d = PFX(pixel_satd_4x4_avx);
        p.chroma[X265_CSP_I422].pu[CHROMA_422_16x24].satd = PFX(pixel_satd_16x24_avx);
        p.chroma[X265_CSP_I422].pu[CHROMA_422_32x48].satd = PFX(pixel_satd_32x48_avx);
        p.chroma[X265_CSP_I422].pu[CHROMA_422_24x64].satd = PFX(pixel_satd_24x64_avx);
        p.chroma[X265_CSP_I422].pu[CHROMA_422_8x64].satd = PFX(pixel_satd_8x64_avx);
        p.chroma[X265_CSP_I422].pu[CHROMA_422_8x12].satd = PFX(pixel_satd_8x12_avx);
        p.chroma[X265_CSP_I422].pu[CHROMA_422_12x32].satd = PFX(pixel_satd_12x32_avx);
        p.chroma[X265_CSP_I422].pu[CHROMA_422_4x32].satd = PFX(pixel_satd_4x32_avx);
        p.chroma[X265_CSP_I422].pu[CHROMA_422_16x32].satd = PFX(pixel_satd_16x32_avx);
        p.chroma[X265_CSP_I422].pu[CHROMA_422_32x64].satd = PFX(pixel_satd_32x64_avx);
        p.chroma[X265_CSP_I422].pu[CHROMA_422_16x16].satd = PFX(pixel_satd_16x16_avx);
        p.chroma[X265_CSP_I422].pu[CHROMA_422_32x32].satd = PFX(pixel_satd_32x32_avx);
        p.chroma[X265_CSP_I422].pu[CHROMA_422_16x64].satd = PFX(pixel_satd_16x64_avx);
        p.chroma[X265_CSP_I422].pu[CHROMA_422_16x8].satd = PFX(pixel_satd_16x8_avx);
        p.chroma[X265_CSP_I422].pu[CHROMA_422_32x16].satd = PFX(pixel_satd_32x16_avx);
        p.chroma[X265_CSP_I422].pu[CHROMA_422_8x4].satd = PFX(pixel_satd_8x4_avx);
        p.chroma[X265_CSP_I422].pu[CHROMA_422_8x16].satd = PFX(pixel_satd_8x16_avx);
        p.chroma[X265_CSP_I422].pu[CHROMA_422_8x8].satd = PFX(pixel_satd_8x8_avx);
        p.chroma[X265_CSP_I422].pu[CHROMA_422_8x32].satd = PFX(pixel_satd_8x32_avx);
        p.chroma[X265_CSP_I422].pu[CHROMA_422_4x8].satd = PFX(pixel_satd_4x8_avx);
        p.chroma[X265_CSP_I422].pu[CHROMA_422_4x16].satd = PFX(pixel_satd_4x16_avx);
        p.chroma[X265_CSP_I422].pu[CHROMA_422_4x4].satd = PFX(pixel_satd_4x4_avx);
        ALL_LUMA_PU(satd, pixel_satd, avx);
        p.chroma[X265_CSP_I420].pu[CHROMA_420_4x4].satd = PFX(pixel_satd_4x4_avx);
        p.chroma[X265_CSP_I420].pu[CHROMA_420_8x8].satd = PFX(pixel_satd_8x8_avx);
        p.chroma[X265_CSP_I420].pu[CHROMA_420_16x16].satd = PFX(pixel_satd_16x16_avx);
        p.chroma[X265_CSP_I420].pu[CHROMA_420_32x32].satd = PFX(pixel_satd_32x32_avx);
        p.chroma[X265_CSP_I420].pu[CHROMA_420_8x4].satd = PFX(pixel_satd_8x4_avx);
        p.chroma[X265_CSP_I420].pu[CHROMA_420_4x8].satd = PFX(pixel_satd_4x8_avx);
        p.chroma[X265_CSP_I420].pu[CHROMA_420_16x8].satd = PFX(pixel_satd_16x8_avx);
        p.chroma[X265_CSP_I420].pu[CHROMA_420_8x16].satd = PFX(pixel_satd_8x16_avx);
        p.chroma[X265_CSP_I420].pu[CHROMA_420_32x16].satd = PFX(pixel_satd_32x16_avx);
        p.chroma[X265_CSP_I420].pu[CHROMA_420_16x32].satd = PFX(pixel_satd_16x32_avx);
        p.chroma[X265_CSP_I420].pu[CHROMA_420_16x12].satd = PFX(pixel_satd_16x12_avx);
        p.chroma[X265_CSP_I420].pu[CHROMA_420_12x16].satd = PFX(pixel_satd_12x16_avx);
        p.chroma[X265_CSP_I420].pu[CHROMA_420_16x4].satd = PFX(pixel_satd_16x4_avx);
        p.chroma[X265_CSP_I420].pu[CHROMA_420_4x16].satd = PFX(pixel_satd_4x16_avx);
        p.chroma[X265_CSP_I420].pu[CHROMA_420_32x24].satd = PFX(pixel_satd_32x24_avx);
        p.chroma[X265_CSP_I420].pu[CHROMA_420_24x32].satd = PFX(pixel_satd_24x32_avx);
        p.chroma[X265_CSP_I420].pu[CHROMA_420_32x8].satd = PFX(pixel_satd_32x8_avx);
        p.chroma[X265_CSP_I420].pu[CHROMA_420_8x32].satd = PFX(pixel_satd_8x32_avx);
        ASSIGN_SA8D(avx);
        p.chroma[X265_CSP_I420].cu[BLOCK_420_32x32].sa8d = PFX(pixel_sa8d_32x32_avx);
        p.chroma[X265_CSP_I420].cu[BLOCK_420_16x16].sa8d = PFX(pixel_sa8d_16x16_avx);
        p.chroma[X265_CSP_I420].cu[BLOCK_420_8x8].sa8d = PFX(pixel_sa8d_8x8_avx);
        p.chroma[X265_CSP_I420].cu[BLOCK_420_4x4].sa8d = PFX(pixel_satd_4x4_avx);
        ASSIGN_SSE_PP(avx);
        p.chroma[X265_CSP_I420].cu[BLOCK_420_8x8].sse_pp = PFX(pixel_ssd_8x8_avx);
        ASSIGN_SSE_SS(avx);
        LUMA_VAR(avx);

        p.pu[LUMA_12x16].sad_x3 = PFX(pixel_sad_x3_12x16_avx);
        p.pu[LUMA_16x4].sad_x3  = PFX(pixel_sad_x3_16x4_avx);
        HEVC_SAD_X3(avx);

        p.pu[LUMA_12x16].sad_x4 = PFX(pixel_sad_x4_12x16_avx);
        p.pu[LUMA_16x4].sad_x4  = PFX(pixel_sad_x4_16x4_avx);
        HEVC_SAD_X4(avx);

        p.cu[BLOCK_16x16].copy_ss = PFX(blockcopy_ss_16x16_avx);
        p.cu[BLOCK_32x32].copy_ss = PFX(blockcopy_ss_32x32_avx);
        p.cu[BLOCK_64x64].copy_ss = PFX(blockcopy_ss_64x64_avx);
        p.chroma[X265_CSP_I420].cu[CHROMA_420_16x16].copy_ss = PFX(blockcopy_ss_16x16_avx);
        p.chroma[X265_CSP_I420].cu[CHROMA_420_32x32].copy_ss = PFX(blockcopy_ss_32x32_avx);
        p.chroma[X265_CSP_I422].cu[CHROMA_422_16x32].copy_ss = PFX(blockcopy_ss_16x32_avx);
        p.chroma[X265_CSP_I422].cu[CHROMA_422_32x64].copy_ss = PFX(blockcopy_ss_32x64_avx);

        p.chroma[X265_CSP_I420].pu[CHROMA_420_32x8].copy_pp = PFX(blockcopy_pp_32x8_avx);
        p.pu[LUMA_32x8].copy_pp = PFX(blockcopy_pp_32x8_avx);

        p.chroma[X265_CSP_I420].pu[CHROMA_420_32x16].copy_pp = PFX(blockcopy_pp_32x16_avx);
        p.chroma[X265_CSP_I422].pu[CHROMA_422_32x16].copy_pp = PFX(blockcopy_pp_32x16_avx);
        p.pu[LUMA_32x16].copy_pp = PFX(blockcopy_pp_32x16_avx);

        p.chroma[X265_CSP_I420].pu[CHROMA_420_32x24].copy_pp = PFX(blockcopy_pp_32x24_avx);
        p.pu[LUMA_32x24].copy_pp = PFX(blockcopy_pp_32x24_avx);

        p.chroma[X265_CSP_I420].pu[CHROMA_420_32x32].copy_pp = PFX(blockcopy_pp_32x32_avx);
        p.chroma[X265_CSP_I422].pu[CHROMA_422_32x32].copy_pp = PFX(blockcopy_pp_32x32_avx);
        p.pu[LUMA_32x32].copy_pp  = PFX(blockcopy_pp_32x32_avx);

        p.chroma[X265_CSP_I422].pu[CHROMA_422_32x48].copy_pp = PFX(blockcopy_pp_32x48_avx);

        p.chroma[X265_CSP_I422].pu[CHROMA_422_32x64].copy_pp = PFX(blockcopy_pp_32x64_avx);
        p.pu[LUMA_32x64].copy_pp = PFX(blockcopy_pp_32x64_avx);

        p.pu[LUMA_64x16].copy_pp = PFX(blockcopy_pp_64x16_avx);
        p.pu[LUMA_64x32].copy_pp = PFX(blockcopy_pp_64x32_avx);
        p.pu[LUMA_64x48].copy_pp = PFX(blockcopy_pp_64x48_avx);
        p.pu[LUMA_64x64].copy_pp = PFX(blockcopy_pp_64x64_avx);

        p.pu[LUMA_48x64].copy_pp = PFX(blockcopy_pp_48x64_avx);

        p.frameInitLowres = PFX(frame_init_lowres_core_avx);
        p.frameInitLowerRes = PFX(frame_init_lowres_core_avx);

        p.frameSubSampleLuma = PFX(frame_subsample_luma_avx);
    }
    if (cpuMask & X265_CPU_XOP)
    {
        //p.pu[LUMA_4x4].satd = p.cu[BLOCK_4x4].sa8d = PFX(pixel_satd_4x4_xop); this one is broken
        ALL_LUMA_PU(satd, pixel_satd, xop);
        ASSIGN_SA8D(xop);
        LUMA_VAR(xop);
        p.cu[BLOCK_8x8].sse_pp = PFX(pixel_ssd_8x8_xop);
        p.cu[BLOCK_16x16].sse_pp = PFX(pixel_ssd_16x16_xop);
        p.frameInitLowres = PFX(frame_init_lowres_core_xop);
        p.frameInitLowerRes = PFX(frame_init_lowres_core_xop);
        p.frameSubSampleLuma = PFX(frame_subsample_luma_xop);

    }
#if X86_64
    if (cpuMask & X265_CPU_AVX2)
    {
        p.cu[BLOCK_16x16].sse_ss = (pixel_sse_ss_t)PFX(pixel_ssd_ss_16x16_avx2);
        p.cu[BLOCK_32x32].sse_ss = (pixel_sse_ss_t)PFX(pixel_ssd_ss_32x32_avx2);
        p.cu[BLOCK_64x64].sse_ss = (pixel_sse_ss_t)PFX(pixel_ssd_ss_64x64_avx2);

        p.cu[BLOCK_16x16].var = PFX(pixel_var_16x16_avx2);
        p.cu[BLOCK_32x32].var = PFX(pixel_var_32x32_avx2);
        p.cu[BLOCK_64x64].var = PFX(pixel_var_64x64_avx2);

        p.planecopy_sp = PFX(downShift_16_avx2);

        p.idst4x4 = PFX(idst4_avx2);
        p.dst4x4 = PFX(dst4_avx2);

        ASSIGN2(p.pu[LUMA_8x4].addAvg, addAvg_8x4_avx2);
        ASSIGN2(p.pu[LUMA_8x8].addAvg, addAvg_8x8_avx2);
        ASSIGN2(p.pu[LUMA_8x16].addAvg, addAvg_8x16_avx2);
        ASSIGN2(p.pu[LUMA_8x32].addAvg, addAvg_8x32_avx2);
        ASSIGN2(p.pu[LUMA_12x16].addAvg, addAvg_12x16_avx2);
        ASSIGN2(p.pu[LUMA_16x4].addAvg, addAvg_16x4_avx2);
        ASSIGN2(p.pu[LUMA_16x8].addAvg, addAvg_16x8_avx2);
        ASSIGN2(p.pu[LUMA_16x12].addAvg, addAvg_16x12_avx2);
        ASSIGN2(p.pu[LUMA_16x16].addAvg, addAvg_16x16_avx2);
        ASSIGN2(p.pu[LUMA_16x32].addAvg, addAvg_16x32_avx2);
        ASSIGN2(p.pu[LUMA_16x64].addAvg, addAvg_16x64_avx2);
        ASSIGN2(p.pu[LUMA_24x32].addAvg, addAvg_24x32_avx2);
        ASSIGN2(p.pu[LUMA_32x8].addAvg, addAvg_32x8_avx2);
        ASSIGN2(p.pu[LUMA_32x16].addAvg, addAvg_32x16_avx2);
        ASSIGN2(p.pu[LUMA_32x24].addAvg, addAvg_32x24_avx2);
        ASSIGN2(p.pu[LUMA_32x32].addAvg, addAvg_32x32_avx2);
        ASSIGN2(p.pu[LUMA_32x64].addAvg, addAvg_32x64_avx2);
        ASSIGN2(p.pu[LUMA_48x64].addAvg, addAvg_48x64_avx2);
        ASSIGN2(p.pu[LUMA_64x16].addAvg, addAvg_64x16_avx2);
        ASSIGN2(p.pu[LUMA_64x32].addAvg, addAvg_64x32_avx2);
        ASSIGN2(p.pu[LUMA_64x48].addAvg, addAvg_64x48_avx2);
        ASSIGN2(p.pu[LUMA_64x64].addAvg, addAvg_64x64_avx2);

        ASSIGN2(p.chroma[X265_CSP_I420].pu[CHROMA_420_8x2].addAvg, addAvg_8x2_avx2);
        ASSIGN2(p.chroma[X265_CSP_I420].pu[CHROMA_420_8x4].addAvg, addAvg_8x4_avx2);
        ASSIGN2(p.chroma[X265_CSP_I420].pu[CHROMA_420_8x6].addAvg, addAvg_8x6_avx2);
        ASSIGN2(p.chroma[X265_CSP_I420].pu[CHROMA_420_8x8].addAvg, addAvg_8x8_avx2);
        ASSIGN2(p.chroma[X265_CSP_I420].pu[CHROMA_420_8x16].addAvg, addAvg_8x16_avx2);
        ASSIGN2(p.chroma[X265_CSP_I420].pu[CHROMA_420_8x32].addAvg, addAvg_8x32_avx2);
        ASSIGN2(p.chroma[X265_CSP_I420].pu[CHROMA_420_12x16].addAvg, addAvg_12x16_avx2);
        ASSIGN2(p.chroma[X265_CSP_I420].pu[CHROMA_420_16x4].addAvg, addAvg_16x4_avx2);
        ASSIGN2(p.chroma[X265_CSP_I420].pu[CHROMA_420_16x8].addAvg, addAvg_16x8_avx2);
        ASSIGN2(p.chroma[X265_CSP_I420].pu[CHROMA_420_16x12].addAvg, addAvg_16x12_avx2);
        ASSIGN2(p.chroma[X265_CSP_I420].pu[CHROMA_420_16x16].addAvg, addAvg_16x16_avx2);
        ASSIGN2(p.chroma[X265_CSP_I420].pu[CHROMA_420_16x32].addAvg, addAvg_16x32_avx2);
        ASSIGN2(p.chroma[X265_CSP_I420].pu[CHROMA_420_32x8].addAvg, addAvg_32x8_avx2);
        ASSIGN2(p.chroma[X265_CSP_I420].pu[CHROMA_420_32x16].addAvg, addAvg_32x16_avx2);
        ASSIGN2(p.chroma[X265_CSP_I420].pu[CHROMA_420_32x24].addAvg, addAvg_32x24_avx2);
        ASSIGN2(p.chroma[X265_CSP_I420].pu[CHROMA_420_32x32].addAvg, addAvg_32x32_avx2);

        ASSIGN2(p.chroma[X265_CSP_I422].pu[CHROMA_422_8x4].addAvg, addAvg_8x4_avx2);
        ASSIGN2(p.chroma[X265_CSP_I422].pu[CHROMA_422_8x8].addAvg, addAvg_8x8_avx2);
        ASSIGN2(p.chroma[X265_CSP_I422].pu[CHROMA_422_8x12].addAvg, addAvg_8x12_avx2);
        ASSIGN2(p.chroma[X265_CSP_I422].pu[CHROMA_422_8x16].addAvg, addAvg_8x16_avx2);
        ASSIGN2(p.chroma[X265_CSP_I422].pu[CHROMA_422_8x32].addAvg, addAvg_8x32_avx2);
        ASSIGN2(p.chroma[X265_CSP_I422].pu[CHROMA_422_8x64].addAvg, addAvg_8x64_avx2);
        ASSIGN2(p.chroma[X265_CSP_I422].pu[CHROMA_422_12x32].addAvg, addAvg_12x32_avx2);
        ASSIGN2(p.chroma[X265_CSP_I422].pu[CHROMA_422_16x8].addAvg, addAvg_16x8_avx2);
        ASSIGN2(p.chroma[X265_CSP_I422].pu[CHROMA_422_16x16].addAvg, addAvg_16x16_avx2);
        ASSIGN2(p.chroma[X265_CSP_I422].pu[CHROMA_422_16x24].addAvg, addAvg_16x24_avx2);
        ASSIGN2(p.chroma[X265_CSP_I422].pu[CHROMA_422_16x32].addAvg, addAvg_16x32_avx2);
        ASSIGN2(p.chroma[X265_CSP_I422].pu[CHROMA_422_16x64].addAvg, addAvg_16x64_avx2);
        ASSIGN2(p.chroma[X265_CSP_I422].pu[CHROMA_422_24x64].addAvg, addAvg_24x64_avx2);
        ASSIGN2(p.chroma[X265_CSP_I422].pu[CHROMA_422_32x16].addAvg, addAvg_32x16_avx2);
        ASSIGN2(p.chroma[X265_CSP_I422].pu[CHROMA_422_32x32].addAvg, addAvg_32x32_avx2);
        ASSIGN2(p.chroma[X265_CSP_I422].pu[CHROMA_422_32x48].addAvg, addAvg_32x48_avx2);
        ASSIGN2(p.chroma[X265_CSP_I422].pu[CHROMA_422_32x64].addAvg, addAvg_32x64_avx2);

        p.cu[BLOCK_8x8].sa8d = PFX(pixel_sa8d_8x8_avx2);
        p.cu[BLOCK_16x16].sa8d = PFX(pixel_sa8d_16x16_avx2);
        p.cu[BLOCK_32x32].sa8d = PFX(pixel_sa8d_32x32_avx2);
        p.chroma[X265_CSP_I420].cu[BLOCK_420_8x8].sa8d = PFX(pixel_sa8d_8x8_avx2);
        p.chroma[X265_CSP_I420].cu[BLOCK_420_16x16].sa8d = PFX(pixel_sa8d_16x16_avx2);
        p.chroma[X265_CSP_I420].cu[BLOCK_420_32x32].sa8d = PFX(pixel_sa8d_32x32_avx2);

        ASSIGN2(p.cu[BLOCK_16x16].add_ps, pixel_add_ps_16x16_avx2);
        ASSIGN2(p.cu[BLOCK_32x32].add_ps, pixel_add_ps_32x32_avx2);
        ASSIGN2(p.cu[BLOCK_64x64].add_ps, pixel_add_ps_64x64_avx2);
        ASSIGN2(p.chroma[X265_CSP_I420].cu[BLOCK_420_16x16].add_ps, pixel_add_ps_16x16_avx2);
        ASSIGN2(p.chroma[X265_CSP_I420].cu[BLOCK_420_32x32].add_ps, pixel_add_ps_32x32_avx2);
        ASSIGN2(p.chroma[X265_CSP_I422].cu[BLOCK_422_16x32].add_ps, pixel_add_ps_16x32_avx2);
        ASSIGN2(p.chroma[X265_CSP_I422].cu[BLOCK_422_32x64].add_ps, pixel_add_ps_32x64_avx2);

        p.cu[BLOCK_16x16].sub_ps = PFX(pixel_sub_ps_16x16_avx2);
        p.cu[BLOCK_32x32].sub_ps = PFX(pixel_sub_ps_32x32_avx2);
        p.cu[BLOCK_64x64].sub_ps = PFX(pixel_sub_ps_64x64_avx2);
        p.chroma[X265_CSP_I420].cu[BLOCK_420_16x16].sub_ps = PFX(pixel_sub_ps_16x16_avx2);
        p.chroma[X265_CSP_I420].cu[BLOCK_420_32x32].sub_ps = PFX(pixel_sub_ps_32x32_avx2);
        p.chroma[X265_CSP_I422].cu[BLOCK_422_16x32].sub_ps = PFX(pixel_sub_ps_16x32_avx2);
        p.chroma[X265_CSP_I422].cu[BLOCK_422_32x64].sub_ps = PFX(pixel_sub_ps_32x64_avx2);
        ASSIGN2(p.pu[LUMA_16x4].pixelavg_pp, pixel_avg_16x4_avx2);
        ASSIGN2(p.pu[LUMA_16x8].pixelavg_pp, pixel_avg_16x8_avx2);
        ASSIGN2(p.pu[LUMA_16x12].pixelavg_pp, pixel_avg_16x12_avx2);
        ASSIGN2(p.pu[LUMA_16x16].pixelavg_pp, pixel_avg_16x16_avx2);
        ASSIGN2(p.pu[LUMA_16x32].pixelavg_pp, pixel_avg_16x32_avx2);
        ASSIGN2(p.pu[LUMA_16x64].pixelavg_pp, pixel_avg_16x64_avx2);

        ASSIGN2(p.pu[LUMA_32x64].pixelavg_pp, pixel_avg_32x64_avx2);
        ASSIGN2(p.pu[LUMA_32x32].pixelavg_pp, pixel_avg_32x32_avx2);
        ASSIGN2(p.pu[LUMA_32x24].pixelavg_pp, pixel_avg_32x24_avx2);
        ASSIGN2(p.pu[LUMA_32x16].pixelavg_pp, pixel_avg_32x16_avx2);
        ASSIGN2(p.pu[LUMA_32x8].pixelavg_pp, pixel_avg_32x8_avx2);
        ASSIGN2(p.pu[LUMA_48x64].pixelavg_pp, pixel_avg_48x64_avx2);
        ASSIGN2(p.pu[LUMA_64x64].pixelavg_pp, pixel_avg_64x64_avx2);
        ASSIGN2(p.pu[LUMA_64x48].pixelavg_pp, pixel_avg_64x48_avx2);
        ASSIGN2(p.pu[LUMA_64x32].pixelavg_pp, pixel_avg_64x32_avx2);
        ASSIGN2(p.pu[LUMA_64x16].pixelavg_pp, pixel_avg_64x16_avx2);
        p.pu[LUMA_16x16].satd = PFX(pixel_satd_16x16_avx2);
        p.pu[LUMA_16x8].satd  = PFX(pixel_satd_16x8_avx2);
        p.pu[LUMA_8x16].satd  = PFX(pixel_satd_8x16_avx2);
        p.pu[LUMA_8x8].satd   = PFX(pixel_satd_8x8_avx2);

        p.pu[LUMA_16x4].satd  = PFX(pixel_satd_16x4_avx2);
        p.pu[LUMA_16x12].satd = PFX(pixel_satd_16x12_avx2);
        p.pu[LUMA_16x32].satd = PFX(pixel_satd_16x32_avx2);
        p.pu[LUMA_16x64].satd = PFX(pixel_satd_16x64_avx2);

        p.pu[LUMA_32x8].satd   = PFX(pixel_satd_32x8_avx2);
        p.pu[LUMA_32x16].satd   = PFX(pixel_satd_32x16_avx2);
        p.pu[LUMA_32x24].satd   = PFX(pixel_satd_32x24_avx2);
        p.pu[LUMA_32x32].satd   = PFX(pixel_satd_32x32_avx2);
        p.pu[LUMA_32x64].satd   = PFX(pixel_satd_32x64_avx2);
        p.pu[LUMA_48x64].satd   = PFX(pixel_satd_48x64_avx2);
        p.pu[LUMA_64x16].satd   = PFX(pixel_satd_64x16_avx2);
        p.pu[LUMA_64x32].satd   = PFX(pixel_satd_64x32_avx2);
        p.pu[LUMA_64x48].satd   = PFX(pixel_satd_64x48_avx2);
        p.pu[LUMA_64x64].satd   = PFX(pixel_satd_64x64_avx2);

        p.pu[LUMA_32x8].sad = PFX(pixel_sad_32x8_avx2);
        p.pu[LUMA_32x16].sad = PFX(pixel_sad_32x16_avx2);
        p.pu[LUMA_32x24].sad = PFX(pixel_sad_32x24_avx2);
        p.pu[LUMA_32x32].sad = PFX(pixel_sad_32x32_avx2);
        p.pu[LUMA_32x64].sad = PFX(pixel_sad_32x64_avx2);
        p.pu[LUMA_48x64].sad = PFX(pixel_sad_48x64_avx2);
        p.pu[LUMA_64x16].sad = PFX(pixel_sad_64x16_avx2);
        p.pu[LUMA_64x32].sad = PFX(pixel_sad_64x32_avx2);
        p.pu[LUMA_64x48].sad = PFX(pixel_sad_64x48_avx2);
        p.pu[LUMA_64x64].sad = PFX(pixel_sad_64x64_avx2);

        p.pu[LUMA_8x4].sad_x3 = PFX(pixel_sad_x3_8x4_avx2);
        p.pu[LUMA_8x8].sad_x3 = PFX(pixel_sad_x3_8x8_avx2);
        p.pu[LUMA_8x16].sad_x3 = PFX(pixel_sad_x3_8x16_avx2);

        p.pu[LUMA_8x8].sad_x4 = PFX(pixel_sad_x4_8x8_avx2);
        p.pu[LUMA_16x8].sad_x4  = PFX(pixel_sad_x4_16x8_avx2);
        p.pu[LUMA_16x12].sad_x4 = PFX(pixel_sad_x4_16x12_avx2);
        p.pu[LUMA_16x16].sad_x4 = PFX(pixel_sad_x4_16x16_avx2);
        p.pu[LUMA_16x32].sad_x4 = PFX(pixel_sad_x4_16x32_avx2);
        p.pu[LUMA_32x32].sad_x4 = PFX(pixel_sad_x4_32x32_avx2);
        p.pu[LUMA_32x16].sad_x4 = PFX(pixel_sad_x4_32x16_avx2);
        p.pu[LUMA_32x64].sad_x4 = PFX(pixel_sad_x4_32x64_avx2);
        p.pu[LUMA_32x24].sad_x4 = PFX(pixel_sad_x4_32x24_avx2);
        p.pu[LUMA_32x8].sad_x4 = PFX(pixel_sad_x4_32x8_avx2);
        p.pu[LUMA_48x64].sad_x4 = PFX(pixel_sad_x4_48x64_avx2);
        p.pu[LUMA_64x16].sad_x4 = PFX(pixel_sad_x4_64x16_avx2);
        p.pu[LUMA_64x32].sad_x4 = PFX(pixel_sad_x4_64x32_avx2);
        p.pu[LUMA_64x48].sad_x4 = PFX(pixel_sad_x4_64x48_avx2);
        p.pu[LUMA_64x64].sad_x4 = PFX(pixel_sad_x4_64x64_avx2);

        p.cu[BLOCK_16x16].sse_pp = PFX(pixel_ssd_16x16_avx2);
        p.cu[BLOCK_32x32].sse_pp = PFX(pixel_ssd_32x32_avx2);
        p.cu[BLOCK_64x64].sse_pp = PFX(pixel_ssd_64x64_avx2);
        p.chroma[X265_CSP_I420].cu[BLOCK_420_16x16].sse_pp = PFX(pixel_ssd_16x16_avx2);
        p.chroma[X265_CSP_I420].cu[BLOCK_420_32x32].sse_pp = PFX(pixel_ssd_32x32_avx2);

        ASSIGN2(p.cu[BLOCK_16x16].ssd_s, pixel_ssd_s_16_avx2);
        ASSIGN2(p.cu[BLOCK_32x32].ssd_s, pixel_ssd_s_32_avx2);
        p.cu[BLOCK_8x8].copy_cnt = PFX(copy_cnt_8_avx2);
        p.cu[BLOCK_16x16].copy_cnt = PFX(copy_cnt_16_avx2);
        p.cu[BLOCK_32x32].copy_cnt = PFX(copy_cnt_32_avx2);
        ASSIGN2(p.cu[BLOCK_16x16].blockfill_s, blockfill_s_16x16_avx2);
        ALL_LUMA_TU_S(cpy1Dto2D_shl[ALIGNED], cpy1Dto2D_shl_, avx2);
        ALL_LUMA_TU_S(cpy1Dto2D_shl[NONALIGNED], cpy1Dto2D_shl_, avx2);
        ALL_LUMA_TU_S(cpy1Dto2D_shr, cpy1Dto2D_shr_, avx2);
        p.cu[BLOCK_8x8].cpy2Dto1D_shl = PFX(cpy2Dto1D_shl_8_avx2);
        p.cu[BLOCK_16x16].cpy2Dto1D_shl = PFX(cpy2Dto1D_shl_16_avx2);
        p.cu[BLOCK_32x32].cpy2Dto1D_shl = PFX(cpy2Dto1D_shl_32_avx2);

        p.cu[BLOCK_8x8].cpy2Dto1D_shr = PFX(cpy2Dto1D_shr_8_avx2);
        p.cu[BLOCK_16x16].cpy2Dto1D_shr = PFX(cpy2Dto1D_shr_16_avx2);
        p.cu[BLOCK_32x32].cpy2Dto1D_shr = PFX(cpy2Dto1D_shr_32_avx2);

        ALL_LUMA_TU(count_nonzero, count_nonzero, avx2);
        p.denoiseDct = PFX(denoise_dct_avx2);
        p.quant = PFX(quant_avx2);
        p.nquant = PFX(nquant_avx2);
        p.dequant_normal = PFX(dequant_normal_avx2);
        p.dequant_scaling = PFX(dequant_scaling_avx2);

        ASSIGN2(p.cu[BLOCK_16x16].calcresidual, getResidual16_avx2);
        ASSIGN2(p.cu[BLOCK_32x32].calcresidual, getResidual32_avx2);

        p.weight_pp = PFX(weight_pp_avx2);
        p.weight_sp = PFX(weight_sp_avx2);

        // copy_sp primitives
        p.cu[BLOCK_16x16].copy_sp = PFX(blockcopy_sp_16x16_avx2);
        p.chroma[X265_CSP_I420].cu[BLOCK_420_16x16].copy_sp = PFX(blockcopy_sp_16x16_avx2);
        p.chroma[X265_CSP_I422].cu[BLOCK_422_16x32].copy_sp = PFX(blockcopy_sp_16x32_avx2);

        p.cu[BLOCK_32x32].copy_sp = PFX(blockcopy_sp_32x32_avx2);
        p.chroma[X265_CSP_I420].cu[BLOCK_420_32x32].copy_sp = PFX(blockcopy_sp_32x32_avx2);
        p.chroma[X265_CSP_I422].cu[BLOCK_422_32x64].copy_sp = PFX(blockcopy_sp_32x64_avx2);

        p.cu[BLOCK_64x64].copy_sp = PFX(blockcopy_sp_64x64_avx2);

        // copy_ps primitives
        p.cu[BLOCK_16x16].copy_ps = PFX(blockcopy_ps_16x16_avx2);
        p.chroma[X265_CSP_I420].cu[CHROMA_420_16x16].copy_ps = PFX(blockcopy_ps_16x16_avx2);
        p.chroma[X265_CSP_I422].cu[CHROMA_422_16x32].copy_ps = PFX(blockcopy_ps_16x32_avx2);

        p.cu[BLOCK_32x32].copy_ps = PFX(blockcopy_ps_32x32_avx2);
        p.chroma[X265_CSP_I420].cu[CHROMA_420_32x32].copy_ps = PFX(blockcopy_ps_32x32_avx2);
        p.chroma[X265_CSP_I422].cu[CHROMA_422_32x64].copy_ps = PFX(blockcopy_ps_32x64_avx2);

        p.cu[BLOCK_64x64].copy_ps = PFX(blockcopy_ps_64x64_avx2);

        ALL_LUMA_TU_S(dct, dct, avx2);
        ALL_LUMA_TU_S(idct, idct, avx2);
        ALL_LUMA_CU_S(transpose, transpose, avx2);

        p.frameInitLowres = PFX(frame_init_lowres_core_avx2);
        p.frameInitLowerRes = PFX(frame_init_lowres_core_avx2);

        p.frameSubSampleLuma = PFX(frame_subsample_luma_avx2);

        p.cu[BLOCK_32x32].copy_ps = PFX(blockcopy_ps_32x32_avx2);
        p.chroma[X265_CSP_I420].cu[CHROMA_420_32x32].copy_ps = PFX(blockcopy_ps_32x32_avx2);
        p.chroma[X265_CSP_I422].cu[CHROMA_422_32x64].copy_ps = PFX(blockcopy_ps_32x64_avx2);
        p.cu[BLOCK_64x64].copy_ps = PFX(blockcopy_ps_64x64_avx2);

        p.pu[LUMA_32x8].sad_x3 = PFX(pixel_sad_x3_32x8_avx2);
        p.pu[LUMA_32x16].sad_x3 = PFX(pixel_sad_x3_32x16_avx2);
        p.pu[LUMA_32x24].sad_x3 = PFX(pixel_sad_x3_32x24_avx2);
        p.pu[LUMA_32x32].sad_x3 = PFX(pixel_sad_x3_32x32_avx2);
        p.pu[LUMA_32x64].sad_x3 = PFX(pixel_sad_x3_32x64_avx2);
        p.pu[LUMA_64x16].sad_x3 = PFX(pixel_sad_x3_64x16_avx2);
        p.pu[LUMA_64x32].sad_x3 = PFX(pixel_sad_x3_64x32_avx2);
        p.pu[LUMA_64x48].sad_x3 = PFX(pixel_sad_x3_64x48_avx2);
        p.pu[LUMA_64x64].sad_x3 = PFX(pixel_sad_x3_64x64_avx2);
        p.pu[LUMA_48x64].sad_x3 = PFX(pixel_sad_x3_48x64_avx2);

    }
    if (cpuMask & X265_CPU_AVX512)
    {
        p.pu[LUMA_32x8].sad = PFX(pixel_sad_32x8_avx512);
      //  p.pu[LUMA_32x16].sad = PFX(pixel_sad_32x16_avx512);
        p.pu[LUMA_32x24].sad = PFX(pixel_sad_32x24_avx512);
        p.pu[LUMA_32x32].sad = PFX(pixel_sad_32x32_avx512);
        //p.pu[LUMA_32x64].sad = PFX(pixel_sad_32x64_avx512);
        p.pu[LUMA_64x16].sad = PFX(pixel_sad_64x16_avx512);
        p.pu[LUMA_64x32].sad = PFX(pixel_sad_64x32_avx512);
        p.pu[LUMA_64x48].sad = PFX(pixel_sad_64x48_avx512);
        p.pu[LUMA_64x64].sad = PFX(pixel_sad_64x64_avx512);

        p.pu[LUMA_32x8].sad_x3 = PFX(pixel_sad_x3_32x8_avx512);
        p.pu[LUMA_32x16].sad_x3 = PFX(pixel_sad_x3_32x16_avx512);
        p.pu[LUMA_32x24].sad_x3 = PFX(pixel_sad_x3_32x24_avx512);
        p.pu[LUMA_32x32].sad_x3 = PFX(pixel_sad_x3_32x32_avx512);
        p.pu[LUMA_32x64].sad_x3 = PFX(pixel_sad_x3_32x64_avx512);
        p.pu[LUMA_64x16].sad_x3 = PFX(pixel_sad_x3_64x16_avx512);
        p.pu[LUMA_64x32].sad_x3 = PFX(pixel_sad_x3_64x32_avx512);
        p.pu[LUMA_64x48].sad_x3 = PFX(pixel_sad_x3_64x48_avx512);
        p.pu[LUMA_64x64].sad_x3 = PFX(pixel_sad_x3_64x64_avx512);
        p.pu[LUMA_48x64].sad_x3 = PFX(pixel_sad_x3_48x64_avx512);

        p.pu[LUMA_32x32].sad_x4 = PFX(pixel_sad_x4_32x32_avx512);
        p.pu[LUMA_32x16].sad_x4 = PFX(pixel_sad_x4_32x16_avx512);
        p.pu[LUMA_32x64].sad_x4 = PFX(pixel_sad_x4_32x64_avx512);
        p.pu[LUMA_32x24].sad_x4 = PFX(pixel_sad_x4_32x24_avx512);
        p.pu[LUMA_32x8].sad_x4 = PFX(pixel_sad_x4_32x8_avx512);
        p.pu[LUMA_64x16].sad_x4 = PFX(pixel_sad_x4_64x16_avx512);
        p.pu[LUMA_64x32].sad_x4 = PFX(pixel_sad_x4_64x32_avx512);
        p.pu[LUMA_64x48].sad_x4 = PFX(pixel_sad_x4_64x48_avx512);
        p.pu[LUMA_64x64].sad_x4 = PFX(pixel_sad_x4_64x64_avx512);
        p.pu[LUMA_48x64].sad_x4 = PFX(pixel_sad_x4_48x64_avx512);

        p.pu[LUMA_4x4].satd = PFX(pixel_satd_4x4_avx512);
        p.pu[LUMA_4x8].satd = PFX(pixel_satd_4x8_avx512);
        p.pu[LUMA_4x16].satd = PFX(pixel_satd_4x16_avx512);
        p.pu[LUMA_8x4].satd = PFX(pixel_satd_8x4_avx512);
        p.pu[LUMA_8x8].satd = PFX(pixel_satd_8x8_avx512);
        p.pu[LUMA_8x16].satd = PFX(pixel_satd_8x16_avx512);
        p.pu[LUMA_16x8].satd = PFX(pixel_satd_16x8_avx512);
        p.pu[LUMA_16x16].satd = PFX(pixel_satd_16x16_avx512);
        p.chroma[X265_CSP_I422].pu[CHROMA_422_4x4].satd = PFX(pixel_satd_4x4_avx512);
        p.chroma[X265_CSP_I422].pu[CHROMA_422_4x8].satd = PFX(pixel_satd_4x8_avx512);
        p.chroma[X265_CSP_I422].pu[CHROMA_422_4x16].satd = PFX(pixel_satd_4x16_avx512);
        p.chroma[X265_CSP_I422].pu[CHROMA_422_8x4].satd = PFX(pixel_satd_8x4_avx512);
        p.chroma[X265_CSP_I422].pu[CHROMA_422_8x8].satd = PFX(pixel_satd_8x8_avx512);
        p.chroma[X265_CSP_I422].pu[CHROMA_422_8x16].satd = PFX(pixel_satd_8x16_avx512);
        p.chroma[X265_CSP_I422].pu[CHROMA_422_16x8].satd = PFX(pixel_satd_16x8_avx512);
        p.chroma[X265_CSP_I422].pu[CHROMA_422_16x16].satd = PFX(pixel_satd_16x16_avx512);

        p.chroma[X265_CSP_I420].pu[CHROMA_420_4x4].satd = PFX(pixel_satd_4x4_avx512);
        p.chroma[X265_CSP_I420].pu[CHROMA_420_4x8].satd = PFX(pixel_satd_4x8_avx512);
        p.chroma[X265_CSP_I420].pu[CHROMA_420_4x16].satd = PFX(pixel_satd_4x16_avx512);
        p.chroma[X265_CSP_I420].pu[CHROMA_420_8x4].satd = PFX(pixel_satd_8x4_avx512);
        p.chroma[X265_CSP_I420].pu[CHROMA_420_8x8].satd = PFX(pixel_satd_8x8_avx512);
        p.chroma[X265_CSP_I420].pu[CHROMA_420_8x16].satd = PFX(pixel_satd_8x16_avx512);
        p.chroma[X265_CSP_I420].pu[CHROMA_420_16x8].satd = PFX(pixel_satd_16x8_avx512);
        p.chroma[X265_CSP_I420].pu[CHROMA_420_16x16].satd = PFX(pixel_satd_16x16_avx512);

        p.cu[BLOCK_8x8].sa8d = PFX(pixel_sa8d_8x8_avx512);
        p.chroma[X265_CSP_I420].cu[BLOCK_420_8x8].sa8d = PFX(pixel_sa8d_8x8_avx512);

        p.cu[BLOCK_8x8].var = PFX(pixel_var_8x8_avx512);
        p.cu[BLOCK_16x16].var = PFX(pixel_var_16x16_avx512);
        p.cu[BLOCK_32x32].var = PFX(pixel_var_32x32_avx512);
        p.cu[BLOCK_64x64].var = PFX(pixel_var_64x64_avx512);
        ASSIGN2(p.pu[LUMA_16x64].pixelavg_pp, pixel_avg_16x64_avx512);
        ASSIGN2(p.pu[LUMA_16x32].pixelavg_pp, pixel_avg_16x32_avx512);
        ASSIGN2(p.pu[LUMA_16x16].pixelavg_pp, pixel_avg_16x16_avx512);
        ASSIGN2(p.pu[LUMA_16x12].pixelavg_pp, pixel_avg_16x12_avx512);
        ASSIGN2(p.pu[LUMA_16x8].pixelavg_pp, pixel_avg_16x8_avx512);
        ASSIGN2(p.pu[LUMA_16x4].pixelavg_pp, pixel_avg_16x4_avx512);
        ASSIGN2(p.pu[LUMA_8x32].pixelavg_pp, pixel_avg_8x32_avx512);
        ASSIGN2(p.pu[LUMA_8x16].pixelavg_pp, pixel_avg_8x16_avx512);
        ASSIGN2(p.pu[LUMA_8x8].pixelavg_pp, pixel_avg_8x8_avx512);
        //p.pu[LUMA_8x4].pixelavg_pp = PFX(pixel_avg_8x4_avx512);
        p.pu[LUMA_4x4].sad = PFX(pixel_sad_4x4_avx512);
        p.pu[LUMA_4x8].sad = PFX(pixel_sad_4x8_avx512);
        p.pu[LUMA_4x16].sad = PFX(pixel_sad_4x16_avx512);
        p.pu[LUMA_8x4].sad = PFX(pixel_sad_8x4_avx512);
        p.pu[LUMA_8x8].sad = PFX(pixel_sad_8x8_avx512);
       // p.pu[LUMA_8x16].sad = PFX(pixel_sad_8x16_avx512);
        p.pu[LUMA_16x8].sad = PFX(pixel_sad_16x8_avx512);
        p.pu[LUMA_16x16].sad = PFX(pixel_sad_16x16_avx512);

        p.pu[LUMA_64x64].copy_pp = PFX(blockcopy_pp_64x64_avx512);
        p.pu[LUMA_64x32].copy_pp = PFX(blockcopy_pp_64x32_avx512);
        p.pu[LUMA_64x48].copy_pp = PFX(blockcopy_pp_64x48_avx512);
        p.pu[LUMA_64x16].copy_pp = PFX(blockcopy_pp_64x16_avx512);
        p.pu[LUMA_32x16].copy_pp = PFX(blockcopy_pp_32x16_avx512);
        p.pu[LUMA_32x24].copy_pp = PFX(blockcopy_pp_32x24_avx512);
        p.pu[LUMA_32x32].copy_pp  = PFX(blockcopy_pp_32x32_avx512);
        p.pu[LUMA_32x64].copy_pp = PFX(blockcopy_pp_32x64_avx512);

        p.chroma[X265_CSP_I420].pu[CHROMA_420_32x16].copy_pp = PFX(blockcopy_pp_32x16_avx512);
        p.chroma[X265_CSP_I420].pu[CHROMA_420_32x24].copy_pp = PFX(blockcopy_pp_32x24_avx512);
        p.chroma[X265_CSP_I420].pu[CHROMA_420_32x32].copy_pp = PFX(blockcopy_pp_32x32_avx512);
        p.chroma[X265_CSP_I422].pu[CHROMA_422_32x16].copy_pp = PFX(blockcopy_pp_32x16_avx512);
        p.chroma[X265_CSP_I422].pu[CHROMA_422_32x32].copy_pp = PFX(blockcopy_pp_32x32_avx512);
        p.chroma[X265_CSP_I422].pu[CHROMA_422_32x48].copy_pp = PFX(blockcopy_pp_32x48_avx512);
        p.chroma[X265_CSP_I422].pu[CHROMA_422_32x64].copy_pp = PFX(blockcopy_pp_32x64_avx512);

        p.cu[BLOCK_64x64].copy_sp = PFX(blockcopy_sp_64x64_avx512);
        p.cu[BLOCK_32x32].copy_sp = PFX(blockcopy_sp_32x32_avx512);
        p.chroma[X265_CSP_I420].cu[BLOCK_420_32x32].copy_sp = PFX(blockcopy_sp_32x32_avx512);
        p.chroma[X265_CSP_I422].cu[BLOCK_422_32x64].copy_sp = PFX(blockcopy_sp_32x64_avx512);

        p.cu[BLOCK_32x32].copy_ps = PFX(blockcopy_ps_32x32_avx512);
        p.chroma[X265_CSP_I420].cu[CHROMA_420_32x32].copy_ps = PFX(blockcopy_ps_32x32_avx512);
        p.chroma[X265_CSP_I422].cu[CHROMA_422_32x64].copy_ps = PFX(blockcopy_ps_32x64_avx512);
        p.cu[BLOCK_64x64].copy_ps = PFX(blockcopy_ps_64x64_avx512);

        p.pu[LUMA_64x16].addAvg[NONALIGNED] = PFX(addAvg_64x16_avx512);
        p.pu[LUMA_64x32].addAvg[NONALIGNED] = PFX(addAvg_64x32_avx512);
        p.pu[LUMA_64x48].addAvg[NONALIGNED] = PFX(addAvg_64x48_avx512);
        p.pu[LUMA_64x64].addAvg[NONALIGNED] = PFX(addAvg_64x64_avx512);
        p.pu[LUMA_32x8].addAvg[NONALIGNED] = PFX(addAvg_32x8_avx512);
        p.pu[LUMA_32x16].addAvg[NONALIGNED] = PFX(addAvg_32x16_avx512);
        p.pu[LUMA_32x24].addAvg[NONALIGNED] = PFX(addAvg_32x24_avx512);
        p.pu[LUMA_32x32].addAvg[NONALIGNED] = PFX(addAvg_32x32_avx512);
        p.pu[LUMA_32x64].addAvg[NONALIGNED] = PFX(addAvg_32x64_avx512);
        p.chroma[X265_CSP_I420].pu[CHROMA_420_32x8].addAvg[NONALIGNED] = PFX(addAvg_32x8_avx512);
        p.chroma[X265_CSP_I420].pu[CHROMA_420_32x16].addAvg[NONALIGNED] = PFX(addAvg_32x16_avx512);
        p.chroma[X265_CSP_I420].pu[CHROMA_420_32x24].addAvg[NONALIGNED] = PFX(addAvg_32x24_avx512);
        p.chroma[X265_CSP_I420].pu[CHROMA_420_32x32].addAvg[NONALIGNED] = PFX(addAvg_32x32_avx512);
        p.chroma[X265_CSP_I422].pu[CHROMA_422_32x16].addAvg[NONALIGNED] = PFX(addAvg_32x16_avx512);
        p.chroma[X265_CSP_I422].pu[CHROMA_422_32x48].addAvg[NONALIGNED] = PFX(addAvg_32x48_avx512);
        p.chroma[X265_CSP_I422].pu[CHROMA_422_32x64].addAvg[NONALIGNED] = PFX(addAvg_32x64_avx512);
        p.chroma[X265_CSP_I422].pu[CHROMA_422_32x32].addAvg[NONALIGNED] = PFX(addAvg_32x32_avx512);

        p.pu[LUMA_32x8].addAvg[ALIGNED] = PFX(addAvg_aligned_32x8_avx512);
        p.pu[LUMA_32x16].addAvg[ALIGNED] = PFX(addAvg_aligned_32x16_avx512);
        p.pu[LUMA_32x24].addAvg[ALIGNED] = PFX(addAvg_aligned_32x24_avx512);
        p.pu[LUMA_32x32].addAvg[ALIGNED] = PFX(addAvg_aligned_32x32_avx512);
        p.pu[LUMA_32x64].addAvg[ALIGNED] = PFX(addAvg_aligned_32x64_avx512);
        p.pu[LUMA_64x16].addAvg[ALIGNED] = PFX(addAvg_aligned_64x16_avx512);
        p.pu[LUMA_64x32].addAvg[ALIGNED] = PFX(addAvg_aligned_64x32_avx512);
        p.pu[LUMA_64x48].addAvg[ALIGNED] = PFX(addAvg_aligned_64x48_avx512);
        p.pu[LUMA_64x64].addAvg[ALIGNED] = PFX(addAvg_aligned_64x64_avx512);

        p.chroma[X265_CSP_I420].pu[CHROMA_420_32x8].addAvg[ALIGNED] = PFX(addAvg_aligned_32x8_avx512);
        p.chroma[X265_CSP_I420].pu[CHROMA_420_32x16].addAvg[ALIGNED] = PFX(addAvg_aligned_32x16_avx512);
        p.chroma[X265_CSP_I420].pu[CHROMA_420_32x24].addAvg[ALIGNED] = PFX(addAvg_aligned_32x24_avx512);
        p.chroma[X265_CSP_I420].pu[CHROMA_420_32x32].addAvg[ALIGNED] = PFX(addAvg_aligned_32x32_avx512);

        p.chroma[X265_CSP_I422].pu[CHROMA_422_32x16].addAvg[ALIGNED] = PFX(addAvg_aligned_32x16_avx512);
        p.chroma[X265_CSP_I422].pu[CHROMA_422_32x48].addAvg[ALIGNED] = PFX(addAvg_aligned_32x48_avx512);
        p.chroma[X265_CSP_I422].pu[CHROMA_422_32x64].addAvg[ALIGNED] = PFX(addAvg_aligned_32x64_avx512);
        p.chroma[X265_CSP_I422].pu[CHROMA_422_32x32].addAvg[ALIGNED] = PFX(addAvg_aligned_32x32_avx512);

        p.cu[BLOCK_32x32].blockfill_s[NONALIGNED] = PFX(blockfill_s_32x32_avx512);
        p.cu[BLOCK_32x32].blockfill_s[ALIGNED] = PFX(blockfill_s_aligned_32x32_avx512);

        p.cu[BLOCK_64x64].add_ps[NONALIGNED] = PFX(pixel_add_ps_64x64_avx512);
        p.cu[BLOCK_32x32].add_ps[NONALIGNED] = PFX(pixel_add_ps_32x32_avx512);
        p.chroma[X265_CSP_I420].cu[BLOCK_420_32x32].add_ps[NONALIGNED] = PFX(pixel_add_ps_32x32_avx512);
        p.chroma[X265_CSP_I422].cu[BLOCK_422_32x64].add_ps[NONALIGNED] = PFX(pixel_add_ps_32x64_avx512);

        p.cu[BLOCK_32x32].add_ps[ALIGNED] = PFX(pixel_add_ps_aligned_32x32_avx512);
        p.cu[BLOCK_64x64].add_ps[ALIGNED] = PFX(pixel_add_ps_aligned_64x64_avx512);
        p.chroma[X265_CSP_I420].cu[BLOCK_420_32x32].add_ps[ALIGNED] = PFX(pixel_add_ps_aligned_32x32_avx512);
        p.chroma[X265_CSP_I422].cu[BLOCK_422_32x64].add_ps[ALIGNED] = PFX(pixel_add_ps_aligned_32x64_avx512);

        p.cu[BLOCK_64x64].sub_ps = PFX(pixel_sub_ps_64x64_avx512);
        p.cu[BLOCK_32x32].sub_ps = PFX(pixel_sub_ps_32x32_avx512);
        p.chroma[X265_CSP_I420].cu[BLOCK_420_32x32].sub_ps = PFX(pixel_sub_ps_32x32_avx512);
        p.chroma[X265_CSP_I422].cu[BLOCK_422_32x64].sub_ps = PFX(pixel_sub_ps_32x64_avx512);

        p.cu[BLOCK_64x64].sse_ss = (pixel_sse_ss_t)PFX(pixel_ssd_ss_64x64_avx512);
        p.cu[BLOCK_32x32].sse_ss = (pixel_sse_ss_t)PFX(pixel_ssd_ss_32x32_avx512);
        p.cu[BLOCK_16x16].sse_ss = (pixel_sse_ss_t)PFX(pixel_ssd_ss_16x16_avx512);
        p.cu[BLOCK_32x32].ssd_s[NONALIGNED] = PFX(pixel_ssd_s_32_avx512);
        p.cu[BLOCK_32x32].ssd_s[ALIGNED] = PFX(pixel_ssd_s_32_avx512);
        p.cu[BLOCK_16x16].ssd_s[NONALIGNED] = PFX(pixel_ssd_s_16_avx512);
        p.cu[BLOCK_16x16].ssd_s[ALIGNED] = PFX(pixel_ssd_s_aligned_16_avx512);
        p.cu[BLOCK_32x32].copy_ss = PFX(blockcopy_ss_32x32_avx512);
        p.chroma[X265_CSP_I420].cu[CHROMA_420_32x32].copy_ss = PFX(blockcopy_ss_32x32_avx512);
        p.chroma[X265_CSP_I422].cu[CHROMA_422_32x64].copy_ss = PFX(blockcopy_ss_32x64_avx512);
        p.cu[BLOCK_64x64].copy_ss = PFX(blockcopy_ss_64x64_avx512);

        p.cu[BLOCK_32x32].calcresidual[NONALIGNED] = PFX(getResidual32_avx512);
        p.cu[BLOCK_32x32].calcresidual[ALIGNED] = PFX(getResidual_aligned32_avx512);
        p.cu[BLOCK_16x16].cpy2Dto1D_shl = PFX(cpy2Dto1D_shl_16_avx512);
        p.cu[BLOCK_32x32].cpy2Dto1D_shl = PFX(cpy2Dto1D_shl_32_avx512);
        p.cu[BLOCK_32x32].cpy1Dto2D_shl[NONALIGNED] = PFX(cpy1Dto2D_shl_32_avx512);
        p.cu[BLOCK_32x32].cpy1Dto2D_shl[ALIGNED] = PFX(cpy1Dto2D_shl_aligned_32_avx512);
        p.cu[BLOCK_16x16].cpy1Dto2D_shr = PFX(cpy1Dto2D_shr_16_avx512);
        p.cu[BLOCK_32x32].cpy1Dto2D_shr = PFX(cpy1Dto2D_shr_32_avx512);

        p.cu[BLOCK_16x16].cpy2Dto1D_shr = PFX(cpy2Dto1D_shr_16_avx512);
        p.cu[BLOCK_32x32].cpy2Dto1D_shr = PFX(cpy2Dto1D_shr_32_avx512);

        p.cu[BLOCK_32x32].copy_cnt = PFX(copy_cnt_32_avx512);
        p.cu[BLOCK_16x16].copy_cnt = PFX(copy_cnt_16_avx512);

        p.dequant_normal = PFX(dequant_normal_avx512);
        p.dequant_scaling = PFX(dequant_scaling_avx512);

        p.cu[BLOCK_8x8].dct    = PFX(dct8_avx512);
        /* TODO: Currently these kernels performance are similar to AVX2 version, we need a to improve them further to ebable
         * it. Probably a Vtune analysis will help here.

         * p.cu[BLOCK_16x16].dct  = PFX(dct16_avx512);
         * p.cu[BLOCK_32x32].dct  = PFX(dct32_avx512); */

        p.cu[BLOCK_8x8].idct   = PFX(idct8_avx512);
        p.cu[BLOCK_16x16].idct = PFX(idct16_avx512);
        p.cu[BLOCK_32x32].idct = PFX(idct32_avx512);
        p.quant = PFX(quant_avx512);
        p.nquant = PFX(nquant_avx512);
        p.denoiseDct = PFX(denoise_dct_avx512);

        p.pu[LUMA_32x8].satd = PFX(pixel_satd_32x8_avx512);
        p.pu[LUMA_32x16].satd = PFX(pixel_satd_32x16_avx512);
        p.pu[LUMA_32x24].satd = PFX(pixel_satd_32x24_avx512);
        p.pu[LUMA_32x32].satd = PFX(pixel_satd_32x32_avx512);
        p.pu[LUMA_32x64].satd = PFX(pixel_satd_32x64_avx512);
        p.pu[LUMA_64x16].satd = PFX(pixel_satd_64x16_avx512);
        p.pu[LUMA_64x32].satd = PFX(pixel_satd_64x32_avx512);
        p.pu[LUMA_64x48].satd = PFX(pixel_satd_64x48_avx512);
        p.pu[LUMA_64x64].satd = PFX(pixel_satd_64x64_avx512);
        p.chroma[X265_CSP_I420].pu[CHROMA_420_32x32].satd = PFX(pixel_satd_32x32_avx512);
        p.chroma[X265_CSP_I420].pu[CHROMA_420_32x16].satd = PFX(pixel_satd_32x16_avx512);
        p.chroma[X265_CSP_I420].pu[CHROMA_420_32x24].satd = PFX(pixel_satd_32x24_avx512);
        p.chroma[X265_CSP_I420].pu[CHROMA_420_32x8].satd = PFX(pixel_satd_32x8_avx512);
        p.chroma[X265_CSP_I422].pu[CHROMA_422_32x64].satd = PFX(pixel_satd_32x64_avx512);
        p.chroma[X265_CSP_I422].pu[CHROMA_422_32x48].satd = PFX(pixel_satd_32x48_avx512);
        p.chroma[X265_CSP_I422].pu[CHROMA_422_32x32].satd = PFX(pixel_satd_32x32_avx512);
        p.chroma[X265_CSP_I422].pu[CHROMA_422_32x16].satd = PFX(pixel_satd_32x16_avx512);
        p.planecopy_sp_shl = PFX(upShift_16_avx512);
        p.cu[BLOCK_16x16].count_nonzero = PFX(count_nonzero_16x16_avx512);
        p.cu[BLOCK_32x32].count_nonzero = PFX(count_nonzero_32x32_avx512);

    }
#endif
}
#endif // if HIGH_BIT_DEPTH

} // namespace X265_NS

extern "C" {
#ifdef __INTEL_COMPILER

/* Agner's patch to Intel's CPU dispatcher from pages 131-132 of
 * http://agner.org/optimize/optimizing_cpp.pdf (2011-01-30)
 * adapted to x265's cpu schema. */

// Global variable indicating cpu
int __intel_cpu_indicator = 0;
// CPU dispatcher function
void PFX(intel_cpu_indicator_init)(void)
{
    uint32_t cpu = x265::cpu_detect(false);

    if (cpu & X265_CPU_AVX)
        __intel_cpu_indicator = 0x20000;
    else if (cpu & X265_CPU_SSE42)
        __intel_cpu_indicator = 0x8000;
    else if (cpu & X265_CPU_SSE4)
        __intel_cpu_indicator = 0x2000;
    else if (cpu & X265_CPU_SSSE3)
        __intel_cpu_indicator = 0x1000;
    else if (cpu & X265_CPU_SSE3)
        __intel_cpu_indicator = 0x800;
    else if (cpu & X265_CPU_SSE2 && !(cpu & X265_CPU_SSE2_IS_SLOW))
        __intel_cpu_indicator = 0x200;
    else if (cpu & X265_CPU_SSE)
        __intel_cpu_indicator = 0x80;
    else if (cpu & X265_CPU_MMX2)
        __intel_cpu_indicator = 8;
    else
        __intel_cpu_indicator = 1;
}

/* __intel_cpu_indicator_init appears to have a non-standard calling convention that
 * assumes certain registers aren't preserved, so we'll route it through a function
 * that backs up all the registers. */
void __intel_cpu_indicator_init(void)
{
    x265_safe_intel_cpu_indicator_init();
}

#else // ifdef __INTEL_COMPILER
void PFX(intel_cpu_indicator_init)(void) {}

#endif // ifdef __INTEL_COMPILER
}
