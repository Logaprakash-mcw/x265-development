/*****************************************************************************
 * Copyright (C) 2013-2020 MulticoreWare, Inc
 *
 * Authors: Deepthi Nandakumar <deepthi@multicorewareinc.com>
 *          Min Chen <min.chen@multicorewareinc.com>
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
#include "slice.h"
#include "threading.h"
#include "param.h"
#include "cpu.h"
#include "x265.h"

#if _MSC_VER
#pragma warning(disable: 4996) // POSIX functions are just fine, thanks
#pragma warning(disable: 4706) // assignment within conditional
#pragma warning(disable: 4127) // conditional expression is constant
#endif

#if _WIN32
#define strcasecmp _stricmp
#endif

#if !defined(HAVE_STRTOK_R)

/*
 * adapted from public domain strtok_r() by Charlie Gordon
 *
 *   from comp.lang.c  9/14/2007
 *
 *      http://groups.google.com/group/comp.lang.c/msg/2ab1ecbb86646684
 *
 *     (Declaration that it's public domain):
 *      http://groups.google.com/group/comp.lang.c/msg/7c7b39328fefab9c
 */

#undef strtok_r
static char* strtok_r(char* str, const char* delim, char** nextp)
{
    if (!str)
        str = *nextp;

    str += strspn(str, delim);

    if (!*str)
        return NULL;

    char *ret = str;

    str += strcspn(str, delim);

    if (*str)
        *str++ = '\0';

    *nextp = str;

    return ret;
}

#endif // if !defined(HAVE_STRTOK_R)

#if EXPORT_C_API

