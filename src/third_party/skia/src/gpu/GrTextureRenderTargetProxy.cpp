/*
 * Copyright 2016 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "src/gpu/GrTextureRenderTargetProxy.h"

#include "src/gpu/GrCaps.h"
#include "src/gpu/GrContextPriv.h"
#include "src/gpu/GrRenderTarget.h"
#include "src/gpu/GrRenderTargetProxyPriv.h"
#include "src/gpu/GrSurfacePriv.h"
#include "src/gpu/GrSurfaceProxyPriv.h"
#include "src/gpu/GrTexture.h"
#include "src/gpu/GrTexturePriv.h"
#include "src/gpu/GrTextureProxyPriv.h"

// Deferred version
// This class is virtually derived from GrSurfaceProxy (via both GrTextureProxy and
// GrRenderTargetProxy) so its constructor must be explicitly called.
GrTextureRenderTargetProxy::GrTextureRenderTargetProxy(const GrCaps& caps,
                                                       const GrBackendFormat& format,
                                                       SkISize dimensions,
                                                       int sampleCnt,
                                                       GrMipMapped mipMapped,
                                                       GrMipMapsStatus mipMapsStatus,
                                                       SkBackingFit fit,
                                                       SkBudgeted budgeted,
                                                       GrProtected isProtected,
                                                       GrInternalSurfaceFlags surfaceFlags,
                                                       UseAllocator useAllocator)
        : GrSurfaceProxy(format, dimensions, fit, budgeted, isProtected, surfaceFlags, useAllocator)
        // for now textures w/ data are always wrapped
        , GrRenderTargetProxy(caps, format, dimensions, sampleCnt, fit, budgeted, isProtected,
                              surfaceFlags, useAllocator)
        , GrTextureProxy(format, dimensions, mipMapped, mipMapsStatus, fit, budgeted, isProtected,
                         surfaceFlags, useAllocator) {
    this->initSurfaceFlags(caps);
}

// Lazy-callback version
GrTextureRenderTargetProxy::GrTextureRenderTargetProxy(const GrCaps& caps,
                                                       LazyInstantiateCallback&& callback,
                                                       const GrBackendFormat& format,
                                                       SkISize dimensions,
                                                       int sampleCnt,
                                                       GrMipMapped mipMapped,
                                                       GrMipMapsStatus mipMapsStatus,
                                                       SkBackingFit fit,
                                                       SkBudgeted budgeted,
                                                       GrProtected isProtected,
                                                       GrInternalSurfaceFlags surfaceFlags,
                                                       UseAllocator useAllocator)
        : GrSurfaceProxy(std::move(callback), format, dimensions, fit, budgeted, isProtected,
                         surfaceFlags, useAllocator)
        // Since we have virtual inheritance, we initialize GrSurfaceProxy directly. Send null
        // callbacks to the texture and RT proxies simply to route to the appropriate constructors.
        , GrRenderTargetProxy(LazyInstantiateCallback(), format, dimensions, sampleCnt, fit,
                              budgeted, isProtected, surfaceFlags, useAllocator,
                              WrapsVkSecondaryCB::kNo)
        , GrTextureProxy(LazyInstantiateCallback(), format, dimensions, mipMapped, mipMapsStatus,
                         fit, budgeted, isProtected, surfaceFlags, useAllocator) {
    this->initSurfaceFlags(caps);
}

// Wrapped version
// This class is virtually derived from GrSurfaceProxy (via both GrTextureProxy and
// GrRenderTargetProxy) so its constructor must be explicitly called.
GrTextureRenderTargetProxy::GrTextureRenderTargetProxy(sk_sp<GrSurface> surf,
                                                       UseAllocator useAllocator)
        : GrSurfaceProxy(surf, SkBackingFit::kExact, useAllocator)
        , GrRenderTargetProxy(surf, useAllocator)
        , GrTextureProxy(surf, useAllocator) {
    SkASSERT(surf->asTexture());
    SkASSERT(surf->asRenderTarget());
    SkASSERT(fSurfaceFlags == fTarget->surfacePriv().flags());
    SkASSERT((this->numSamples() <= 1 ||
              fTarget->getContext()->priv().caps()->msaaResolvesAutomatically()) !=
             this->requiresManualMSAAResolve());
}

void GrTextureRenderTargetProxy::initSurfaceFlags(const GrCaps& caps) {
    // FBO 0 should never be wrapped as a texture render target.
    SkASSERT(!this->rtPriv().glRTFBOIDIs0());
    if (this->numSamples() > 1 && !caps.msaaResolvesAutomatically())  {
        // MSAA texture-render-targets always require manual resolve if we are not using a
        // multisampled-render-to-texture extension.
        //
        // NOTE: This is the only instance where we need to set the manual resolve flag on a proxy.
        // Any other proxies that require manual resolve (e.g., wrapBackendTextureAsRenderTarget())
        // will be wrapped, and the wrapped version of the GrSurface constructor will automatically
        // get the manual resolve flag when copying the target GrSurface's flags.
        fSurfaceFlags |= GrInternalSurfaceFlags::kRequiresManualMSAAResolve;
    }
}

size_t GrTextureRenderTargetProxy::onUninstantiatedGpuMemorySize(const GrCaps& caps) const {
    int colorSamplesPerPixel = this->numSamples();
    if (colorSamplesPerPixel > 1) {
        // Add one to account for the resolve buffer.
        ++colorSamplesPerPixel;
    }

    // TODO: do we have enough information to improve this worst case estimate?
    return GrSurface::ComputeSize(caps, this->backendFormat(), this->dimensions(),
                                  colorSamplesPerPixel, this->proxyMipMapped(),
                                  !this->priv().isExact());
}

bool GrTextureRenderTargetProxy::instantiate(GrResourceProvider* resourceProvider) {
    if (this->isLazy()) {
        return false;
    }

    const GrUniqueKey& key = this->getUniqueKey();

    if (!this->instantiateImpl(resourceProvider, this->numSamples(), GrRenderable::kYes,
                               this->mipMapped(), key.isValid() ? &key : nullptr)) {
        return false;
    }
    if (key.isValid()) {
        SkASSERT(key == this->getUniqueKey());
    }

    SkASSERT(this->peekRenderTarget());
    SkASSERT(this->peekTexture());

    return true;
}

sk_sp<GrSurface> GrTextureRenderTargetProxy::createSurface(
                                                    GrResourceProvider* resourceProvider) const {
    sk_sp<GrSurface> surface = this->createSurfaceImpl(resourceProvider, this->numSamples(),
                                                       GrRenderable::kYes, this->mipMapped());
    if (!surface) {
        return nullptr;
    }
    SkASSERT(surface->asRenderTarget());
    SkASSERT(surface->asTexture());

    return surface;
}

GrSurfaceProxy::LazySurfaceDesc GrTextureRenderTargetProxy::callbackDesc() const {
    SkISize dims;
    SkBackingFit fit;
    if (this->isFullyLazy()) {
        fit = SkBackingFit::kApprox;
        dims = {-1, -1};
    } else {
        fit = this->isFunctionallyExact() ? SkBackingFit::kExact : SkBackingFit::kApprox;
        dims = this->dimensions();
    }
    return {
            dims,
            fit,
            GrRenderable::kYes,
            this->mipMapped(),
            this->numSamples(),
            this->backendFormat(),
            this->isProtected(),
            this->isBudgeted(),
    };
}

#ifdef SK_DEBUG
void GrTextureRenderTargetProxy::onValidateSurface(const GrSurface* surface) {
    // Anything checked here should also be checking the GrTextureProxy version
    SkASSERT(surface->asTexture());
    SkASSERT(GrMipMapped::kNo == this->proxyMipMapped() ||
             GrMipMapped::kYes == surface->asTexture()->texturePriv().mipMapped());

    // Anything checked here should also be checking the GrRenderTargetProxy version
    SkASSERT(surface->asRenderTarget());
    SkASSERT(surface->asRenderTarget()->numSamples() == this->numSamples());

    SkASSERT(surface->asTexture()->texturePriv().textureType() == this->textureType());

    GrInternalSurfaceFlags proxyFlags = fSurfaceFlags;
    GrInternalSurfaceFlags surfaceFlags = surface->surfacePriv().flags();

    // Only non-RT textures can be read only.
    SkASSERT(!(proxyFlags & GrInternalSurfaceFlags::kReadOnly));
    SkASSERT(!(surfaceFlags & GrInternalSurfaceFlags::kReadOnly));

    SkASSERT(((int)proxyFlags & kGrInternalTextureRenderTargetFlagsMask) ==
             ((int)surfaceFlags & kGrInternalTextureRenderTargetFlagsMask));
}
#endif

