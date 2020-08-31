/*
 * Copyright 2011 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// This is a GPU-backend specific test.

#include "include/core/SkTypes.h"

#include "include/private/SkChecksum.h"
#include "include/utils/SkRandom.h"
#include "src/gpu/GrAutoLocaleSetter.h"
#include "src/gpu/GrContextPriv.h"
#include "src/gpu/GrDrawOpTest.h"
#include "src/gpu/GrDrawingManager.h"
#include "src/gpu/GrPipeline.h"
#include "src/gpu/GrProxyProvider.h"
#include "src/gpu/GrRenderTargetContextPriv.h"
#include "src/gpu/GrXferProcessor.h"
#include "src/gpu/effects/GrPorterDuffXferProcessor.h"
#include "src/gpu/effects/GrXfermodeFragmentProcessor.h"
#include "src/gpu/effects/generated/GrConfigConversionEffect.h"
#include "src/gpu/glsl/GrGLSLFragmentProcessor.h"
#include "src/gpu/glsl/GrGLSLFragmentShaderBuilder.h"
#include "src/gpu/glsl/GrGLSLProgramBuilder.h"
#include "src/gpu/ops/GrDrawOp.h"
#include "tests/Test.h"
#include "tools/gpu/GrContextFactory.h"

#ifdef SK_GL
#include "src/gpu/gl/GrGLGpu.h"
#endif

/*
 * A dummy processor which just tries to insert a massive key and verify that it can retrieve the
 * whole thing correctly
 */
static const uint32_t kMaxKeySize = 1024;

class GLBigKeyProcessor : public GrGLSLFragmentProcessor {
public:
    void emitCode(EmitArgs& args) override {
        // pass through
        GrGLSLFragmentBuilder* fragBuilder = args.fFragBuilder;
        if (args.fInputColor) {
            fragBuilder->codeAppendf("%s = %s;\n", args.fOutputColor, args.fInputColor);
        } else {
            fragBuilder->codeAppendf("%s = vec4(1.0);\n", args.fOutputColor);
        }
    }

    static void GenKey(const GrProcessor&, const GrShaderCaps&, GrProcessorKeyBuilder* b) {
        for (uint32_t i = 0; i < kMaxKeySize; i++) {
            b->add32(i);
        }
    }

private:
    typedef GrGLSLFragmentProcessor INHERITED;
};

class BigKeyProcessor : public GrFragmentProcessor {
public:
    static std::unique_ptr<GrFragmentProcessor> Make() {
        return std::unique_ptr<GrFragmentProcessor>(new BigKeyProcessor);
    }

    const char* name() const override { return "Big_Ole_Key"; }

    GrGLSLFragmentProcessor* onCreateGLSLInstance() const override {
        return new GLBigKeyProcessor;
    }

    std::unique_ptr<GrFragmentProcessor> clone() const override { return Make(); }

private:
    BigKeyProcessor() : INHERITED(kBigKeyProcessor_ClassID, kNone_OptimizationFlags) { }
    virtual void onGetGLSLProcessorKey(const GrShaderCaps& caps,
                                       GrProcessorKeyBuilder* b) const override {
        GLBigKeyProcessor::GenKey(*this, caps, b);
    }
    bool onIsEqual(const GrFragmentProcessor&) const override { return true; }

    GR_DECLARE_FRAGMENT_PROCESSOR_TEST

    typedef GrFragmentProcessor INHERITED;
};

GR_DEFINE_FRAGMENT_PROCESSOR_TEST(BigKeyProcessor);

#if GR_TEST_UTILS
std::unique_ptr<GrFragmentProcessor> BigKeyProcessor::TestCreate(GrProcessorTestData*) {
    return BigKeyProcessor::Make();
}
#endif

//////////////////////////////////////////////////////////////////////////////

class BlockInputFragmentProcessor : public GrFragmentProcessor {
public:
    static std::unique_ptr<GrFragmentProcessor> Make(std::unique_ptr<GrFragmentProcessor> fp) {
        return std::unique_ptr<GrFragmentProcessor>(new BlockInputFragmentProcessor(std::move(fp)));
    }

    const char* name() const override { return "Block_Input"; }

    GrGLSLFragmentProcessor* onCreateGLSLInstance() const override { return new GLFP; }

    std::unique_ptr<GrFragmentProcessor> clone() const override {
        return Make(this->childProcessor(0).clone());
    }

private:
    class GLFP : public GrGLSLFragmentProcessor {
    public:
        void emitCode(EmitArgs& args) override {
            SkString temp = this->invokeChild(0, args);
            args.fFragBuilder->codeAppendf("%s = %s;", args.fOutputColor, temp.c_str());
        }

    private:
        typedef GrGLSLFragmentProcessor INHERITED;
    };

