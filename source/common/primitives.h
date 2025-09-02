/*****************************************************************************
 * Copyright (C) 2013-2020 MulticoreWare, Inc
 *
 * Authors: Steve Borho <steve@borho.org>
 *          Mandar Gurav <mandar@multicorewareinc.com>
 *          Deepthi Devaki Akkoorath <deepthidevaki@multicorewareinc.com>
 *          Mahesh Pittala <mahesh@multicorewareinc.com>
 *          Rajesh Paulraj <rajesh@multicorewareinc.com>
 *          Praveen Kumar Tiwari <praveen@multicorewareinc.com>
 *          Min Chen <chenm003@163.com>
 *          Hongbin Liu<liuhongbin1@huawei.com>
 *          Yimeng Su <yimeng.su@huawei.com>
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

#ifndef X265_PRIMITIVES_H
#define X265_PRIMITIVES_H

#include "common.h"
#include "cpu.h"

namespace X265_NS {
// x265 private namespace

enum LumaPU
{
    // Square (the first 5 PUs match the block sizes)
    LUMA_4x4,   LUMA_8x8,   LUMA_16x16, LUMA_32x32, LUMA_64x64,
    // Rectangular
    LUMA_8x4,   LUMA_4x8,
    LUMA_16x8,  LUMA_8x16,
    LUMA_32x16, LUMA_16x32,
    LUMA_64x32, LUMA_32x64,
    // Asymmetrical (0.75, 0.25)
    LUMA_16x12, LUMA_12x16, LUMA_16x4,  LUMA_4x16,
    LUMA_32x24, LUMA_24x32, LUMA_32x8,  LUMA_8x32,
    LUMA_64x48, LUMA_48x64, LUMA_64x16, LUMA_16x64,
    NUM_PU_SIZES
};

enum LumaCU // can be indexed using log2n(width)-2
{
    BLOCK_4x4,
    BLOCK_8x8,
    BLOCK_16x16,
    BLOCK_32x32,
    BLOCK_64x64,
    NUM_CU_SIZES
};

enum AlignPrimitive
{
    NONALIGNED,
    ALIGNED,
    NUM_ALIGNMENT_TYPES
};

enum { NUM_TR_SIZE = 4 }; // TU are 4x4, 8x8, 16x16, and 32x32


/* Chroma partition sizes. These enums are only a convenience for indexing into
 * the chroma primitive arrays when instantiating macros or templates. The
 * chroma function tables should always be indexed by a LumaPU enum when used. */
enum ChromaPU420
{
    CHROMA_420_2x2,   CHROMA_420_4x4,   CHROMA_420_8x8,  CHROMA_420_16x16, CHROMA_420_32x32,
    CHROMA_420_4x2,   CHROMA_420_2x4,
    CHROMA_420_8x4,   CHROMA_420_4x8,
    CHROMA_420_16x8,  CHROMA_420_8x16,
    CHROMA_420_32x16, CHROMA_420_16x32,
    CHROMA_420_8x6,   CHROMA_420_6x8,   CHROMA_420_8x2,  CHROMA_420_2x8,
    CHROMA_420_16x12, CHROMA_420_12x16, CHROMA_420_16x4, CHROMA_420_4x16,
    CHROMA_420_32x24, CHROMA_420_24x32, CHROMA_420_32x8, CHROMA_420_8x32,
};

enum ChromaCU420
{
    BLOCK_420_2x2,
    BLOCK_420_4x4,
    BLOCK_420_8x8,
    BLOCK_420_16x16,
    BLOCK_420_32x32
};

enum ChromaPU422
{
    CHROMA_422_2x4,   CHROMA_422_4x8,   CHROMA_422_8x16,  CHROMA_422_16x32, CHROMA_422_32x64,
    CHROMA_422_4x4,   CHROMA_422_2x8,
    CHROMA_422_8x8,   CHROMA_422_4x16,
    CHROMA_422_16x16, CHROMA_422_8x32,
    CHROMA_422_32x32, CHROMA_422_16x64,
    CHROMA_422_8x12,  CHROMA_422_6x16,  CHROMA_422_8x4,   CHROMA_422_2x16,
    CHROMA_422_16x24, CHROMA_422_12x32, CHROMA_422_16x8,  CHROMA_422_4x32,
    CHROMA_422_32x48, CHROMA_422_24x64, CHROMA_422_32x16, CHROMA_422_8x64,
};

