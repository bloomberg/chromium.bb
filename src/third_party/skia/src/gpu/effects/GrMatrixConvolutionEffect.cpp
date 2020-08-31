/*
 * Copyright 2014 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "src/gpu/effects/GrMatrixConvolutionEffect.h"

#include "include/private/SkHalf.h"
#include "src/gpu/GrBitmapTextureMaker.h"
#include "src/gpu/GrContextPriv.h"
#include "src/gpu/GrProxyProvider.h"
#include "src/gpu/GrRecordingContextPriv.h"
#include "src/gpu/GrTexture.h"
#include "src/gpu/GrTextureProxy.h"
#include "src/gpu/effects/GrTextureEffect.h"
#include "src/gpu/glsl/GrGLSLFragmentProcessor.h"
#include "src/gpu/glsl/GrGLSLFragmentShaderBuilder.h"
#include "src/gpu/glsl/GrGLSLProgramDataManager.h"
#include "src/gpu/glsl/GrGLSLUniformHandler.h"

class GrGLMatrixConvolutionEffect : public GrGLSLFragmentProcessor {
public:
    void emitCode(EmitArgs&) override;

    static inline void GenKey(const GrProcessor&, const GrShaderCaps&, GrProcessorKeyBuilder*);

protected:
    void onSetData(const GrGLSLProgramDataManager&, const GrFragmentProcessor&) override;

private:
    typedef GrGLSLProgramDataManager::UniformHandle UniformHandle;

    void emitKernelBlock(EmitArgs&, SkIPoint);

    UniformHandle               fKernelUni;
    UniformHandle               fKernelOffsetUni;
    UniformHandle               fGainUni;
    UniformHandle               fBiasUni;
    UniformHandle               fKernelBiasUni;

    typedef GrGLSLFragmentProcessor INHERITED;
};

GrMatrixConvolutionEffect::KernelWrapper GrMatrixConvolutionEffect::KernelWrapper::Make(
        GrRecordingContext* context, SkISize size, const GrCaps& caps, const SkScalar* values) {
    if (nullptr == context || nullptr == values || size.isEmpty()) {
        return {};
    }
    const int length = size.area();
    // Small kernel -> just fill the array.
    KernelWrapper result(size);
    if (length <= kMaxUniformSize) {
        for (int i = 0; i < length; i++) {
            result.fArray[i] = SkScalarToFloat(values[i]);
        }
        return result;
    }

    ScalableSampler& scalableSampler = result.fScalableSampler;
    bool useA16 =
        context->defaultBackendFormat(kA16_float_SkColorType, GrRenderable::kNo).isValid();
    SkScalar min = values[0];
    if (!useA16) {
        // Determine min and max values to figure out inner gain & bias.
        SkScalar max = values[0];
        for (int i = 1; i < length; i++) {
            if (values[i] < min) {
                min = values[i];
            }
            if (values[i] > max) {
                max = values[i];
            }
        }
        // Treat near-0 gain (i.e. box blur) as 1, and let the kernelBias
        // move everything up to the final value.
        const SkScalar computedGain = max - min;
        scalableSampler.fGain =
            SkScalarNearlyZero(computedGain) ? 1.0f : SkScalarToFloat(computedGain);
        // Inner bias is pre-inner-gain so we divide that out.
        scalableSampler.fBias = SkScalarToFloat(min) / scalableSampler.fGain;
    }

    // TODO: Pick cache or dont-cache based on observed perf.
    static constexpr bool kCacheKernelTexture = true;

    GrUniqueKey key;
    if (kCacheKernelTexture) {
        static const GrUniqueKey::Domain kDomain = GrUniqueKey::GenerateDomain();
        GrUniqueKey::Builder builder(&key, kDomain, length, "Matrix Convolution Kernel");
        // Texture cache key is the exact content of the kernel.
        static_assert(sizeof(float) == 4);
        for (int i = 0; i < length; i++) {
            builder[i] = *(const uint32_t*)&values[i];
        }
        builder.finish();
    }

    // Find or create a texture.
    GrProxyProvider* proxyProvider = context->priv().proxyProvider();
    GrSurfaceProxyView view;
    SkColorType colorType = useA16 ? kA16_float_SkColorType : kAlpha_8_SkColorType;
    sk_sp<GrTextureProxy> cachedKernel;
    if (kCacheKernelTexture && (cachedKernel = proxyProvider->findOrCreateProxyByUniqueKey(key))) {
        GrSwizzle swizzle =
            context->priv().caps()->getReadSwizzle(cachedKernel->backendFormat(),
                                                   SkColorTypeToGrColorType(colorType));
        view = {std::move(cachedKernel), kTopLeft_GrSurfaceOrigin, swizzle};
    } else {
        SkBitmap bm;
        auto info = SkImageInfo::Make({(int)GrNextPow2(length), 1}, colorType,
                                      kPremul_SkAlphaType, nullptr);
        if (!bm.tryAllocPixels(info)) {
            return {};
        }
        for (int i = 0; i < length; i++) {
            if (useA16) {
                *bm.getAddr16(i, 0) = SkFloatToHalf(values[i]);
            } else {
                *bm.getAddr8(i, 0) =
                    SkScalarRoundToInt((values[i] - min) / scalableSampler.fGain * 255);
            }
        }
        bm.setImmutable();
        GrBitmapTextureMaker maker(context, bm, GrImageTexGenPolicy::kNew_Uncached_Budgeted);
        view = maker.view(GrMipMapped::kNo);
        if (!view) {
            return {};
        }
        if (kCacheKernelTexture) {
            proxyProvider->assignUniqueKeyToProxy(key, view.asTextureProxy());
        }
    }
    scalableSampler.fSampler = { std::move(view) };
    return result;
}

bool GrMatrixConvolutionEffect::KernelWrapper::operator==(const KernelWrapper& k) const {
    if (fSize != k.fSize) {
        return false;
    } else if (this->isSampled()) {
        return fScalableSampler == k.fScalableSampler;
    } else {
        return std::equal(fArray.begin(), fArray.begin() + fSize.area(), k.fArray.begin());
    }
}

bool GrMatrixConvolutionEffect::KernelWrapper::ScalableSampler::operator==(
                                                                const ScalableSampler& k) const {
    return fSampler == k.fSampler && fGain == k.fGain && fBias == k.fBias;
}

// For sampled kernels, emit a for loop that does all the kernel accumulation.
// For uniform kernels, emit a single iteration. Function is called repeatedly in a for loop.
// loc is ignored for sampled kernels.
void GrGLMatrixConvolutionEffect::emitKernelBlock(EmitArgs& args, SkIPoint loc) {
    const GrMatrixConvolutionEffect& mce = args.fFp.cast<GrMatrixConvolutionEffect>();
    GrGLSLFPFragmentBuilder* fragBuilder = args.fFragBuilder;
    GrGLSLUniformHandler* uniformHandler = args.fUniformHandler;
    int kernelWidth = mce.kernelSize().width();
    int kernelHeight = mce.kernelSize().height();
    int kernelArea = kernelWidth * kernelHeight;

    if (mce.kernelIsSampled()) {
        fragBuilder->codeAppendf("half2 kernelCoord = half2(0, 0);");
        fragBuilder->codeAppendf("for (int i = 0; i < %d; ++i)", (int)kernelArea);
    }

    GrGLSLShaderBuilder::ShaderBlock block(fragBuilder);

    fragBuilder->codeAppend("half k;");
    fragBuilder->codeAppend("half2 sourceOffset;");
    if (mce.kernelIsSampled()) {
        const char* kernelBias = uniformHandler->getUniformCStr(fKernelBiasUni);
        fragBuilder->codeAppend("k = ");
        fragBuilder->appendTextureLookup(args.fTexSamplers[0], "kernelCoord");
        fragBuilder->codeAppendf(".w + %s;", kernelBias);
        fragBuilder->codeAppendf("sourceOffset.y = floor(i / %d);", kernelWidth);
        fragBuilder->codeAppendf("sourceOffset.x = i - sourceOffset.y * %d;", kernelWidth);
        float kernelStride = 1.0f / (float)GrNextPow2(kernelArea);
        fragBuilder->codeAppendf("kernelCoord.x += %f;", kernelStride);
    } else {
        fragBuilder->codeAppendf("sourceOffset = half2(%d, %d);", loc.x(), loc.y());
        int offset = loc.y() * kernelWidth + loc.x();
        static constexpr const char kVecSuffix[][4] = { ".x", ".y", ".z", ".w" };
        const char* kernel = uniformHandler->getUniformCStr(fKernelUni);
        fragBuilder->codeAppendf("k = %s[%d]%s;", kernel, offset / 4,
                                 kVecSuffix[offset & 0x3]);
    }

    auto sample = this->invokeChild(0, args, "coord + sourceOffset");
    fragBuilder->codeAppendf("half4 c = %s;", sample.c_str());
    if (!mce.convolveAlpha()) {
        fragBuilder->codeAppend("c.rgb /= max(c.a, 0.0001);");
        fragBuilder->codeAppend("c.rgb = saturate(c.rgb);");
    }
    fragBuilder->codeAppend("sum += c * k;");
}

void GrGLMatrixConvolutionEffect::emitCode(EmitArgs& args) {
    const GrMatrixConvolutionEffect& mce = args.fFp.cast<GrMatrixConvolutionEffect>();

    int kernelWidth = mce.kernelSize().width();
    int kernelHeight = mce.kernelSize().height();

    int arrayCount = (kernelWidth * kernelHeight + 3) / 4;
    SkASSERT(4 * arrayCount >= kernelWidth * kernelHeight);

    GrGLSLUniformHandler* uniformHandler = args.fUniformHandler;
    if (mce.kernelIsSampled()) {
        fKernelBiasUni = uniformHandler->addUniform(&mce, kFragment_GrShaderFlag,
                                                    kHalf_GrSLType, "KernelBias");
    } else {
        fKernelUni = uniformHandler->addUniformArray(&mce, kFragment_GrShaderFlag,
                                                     kHalf4_GrSLType, "Kernel", arrayCount);
    }
    fKernelOffsetUni = uniformHandler->addUniform(&mce, kFragment_GrShaderFlag, kHalf2_GrSLType,
                                                  "KernelOffset");
    fGainUni = uniformHandler->addUniform(&mce, kFragment_GrShaderFlag, kHalf_GrSLType, "Gain");
    fBiasUni = uniformHandler->addUniform(&mce, kFragment_GrShaderFlag, kHalf_GrSLType, "Bias");

    const char* kernelOffset = uniformHandler->getUniformCStr(fKernelOffsetUni);
    const char* gain = uniformHandler->getUniformCStr(fGainUni);
    const char* bias = uniformHandler->getUniformCStr(fBiasUni);

    GrGLSLFPFragmentBuilder* fragBuilder = args.fFragBuilder;
    SkString coords2D = fragBuilder->ensureCoords2D(args.fTransformedCoords[0].fVaryingPoint,
                                                    mce.sampleMatrix());
    fragBuilder->codeAppend("half4 sum = half4(0, 0, 0, 0);");
    fragBuilder->codeAppendf("float2 coord = %s - %s;", coords2D.c_str(), kernelOffset);

    if (mce.kernelIsSampled()) {
        this->emitKernelBlock(args, {});
    } else {
        for (int x = 0; x < kernelWidth; ++x) {
            for (int y = 0; y < kernelHeight; ++y) {
                this->emitKernelBlock(args, SkIPoint::Make(x, y));
            }
        }
    }

    if (mce.convolveAlpha()) {
        fragBuilder->codeAppendf("%s = sum * %s + %s;", args.fOutputColor, gain, bias);
        fragBuilder->codeAppendf("%s.a = saturate(%s.a);", args.fOutputColor, args.fOutputColor);
        fragBuilder->codeAppendf("%s.rgb = clamp(%s.rgb, 0.0, %s.a);",
                                 args.fOutputColor, args.fOutputColor, args.fOutputColor);
    } else {
        auto sample = this->invokeChild(0, args, coords2D.c_str());
        fragBuilder->codeAppendf("half4 c = %s;", sample.c_str());
        fragBuilder->codeAppendf("%s.a = c.a;", args.fOutputColor);
        fragBuilder->codeAppendf("%s.rgb = saturate(sum.rgb * %s + %s);", args.fOutputColor, gain, bias);
        fragBuilder->codeAppendf("%s.rgb *= %s.a;", args.fOutputColor, args.fOutputColor);
    }
    fragBuilder->codeAppendf("%s *= %s;\n", args.fOutputColor, args.fInputColor);
}

void GrGLMatrixConvolutionEffect::GenKey(const GrProcessor& processor,
                                         const GrShaderCaps&, GrProcessorKeyBuilder* b) {
    const GrMatrixConvolutionEffect& m = processor.cast<GrMatrixConvolutionEffect>();
    SkASSERT(m.kernelSize().width() <= 0x7FFF && m.kernelSize().height() <= 0xFFFF);
    uint32_t key = m.kernelSize().width() << 16 | m.kernelSize().height();
    key |= m.convolveAlpha() ? 1U << 31 : 0;
    b->add32(key);
}

void GrGLMatrixConvolutionEffect::onSetData(const GrGLSLProgramDataManager& pdman,
                                            const GrFragmentProcessor& processor) {
    const GrMatrixConvolutionEffect& conv = processor.cast<GrMatrixConvolutionEffect>();
    pdman.set2fv(fKernelOffsetUni, 1, conv.kernelOffset().ptr());
    float totalGain = conv.gain();
    if (conv.kernelIsSampled()) {
        totalGain *= conv.kernelSampleGain();
        pdman.set1f(fKernelBiasUni, conv.kernelSampleBias());
    } else {
        int kernelCount = conv.kernelSize().area();
        int arrayCount = (kernelCount + 3) / 4;
        SkASSERT(4 * arrayCount >= kernelCount);
        pdman.set4fv(fKernelUni, arrayCount, conv.kernel());
    }
    pdman.set1f(fBiasUni, conv.bias());
    pdman.set1f(fGainUni, totalGain);
}

GrMatrixConvolutionEffect::GrMatrixConvolutionEffect(std::unique_ptr<GrFragmentProcessor> child,
                                                     KernelWrapper kernel,
                                                     SkScalar gain,
                                                     SkScalar bias,
                                                     const SkIPoint& kernelOffset,
                                                     bool convolveAlpha)
        // To advertise either the modulation or opaqueness optimizations we'd have to examine the
        // parameters.
        : INHERITED(kGrMatrixConvolutionEffect_ClassID, kNone_OptimizationFlags)
        , fKernel(std::move(kernel))
        , fGain(SkScalarToFloat(gain))
        , fBias(SkScalarToFloat(bias) / 255.0f)
        , fConvolveAlpha(convolveAlpha) {
    child->setSampledWithExplicitCoords();
    this->registerChildProcessor(std::move(child));
    if (fKernel.isSampled()) {
        this->setTextureSamplerCnt(1);
    }
    fKernelOffset = {static_cast<float>(kernelOffset.x()),
                     static_cast<float>(kernelOffset.y())};
    this->addCoordTransform(&fCoordTransform);
}

GrMatrixConvolutionEffect::GrMatrixConvolutionEffect(const GrMatrixConvolutionEffect& that)
        : INHERITED(kGrMatrixConvolutionEffect_ClassID, kNone_OptimizationFlags)
        , fKernel(that.fKernel)
        , fGain(that.fGain)
        , fBias(that.fBias)
        , fKernelOffset(that.fKernelOffset)
        , fConvolveAlpha(that.fConvolveAlpha) {
    auto child = that.childProcessor(0).clone();
    child->setSampledWithExplicitCoords();
    this->registerChildProcessor(std::move(child));
    if (fKernel.isSampled()) {
        this->setTextureSamplerCnt(1);
    }
    this->addCoordTransform(&fCoordTransform);
}

std::unique_ptr<GrFragmentProcessor> GrMatrixConvolutionEffect::clone() const {
    return std::unique_ptr<GrFragmentProcessor>(new GrMatrixConvolutionEffect(*this));
}

void GrMatrixConvolutionEffect::onGetGLSLProcessorKey(const GrShaderCaps& caps,
                                                      GrProcessorKeyBuilder* b) const {
    GrGLMatrixConvolutionEffect::GenKey(*this, caps, b);
}

GrGLSLFragmentProcessor* GrMatrixConvolutionEffect::onCreateGLSLInstance() const  {
    return new GrGLMatrixConvolutionEffect;
}

bool GrMatrixConvolutionEffect::onIsEqual(const GrFragmentProcessor& sBase) const {
    const GrMatrixConvolutionEffect& s = sBase.cast<GrMatrixConvolutionEffect>();
    return fKernel == s.fKernel &&
           fGain == s.gain() &&
           fBias == s.bias() &&
           fKernelOffset == s.kernelOffset() &&
           fConvolveAlpha == s.convolveAlpha();
}

const GrFragmentProcessor::TextureSampler& GrMatrixConvolutionEffect::onTextureSampler(
        int index) const {
    return IthTextureSampler(index, fKernel.scalableSampler().fSampler);
}

static void fill_in_1D_gaussian_kernel_with_stride(float* kernel, int size, int stride,
                                                   float twoSigmaSqrd) {
    SkASSERT(!SkScalarNearlyZero(twoSigmaSqrd, SK_ScalarNearlyZero));

    const float sigmaDenom = 1.0f / twoSigmaSqrd;
    const int radius = size / 2;

    float sum = 0.0f;
    for (int i = 0; i < size; ++i) {
        float term = static_cast<float>(i - radius);
        // Note that the constant term (1/(sqrt(2*pi*sigma^2)) of the Gaussian
        // is dropped here, since we renormalize the kernel below.
        kernel[i * stride] = sk_float_exp(-term * term * sigmaDenom);
        sum += kernel[i * stride];
    }
    // Normalize the kernel
    float scale = 1.0f / sum;
    for (int i = 0; i < size; ++i) {
        kernel[i * stride] *= scale;
    }
}

static void fill_in_2D_gaussian_kernel(float* kernel, int width, int height,
                                       SkScalar sigmaX, SkScalar sigmaY) {
    const float twoSigmaSqrdX = 2.0f * SkScalarToFloat(SkScalarSquare(sigmaX));
    const float twoSigmaSqrdY = 2.0f * SkScalarToFloat(SkScalarSquare(sigmaY));

    // TODO: in all of these degenerate cases we're uploading (and using) a whole lot of zeros.
    if (SkScalarNearlyZero(twoSigmaSqrdX, SK_ScalarNearlyZero) ||
        SkScalarNearlyZero(twoSigmaSqrdY, SK_ScalarNearlyZero)) {
        // In this case the 2D Gaussian degenerates to a 1D Gaussian (in X or Y) or a point
        SkASSERT(3 == width || 3 == height);
        std::fill_n(kernel, width*height, 0);

        if (SkScalarNearlyZero(twoSigmaSqrdX, SK_ScalarNearlyZero) &&
            SkScalarNearlyZero(twoSigmaSqrdY, SK_ScalarNearlyZero)) {
            // A point
            SkASSERT(3 == width && 3 == height);
            kernel[4] = 1.0f;
        } else if (SkScalarNearlyZero(twoSigmaSqrdX, SK_ScalarNearlyZero)) {
            // A 1D Gaussian in Y
            SkASSERT(3 == width);
            // Down the middle column of the kernel with a stride of width
            fill_in_1D_gaussian_kernel_with_stride(&kernel[1], height, width, twoSigmaSqrdY);
        } else {
            // A 1D Gaussian in X
            SkASSERT(SkScalarNearlyZero(twoSigmaSqrdY, SK_ScalarNearlyZero));
            SkASSERT(3 == height);
            // Down the middle row of the kernel with a stride of 1
            fill_in_1D_gaussian_kernel_with_stride(&kernel[width], width, 1, twoSigmaSqrdX);
        }
        return;
    }

    const float sigmaXDenom = 1.0f / twoSigmaSqrdX;
    const float sigmaYDenom = 1.0f / twoSigmaSqrdY;
    const int xRadius = width / 2;
    const int yRadius = height / 2;

    float sum = 0.0f;
    for (int x = 0; x < width; x++) {
        float xTerm = static_cast<float>(x - xRadius);
        xTerm = xTerm * xTerm * sigmaXDenom;
        for (int y = 0; y < height; y++) {
            float yTerm = static_cast<float>(y - yRadius);
            float xyTerm = sk_float_exp(-(xTerm + yTerm * yTerm * sigmaYDenom));
            // Note that the constant term (1/(sqrt(2*pi*sigma^2)) of the Gaussian
            // is dropped here, since we renormalize the kernel below.
            kernel[y * width + x] = xyTerm;
            sum += xyTerm;
        }
    }
    // Normalize the kernel
    float scale = 1.0f / sum;
    for (int i = 0; i < width * height; ++i) {
        kernel[i] *= scale;
    }
}

std::unique_ptr<GrFragmentProcessor> GrMatrixConvolutionEffect::Make(GrRecordingContext* context,
                                                                     GrSurfaceProxyView srcView,
                                                                     const SkIRect& srcBounds,
                                                                     const SkISize& kernelSize,
                                                                     const SkScalar* kernel,
                                                                     SkScalar gain,
                                                                     SkScalar bias,
                                                                     const SkIPoint& kernelOffset,
                                                                     GrSamplerState::WrapMode wm,
                                                                     bool convolveAlpha,
                                                                     const GrCaps& caps) {
    auto kw = KernelWrapper::Make(context, kernelSize, caps, kernel);
    if (!kw.isValid()) {
        return nullptr;
    }
    GrSamplerState sampler(wm, GrSamplerState::Filter::kNearest);
    auto child = GrTextureEffect::MakeSubset(std::move(srcView), kPremul_SkAlphaType, SkMatrix::I(),
                                             sampler, SkRect::Make(srcBounds), caps);
    return std::unique_ptr<GrFragmentProcessor>(new GrMatrixConvolutionEffect(
            std::move(child), std::move(kw), gain, bias, kernelOffset, convolveAlpha));
}

std::unique_ptr<GrFragmentProcessor> GrMatrixConvolutionEffect::MakeGaussian(
        GrRecordingContext* context,
        GrSurfaceProxyView srcView,
        const SkIRect& srcBounds,
        const SkISize& kernelSize,
        SkScalar gain,
        SkScalar bias,
        const SkIPoint& kernelOffset,
        GrSamplerState::WrapMode wm,
        bool convolveAlpha,
        SkScalar sigmaX,
        SkScalar sigmaY,
        const GrCaps& caps) {
    SkAutoSTMalloc<32, float> kernel(kernelSize.area());
    fill_in_2D_gaussian_kernel(kernel.get(), kernelSize.width(), kernelSize.height(),
                               sigmaX, sigmaY);
    return Make(context, std::move(srcView), srcBounds, kernelSize, kernel.get(),
                gain, bias, kernelOffset, wm, convolveAlpha, caps);
}

GR_DEFINE_FRAGMENT_PROCESSOR_TEST(GrMatrixConvolutionEffect);

#if GR_TEST_UTILS
std::unique_ptr<GrFragmentProcessor> GrMatrixConvolutionEffect::TestCreate(GrProcessorTestData* d) {
    auto [view, ct, at] = d->randomView();

    static constexpr size_t kMaxTestKernelSize = 2 * kMaxUniformSize;
    int width = d->fRandom->nextRangeU(1, kMaxTestKernelSize);
    int height = d->fRandom->nextRangeU(1, kMaxTestKernelSize / width);
    SkISize kernelSize = SkISize::Make(width, height);
    std::unique_ptr<SkScalar[]> kernel(new SkScalar[width * height]);
    for (int i = 0; i < width * height; i++) {
        kernel.get()[i] = d->fRandom->nextSScalar1();
    }
    SkScalar gain = d->fRandom->nextSScalar1();
    SkScalar bias = d->fRandom->nextSScalar1();

    uint32_t kernalOffsetX = d->fRandom->nextRangeU(0, kernelSize.width());
    uint32_t kernalOffsetY = d->fRandom->nextRangeU(0, kernelSize.height());
    SkIPoint kernelOffset = SkIPoint::Make(kernalOffsetX, kernalOffsetY);

    uint32_t boundsX = d->fRandom->nextRangeU(0, view.width());
    uint32_t boundsY = d->fRandom->nextRangeU(0, view.height());
    uint32_t boundsW = d->fRandom->nextRangeU(0, view.width());
    uint32_t boundsH = d->fRandom->nextRangeU(0, view.height());
    SkIRect bounds = SkIRect::MakeXYWH(boundsX, boundsY, boundsW, boundsH);

    auto wm = static_cast<GrSamplerState::WrapMode>(
            d->fRandom->nextULessThan(GrSamplerState::kWrapModeCount));
    bool convolveAlpha = d->fRandom->nextBool();
    return GrMatrixConvolutionEffect::Make(d->context()->priv().asRecordingContext(),
                                           std::move(view),
                                           bounds,
                                           kernelSize,
                                           kernel.get(),
                                           gain,
                                           bias,
                                           kernelOffset,
                                           wm,
                                           convolveAlpha,
                                           *d->caps());
}
#endif
