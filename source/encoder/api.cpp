/*****************************************************************************
 * Copyright (C) 2013-2020 MulticoreWare, Inc
 *
 * Authors: Steve Borho <steve@borho.org>
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
#include "param.h"

#include "encoder.h"
#include "bitcost.h"

#if ENABLE_LIBVMAF
#include "libvmaf/libvmaf.h"
#endif

/* multilib namespace reflectors */
#if LINKED_8BIT
namespace x265_8bit {
const x265_api* x265_api_get(int bitDepth);
const x265_api* x265_api_query(int bitDepth, int apiVersion, int* err);
}
#endif

#if LINKED_10BIT
namespace x265_10bit {
const x265_api* x265_api_get(int bitDepth);
const x265_api* x265_api_query(int bitDepth, int apiVersion, int* err);
}
#endif

#if LINKED_12BIT
namespace x265_12bit {
const x265_api* x265_api_get(int bitDepth);
const x265_api* x265_api_query(int bitDepth, int apiVersion, int* err);
}
#endif

#if EXPORT_C_API
/* these functions are exported as C functions (default) */
using namespace X265_NS;
extern "C" {
#else
/* these functions exist within private namespace (multilib) */
namespace X265_NS {
#endif

static const char* summaryCSVHeader =
    "Command, Date/Time, Elapsed Time, FPS, Bitrate, "
    "Y PSNR, U PSNR, V PSNR, Global PSNR, SSIM, SSIM (dB), "
    "I count, I ave-QP, I kbps, I-PSNR Y, I-PSNR U, I-PSNR V, I-SSIM (dB), "
    "P count, P ave-QP, P kbps, P-PSNR Y, P-PSNR U, P-PSNR V, P-SSIM (dB), "
    "B count, B ave-QP, B kbps, B-PSNR Y, B-PSNR U, B-PSNR V, B-SSIM (dB), ";
x265_encoder *x265_encoder_open(x265_param *p)
{
    if (!p)
        return NULL;

#if _MSC_VER
#pragma warning(disable: 4127) // conditional expression is constant, yes I know
#endif

#if HIGH_BIT_DEPTH
    if (X265_DEPTH != 10 && X265_DEPTH != 12)
#else
    if (X265_DEPTH != 8)
#endif
    {
        x265_log(p, X265_LOG_ERROR, "Build error, internal bit depth mismatch\n");
        return NULL;
    }

    Encoder* encoder = NULL;
    x265_param* param = PARAM_NS::x265_param_alloc();
    x265_param* latestParam = PARAM_NS::x265_param_alloc();
    x265_param* zoneParam = PARAM_NS::x265_param_alloc();

    if(param) PARAM_NS::x265_param_default(param);
    if(latestParam) PARAM_NS::x265_param_default(latestParam);
    if(zoneParam) PARAM_NS::x265_param_default(zoneParam);
  
    if (!param || !latestParam || !zoneParam)
        goto fail;

    x265_copy_params(param, p);
    x265_copy_params(latestParam, p);
    x265_copy_params(zoneParam, p);
    x265_log(param, X265_LOG_INFO, "HEVC encoder version %s\n", PFX(version_str));
    x265_log(param, X265_LOG_INFO, "build info %s\n", PFX(build_info_str));

    encoder = new Encoder;

    x265_setup_primitives(param);

    if (x265_check_params(param))
        goto fail;

    // may change params for auto-detect, etc
    encoder->configure(param);
    if (encoder->m_aborted)
        goto fail;

    encoder->create();
    p->frameNumThreads = encoder->m_param->frameNumThreads;

    encoder->m_latestParam = latestParam;
    x265_copy_params(latestParam, param);
    if (encoder->m_aborted)
        goto fail;

    x265_print_params(param);
    return encoder;

fail:
    delete encoder;
    PARAM_NS::x265_param_free(param);
    PARAM_NS::x265_param_free(latestParam);
    PARAM_NS::x265_param_free(zoneParam);
    return NULL;
}

void x265_encoder_parameters(x265_encoder *enc, x265_param *out)
{
    if (enc && out)
    {
        Encoder *encoder = static_cast<Encoder*>(enc);
        x265_copy_params(out, encoder->m_param);
    }
}


int x265_encoder_encode(x265_encoder *enc, x265_nal **pp_nal, uint32_t *pi_nal, x265_picture *pic_in, x265_picture *pic_out)
{
    if (!enc)
        return -1;

    Encoder *encoder = static_cast<Encoder*>(enc);
    int numEncoded;

#ifdef SVT_HEVC
    EB_ERRORTYPE return_error;
    if (encoder->m_param->bEnableSvtHevc)
    {
        static unsigned char picSendDone = 0;
        numEncoded = 0;
        static int codedNal = 0, eofReached = 0;
        EB_H265_ENC_CONFIGURATION* svtParam = (EB_H265_ENC_CONFIGURATION*)encoder->m_svtAppData->svtHevcParams;
        if (pic_in)
        {
            if (pic_in->colorSpace == X265_CSP_I420) // SVT-HEVC supports only yuv420p color space
            {
                EB_BUFFERHEADERTYPE *inputPtr = encoder->m_svtAppData->inputPictureBuffer;

                if (pic_in->framesize) inputPtr->nFilledLen = (uint32_t)pic_in->framesize;
                inputPtr->nFlags = 0;
                inputPtr->pts = pic_in->pts;
                inputPtr->dts = pic_in->dts;
                inputPtr->sliceType = EB_INVALID_PICTURE;

                EB_H265_ENC_INPUT *inputData = (EB_H265_ENC_INPUT*) inputPtr->pBuffer;
                inputData->luma = (unsigned char*) pic_in->planes[0];
                inputData->cb = (unsigned char*) pic_in->planes[1];
                inputData->cr = (unsigned char*) pic_in->planes[2];

                inputData->yStride = encoder->m_param->sourceWidth;
                inputData->cbStride = encoder->m_param->sourceWidth >> 1;
                inputData->crStride = encoder->m_param->sourceWidth >> 1;

                inputData->lumaExt = NULL;
                inputData->cbExt = NULL;
                inputData->crExt = NULL;

                if (pic_in->rpu.payloadSize)
                {
                    inputData->dolbyVisionRpu.payload = X265_MALLOC(uint8_t, 1024);
                    memcpy(inputData->dolbyVisionRpu.payload, pic_in->rpu.payload, pic_in->rpu.payloadSize);
                    inputData->dolbyVisionRpu.payloadSize = pic_in->rpu.payloadSize;
                    inputData->dolbyVisionRpu.payloadType = NAL_UNIT_UNSPECIFIED;
                }
                else
                {
                    inputData->dolbyVisionRpu.payload = NULL;
                    inputData->dolbyVisionRpu.payloadSize = 0;
                }

                // Send the picture to the encoder
                return_error = EbH265EncSendPicture(encoder->m_svtAppData->svtEncoderHandle, inputPtr);

                if (return_error != EB_ErrorNone)
                {
                    x265_log(encoder->m_param, X265_LOG_ERROR, "SVT HEVC encoder: Error while encoding \n");
                    numEncoded = -1;
                    goto fail;
                }
            }
            else
            {
                x265_log(encoder->m_param, X265_LOG_ERROR, "SVT HEVC Encoder accepts only yuv420p input \n");
                numEncoded = -1;
                goto fail;
            }
        }
        else if (!picSendDone) //Encoder flush
        {
            picSendDone = 1;
            EB_BUFFERHEADERTYPE inputPtrLast;
            inputPtrLast.nAllocLen = 0;
            inputPtrLast.nFilledLen = 0;
            inputPtrLast.nTickCount = 0;
            inputPtrLast.pAppPrivate = NULL;
            inputPtrLast.nFlags = EB_BUFFERFLAG_EOS;
            inputPtrLast.pBuffer = NULL;

            return_error = EbH265EncSendPicture(encoder->m_svtAppData->svtEncoderHandle, &inputPtrLast);
            if (return_error != EB_ErrorNone)
            {
                x265_log(encoder->m_param, X265_LOG_ERROR, "SVT HEVC encoder: Error while encoding \n");
                numEncoded = -1;
                goto fail;
            }
        }

        if (eofReached && svtParam->codeEosNal == 0 && !codedNal)
        {
            EB_BUFFERHEADERTYPE *outputStreamPtr = 0;
            return_error = EbH265EncEosNal(encoder->m_svtAppData->svtEncoderHandle, &outputStreamPtr);
            if (return_error == EB_ErrorMax)
            {
                x265_log(encoder->m_param, X265_LOG_ERROR, "SVT HEVC encoder: Error while encoding \n");
                numEncoded = -1;
                goto fail;
            }
            if (return_error != EB_NoErrorEmptyQueue)
            {
                if (outputStreamPtr->pBuffer)
                {
                    //Copy data from output packet to NAL
                    encoder->m_nalList.m_nal[0].payload = outputStreamPtr->pBuffer;
                    encoder->m_nalList.m_nal[0].sizeBytes = outputStreamPtr->nFilledLen;
                    encoder->m_svtAppData->byteCount += outputStreamPtr->nFilledLen;
                    *pp_nal = &encoder->m_nalList.m_nal[0];
                    *pi_nal = 1;
                    numEncoded = 0;
                    codedNal = 1;
                    return numEncoded;
                }

                // Release the output buffer
                EbH265ReleaseOutBuffer(&outputStreamPtr);
            }
        }
        else if (eofReached)
        {
            *pi_nal = 0;
            return numEncoded;
        }

        //Receive Packet
        EB_BUFFERHEADERTYPE *outputPtr;
        return_error = EbH265GetPacket(encoder->m_svtAppData->svtEncoderHandle, &outputPtr, picSendDone);
        if (return_error == EB_ErrorMax)
        {
            x265_log(encoder->m_param, X265_LOG_ERROR, "SVT HEVC encoder: Error while encoding \n");
            numEncoded = -1;
            goto fail;
        }

        if (return_error != EB_NoErrorEmptyQueue)
        {
            if (outputPtr->pBuffer)
            {
                //Copy data from output packet to NAL
                encoder->m_nalList.m_nal[0].payload = outputPtr->pBuffer;
                encoder->m_nalList.m_nal[0].sizeBytes = outputPtr->nFilledLen;
                encoder->m_svtAppData->byteCount += outputPtr->nFilledLen;
                encoder->m_svtAppData->outFrameCount++;
                *pp_nal = &encoder->m_nalList.m_nal[0];
                *pi_nal = 1;
                numEncoded = 1;
            }

            eofReached = outputPtr->nFlags & EB_BUFFERFLAG_EOS;

            // Release the output buffer
            EbH265ReleaseOutBuffer(&outputPtr);
        }
        else if (pi_nal)
            *pi_nal = 0;

        pic_out = NULL;

fail:
        if (numEncoded < 0)
            encoder->m_aborted = true;

        return numEncoded;
    }
#endif

    // While flushing, we cannot return 0 until the entire stream is flushed
    do
    {
        numEncoded = encoder->encode(pic_in, pic_out);
    }
    while ((numEncoded == 0 && !pic_in && encoder->m_numDelayedPic && !encoder->m_latestParam->forceFlush) && !encoder->m_externalFlush);
    if (numEncoded)
        encoder->m_externalFlush = false;

    if (numEncoded < 0)
        encoder->m_aborted = true;

    return numEncoded;
}

#if ENABLE_LIBVMAF
void x265_vmaf_encoder_log(x265_encoder* enc, int argc, char **argv, x265_param *param, x265_vmaf_data *vmafdata)
{
    if (enc)
    {
        Encoder *encoder = static_cast<Encoder*>(enc);
        x265_stats stats;       
        stats.aggregateVmafScore = x265_calculate_vmafscore(param, vmafdata);
        if(vmafdata->reference_file)
            fclose(vmafdata->reference_file);
        if(vmafdata->distorted_file)
            fclose(vmafdata->distorted_file);
        if(vmafdata)
            x265_free(vmafdata);
        encoder->fetchStats(&stats, sizeof(stats));
        int padx = encoder->m_sps.conformanceWindow.rightOffset;
        int pady = encoder->m_sps.conformanceWindow.bottomOffset;
        x265_csvlog_encode(encoder->m_param, &stats, padx, pady, argc, argv);
    }
}
#endif

#ifdef SVT_HEVC
static void svt_print_summary(x265_encoder *enc)
{
    Encoder *encoder = static_cast<Encoder*>(enc);
    double frameRate = 0, bitrate = 0;
    EB_H265_ENC_CONFIGURATION *svtParam = (EB_H265_ENC_CONFIGURATION*)encoder->m_svtAppData->svtHevcParams;
    if (svtParam->frameRateNumerator && svtParam->frameRateDenominator && (svtParam->frameRateNumerator != 0 && svtParam->frameRateDenominator != 0))
    {
        frameRate = ((double)svtParam->frameRateNumerator) / ((double)svtParam->frameRateDenominator);
        if(encoder->m_svtAppData->outFrameCount)
            bitrate = ((double)(encoder->m_svtAppData->byteCount << 3) * frameRate / (encoder->m_svtAppData->outFrameCount * 1000));

        printf("Total Frames\t\tFrame Rate\t\tByte Count\t\tBitrate\n");
        printf("%12d\t\t%4.2f fps\t\t%10.0f\t\t%5.2f kbps\n", (int32_t)encoder->m_svtAppData->outFrameCount, (double)frameRate, (double)encoder->m_svtAppData->byteCount, bitrate);
    }
}
#endif

void x265_encoder_close(x265_encoder *enc)
{
    if (enc)
    {
        Encoder *encoder = static_cast<Encoder*>(enc);

        encoder->stopJobs();

        encoder->destroy();
        delete encoder;
    }
}

int x265_encoder_intra_refresh(x265_encoder *enc)
{
    if (!enc)
        return -1;

    Encoder *encoder = static_cast<Encoder*>(enc);
    encoder->m_bQueuedIntraRefresh = 1;
    return 0;
}
int x265_encoder_ctu_info(x265_encoder *enc, int poc, x265_ctu_info_t** ctu)
{
    if (!ctu || !enc)
        return -1;
    Encoder* encoder = static_cast<Encoder*>(enc);
    //encoder->copyCtuInfo(ctu, poc);
    return 0;
}


//int x265_get_ref_frame_list(x265_encoder *enc, x265_picyuv** l0, x265_picyuv** l1, int sliceType, int poc, int* pocL0, int* pocL1)
//{
//    if (!enc)
//        return -1;
//
//    Encoder *encoder = static_cast<Encoder*>(enc);
//    return encoder->getRefFrameList((PicYuv**)l0, (PicYuv**)l1, sliceType, poc, pocL0, pocL1);
//}

void x265_cleanup(void)
{
    BitCost::destroy();
}

x265_picture *x265_picture_alloc()
{
    return (x265_picture*)x265_malloc(sizeof(x265_picture));
}

void x265_picture_init(x265_param *param, x265_picture *pic)
{
    memset(pic, 0, sizeof(x265_picture));

    pic->bitDepth = param->internalBitDepth;
    pic->colorSpace = param->internalCsp;
}

void x265_picture_free(x265_picture *p)
{
    return x265_free(p);
}

x265_zone *x265_zone_alloc(int zoneCount, int isZoneFile)
{
    x265_zone* zone = (x265_zone*)x265_malloc(sizeof(x265_zone) * zoneCount);
    if (isZoneFile) {
        for (int i = 0; i < zoneCount; i++)
            zone[i].zoneParam = (x265_param*)x265_malloc(sizeof(x265_param));
    }
    return zone;
}

static const x265_api libapi =
{
    X265_MAJOR_VERSION,
    X265_BUILD,
    sizeof(x265_param),
    sizeof(x265_picture),
    sizeof(x265_analysis_data),
    sizeof(x265_zone),
    sizeof(x265_stats),

    PFX(max_bit_depth),
    PFX(version_str),
    PFX(build_info_str),

    &PARAM_NS::x265_param_alloc,
    &PARAM_NS::x265_param_free,
    &PARAM_NS::x265_param_default,
    &PARAM_NS::x265_param_parse,
    &PARAM_NS::x265_scenecut_aware_qp_param_parse,
    //&PARAM_NS::x265_param_apply_profile,
    //&PARAM_NS::x265_param_default_preset,
    &x265_picture_alloc,
    &x265_picture_free,
    &x265_picture_init,
    &x265_encoder_open,
    &x265_encoder_parameters,
    //&x265_encoder_reconfig_zone,
    &x265_encoder_encode,
    &x265_encoder_close,
    &x265_cleanup,

    sizeof(x265_frame_stats),
    &x265_encoder_intra_refresh,
    &x265_encoder_ctu_info,
    &x265_dither_image,
//#if ENABLE_LIBVMAF
//    &x265_calculate_vmafscore,
//    &x265_calculate_vmaf_framelevelscore,
//    &x265_vmaf_encoder_log,
//#endif
//    &PARAM_NS::x265_zone_param_parse
};

typedef const x265_api* (*api_get_func)(int bitDepth);
typedef const x265_api* (*api_query_func)(int bitDepth, int apiVersion, int* err);

#define xstr(s) str(s)
#define str(s) #s

#if _WIN32
#define ext ".dll"
#elif MACOS
#include <dlfcn.h>
#define ext ".dylib"
#else
#include <dlfcn.h>
#define ext ".so"
#endif
#if defined(__GNUC__) && __GNUC__ >= 8
#pragma GCC diagnostic ignored "-Wcast-function-type"
#endif

static int g_recursion /* = 0 */;
const x265_api* x265_api_get(int bitDepth)
{
    if (bitDepth && bitDepth != X265_DEPTH)
    {
#if LINKED_8BIT
        if (bitDepth == 8) return x265_8bit::x265_api_get(0);
#endif
#if LINKED_10BIT
        if (bitDepth == 10) return x265_10bit::x265_api_get(0);
#endif
#if LINKED_12BIT
        if (bitDepth == 12) return x265_12bit::x265_api_get(0);
#endif

        const char* libname = NULL;
        const char* method = "x265_api_get_" xstr(X265_BUILD);
        const char* multilibname = "libx265" ext;

        if (bitDepth == 12)
            libname = "libx265_main12" ext;
        else if (bitDepth == 10)
            libname = "libx265_main10" ext;
        else if (bitDepth == 8)
            libname = "libx265_main" ext;
        else
            return NULL;

        const x265_api* api = NULL;
        int reqDepth = 0;

        if (g_recursion > 1)
            return NULL;
        else
            g_recursion++;

#if _WIN32
        HMODULE h = LoadLibraryA(libname);
        if (!h)
        {
            h = LoadLibraryA(multilibname);
            reqDepth = bitDepth;
        }
        if (h)
        {
            api_get_func get = (api_get_func)GetProcAddress(h, method);
            if (get)
                api = get(reqDepth);
        }
#else
        void* h = dlopen(libname, RTLD_LAZY | RTLD_LOCAL);
        if (!h)
        {
            h = dlopen(multilibname, RTLD_LAZY | RTLD_LOCAL);
            reqDepth = bitDepth;
        }
        if (h)
        {
            api_get_func get = (api_get_func)dlsym(h, method);
            if (get)
                api = get(reqDepth);
        }
#endif

        g_recursion--;

        if (api && bitDepth != api->bit_depth)
        {
            x265_log(NULL, X265_LOG_WARNING, "%s does not support requested bitDepth %d\n", libname, bitDepth);
            return NULL;
        }

        return api;
    }

    return &libapi;
}

const x265_api* x265_api_query(int bitDepth, int apiVersion, int* err)
{
    if (apiVersion < 51)
    {
        /* builds before 1.6 had re-ordered public structs */
        if (err) *err = X265_API_QUERY_ERR_VER_REFUSED;
        return NULL;
    }

    if (err) *err = X265_API_QUERY_ERR_NONE;

    if (bitDepth && bitDepth != X265_DEPTH)
    {
#if LINKED_8BIT
        if (bitDepth == 8) return x265_8bit::x265_api_query(0, apiVersion, err);
#endif
#if LINKED_10BIT
        if (bitDepth == 10) return x265_10bit::x265_api_query(0, apiVersion, err);
#endif
#if LINKED_12BIT
        if (bitDepth == 12) return x265_12bit::x265_api_query(0, apiVersion, err);
#endif

        const char* libname = NULL;
        const char* method = "x265_api_query";
        const char* multilibname = "libx265" ext;

        if (bitDepth == 12)
            libname = "libx265_main12" ext;
        else if (bitDepth == 10)
            libname = "libx265_main10" ext;
        else if (bitDepth == 8)
            libname = "libx265_main" ext;
        else
        {
            if (err) *err = X265_API_QUERY_ERR_LIB_NOT_FOUND;
            return NULL;
        }

        const x265_api* api = NULL;
        int reqDepth = 0;
        int e = X265_API_QUERY_ERR_LIB_NOT_FOUND;

        if (g_recursion > 1)
        {
            if (err) *err = X265_API_QUERY_ERR_LIB_NOT_FOUND;
            return NULL;
        }
        else
            g_recursion++;

#if _WIN32
        HMODULE h = LoadLibraryA(libname);
        if (!h)
        {
            h = LoadLibraryA(multilibname);
            reqDepth = bitDepth;
        }
        if (h)
        {
            e = X265_API_QUERY_ERR_FUNC_NOT_FOUND;
            api_query_func query = (api_query_func)GetProcAddress(h, method);
            if (query)
                api = query(reqDepth, apiVersion, err);
        }
#else
        void* h = dlopen(libname, RTLD_LAZY | RTLD_LOCAL);
        if (!h)
        {
            h = dlopen(multilibname, RTLD_LAZY | RTLD_LOCAL);
            reqDepth = bitDepth;
        }
        if (h)
        {
            e = X265_API_QUERY_ERR_FUNC_NOT_FOUND;
            api_query_func query = (api_query_func)dlsym(h, method);
            if (query)
                api = query(reqDepth, apiVersion, err);
        }
#endif

        g_recursion--;

        if (api && bitDepth != api->bit_depth)
        {
            x265_log(NULL, X265_LOG_WARNING, "%s does not support requested bitDepth %d\n", libname, bitDepth);
            if (err) *err = X265_API_QUERY_ERR_WRONG_BITDEPTH;
            return NULL;
        }

        if (err) *err = api ? X265_API_QUERY_ERR_NONE : e;
        return api;
    }

    return &libapi;
}


/* The dithering algorithm is based on Sierra-2-4A error diffusion.
 * We convert planes in place (without allocating a new buffer). */
static void ditherPlane(uint16_t *src, int srcStride, int width, int height, int16_t *errors, int bitDepth)
{
    const int lShift = 16 - bitDepth;
    const int rShift = 16 - bitDepth + 2;
    const int half = (1 << (16 - bitDepth + 1));
    const int pixelMax = (1 << bitDepth) - 1;

    memset(errors, 0, (width + 1) * sizeof(int16_t));

    if (bitDepth == 8)
    {
        for (int y = 0; y < height; y++, src += srcStride)
        {
            uint8_t* dst = (uint8_t *)src;
            int16_t err = 0;
            for (int x = 0; x < width; x++)
            {
                err = err * 2 + errors[x] + errors[x + 1];
                int tmpDst = x265_clip3(0, pixelMax, ((src[x] << 2) + err + half) >> rShift);
                errors[x] = err = (int16_t)(src[x] - (tmpDst << lShift));
                dst[x] = (uint8_t)tmpDst;
            }
        }
    }
    else
    {
        for (int y = 0; y < height; y++, src += srcStride)
        {
            int16_t err = 0;
            for (int x = 0; x < width; x++)
            {
                err = err * 2 + errors[x] + errors[x + 1];
                int tmpDst = x265_clip3(0, pixelMax, ((src[x] << 2) + err + half) >> rShift);
                errors[x] = err = (int16_t)(src[x] - (tmpDst << lShift));
                src[x] = (uint16_t)tmpDst;
            }
        }
    }
}

void x265_dither_image(x265_picture* picIn, int picWidth, int picHeight, int16_t *errorBuf, int bitDepth)
{
    const x265_api* api = x265_api_get(0);

    if (sizeof(x265_picture) != api->sizeof_picture)
    {
        fprintf(stderr, "extras [error]: structure size skew, unable to dither\n");
        return;
    }

    if (picIn->bitDepth <= 8)
    {
        fprintf(stderr, "extras [error]: dither support enabled only for input bitdepth > 8\n");
        return;
    }

    if (picIn->bitDepth == bitDepth)
    {
        fprintf(stderr, "extras[error]: dither support enabled only if encoder depth is different from picture depth\n");
        return;
    }

    /* This portion of code is from readFrame in x264. */
    for (int i = 0; i < x265_cli_csps[picIn->colorSpace].planes; i++)
    {
        if (picIn->bitDepth < 16)
        {
            /* upconvert non 16bit high depth planes to 16bit */
            uint16_t *plane = (uint16_t*)picIn->planes[i];
            uint32_t pixelCount = x265_picturePlaneSize(picIn->colorSpace, picWidth, picHeight, i);
            int lShift = 16 - picIn->bitDepth;

            /* This loop assumes width is equal to stride which
             * happens to be true for file reader outputs */
            for (uint32_t j = 0; j < pixelCount; j++)
                plane[j] = plane[j] << lShift;
        }

        int height = (int)(picHeight >> x265_cli_csps[picIn->colorSpace].height[i]);
        int width = (int)(picWidth >> x265_cli_csps[picIn->colorSpace].width[i]);

        ditherPlane(((uint16_t*)picIn->planes[i]), picIn->stride[i] / 2, width, height, errorBuf, bitDepth);
    }
}

#if ENABLE_LIBVMAF
/* Read y values of single frame for 8-bit input */
int read_image_byte(FILE *file, float *buf, int width, int height, int stride)
{
    char *byte_ptr = (char *)buf;
    unsigned char *tmp_buf = 0;
    int i, j;
    int ret = 1;

    if (width <= 0 || height <= 0)
    {
        goto fail_or_end;
    }

    if (!(tmp_buf = (unsigned char*)malloc(width)))
    {
        goto fail_or_end;
    }

    for (i = 0; i < height; ++i)
    {
        float *row_ptr = (float *)byte_ptr;

        if (fread(tmp_buf, 1, width, file) != (size_t)width)
        {
            goto fail_or_end;
        }

        for (j = 0; j < width; ++j)
        {
            row_ptr[j] = tmp_buf[j];
        }

        byte_ptr += stride;
    }

    ret = 0;

fail_or_end:
    free(tmp_buf);
    return ret;
}
/* Read y values of single frame for 10-bit input */
int read_image_word(FILE *file, float *buf, int width, int height, int stride)
{
    char *byte_ptr = (char *)buf;
    unsigned short *tmp_buf = 0;
    int i, j;
    int ret = 1;

    if (width <= 0 || height <= 0)
    {
        goto fail_or_end;
    }

    if (!(tmp_buf = (unsigned short*)malloc(width * 2))) // '*2' to accommodate words
    {
        goto fail_or_end;
    }

    for (i = 0; i < height; ++i)
    {
        float *row_ptr = (float *)byte_ptr;

        if (fread(tmp_buf, 2, width, file) != (size_t)width) // '2' for word
        {
            goto fail_or_end;
        }

        for (j = 0; j < width; ++j)
        {
            row_ptr[j] = tmp_buf[j] / 4.0; // '/4' to convert from 10 to 8-bit
        }

        byte_ptr += stride;
    }

    ret = 0;

fail_or_end:
    free(tmp_buf);
    return ret;
}

int read_frame(float *reference_data, float *distorted_data, float *temp_data, int stride_byte, void *s)
{
    x265_vmaf_data *user_data = (x265_vmaf_data *)s;
    int ret;

    // read reference y
    if (user_data->internalBitDepth == 8)
    {
        ret = read_image_byte(user_data->reference_file, reference_data, user_data->width, user_data->height, stride_byte);
    }
    else if (user_data->internalBitDepth == 10)
    {
        ret = read_image_word(user_data->reference_file, reference_data, user_data->width, user_data->height, stride_byte);
    }
    else
    {
        x265_log(NULL, X265_LOG_ERROR, "Invalid bitdepth\n");
        return 1;
    }
    if (ret)
    {
        if (feof(user_data->reference_file))
        {
            ret = 2; // OK if end of file
        }
        return ret;
    }

    // read distorted y
    if (user_data->internalBitDepth == 8)
    {
        ret = read_image_byte(user_data->distorted_file, distorted_data, user_data->width, user_data->height, stride_byte);
    }
    else if (user_data->internalBitDepth == 10)
    {
        ret = read_image_word(user_data->distorted_file, distorted_data, user_data->width, user_data->height, stride_byte);
    }
    else
    {
        x265_log(NULL, X265_LOG_ERROR, "Invalid bitdepth\n");
        return 1;
    }
    if (ret)
    {
        if (feof(user_data->distorted_file))
        {
            ret = 2; // OK if end of file
        }
        return ret;
    }

    // reference skip u and v
    if (user_data->internalBitDepth == 8)
    {
        if (fread(temp_data, 1, user_data->offset, user_data->reference_file) != (size_t)user_data->offset)
        {
            x265_log(NULL, X265_LOG_ERROR, "reference fread to skip u and v failed.\n");
            goto fail_or_end;
        }
    }
    else if (user_data->internalBitDepth == 10)
    {
        if (fread(temp_data, 2, user_data->offset, user_data->reference_file) != (size_t)user_data->offset)
        {
            x265_log(NULL, X265_LOG_ERROR, "reference fread to skip u and v failed.\n");
            goto fail_or_end;
        }
    }
    else
    {
        x265_log(NULL, X265_LOG_ERROR, "Invalid format\n");
        goto fail_or_end;
    }

    // distorted skip u and v
    if (user_data->internalBitDepth == 8)
    {
        if (fread(temp_data, 1, user_data->offset, user_data->distorted_file) != (size_t)user_data->offset)
        {
            x265_log(NULL, X265_LOG_ERROR, "distorted fread to skip u and v failed.\n");
            goto fail_or_end;
        }
    }
    else if (user_data->internalBitDepth == 10)
    {
        if (fread(temp_data, 2, user_data->offset, user_data->distorted_file) != (size_t)user_data->offset)
        {
            x265_log(NULL, X265_LOG_ERROR, "distorted fread to skip u and v failed.\n");
            goto fail_or_end;
        }
    }
    else
    {
        x265_log(NULL, X265_LOG_ERROR, "Invalid format\n");
        goto fail_or_end;
    }


fail_or_end:
    return ret;
}

double x265_calculate_vmafscore(x265_param *param, x265_vmaf_data *data)
{
    double score;

    data->width = param->sourceWidth;
    data->height = param->sourceHeight;
    data->internalBitDepth = param->internalBitDepth;

    if (param->internalCsp == X265_CSP_I420)
    {
        if ((param->sourceWidth * param->sourceHeight) % 2 != 0)
            x265_log(NULL, X265_LOG_ERROR, "Invalid file size\n");
        data->offset = param->sourceWidth * param->sourceHeight / 2;
    }
    else if (param->internalCsp == X265_CSP_I422)
        data->offset = param->sourceWidth * param->sourceHeight;
    else if (param->internalCsp == X265_CSP_I444)
        data->offset = param->sourceWidth * param->sourceHeight * 2;
    else
        x265_log(NULL, X265_LOG_ERROR, "Invalid format\n");

    compute_vmaf(&score, vcd->format, data->width, data->height, read_frame, data, vcd->model_path, vcd->log_path, vcd->log_fmt, vcd->disable_clip, vcd->disable_avx, vcd->enable_transform, vcd->phone_model, vcd->psnr, vcd->ssim, vcd->ms_ssim, vcd->pool, vcd->thread, vcd->subsample, vcd->enable_conf_interval);

    return score;
}

int read_frame_10bit(float *reference_data, float *distorted_data, float *temp_data, int stride, void *s)
{
    x265_vmaf_framedata *user_data = (x265_vmaf_framedata *)s;

    PicYuv *reference_frame = (PicYuv *)user_data->reference_frame;
    PicYuv *distorted_frame = (PicYuv *)user_data->distorted_frame;

    if(!user_data->frame_set) {

        int reference_stride = reference_frame->m_stride;
        int distorted_stride = distorted_frame->m_stride;

        const uint16_t *reference_ptr = (const uint16_t *)reference_frame->m_picOrg[0];
        const uint16_t *distorted_ptr = (const uint16_t *)distorted_frame->m_picOrg[0];

        temp_data = reference_data;

        int height = user_data->height;
        int width = user_data->width; 

        int i,j;
        for (i = 0; i < height; i++) {
            for ( j = 0; j < width; j++) {
                temp_data[j] = ((float)reference_ptr[j] / 4.0);
            }
            reference_ptr += reference_stride;
            temp_data += stride / sizeof(*temp_data);
        }

        temp_data = distorted_data;
        for (i = 0; i < height; i++) {
            for (j = 0; j < width; j++) {
                 temp_data[j] = ((float)distorted_ptr[j] / 4.0);
            }
            distorted_ptr += distorted_stride;
            temp_data += stride / sizeof(*temp_data);
        }

        user_data->frame_set = 1;
        return 0;
    }
    return 2;
}

int read_frame_8bit(float *reference_data, float *distorted_data, float *temp_data, int stride, void *s)
{
    x265_vmaf_framedata *user_data = (x265_vmaf_framedata *)s;

    PicYuv *reference_frame = (PicYuv *)user_data->reference_frame;
    PicYuv *distorted_frame = (PicYuv *)user_data->distorted_frame;

    if(!user_data->frame_set) {

        int reference_stride = reference_frame->m_stride;
        int distorted_stride = distorted_frame->m_stride;

        const uint8_t *reference_ptr = (const uint8_t *)reference_frame->m_picOrg[0]; 
        const uint8_t *distorted_ptr = (const uint8_t *)distorted_frame->m_picOrg[0];

        temp_data = reference_data;

        int height = user_data->height;
        int width = user_data->width; 

        int i,j;
        for (i = 0; i < height; i++) {
            for ( j = 0; j < width; j++) {
                temp_data[j] = (float)reference_ptr[j];
            }
            reference_ptr += reference_stride;
            temp_data += stride / sizeof(*temp_data);
        }

        temp_data = distorted_data;
        for (i = 0; i < height; i++) {
            for (j = 0; j < width; j++) {
                 temp_data[j] = (float)distorted_ptr[j];
            }
            distorted_ptr += distorted_stride;
            temp_data += stride / sizeof(*temp_data);
        }

        user_data->frame_set = 1;
        return 0;
    }
    return 2;
}

double x265_calculate_vmaf_framelevelscore(x265_vmaf_framedata *vmafframedata)
{
    double score; 
    int (*read_frame)(float *reference_data, float *distorted_data, float *temp_data,
                      int stride, void *s);
    if (vmafframedata->internalBitDepth == 8)
        read_frame = read_frame_8bit;
    else
        read_frame = read_frame_10bit;
    compute_vmaf(&score, vcd->format, vmafframedata->width, vmafframedata->height, read_frame, vmafframedata, vcd->model_path, vcd->log_path, vcd->log_fmt, vcd->disable_clip, vcd->disable_avx, vcd->enable_transform, vcd->phone_model, vcd->psnr, vcd->ssim, vcd->ms_ssim, vcd->pool, vcd->thread, vcd->subsample, vcd->enable_conf_interval);

    return score;
}
#endif

} /* end namespace or extern "C" */

namespace X265_NS {
#ifdef SVT_HEVC

void svt_initialise_app_context(x265_encoder *enc)
{
    Encoder *encoder = static_cast<Encoder*>(enc);

    //Initialise Application Context
    encoder->m_svtAppData = (SvtAppContext*)x265_malloc(sizeof(SvtAppContext));
    encoder->m_svtAppData->svtHevcParams = (EB_H265_ENC_CONFIGURATION*)x265_malloc(sizeof(EB_H265_ENC_CONFIGURATION));
    encoder->m_svtAppData->byteCount = 0;
    encoder->m_svtAppData->outFrameCount = 0;
}

int svt_initialise_input_buffer(x265_encoder *enc)
{
    Encoder *encoder = static_cast<Encoder*>(enc);

    //Initialise Input Buffer
    encoder->m_svtAppData->inputPictureBuffer = (EB_BUFFERHEADERTYPE*)x265_malloc(sizeof(EB_BUFFERHEADERTYPE));
    EB_BUFFERHEADERTYPE *inputPtr = encoder->m_svtAppData->inputPictureBuffer;
    inputPtr->pBuffer = (unsigned char*)x265_malloc(sizeof(EB_H265_ENC_INPUT));

    EB_H265_ENC_INPUT *inputData = (EB_H265_ENC_INPUT*)inputPtr->pBuffer;
    inputData->dolbyVisionRpu.payload = NULL;
    inputData->dolbyVisionRpu.payloadSize = 0;


    if (!inputPtr->pBuffer)
        return 0;

    inputPtr->nSize = sizeof(EB_BUFFERHEADERTYPE);
    inputPtr->pAppPrivate = NULL;
    return 1;
}
#endif // ifdef SVT_HEVC

} // end namespace X265_NS
