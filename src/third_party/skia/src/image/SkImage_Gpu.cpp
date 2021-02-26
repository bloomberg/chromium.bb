/*
 * Copyright 2012 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "src/image/SkImage_Gpu.h"

#include "include/core/SkCanvas.h"
#include "include/gpu/GrBackendSurface.h"
#include "include/gpu/GrDirectContext.h"
#include "include/gpu/GrRecordingContext.h"
#include "include/gpu/GrYUVABackendTextures.h"
#include "include/private/SkImageInfoPriv.h"
#include "src/core/SkAutoPixmapStorage.h"
#include "src/core/SkBitmapCache.h"
#include "src/core/SkMipmap.h"
#include "src/core/SkScopeExit.h"
#include "src/core/SkTraceEvent.h"
#include "src/gpu/GrAHardwareBufferImageGenerator.h"
#include "src/gpu/GrAHardwareBufferUtils.h"
#include "src/gpu/GrBackendTextureImageGenerator.h"
#include "src/gpu/GrBackendUtils.h"
#include "src/gpu/GrBitmapTextureMaker.h"
#include "src/gpu/GrCaps.h"
#include "src/gpu/GrColorSpaceXform.h"
#include "src/gpu/GrDirectContextPriv.h"
#include "src/gpu/GrDrawingManager.h"
#include "src/gpu/GrGpu.h"
#include "src/gpu/GrImageContextPriv.h"
#include "src/gpu/GrImageInfo.h"
#include "src/gpu/GrImageTextureMaker.h"
#include "src/gpu/GrProxyProvider.h"
#include "src/gpu/GrRecordingContextPriv.h"
#include "src/gpu/GrRenderTargetContext.h"
#include "src/gpu/GrSemaphore.h"
#include "src/gpu/GrTexture.h"
#include "src/gpu/GrTextureAdjuster.h"
#include "src/gpu/GrTextureProxy.h"
#include "src/gpu/GrTextureProxyPriv.h"
#include "src/gpu/SkGr.h"
#include "src/gpu/gl/GrGLTexture.h"

#include <cstddef>
#include <cstring>
#include <type_traits>

SkImage_Gpu::SkImage_Gpu(sk_sp<GrImageContext> context, uint32_t uniqueID, GrSurfaceProxyView view,
                         SkColorType ct, SkAlphaType at, sk_sp<SkColorSpace> colorSpace)
        : INHERITED(std::move(context), view.proxy()->backingStoreDimensions(), uniqueID,
                    ct, at, colorSpace)
        , fView(std::move(view)) {
#ifdef SK_DEBUG
    const GrBackendFormat& format = fView.proxy()->backendFormat();
    GrColorType grCT = SkColorTypeToGrColorType(ct);
    const GrCaps* caps = this->context()->priv().caps();
    if (caps->isFormatSRGB(format)) {
        SkASSERT(grCT == GrColorType::kRGBA_8888);
        grCT = GrColorType::kRGBA_8888_SRGB;
    }
    SkASSERT(caps->isFormatCompressed(format) ||
             caps->areColorTypeAndFormatCompatible(grCT, format));
#endif
}

SkImage_Gpu::~SkImage_Gpu() {}

GrSemaphoresSubmitted SkImage_Gpu::onFlush(GrDirectContext* dContext, const GrFlushInfo& info) {
    if (!fContext->priv().matches(dContext) || dContext->abandoned()) {
        if (info.fSubmittedProc) {
            info.fSubmittedProc(info.fSubmittedContext, false);
        }
        if (info.fFinishedProc) {
            info.fFinishedProc(info.fFinishedContext);
        }
        return GrSemaphoresSubmitted::kNo;
    }

    GrSurfaceProxy* p[1] = {fView.proxy()};
    return dContext->priv().flushSurfaces(p, info);
}

sk_sp<SkImage> SkImage_Gpu::onMakeColorTypeAndColorSpace(SkColorType targetCT,
                                                         sk_sp<SkColorSpace> targetCS,
                                                         GrDirectContext* direct) const {
    if (!fContext->priv().matches(direct)) {
        return nullptr;
    }

    auto renderTargetContext = GrRenderTargetContext::MakeWithFallback(
            direct, SkColorTypeToGrColorType(targetCT), nullptr, SkBackingFit::kExact,
            this->dimensions());
    if (!renderTargetContext) {
        return nullptr;
    }

    auto texFP = GrTextureEffect::Make(*this->view(direct), this->alphaType());
    auto colorFP = GrColorSpaceXformEffect::Make(std::move(texFP),
                                                 this->colorSpace(), this->alphaType(),
                                                 targetCS.get(), this->alphaType());
    SkASSERT(colorFP);

    GrPaint paint;
    paint.setPorterDuffXPFactory(SkBlendMode::kSrc);
    paint.setColorFragmentProcessor(std::move(colorFP));

    renderTargetContext->drawRect(nullptr, std::move(paint), GrAA::kNo, SkMatrix::I(),
                                  SkRect::MakeIWH(this->width(), this->height()));
    if (!renderTargetContext->asTextureProxy()) {
        return nullptr;
    }

    targetCT = GrColorTypeToSkColorType(renderTargetContext->colorInfo().colorType());
    // MDB: this call is okay bc we know 'renderTargetContext' was exact
    return sk_make_sp<SkImage_Gpu>(sk_ref_sp(direct), kNeedNewImageUniqueID,
                                   renderTargetContext->readSurfaceView(), targetCT,
                                   this->alphaType(), std::move(targetCS));
}

sk_sp<SkImage> SkImage_Gpu::onReinterpretColorSpace(sk_sp<SkColorSpace> newCS) const {
    return sk_make_sp<SkImage_Gpu>(fContext, kNeedNewImageUniqueID, fView, this->colorType(),
                                   this->alphaType(), std::move(newCS));
}

void SkImage_Gpu::onAsyncRescaleAndReadPixels(const SkImageInfo& info,
                                              const SkIRect& srcRect,
                                              RescaleGamma rescaleGamma,
                                              SkFilterQuality rescaleQuality,
                                              ReadPixelsCallback callback,
                                              ReadPixelsContext context) {
    auto dContext = fContext->asDirectContext();
    if (!dContext) {
        // DDL TODO: buffer up the readback so it occurs when the DDL is drawn?
        callback(context, nullptr);
        return;
    }
    GrColorType ct = SkColorTypeToGrColorType(this->colorType());
    auto ctx = GrSurfaceContext::Make(dContext, fView, ct, this->alphaType(),
                                      this->refColorSpace());
    if (!ctx) {
        callback(context, nullptr);
        return;
    }
    ctx->asyncRescaleAndReadPixels(dContext, info, srcRect, rescaleGamma, rescaleQuality,
                                   callback, context);
}

void SkImage_Gpu::onAsyncRescaleAndReadPixelsYUV420(SkYUVColorSpace yuvColorSpace,
                                                    sk_sp<SkColorSpace> dstColorSpace,
                                                    const SkIRect& srcRect,
                                                    const SkISize& dstSize,
                                                    RescaleGamma rescaleGamma,
                                                    SkFilterQuality rescaleQuality,
                                                    ReadPixelsCallback callback,
                                                    ReadPixelsContext context) {
    auto dContext = fContext->asDirectContext();
    if (!dContext) {
        // DDL TODO: buffer up the readback so it occurs when the DDL is drawn?
        callback(context, nullptr);
        return;
    }
    GrColorType ct = SkColorTypeToGrColorType(this->colorType());
    auto ctx = GrSurfaceContext::Make(dContext, fView, ct, this->alphaType(),
                                      this->refColorSpace());
    if (!ctx) {
        callback(context, nullptr);
        return;
    }
    ctx->asyncRescaleAndReadPixelsYUV420(dContext,
                                         yuvColorSpace,
                                         std::move(dstColorSpace),
                                         srcRect,
                                         dstSize,
                                         rescaleGamma,
                                         rescaleQuality,
                                         callback,
                                         context);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

static sk_sp<SkImage> new_wrapped_texture_common(GrRecordingContext* rContext,
                                                 const GrBackendTexture& backendTex,
                                                 GrColorType colorType, GrSurfaceOrigin origin,
                                                 SkAlphaType at, sk_sp<SkColorSpace> colorSpace,
                                                 GrWrapOwnership ownership,
                                                 sk_sp<GrRefCntedCallback> releaseHelper) {
    if (!backendTex.isValid() || backendTex.width() <= 0 || backendTex.height() <= 0) {
        return nullptr;
    }

    GrProxyProvider* proxyProvider = rContext->priv().proxyProvider();
    sk_sp<GrTextureProxy> proxy = proxyProvider->wrapBackendTexture(
            backendTex, ownership, GrWrapCacheable::kNo, kRead_GrIOType, std::move(releaseHelper));
    if (!proxy) {
        return nullptr;
    }

    GrSwizzle swizzle = rContext->priv().caps()->getReadSwizzle(proxy->backendFormat(), colorType);
    GrSurfaceProxyView view(std::move(proxy), origin, swizzle);
    return sk_make_sp<SkImage_Gpu>(sk_ref_sp(rContext), kNeedNewImageUniqueID, std::move(view),
                                   GrColorTypeToSkColorType(colorType), at, std::move(colorSpace));
}

sk_sp<SkImage> SkImage::MakeFromCompressedTexture(GrRecordingContext* rContext,
                                                  const GrBackendTexture& tex,
                                                  GrSurfaceOrigin origin,
                                                  SkAlphaType at,
                                                  sk_sp<SkColorSpace> cs,
                                                  TextureReleaseProc releaseP,
                                                  ReleaseContext releaseC) {
    auto releaseHelper = GrRefCntedCallback::Make(releaseP, releaseC);

    if (!rContext) {
        return nullptr;
    }

    const GrCaps* caps = rContext->priv().caps();

    if (!SkImage_GpuBase::ValidateCompressedBackendTexture(caps, tex, at)) {
        return nullptr;
    }

    GrProxyProvider* proxyProvider = rContext->priv().proxyProvider();
    sk_sp<GrTextureProxy> proxy = proxyProvider->wrapCompressedBackendTexture(
            tex, kBorrow_GrWrapOwnership, GrWrapCacheable::kNo, std::move(releaseHelper));
    if (!proxy) {
        return nullptr;
    }

    CompressionType type = GrBackendFormatToCompressionType(tex.getBackendFormat());
    SkColorType ct = GrCompressionTypeToSkColorType(type);

    GrSurfaceProxyView view(std::move(proxy), origin, GrSwizzle::RGBA());
    return sk_make_sp<SkImage_Gpu>(sk_ref_sp(rContext), kNeedNewImageUniqueID,  std::move(view),
                                   ct, at, std::move(cs));
}

sk_sp<SkImage> SkImage::MakeFromTexture(GrRecordingContext* rContext,
                                        const GrBackendTexture& tex, GrSurfaceOrigin origin,
                                        SkColorType ct, SkAlphaType at, sk_sp<SkColorSpace> cs,
                                        TextureReleaseProc releaseP, ReleaseContext releaseC) {
    auto releaseHelper = GrRefCntedCallback::Make(releaseP, releaseC);

    if (!rContext) {
        return nullptr;
    }

    const GrCaps* caps = rContext->priv().caps();

    GrColorType grColorType = SkColorTypeAndFormatToGrColorType(caps, ct, tex.getBackendFormat());
    if (GrColorType::kUnknown == grColorType) {
        return nullptr;
    }

    if (!SkImage_GpuBase::ValidateBackendTexture(caps, tex, grColorType, ct, at, cs)) {
        return nullptr;
    }

    return new_wrapped_texture_common(rContext, tex, grColorType, origin, at, std::move(cs),
                                      kBorrow_GrWrapOwnership, std::move(releaseHelper));
}

sk_sp<SkImage> SkImage::MakeFromAdoptedTexture(GrRecordingContext* rContext,
                                               const GrBackendTexture& tex, GrSurfaceOrigin origin,
                                               SkColorType ct, SkAlphaType at,
                                               sk_sp<SkColorSpace> cs) {
    auto dContext = GrAsDirectContext(rContext);
    if (!dContext) {
        // We have a DDL context and we don't support adopted textures for them.
        return nullptr;
    }

    const GrCaps* caps = dContext->priv().caps();

    GrColorType grColorType = SkColorTypeAndFormatToGrColorType(caps, ct, tex.getBackendFormat());
    if (GrColorType::kUnknown == grColorType) {
        return nullptr;
    }

    if (!SkImage_GpuBase::ValidateBackendTexture(caps, tex, grColorType, ct, at, cs)) {
        return nullptr;
    }

    return new_wrapped_texture_common(dContext, tex, grColorType, origin, at, std::move(cs),
                                      kAdopt_GrWrapOwnership, nullptr);
}

sk_sp<SkImage> SkImage::MakeTextureFromCompressed(GrDirectContext* direct, sk_sp<SkData> data,
                                                  int width, int height, CompressionType type,
                                                  GrMipmapped mipMapped,
                                                  GrProtected isProtected) {
    if (!direct || !data) {
        return nullptr;
    }

    GrBackendFormat beFormat = direct->compressedBackendFormat(type);
    if (!beFormat.isValid()) {
        sk_sp<SkImage> tmp = MakeRasterFromCompressed(std::move(data), width, height, type);
        if (!tmp) {
            return nullptr;
        }
        return tmp->makeTextureImage(direct, mipMapped);
    }

    GrProxyProvider* proxyProvider = direct->priv().proxyProvider();
    sk_sp<GrTextureProxy> proxy = proxyProvider->createCompressedTextureProxy(
            {width, height}, SkBudgeted::kYes, mipMapped, isProtected, type, std::move(data));
    if (!proxy) {
        return nullptr;
    }
    GrSurfaceProxyView view(std::move(proxy));

    SkColorType colorType = GrCompressionTypeToSkColorType(type);

    return sk_make_sp<SkImage_Gpu>(sk_ref_sp(direct), kNeedNewImageUniqueID, std::move(view),
                                   colorType, kOpaque_SkAlphaType, nullptr);
}

sk_sp<SkImage> SkImage_Gpu::ConvertYUVATexturesToRGB(GrRecordingContext* rContext,
                                                     SkYUVColorSpace yuvColorSpace,
                                                     const GrBackendTexture yuvaTextures[],
                                                     const SkYUVAIndex yuvaIndices[4],
                                                     SkISize size,
                                                     GrSurfaceOrigin origin,
                                                     GrRenderTargetContext* renderTargetContext,
                                                     sk_sp<GrRefCntedCallback> releaseHelper) {
    SkASSERT(renderTargetContext);

    int numTextures;
    if (!SkYUVAIndex::AreValidIndices(yuvaIndices, &numTextures)) {
        return nullptr;
    }

    GrSurfaceProxyView tempViews[4];
    if (!SkImage_GpuBase::MakeTempTextureProxies(rContext, yuvaTextures, numTextures, yuvaIndices,
                                                 origin, tempViews, std::move(releaseHelper))) {
        return nullptr;
    }

    const SkRect rect = SkRect::MakeIWH(size.width(), size.height());
    const GrCaps& caps = *rContext->priv().caps();
    if (!RenderYUVAToRGBA(caps, renderTargetContext, rect, yuvColorSpace, nullptr,
                          tempViews, yuvaIndices)) {
        return nullptr;
    }

    SkColorType ct = GrColorTypeToSkColorType(renderTargetContext->colorInfo().colorType());
    SkAlphaType at = GetAlphaTypeFromYUVAIndices(yuvaIndices);
    // MDB: this call is okay bc we know 'renderTargetContext' was exact
    return sk_make_sp<SkImage_Gpu>(sk_ref_sp(rContext), kNeedNewImageUniqueID,
                                   renderTargetContext->readSurfaceView(), ct, at,
                                   renderTargetContext->colorInfo().refColorSpace());
}

static sk_sp<SkImage> make_flattened_image_with_external_backend(
        GrRecordingContext* rContext,
        SkYUVColorSpace yuvColorSpace,
        const GrBackendTexture yuvaTextures[],
        const SkYUVAIndex yuvaIndices[4],
        SkISize imageSize,
        GrSurfaceOrigin imageOrigin,
        const GrBackendTexture& backendTexture,
        GrColorType colorType,
        sk_sp<SkColorSpace> imageColorSpace,
        sk_sp<GrRefCntedCallback> yuvaReleaseHelper,
        sk_sp<GrRefCntedCallback> rgbaReleaseHelper) {
    const GrCaps* caps = rContext->priv().caps();
    SkAlphaType at = SkImage_GpuBase::GetAlphaTypeFromYUVAIndices(yuvaIndices);
    if (!SkImage_Gpu::ValidateBackendTexture(caps, backendTexture, colorType,
                                             kRGBA_8888_SkColorType, at, nullptr)) {
        return nullptr;
    }

    // Needs to create a render target with external texture
    // in order to draw to it for the yuv->rgb conversion.
    auto renderTargetContext = GrRenderTargetContext::MakeFromBackendTexture(
            rContext, colorType, std::move(imageColorSpace), backendTexture, 1, imageOrigin,
            nullptr, std::move(rgbaReleaseHelper));
    if (!renderTargetContext) {
        return nullptr;
    }

    return SkImage_Gpu::ConvertYUVATexturesToRGB(rContext, yuvColorSpace, yuvaTextures, yuvaIndices,
                                                 imageSize, imageOrigin, renderTargetContext.get(),
                                                 std::move(yuvaReleaseHelper));
}

// Some YUVA factories infer the YUVAIndices. This helper identifies the channel to use for single
// channel textures.
static SkColorChannel get_single_channel(const GrBackendTexture& tex) {
    switch (tex.getBackendFormat().channelMask()) {
        case kGray_SkColorChannelFlag:  // Gray can be read as any of kR, kG, kB.
        case kRed_SkColorChannelFlag:
            return SkColorChannel::kR;
        case kAlpha_SkColorChannelFlag:
            return SkColorChannel::kA;
        default:  // multiple channels in the texture. Guess kR.
            return SkColorChannel::kR;
    }
}

sk_sp<SkImage> SkImage::MakeFromYUVATexturesCopyToExternal(
        GrRecordingContext* context,
        const GrYUVABackendTextures& yuvaTextures,
        const GrBackendTexture& rgbaResultTexture,
        SkColorType colorType,
        sk_sp<SkColorSpace> imageColorSpace,
        TextureReleaseProc yuvaReleaseProc,
        ReleaseContext yuvaReleaseContext,
        TextureReleaseProc rgbaReleaseProc,
        ReleaseContext rgbaReleaseContext) {
    auto yuvaReleaseHelper = GrRefCntedCallback::Make(yuvaReleaseProc, yuvaReleaseContext);
    auto rgbaReleaseHelper = GrRefCntedCallback::Make(rgbaReleaseProc, rgbaReleaseContext);

    SkYUVAIndex yuvaIndices[4];
    int numTextures;
    if (!yuvaTextures.toYUVAIndices(yuvaIndices) ||
        !SkYUVAIndex::AreValidIndices(yuvaIndices, &numTextures)) {
        return nullptr;
    }
    SkASSERT(numTextures == yuvaTextures.numPlanes());
    if (!rgbaResultTexture.isValid() ||
        rgbaResultTexture.dimensions() != yuvaTextures.yuvaInfo().dimensions()) {
        return nullptr;
    }
    return make_flattened_image_with_external_backend(context,
                                                      yuvaTextures.yuvaInfo().yuvColorSpace(),
                                                      yuvaTextures.textures().data(),
                                                      yuvaIndices,
                                                      rgbaResultTexture.dimensions(),
                                                      yuvaTextures.textureOrigin(),
                                                      rgbaResultTexture,
                                                      SkColorTypeToGrColorType(colorType),
                                                      std::move(imageColorSpace),
                                                      std::move(yuvaReleaseHelper),
                                                      std::move(rgbaReleaseHelper));
}

sk_sp<SkImage> SkImage::MakeFromYUVTexturesCopyWithExternalBackend(
        GrRecordingContext* ctx, SkYUVColorSpace yuvColorSpace,
        const GrBackendTexture yuvTextures[3], GrSurfaceOrigin imageOrigin,
        const GrBackendTexture& backendTexture, sk_sp<SkColorSpace> imageColorSpace) {
    SkYUVAIndex yuvaIndices[4] = {
            SkYUVAIndex{0, get_single_channel(yuvTextures[0])},
            SkYUVAIndex{1, get_single_channel(yuvTextures[1])},
            SkYUVAIndex{2, get_single_channel(yuvTextures[2])},
            SkYUVAIndex{-1, SkColorChannel::kA}};
    return make_flattened_image_with_external_backend(ctx,
                                                      yuvColorSpace,
                                                      yuvTextures,
                                                      yuvaIndices,
                                                      yuvTextures[0].dimensions(),
                                                      imageOrigin,
                                                      backendTexture,
                                                      GrColorType::kRGBA_8888,
                                                      std::move(imageColorSpace),
                                                      nullptr,
                                                      nullptr);
}

sk_sp<SkImage> SkImage::MakeFromNV12TexturesCopyWithExternalBackend(
        GrRecordingContext* ctx,
        SkYUVColorSpace yuvColorSpace,
        const GrBackendTexture nv12Textures[2],
        GrSurfaceOrigin imageOrigin,
        const GrBackendTexture& backendTexture,
        sk_sp<SkColorSpace> imageColorSpace,
        TextureReleaseProc textureReleaseProc,
        ReleaseContext releaseContext) {
    auto releaseHelper = GrRefCntedCallback::Make(textureReleaseProc, releaseContext);

    SkYUVAIndex yuvaIndices[4] = {
            SkYUVAIndex{0, get_single_channel(nv12Textures[0])},
            SkYUVAIndex{1, SkColorChannel::kR},
            SkYUVAIndex{1, SkColorChannel::kG},
            SkYUVAIndex{-1, SkColorChannel::kA}};
    SkISize size{nv12Textures[0].width(), nv12Textures[0].height()};

    return make_flattened_image_with_external_backend(ctx,
                                                      yuvColorSpace,
                                                      nv12Textures,
                                                      yuvaIndices,
                                                      size,
                                                      imageOrigin,
                                                      backendTexture,
                                                      GrColorType::kRGBA_8888,
                                                      std::move(imageColorSpace),
                                                      /* plane release helper*/ nullptr,
                                                      std::move(releaseHelper));
}

