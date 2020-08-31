/*
 * Copyright 2018 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "tests/Test.h"

#include "include/core/SkColorFilter.h"
#include "include/core/SkPromiseImageTexture.h"
#include "include/gpu/GrBackendSurface.h"
#include "src/gpu/GrContextPriv.h"
#include "src/gpu/GrGpu.h"
#include "src/gpu/GrTexture.h"
#include "src/image/SkImage_Gpu.h"
#include "tests/TestUtils.h"

using namespace sk_gpu_test;

struct PromiseTextureChecker {
    // shared indicates whether the backend texture is used to fulfill more than one promise
    // image.
    explicit PromiseTextureChecker(const GrBackendTexture& tex, skiatest::Reporter* reporter,
                                   bool shared)
            : fTexture(SkPromiseImageTexture::Make(tex))
            , fReporter(reporter)
            , fShared(shared)
            , fFulfillCount(0)
            , fReleaseCount(0)
            , fDoneCount(0) {}
    sk_sp<SkPromiseImageTexture> fTexture;
    skiatest::Reporter* fReporter;
    bool fShared;
    int fFulfillCount;
    int fReleaseCount;
    int fDoneCount;

    /**
     * Releases the SkPromiseImageTexture. Used to test that cached GrTexture representations
     * in the cache are freed.
     */
    void releaseTexture() { fTexture.reset(); }

    SkTArray<GrUniqueKey> uniqueKeys() const {
        return fTexture->testingOnly_uniqueKeysToInvalidate();
    }

    static sk_sp<SkPromiseImageTexture> Fulfill(void* self) {
        auto checker = static_cast<PromiseTextureChecker*>(self);
        checker->fFulfillCount++;
        return checker->fTexture;
    }
    static void Release(void* self) {
        auto checker = static_cast<PromiseTextureChecker*>(self);
        checker->fReleaseCount++;
        if (!checker->fShared) {
            // This is only used in a single threaded fashion with a single promise image. So
            // every fulfill should be balanced by a release before the next fulfill.
            REPORTER_ASSERT(checker->fReporter, checker->fReleaseCount == checker->fFulfillCount);
        }
    }
    static void Done(void* self) {
        static_cast<PromiseTextureChecker*>(self)->fDoneCount++;
    }
};

enum class ReleaseBalanceExpectation {
    kBalanced,
    kAllUnbalanced,
    kUnbalancedByOne,
};

enum class DoneBalanceExpectation {
    kBalanced,
    kAllUnbalanced,
    kUnknown,
    kUnbalancedByOne,
    kBalancedOrOffByOne,
};

static void check_fulfill_and_release_cnts(skiatest::Reporter* reporter,
                                           const PromiseTextureChecker& promiseChecker,
                                           int expectedFulfillCnt,
                                           ReleaseBalanceExpectation releaseBalanceExpecation,
                                           DoneBalanceExpectation doneBalanceExpecation) {
    REPORTER_ASSERT(reporter, promiseChecker.fFulfillCount == expectedFulfillCnt);
    if (!expectedFulfillCnt) {
        // Release and Done should only ever be called after Fulfill.
        REPORTER_ASSERT(reporter, !promiseChecker.fReleaseCount);
        REPORTER_ASSERT(reporter, !promiseChecker.fDoneCount);
        return;
    }
    int releaseDiff = promiseChecker.fFulfillCount - promiseChecker.fReleaseCount;
    switch (releaseBalanceExpecation) {
        case ReleaseBalanceExpectation::kBalanced:
            REPORTER_ASSERT(reporter, !releaseDiff);
            break;
        case ReleaseBalanceExpectation::kAllUnbalanced:
            REPORTER_ASSERT(reporter, releaseDiff == promiseChecker.fFulfillCount);
            break;
        case ReleaseBalanceExpectation::kUnbalancedByOne:
            REPORTER_ASSERT(reporter, releaseDiff == 1);
            break;
    }
    int doneDiff = promiseChecker.fFulfillCount - promiseChecker.fDoneCount;
    switch (doneBalanceExpecation) {
        case DoneBalanceExpectation::kBalanced:
            REPORTER_ASSERT(reporter, !doneDiff);
            break;
        case DoneBalanceExpectation::kAllUnbalanced:
            REPORTER_ASSERT(reporter, doneDiff == promiseChecker.fFulfillCount);
            break;
        case DoneBalanceExpectation::kUnknown:
            REPORTER_ASSERT(reporter, doneDiff >= 0 && doneDiff <= promiseChecker.fFulfillCount);
            break;
        case DoneBalanceExpectation::kUnbalancedByOne:
            REPORTER_ASSERT(reporter, doneDiff == 1);
            break;
        case DoneBalanceExpectation::kBalancedOrOffByOne:
            REPORTER_ASSERT(reporter, doneDiff == 0 || doneDiff == 1);
            break;
    }
}