    BlockInputFragmentProcessor(std::unique_ptr<GrFragmentProcessor> child)
            : INHERITED(kBlockInputFragmentProcessor_ClassID, kNone_OptimizationFlags) {
        this->registerChildProcessor(std::move(child));
    }

    void onGetGLSLProcessorKey(const GrShaderCaps& caps, GrProcessorKeyBuilder* b) const override {}

    bool onIsEqual(const GrFragmentProcessor&) const override { return true; }

    typedef GrFragmentProcessor INHERITED;
};

//////////////////////////////////////////////////////////////////////////////

/*
 * Begin test code
 */
static const int kRenderTargetHeight = 1;
static const int kRenderTargetWidth = 1;

static std::unique_ptr<GrRenderTargetContext> random_render_target_context(GrContext* context,
                                                                           SkRandom* random,
                                                                           const GrCaps* caps) {
    GrSurfaceOrigin origin = random->nextBool() ? kTopLeft_GrSurfaceOrigin
                                                : kBottomLeft_GrSurfaceOrigin;

    GrColorType ct = GrColorType::kRGBA_8888;
    const GrBackendFormat format = caps->getDefaultBackendFormat(ct, GrRenderable::kYes);

    int sampleCnt = random->nextBool() ? caps->getRenderTargetSampleCount(2, format) : 1;
    // Above could be 0 if msaa isn't supported.
    sampleCnt = std::max(1, sampleCnt);

    return GrRenderTargetContext::Make(
            context, GrColorType::kRGBA_8888, nullptr, SkBackingFit::kExact,
            {kRenderTargetWidth, kRenderTargetHeight}, sampleCnt, GrMipMapped::kNo,
            GrProtected::kNo, origin);
}

#if GR_TEST_UTILS
static void set_random_xpf(GrPaint* paint, GrProcessorTestData* d) {
    paint->setXPFactory(GrXPFactoryTestFactory::Get(d));
}

static std::unique_ptr<GrFragmentProcessor> create_random_proc_tree(GrProcessorTestData* d,
                                                                    int minLevels, int maxLevels) {
    SkASSERT(1 <= minLevels);
    SkASSERT(minLevels <= maxLevels);

    // Return a leaf node if maxLevels is 1 or if we randomly chose to terminate.
    // If returning a leaf node, make sure that it doesn't have children (e.g. another
    // GrComposeEffect)
    const float terminateProbability = 0.3f;
    if (1 == minLevels) {
        bool terminate = (1 == maxLevels) || (d->fRandom->nextF() < terminateProbability);
        if (terminate) {
            std::unique_ptr<GrFragmentProcessor> fp;
            while (true) {
                fp = GrFragmentProcessorTestFactory::Make(d);
                if (!fp) {
                    return nullptr;
                }
                if (0 == fp->numChildProcessors()) {
                    break;
                }
            }
            return fp;
        }
    }
    // If we didn't terminate, choose either the left or right subtree to fulfill
    // the minLevels requirement of this tree; the other child can have as few levels as it wants.
    // Also choose a random xfer mode.
    if (minLevels > 1) {
        --minLevels;
    }
    auto minLevelsChild = create_random_proc_tree(d, minLevels, maxLevels - 1);
    std::unique_ptr<GrFragmentProcessor> otherChild(create_random_proc_tree(d, 1, maxLevels - 1));
    if (!minLevelsChild || !otherChild) {
        return nullptr;
    }
    SkBlendMode mode = static_cast<SkBlendMode>(d->fRandom->nextRangeU(0,
                                                               (int)SkBlendMode::kLastMode));
    std::unique_ptr<GrFragmentProcessor> fp;
    if (d->fRandom->nextF() < 0.5f) {
        fp = GrXfermodeFragmentProcessor::MakeFromTwoProcessors(std::move(minLevelsChild),
                                                                std::move(otherChild), mode);
        SkASSERT(fp);
    } else {
        fp = GrXfermodeFragmentProcessor::MakeFromTwoProcessors(std::move(otherChild),
                                                                std::move(minLevelsChild), mode);
        SkASSERT(fp);
    }
    return fp;
}

static void set_random_color_coverage_stages(GrPaint* paint,
                                             GrProcessorTestData* d,
                                             int maxStages,
                                             int maxTreeLevels) {
    // Randomly choose to either create a linear pipeline of procs or create one proc tree
    const float procTreeProbability = 0.5f;
    if (d->fRandom->nextF() < procTreeProbability) {
        std::unique_ptr<GrFragmentProcessor> fp(create_random_proc_tree(d, 2, maxTreeLevels));
        if (fp) {
            paint->addColorFragmentProcessor(std::move(fp));
        }
    } else {
        int numProcs = d->fRandom->nextULessThan(maxStages + 1);
        int numColorProcs = d->fRandom->nextULessThan(numProcs + 1);

        for (int s = 0; s < numProcs; ++s) {
            std::unique_ptr<GrFragmentProcessor> fp(GrFragmentProcessorTestFactory::Make(d));
            if (!fp) {
                continue;
            }
            // finally add the stage to the correct pipeline in the drawstate
            if (s < numColorProcs) {
                paint->addColorFragmentProcessor(std::move(fp));
            } else {
                paint->addCoverageFragmentProcessor(std::move(fp));
            }
        }
    }
}

