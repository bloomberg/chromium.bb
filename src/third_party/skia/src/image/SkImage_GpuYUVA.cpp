/*
 * Copyright 2018 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <cstddef>
#include <cstring>
#include <type_traits>

#include "include/core/SkYUVAPixmaps.h"
#include "include/core/SkYUVASizeInfo.h"
#include "include/gpu/GrDirectContext.h"
#include "include/gpu/GrRecordingContext.h"
#include "include/gpu/GrYUVABackendTextures.h"
#include "src/core/SkAutoPixmapStorage.h"
#include "src/core/SkMipmap.h"
#include "src/core/SkScopeExit.h"
#include "src/gpu/GrBitmapTextureMaker.h"
#include "src/gpu/GrClip.h"
#include "src/gpu/GrDirectContextPriv.h"
#include "src/gpu/GrGpu.h"
#include "src/gpu/GrImageContextPriv.h"
#include "src/gpu/GrRecordingContextPriv.h"
#include "src/gpu/GrRenderTargetContext.h"
#include "src/gpu/GrTexture.h"
#include "src/gpu/GrTextureProducer.h"
#include "src/gpu/SkGr.h"
#include "src/gpu/effects/GrYUVtoRGBEffect.h"
#include "src/image/SkImage_Gpu.h"
#include "src/image/SkImage_GpuYUVA.h"

static constexpr auto kAssumedColorType = kRGBA_8888_SkColorType;

SkImage_GpuYUVA::SkImage_GpuYUVA(sk_sp<GrImageContext> context,
                                 SkISize size,
                                 uint32_t uniqueID,
                                 SkYUVColorSpace colorSpace,
                                 GrSurfaceProxyView views[],
                                 int numViews,
                                 const SkYUVAIndex yuvaIndices[4],
                                 sk_sp<SkColorSpace> imageColorSpace)
        : INHERITED(std::move(context),
                    size,
                    uniqueID,
                    kAssumedColorType,
                    // If an alpha channel is present we always switch to kPremul. This is because,
                    // although the planar data is always un-premul, the final interleaved RGB image
                    // is/would-be premul.
                    GetAlphaTypeFromYUVAIndices(yuvaIndices),
                    std::move(imageColorSpace))
        , fNumViews(numViews)
        , fYUVColorSpace(colorSpace) {
    // The caller should have done this work, just verifying
    SkDEBUGCODE(int textureCount;)
    SkASSERT(SkYUVAIndex::AreValidIndices(yuvaIndices, &textureCount));
    SkASSERT(textureCount == fNumViews);

    for (int i = 0; i < numViews; ++i) {
        fViews[i] = std::move(views[i]);
    }
    memcpy(fYUVAIndices, yuvaIndices, 4 * sizeof(SkYUVAIndex));
}

// For onMakeColorSpace()
SkImage_GpuYUVA::SkImage_GpuYUVA(sk_sp<GrImageContext> context, const SkImage_GpuYUVA* image,
                                 sk_sp<SkColorSpace> targetCS)
        : INHERITED(std::move(context), image->dimensions(), kNeedNewImageUniqueID,
                    kAssumedColorType,
                    // If an alpha channel is present we always switch to kPremul. This is because,
                    // although the planar data is always un-premul, the final interleaved RGB image
                    // is/would-be premul.
                    GetAlphaTypeFromYUVAIndices(image->fYUVAIndices), std::move(targetCS))
        , fNumViews(image->fNumViews)
        , fYUVColorSpace(image->fYUVColorSpace)
        // Since null fFromColorSpace means no GrColorSpaceXform, we turn a null
        // image->refColorSpace() into an explicit SRGB.
        , fFromColorSpace(image->colorSpace() ? image->refColorSpace() : SkColorSpace::MakeSRGB()) {
    // The caller should have done this work, just verifying
    SkDEBUGCODE(int textureCount;)
    SkASSERT(SkYUVAIndex::AreValidIndices(image->fYUVAIndices, &textureCount));
    SkASSERT(textureCount == fNumViews);

    if (image->fRGBView.proxy()) {
        fRGBView = image->fRGBView;  // we ref in this case, not move
    } else {
        for (int i = 0; i < fNumViews; ++i) {
            fViews[i] = image->fViews[i];  // we ref in this case, not move
        }
    }
    memcpy(fYUVAIndices, image->fYUVAIndices, 4 * sizeof(SkYUVAIndex));
}

bool SkImage_GpuYUVA::setupMipmapsForPlanes(GrRecordingContext* context) const {
    // We shouldn't get here if the planes were already flattened to RGBA.
    SkASSERT(fViews[0].proxy() && !fRGBView.proxy());
    if (!context || !fContext->priv().matches(context)) {
        return false;
    }
    GrSurfaceProxyView newViews[4];
    if (!context->priv().caps()->mipmapSupport()) {
        // We succeed in this case by doing nothing.
        return true;
    }
    for (int i = 0; i < fNumViews; ++i) {
        auto* t = fViews[i].asTextureProxy();
        if (t->mipmapped() == GrMipmapped::kNo && (t->width() > 1 || t->height() > 1)) {
            if (!(newViews[i] = GrCopyBaseMipMapToView(context, fViews[i]))) {
                return false;
            }
        } else {
            newViews[i] = fViews[i];
        }
    }
    for (int i = 0; i < fNumViews; ++i) {
        fViews[i] = std::move(newViews[i]);
    }
    return true;
}

//////////////////////////////////////////////////////////////////////////////////////////////////

GrSemaphoresSubmitted SkImage_GpuYUVA::onFlush(GrDirectContext* dContext, const GrFlushInfo& info) {
    if (!fContext->priv().matches(dContext) || dContext->abandoned()) {
        if (info.fSubmittedProc) {
            info.fSubmittedProc(info.fSubmittedContext, false);
        }
        if (info.fFinishedProc) {
            info.fFinishedProc(info.fFinishedContext);
        }
        return GrSemaphoresSubmitted::kNo;
    }

    GrSurfaceProxy* proxies[4] = {fViews[0].proxy(), fViews[1].proxy(), fViews[2].proxy(),
                                  fViews[3].proxy()};
    size_t numProxies = fNumViews;
    if (fRGBView.proxy()) {
        // Either we've already flushed the flattening draw or the flattening is unflushed. In the
        // latter case it should still be ok to just pass fRGBView proxy because it in turn depends
        // on the planar proxies and will cause all of their work to flush as well.
        proxies[0] = fRGBView.proxy();
        numProxies = 1;
    }
    return dContext->priv().flushSurfaces({proxies, numProxies}, info);
}

GrTextureProxy* SkImage_GpuYUVA::peekProxy() const { return fRGBView.asTextureProxy(); }

void SkImage_GpuYUVA::flattenToRGB(GrRecordingContext* context) const {
    if (fRGBView.proxy()) {
        return;
    }

    if (!context || !fContext->priv().matches(context)) {
        return;
    }

    // Needs to create a render target in order to draw to it for the yuv->rgb conversion.
    auto renderTargetContext = GrRenderTargetContext::Make(
            context, GrColorType::kRGBA_8888, this->refColorSpace(), SkBackingFit::kExact,
            this->dimensions(), 1, GrMipmapped::kNo, GrProtected::kNo);
    if (!renderTargetContext) {
        return;
    }

    sk_sp<GrColorSpaceXform> colorSpaceXform;
    if (fFromColorSpace) {
        colorSpaceXform = GrColorSpaceXform::Make(fFromColorSpace.get(), this->alphaType(),
                                                  this->colorSpace(), this->alphaType());
    }
    const SkRect rect = SkRect::MakeIWH(this->width(), this->height());
    const GrCaps& caps = *context->priv().caps();
    if (!RenderYUVAToRGBA(caps, renderTargetContext.get(), rect, fYUVColorSpace,
                          std::move(colorSpaceXform), fViews, fYUVAIndices)) {
        return;
    }

    fRGBView = renderTargetContext->readSurfaceView();
    SkASSERT(fRGBView.swizzle() == GrSwizzle());
    for (auto& v : fViews) {
        v.reset();
    }
}

GrSurfaceProxyView SkImage_GpuYUVA::refMippedView(GrRecordingContext* context) const {
    // if invalid or already has miplevels
    this->flattenToRGB(context);
    if (!fRGBView || fRGBView.asTextureProxy()->mipmapped() == GrMipmapped::kYes) {
        return fRGBView;
    }

    // need to generate mips for the proxy
    auto mippedView = GrCopyBaseMipMapToView(context, fRGBView);
    if (!mippedView) {
        return {};
    }

    fRGBView = std::move(mippedView);
    return fRGBView;
}

const GrSurfaceProxyView* SkImage_GpuYUVA::view(GrRecordingContext* context) const {
    this->flattenToRGB(context);
    if (!fRGBView.proxy()) {
        return nullptr;
    }
    return &fRGBView;
}

//////////////////////////////////////////////////////////////////////////////////////////////////

sk_sp<SkImage> SkImage_GpuYUVA::onMakeColorTypeAndColorSpace(
        SkColorType, sk_sp<SkColorSpace> targetCS, GrDirectContext* direct) const {
    // We explicitly ignore color type changes, for now.

    // we may need a mutex here but for now we expect usage to be in a single thread
    if (fOnMakeColorSpaceTarget &&
        SkColorSpace::Equals(targetCS.get(), fOnMakeColorSpaceTarget.get())) {
        return fOnMakeColorSpaceResult;
    }
    sk_sp<SkImage> result = sk_sp<SkImage>(new SkImage_GpuYUVA(sk_ref_sp(direct), this, targetCS));
    if (result) {
        fOnMakeColorSpaceTarget = targetCS;
        fOnMakeColorSpaceResult = result;
    }
    return result;
}

sk_sp<SkImage> SkImage_GpuYUVA::onReinterpretColorSpace(sk_sp<SkColorSpace> newCS) const {
    return sk_make_sp<SkImage_GpuYUVA>(fContext, this->dimensions(), kNeedNewImageUniqueID,
                                       fYUVColorSpace, fViews, fNumViews, fYUVAIndices,
                                       std::move(newCS));
}

//////////////////////////////////////////////////////////////////////////////////////////////////

sk_sp<SkImage> SkImage::MakeFromYUVATextures(GrRecordingContext* context,
                                             const GrYUVABackendTextures& yuvaTextures,
                                             sk_sp<SkColorSpace> imageColorSpace,
                                             TextureReleaseProc textureReleaseProc,
                                             ReleaseContext releaseContext) {
    auto releaseHelper = GrRefCntedCallback::Make(textureReleaseProc, releaseContext);

    SkYUVAIndex yuvaIndices[4];
    int numTextures;
    if (!yuvaTextures.toYUVAIndices(yuvaIndices) ||
        !SkYUVAIndex::AreValidIndices(yuvaIndices, &numTextures)) {
        return nullptr;
    }
    SkASSERT(numTextures == yuvaTextures.numPlanes());

    GrSurfaceProxyView tempViews[4];
    if (!SkImage_GpuBase::MakeTempTextureProxies(context,
                                                 yuvaTextures.textures().data(),
                                                 numTextures,
                                                 yuvaIndices,
                                                 yuvaTextures.textureOrigin(),
                                                 tempViews,
                                                 std::move(releaseHelper))) {
        return nullptr;
    }

    return sk_make_sp<SkImage_GpuYUVA>(sk_ref_sp(context),
                                       yuvaTextures.yuvaInfo().dimensions(),
                                       kNeedNewImageUniqueID,
                                       yuvaTextures.yuvaInfo().yuvColorSpace(),
                                       tempViews,
                                       numTextures,
                                       yuvaIndices,
                                       imageColorSpace);
}

sk_sp<SkImage> SkImage::MakeFromYUVATextures(GrRecordingContext* ctx,
                                             SkYUVColorSpace colorSpace,
                                             const GrBackendTexture yuvaTextures[],
                                             const SkYUVAIndex yuvaIndices[4],
                                             SkISize imageSize,
                                             GrSurfaceOrigin textureOrigin,
                                             sk_sp<SkColorSpace> imageColorSpace,
                                             TextureReleaseProc textureReleaseProc,
                                             ReleaseContext releaseContext) {
    auto releaseHelper = GrRefCntedCallback::Make(textureReleaseProc, releaseContext);

    int numTextures;
    if (!SkYUVAIndex::AreValidIndices(yuvaIndices, &numTextures)) {
        return nullptr;
    }

    GrSurfaceProxyView tempViews[4];
    if (!SkImage_GpuBase::MakeTempTextureProxies(ctx, yuvaTextures, numTextures, yuvaIndices,
                                                 textureOrigin, tempViews,
                                                 std::move(releaseHelper))) {
        return nullptr;
    }

    return sk_make_sp<SkImage_GpuYUVA>(sk_ref_sp(ctx), imageSize, kNeedNewImageUniqueID, colorSpace,
                                       tempViews, numTextures, yuvaIndices, imageColorSpace);
}

sk_sp<SkImage> SkImage::MakeFromYUVAPixmaps(GrRecordingContext* context,
                                            const SkYUVAPixmaps& pixmaps,
                                            GrMipMapped buildMips,
                                            bool limitToMaxTextureSize,
                                            sk_sp<SkColorSpace> imageColorSpace) {
    if (!context) {
        return nullptr;  // until we impl this for raster backend
    }

    if (!pixmaps.isValid()) {
        return nullptr;
    }

    SkYUVAIndex yuvaIndices[4];
    if (!pixmaps.toLegacy(nullptr, yuvaIndices)) {
        return nullptr;
    }

    // SkImage_GpuYUVA doesn't yet support different encoded origins.
    if (pixmaps.yuvaInfo().origin() != kTopLeft_SkEncodedOrigin) {
        return nullptr;
    }

    if (!context->priv().caps()->mipmapSupport()) {
        buildMips = GrMipMapped::kNo;
    }

    // Make proxies
    GrSurfaceProxyView tempViews[4];
    int numPlanes = pixmaps.numPlanes();
    int maxTextureSize = context->priv().caps()->maxTextureSize();
    for (int i = 0; i < numPlanes; ++i) {
        const SkPixmap* pixmap = &pixmaps.plane(i);
        SkAutoPixmapStorage resized;
        int maxDim = std::max(pixmap->width(), pixmap->height());
        if (maxDim > maxTextureSize) {
            if (!limitToMaxTextureSize) {
                return nullptr;
            }
            float scale = static_cast<float>(maxTextureSize)/maxDim;
            int newWidth  = std::min(static_cast<int>(pixmap->width() *scale), maxTextureSize);
            int newHeight = std::min(static_cast<int>(pixmap->height()*scale), maxTextureSize);
            SkImageInfo info = pixmap->info().makeWH(newWidth, newHeight);
            if (!resized.tryAlloc(info) || !pixmap->scalePixels(resized, kLow_SkFilterQuality)) {
                return nullptr;
            }
            pixmap = &resized;
        }
        // Turn the pixmap into a GrTextureProxy
        SkBitmap bmp;
        bmp.installPixels(*pixmap);
        GrBitmapTextureMaker bitmapMaker(context, bmp, GrImageTexGenPolicy::kNew_Uncached_Budgeted);
        tempViews[i] = bitmapMaker.view(buildMips);
        if (!tempViews[i]) {
            return nullptr;
        }
    }

    return sk_make_sp<SkImage_GpuYUVA>(sk_ref_sp(context),
                                       pixmaps.yuvaInfo().dimensions(),
                                       kNeedNewImageUniqueID,
                                       pixmaps.yuvaInfo().yuvColorSpace(),
                                       tempViews,
                                       numPlanes,
                                       yuvaIndices,
                                       std::move(imageColorSpace));
}

/////////////////////////////////////////////////////////////////////////////////////////////////

sk_sp<SkImage> SkImage_GpuYUVA::MakePromiseYUVATexture(
        GrRecordingContext* context,
        SkYUVColorSpace yuvColorSpace,
        const GrBackendFormat yuvaFormats[],
        const SkISize yuvaSizes[],
        const SkYUVAIndex yuvaIndices[4],
        int imageWidth,
        int imageHeight,
        GrSurfaceOrigin textureOrigin,
        sk_sp<SkColorSpace> imageColorSpace,
        PromiseImageTextureFulfillProc textureFulfillProc,
        PromiseImageTextureReleaseProc textureReleaseProc,
        PromiseImageTextureContext textureContexts[]) {
    int numTextures;
    bool valid = SkYUVAIndex::AreValidIndices(yuvaIndices, &numTextures);

    // Our contract is that we will always call the release proc even on failure.
    // We use the helper to convey the context, so we need to ensure make doesn't fail.
    textureReleaseProc = textureReleaseProc ? textureReleaseProc : [](void*) {};
    sk_sp<GrRefCntedCallback> releaseHelpers[4];
    for (int i = 0; i < numTextures; ++i) {
        releaseHelpers[i] = GrRefCntedCallback::Make(textureReleaseProc, textureContexts[i]);
    }

    if (!context) {
        return nullptr;
    }

    if (!valid) {
        return nullptr;
    }

    if (imageWidth <= 0 || imageHeight <= 0) {
        return nullptr;
    }

    SkAlphaType at = (-1 != yuvaIndices[SkYUVAIndex::kA_Index].fIndex) ? kPremul_SkAlphaType
                                                                       : kOpaque_SkAlphaType;
    SkImageInfo info =
            SkImageInfo::Make(imageWidth, imageHeight, kAssumedColorType, at, imageColorSpace);
    if (!SkImageInfoIsValid(info)) {
        return nullptr;
    }

    // verify sizes with expected texture count
    for (int i = 0; i < numTextures; ++i) {
        if (yuvaSizes[i].isEmpty()) {
            return nullptr;
        }
    }
    for (int i = numTextures; i < SkYUVASizeInfo::kMaxCount; ++i) {
        if (!yuvaSizes[i].isEmpty()) {
            return nullptr;
        }
    }

    // Get lazy proxies
    GrSurfaceProxyView views[4];
    for (int texIdx = 0; texIdx < numTextures; ++texIdx) {
        auto proxy = MakePromiseImageLazyProxy(context,
                                               yuvaSizes[texIdx],
                                               yuvaFormats[texIdx],
                                               GrMipmapped::kNo,
                                               textureFulfillProc,
                                               std::move(releaseHelpers[texIdx]));
        if (!proxy) {
            return nullptr;
        }
        views[texIdx] = GrSurfaceProxyView(std::move(proxy), textureOrigin, GrSwizzle("rgba"));
    }

    return sk_make_sp<SkImage_GpuYUVA>(sk_ref_sp(context), SkISize{imageWidth, imageHeight},
                                       kNeedNewImageUniqueID, yuvColorSpace, views, numTextures,
                                       yuvaIndices, std::move(imageColorSpace));
}