static void check_unfulfilled(const PromiseTextureChecker& promiseChecker,
                              skiatest::Reporter* reporter) {
    check_fulfill_and_release_cnts(reporter, promiseChecker, 0,
                                   ReleaseBalanceExpectation::kBalanced,
                                   DoneBalanceExpectation::kBalanced);
}

static void check_only_fulfilled(skiatest::Reporter* reporter,
                                 const PromiseTextureChecker& promiseChecker,
                                 int expectedFulfillCnt = 1) {
    check_fulfill_and_release_cnts(reporter, promiseChecker, expectedFulfillCnt,
                                   ReleaseBalanceExpectation::kAllUnbalanced,
                                   DoneBalanceExpectation::kAllUnbalanced);
}

static void check_all_flushed_but_not_synced(skiatest::Reporter* reporter,
                                             const PromiseTextureChecker& promiseChecker,
                                             GrBackendApi api,
                                             int expectedFulfillCnt = 1) {
    DoneBalanceExpectation doneBalanceExpectation = DoneBalanceExpectation::kBalanced;
    // On Vulkan Done isn't guaranteed to be called until a sync has occurred.
    if (api == GrBackendApi::kVulkan) {
        doneBalanceExpectation = expectedFulfillCnt == 1
                                         ? DoneBalanceExpectation::kBalancedOrOffByOne
                                         : DoneBalanceExpectation::kUnknown;
    }
    check_fulfill_and_release_cnts(reporter, promiseChecker, expectedFulfillCnt,
                                   ReleaseBalanceExpectation::kBalanced, doneBalanceExpectation);
}

static void check_all_done(skiatest::Reporter* reporter,
                           const PromiseTextureChecker& promiseChecker,
                           int expectedFulfillCnt = 1) {
    check_fulfill_and_release_cnts(reporter, promiseChecker, expectedFulfillCnt,
                                   ReleaseBalanceExpectation::kBalanced,
                                   DoneBalanceExpectation::kBalanced);
}