enum ChromaCU422
{
    BLOCK_422_2x4,
    BLOCK_422_4x8,
    BLOCK_422_8x16,
    BLOCK_422_16x32,
    BLOCK_422_32x64
};

enum IntegralSize
{
    INTEGRAL_4,
    INTEGRAL_8,
    INTEGRAL_12,
    INTEGRAL_16,
    INTEGRAL_24,
    INTEGRAL_32,
    NUM_INTEGRAL_SIZE
};

typedef int  (*pixelcmp_t)(const pixel* fenc, intptr_t fencstride, const pixel* fref, intptr_t frefstride); // fenc is aligned
typedef int  (*pixelcmp_ss_t)(const int16_t* fenc, intptr_t fencstride, const int16_t* fref, intptr_t frefstride);
typedef sse_t (*pixel_sse_t)(const pixel* fenc, intptr_t fencstride, const pixel* fref, intptr_t frefstride); // fenc is aligned
typedef sse_t (*pixel_sse_ss_t)(const int16_t* fenc, intptr_t fencstride, const int16_t* fref, intptr_t frefstride);
typedef sse_t (*pixel_ssd_s_t)(const int16_t* fenc, intptr_t fencstride);
typedef int(*pixelcmp_ads_t)(int encDC[], uint32_t *sums, int delta, uint16_t *costMvX, int16_t *mvs, int width, int thresh);
typedef void (*pixelcmp_x4_t)(const pixel* fenc, const pixel* fref0, const pixel* fref1, const pixel* fref2, const pixel* fref3, intptr_t frefstride, int32_t* res);
typedef void (*pixelcmp_x3_t)(const pixel* fenc, const pixel* fref0, const pixel* fref1, const pixel* fref2, intptr_t frefstride, int32_t* res);
typedef void (*blockfill_s_t)(int16_t* dst, intptr_t dstride, int16_t val);

typedef void (*cpy2Dto1D_shl_t)(int16_t* dst, const int16_t* src, intptr_t srcStride, int shift);
typedef void (*cpy2Dto1D_shr_t)(int16_t* dst, const int16_t* src, intptr_t srcStride, int shift);
typedef void (*cpy1Dto2D_shl_t)(int16_t* dst, const int16_t* src, intptr_t dstStride, int shift);
typedef void (*cpy1Dto2D_shr_t)(int16_t* dst, const int16_t* src, intptr_t dstStride, int shift);
typedef uint32_t (*copy_cnt_t)(int16_t* coeff, const int16_t* residual, intptr_t resiStride);

typedef void (*dct_t)(const int16_t* src, int16_t* dst, intptr_t srcStride);
typedef void (*idct_t)(const int16_t* src, int16_t* dst, intptr_t dstStride);
typedef void (*denoiseDct_t)(int16_t* dctCoef, uint32_t* resSum, const uint16_t* offset, int numCoeff);

typedef void (*calcresidual_t)(const pixel* fenc, const pixel* pred, int16_t* residual, intptr_t stride);
typedef void (*transpose_t)(pixel* dst, const pixel* src, intptr_t stride);
typedef uint32_t (*quant_t)(const int16_t* coef, const int32_t* quantCoeff, int32_t* deltaU, int16_t* qCoef, int qBits, int add, int numCoeff);
typedef uint32_t (*nquant_t)(const int16_t* coef, const int32_t* quantCoeff, int16_t* qCoef, int qBits, int add, int numCoeff);
typedef void (*dequant_scaling_t)(const int16_t* src, const int32_t* dequantCoef, int16_t* dst, int num, int mcqp_miper, int shift);
typedef void (*dequant_normal_t)(const int16_t* quantCoef, int16_t* coef, int num, int scale, int shift);
typedef int(*count_nonzero_t)(const int16_t* quantCoeff);
typedef void (*weightp_pp_t)(const pixel* src, pixel* dst, intptr_t stride, int width, int height, int w0, int round, int shift, int offset);
typedef void (*weightp_sp_t)(const int16_t* src, pixel* dst, intptr_t srcStride, intptr_t dstStride, int width, int height, int w0, int round, int shift, int offset);
typedef void (*downscale_t)(const pixel* src0, pixel* dstf, pixel* dsth, pixel* dstv, pixel* dstc,
                            intptr_t src_stride, intptr_t dst_stride, int width, int height);
