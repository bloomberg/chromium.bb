/*
 * Copyright 2016 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// This is a GPU-backend specific test.

#include "Test.h"

#include "GrBackendSurface.h"
#include "GrContextPriv.h"
#include "GrProxyProvider.h"
#include "GrRenderTargetPriv.h"
#include "GrRenderTargetProxy.h"
#include "GrResourceProvider.h"
#include "GrSurfacePriv.h"
#include "GrSurfaceProxyPriv.h"
#include "GrTexture.h"
#include "GrTextureProxy.h"
#include "SkGr.h"

// Check that the surface proxy's member vars are set as expected
static void check_surface(skiatest::Reporter* reporter,
                          GrSurfaceProxy* proxy,
                          GrSurfaceOrigin origin,
                          int width, int height,
                          GrPixelConfig config,
                          SkBudgeted budgeted) {
    REPORTER_ASSERT(reporter, proxy->origin() == origin);
    REPORTER_ASSERT(reporter, proxy->width() == width);
    REPORTER_ASSERT(reporter, proxy->height() == height);
    REPORTER_ASSERT(reporter, proxy->config() == config);
    REPORTER_ASSERT(reporter, !proxy->uniqueID().isInvalid());
    REPORTER_ASSERT(reporter, proxy->isBudgeted() == budgeted);
}

static void check_rendertarget(skiatest::Reporter* reporter,
                               const GrCaps& caps,
                               GrResourceProvider* provider,
                               GrRenderTargetProxy* rtProxy,
                               int numSamples,
                               SkBackingFit fit,
                               int expectedMaxWindowRects) {
    REPORTER_ASSERT(reporter, rtProxy->maxWindowRectangles(caps) == expectedMaxWindowRects);
    REPORTER_ASSERT(reporter, rtProxy->numStencilSamples() == numSamples);

    GrSurfaceProxy::UniqueID idBefore = rtProxy->uniqueID();
    bool preinstantiated = rtProxy->isInstantiated();
    REPORTER_ASSERT(reporter, rtProxy->instantiate(provider));
    GrRenderTarget* rt = rtProxy->peekRenderTarget();

    REPORTER_ASSERT(reporter, rtProxy->uniqueID() == idBefore);
    // Deferred resources should always have a different ID from their instantiated rendertarget
    if (preinstantiated) {
        REPORTER_ASSERT(reporter, rtProxy->uniqueID().asUInt() == rt->uniqueID().asUInt());
    } else {
        REPORTER_ASSERT(reporter, rtProxy->uniqueID().asUInt() != rt->uniqueID().asUInt());
    }

    if (SkBackingFit::kExact == fit) {
        REPORTER_ASSERT(reporter, rt->width() == rtProxy->width());
        REPORTER_ASSERT(reporter, rt->height() == rtProxy->height());
    } else {
        REPORTER_ASSERT(reporter, rt->width() >= rtProxy->width());
        REPORTER_ASSERT(reporter, rt->height() >= rtProxy->height());
    }
    REPORTER_ASSERT(reporter, rt->config() == rtProxy->config());

    REPORTER_ASSERT(reporter, rt->fsaaType() == rtProxy->fsaaType());
    REPORTER_ASSERT(reporter, rt->numColorSamples() == rtProxy->numColorSamples());
    REPORTER_ASSERT(reporter, rt->numStencilSamples() == rtProxy->numStencilSamples());
    REPORTER_ASSERT(reporter, rt->surfacePriv().flags() == rtProxy->testingOnly_getFlags());
}

static void check_texture(skiatest::Reporter* reporter,
                          GrResourceProvider* provider,
                          GrTextureProxy* texProxy,
                          SkBackingFit fit) {
    GrSurfaceProxy::UniqueID idBefore = texProxy->uniqueID();

    bool preinstantiated = texProxy->isInstantiated();
    REPORTER_ASSERT(reporter, texProxy->instantiate(provider));
    GrTexture* tex = texProxy->peekTexture();

    REPORTER_ASSERT(reporter, texProxy->uniqueID() == idBefore);
    // Deferred resources should always have a different ID from their instantiated texture
    if (preinstantiated) {
        REPORTER_ASSERT(reporter, texProxy->uniqueID().asUInt() == tex->uniqueID().asUInt());
    } else {
        REPORTER_ASSERT(reporter, texProxy->uniqueID().asUInt() != tex->uniqueID().asUInt());
    }

    if (SkBackingFit::kExact == fit) {
        REPORTER_ASSERT(reporter, tex->width() == texProxy->width());
        REPORTER_ASSERT(reporter, tex->height() == texProxy->height());
    } else {
        REPORTER_ASSERT(reporter, tex->width() >= texProxy->width());
        REPORTER_ASSERT(reporter, tex->height() >= texProxy->height());
    }
    REPORTER_ASSERT(reporter, tex->config() == texProxy->config());
}


DEF_GPUTEST_FOR_RENDERING_CONTEXTS(DeferredProxyTest, reporter, ctxInfo) {
    GrProxyProvider* proxyProvider = ctxInfo.grContext()->contextPriv().proxyProvider();
    GrResourceProvider* resourceProvider = ctxInfo.grContext()->contextPriv().resourceProvider();
    const GrCaps& caps = *ctxInfo.grContext()->contextPriv().caps();

    int attempt = 0; // useful for debugging

    for (auto origin : { kBottomLeft_GrSurfaceOrigin, kTopLeft_GrSurfaceOrigin }) {
        for (auto widthHeight : { 100, 128, 1048576 }) {
            for (auto config : { kAlpha_8_GrPixelConfig, kRGB_565_GrPixelConfig,
                                 kRGBA_8888_GrPixelConfig, kRGBA_1010102_GrPixelConfig }) {
                for (auto fit : { SkBackingFit::kExact, SkBackingFit::kApprox }) {
                    for (auto budgeted : { SkBudgeted::kYes, SkBudgeted::kNo }) {
                        for (auto numSamples : {1, 4, 16, 128}) {
                            GrSurfaceDesc desc;
                            desc.fFlags = kRenderTarget_GrSurfaceFlag;
                            desc.fWidth = widthHeight;
                            desc.fHeight = widthHeight;
                            desc.fConfig = config;
                            desc.fSampleCnt = numSamples;

                            {
                                sk_sp<GrTexture> tex;
                                if (SkBackingFit::kApprox == fit) {
                                    tex = resourceProvider->createApproxTexture(
                                            desc, GrResourceProvider::Flags::kNone);
                                } else {
                                    tex = resourceProvider->createTexture(desc, budgeted);
                                }

                                sk_sp<GrTextureProxy> proxy =
                                        proxyProvider->createProxy(desc, origin, fit, budgeted);
                                REPORTER_ASSERT(reporter, SkToBool(tex) == SkToBool(proxy));
                                if (proxy) {
                                    REPORTER_ASSERT(reporter, proxy->asRenderTargetProxy());
                                    // This forces the proxy to compute and cache its
                                    // pre-instantiation size guess. Later, when it is actually
                                    // instantiated, it checks that the instantiated size is <= to
                                    // the pre-computation. If the proxy never computed its
                                    // pre-instantiation size then the check is skipped.
                                    proxy->gpuMemorySize();

                                    check_surface(reporter, proxy.get(), origin,
                                                  widthHeight, widthHeight, config, budgeted);
                                    int supportedSamples =
                                            caps.getRenderTargetSampleCount(numSamples, config);
                                    check_rendertarget(reporter, caps, resourceProvider,
                                                       proxy->asRenderTargetProxy(),
                                                       supportedSamples,
                                                       fit, caps.maxWindowRectangles());
                                }
                            }

                            desc.fFlags = kNone_GrSurfaceFlags;

                            {
                                sk_sp<GrTexture> tex;
                                if (SkBackingFit::kApprox == fit) {
                                    tex = resourceProvider->createApproxTexture(
                                            desc, GrResourceProvider::Flags::kNone);
                                } else {
                                    tex = resourceProvider->createTexture(desc, budgeted);
                                }

                                sk_sp<GrTextureProxy> proxy(
                                        proxyProvider->createProxy(desc, origin, fit, budgeted));
                                REPORTER_ASSERT(reporter, SkToBool(tex) == SkToBool(proxy));
                                if (proxy) {
                                    // This forces the proxy to compute and cache its
                                    // pre-instantiation size guess. Later, when it is actually
                                    // instantiated, it checks that the instantiated size is <= to
                                    // the pre-computation. If the proxy never computed its
                                    // pre-instantiation size then the check is skipped.
                                    proxy->gpuMemorySize();

                                    check_surface(reporter, proxy.get(), origin,
                                                  widthHeight, widthHeight, config, budgeted);
                                    check_texture(reporter, resourceProvider,
                                                  proxy->asTextureProxy(), fit);
                                }
                            }

                            attempt++;
                        }
                    }
                }
            }
        }
    }
}

DEF_GPUTEST_FOR_RENDERING_CONTEXTS(WrappedProxyTest, reporter, ctxInfo) {
    GrProxyProvider* proxyProvider = ctxInfo.grContext()->contextPriv().proxyProvider();
    GrResourceProvider* resourceProvider = ctxInfo.grContext()->contextPriv().resourceProvider();
    GrGpu* gpu = ctxInfo.grContext()->contextPriv().getGpu();
    const GrCaps& caps = *ctxInfo.grContext()->contextPriv().caps();

    static const int kWidthHeight = 100;

    for (auto origin : { kBottomLeft_GrSurfaceOrigin, kTopLeft_GrSurfaceOrigin }) {
        for (auto colorType : { kAlpha_8_SkColorType, kRGBA_8888_SkColorType,
                                kRGBA_1010102_SkColorType }) {
            // External on-screen render target.
            // Tests wrapBackendRenderTarget with a GrBackendRenderTarget
            // Our test-only function that creates a backend render target doesn't currently support
            // sample counts :(.
            if (ctxInfo.grContext()->colorTypeSupportedAsSurface(colorType)) {
                GrBackendRenderTarget backendRT = gpu->createTestingOnlyBackendRenderTarget(
                        kWidthHeight, kWidthHeight, SkColorTypeToGrColorType(colorType));
                sk_sp<GrSurfaceProxy> sProxy(
                        proxyProvider->wrapBackendRenderTarget(backendRT, origin));
                check_surface(reporter, sProxy.get(), origin, kWidthHeight, kWidthHeight,
                              backendRT.pixelConfig(), SkBudgeted::kNo);
                static constexpr int kExpectedNumSamples = 1;
                check_rendertarget(reporter, caps, resourceProvider, sProxy->asRenderTargetProxy(),
                                   kExpectedNumSamples, SkBackingFit::kExact,
                                   caps.maxWindowRectangles());
                gpu->deleteTestingOnlyBackendRenderTarget(backendRT);
            }

            for (auto numSamples : {1, 4}) {
                GrPixelConfig config = SkColorType2GrPixelConfig(colorType);
                SkASSERT(kUnknown_GrPixelConfig != config);
                int supportedNumSamples = caps.getRenderTargetSampleCount(numSamples, config);

                if (!supportedNumSamples) {
                    continue;
                }

                // Test wrapping FBO 0 (with made up properties). This tests sample count and the
                // special case where FBO 0 doesn't support window rectangles.
                if (kOpenGL_GrBackend == ctxInfo.backend()) {
                    GrGLFramebufferInfo fboInfo;
                    fboInfo.fFBOID = 0;
                    static constexpr int kStencilBits = 8;
                    GrBackendRenderTarget backendRT(kWidthHeight, kWidthHeight, numSamples,
                                                    kStencilBits, fboInfo);
                    backendRT.setPixelConfig(config);
                    sk_sp<GrSurfaceProxy> sProxy(
                            proxyProvider->wrapBackendRenderTarget(backendRT, origin));
                    check_surface(reporter, sProxy.get(), origin,
                                  kWidthHeight, kWidthHeight,
                                  backendRT.pixelConfig(), SkBudgeted::kNo);
                    check_rendertarget(reporter, caps, resourceProvider,
                                       sProxy->asRenderTargetProxy(),
                                       supportedNumSamples, SkBackingFit::kExact, 0);
                }

                // Tests wrapBackendRenderTarget with a GrBackendTexture
                {
                    GrBackendTexture backendTex =
                            gpu->createTestingOnlyBackendTexture(nullptr, kWidthHeight,
                                                                 kWidthHeight, colorType,
                                                                 true, GrMipMapped::kNo);
                    sk_sp<GrSurfaceProxy> sProxy = proxyProvider->wrapBackendTextureAsRenderTarget(
                            backendTex, origin, supportedNumSamples);
                    if (!sProxy) {
                        gpu->deleteTestingOnlyBackendTexture(backendTex);
                        continue;  // This can fail on Mesa
                    }

                    check_surface(reporter, sProxy.get(), origin,
                                  kWidthHeight, kWidthHeight,
                                  backendTex.pixelConfig(), SkBudgeted::kNo);
                    check_rendertarget(reporter, caps, resourceProvider,
                                       sProxy->asRenderTargetProxy(),
                                       supportedNumSamples, SkBackingFit::kExact,
                                       caps.maxWindowRectangles());

                    gpu->deleteTestingOnlyBackendTexture(backendTex);
                }

                // Tests wrapBackendTexture that is only renderable
                {
                    GrBackendTexture backendTex =
                            gpu->createTestingOnlyBackendTexture(nullptr, kWidthHeight,
                                                                 kWidthHeight, colorType,
                                                                 true, GrMipMapped::kNo);

                    sk_sp<GrSurfaceProxy> sProxy = proxyProvider->wrapRenderableBackendTexture(
                            backendTex, origin, supportedNumSamples);
                    if (!sProxy) {
                        gpu->deleteTestingOnlyBackendTexture(backendTex);
                        continue;  // This can fail on Mesa
                    }

                    check_surface(reporter, sProxy.get(), origin,
                                  kWidthHeight, kWidthHeight,
                                  backendTex.pixelConfig(), SkBudgeted::kNo);
                    check_rendertarget(reporter, caps, resourceProvider,
                                       sProxy->asRenderTargetProxy(),
                                       supportedNumSamples, SkBackingFit::kExact,
                                       caps.maxWindowRectangles());

                    gpu->deleteTestingOnlyBackendTexture(backendTex);
                }

                // Tests wrapBackendTexture that is only textureable
                {
                    // Internal offscreen texture
                    GrBackendTexture backendTex =
                            gpu->createTestingOnlyBackendTexture(nullptr, kWidthHeight,
                                                                 kWidthHeight, colorType,
                                                                 false, GrMipMapped::kNo);

                    sk_sp<GrSurfaceProxy> sProxy = proxyProvider->wrapBackendTexture(
                            backendTex, origin, kBorrow_GrWrapOwnership, nullptr, nullptr);
                    if (!sProxy) {
                        gpu->deleteTestingOnlyBackendTexture(backendTex);
                        continue;
                    }

                    check_surface(reporter, sProxy.get(), origin,
                                  kWidthHeight, kWidthHeight,
                                  backendTex.pixelConfig(), SkBudgeted::kNo);
                    check_texture(reporter, resourceProvider, sProxy->asTextureProxy(),
                                  SkBackingFit::kExact);

                    gpu->deleteTestingOnlyBackendTexture(backendTex);
                }
            }
        }
    }
}

DEF_GPUTEST_FOR_RENDERING_CONTEXTS(ZeroSizedProxyTest, reporter, ctxInfo) {
    GrProxyProvider* provider = ctxInfo.grContext()->contextPriv().proxyProvider();

    for (auto flags : { kRenderTarget_GrSurfaceFlag, kNone_GrSurfaceFlags }) {
        for (auto fit : { SkBackingFit::kExact, SkBackingFit::kApprox }) {
            for (int width : { 0, 100 }) {
                for (int height : { 0, 100}) {
                    if (width && height) {
                        continue; // not zero-sized
                    }

                    GrSurfaceDesc desc;
                    desc.fFlags = flags;
                    desc.fWidth = width;
                    desc.fHeight = height;
                    desc.fConfig = kRGBA_8888_GrPixelConfig;
                    desc.fSampleCnt = 1;

                    sk_sp<GrTextureProxy> proxy = provider->createProxy(
                            desc, kBottomLeft_GrSurfaceOrigin, fit, SkBudgeted::kNo);
                    REPORTER_ASSERT(reporter, !proxy);
                }
            }
        }
    }
}