DEF_GPUTEST_FOR_RENDERING_CONTEXTS(PromiseImageTest, reporter, ctxInfo) {
    const int kWidth = 10;
    const int kHeight = 10;

    GrContext* ctx = ctxInfo.grContext();
    GrGpu* gpu = ctx->priv().getGpu();

    GrBackendTexture backendTex = ctx->createBackendTexture(
            kWidth, kHeight, kRGBA_8888_SkColorType,
            SkColors::kTransparent, GrMipMapped::kNo, GrRenderable::kYes, GrProtected::kNo);
    REPORTER_ASSERT(reporter, backendTex.isValid());

    GrBackendFormat backendFormat = backendTex.getBackendFormat();
    REPORTER_ASSERT(reporter, backendFormat.isValid());

    PromiseTextureChecker promiseChecker(backendTex, reporter, false);
    GrSurfaceOrigin texOrigin = kTopLeft_GrSurfaceOrigin;
    sk_sp<SkImage> refImg(
            SkImage_Gpu::MakePromiseTexture(
                    ctx, backendFormat, kWidth, kHeight,
                    GrMipMapped::kNo, texOrigin,
                    kRGBA_8888_SkColorType, kPremul_SkAlphaType,
                    nullptr,
                    PromiseTextureChecker::Fulfill,
                    PromiseTextureChecker::Release,
                    PromiseTextureChecker::Done,
                    &promiseChecker,
                    SkDeferredDisplayListRecorder::PromiseImageApiVersion::kNew));

    SkImageInfo info = SkImageInfo::MakeN32Premul(kWidth, kHeight);
    sk_sp<SkSurface> surface = SkSurface::MakeRenderTarget(ctx, SkBudgeted::kNo, info);
    SkCanvas* canvas = surface->getCanvas();

    canvas->drawImage(refImg, 0, 0);
    check_unfulfilled(promiseChecker, reporter);

    surface->flush();
    // We still own the image so we should not have called Release or Done.
    check_only_fulfilled(reporter, promiseChecker);

    gpu->testingOnly_flushGpuAndSync();
    check_only_fulfilled(reporter, promiseChecker);

    canvas->drawImage(refImg, 0, 0);
    canvas->drawImage(refImg, 0, 0);

    surface->flush();

    gpu->testingOnly_flushGpuAndSync();
    // Image should still be fulfilled from the first time we drew/flushed it.
    check_only_fulfilled(reporter, promiseChecker);

    canvas->drawImage(refImg, 0, 0);
    surface->flush();
    check_only_fulfilled(reporter, promiseChecker);

    canvas->drawImage(refImg, 0, 0);
    refImg.reset();
    // We no longer own the image but the last draw is still unflushed.
    check_only_fulfilled(reporter, promiseChecker);

    surface->flush();
    // Flushing should have called Release. Depending on the backend and timing it may have called
    // done.
    check_all_flushed_but_not_synced(reporter, promiseChecker, ctx->backend());
    gpu->testingOnly_flushGpuAndSync();
    // Now Done should definitely have been called.
    check_all_done(reporter, promiseChecker);

    ctx->deleteBackendTexture(backendTex);
}

DEF_GPUTEST(PromiseImageTextureShutdown, reporter, ctxInfo) {
    const int kWidth = 10;
    const int kHeight = 10;

    // Different ways of killing contexts.
    using DeathFn = std::function<void(sk_gpu_test::GrContextFactory*, GrContext*)>;
    DeathFn destroy = [](sk_gpu_test::GrContextFactory* factory, GrContext* context) {
        factory->destroyContexts();
    };
    DeathFn abandon = [](sk_gpu_test::GrContextFactory* factory, GrContext* context) {
        context->abandonContext();
    };
    DeathFn releaseResourcesAndAbandon = [](sk_gpu_test::GrContextFactory* factory,
                                            GrContext* context) {
        context->releaseResourcesAndAbandonContext();
    };

    for (int type = 0; type < sk_gpu_test::GrContextFactory::kContextTypeCnt; ++type) {
        auto contextType = static_cast<sk_gpu_test::GrContextFactory::ContextType>(type);
        // These tests are difficult to get working with Vulkan. See http://skbug.com/8705
        // and http://skbug.com/8275
        GrBackendApi api = sk_gpu_test::GrContextFactory::ContextTypeBackend(contextType);
        if (api == GrBackendApi::kVulkan) {
            continue;
        }
        DeathFn contextKillers[] = {destroy, abandon, releaseResourcesAndAbandon};
        for (auto contextDeath : contextKillers) {
            sk_gpu_test::GrContextFactory factory;
            auto ctx = factory.get(contextType);
            if (!ctx) {
                continue;
            }

            GrBackendTexture backendTex;
            CreateBackendTexture(ctx, &backendTex, kWidth, kHeight, kAlpha_8_SkColorType,
                    SkColors::kTransparent, GrMipMapped::kNo, GrRenderable::kNo, GrProtected::kNo);
            REPORTER_ASSERT(reporter, backendTex.isValid());

            SkImageInfo info = SkImageInfo::Make(kWidth, kHeight, kRGBA_8888_SkColorType,
                                                 kPremul_SkAlphaType);
            sk_sp<SkSurface> surface = SkSurface::MakeRenderTarget(ctx, SkBudgeted::kNo, info);
            SkCanvas* canvas = surface->getCanvas();

            PromiseTextureChecker promiseChecker(backendTex, reporter, false);
            sk_sp<SkImage> image(SkImage_Gpu::MakePromiseTexture(
                    ctx, backendTex.getBackendFormat(), kWidth, kHeight, GrMipMapped::kNo,
                    kTopLeft_GrSurfaceOrigin, kAlpha_8_SkColorType, kPremul_SkAlphaType, nullptr,
                    PromiseTextureChecker::Fulfill, PromiseTextureChecker::Release,
                    PromiseTextureChecker::Done, &promiseChecker,
                    SkDeferredDisplayListRecorder::PromiseImageApiVersion::kNew));
            REPORTER_ASSERT(reporter, image);

            canvas->drawImage(image, 0, 0);
            image.reset();
            // If the surface still holds a ref to the context then the factory will not be able
            // to destroy the context (and instead will release-all-and-abandon).
            surface.reset();

            ctx->flush();
            contextDeath(&factory, ctx);

            check_all_done(reporter, promiseChecker);
        }
    }
}