typedef void (*extendCURowBorder_t)(pixel* txt, intptr_t stride, int width, int height, int marginX);
typedef uint64_t (*var_t)(const pixel* pix, intptr_t stride);
typedef void (*plane_copy_deinterleave_t)(pixel* dstu, intptr_t dstuStride, pixel* dstv, intptr_t dstvStride, const pixel* src, intptr_t srcStride, int w, int h);

typedef void (*copy_pp_t)(pixel* dst, intptr_t dstStride, const pixel* src, intptr_t srcStride); // dst is aligned
typedef void (*copy_sp_t)(pixel* dst, intptr_t dstStride, const int16_t* src, intptr_t srcStride);
typedef void (*copy_ps_t)(int16_t* dst, intptr_t dstStride, const pixel* src, intptr_t srcStride);
typedef void (*copy_ss_t)(int16_t* dst, intptr_t dstStride, const int16_t* src, intptr_t srcStride);

typedef void (*pixel_sub_ps_t)(int16_t* dst, intptr_t dstride, const pixel* src0, const pixel* src1, intptr_t sstride0, intptr_t sstride1);
typedef void (*pixel_add_ps_t)(pixel* a, intptr_t dstride, const pixel* b0, const int16_t* b1, intptr_t sstride0, intptr_t sstride1);
typedef void (*pixelavg_pp_t)(pixel* dst, intptr_t dstride, const pixel* src0, intptr_t sstride0, const pixel* src1, intptr_t sstride1, int weight);
typedef void (*addAvg_t)(const int16_t* src0, const int16_t* src1, pixel* dst, intptr_t src0Stride, intptr_t src1Stride, intptr_t dstStride);

typedef void (*sign_t)(int8_t *dst, const pixel *src1, const pixel *src2, const int endX);
typedef void (*planecopy_cp_t) (const uint8_t* src, intptr_t srcStride, pixel* dst, intptr_t dstStride, int width, int height, int shift);
typedef void (*planecopy_sp_t) (const uint16_t* src, intptr_t srcStride, pixel* dst, intptr_t dstStride, int width, int height, int shift, uint16_t mask);
typedef void (*planecopy_pp_t) (const pixel* src, intptr_t srcStride, pixel* dst, intptr_t dstStride, int width, int height, int shift);
typedef pixel (*planeClipAndMax_t)(pixel *src, intptr_t stride, int width, int height, uint64_t *outsum, const pixel minPix, const pixel maxPix);

typedef void (*pelFilterLumaStrong_t)(pixel* src, intptr_t srcStep, intptr_t offset, int32_t tcP, int32_t tcQ);
typedef void (*pelFilterChroma_t)(pixel* src, intptr_t srcStep, intptr_t offset, int32_t tc, int32_t maskP, int32_t maskQ);

/* SubSampling Luma */
typedef void (*downscaleluma_t)(const pixel* src0, pixel* dstf, intptr_t src_stride, intptr_t dst_stride, int width, int height);
/* Function pointers to optimized encoder primitives. Each pointer can reference
 * either an assembly routine, a SIMD intrinsic primitive, or a C function */
struct EncoderPrimitives
{
    /* These primitives can be used for any sized prediction unit (from 4x4 to
     * 64x64, square, rectangular - 50/50 or asymmetrical - 25/75) and are
     * generally restricted to motion estimation and motion compensation (inter
     * prediction. Note that the 4x4 PU can only be used for intra, which is
     * really a 4x4 TU, so at most copy_pp and satd will use 4x4. This array is
     * indexed by LumaPU values, which can be retrieved by partitionFromSizes() */
    struct PU
    {
        pixelcmp_t     sad;         // Sum of Absolute Differences
        pixelcmp_x3_t  sad_x3;      // Sum of Absolute Differences, 3 mv offsets at once
        pixelcmp_x4_t  sad_x4;      // Sum of Absolute Differences, 4 mv offsets at once
        pixelcmp_ads_t ads;         // Absolute Differences sum
        pixelcmp_t     satd;        // Sum of Absolute Transformed Differences (4x4 Hadamard)