#endif

#if !GR_TEST_UTILS
bool GrDrawingManager::ProgramUnitTest(GrContext*, int) { return true; }
#else
bool GrDrawingManager::ProgramUnitTest(GrContext* context, int maxStages, int maxLevels) {
    GrDrawingManager* drawingManager = context->priv().drawingManager();
    GrProxyProvider* proxyProvider = context->priv().proxyProvider();

    GrProcessorTestData::ViewInfo views[2];

    // setup dummy textures
    GrMipMapped mipMapped = GrMipMapped(context->priv().caps()->mipMapSupport());
    {
        static constexpr SkISize kDummyDims = {34, 18};
        const GrBackendFormat format =
            context->priv().caps()->getDefaultBackendFormat(GrColorType::kRGBA_8888,
                                                            GrRenderable::kYes);
        auto proxy = proxyProvider->createProxy(format, kDummyDims, GrRenderable::kYes, 1,
                                                mipMapped, SkBackingFit::kExact, SkBudgeted::kNo,
                                                GrProtected::kNo, GrInternalSurfaceFlags::kNone);
        GrSwizzle swizzle = context->priv().caps()->getReadSwizzle(format, GrColorType::kRGBA_8888);
        views[0] = {{std::move(proxy), kBottomLeft_GrSurfaceOrigin, swizzle},
                    GrColorType::kRGBA_8888, kPremul_SkAlphaType};
    }
    {
        static constexpr SkISize kDummyDims = {16, 22};
        const GrBackendFormat format =
            context->priv().caps()->getDefaultBackendFormat(GrColorType::kAlpha_8,
                                                            GrRenderable::kNo);
        auto proxy = proxyProvider->createProxy(format, kDummyDims, GrRenderable::kNo, 1, mipMapped,
                                                SkBackingFit::kExact, SkBudgeted::kNo,
                                                GrProtected::kNo, GrInternalSurfaceFlags::kNone);
        GrSwizzle swizzle = context->priv().caps()->getReadSwizzle(format, GrColorType::kAlpha_8);
        views[1] = {{std::move(proxy), kTopLeft_GrSurfaceOrigin, swizzle},
                      GrColorType::kAlpha_8, kPremul_SkAlphaType};
    }

    if (!std::get<0>(views[0]) || !std::get<0>(views[1])) {
        SkDebugf("Could not allocate dummy textures");
        return false;
    }

    SkRandom random;
    static const int NUM_TESTS = 1024;
    for (int t = 0; t < NUM_TESTS; t++) {
        // setup random render target(can fail)
        auto renderTargetContext =
                random_render_target_context(context, &random, context->priv().caps());
        if (!renderTargetContext) {
            SkDebugf("Could not allocate renderTargetContext");
            return false;
        }

        GrPaint paint;
        GrProcessorTestData ptd(&random, context, 2, views);
        set_random_color_coverage_stages(&paint, &ptd, maxStages, maxLevels);
        set_random_xpf(&paint, &ptd);
        GrDrawRandomOp(&random, renderTargetContext.get(), std::move(paint));
    }
    // Flush everything, test passes if flush is successful(ie, no asserts are hit, no crashes)
    if (drawingManager->flush(nullptr, 0, SkSurface::BackendSurfaceAccess::kNoAccess, GrFlushInfo(),
                              GrPrepareForExternalIORequests())) {
        drawingManager->submitToGpu(false);
    }

    // Validate that GrFPs work correctly without an input.
    auto renderTargetContext = GrRenderTargetContext::Make(
            context, GrColorType::kRGBA_8888, nullptr, SkBackingFit::kExact,
            {kRenderTargetWidth, kRenderTargetHeight});
    if (!renderTargetContext) {
        SkDebugf("Could not allocate a renderTargetContext");
        return false;
    }

    int fpFactoryCnt = GrFragmentProcessorTestFactory::Count();
    for (int i = 0; i < fpFactoryCnt; ++i) {
        // Since FP factories internally randomize, call each 10 times.
        for (int j = 0; j < 10; ++j) {
            GrProcessorTestData ptd(&random, context, 2, views);

            GrPaint paint;
            paint.setXPFactory(GrPorterDuffXPFactory::Get(SkBlendMode::kSrc));
            auto fp = GrFragmentProcessorTestFactory::MakeIdx(i, &ptd);
            auto blockFP = BlockInputFragmentProcessor::Make(std::move(fp));
            paint.addColorFragmentProcessor(std::move(blockFP));
            GrDrawRandomOp(&random, renderTargetContext.get(), std::move(paint));
            if (drawingManager->flush(nullptr, 0, SkSurface::BackendSurfaceAccess::kNoAccess,
                                      GrFlushInfo(), GrPrepareForExternalIORequests())) {
                drawingManager->submitToGpu(false);
            }
        }
    }

    return true;
}
#endif