DEF_GPUTEST_FOR_RENDERING_CONTEXTS(PromiseImageTextureFullCache, reporter, ctxInfo) {
    const int kWidth = 10;
    const int kHeight = 10;

    GrContext* ctx = ctxInfo.grContext();

    GrBackendTexture backendTex = ctx->createBackendTexture(
            kWidth, kHeight, kAlpha_8_SkColorType,
            SkColors::kTransparent, GrMipMapped::kNo, GrRenderable::kNo, GrProtected::kNo);
    REPORTER_ASSERT(reporter, backendTex.isValid());

    SkImageInfo info =
            SkImageInfo::Make(kWidth, kHeight, kRGBA_8888_SkColorType, kPremul_SkAlphaType);
    sk_sp<SkSurface> surface = SkSurface::MakeRenderTarget(ctx, SkBudgeted::kNo, info);
    SkCanvas* canvas = surface->getCanvas();

    PromiseTextureChecker promiseChecker(backendTex, reporter, false);
    sk_sp<SkImage> image(SkImage_Gpu::MakePromiseTexture(
            ctx, backendTex.getBackendFormat(), kWidth, kHeight, GrMipMapped::kNo,
            kTopLeft_GrSurfaceOrigin, kAlpha_8_SkColorType, kPremul_SkAlphaType, nullptr,
            PromiseTextureChecker::Fulfill, PromiseTextureChecker::Release,
            PromiseTextureChecker::Done, &promiseChecker,
            SkDeferredDisplayListRecorder::PromiseImageApiVersion::kNew));
    REPORTER_ASSERT(reporter, image);

    // Make the cache full. This tests that we don't preemptively purge cached textures for
    // fulfillment due to cache pressure.
    static constexpr int kMaxBytes = 1;
    ctx->setResourceCacheLimit(kMaxBytes);
    SkTArray<sk_sp<GrTexture>> textures;
    for (int i = 0; i < 5; ++i) {
        auto format = ctx->priv().caps()->getDefaultBackendFormat(GrColorType::kRGBA_8888,
                                                                  GrRenderable::kNo);
        textures.emplace_back(ctx->priv().resourceProvider()->createTexture(
                {100, 100}, format, GrRenderable::kNo, 1, GrMipMapped::kNo, SkBudgeted::kYes,
                GrProtected::kNo));
        REPORTER_ASSERT(reporter, textures[i]);
    }

    size_t bytesUsed;

    ctx->getResourceCacheUsage(nullptr, &bytesUsed);
    REPORTER_ASSERT(reporter, bytesUsed > kMaxBytes);

    // Relying on the asserts in the promiseImageChecker to ensure that fulfills and releases are
    // properly ordered.
    canvas->drawImage(image, 0, 0);
    surface->flush();
    canvas->drawImage(image, 1, 0);
    surface->flush();
    canvas->drawImage(image, 2, 0);
    surface->flush();
    canvas->drawImage(image, 3, 0);
    surface->flush();
    canvas->drawImage(image, 4, 0);
    surface->flush();
    canvas->drawImage(image, 5, 0);
    surface->flush();
    // Must call these to ensure that all callbacks are performed before the checker is destroyed.
    image.reset();
    ctx->flush();
    ctx->priv().getGpu()->testingOnly_flushGpuAndSync();

    ctx->deleteBackendTexture(backendTex);
}