/* these functions are exported as C functions (default) */
using namespace X265_NS;
extern "C" {

#else

/* these functions exist within private namespace (multilib) */
namespace X265_NS {

#endif

x265_param *x265_param_alloc()
{
    x265_param* param = (x265_param*)x265_malloc(sizeof(x265_param));
    return param;
}

void x265_param_free(x265_param* p)
{
    x265_free(p);
}

void x265_param_default(x265_param* param)
{
    memset(param, 0, sizeof(x265_param));

    /* Applying default values to all elements in the param structure */
    param->cpuid = X265_NS::cpu_detect(false);
    param->bEnableWavefront = 1;
    param->frameNumThreads = 0;

    /* Source specifications */
    param->internalBitDepth = X265_DEPTH;
    param->sourceBitDepth = 8;
    param->internalCsp = X265_CSP_I420;


    /* CU definitions */
    param->maxCUSize = 64;
    param->minCUSize = 8;
    param->tuQTMaxInterDepth = 1;
    param->tuQTMaxIntraDepth = 1;
    param->maxTUSize = 32;

    param->lookaheadSlices = 8;

    param->bUseAnalysisFile = 1;
    param->csvfpt = NULL;
    param->bDisableLookahead = 1;
    param->bCopyPicToFrame = 1;
    param->qp = 30;

    /* DCT Approximations */
    param->bLowPassDct = 0;
    param->temporalFilterStrength = 0.95;
    /* Film grain characteristics model filename */
    param->filmGrain = (char *)"model.bin";
}

static int x265_atobool(const char* str, bool& bError)
{
    if (!strcmp(str, "1") ||
        !strcmp(str, "true") ||
        !strcmp(str, "yes"))
        return 1;
    if (!strcmp(str, "0") ||
        !strcmp(str, "false") ||
        !strcmp(str, "no"))
        return 0;
    bError = true;
    return 0;
}

static int parseName(const char* arg, const char* const* names, bool& bError)
{
    for (int i = 0; names[i]; i++)
        if (!strcmp(arg, names[i]))
            return i;

    return x265_atoi(arg, bError);
}

#define atoi(str) x265_atoi(str, bError)
#define atof(str) x265_atof(str, bError)
#define atobool(str) (bNameWasBool = true, x265_atobool(str, bError))

int x265_param_parse(x265_param* p, const char* name, const char* value)
{
    bool bError = false;
    bool bNameWasBool = false;
    bool bValueWasNull = !value;
    bool bExtraParams = false;
    char nameBuf[64];
    static int count;

    if (!name)
        return X265_PARAM_BAD_NAME;

    count++;
    // skip -- prefix if provided
    if (name[0] == '-' && name[1] == '-')
        name += 2;

    // s/_/-/g
    if (strlen(name) + 1 < sizeof(nameBuf) && strchr(name, '_'))
    {
        char *c;
        strcpy(nameBuf, name);
        while ((c = strchr(nameBuf, '_')) != 0)
            *c = '-';

        name = nameBuf;
    }

    if (!strncmp(name, "no-", 3))
    {
        name += 3;
        value = !value || x265_atobool(value, bError) ? "false" : "true";
    }
    else if (!strncmp(name, "no", 2))
    {
        name += 2;
        value = !value || x265_atobool(value, bError) ? "false" : "true";
    }
    else if (!value)
        value = "true";
    else if (value[0] == '=')
        value++;

#if defined(_MSC_VER)
#pragma warning(disable: 4127) // conditional expression is constant
#endif
#define OPT(STR) else if (!strcmp(name, STR))
#define OPT2(STR1, STR2) else if (!strcmp(name, STR1) || !strcmp(name, STR2))

    if (0) ;
    OPT("asm")
    {
#if X265_ARCH_X86
        if (!strcasecmp(value, "avx512"))
        {
            p->cpuid = X265_NS::cpu_detect(true);
            if (!(p->cpuid & X265_CPU_AVX512))
                x265_log( X265_LOG_WARNING, "AVX512 is not supported\n");
        }
        else
        {
            if (bValueWasNull)
                p->cpuid = atobool(value);
            else
                p->cpuid = parseCpuName(value, bError, false);
        }
#else
        if (bValueWasNull)
            p->cpuid = atobool(value);
        else
            p->cpuid = parseCpuName(value, bError, false);
#endif
    }
    OPT("fps")
    {
        if (sscanf(value, "%u/%u", &p->fpsNum, &p->fpsDenom) == 2)
            ;
        else
        {
            float fps = (float)atof(value);
            if (fps > 0 && fps <= INT_MAX / 1000)
            {
                p->fpsNum = (int)(fps * 1000 + .5);
                p->fpsDenom = 1000;
            }
            else
            {
                p->fpsNum = atoi(value);
                p->fpsDenom = 1;
            }
        }
    }
    OPT("frame-threads") p->frameNumThreads = atoi(value);


    OPT("input-res") bError |= sscanf(value, "%dx%d", &p->sourceWidth, &p->sourceHeight) != 2;
    OPT("input-csp") p->internalCsp = parseName(value, x265_source_csp_names, bError);


    OPT2("pools", "numa-pools") p->numaPools = strdup(value);
    OPT("qp") p->qp = atoi(value);

    else
        bExtraParams = true;

    // solve "fatal error C1061: compiler limit : blocks nested too deeply"
    if (bExtraParams)
    {
        if (0) ;
        OPT("film-grain") p->filmGrain = (char* )value;
        else
            return X265_PARAM_BAD_NAME;
    }
#undef OPT
#undef atobool
#undef atoi
#undef atof

    bError |= bValueWasNull && !bNameWasBool;
    return bError ? X265_PARAM_BAD_VALUE : 0;
}

} /* end extern "C" or namespace */

namespace X265_NS {
// internal encoder functions

int x265_atoi(const char* str, bool& bError)
{
    char *end;
    int v = strtol(str, &end, 0);

    if (end == str || *end != '\0')
        bError = true;
    return v;
}

double x265_atof(const char* str, bool& bError)
{
    char *end;
    double v = strtod(str, &end);

    if (end == str || *end != '\0')
        bError = true;
    return v;
}

/* cpu name can be:
 *   auto || true - x265::cpu_detect()
 *   false || no  - disabled
 *   integer bitmap value
 *   comma separated list of SIMD names, eg: SSE4.1,XOP */
int parseCpuName(const char* value, bool& bError, bool bEnableavx512)
{
    if (!value)
    {
        bError = 1;
        return 0;
    }
    int cpu;
    if (isdigit(value[0]))
        cpu = x265_atoi(value, bError);
    else
        cpu = !strcmp(value, "auto") || x265_atobool(value, bError) ? X265_NS::cpu_detect(bEnableavx512) : 0;

    if (bError)
    {
        char *buf = strdup(value);
        char *tok, *saveptr = NULL, *init;
        bError = 0;
        cpu = 0;
        for (init = buf; (tok = strtok_r(init, ",", &saveptr)); init = NULL)
        {
            int i;
            for (i = 0; X265_NS::cpu_names[i].flags && strcasecmp(tok, X265_NS::cpu_names[i].name); i++)
            {
            }

            cpu |= X265_NS::cpu_names[i].flags;
            if (!X265_NS::cpu_names[i].flags)
                bError = 1;
        }

        free(buf);
        if ((cpu & X265_CPU_SSSE3) && !(cpu & X265_CPU_SSE2_IS_SLOW))
            cpu |= X265_CPU_SSE2_IS_FAST;
    }

    return cpu;
}

static const int fixedRatios[][2] =
{
    { 1,  1 },
    { 12, 11 },
    { 10, 11 },
    { 16, 11 },
    { 40, 33 },
    { 24, 11 },
    { 20, 11 },
    { 32, 11 },
    { 80, 33 },
    { 18, 11 },
    { 15, 11 },
    { 64, 33 },
    { 160, 99 },
    { 4, 3 },
    { 3, 2 },
    { 2, 1 },
};


static inline int _confirm(bool bflag, const char* message)
{
    if (!bflag)
        return 0;

    x265_log(X265_LOG_ERROR, "%s\n", message);
    return 1;
}

int x265_check_params(x265_param* param)
{
#define CHECK(expr, msg) check_failed |= _confirm(expr, msg)
    int check_failed = 0; /* abort if there is a fatal configuration problem */

    if (check_failed == 1)
        return check_failed;

    uint32_t maxLog2CUSize = (uint32_t)g_log2Size[param->maxCUSize];
    uint32_t tuQTMaxLog2Size = X265_MIN(maxLog2CUSize, 5);
    uint32_t tuQTMinLog2Size = 2; //log2(4)

    CHECK(param->internalBitDepth != X265_DEPTH,
          "internalBitDepth must match compiled bit depth");
    CHECK(param->minCUSize != 32 && param->minCUSize != 16 && param->minCUSize != 8,
          "minimim CU size must be 8, 16 or 32");
    CHECK(param->minCUSize > param->maxCUSize,
          "min CU size must be less than or equal to max CU size");
    CHECK(param->qp < -6 * (param->internalBitDepth - 8) || param->qp > QP_MAX_SPEC,
          "QP exceeds supported range (-QpBDOffsety to 51)");
    CHECK(param->fpsNum == 0 || param->fpsDenom == 0,
          "Frame rate numerator and denominator must be specified");

    //CHECK(param->frameNumThreads != 1,
    //      "FrameThreads should always be 1");


    CHECK(tuQTMaxLog2Size > maxLog2CUSize,
          "QuadtreeTULog2MaxSize must be log2(maxCUSize) or smaller.");

    CHECK(param->tuQTMaxInterDepth < 1 || param->tuQTMaxInterDepth > 4,
          "QuadtreeTUMaxDepthInter must be greater than 0 and less than 5");
    CHECK(maxLog2CUSize < tuQTMinLog2Size + param->tuQTMaxInterDepth - 1,
          "QuadtreeTUMaxDepthInter must be less than or equal to the difference between log2(maxCUSize) and QuadtreeTULog2MinSize plus 1");
    CHECK(param->tuQTMaxIntraDepth < 1 || param->tuQTMaxIntraDepth > 4,
          "QuadtreeTUMaxDepthIntra must be greater 0 and less than 5");
    CHECK(maxLog2CUSize < tuQTMinLog2Size + param->tuQTMaxIntraDepth - 1,
          "QuadtreeTUMaxDepthInter must be less than or equal to the difference between log2(maxCUSize) and QuadtreeTULog2MinSize plus 1");
    CHECK((param->maxTUSize != 32 && param->maxTUSize != 16 && param->maxTUSize != 8 && param->maxTUSize != 4),
          "max TU size must be 4, 8, 16, or 32");

    CHECK(param->sourceWidth < (int)param->maxCUSize || param->sourceHeight < (int)param->maxCUSize,
          "Picture size must be at least one CTU");
    CHECK(param->internalCsp < X265_CSP_I400 || X265_CSP_I444 < param->internalCsp,
          "chroma subsampling must be i400 (4:0:0 monochrome), i420 (4:2:0 default), i422 (4:2:0), i444 (4:4:4)");
    CHECK(param->sourceWidth & !!CHROMA_H_SHIFT(param->internalCsp),
          "Picture width must be an integer multiple of the specified chroma subsampling");
    CHECK(param->sourceHeight & !!CHROMA_V_SHIFT(param->internalCsp),
          "Picture height must be an integer multiple of the specified chroma subsampling");
    CHECK(!param->filmGrain,
          "Filename to dump the film grain characteristics must be specified");
    CHECK(param->frameNumThreads < 0 || param->frameNumThreads > X265_MAX_FRAME_THREADS,
          "frameNumThreads (--frame-threads) must be [0 .. X265_MAX_FRAME_THREADS)");

    return check_failed;
}

void x265_print_params(x265_param* param)
{
    x265_log(X265_LOG_INFO, "QP is set as %d\n", param->qp);
    fflush(stderr);
}

char *x265_param2string(x265_param* p, int padx, int pady)
{
    char *buf, *s;
    size_t bufSize = 4000;
    if (p->numaPools)
        bufSize += strlen(p->numaPools);


    buf = s = X265_MALLOC(char, bufSize);
    if (!buf)
        return NULL;
#define BOOL(param, cliopt) \
    s += sprintf(s, " %s", (param) ? cliopt : "no-" cliopt);

    s += sprintf(s, "cpuid=%d", p->cpuid);
    s += sprintf(s, " frame-threads=%d", p->frameNumThreads);
    if (p->numaPools)
        s += sprintf(s, " numa-pools=%s", p->numaPools);
    BOOL(p->bEnableWavefront, "wpp");
    s += sprintf(s, " bitdepth=%d", p->internalBitDepth);
    s += sprintf(s, " input-csp=%d", p->internalCsp);
    s += sprintf(s, " fps=%u/%u", p->fpsNum, p->fpsDenom);
    s += sprintf(s, " input-res=%dx%d", p->sourceWidth - padx, p->sourceHeight - pady);

    s += sprintf(s, " total-frames=%d", p->totalFrames);

    s += sprintf(s, " rc-lookahead=%d", p->lookaheadDepth);
    s += sprintf(s, " lookahead-slices=%d", p->lookaheadSlices);

    s += sprintf(s, " ctu=%d", p->maxCUSize);
    s += sprintf(s, " min-cu-size=%d", p->minCUSize);

    s += sprintf(s, " max-tu-size=%d", p->maxTUSize);
    s += sprintf(s, " tu-inter-depth=%d", p->tuQTMaxInterDepth);
    s += sprintf(s, " tu-intra-depth=%d", p->tuQTMaxIntraDepth);
    s += sprintf(s, " qp=%d", p->qp);
    if (p->filmGrain)
        s += sprintf(s, " film-grain=%s", p->filmGrain); // Film grain characteristics model filename


#undef BOOL
    return buf;
}

void x265_copy_params(x265_param* dst, x265_param* src)
{
    dst->cpuid = src->cpuid;
    dst->frameNumThreads = src->frameNumThreads;
    if (src->numaPools) dst->numaPools = strdup(src->numaPools);
    else dst->numaPools = NULL;

    dst->bEnableWavefront = src->bEnableWavefront;
    dst->internalBitDepth = src->internalBitDepth;
    dst->sourceBitDepth = src->sourceBitDepth;
    dst->internalCsp = src->internalCsp;
    dst->fpsNum = src->fpsNum;
    dst->fpsDenom = src->fpsDenom;
    dst->sourceHeight = src->sourceHeight;
    dst->sourceWidth = src->sourceWidth;
    dst->totalFrames = src->totalFrames;
    dst->maxTUSize = src->maxTUSize;
    dst->tuQTMaxInterDepth = src->tuQTMaxInterDepth;
    dst->tuQTMaxIntraDepth = src->tuQTMaxIntraDepth;
    dst->maxLog2CUSize = src->maxLog2CUSize;
    dst->maxCUDepth = src->maxCUDepth;
    dst->unitSizeDepth = src->unitSizeDepth;
    dst->num4x4Partitions = src->num4x4Partitions;

    dst->bUseAnalysisFile = src->bUseAnalysisFile;
    dst->bDisableLookahead = src->bDisableLookahead;
    dst->bLowPassDct = src->bLowPassDct;
    dst->filmGrain = src->filmGrain;
    dst->qp = src->qp;
}
}