static int get_programs_max_stages(const sk_gpu_test::ContextInfo& ctxInfo) {
    int maxStages = 6;
#ifdef SK_GL
    GrContext* context = ctxInfo.grContext();
    if (skiatest::IsGLContextType(ctxInfo.type())) {
        GrGLGpu* gpu = static_cast<GrGLGpu*>(context->priv().getGpu());
        if (kGLES_GrGLStandard == gpu->glStandard()) {
        // We've had issues with driver crashes and HW limits being exceeded with many effects on
        // Android devices. We have passes on ARM devices with the default number of stages.
        // TODO When we run ES 3.00 GLSL in more places, test again
#ifdef SK_BUILD_FOR_ANDROID
            if (kARM_GrGLVendor != gpu->ctxInfo().vendor()) {
                maxStages = 1;
            }
#endif
        // On iOS we can exceed the maximum number of varyings. http://skbug.com/6627.
#ifdef SK_BUILD_FOR_IOS
            maxStages = 3;
#endif
        }
        // On Angle D3D we will hit a limit of out variables if we use too many stages. This is
        // particularly true on D3D9 with a low limit on varyings and the fact that every varying is
        // packed as though it has 4 components.
        if (ctxInfo.type() == sk_gpu_test::GrContextFactory::kANGLE_D3D9_ES2_ContextType) {
            maxStages = 2;
        } else if (ctxInfo.type() == sk_gpu_test::GrContextFactory::kANGLE_D3D11_ES2_ContextType) {
            maxStages = 3;
        }
    }
#endif
    return maxStages;
}

static int get_programs_max_levels(const sk_gpu_test::ContextInfo& ctxInfo) {
    // A full tree with 5 levels (31 nodes) may cause a program that exceeds shader limits
    // (e.g. uniform or varying limits); maxTreeLevels should be a number from 1 to 4 inclusive.
    int maxTreeLevels = 4;
    if (skiatest::IsGLContextType(ctxInfo.type())) {
        // On iOS we can exceed the maximum number of varyings. http://skbug.com/6627.
#ifdef SK_BUILD_FOR_IOS
        maxTreeLevels = 2;
#endif
#ifdef SK_BUILD_FOR_ANDROID
        GrGLGpu* gpu = static_cast<GrGLGpu*>(ctxInfo.grContext()->priv().getGpu());
        // Tecno Spark 3 Pro with Power VR Rogue GE8300 will fail shader compiles with
        // no message if the shader is particularly long.
        if (gpu->ctxInfo().vendor() == kImagination_GrGLVendor) {
            maxTreeLevels = 3;
        }
#endif
        if (ctxInfo.type() == sk_gpu_test::GrContextFactory::kANGLE_D3D9_ES2_ContextType ||
            ctxInfo.type() == sk_gpu_test::GrContextFactory::kANGLE_D3D11_ES2_ContextType) {
            // On Angle D3D we will hit a limit of out variables if we use too many stages.
            maxTreeLevels = 2;
        }
    }
    return maxTreeLevels;
}

static void test_programs(skiatest::Reporter* reporter, const sk_gpu_test::ContextInfo& ctxInfo) {
    int maxStages = get_programs_max_stages(ctxInfo);
    if (maxStages == 0) {
        return;
    }
    int maxLevels = get_programs_max_levels(ctxInfo);
    if (maxLevels == 0) {
        return;
    }

    REPORTER_ASSERT(reporter, GrDrawingManager::ProgramUnitTest(ctxInfo.grContext(), maxStages,
                                                                maxLevels));
}

DEF_GPUTEST(Programs, reporter, options) {
    // Set a locale that would cause shader compilation to fail because of , as decimal separator.
    // skbug 3330
#ifdef SK_BUILD_FOR_WIN
    GrAutoLocaleSetter als("sv-SE");
#else
    GrAutoLocaleSetter als("sv_SE.UTF-8");
#endif

    // We suppress prints to avoid spew
    GrContextOptions opts = options;
    opts.fSuppressPrints = true;
    sk_gpu_test::GrContextFactory debugFactory(opts);
    skiatest::RunWithGPUTestContexts(test_programs, &skiatest::IsRenderingGLOrMetalContextType,
                                     reporter, opts);
}