// Test case where promise image fulfill returns nullptr.
DEF_GPUTEST_FOR_RENDERING_CONTEXTS(PromiseImageNullFulfill, reporter, ctxInfo) {
    const int kWidth = 10;
    const int kHeight = 10;

    GrContext* ctx = ctxInfo.grContext();

    // Do all this just to get a valid backend format for the image.
    GrBackendTexture backendTex;
    CreateBackendTexture(ctx, &backendTex, kWidth, kHeight, kRGBA_8888_SkColorType,
                         SkColors::kTransparent, GrMipMapped::kNo, GrRenderable::kYes,
                         GrProtected::kNo);
    REPORTER_ASSERT(reporter, backendTex.isValid());
    GrBackendFormat backendFormat = backendTex.getBackendFormat();
    REPORTER_ASSERT(reporter, backendFormat.isValid());
    ctx->deleteBackendTexture(backendTex);

    struct Counts {
        int fFulfillCount = 0;
        int fReleaseCount = 0;
        int fDoneCount = 0;
    } counts;
    auto fulfill = [](SkDeferredDisplayListRecorder::PromiseImageTextureContext ctx) {
        ++static_cast<Counts*>(ctx)->fFulfillCount;
        return sk_sp<SkPromiseImageTexture>();
    };
    auto release = [](SkDeferredDisplayListRecorder::PromiseImageTextureContext ctx) {
        ++static_cast<Counts*>(ctx)->fReleaseCount;
    };
    auto done = [](SkDeferredDisplayListRecorder::PromiseImageTextureContext ctx) {
        ++static_cast<Counts*>(ctx)->fDoneCount;
    };
    GrSurfaceOrigin texOrigin = kTopLeft_GrSurfaceOrigin;
    sk_sp<SkImage> refImg(SkImage_Gpu::MakePromiseTexture(
            ctx, backendFormat, kWidth, kHeight, GrMipMapped::kNo, texOrigin,
            kRGBA_8888_SkColorType, kPremul_SkAlphaType, nullptr, fulfill, release, done, &counts,
            SkDeferredDisplayListRecorder::PromiseImageApiVersion::kNew));

    SkImageInfo info = SkImageInfo::MakeN32Premul(kWidth, kHeight);
    sk_sp<SkSurface> surface = SkSurface::MakeRenderTarget(ctx, SkBudgeted::kNo, info);
    SkCanvas* canvas = surface->getCanvas();
    // Draw the image a few different ways.
    canvas->drawImage(refImg, 0, 0);
    SkPaint paint;
    paint.setColorFilter(SkColorFilters::LinearToSRGBGamma());
    canvas->drawImage(refImg, 0, 0, &paint);
    auto shader = refImg->makeShader(SkTileMode::kClamp, SkTileMode::kClamp);
    REPORTER_ASSERT(reporter, shader);
    paint.setShader(std::move(shader));
    canvas->drawRect(SkRect::MakeWH(1,1), paint);
    paint.setShader(nullptr);
    refImg.reset();
    surface->flush();
    // We should only call each callback once and we should have made all the calls by this point.
    REPORTER_ASSERT(reporter, counts.fFulfillCount == 1);
    REPORTER_ASSERT(reporter, counts.fReleaseCount == 1);
    REPORTER_ASSERT(reporter, counts.fDoneCount == 1);
}
