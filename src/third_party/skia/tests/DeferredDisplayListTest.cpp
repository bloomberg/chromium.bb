/*
 * Copyright 2017 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <initializer_list>
#include <memory>
#include <utility>
#include "GrBackendSurface.h"
#include "GrCaps.h"
#include "GrContext.h"
#include "GrContextFactory.h"
#include "GrContextPriv.h"
#include "GrGpu.h"
#include "GrRenderTargetContext.h"
#include "GrRenderTargetProxy.h"
#include "GrTextureProxy.h"
#include "GrTextureProxyPriv.h"
#include "GrTypes.h"
#include "GrTypesPriv.h"
#include "SkBitmap.h"
#include "SkCanvas.h"
#include "SkColorSpace.h"
#include "SkDeferredDisplayList.h"
#include "SkDeferredDisplayListPriv.h"
#include "SkDeferredDisplayListRecorder.h"
#include "SkGpuDevice.h"
#include "SkImage.h"
#include "SkImageInfo.h"
#include "SkImage_Gpu.h"
#include "SkPaint.h"
#include "SkPromiseImageTexture.h"
#include "SkRect.h"
#include "SkRefCnt.h"
#include "SkSurface.h"
#include "SkSurfaceCharacterization.h"
#include "SkSurfaceProps.h"
#include "SkSurface_Gpu.h"
#include "Test.h"
#include "gl/GrGLCaps.h"
#include "gl/GrGLDefines.h"
#include "gl/GrGLTypes.h"
#ifdef SK_VULKAN
#include <vulkan/vulkan_core.h>
#endif

class SurfaceParameters {
public:
    static const int kNumParams   = 11;
    static const int kSampleCount = 5;
    static const int kMipMipCount = 8;
    static const int kFBO0Count   = 9;

    SurfaceParameters(GrBackendApi backend)
            : fBackend(backend)
            , fWidth(64)
            , fHeight(64)
            , fOrigin(kTopLeft_GrSurfaceOrigin)
            , fColorType(kRGBA_8888_SkColorType)
            , fColorSpace(SkColorSpace::MakeSRGB())
            , fSampleCount(1)
            , fSurfaceProps(0x0, kUnknown_SkPixelGeometry)
            , fShouldCreateMipMaps(true)
            , fUsesGLFBO0(false)
            , fIsTextureable(true) {
    }

    int sampleCount() const { return fSampleCount; }

    void setColorType(SkColorType ct) { fColorType = ct; }
    void setColorSpace(sk_sp<SkColorSpace> cs) { fColorSpace = std::move(cs); }
    void setTextureable(bool isTextureable) { fIsTextureable = isTextureable; }

    // Modify the SurfaceParameters in just one way
    void modify(int i) {
        switch (i) {
        case 0:
            fWidth = 63;
            break;
        case 1:
            fHeight = 63;
            break;
        case 2:
            fOrigin = kBottomLeft_GrSurfaceOrigin;
            break;
        case 3:
            fColorType = kRGBA_F16_SkColorType;
            break;
        case 4:
            // This just needs to be a colorSpace different from that returned by MakeSRGB().
            // In this case we just change the gamut.
            fColorSpace = SkColorSpace::MakeRGB(SkNamedTransferFn::kSRGB, SkNamedGamut::kAdobeRGB);
            break;
        case kSampleCount:
            fSampleCount = 4;
            break;
        case 6:
            fSurfaceProps = SkSurfaceProps(0x0, kRGB_H_SkPixelGeometry);
            break;
        case 7:
            fSurfaceProps = SkSurfaceProps(SkSurfaceProps::kUseDeviceIndependentFonts_Flag,
                                           kUnknown_SkPixelGeometry);
            break;
        case 8:
            fShouldCreateMipMaps = false;
            break;
        case 9:
            if (GrBackendApi::kOpenGL == fBackend) {
                fUsesGLFBO0 = true;
                fIsTextureable = false;
            }
            break;
        case 10:
            fIsTextureable = false;
            break;
        }
    }

    SkSurfaceCharacterization createCharacterization(GrContext* context) const {
        int maxResourceCount;
        size_t maxResourceBytes;
        context->getResourceCacheLimits(&maxResourceCount, &maxResourceBytes);

        // Note that Ganesh doesn't make use of the SkImageInfo's alphaType
        SkImageInfo ii = SkImageInfo::Make(fWidth, fHeight, fColorType,
                                           kPremul_SkAlphaType, fColorSpace);

        const GrCaps* caps = context->priv().caps();
        GrBackendFormat backendFormat = caps->getBackendFormatFromColorType(fColorType);
        if (!backendFormat.isValid()) {
            return SkSurfaceCharacterization();
        }

        SkSurfaceCharacterization c = context->threadSafeProxy()->createCharacterization(
                                                maxResourceBytes, ii, backendFormat, fSampleCount,
                                                fOrigin, fSurfaceProps, fShouldCreateMipMaps,
                                                fUsesGLFBO0, fIsTextureable);
        return c;
    }

    // Create a DDL whose characterization captures the current settings
    std::unique_ptr<SkDeferredDisplayList> createDDL(GrContext* context) const {
        SkSurfaceCharacterization c = this->createCharacterization(context);
        SkAssertResult(c.isValid());

        SkDeferredDisplayListRecorder r(c);
        SkCanvas* canvas = r.getCanvas();
        if (!canvas) {
            return nullptr;
        }

        canvas->drawRect(SkRect::MakeXYWH(10, 10, 10, 10), SkPaint());
        return r.detach();
    }

    // Create the surface with the current set of parameters
    sk_sp<SkSurface> make(GrContext* context, GrBackendTexture* backend) const {
        GrGpu* gpu = context->priv().getGpu();

        GrMipMapped mipmapped = !fIsTextureable
                                        ? GrMipMapped::kNo
                                        : GrMipMapped(fShouldCreateMipMaps);

        if (fUsesGLFBO0) {
            if (GrBackendApi::kOpenGL != context->backend()) {
                return nullptr;
            }

            GrGLFramebufferInfo fboInfo;
            fboInfo.fFBOID = 0;
            fboInfo.fFormat = GR_GL_RGBA8;
            static constexpr int kStencilBits = 8;
            GrBackendRenderTarget backendRT(fWidth, fHeight, 1, kStencilBits, fboInfo);

            if (!backendRT.isValid()) {
                return nullptr;
            }

            return SkSurface::MakeFromBackendRenderTarget(context, backendRT, fOrigin,
                                                          fColorType, fColorSpace, &fSurfaceProps);
        }

        *backend = gpu->createTestingOnlyBackendTexture(nullptr, fWidth, fHeight,
                                                        fColorType, true, mipmapped);
        if (!backend->isValid() || !gpu->isTestingOnlyBackendTexture(*backend)) {
            return nullptr;
        }

        sk_sp<SkSurface> surface;
        if (!fIsTextureable) {
            // Create a surface w/ the current parameters but make it non-textureable
            surface = SkSurface::MakeFromBackendTextureAsRenderTarget(
                                            context, *backend, fOrigin, fSampleCount, fColorType,
                                            fColorSpace, &fSurfaceProps);
        } else {
            surface = SkSurface::MakeFromBackendTexture(
                                            context, *backend, fOrigin, fSampleCount, fColorType,
                                            fColorSpace, &fSurfaceProps);
        }

        if (!surface) {
            gpu->deleteTestingOnlyBackendTexture(*backend);
            return nullptr;
        }

        return surface;
    }

    void cleanUpBackEnd(GrContext* context, const GrBackendTexture& backend) const {
        if (!backend.isValid()) {
            return;
        }

        GrGpu* gpu = context->priv().getGpu();

        gpu->deleteTestingOnlyBackendTexture(backend);
    }

private:
    GrBackendApi        fBackend;
    int                 fWidth;
    int                 fHeight;
    GrSurfaceOrigin     fOrigin;
    SkColorType         fColorType;
    sk_sp<SkColorSpace> fColorSpace;
    int                 fSampleCount;
    SkSurfaceProps      fSurfaceProps;
    bool                fShouldCreateMipMaps;
    bool                fUsesGLFBO0;
    bool                fIsTextureable;
};

// Test out operator== && operator!=
DEF_GPUTEST_FOR_RENDERING_CONTEXTS(DDLOperatorEqTest, reporter, ctxInfo) {
    GrContext* context = ctxInfo.grContext();

    for (int i = 0; i < SurfaceParameters::kNumParams; ++i) {
        SurfaceParameters params1(context->backend());
        params1.modify(i);

        SkSurfaceCharacterization char1 = params1.createCharacterization(context);
        if (!char1.isValid()) {
            continue;  // can happen on some platforms (ChromeOS)
        }

        for (int j = 0; j < SurfaceParameters::kNumParams; ++j) {
            SurfaceParameters params2(context->backend());
            params2.modify(j);

            SkSurfaceCharacterization char2 = params2.createCharacterization(context);
            if (!char2.isValid()) {
                continue;  // can happen on some platforms (ChromeOS)
            }

            if (i == j) {
                REPORTER_ASSERT(reporter, char1 == char2);
            } else {
                REPORTER_ASSERT(reporter, char1 != char2);
            }

        }
    }

    {
        SurfaceParameters params(context->backend());

        SkSurfaceCharacterization valid = params.createCharacterization(context);
        SkASSERT(valid.isValid());

        SkSurfaceCharacterization inval1, inval2;
        SkASSERT(!inval1.isValid() && !inval2.isValid());

        REPORTER_ASSERT(reporter, inval1 != inval2);
        REPORTER_ASSERT(reporter, valid != inval1);
        REPORTER_ASSERT(reporter, inval1 != valid);
    }
}

////////////////////////////////////////////////////////////////////////////////
// This tests SkSurfaceCharacterization/SkSurface compatibility
DEF_GPUTEST_FOR_RENDERING_CONTEXTS(DDLSurfaceCharacterizationTest, reporter, ctxInfo) {
    GrContext* context = ctxInfo.grContext();
    GrGpu* gpu = context->priv().getGpu();

    // Create a bitmap that we can readback into
    SkImageInfo imageInfo = SkImageInfo::Make(64, 64, kRGBA_8888_SkColorType,
                                              kPremul_SkAlphaType);
    SkBitmap bitmap;
    bitmap.allocPixels(imageInfo);

    std::unique_ptr<SkDeferredDisplayList> ddl;

    // First, create a DDL using the stock SkSurface parameters
    {
        SurfaceParameters params(context->backend());

        ddl = params.createDDL(context);
        SkAssertResult(ddl);

        // The DDL should draw into an SkSurface created with the same parameters
        GrBackendTexture backend;
        sk_sp<SkSurface> s = params.make(context, &backend);
        if (!s) {
            return;
        }

        REPORTER_ASSERT(reporter, s->draw(ddl.get()));
        s->readPixels(imageInfo, bitmap.getPixels(), bitmap.rowBytes(), 0, 0);
        context->flush();
        gpu->testingOnly_flushGpuAndSync();
        s = nullptr;
        params.cleanUpBackEnd(context, backend);
    }

    // Then, alter each parameter in turn and check that the DDL & surface are incompatible
    for (int i = 0; i < SurfaceParameters::kNumParams; ++i) {
        SurfaceParameters params(context->backend());
        params.modify(i);

        GrBackendTexture backend;
        sk_sp<SkSurface> s = params.make(context, &backend);
        if (!s) {
            continue;
        }

        if (SurfaceParameters::kSampleCount == i) {
            SkSurface_Gpu* gpuSurf = static_cast<SkSurface_Gpu*>(s.get());

            int supportedSampleCount = context->priv().caps()->getRenderTargetSampleCount(
                    params.sampleCount(),
                    gpuSurf->getDevice()
                            ->accessRenderTargetContext()
                            ->asRenderTargetProxy()
                            ->config());
            if (1 == supportedSampleCount) {
                // If changing the sample count won't result in a different
                // surface characterization, skip this step
                s = nullptr;
                params.cleanUpBackEnd(context, backend);
                continue;
            }
        }

        if (SurfaceParameters::kMipMipCount == i &&
            !context->priv().caps()->mipMapSupport()) {
            // If changing the mipmap setting won't result in a different surface characterization,
            // skip this step
            s = nullptr;
            params.cleanUpBackEnd(context, backend);
            continue;
        }

        if (SurfaceParameters::kFBO0Count == i && context->backend() != GrBackendApi::kOpenGL) {
            // FBO0 only affects the surface characterization when using OpenGL
            s = nullptr;
            params.cleanUpBackEnd(context, backend);
            continue;
        }

        REPORTER_ASSERT(reporter, !s->draw(ddl.get()),
                        "DDLSurfaceCharacterizationTest failed on parameter: %d\n", i);

        context->flush();
        gpu->testingOnly_flushGpuAndSync();
        s = nullptr;
        params.cleanUpBackEnd(context, backend);
    }

    // Next test the compatibility of resource cache parameters
    {
        const SurfaceParameters params(context->backend());
        GrBackendTexture backend;

        sk_sp<SkSurface> s = params.make(context, &backend);

        int maxResourceCount;
        size_t maxResourceBytes;
        context->getResourceCacheLimits(&maxResourceCount, &maxResourceBytes);

        context->setResourceCacheLimits(maxResourceCount, maxResourceBytes/2);
        REPORTER_ASSERT(reporter, !s->draw(ddl.get()));

        // DDL TODO: once proxies/ops can be de-instantiated we can re-enable these tests.
        // For now, DDLs are drawn once.
#if 0
        // resource limits >= those at characterization time are accepted
        context->setResourceCacheLimits(2*maxResourceCount, maxResourceBytes);
        REPORTER_ASSERT(reporter, s->draw(ddl.get()));
        s->readPixels(imageInfo, bitmap.getPixels(), bitmap.rowBytes(), 0, 0);

        context->setResourceCacheLimits(maxResourceCount, 2*maxResourceBytes);
        REPORTER_ASSERT(reporter, s->draw(ddl.get()));
        s->readPixels(imageInfo, bitmap.getPixels(), bitmap.rowBytes(), 0, 0);

        context->setResourceCacheLimits(maxResourceCount, maxResourceBytes);
        REPORTER_ASSERT(reporter, s->draw(ddl.get()));
        s->readPixels(imageInfo, bitmap.getPixels(), bitmap.rowBytes(), 0, 0);
#endif

        context->flush();
        gpu->testingOnly_flushGpuAndSync();
        s = nullptr;
        params.cleanUpBackEnd(context, backend);
    }

    // Test that the textureability of the DDL characterization can block a DDL draw
    {
        GrBackendTexture backend;
        SurfaceParameters params(context->backend());
        params.setTextureable(false);

        sk_sp<SkSurface> s = params.make(context, &backend);
        if (s) {
            REPORTER_ASSERT(reporter, !s->draw(ddl.get())); // bc the DDL was made w/ textureability

            context->flush();
            gpu->testingOnly_flushGpuAndSync();
            s = nullptr;
            params.cleanUpBackEnd(context, backend);
        }
    }

    // Make sure non-GPU-backed surfaces fail characterization
    {
        SkImageInfo ii = SkImageInfo::MakeN32(64, 64, kOpaque_SkAlphaType);

        sk_sp<SkSurface> rasterSurface = SkSurface::MakeRaster(ii);
        SkSurfaceCharacterization c;
        REPORTER_ASSERT(reporter, !rasterSurface->characterize(&c));
    }

    // Exercise the createResized method
    {
        SurfaceParameters params(context->backend());
        GrBackendTexture backend;

        sk_sp<SkSurface> s = params.make(context, &backend);
        if (!s) {
            return;
        }

        SkSurfaceCharacterization char0;
        SkAssertResult(s->characterize(&char0));

        // Too small
        SkSurfaceCharacterization char1 = char0.createResized(-1, -1);
        REPORTER_ASSERT(reporter, !char1.isValid());

        // Too large
        SkSurfaceCharacterization char2 = char0.createResized(1000000, 32);
        REPORTER_ASSERT(reporter, !char2.isValid());

        // Just right
        SkSurfaceCharacterization char3 = char0.createResized(32, 32);
        REPORTER_ASSERT(reporter, char3.isValid());
        REPORTER_ASSERT(reporter, 32 == char3.width());
        REPORTER_ASSERT(reporter, 32 == char3.height());

        s = nullptr;
        params.cleanUpBackEnd(context, backend);
    }
}

// Test that a DDL created w/o textureability can be replayed into both a textureable and
// non-textureable destination. Note that DDLSurfaceCharacterizationTest tests that a
// textureable DDL cannot be played into a non-textureable destination but can be replayed
// into a textureable destination.
DEF_GPUTEST_FOR_RENDERING_CONTEXTS(DDLNonTextureabilityTest, reporter, ctxInfo) {
    GrContext* context = ctxInfo.grContext();
    GrGpu* gpu = context->priv().getGpu();

    // Create a bitmap that we can readback into
    SkImageInfo imageInfo = SkImageInfo::Make(64, 64, kRGBA_8888_SkColorType,
                                              kPremul_SkAlphaType);
    SkBitmap bitmap;
    bitmap.allocPixels(imageInfo);

    for (bool textureability : { true, false }) {
        std::unique_ptr<SkDeferredDisplayList> ddl;

        // First, create a DDL w/o textureability. TODO: once we have reusable DDLs, move this
        // outside of the loop.
        {
            SurfaceParameters params(context->backend());
            params.setTextureable(false);

            ddl = params.createDDL(context);
            SkAssertResult(ddl);
        }

        // Then verify it can draw into either flavor of destination
        SurfaceParameters params(context->backend());
        params.setTextureable(textureability);

        GrBackendTexture backend;
        sk_sp<SkSurface> s = params.make(context, &backend);
        if (!s) {
            continue;
        }

        REPORTER_ASSERT(reporter, s->draw(ddl.get()));
        s->readPixels(imageInfo, bitmap.getPixels(), bitmap.rowBytes(), 0, 0);
        context->flush();
        gpu->testingOnly_flushGpuAndSync();
        s = nullptr;
        params.cleanUpBackEnd(context, backend);
    }

}

////////////////////////////////////////////////////////////////////////////////
// This tests the SkSurface::MakeRenderTarget variant that takes an SkSurfaceCharacterization.
// In particular, the SkSurface and the SkSurfaceCharacterization should always be compatible.
DEF_GPUTEST_FOR_RENDERING_CONTEXTS(DDLMakeRenderTargetTest, reporter, ctxInfo) {
    GrContext* context = ctxInfo.grContext();

    for (int i = 0; i < SurfaceParameters::kNumParams; ++i) {
        SurfaceParameters params(context->backend());
        params.modify(i);

        SkSurfaceCharacterization c = params.createCharacterization(context);
        GrBackendTexture backend;

        if (!c.isValid()) {
            sk_sp<SkSurface> tmp = params.make(context, &backend);

            // If we couldn't characterize the surface we shouldn't be able to create it either
            REPORTER_ASSERT(reporter, !tmp);
            if (tmp) {
                tmp = nullptr;
                params.cleanUpBackEnd(context, backend);
            }
            continue;
        }

        sk_sp<SkSurface> s = params.make(context, &backend);
        if (!s) {
            REPORTER_ASSERT(reporter, !c.isValid());
            continue;
        }

        REPORTER_ASSERT(reporter, c.isValid());

        if (SurfaceParameters::kFBO0Count == i) {
            // MakeRenderTarget doesn't support FBO0
            params.cleanUpBackEnd(context, backend);
            continue;
        }

        s = SkSurface::MakeRenderTarget(context, c, SkBudgeted::kYes);
        REPORTER_ASSERT(reporter, s);

        SkSurface_Gpu* g = static_cast<SkSurface_Gpu*>(s.get());
        REPORTER_ASSERT(reporter, g->isCompatible(c));

        s = nullptr;
        params.cleanUpBackEnd(context, backend);
    }
}

////////////////////////////////////////////////////////////////////////////////
static constexpr int kSize = 8;

struct TextureReleaseChecker {
    TextureReleaseChecker() : fReleaseCount(0) {}
    int fReleaseCount;
    static void Release(void* self) {
        static_cast<TextureReleaseChecker*>(self)->fReleaseCount++;
    }
};

enum class DDLStage { kMakeImage, kDrawImage, kDetach, kDrawDDL };

// This tests the ability to create and use wrapped textures in a DDL world
DEF_GPUTEST_FOR_RENDERING_CONTEXTS(DDLWrapBackendTest, reporter, ctxInfo) {
    GrContext* context = ctxInfo.grContext();
    GrGpu* gpu = context->priv().getGpu();
    GrBackendTexture backendTex = gpu->createTestingOnlyBackendTexture(
            nullptr, kSize, kSize, GrColorType::kRGBA_8888, false, GrMipMapped::kNo);
    if (!backendTex.isValid()) {
        return;
    }

    SurfaceParameters params(context->backend());
    GrBackendTexture backend;

    sk_sp<SkSurface> s = params.make(context, &backend);
    if (!s) {
        gpu->deleteTestingOnlyBackendTexture(backendTex);
        return;
    }

    SkSurfaceCharacterization c;
    SkAssertResult(s->characterize(&c));

    std::unique_ptr<SkDeferredDisplayListRecorder> recorder(new SkDeferredDisplayListRecorder(c));

    SkCanvas* canvas = recorder->getCanvas();
    if (!canvas) {
        s = nullptr;
        params.cleanUpBackEnd(context, backend);
        gpu->deleteTestingOnlyBackendTexture(backendTex);
        return;
    }

    GrContext* deferredContext = canvas->getGrContext();
    if (!deferredContext) {
        s = nullptr;
        params.cleanUpBackEnd(context, backend);
        gpu->deleteTestingOnlyBackendTexture(backendTex);
        return;
    }

    // Wrapped Backend Textures are not supported in DDL
    sk_sp<SkImage> image =
            SkImage::MakeFromAdoptedTexture(deferredContext, backendTex, kTopLeft_GrSurfaceOrigin,
                                            kRGBA_8888_SkColorType, kPremul_SkAlphaType, nullptr);
    REPORTER_ASSERT(reporter, !image);

    TextureReleaseChecker releaseChecker;
    image = SkImage::MakeFromTexture(deferredContext, backendTex, kTopLeft_GrSurfaceOrigin,
                                     kRGBA_8888_SkColorType, kPremul_SkAlphaType, nullptr,
                                     TextureReleaseChecker::Release, &releaseChecker);
    REPORTER_ASSERT(reporter, !image);

    gpu->deleteTestingOnlyBackendTexture(backendTex);

    s = nullptr;
    params.cleanUpBackEnd(context, backend);
}

static sk_sp<SkPromiseImageTexture> dummy_fulfill_proc(void*) {
    SkASSERT(0);
    return nullptr;
}
static void dummy_release_proc(void*) { SkASSERT(0); }
static void dummy_done_proc(void*) {}

////////////////////////////////////////////////////////////////////////////////
// Test out the behavior of an invalid DDLRecorder
DEF_GPUTEST_FOR_RENDERING_CONTEXTS(DDLInvalidRecorder, reporter, ctxInfo) {
    GrContext* context = ctxInfo.grContext();

    {
        SkImageInfo ii = SkImageInfo::MakeN32Premul(32, 32);
        sk_sp<SkSurface> s = SkSurface::MakeRenderTarget(context, SkBudgeted::kNo, ii);

        SkSurfaceCharacterization characterization;
        SkAssertResult(s->characterize(&characterization));

        // never calling getCanvas means the backing surface is never allocated
        SkDeferredDisplayListRecorder recorder(characterization);
    }

    {
        SkSurfaceCharacterization invalid;

        SkDeferredDisplayListRecorder recorder(invalid);

        const SkSurfaceCharacterization c = recorder.characterization();
        REPORTER_ASSERT(reporter, !c.isValid());
        REPORTER_ASSERT(reporter, !recorder.getCanvas());
        REPORTER_ASSERT(reporter, !recorder.detach());

        const GrCaps* caps = context->priv().caps();
        GrBackendFormat format = caps->getBackendFormatFromColorType(kRGBA_8888_SkColorType);

        sk_sp<SkImage> image = recorder.makePromiseTexture(
                format, 32, 32, GrMipMapped::kNo,
                kTopLeft_GrSurfaceOrigin,
                kRGBA_8888_SkColorType,
                kPremul_SkAlphaType, nullptr,
                dummy_fulfill_proc,
                dummy_release_proc,
                dummy_done_proc,
                nullptr);
        REPORTER_ASSERT(reporter, !image);
    }

}

////////////////////////////////////////////////////////////////////////////////
// Ensure that flushing while DDL recording doesn't cause a crash
DEF_GPUTEST_FOR_RENDERING_CONTEXTS(DDLFlushWhileRecording, reporter, ctxInfo) {
    GrContext* context = ctxInfo.grContext();

    SkImageInfo ii = SkImageInfo::MakeN32Premul(32, 32);
    sk_sp<SkSurface> s = SkSurface::MakeRenderTarget(context, SkBudgeted::kNo, ii);

    SkSurfaceCharacterization characterization;
    SkAssertResult(s->characterize(&characterization));

    SkDeferredDisplayListRecorder recorder(characterization);
    SkCanvas* canvas = recorder.getCanvas();

    canvas->getGrContext()->flush();
}

////////////////////////////////////////////////////////////////////////////////
// Ensure that reusing a single DDLRecorder to create multiple DDLs works cleanly
DEF_GPUTEST_FOR_RENDERING_CONTEXTS(DDLMultipleDDLs, reporter, ctxInfo) {
    GrContext* context = ctxInfo.grContext();

    SkImageInfo ii = SkImageInfo::MakeN32Premul(32, 32);
    sk_sp<SkSurface> s = SkSurface::MakeRenderTarget(context, SkBudgeted::kNo, ii);

    SkBitmap bitmap;
    bitmap.allocPixels(ii);

    SkSurfaceCharacterization characterization;
    SkAssertResult(s->characterize(&characterization));

    SkDeferredDisplayListRecorder recorder(characterization);

    SkCanvas* canvas1 = recorder.getCanvas();

    canvas1->clear(SK_ColorRED);

    canvas1->save();
    canvas1->clipRect(SkRect::MakeXYWH(8, 8, 16, 16));

    std::unique_ptr<SkDeferredDisplayList> ddl1 = recorder.detach();

    SkCanvas* canvas2 = recorder.getCanvas();

    SkPaint p;
    p.setColor(SK_ColorGREEN);
    canvas2->drawRect(SkRect::MakeWH(32, 32), p);

    std::unique_ptr<SkDeferredDisplayList> ddl2 = recorder.detach();

    REPORTER_ASSERT(reporter, ddl1->priv().lazyProxyData());
    REPORTER_ASSERT(reporter, ddl2->priv().lazyProxyData());

    // The lazy proxy data being different ensures that the SkSurface, SkCanvas and backing-
    // lazy proxy are all different between the two DDLs
    REPORTER_ASSERT(reporter, ddl1->priv().lazyProxyData() != ddl2->priv().lazyProxyData());

    s->draw(ddl1.get());
    s->draw(ddl2.get());

    // Make sure the clipRect from DDL1 didn't percolate into DDL2
    s->readPixels(ii, bitmap.getPixels(), bitmap.rowBytes(), 0, 0);
    for (int y = 0; y < 32; ++y) {
        for (int x = 0; x < 32; ++x) {
            REPORTER_ASSERT(reporter, bitmap.getColor(x, y) == SK_ColorGREEN);
            if (bitmap.getColor(x, y) != SK_ColorGREEN) {
                return; // we only really need to report the error once
            }
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
// Check that the texture-specific flags (i.e., for external & rectangle textures) work
// for promise images. As such, this is a GL-only test.
DEF_GPUTEST_FOR_GL_RENDERING_CONTEXTS(DDLTextureFlagsTest, reporter, ctxInfo) {
    GrContext* context = ctxInfo.grContext();

    SkImageInfo ii = SkImageInfo::MakeN32Premul(32, 32);
    sk_sp<SkSurface> s = SkSurface::MakeRenderTarget(context, SkBudgeted::kNo, ii);

    SkSurfaceCharacterization characterization;
    SkAssertResult(s->characterize(&characterization));

    SkDeferredDisplayListRecorder recorder(characterization);

    for (GrGLenum target : { GR_GL_TEXTURE_EXTERNAL, GR_GL_TEXTURE_RECTANGLE, GR_GL_TEXTURE_2D } ) {
        for (auto mipMapped : { GrMipMapped::kNo, GrMipMapped::kYes }) {
            GrBackendFormat format = GrBackendFormat::MakeGL(GR_GL_RGBA8, target);

            sk_sp<SkImage> image = recorder.makePromiseTexture(
                    format, 32, 32, mipMapped,
                    kTopLeft_GrSurfaceOrigin,
                    kRGBA_8888_SkColorType,
                    kPremul_SkAlphaType, nullptr,
                    dummy_fulfill_proc,
                    dummy_release_proc,
                    dummy_done_proc,
                    nullptr);
            if (GR_GL_TEXTURE_2D != target && mipMapped == GrMipMapped::kYes) {
                REPORTER_ASSERT(reporter, !image);
                continue;
            }
            REPORTER_ASSERT(reporter, image);

            GrTextureProxy* backingProxy = ((SkImage_GpuBase*) image.get())->peekProxy();

            REPORTER_ASSERT(reporter, backingProxy->mipMapped() == mipMapped);
            if (GR_GL_TEXTURE_2D == target) {
                REPORTER_ASSERT(reporter, !backingProxy->hasRestrictedSampling());
            } else {
                REPORTER_ASSERT(reporter, backingProxy->hasRestrictedSampling());
            }
        }
    }
}

////////////////////////////////////////////////////////////////////////////////

// Test colorType and pixelConfig compatibility.
DEF_GPUTEST_FOR_GL_RENDERING_CONTEXTS(DDLCompatibilityTest, reporter, ctxInfo) {
    GrContext* context = ctxInfo.grContext();

    for (int ct = 0; ct <= kLastEnum_SkColorType; ++ct) {
        SkColorType colorType = static_cast<SkColorType>(ct);

        if (colorType == kRGBA_F16Norm_SkColorType) {
            // TODO(brianosman): we can't yet distinguish F16 and F16Norm in this test.
            continue;
        }

        SurfaceParameters params(context->backend());
        params.setColorType(colorType);
        params.setColorSpace(nullptr);

        SkSurfaceCharacterization c = params.createCharacterization(context);
        GrBackendTexture backend;

        if (!c.isValid()) {
            sk_sp<SkSurface> tmp = params.make(context, &backend);

            // If we couldn't characterize the surface we shouldn't be able to create it either
            REPORTER_ASSERT(reporter, !tmp);
            if (tmp) {
                tmp = nullptr;
                params.cleanUpBackEnd(context, backend);
            }
            continue;
        }

        sk_sp<SkSurface> s = params.make(context, &backend);
        REPORTER_ASSERT(reporter, s);
        if (!s) {
            s = nullptr;
            params.cleanUpBackEnd(context, backend);
            continue;
        }

        SkSurface_Gpu* gpuSurface = static_cast<SkSurface_Gpu*>(s.get());
        REPORTER_ASSERT(reporter, gpuSurface->isCompatible(c));

        s = nullptr;
        params.cleanUpBackEnd(context, backend);

        s = SkSurface::MakeRenderTarget(context, c, SkBudgeted::kYes);
        REPORTER_ASSERT(reporter, s);
        if (!s) {
            continue;
        }

        gpuSurface = static_cast<SkSurface_Gpu*>(s.get());
        REPORTER_ASSERT(reporter, gpuSurface->isCompatible(c));
    }

}