        pixelavg_pp_t  pixelavg_pp[NUM_ALIGNMENT_TYPES]; // quick bidir using pixels (borrowed from x264)
        addAvg_t       addAvg[NUM_ALIGNMENT_TYPES];      // bidir motion compensation, uses 16bit values
        copy_pp_t      copy_pp;

    }
    pu[NUM_PU_SIZES];

    /* These primitives can be used for square TU blocks (4x4 to 32x32) or
     * possibly square CU blocks (8x8 to 64x64). Some primitives are used for
     * both CU and TU so we merge them into one array that is indexed uniformly.
     * This keeps the index logic uniform and simple and improves cache
     * coherency. CU only primitives will leave 4x4 pointers NULL while TU only
     * primitives will leave 64x64 pointers NULL.  Indexed by LumaCU */
    struct CU
    {
        dct_t           dct;    // active dct transformation
        idct_t          idct;   // active idct transformation

        dct_t           standard_dct;   // original dct function, used by lowpass_dct
        dct_t           lowpass_dct;    // lowpass dct approximation

        calcresidual_t  calcresidual[NUM_ALIGNMENT_TYPES];
        pixel_sub_ps_t  sub_ps;
        pixel_add_ps_t  add_ps[NUM_ALIGNMENT_TYPES];
        blockfill_s_t   blockfill_s[NUM_ALIGNMENT_TYPES];   // block fill, for DC transforms
        copy_cnt_t      copy_cnt;      // copy coeff while counting non-zero
        count_nonzero_t count_nonzero;
        cpy2Dto1D_shl_t cpy2Dto1D_shl;
        cpy2Dto1D_shr_t cpy2Dto1D_shr;
        cpy1Dto2D_shl_t cpy1Dto2D_shl[NUM_ALIGNMENT_TYPES];
        cpy1Dto2D_shr_t cpy1Dto2D_shr;
        copy_sp_t       copy_sp;
        copy_ps_t       copy_ps;
        copy_ss_t       copy_ss;
        copy_pp_t       copy_pp;       // alias to pu[].copy_pp

        var_t           var;           // block internal variance

        pixel_sse_t     sse_pp;        // Sum of Square Error (pixel, pixel) fenc alignment not assumed
        pixel_sse_ss_t  sse_ss;        // Sum of Square Error (short, short) fenc alignment not assumed
        pixelcmp_t      psy_cost_pp;   // difference in AC energy between two pixel blocks
        pixel_ssd_s_t   ssd_s[NUM_ALIGNMENT_TYPES];         // Sum of Square Error (residual coeff to self)
        pixelcmp_t      sa8d;          // Sum of Transformed Differences (8x8 Hadamard), uses satd for 4x4 intra TU
        transpose_t     transpose;     // transpose pixel block; for use with intra all-angs

    }
    cu[NUM_CU_SIZES];
    /* These remaining primitives work on either fixed block sizes or take
     * block dimensions as arguments and thus do not belong in either the PU or
     * the CU arrays */
    dct_t                 dst4x4;
    idct_t                idst4x4;

    quant_t               quant;
    nquant_t              nquant;
    dequant_scaling_t     dequant_scaling;
    dequant_normal_t      dequant_normal;
    denoiseDct_t          denoiseDct;

    sign_t                sign;

    downscale_t           frameInitLowres;
    downscale_t           frameInitLowerRes;
    /* Sub Sample Luma */
    downscaleluma_t        frameSubSampleLuma;

    extendCURowBorder_t   extendRowBorder;
    planecopy_cp_t        planecopy_cp;
    planecopy_sp_t        planecopy_sp;
    planecopy_sp_t        planecopy_sp_shl;
    planecopy_pp_t        planecopy_pp_shr;
    planeClipAndMax_t     planeClipAndMax;

    weightp_sp_t          weight_sp;
    weightp_pp_t          weight_pp;

    pelFilterLumaStrong_t pelFilterLumaStrong[2]; // EDGE_VER = 0, EDGE_HOR = 1
    pelFilterChroma_t     pelFilterChroma[2];     // EDGE_VER = 0, EDGE_HOR = 1

