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

#ifndef X265_H
#define X265_H
#include <stdint.h>
#include <stdio.h>
#include <sys/stat.h>
#include "x265_config.h"
#ifdef __cplusplus
extern "C" {
#endif

#if _MSC_VER
#pragma warning(disable: 4201) // non-standard extension used (nameless struct/union)
#endif

/* x265_encoder:
 *      opaque handler for encoder */
typedef struct x265_encoder x265_encoder;

/* x265_picyuv:
 *      opaque handler for PicYuv */
typedef struct x265_picyuv x265_picyuv;

/* Application developers planning to link against a shared library version of
 * libx265 from a Microsoft Visual Studio or similar development environment
 * will need to define X265_API_IMPORTS before including this header.
 * This clause does not apply to MinGW, similar development environments, or non
 * Windows platforms. */
#ifdef X265_API_IMPORTS
#define X265_API __declspec(dllimport)
#else
#define X265_API
#endif

static const char * const x265_source_csp_names[] = { "i400", "i420", "i422", "i444", "nv12", "nv16", 0 };

#define X265_LOOKAHEAD_MAX 250

#if X265_DEPTH < 10
typedef uint32_t sse_t;
#else
typedef uint64_t sse_t;
#endif

#define CTU_DISTORTION_OFF 0
#define CTU_DISTORTION_INTERNAL 1
#define CTU_DISTORTION_EXTERNAL 2

#define MAX_NUM_REF 16
#define EDGE_BINS 2
#define MAX_HIST_BINS 1024
#define X265_BFRAME_MAX         16

/* Arbitrary User SEI
 * Payload size is in bytes and the payload pointer must be non-NULL. 
 * Payload types and syntax can be found in Annex D of the H.265 Specification.
 * SEI Payload Alignment bits as described in Annex D must be included at the 
 * end of the payload if needed. The payload should not be NAL-encapsulated.
 * Payloads are written in the order of input */

typedef enum
{
    BUFFERING_PERIOD                     = 0,
    PICTURE_TIMING                       = 1,
    PAN_SCAN_RECT                        = 2,
    FILLER_PAYLOAD                       = 3,
    USER_DATA_REGISTERED_ITU_T_T35       = 4,
    USER_DATA_UNREGISTERED               = 5,
    RECOVERY_POINT                       = 6,
    SCENE_INFO                           = 9,
    FULL_FRAME_SNAPSHOT                  = 15,
    PROGRESSIVE_REFINEMENT_SEGMENT_START = 16,
    PROGRESSIVE_REFINEMENT_SEGMENT_END   = 17,
    FILM_GRAIN_CHARACTERISTICS           = 19,
    POST_FILTER_HINT                     = 22,
    TONE_MAPPING_INFO                    = 23,
    FRAME_PACKING                        = 45,
    DISPLAY_ORIENTATION                  = 47,
    SOP_DESCRIPTION                      = 128,
    ACTIVE_PARAMETER_SETS                = 129,
    DECODING_UNIT_INFO                   = 130,
    TEMPORAL_LEVEL0_INDEX                = 131,
    DECODED_PICTURE_HASH                 = 132,
    SCALABLE_NESTING                     = 133,
    REGION_REFRESH_INFO                  = 134,
    MASTERING_DISPLAY_INFO               = 137,
    CONTENT_LIGHT_LEVEL_INFO             = 144,
    ALTERNATIVE_TRANSFER_CHARACTERISTICS = 147,
} SEIPayloadType;

typedef struct x265_sei_payload
{
    int payloadSize;
    SEIPayloadType payloadType;
    uint8_t* payload;
} x265_sei_payload;

typedef struct x265_sei
{
    int numPayloads;
    x265_sei_payload *payloads;
} x265_sei;


typedef struct x265_FilmGrainCharacteristics
{
    struct CompModelIntensityValues
    {
        uint8_t intensityIntervalLowerBound;
        uint8_t intensityIntervalUpperBound;
        int*    compModelValue;
    };

    struct CompModel
    {
        bool    bPresentFlag;
        uint8_t numModelValues;
        uint8_t m_filmGrainNumIntensityIntervalMinus1;
        CompModelIntensityValues* intensityValues;
    };

    CompModel   *m_compModel[3];
    bool        m_filmGrainCharacteristicsPersistenceFlag;
    bool        m_filmGrainCharacteristicsCancelFlag;
    bool        m_separateColourDescriptionPresentFlag;
    bool        m_filmGrainFullRangeFlag;
    uint8_t     m_filmGrainModelId;
    uint8_t     m_blendingModeId;
    uint8_t     m_log2ScaleFactor;
    uint8_t     m_filmGrainBitDepthLumaMinus8;
    uint8_t     m_filmGrainBitDepthChromaMinus8;
    uint8_t     m_filmGrainColourPrimaries;
    uint8_t     m_filmGrainTransferCharacteristics;
    uint8_t     m_filmGrainMatrixCoeffs;

}x265_FilmGrainCharacteristics;

/* Used to pass pictures into the encoder, and to get picture data back out of
 * the encoder.  The input and output semantics are different */
typedef struct x265_picture
{

    /* Must be specified on input pictures, the number of planes is determined
     * by the colorSpace value */
    void*   planes[3];

    /* Stride is the number of bytes between row starts */
    int     stride[3];

    /* Must be specified on input pictures. x265_picture_init() will set it to
     * the encoder's internal bit depth, but this field must describe the depth
     * of the input pictures. Must be between 8 and 16. Values larger than 8
     * imply 16bits per input sample. If input bit depth is larger than the
     * internal bit depth, the encoder will down-shift pixels. Input samples
     * larger than 8bits will be masked to internal bit depth. On output the
     * bitDepth will be the internal encoder bit depth */
    int     bitDepth;

    /* Must be specified on input pictures: X265_TYPE_AUTO or other.
     * x265_picture_init() sets this to auto, returned on output */
    //int     sliceType;

    /* Ignored on input, set to picture count, returned on output */
    int     poc;

    /* Must be specified on input pictures: X265_CSP_I420 or other. It must
     * match the internal color space of the encoder. x265_picture_init() will
     * initialize this value to the internal color space */
    int     colorSpace;

    ///* Frame level statistics */
    //x265_frame_stats frameData;

    size_t framesize;

    int    height;

    int    width;

    x265_FilmGrainCharacteristics* m_fg;
} x265_picture;

typedef enum
{
    X265_DIA_SEARCH,
    X265_HEX_SEARCH,
    X265_UMH_SEARCH,
    X265_STAR_SEARCH,
    X265_SEA,
    X265_FULL_SEARCH
} X265_ME_METHODS;

/* CPU flags */

/* x86 */
#define X265_CPU_MMX             (1 << 0)
#define X265_CPU_MMX2            (1 << 1)  /* MMX2 aka MMXEXT aka ISSE */
#define X265_CPU_MMXEXT          X265_CPU_MMX2
#define X265_CPU_SSE             (1 << 2)
#define X265_CPU_SSE2            (1 << 3)
#define X265_CPU_LZCNT           (1 << 4)
#define X265_CPU_SSE3            (1 << 5)
#define X265_CPU_SSSE3           (1 << 6)
#define X265_CPU_SSE4            (1 << 7)  /* SSE4.1 */
#define X265_CPU_SSE42           (1 << 8)  /* SSE4.2 */
#define X265_CPU_AVX             (1 << 9)  /* Requires OS support even if YMM registers aren't used. */
#define X265_CPU_XOP             (1 << 10)  /* AMD XOP */
#define X265_CPU_FMA4            (1 << 11)  /* AMD FMA4 */
#define X265_CPU_FMA3            (1 << 12)  /* Intel FMA3 */
#define X265_CPU_BMI1            (1 << 13)  /* BMI1 */
#define X265_CPU_BMI2            (1 << 14)  /* BMI2 */
#define X265_CPU_AVX2            (1 << 15)  /* AVX2 */
#define X265_CPU_AVX512          (1 << 16)  /* AVX-512 {F, CD, BW, DQ, VL}, requires OS support */
/* x86 modifiers */
#define X265_CPU_CACHELINE_32    (1 << 17)  /* avoid memory loads that span the border between two cachelines */
#define X265_CPU_CACHELINE_64    (1 << 18)  /* 32/64 is the size of a cacheline in bytes */
#define X265_CPU_SSE2_IS_SLOW    (1 << 19)  /* avoid most SSE2 functions on Athlon64 */
#define X265_CPU_SSE2_IS_FAST    (1 << 20)  /* a few functions are only faster on Core2 and Phenom */
#define X265_CPU_SLOW_SHUFFLE    (1 << 21)  /* The Conroe has a slow shuffle unit (relative to overall SSE performance) */
#define X265_CPU_STACK_MOD4      (1 << 22)  /* if stack is only mod4 and not mod16 */
#define X265_CPU_SLOW_ATOM       (1 << 23)  /* The Atom is terrible: slow SSE unaligned loads, slow
                                             * SIMD multiplies, slow SIMD variable shifts, slow pshufb,
                                             * cacheline split penalties -- gather everything here that
                                             * isn't shared by other CPUs to avoid making half a dozen
                                             * new SLOW flags. */
#define X265_CPU_SLOW_PSHUFB     (1 << 24)  /* such as on the Intel Atom */
#define X265_CPU_SLOW_PALIGNR    (1 << 25)  /* such as on the AMD Bobcat */

/* ARM */
#define X265_CPU_ARMV6           0x0000001
#define X265_CPU_NEON            0x0000002  /* ARM NEON */
#define X265_CPU_FAST_NEON_MRC   0x0000004  /* Transfer from NEON to ARM register is fast (Cortex-A9) */

/* IBM Power8 */
#define X265_CPU_ALTIVEC         0x0000001

#define X265_MAX_SUBPEL_LEVEL   7

/* Log level */
#define X265_LOG_NONE          (-1)
#define X265_LOG_ERROR          0
#define X265_LOG_WARNING        1
#define X265_LOG_INFO           2
#define X265_LOG_DEBUG          3
#define X265_LOG_FULL           4

#define X265_MAX_FRAME_THREADS  16

#define X265_TYPE_AUTO          0x0000  /* Let x265 choose the right type */
#define X265_TYPE_IDR           0x0001
#define X265_TYPE_I             0x0002
#define X265_TYPE_P             0x0003
#define X265_TYPE_BREF          0x0004  /* Non-disposable B-frame */
#define X265_TYPE_B             0x0005
#define IS_X265_TYPE_I(x) ((x) == X265_TYPE_I || (x) == X265_TYPE_IDR)
#define IS_X265_TYPE_B(x) ((x) == X265_TYPE_B || (x) == X265_TYPE_BREF)

#define X265_QP_AUTO                 0

/* NOTE! For this release only X265_CSP_I420 and X265_CSP_I444 are supported */
/* Supported internal color space types (according to semantics of chroma_format_idc) */
#define X265_CSP_I400           0  /* yuv 4:0:0 planar */
#define X265_CSP_I420           1  /* yuv 4:2:0 planar */
#define X265_CSP_I422           2  /* yuv 4:2:2 planar */
#define X265_CSP_I444           3  /* yuv 4:4:4 planar */
#define X265_CSP_COUNT          4  /* Number of supported internal color spaces */

/* These color spaces will eventually be supported as input pictures. The pictures will
 * be converted to the appropriate planar color spaces at ingest */
#define X265_CSP_NV12           4  /* yuv 4:2:0, with one y plane and one packed u+v */
#define X265_CSP_NV16           5  /* yuv 4:2:2, with one y plane and one packed u+v */

/* Interleaved color-spaces may eventually be supported as input pictures */
#define X265_CSP_BGR            6  /* packed bgr 24bits   */
#define X265_CSP_BGRA           7  /* packed bgr 32bits   */
#define X265_CSP_RGB            8  /* packed rgb 24bits   */
#define X265_CSP_MAX            9  /* end of list */
#define X265_EXTENDED_SAR       255 /* aspect ratio explicitly specified as width:height */
/* Analysis options */
#define X265_ANALYSIS_OFF  0
#define X265_ANALYSIS_SAVE 1
#define X265_ANALYSIS_LOAD 2

#define FORWARD                 1
#define BACKWARD                2
#define BI_DIRECTIONAL          3
#define SLICE_TYPE_DELTA        0.3 /* The offset decremented or incremented for P-frames or b-frames respectively*/
#define BACKWARD_WINDOW         1 /* Scenecut window before a scenecut */
#define FORWARD_WINDOW          2 /* Scenecut window after a scenecut */
#define BWD_WINDOW_DELTA        0.4

#define X265_MAX_GOP_CONFIG 3
#define X265_MAX_GOP_LENGTH 16
#define MAX_T_LAYERS 7

typedef struct x265_cli_csp
{
    int planes;
    int width[3];
    int height[3];
} x265_cli_csp;

static const x265_cli_csp x265_cli_csps[] =
{
    { 1, { 0, 0, 0 }, { 0, 0, 0 } }, /* i400 */
    { 3, { 0, 1, 1 }, { 0, 1, 1 } }, /* i420 */
    { 3, { 0, 1, 1 }, { 0, 0, 0 } }, /* i422 */
    { 3, { 0, 0, 0 }, { 0, 0, 0 } }, /* i444 */
    { 2, { 0, 0 },    { 0, 1 } },    /* nv12 */
    { 2, { 0, 0 },    { 0, 0 } },    /* nv16 */
};

/* x265 input parameters
 *
 * For version safety you may use x265_param_alloc/free() to manage the
 * allocation of x265_param instances, and x265_param_parse() to assign values
 * by name.  By never dereferencing param fields in your own code you can treat
 * x265_param as an opaque data structure */
typedef struct x265_param
{
    /* x265_param_default() will auto-detect this cpu capability bitmap.  it is
     * recommended to not change this value unless you know the cpu detection is
     * somehow flawed on your target hardware. The asm function tables are
     * process global, the first encoder configures them for all encoders */
    int       cpuid;
    /*== Parallelism Features ==*/

    /* Number of concurrently encoded frames between 1 and X265_MAX_FRAME_THREADS
     * or 0 for auto-detection. By default x265 will use a number of frame
     * threads empirically determined to be optimal for your CPU core count,
     * between 2 and 6.  Using more than one frame thread causes motion search
     * in the down direction to be clamped but otherwise encode behavior is
     * unaffected. With CQP rate control the output bitstream is deterministic
     * for all values of frameNumThreads greater than 1. All other forms of
     * rate-control can be negatively impacted by increases to the number of
     * frame threads because the extra concurrency adds uncertainty to the
     * bitrate estimations. Frame parallelism is generally limited by the the
     * is generally limited by the the number of CU rows
     *
     * When thread pools are used, each frame thread is assigned to a single
     * pool and the frame thread itself is given the node affinity of its pool.
     * But when no thread pools are used no node affinity is assigned. */
    int       frameNumThreads;

    /* Comma seperated list of threads per NUMA node. If "none", then no worker
     * pools are created and only frame parallelism is possible. If NULL or ""
     * (default) x265 will use all available threads on each NUMA node.
     *
     * '+'  is a special value indicating all cores detected on the node
     * '*'  is a special value indicating all cores detected on the node and all
     *      remaining nodes.
     * '-'  is a special value indicating no cores on the node, same as '0'
     *
     * example strings for a 4-node system:
     *   ""        - default, unspecified, all numa nodes are used for thread pools
     *   "*"       - same as default
     *   "none"    - no thread pools are created, only frame parallelism possible
     *   "-"       - same as "none"
     *   "10"      - allocate one pool, using up to 10 cores on node 0
     *   "-,+"     - allocate one pool, using all cores on node 1
     *   "+,-,+"   - allocate two pools, using all cores on nodes 0 and 2
     *   "+,-,+,-" - allocate two pools, using all cores on nodes 0 and 2
     *   "-,*"     - allocate three pools, using all cores on nodes 1, 2 and 3
     *   "8,8,8,8" - allocate four pools with up to 8 threads in each pool
     *
     * The total number of threads will be determined by the number of threads
     * assigned to all nodes. The worker threads will each be given affinity for
     * their node, they will not be allowed to migrate between nodes, but they
     * will be allowed to move between CPU cores within their node.
     *
     * If the three pool features: bEnableWavefront, bDistributeModeAnalysis and
     * bDistributeMotionEstimation are all disabled, then numaPools is ignored
     * and no thread pools are created.
     *
     * If "none" is specified, then all three of the thread pool features are
     * implicitly disabled.
     *
     * Multiple thread pools will be allocated for any NUMA node with more than
     * 64 logical CPU cores. But any given thread pool will always use at most
     * one NUMA node.
     *
     * Frame encoders are distributed between the available thread pools, and
     * the encoder will never generate more thread pools than frameNumThreads */
    const char* numaPools;

    /* Enable wavefront parallel processing, greatly increases parallelism for
     * less than 1% compression efficiency loss. Requires a thread pool, enabled
     * by default */
    int       bEnableWavefront;


    /*== Internal Picture Specification ==*/

    /* Internal encoder bit depth. If x265 was compiled to use 8bit pixels
     * (HIGH_BIT_DEPTH=0), this field must be 8, else this field must be 10.
     * Future builds may support 12bit pixels. */
    int       internalBitDepth;

    /* Color space of internal pictures, must match color space of input
     * pictures */
    int       internalCsp;

    /* Numerator and denominator of frame rate */
    uint32_t  fpsNum;
    uint32_t  fpsDenom;

    /* Width (in pixels) of the source pictures. If this width is not an even
     * multiple of 4, the encoder will pad the pictures internally to meet this
     * minimum requirement. All valid HEVC widths are supported */
    int       sourceWidth;

    /* Height (in pixels) of the source pictures. If this height is not an even
     * multiple of 4, the encoder will pad the pictures internally to meet this
     * minimum requirement. All valid HEVC heights are supported */
    int       sourceHeight;

    /* Total Number of frames to be encoded, calculated from the user input
     * (--frames) and (--seek). In case, the input is read from a pipe, this can
     * remain as 0. It is later used in 2 pass RateControl, hence storing the
     * value in param */
    int       totalFrames;



    /* The number of frames that must be queued in the lookahead before it may
     * make slice decisions. Increasing this value directly increases the encode
     * latency. The longer the queue the more optimally the lookahead may make
     * slice decisions, particularly with b-adapt 2. When cu-tree is enabled,
     * the length of the queue linearly increases the effectiveness of the
     * cu-tree analysis. Default is 40 frames, maximum is 250 */
    int       lookaheadDepth;

    /* Use multiple worker threads to measure the estimated cost of each frame
     * within the lookahead. When bFrameAdaptive is 2, most frame cost estimates
     * will be performed in batch mode, many cost estimates at the same time,
     * and lookaheadSlices is ignored for batched estimates. The effect on
     * performance can be quite small.  The higher this parameter, the less
     * accurate the frame costs will be (since context is lost across slice
     * boundaries) which will result in less accurate B-frame and scene-cut
     * decisions. Default is 0 - disabled. 1 is the same as 0. Max 16 */
    int       lookaheadSlices;

    /*== Coding Unit (CU) definitions ==*/

    /* Maximum CU width and height in pixels.  The size must be 64, 32, or 16.
     * The higher the size, the more efficiently x265 can encode areas of low
     * complexity, greatly improving compression efficiency at large
     * resolutions.  The smaller the size, the more effective wavefront and
     * frame parallelism will become because of the increase in rows. default 64
     * All encoders within the same process must use the same maxCUSize, until
     * all encoders are closed and x265_cleanup() is called to reset the value. */
    uint32_t  maxCUSize;

    /* Minimum CU width and height in pixels.  The size must be 64, 32, 16, or
     * 8. Default 8. All encoders within the same process must use the same
     * minCUSize. */
    uint32_t  minCUSize;

    /*== Residual Quadtree Transform Unit (TU) definitions ==*/

    /* Maximum TU width and height in pixels.  The size must be 32, 16, 8 or 4.
     * The larger the size the more efficiently the residual can be compressed
     * by the DCT transforms, at the expense of more computation */
    uint32_t  maxTUSize;

    /* The additional depth the residual quad-tree is allowed to recurse beyond
     * the coding quad-tree, for inter coded blocks. This must be between 1 and
     * 4. The higher the value the more efficiently the residual can be
     * compressed by the DCT transforms, at the expense of much more compute */
    uint32_t  tuQTMaxInterDepth;

    /* The additional depth the residual quad-tree is allowed to recurse beyond
     * the coding quad-tree, for intra coded blocks. This must be between 1 and
     * 4. The higher the value the more efficiently the residual can be
     * compressed by the DCT transforms, at the expense of much more compute */
    uint32_t  tuQTMaxIntraDepth;

    /* Log of maximum CTU size */
    uint32_t  maxLog2CUSize;

    /* Actual CU depth with respect to config depth */
    uint32_t  maxCUDepth;

    /* CU depth with respect to maximum transform size */
    uint32_t  unitSizeDepth;

    /* Number of 4x4 units in maximum CU size */
    uint32_t  num4x4Partitions;

    /* Specify if analysis mode uses file for data reuse */
    int       bUseAnalysisFile;

    /* File pointer for csv log */
    FILE*     csvfpt;

    /* Disable lookahead */
    int       bDisableLookahead;

    /* Use low-pass subband dct approximation 
    *  This DCT approximation is less computational intensive and gives results close to standard DCT */
    int       bLowPassDct;

    /* Allow the encoder to have a copy of the planes of x265_picture in Frame */
    int       bCopyPicToFrame;

    
    /*Input sequence bit depth. It can be either 8bit, 10bit or 12bit.*/
    int       sourceBitDepth;

    /* Film Grain Characteristic file */
    char* filmGrain;

    /*Motion compensated temporal filter*/
    double   temporalFilterStrength;

    int qp;
} x265_param;

/* x265_param_alloc:
 *  Allocates an x265_param instance. The returned param structure is not
 *  special in any way, but using this method together with x265_param_free()
 *  and x265_param_parse() to set values by name allows the application to treat
 *  x265_param as an opaque data struct for version safety */
x265_param *x265_param_alloc(void);

/* x265_param_free:
 *  Use x265_param_free() to release storage for an x265_param instance
 *  allocated by x265_param_alloc() */
void x265_param_free(x265_param *);

/* x265_param_default:
 *  Initialize an x265_param structure to default values */
void x265_param_default(x265_param *param);

/* x265_param_parse:
 *  set one parameter by name.
 *  returns 0 on success, or returns one of the following errors.
 *  note: BAD_VALUE occurs only if it can't even parse the value,
 *  numerical range is not checked until x265_encoder_open().
 *  value=NULL means "true" for boolean options, but is a BAD_VALUE for non-booleans. */
#define X265_PARAM_BAD_NAME  (-1)
#define X265_PARAM_BAD_VALUE (-2)
int x265_param_parse(x265_param *p, const char *name, const char *value);


/* x265_param_apply_profile:
 *      Applies the restrictions of the given profile. (one of x265_profile_names)
 *      (can be NULL, in which case the function will do nothing)
 *      Note: the detected profile can be lower than the one specified to this
 *      function. This function will force the encoder parameters to fit within
 *      the specified profile, or fail if that is impossible.
 *      returns 0 on success, negative on failure (e.g. invalid profile name). */
int x265_param_apply_profile(x265_param *, const char *profile);

/* x265_picture_alloc:
 *  Allocates an x265_picture instance. The returned picture structure is not
 *  special in any way, but using this method together with x265_picture_free()
 *  and x265_picture_init() allows some version safety. New picture fields will
 *  always be added to the end of x265_picture */
x265_picture *x265_picture_alloc(void);

/* x265_picture_free:
 *  Use x265_picture_free() to release storage for an x265_picture instance
 *  allocated by x265_picture_alloc() */
void x265_picture_free(x265_picture *);

/* x265_picture_init:
 *       Initialize an x265_picture structure to default values. It sets the pixel
 *       depth and color space to the encoder's internal values and sets the slice
 *       type to auto - so the lookahead will determine slice type. */
void x265_picture_init(x265_param *param, x265_picture *pic);

/* x265_max_bit_depth:
 *      Specifies the numer of bits per pixel that x265 uses internally to
 *      represent a pixel, and the bit depth of the output bitstream.
 *      param->internalBitDepth must be set to this value. x265_max_bit_depth
 *      will be 8 for default builds, 10 for HIGH_BIT_DEPTH builds. */
X265_API extern const int x265_max_bit_depth;

/* x265_version_str:
 *      A static string containing the version of this compiled x265 library */
X265_API extern const char *x265_version_str;

/* x265_build_info:
 *      A static string describing the compiler and target architecture */
X265_API extern const char *x265_build_info_str;

/* Force a link error in the case of linking against an incompatible API version.
 * Glue #defines exist to force correct macro expansion; the final output of the macro
 * is x265_encoder_open_##X265_BUILD (for purposes of dlopen). */
#define x265_encoder_glue1(x, y) x ## y
#define x265_encoder_glue2(x, y) x265_encoder_glue1(x, y)
#define x265_encoder_open x265_encoder_glue2(x265_encoder_open_, X265_BUILD)

/* x265_encoder_open:
 *      create a new encoder handler, all parameters from x265_param are copied */
x265_encoder* x265_encoder_open(x265_param *);

/* x265_encoder_parameters:
 *      copies the current internal set of parameters to the pointer provided
 *      by the caller.  useful when the calling application needs to know
 *      how x265_encoder_open has changed the parameters.
 *      note that the data accessible through pointers in the returned param struct
 *      (e.g. filenames) should not be modified by the calling application. */
void x265_encoder_parameters(x265_encoder *, x265_param *);

/* x265_encoder_encode:
 *      encode one picture.
 *      *pi_nal is the number of NAL units outputted in pp_nal.
 *      returns negative on error, 1 if a picture and access unit were output,
 *      or zero if the encoder pipeline is still filling or is empty after flushing.
 *      the payloads of all output NALs are guaranteed to be sequential in memory.
 *      To flush the encoder and retrieve delayed output pictures, pass pic_in as NULL.
 *      Once flushing has begun, all subsequent calls must pass pic_in as NULL. */
int x265_encoder_encode(x265_encoder *encoder, x265_picture *pic_in, x265_picture *pic_denoised_in, x265_picture *pic_out);

/* x265_encoder_close:
 *      close an encoder handler */
void x265_encoder_close(x265_encoder *);

/* x265_cleanup:
 *       release library static allocations, reset configured CTU size */
void x265_cleanup(void);


/* In-place downshift from a bit-depth greater than 8 to a bit-depth of 8, using
 * the residual bits to dither each row. */
void x265_dither_image(x265_picture *, int picWidth, int picHeight, int16_t *errorBuf, int bitDepth);

#define X265_MAJOR_VERSION 1

/* === Multi-lib API ===
 * By using this method to gain access to the libx265 interfaces, you allow run-
 * time selection between various available libx265 libraries based on the
 * encoder parameters. The most likely use case is to choose between Main and
 * Main10 builds of libx265. */

typedef struct x265_api
{
    int           api_major_version;    /* X265_MAJOR_VERSION */
    int           api_build_number;     /* X265_BUILD (soname) */
    int           sizeof_param;         /* sizeof(x265_param) */
    int           sizeof_picture;       /* sizeof(x265_picture) */

    int           bit_depth;
    const char*   version_str;
    const char*   build_info_str;

    /* libx265 public API functions, documented above with x265_ prefixes */
    x265_param*   (*param_alloc)(void);
    void          (*param_free)(x265_param*);
    void          (*param_default)(x265_param*);
    int           (*param_parse)(x265_param*, const char*, const char*);
    x265_picture* (*picture_alloc)(void);
    void          (*picture_free)(x265_picture*);
    void          (*picture_init)(x265_param*, x265_picture*);
    x265_encoder* (*encoder_open)(x265_param*);
    void          (*encoder_parameters)(x265_encoder*, x265_param*);
    int           (*encoder_encode)(x265_encoder*, x265_picture*, x265_picture*, x265_picture*);
    void          (*encoder_close)(x265_encoder*);
    void          (*cleanup)(void);

    int           (*encoder_intra_refresh)(x265_encoder*);
    void          (*dither_image)(x265_picture*, int, int, int16_t*, int);
    /* add new pointers to the end, or increment X265_MAJOR_VERSION */
} x265_api;

/* Force a link error in the case of linking against an incompatible API version.
 * Glue #defines exist to force correct macro expansion; the final output of the macro
 * is x265_api_get_##X265_BUILD (for purposes of dlopen). */
#define x265_api_glue1(x, y) x ## y
#define x265_api_glue2(x, y) x265_api_glue1(x, y)
#define x265_api_get x265_api_glue2(x265_api_get_, X265_BUILD)

/* x265_api_get:
 *   Retrieve the programming interface for a linked x265 library.
 *   May return NULL if no library is available that supports the
 *   requested bit depth. If bitDepth is 0 the function is guarunteed
 *   to return a non-NULL x265_api pointer, from the linked libx265.
 *
 *   If the requested bitDepth is not supported by the linked libx265,
 *   it will attempt to dynamically bind x265_api_get() from a shared
 *   library with an appropriate name:
 *     8bit:  libx265_main.so
 *     10bit: libx265_main10.so
 *   Obviously the shared library file extension is platform specific */
const x265_api* x265_api_get(int bitDepth);

/* x265_api_query:
 *   Retrieve the programming interface for a linked x265 library, like
 *   x265_api_get(), except this function accepts X265_BUILD as the second
 *   argument rather than using the build number as part of the function name.
 *   Applications which dynamically link to libx265 can use this interface to
 *   query the library API and achieve a relative amount of version skew
 *   flexibility. The function may return NULL if the library determines that
 *   the apiVersion that your application was compiled against is not compatible
 *   with the library you have linked with.
 *
 *   api_major_version will be incremented any time non-backward compatible
 *   changes are made to any public structures or functions. If
 *   api_major_version does not match X265_MAJOR_VERSION from the x265.h your
 *   application compiled against, your application must not use the returned
 *   x265_api pointer.
 *
 *   Users of this API *must* also validate the sizes of any structures which
 *   are not treated as opaque in application code. For instance, if your
 *   application dereferences a x265_param pointer, then it must check that
 *   api->sizeof_param matches the sizeof(x265_param) that your application
 *   compiled with. */
const x265_api* x265_api_query(int bitDepth, int apiVersion, int* err);

#define X265_API_QUERY_ERR_NONE           0 /* returned API pointer is non-NULL */
#define X265_API_QUERY_ERR_VER_REFUSED    1 /* incompatible version skew        */
#define X265_API_QUERY_ERR_LIB_NOT_FOUND  2 /* libx265_main10 not found, for ex */
#define X265_API_QUERY_ERR_FUNC_NOT_FOUND 3 /* unable to bind x265_api_query    */
#define X265_API_QUERY_ERR_WRONG_BITDEPTH 4 /* libx265_main10 not 10bit, for ex */

static const char * const x265_api_query_errnames[] = {
    "api queried from libx265",
    "libx265 version is not compatible with this application",
    "unable to bind a libx265 with requested bit depth",
    "unable to bind x265_api_query from libx265",
    "libx265 has an invalid bitdepth"
};

#ifdef __cplusplus
}
#endif

#endif // X265_H
