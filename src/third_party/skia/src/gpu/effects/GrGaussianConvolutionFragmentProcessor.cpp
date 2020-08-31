/*
 * Copyright 2012 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "src/gpu/effects/GrGaussianConvolutionFragmentProcessor.h"

#include "src/gpu/GrTexture.h"
#include "src/gpu/GrTextureProxy.h"
#include "src/gpu/effects/GrTextureEffect.h"
#include "src/gpu/glsl/GrGLSLFragmentProcessor.h"
#include "src/gpu/glsl/GrGLSLFragmentShaderBuilder.h"
#include "src/gpu/glsl/GrGLSLProgramDataManager.h"
#include "src/gpu/glsl/GrGLSLUniformHandler.h"

// For brevity
using UniformHandle = GrGLSLProgramDataManager::UniformHandle;
using Direction = GrGaussianConvolutionFragmentProcessor::Direction;

static constexpr int radius_to_width(int r) { return 2*r + 1; }

class GrGaussianConvolutionFragmentProcessor::Impl : public GrGLSLFragmentProcessor {
public:
    void emitCode(EmitArgs&) override;

    static inline void GenKey(const GrProcessor&, const GrShaderCaps&, GrProcessorKeyBuilder*);

protected:
    void onSetData(const GrGLSLProgramDataManager&, const GrFragmentProcessor&) override;

private:
    UniformHandle fKernelUni;
    UniformHandle fIncrementUni;

    typedef GrGLSLFragmentProcessor INHERITED;
};

void GrGaussianConvolutionFragmentProcessor::Impl::emitCode(EmitArgs& args) {
    const GrGaussianConvolutionFragmentProcessor& ce =
            args.fFp.cast<GrGaussianConvolutionFragmentProcessor>();

    GrGLSLUniformHandler* uniformHandler = args.fUniformHandler;

    const char* inc;
    fIncrementUni = uniformHandler->addUniform(&ce, kFragment_GrShaderFlag, kHalf2_GrSLType,
                                               "Increment", &inc);

    int width = radius_to_width(ce.fRadius);

    int arrayCount = (width + 3) / 4;
    SkASSERT(4 * arrayCount >= width);

    const char* kernel;
    fKernelUni = uniformHandler->addUniformArray(&ce, kFragment_GrShaderFlag, kHalf4_GrSLType,
                                                 "Kernel", arrayCount, &kernel);

    GrGLSLFPFragmentBuilder* fragBuilder = args.fFragBuilder;
    auto coords2D = fragBuilder->ensureCoords2D(args.fTransformedCoords[0].fVaryingPoint,
                                                ce.sampleMatrix());

    fragBuilder->codeAppendf("%s = half4(0, 0, 0, 0);", args.fOutputColor);

    fragBuilder->codeAppendf("float2 coord = %s - %d.0 * %s;", coords2D.c_str(), ce.fRadius, inc);
    fragBuilder->codeAppend("float2 coordSampled = half2(0, 0);");

    // Manually unroll loop because some drivers don't; yields 20-30% speedup.
    static constexpr const char* kVecSuffix[4] = {".x", ".y", ".z", ".w"};
    for (int i = 0; i < width; i++) {
        SkString kernelIndex;
        kernelIndex.printf("%s[%d]", kernel, i/4);
        kernelIndex.append(kVecSuffix[i & 0x3]);

        fragBuilder->codeAppend("coordSampled = coord;");
        auto sample = this->invokeChild(0, args, "coordSampled");
        fragBuilder->codeAppendf("%s += %s", args.fOutputColor, sample.c_str());
        fragBuilder->codeAppendf(" * %s;", kernelIndex.c_str());
        fragBuilder->codeAppendf("coord += %s;", inc);
    }
    fragBuilder->codeAppendf("%s *= %s;", args.fOutputColor, args.fInputColor);
}

void GrGaussianConvolutionFragmentProcessor::Impl::onSetData(const GrGLSLProgramDataManager& pdman,
                                                             const GrFragmentProcessor& processor) {
    const auto& conv = processor.cast<GrGaussianConvolutionFragmentProcessor>();

    float increment[2] = {};
    increment[static_cast<int>(conv.fDirection)] = 1;
    pdman.set2fv(fIncrementUni, 1, increment);

    int width = radius_to_width(conv.fRadius);
    int arrayCount = (width + 3)/4;
    SkDEBUGCODE(size_t arraySize = 4*arrayCount;)
    SkASSERT(arraySize >= static_cast<size_t>(width));
    SkASSERT(arraySize <= SK_ARRAY_COUNT(GrGaussianConvolutionFragmentProcessor::fKernel));
    pdman.set4fv(fKernelUni, arrayCount, conv.fKernel);
}

void GrGaussianConvolutionFragmentProcessor::Impl::GenKey(const GrProcessor& processor,
                                                          const GrShaderCaps&,
                                                          GrProcessorKeyBuilder* b) {
    const auto& conv = processor.cast<GrGaussianConvolutionFragmentProcessor>();
    b->add32(conv.fRadius);
}

///////////////////////////////////////////////////////////////////////////////

static void fill_in_1D_gaussian_kernel(float* kernel, float gaussianSigma, int radius) {
    const float twoSigmaSqrd = 2.0f * gaussianSigma * gaussianSigma;
    int width = radius_to_width(radius);
    if (SkScalarNearlyZero(twoSigmaSqrd, SK_ScalarNearlyZero)) {
        for (int i = 0; i < width; ++i) {
            kernel[i] = 0.0f;
        }
        return;
    }

    const float denom = 1.0f / twoSigmaSqrd;

    float sum = 0.0f;
    for (int i = 0; i < width; ++i) {
        float x = static_cast<float>(i - radius);
        // Note that the constant term (1/(sqrt(2*pi*sigma^2)) of the Gaussian
        // is dropped here, since we renormalize the kernel below.
        kernel[i] = sk_float_exp(-x * x * denom);
        sum += kernel[i];
    }
    // Normalize the kernel
    float scale = 1.0f / sum;
    for (int i = 0; i < width; ++i) {
        kernel[i] *= scale;
    }
}

std::unique_ptr<GrFragmentProcessor> GrGaussianConvolutionFragmentProcessor::Make(
        GrSurfaceProxyView view,
        SkAlphaType alphaType,
        Direction dir,
        int halfWidth,
        float gaussianSigma,
        GrSamplerState::WrapMode wm,
        const int bounds[2],
        const GrCaps& caps) {
    std::unique_ptr<GrFragmentProcessor> child;
    GrSamplerState sampler;
    switch (dir) {
        case Direction::kX: sampler.setWrapModeX(wm); break;
        case Direction::kY: sampler.setWrapModeY(wm); break;
    }
    if (bounds) {
        SkASSERT(bounds[0] < bounds[1]);
        SkRect subset;
        switch (dir) {
            case Direction::kX:
                subset = SkRect::MakeLTRB(bounds[0], 0, bounds[1], view.height());
                break;
            case Direction::kY:
                subset = SkRect::MakeLTRB(0, bounds[0], view.width(), bounds[1]);
                break;
        }
        child = GrTextureEffect::MakeSubset(std::move(view), alphaType, SkMatrix::I(), sampler,
                                            subset, caps);
    } else {
        child = GrTextureEffect::Make(std::move(view), alphaType, SkMatrix::I(), sampler, caps);
    }
    return std::unique_ptr<GrFragmentProcessor>(new GrGaussianConvolutionFragmentProcessor(
            std::move(child), dir, halfWidth, gaussianSigma));
}

GrGaussianConvolutionFragmentProcessor::GrGaussianConvolutionFragmentProcessor(
        std::unique_ptr<GrFragmentProcessor> child,
        Direction direction,
        int radius,
        float gaussianSigma)
        : INHERITED(kGrGaussianConvolutionFragmentProcessor_ClassID,
                    ProcessorOptimizationFlags(child.get()))
        , fRadius(radius)
        , fDirection(direction) {
    child->setSampledWithExplicitCoords();
    this->registerChildProcessor(std::move(child));
    SkASSERT(radius <= kMaxKernelRadius);
    fill_in_1D_gaussian_kernel(fKernel, gaussianSigma, fRadius);
    this->addCoordTransform(&fCoordTransform);
}

GrGaussianConvolutionFragmentProcessor::GrGaussianConvolutionFragmentProcessor(
        const GrGaussianConvolutionFragmentProcessor& that)
        : INHERITED(kGrGaussianConvolutionFragmentProcessor_ClassID, that.optimizationFlags())
        , fRadius(that.fRadius)
        , fDirection(that.fDirection) {
    auto child = that.childProcessor(0).clone();
    child->setSampledWithExplicitCoords();
    this->registerChildProcessor(std::move(child));
    memcpy(fKernel, that.fKernel, radius_to_width(fRadius) * sizeof(float));
    this->addCoordTransform(&fCoordTransform);
}

void GrGaussianConvolutionFragmentProcessor::onGetGLSLProcessorKey(const GrShaderCaps& caps,
                                                                   GrProcessorKeyBuilder* b) const {
    Impl::GenKey(*this, caps, b);
}

GrGLSLFragmentProcessor* GrGaussianConvolutionFragmentProcessor::onCreateGLSLInstance() const {
    return new Impl;
}

bool GrGaussianConvolutionFragmentProcessor::onIsEqual(const GrFragmentProcessor& sBase) const {
    const auto& that = sBase.cast<GrGaussianConvolutionFragmentProcessor>();
    return fRadius == that.fRadius && fDirection == that.fDirection &&
           std::equal(fKernel, fKernel + radius_to_width(fRadius), that.fKernel);
}

///////////////////////////////////////////////////////////////////////////////

GR_DEFINE_FRAGMENT_PROCESSOR_TEST(GrGaussianConvolutionFragmentProcessor);

#if GR_TEST_UTILS
std::unique_ptr<GrFragmentProcessor> GrGaussianConvolutionFragmentProcessor::TestCreate(
        GrProcessorTestData* d) {
    auto [view, ct, at] = d->randomView();

    Direction dir;
    int bounds[2];
    do {
        if (d->fRandom->nextBool()) {
            dir = Direction::kX;
            bounds[0] = d->fRandom->nextRangeU(0, view.width() - 1);
            bounds[1] = d->fRandom->nextRangeU(0, view.width() - 1);
        } else {
            dir = Direction::kY;
            bounds[0] = d->fRandom->nextRangeU(0, view.height() - 1);
            bounds[1] = d->fRandom->nextRangeU(0, view.height() - 1);
        }
    } while (bounds[0] == bounds[1]);
    std::sort(bounds, bounds + 2);

    auto wm = static_cast<GrSamplerState::WrapMode>(
            d->fRandom->nextULessThan(GrSamplerState::kWrapModeCount));
    int radius = d->fRandom->nextRangeU(1, kMaxKernelRadius);
    float sigma = radius / 3.f;

    return GrGaussianConvolutionFragmentProcessor::Make(std::move(view), at, dir, radius, sigma, wm,
                                                        bounds, *d->caps());
}
#endif