    /* There is one set of chroma primitives per color space. An encoder will
     * have just a single color space and thus it will only ever use one entry
     * in this array. However we always fill all entries in the array in case
     * multiple encoders with different color spaces share the primitive table
     * in a single process. Note that 4:2:0 PU and CU are 1/2 width and 1/2
     * height of their luma counterparts. 4:2:2 PU and CU are 1/2 width and full
     * height, while 4:4:4 directly uses the luma block sizes and shares luma
     * primitives for all cases except for the interpolation filters. 4:4:4
     * interpolation filters have luma partition sizes but are only 4-tap. */
    struct Chroma
    {
        /* Chroma prediction unit primitives. Indexed by LumaPU */
        struct PUChroma
        {
            pixelcmp_t   satd;      // if chroma PU is not multiple of 4x4, will be NULL
            addAvg_t     addAvg[NUM_ALIGNMENT_TYPES];
            copy_pp_t    copy_pp;

        }
        pu[NUM_PU_SIZES];

        /* Chroma transform and coding unit primitives. Indexed by LumaCU */
        struct CUChroma
        {
            pixelcmp_t     sa8d;    // if chroma CU is not multiple of 8x8, will use satd
            pixel_sse_t    sse_pp;
            pixel_sub_ps_t sub_ps;
            pixel_add_ps_t add_ps[NUM_ALIGNMENT_TYPES];

            copy_ps_t      copy_ps;
            copy_sp_t      copy_sp;
            copy_ss_t      copy_ss;
            copy_pp_t      copy_pp;
        }
        cu[NUM_CU_SIZES];

    }
    chroma[X265_CSP_COUNT];
};

/* This copy of the table is what gets used by the encoder */
extern EncoderPrimitives primitives;

/* Returns a LumaPU enum for the given size, always expected to return a valid enum */
inline int partitionFromSizes(int width, int height)
{
    X265_CHECK(((width | height) & ~(4 | 8 | 16 | 32 | 64)) == 0, "Invalid block width/height\n");
    extern const uint8_t lumaPartitionMapTable[];
    int w = (width >> 2) - 1;
    int h = (height >> 2) - 1;
    int part = (int)lumaPartitionMapTable[(w << 4) + h];
    X265_CHECK(part != 255, "Invalid block width %d height %d\n", width, height);
    return part;
}

/* Computes the size of the LumaPU for a given LumaPU enum */
inline void sizesFromPartition(int part, int *width, int *height)
{
    X265_CHECK(part >= 0 && part <= 24, "Invalid part %d \n", part);
    extern const uint8_t lumaPartitionMapTable[];
    int index = 0;
    for (int i = 0; i < 256;i++)
        if (part == lumaPartitionMapTable[i])
        {
            index = i;
            break;
        }
    *width = 4 * ((index >> 4) + 1);
    *height = 4 * ((index % 16) + 1);
}

inline int partitionFromLog2Size(int log2Size)
{
    X265_CHECK(2 <= log2Size && log2Size <= 6, "Invalid block size\n");
    return log2Size - 2;
}

void setupCPrimitives(EncoderPrimitives &p);
void setupInstrinsicPrimitives(EncoderPrimitives &p, int cpuMask);
void setupAssemblyPrimitives(EncoderPrimitives &p, int cpuMask);
void setupFilterPrimitives(EncoderPrimitives &p);
void setupAliasPrimitives(EncoderPrimitives &p);
#if X265_ARCH_ARM64
void setupAliasCPrimitives(EncoderPrimitives &cp, EncoderPrimitives &asmp, int cpuMask);
#endif
#if HAVE_ALTIVEC
void setupPixelPrimitives_altivec(EncoderPrimitives &p);
void setupDCTPrimitives_altivec(EncoderPrimitives &p);
void setupFilterPrimitives_altivec(EncoderPrimitives &p);
void setupIntraPrimitives_altivec(EncoderPrimitives &p);
#endif
}

#if !EXPORT_C_API
extern const int   PFX(max_bit_depth);
extern const char* PFX(version_str);
extern const char* PFX(build_info_str);
#endif

#if ENABLE_ASSEMBLY && X265_ARCH_ARM64
extern "C" {
#include "aarch64/fun-decls.h"
}
#endif

#endif // ifndef X265_PRIMITIVES_H
