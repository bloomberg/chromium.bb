/*
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "config.h"

#include <stdint.h>
#include <string.h>

#include <VideoToolbox/VideoToolbox.h>

#include "buffer.h"
#include "buffer_internal.h"
#include "common.h"
#include "hwcontext.h"
#include "hwcontext_internal.h"
#include "hwcontext_videotoolbox.h"
#include "mem.h"
#include "pixfmt.h"
#include "pixdesc.h"

typedef struct VTFramesContext {
    CVPixelBufferPoolRef pool;
} VTFramesContext;

static const struct {
    uint32_t cv_fmt;
    bool full_range;
    enum AVPixelFormat pix_fmt;
} cv_pix_fmts[] = {
    { kCVPixelFormatType_420YpCbCr8Planar,              false, AV_PIX_FMT_YUV420P },
    { kCVPixelFormatType_422YpCbCr8,                    false, AV_PIX_FMT_UYVY422 },
    { kCVPixelFormatType_32BGRA,                        false, AV_PIX_FMT_BGRA },
#ifdef kCFCoreFoundationVersionNumber10_7
    { kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange,  false, AV_PIX_FMT_NV12 },
    { kCVPixelFormatType_420YpCbCr8BiPlanarFullRange,   true,  AV_PIX_FMT_NV12 },
#endif
#if HAVE_KCVPIXELFORMATTYPE_420YPCBCR10BIPLANARVIDEORANGE
    { kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange, false, AV_PIX_FMT_P010 },
    { kCVPixelFormatType_420YpCbCr10BiPlanarFullRange,  true,  AV_PIX_FMT_P010 },
#endif
};

static const enum AVPixelFormat supported_formats[] = {
    AV_PIX_FMT_NV12,
    AV_PIX_FMT_YUV420P,
    AV_PIX_FMT_UYVY422,
    AV_PIX_FMT_P010,
    AV_PIX_FMT_BGRA,
};

static int vt_frames_get_constraints(AVHWDeviceContext *ctx,
                                     const void *hwconfig,
                                     AVHWFramesConstraints *constraints)
{
    int i;

    constraints->valid_sw_formats = av_malloc_array(FF_ARRAY_ELEMS(supported_formats) + 1,
                                                    sizeof(*constraints->valid_sw_formats));
    if (!constraints->valid_sw_formats)
        return AVERROR(ENOMEM);

    for (i = 0; i < FF_ARRAY_ELEMS(supported_formats); i++)
        constraints->valid_sw_formats[i] = supported_formats[i];
    constraints->valid_sw_formats[FF_ARRAY_ELEMS(supported_formats)] = AV_PIX_FMT_NONE;

    constraints->valid_hw_formats = av_malloc_array(2, sizeof(*constraints->valid_hw_formats));
    if (!constraints->valid_hw_formats)
        return AVERROR(ENOMEM);

    constraints->valid_hw_formats[0] = AV_PIX_FMT_VIDEOTOOLBOX;
    constraints->valid_hw_formats[1] = AV_PIX_FMT_NONE;

    return 0;
}

enum AVPixelFormat av_map_videotoolbox_format_to_pixfmt(uint32_t cv_fmt)
{
    int i;
    for (i = 0; i < FF_ARRAY_ELEMS(cv_pix_fmts); i++) {
        if (cv_pix_fmts[i].cv_fmt == cv_fmt)
            return cv_pix_fmts[i].pix_fmt;
    }
    return AV_PIX_FMT_NONE;
}

uint32_t av_map_videotoolbox_format_from_pixfmt(enum AVPixelFormat pix_fmt)
{
    return av_map_videotoolbox_format_from_pixfmt2(pix_fmt, false);
}

uint32_t av_map_videotoolbox_format_from_pixfmt2(enum AVPixelFormat pix_fmt, bool full_range)
{
    int i;
    for (i = 0; i < FF_ARRAY_ELEMS(cv_pix_fmts); i++) {
        if (cv_pix_fmts[i].pix_fmt == pix_fmt && cv_pix_fmts[i].full_range == full_range)
            return cv_pix_fmts[i].cv_fmt;
    }
    return 0;
}

static int vt_pool_alloc(AVHWFramesContext *ctx)
{
    VTFramesContext *fctx = ctx->internal->priv;
    CVReturn err;
    CFNumberRef w, h, pixfmt;
    uint32_t cv_pixfmt;
    CFMutableDictionaryRef attributes, iosurface_properties;

    attributes = CFDictionaryCreateMutable(
        NULL,
        2,
        &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);

    cv_pixfmt = av_map_videotoolbox_format_from_pixfmt(ctx->sw_format);
    pixfmt = CFNumberCreate(NULL, kCFNumberSInt32Type, &cv_pixfmt);
    CFDictionarySetValue(
        attributes,
        kCVPixelBufferPixelFormatTypeKey,
        pixfmt);
    CFRelease(pixfmt);

    iosurface_properties = CFDictionaryCreateMutable(
        NULL,
        0,
        &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(attributes, kCVPixelBufferIOSurfacePropertiesKey, iosurface_properties);
    CFRelease(iosurface_properties);

    w = CFNumberCreate(NULL, kCFNumberSInt32Type, &ctx->width);
    h = CFNumberCreate(NULL, kCFNumberSInt32Type, &ctx->height);
    CFDictionarySetValue(attributes, kCVPixelBufferWidthKey, w);
    CFDictionarySetValue(attributes, kCVPixelBufferHeightKey, h);
    CFRelease(w);
    CFRelease(h);

    err = CVPixelBufferPoolCreate(
        NULL,
        NULL,
        attributes,
        &fctx->pool);
    CFRelease(attributes);

    if (err == kCVReturnSuccess)
        return 0;

    av_log(ctx, AV_LOG_ERROR, "Error creating CVPixelBufferPool: %d\n", err);
    return AVERROR_EXTERNAL;
}

static AVBufferRef *vt_dummy_pool_alloc(void *opaque, size_t size)
{
    return NULL;
}

static void vt_frames_uninit(AVHWFramesContext *ctx)
{
    VTFramesContext *fctx = ctx->internal->priv;
    if (fctx->pool) {
        CVPixelBufferPoolRelease(fctx->pool);
        fctx->pool = NULL;
    }
}

static int vt_frames_init(AVHWFramesContext *ctx)
{
    int i, ret;

    for (i = 0; i < FF_ARRAY_ELEMS(supported_formats); i++) {
        if (ctx->sw_format == supported_formats[i])
            break;
    }
    if (i == FF_ARRAY_ELEMS(supported_formats)) {
        av_log(ctx, AV_LOG_ERROR, "Pixel format '%s' is not supported\n",
               av_get_pix_fmt_name(ctx->sw_format));
        return AVERROR(ENOSYS);
    }

    // create a dummy pool so av_hwframe_get_buffer doesn't EINVAL
    if (!ctx->pool) {
        ctx->internal->pool_internal = av_buffer_pool_init2(0, ctx, vt_dummy_pool_alloc, NULL);
        if (!ctx->internal->pool_internal)
            return AVERROR(ENOMEM);
    }

    ret = vt_pool_alloc(ctx);
    if (ret < 0)
        return ret;

    return 0;
}

static void videotoolbox_buffer_release(void *opaque, uint8_t *data)
{
    CVPixelBufferRelease((CVPixelBufferRef)data);
}

static int vt_get_buffer(AVHWFramesContext *ctx, AVFrame *frame)
{
    VTFramesContext *fctx = ctx->internal->priv;

    if (ctx->pool && ctx->pool->size != 0) {
        frame->buf[0] = av_buffer_pool_get(ctx->pool);
        if (!frame->buf[0])
            return AVERROR(ENOMEM);
    } else {
        CVPixelBufferRef pixbuf;
        AVBufferRef *buf = NULL;
        CVReturn err;

        err = CVPixelBufferPoolCreatePixelBuffer(
            NULL,
            fctx->pool,
            &pixbuf
        );
        if (err != kCVReturnSuccess) {
            av_log(ctx, AV_LOG_ERROR, "Failed to create pixel buffer from pool: %d\n", err);
            return AVERROR_EXTERNAL;
        }

        buf = av_buffer_create((uint8_t *)pixbuf, 1, videotoolbox_buffer_release, NULL, 0);
        if (!buf) {
            CVPixelBufferRelease(pixbuf);
            return AVERROR(ENOMEM);
        }
        frame->buf[0] = buf;
    }

    frame->data[3] = frame->buf[0]->data;
    frame->format  = AV_PIX_FMT_VIDEOTOOLBOX;
    frame->width   = ctx->width;
    frame->height  = ctx->height;

    return 0;
}

static int vt_transfer_get_formats(AVHWFramesContext *ctx,
                                   enum AVHWFrameTransferDirection dir,
                                   enum AVPixelFormat **formats)
{
    enum AVPixelFormat *fmts = av_malloc_array(2, sizeof(*fmts));
    if (!fmts)
        return AVERROR(ENOMEM);

    fmts[0] = ctx->sw_format;
    fmts[1] = AV_PIX_FMT_NONE;

    *formats = fmts;
    return 0;
}

static void vt_unmap(AVHWFramesContext *ctx, HWMapDescriptor *hwmap)
{
    CVPixelBufferRef pixbuf = (CVPixelBufferRef)hwmap->source->data[3];

    CVPixelBufferUnlockBaseAddress(pixbuf, (uintptr_t)hwmap->priv);
}

static int vt_pixbuf_set_par(AVHWFramesContext *hwfc,
                             CVPixelBufferRef pixbuf, const AVFrame *src)
{
    CFMutableDictionaryRef par = NULL;
    CFNumberRef num = NULL, den = NULL;
    AVRational avpar = src->sample_aspect_ratio;

    if (avpar.num == 0)
        return 0;

    av_reduce(&avpar.num, &avpar.den,
                avpar.num, avpar.den,
                0xFFFFFFFF);

    num = CFNumberCreate(kCFAllocatorDefault,
                            kCFNumberIntType,
                            &avpar.num);

    den = CFNumberCreate(kCFAllocatorDefault,
                            kCFNumberIntType,
                            &avpar.den);

    par = CFDictionaryCreateMutable(kCFAllocatorDefault,
                                    2,
                                    &kCFCopyStringDictionaryKeyCallBacks,
                                    &kCFTypeDictionaryValueCallBacks);

    if (!par || !num || !den) {
        if (par) CFRelease(par);
        if (num) CFRelease(num);
        if (den) CFRelease(den);
        return AVERROR(ENOMEM);
    }

    CFDictionarySetValue(
        par,
        kCVImageBufferPixelAspectRatioHorizontalSpacingKey,
        num);
    CFDictionarySetValue(
        par,
        kCVImageBufferPixelAspectRatioVerticalSpacingKey,
        den);

    CVBufferSetAttachment(
        pixbuf,
        kCVImageBufferPixelAspectRatioKey,
        par,
        kCVAttachmentMode_ShouldPropagate
    );

    CFRelease(par);
    CFRelease(num);
    CFRelease(den);

    return 0;
}

static int vt_pixbuf_set_chromaloc(AVHWFramesContext *hwfc,
                                   CVPixelBufferRef pixbuf, const AVFrame *src)
{
    CFStringRef loc = NULL;

    switch (src->chroma_location) {
    case AVCHROMA_LOC_LEFT:
        loc = kCVImageBufferChromaLocation_Left;
        break;
    case AVCHROMA_LOC_CENTER:
        loc = kCVImageBufferChromaLocation_Center;
        break;
    case AVCHROMA_LOC_TOP:
        loc = kCVImageBufferChromaLocation_Top;
        break;
    case AVCHROMA_LOC_BOTTOM:
        loc = kCVImageBufferChromaLocation_Bottom;
        break;
    case AVCHROMA_LOC_TOPLEFT:
        loc = kCVImageBufferChromaLocation_TopLeft;
        break;
    case AVCHROMA_LOC_BOTTOMLEFT:
        loc = kCVImageBufferChromaLocation_BottomLeft;
        break;
    }

    if (loc) {
        CVBufferSetAttachment(
            pixbuf,
            kCVImageBufferChromaLocationTopFieldKey,
            loc,
            kCVAttachmentMode_ShouldPropagate);
    }

    return 0;
}

static int vt_pixbuf_set_colorspace(AVHWFramesContext *hwfc,
                                    CVPixelBufferRef pixbuf, const AVFrame *src)
{
    CFStringRef colormatrix = NULL, colorpri = NULL, colortrc = NULL;
    Float32 gamma = 0;

    switch (src->colorspace) {
    case AVCOL_SPC_BT2020_CL:
    case AVCOL_SPC_BT2020_NCL:
        if (__builtin_available(macOS 10.11, *))
            colormatrix = kCVImageBufferYCbCrMatrix_ITU_R_2020;
        else
            colormatrix = CFSTR("ITU_R_2020");
        break;
    case AVCOL_SPC_BT470BG:
    case AVCOL_SPC_SMPTE170M:
        colormatrix = kCVImageBufferYCbCrMatrix_ITU_R_601_4;
        break;
    case AVCOL_SPC_BT709:
        colormatrix = kCVImageBufferYCbCrMatrix_ITU_R_709_2;
        break;
    case AVCOL_SPC_SMPTE240M:
        colormatrix = kCVImageBufferYCbCrMatrix_SMPTE_240M_1995;
        break;
    case AVCOL_SPC_UNSPECIFIED:
        break;
    default:
        av_log(hwfc, AV_LOG_WARNING, "Color space %s is not supported.\n", av_color_space_name(src->colorspace));
    }

    switch (src->color_primaries) {
    case AVCOL_PRI_BT2020:
        if (__builtin_available(macOS 10.11, *))
            colorpri = kCVImageBufferColorPrimaries_ITU_R_2020;
        else
            colorpri = CFSTR("ITU_R_2020");
        break;
    case AVCOL_PRI_BT709:
        colorpri = kCVImageBufferColorPrimaries_ITU_R_709_2;
        break;
    case AVCOL_PRI_SMPTE170M:
        colorpri = kCVImageBufferColorPrimaries_SMPTE_C;
        break;
    case AVCOL_PRI_BT470BG:
        colorpri = kCVImageBufferColorPrimaries_EBU_3213;
        break;
    case AVCOL_PRI_UNSPECIFIED:
        break;
    default:
        av_log(hwfc, AV_LOG_WARNING, "Color primaries %s is not supported.\n", av_color_primaries_name(src->color_primaries));
    }

    switch (src->color_trc) {
    case AVCOL_TRC_SMPTE2084:
        if (__builtin_available(macOS 10.13, *))
            colortrc = kCVImageBufferTransferFunction_SMPTE_ST_2084_PQ;
        else
            colortrc = CFSTR("SMPTE_ST_2084_PQ");
        break;
    case AVCOL_TRC_BT2020_10:
    case AVCOL_TRC_BT2020_12:
        if (__builtin_available(macOS 10.11, *))
            colortrc = kCVImageBufferTransferFunction_ITU_R_2020;
        else
            colortrc = CFSTR("ITU_R_2020");
        break;
    case AVCOL_TRC_BT709:
        colortrc = kCVImageBufferTransferFunction_ITU_R_709_2;
        break;
    case AVCOL_TRC_SMPTE240M:
        colortrc = kCVImageBufferTransferFunction_SMPTE_240M_1995;
        break;
    case AVCOL_TRC_SMPTE428:
        if (__builtin_available(macOS 10.12, *))
            colortrc = kCVImageBufferTransferFunction_SMPTE_ST_428_1;
        else
            colortrc = CFSTR("SMPTE_ST_428_1");
        break;
    case AVCOL_TRC_ARIB_STD_B67:
        if (__builtin_available(macOS 10.13, *))
            colortrc = kCVImageBufferTransferFunction_ITU_R_2100_HLG;
        else
            colortrc = CFSTR("ITU_R_2100_HLG");
        break;
    case AVCOL_TRC_GAMMA22:
        gamma = 2.2;
        colortrc = kCVImageBufferTransferFunction_UseGamma;
        break;
    case AVCOL_TRC_GAMMA28:
        gamma = 2.8;
        colortrc = kCVImageBufferTransferFunction_UseGamma;
        break;
    case AVCOL_TRC_UNSPECIFIED:
        break;
    default:
        av_log(hwfc, AV_LOG_WARNING, "Color transfer function %s is not supported.\n", av_color_transfer_name(src->color_trc));
    }

    if (colormatrix) {
        CVBufferSetAttachment(
            pixbuf,
            kCVImageBufferYCbCrMatrixKey,
            colormatrix,
            kCVAttachmentMode_ShouldPropagate);
    }
    if (colorpri) {
        CVBufferSetAttachment(
            pixbuf,
            kCVImageBufferColorPrimariesKey,
            colorpri,
            kCVAttachmentMode_ShouldPropagate);
    }
    if (colortrc) {
        CVBufferSetAttachment(
            pixbuf,
            kCVImageBufferTransferFunctionKey,
            colortrc,
            kCVAttachmentMode_ShouldPropagate);
    }
    if (gamma != 0) {
        CFNumberRef gamma_level = CFNumberCreate(NULL, kCFNumberFloat32Type, &gamma);
        CVBufferSetAttachment(
            pixbuf,
            kCVImageBufferGammaLevelKey,
            gamma_level,
            kCVAttachmentMode_ShouldPropagate);
        CFRelease(gamma_level);
    }

    return 0;
}

static int vt_pixbuf_set_attachments(AVHWFramesContext *hwfc,
                                     CVPixelBufferRef pixbuf, const AVFrame *src)
{
    int ret;
    ret = vt_pixbuf_set_par(hwfc, pixbuf, src);
    if (ret < 0)
        return ret;
    ret = vt_pixbuf_set_colorspace(hwfc, pixbuf, src);
    if (ret < 0)
        return ret;
    ret = vt_pixbuf_set_chromaloc(hwfc, pixbuf, src);
    if (ret < 0)
        return ret;
    return 0;
}

static int vt_map_frame(AVHWFramesContext *ctx, AVFrame *dst, const AVFrame *src,
                        int flags)
{
    CVPixelBufferRef pixbuf = (CVPixelBufferRef)src->data[3];
    OSType pixel_format = CVPixelBufferGetPixelFormatType(pixbuf);
    CVReturn err;
    uint32_t map_flags = 0;
    int ret;
    int i;
    enum AVPixelFormat format;

    format = av_map_videotoolbox_format_to_pixfmt(pixel_format);
    if (dst->format != format) {
        av_log(ctx, AV_LOG_ERROR, "Unsupported or mismatching pixel format: %s\n",
               av_fourcc2str(pixel_format));
        return AVERROR_UNKNOWN;
    }

    if (CVPixelBufferGetWidth(pixbuf) != ctx->width ||
        CVPixelBufferGetHeight(pixbuf) != ctx->height) {
        av_log(ctx, AV_LOG_ERROR, "Inconsistent frame dimensions.\n");
        return AVERROR_UNKNOWN;
    }

    if (flags == AV_HWFRAME_MAP_READ)
        map_flags = kCVPixelBufferLock_ReadOnly;

    err = CVPixelBufferLockBaseAddress(pixbuf, map_flags);
    if (err != kCVReturnSuccess) {
        av_log(ctx, AV_LOG_ERROR, "Error locking the pixel buffer.\n");
        return AVERROR_UNKNOWN;
    }

    if (CVPixelBufferIsPlanar(pixbuf)) {
        int planes = CVPixelBufferGetPlaneCount(pixbuf);
        for (i = 0; i < planes; i++) {
            dst->data[i]     = CVPixelBufferGetBaseAddressOfPlane(pixbuf, i);
            dst->linesize[i] = CVPixelBufferGetBytesPerRowOfPlane(pixbuf, i);
        }
    } else {
        dst->data[0]     = CVPixelBufferGetBaseAddress(pixbuf);
        dst->linesize[0] = CVPixelBufferGetBytesPerRow(pixbuf);
    }

    ret = ff_hwframe_map_create(src->hw_frames_ctx, dst, src, vt_unmap,
                                (void *)(uintptr_t)map_flags);
    if (ret < 0)
        goto unlock;

    return 0;

unlock:
    CVPixelBufferUnlockBaseAddress(pixbuf, map_flags);
    return ret;
}

static int vt_transfer_data_from(AVHWFramesContext *hwfc,
                                 AVFrame *dst, const AVFrame *src)
{
    AVFrame *map;
    int err;

    if (dst->width > hwfc->width || dst->height > hwfc->height)
        return AVERROR(EINVAL);

    map = av_frame_alloc();
    if (!map)
        return AVERROR(ENOMEM);
    map->format = dst->format;

    err = vt_map_frame(hwfc, map, src, AV_HWFRAME_MAP_READ);
    if (err)
        goto fail;

    map->width  = dst->width;
    map->height = dst->height;

    err = av_frame_copy(dst, map);
    if (err)
        goto fail;

    err = 0;
fail:
    av_frame_free(&map);
    return err;
}

static int vt_transfer_data_to(AVHWFramesContext *hwfc,
                               AVFrame *dst, const AVFrame *src)
{
    AVFrame *map;
    int err;

    if (src->width > hwfc->width || src->height > hwfc->height)
        return AVERROR(EINVAL);

    map = av_frame_alloc();
    if (!map)
        return AVERROR(ENOMEM);
    map->format = src->format;

    err = vt_map_frame(hwfc, map, dst, AV_HWFRAME_MAP_WRITE | AV_HWFRAME_MAP_OVERWRITE);
    if (err)
        goto fail;

    map->width  = src->width;
    map->height = src->height;

    err = av_frame_copy(map, src);
    if (err)
        goto fail;

    err = vt_pixbuf_set_attachments(hwfc, (CVPixelBufferRef)dst->data[3], src);
    if (err)
        goto fail;

    err = 0;
fail:
    av_frame_free(&map);
    return err;
}

static int vt_device_create(AVHWDeviceContext *ctx, const char *device,
                            AVDictionary *opts, int flags)
{
    if (device && device[0]) {
        av_log(ctx, AV_LOG_ERROR, "Device selection unsupported.\n");
        return AVERROR_UNKNOWN;
    }

    return 0;
}

const HWContextType ff_hwcontext_type_videotoolbox = {
    .type                 = AV_HWDEVICE_TYPE_VIDEOTOOLBOX,
    .name                 = "videotoolbox",

    .frames_priv_size     = sizeof(VTFramesContext),

    .device_create        = vt_device_create,
    .frames_init          = vt_frames_init,
    .frames_get_buffer    = vt_get_buffer,
    .frames_get_constraints = vt_frames_get_constraints,
    .frames_uninit        = vt_frames_uninit,
    .transfer_get_formats = vt_transfer_get_formats,
    .transfer_data_to     = vt_transfer_data_to,
    .transfer_data_from   = vt_transfer_data_from,

    .pix_fmts = (const enum AVPixelFormat[]){ AV_PIX_FMT_VIDEOTOOLBOX, AV_PIX_FMT_NONE },
};