static sk_sp<SkImage> create_image_from_producer(GrRecordingContext* context,
                                                 GrTextureProducer* producer,
                                                 uint32_t id, GrMipmapped mipMapped) {
    auto view = producer->view(mipMapped);
    if (!view) {
        return nullptr;
    }
    return sk_make_sp<SkImage_Gpu>(sk_ref_sp(context), id, std::move(view),
                                   GrColorTypeToSkColorType(producer->colorType()),
                                   producer->alphaType(), sk_ref_sp(producer->colorSpace()));
}


sk_sp<SkImage> SkImage::makeTextureImage(GrDirectContext* dContext,
                                         GrMipmapped mipMapped,
                                         SkBudgeted budgeted) const {
    if (!dContext) {
        return nullptr;
    }

    if (this->isTextureBacked()) {
        if (!as_IB(this)->context()->priv().matches(dContext)) {
            return nullptr;
        }

        // TODO: Don't flatten YUVA images here.
        const GrSurfaceProxyView* view = as_IB(this)->view(dContext);
        SkASSERT(view && view->asTextureProxy());

        if (mipMapped == GrMipmapped::kNo || view->asTextureProxy()->mipmapped() == mipMapped ||
            !dContext->priv().caps()->mipmapSupport()) {
            return sk_ref_sp(const_cast<SkImage*>(this));
        }
        auto copy = GrCopyBaseMipMapToView(dContext, *view, budgeted);
        if (!copy) {
            return nullptr;
        }
        return sk_make_sp<SkImage_Gpu>(sk_ref_sp(dContext), this->uniqueID(), copy,
                                       this->colorType(), this->alphaType(), this->refColorSpace());
    }

    auto policy = budgeted == SkBudgeted::kYes ? GrImageTexGenPolicy::kNew_Uncached_Budgeted
                                               : GrImageTexGenPolicy::kNew_Uncached_Unbudgeted;
    if (this->isLazyGenerated()) {
        GrImageTextureMaker maker(dContext, this, policy);
        return create_image_from_producer(dContext, &maker, this->uniqueID(), mipMapped);
    }

    if (const SkBitmap* bmp = as_IB(this)->onPeekBitmap()) {
        GrBitmapTextureMaker maker(dContext, *bmp, policy);
        return create_image_from_producer(dContext, &maker, this->uniqueID(), mipMapped);
    }
    return nullptr;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

sk_sp<SkImage> SkImage_Gpu::MakePromiseTexture(GrRecordingContext* context,
                                               const GrBackendFormat& backendFormat,
                                               SkISize dimensions,
                                               GrMipmapped mipMapped,
                                               GrSurfaceOrigin origin,
                                               SkColorType colorType,
                                               SkAlphaType alphaType,
                                               sk_sp<SkColorSpace> colorSpace,
                                               PromiseImageTextureFulfillProc textureFulfillProc,
                                               PromiseImageTextureReleaseProc textureReleaseProc,
                                               PromiseImageTextureContext textureContext) {
    // Our contract is that we will always call the release proc even on failure.
    // We use the helper to convey the context, so we need to ensure make doesn't fail.
    textureReleaseProc = textureReleaseProc ? textureReleaseProc : [](void*) {};
    auto releaseHelper = GrRefCntedCallback::Make(textureReleaseProc, textureContext);
    SkImageInfo info = SkImageInfo::Make(dimensions, colorType, alphaType, colorSpace);
    if (!SkImageInfoIsValid(info)) {
        return nullptr;
    }

    if (!context) {
        return nullptr;
    }

    if (dimensions.isEmpty()) {
        return nullptr;
    }

    GrColorType grColorType = SkColorTypeAndFormatToGrColorType(context->priv().caps(),
                                                                colorType,
                                                                backendFormat);
    if (GrColorType::kUnknown == grColorType) {
        return nullptr;
    }

    if (!context->priv().caps()->areColorTypeAndFormatCompatible(grColorType, backendFormat)) {
        return nullptr;
    }

    auto proxy = MakePromiseImageLazyProxy(context,
                                           dimensions,
                                           backendFormat,
                                           mipMapped,
                                           textureFulfillProc,
                                           std::move(releaseHelper));
    if (!proxy) {
        return nullptr;
    }
    GrSwizzle swizzle = context->priv().caps()->getReadSwizzle(backendFormat, grColorType);
    GrSurfaceProxyView view(std::move(proxy), origin, swizzle);
    // CONTEXT TODO: rm this usage of the 'backdoor' to create an image
    return sk_make_sp<SkImage_Gpu>(sk_ref_sp(context), kNeedNewImageUniqueID,
                                   std::move(view), colorType, alphaType, std::move(colorSpace));
}

///////////////////////////////////////////////////////////////////////////////////////////////////

sk_sp<SkImage> SkImage::MakeCrossContextFromPixmap(GrDirectContext* dContext,
                                                   const SkPixmap& originalPixmap, bool buildMips,
                                                   bool limitToMaxTextureSize) {
    // Some backends or drivers don't support (safely) moving resources between contexts
    if (!dContext || !dContext->priv().caps()->crossContextTextureSupport()) {
        return SkImage::MakeRasterCopy(originalPixmap);
    }

    // If non-power-of-two mipmapping isn't supported, ignore the client's request
    if (!dContext->priv().caps()->mipmapSupport()) {
        buildMips = false;
    }

    const SkPixmap* pixmap = &originalPixmap;
    SkAutoPixmapStorage resized;
    int maxTextureSize = dContext->priv().caps()->maxTextureSize();
    int maxDim = std::max(originalPixmap.width(), originalPixmap.height());
    if (limitToMaxTextureSize && maxDim > maxTextureSize) {
        float scale = static_cast<float>(maxTextureSize) / maxDim;
        int newWidth = std::min(static_cast<int>(originalPixmap.width() * scale), maxTextureSize);
        int newHeight = std::min(static_cast<int>(originalPixmap.height() * scale), maxTextureSize);
        SkImageInfo info = originalPixmap.info().makeWH(newWidth, newHeight);
        if (!resized.tryAlloc(info) || !originalPixmap.scalePixels(resized, kLow_SkFilterQuality)) {
            return nullptr;
        }
        pixmap = &resized;
    }
    // Turn the pixmap into a GrTextureProxy
    SkBitmap bmp;
    bmp.installPixels(*pixmap);
    GrBitmapTextureMaker bitmapMaker(dContext, bmp, GrImageTexGenPolicy::kNew_Uncached_Budgeted);
    GrMipmapped mipMapped = buildMips ? GrMipmapped::kYes : GrMipmapped::kNo;
    auto view = bitmapMaker.view(mipMapped);
    if (!view) {
        return SkImage::MakeRasterCopy(*pixmap);
    }

    sk_sp<GrTexture> texture = sk_ref_sp(view.proxy()->peekTexture());

    // Flush any writes or uploads
    dContext->priv().flushSurface(view.proxy());
    GrGpu* gpu = dContext->priv().getGpu();

    std::unique_ptr<GrSemaphore> sema = gpu->prepareTextureForCrossContextUsage(texture.get());

    SkColorType skCT = GrColorTypeToSkColorType(bitmapMaker.colorType());
    auto gen = GrBackendTextureImageGenerator::Make(std::move(texture), view.origin(),
                                                    std::move(sema), skCT,
                                                    pixmap->alphaType(),
                                                    pixmap->info().refColorSpace());
    return SkImage::MakeFromGenerator(std::move(gen));
}

#if defined(SK_BUILD_FOR_ANDROID) && __ANDROID_API__ >= 26
sk_sp<SkImage> SkImage::MakeFromAHardwareBuffer(AHardwareBuffer* graphicBuffer, SkAlphaType at,
                                                sk_sp<SkColorSpace> cs,
                                                GrSurfaceOrigin surfaceOrigin) {
    auto gen = GrAHardwareBufferImageGenerator::Make(graphicBuffer, at, cs, surfaceOrigin);
    return SkImage::MakeFromGenerator(std::move(gen));
}

sk_sp<SkImage> SkImage::MakeFromAHardwareBufferWithData(GrDirectContext* dContext,
                                                        const SkPixmap& pixmap,
                                                        AHardwareBuffer* hardwareBuffer,
                                                        GrSurfaceOrigin surfaceOrigin) {
    AHardwareBuffer_Desc bufferDesc;
    AHardwareBuffer_describe(hardwareBuffer, &bufferDesc);

    if (!SkToBool(bufferDesc.usage & AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE)) {
        return nullptr;
    }

    GrBackendFormat backendFormat = GrAHardwareBufferUtils::GetBackendFormat(dContext,
                                                                             hardwareBuffer,
                                                                             bufferDesc.format,
                                                                             true);

    if (!backendFormat.isValid()) {
        return nullptr;
    }

    GrAHardwareBufferUtils::DeleteImageProc deleteImageProc = nullptr;
    GrAHardwareBufferUtils::UpdateImageProc updateImageProc = nullptr;
    GrAHardwareBufferUtils::TexImageCtx deleteImageCtx = nullptr;

    GrBackendTexture backendTexture =
            GrAHardwareBufferUtils::MakeBackendTexture(dContext, hardwareBuffer,
                                                       bufferDesc.width, bufferDesc.height,
                                                       &deleteImageProc, &updateImageProc,
                                                       &deleteImageCtx, false, backendFormat, true);
    if (!backendTexture.isValid()) {
        return nullptr;
    }
    SkASSERT(deleteImageProc);

    auto releaseHelper = GrRefCntedCallback::Make(deleteImageProc, deleteImageCtx);

    SkColorType colorType =
            GrAHardwareBufferUtils::GetSkColorTypeFromBufferFormat(bufferDesc.format);

    GrColorType grColorType = SkColorTypeToGrColorType(colorType);

    GrProxyProvider* proxyProvider = dContext->priv().proxyProvider();
    if (!proxyProvider) {
        return nullptr;
    }

    sk_sp<GrTextureProxy> proxy = proxyProvider->wrapBackendTexture(
            backendTexture, kBorrow_GrWrapOwnership, GrWrapCacheable::kNo, kRW_GrIOType,
            std::move(releaseHelper));
    if (!proxy) {
        return nullptr;
    }

    sk_sp<SkColorSpace> cs = pixmap.refColorSpace();
    SkAlphaType at =  pixmap.alphaType();

    GrSwizzle swizzle = dContext->priv().caps()->getReadSwizzle(backendFormat, grColorType);
    GrSurfaceProxyView view(std::move(proxy), surfaceOrigin, swizzle);
    sk_sp<SkImage> image = sk_make_sp<SkImage_Gpu>(sk_ref_sp(dContext), kNeedNewImageUniqueID, view,
                                                   colorType, at, cs);
    if (!image) {
        return nullptr;
    }

    GrDrawingManager* drawingManager = dContext->priv().drawingManager();
    if (!drawingManager) {
        return nullptr;
    }

    GrSurfaceContext surfaceContext(dContext, std::move(view),
                                    SkColorTypeToGrColorType(pixmap.colorType()),
                                    pixmap.alphaType(), cs);

    SkImageInfo srcInfo = SkImageInfo::Make(bufferDesc.width, bufferDesc.height, colorType, at,
                                            std::move(cs));
    surfaceContext.writePixels(dContext, srcInfo, pixmap.addr(0, 0), pixmap.rowBytes(), {0, 0});

    GrSurfaceProxy* p[1] = {surfaceContext.asSurfaceProxy()};
    drawingManager->flush(p, SkSurface::BackendSurfaceAccess::kNoAccess, {}, nullptr);

    return image;
}
#endif

///////////////////////////////////////////////////////////////////////////////////////////////////

bool SkImage::MakeBackendTextureFromSkImage(GrDirectContext* direct,
                                            sk_sp<SkImage> image,
                                            GrBackendTexture* backendTexture,
                                            BackendTextureReleaseProc* releaseProc) {
    if (!image || !backendTexture || !releaseProc) {
        return false;
    }

    // Ensure we have a texture backed image.
    if (!image->isTextureBacked()) {
        image = image->makeTextureImage(direct);
        if (!image) {
            return false;
        }
    }
    SkImage_GpuBase* gpuImage = static_cast<SkImage_GpuBase*>(as_IB(image));
    GrTexture* texture = gpuImage->getTexture();
    if (!texture) {
        // In context-loss cases, we may not have a texture.
        return false;
    }

    // If the image's context doesn't match the provided context, fail.
    if (texture->getContext() != direct) {
        return false;
    }

    // Flush any pending IO on the texture.
    direct->priv().flushSurface(as_IB(image)->peekProxy());

    // We must make a copy of the image if the image is not unique, if the GrTexture owned by the
    // image is not unique, or if the texture wraps an external object.
    if (!image->unique() || !texture->unique() ||
        texture->resourcePriv().refsWrappedObjects()) {
        // onMakeSubset will always copy the image.
        image = as_IB(image)->onMakeSubset(image->bounds(), direct);
        if (!image) {
            return false;
        }

        texture = gpuImage->getTexture();
        if (!texture) {
            return false;
        }

        // Flush to ensure that the copy is completed before we return the texture.
        direct->priv().flushSurface(as_IB(image)->peekProxy());
    }

    SkASSERT(!texture->resourcePriv().refsWrappedObjects());
    SkASSERT(texture->unique());
    SkASSERT(image->unique());

    // Take a reference to the GrTexture and release the image.
    sk_sp<GrTexture> textureRef(SkSafeRef(texture));
    image = nullptr;

    // Steal the backend texture from the GrTexture, releasing the GrTexture in the process.
    return GrTexture::StealBackendTexture(std::move(textureRef), backendTexture, releaseProc);
}
