/*
 * Copyright 2013 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "include/effects/SkDisplacementMapEffect.h"

#include "include/core/SkBitmap.h"
#include "include/core/SkUnPreMultiply.h"
#include "include/private/SkColorData.h"
#include "src/core/SkImageFilter_Base.h"
#include "src/core/SkReadBuffer.h"
#include "src/core/SkSpecialImage.h"
#include "src/core/SkWriteBuffer.h"
#if SK_SUPPORT_GPU
#include "include/private/GrRecordingContext.h"
#include "src/gpu/GrCaps.h"
#include "src/gpu/GrClip.h"
#include "src/gpu/GrColorSpaceXform.h"
#include "src/gpu/GrCoordTransform.h"
#include "src/gpu/GrRecordingContextPriv.h"
#include "src/gpu/GrRenderTargetContext.h"
#include "src/gpu/GrTexture.h"
#include "src/gpu/GrTextureProxy.h"
#include "src/gpu/SkGr.h"
#include "src/gpu/glsl/GrGLSLFragmentProcessor.h"
#include "src/gpu/glsl/GrGLSLFragmentShaderBuilder.h"
#include "src/gpu/glsl/GrGLSLProgramDataManager.h"
#include "src/gpu/glsl/GrGLSLUniformHandler.h"
#endif

namespace {

class SkDisplacementMapEffectImpl final : public SkImageFilter_Base {
public:
    SkDisplacementMapEffectImpl(SkColorChannel xChannelSelector, SkColorChannel yChannelSelector,
                                SkScalar scale, sk_sp<SkImageFilter> inputs[2],
                                const CropRect* cropRect)
            : INHERITED(inputs, 2, cropRect)
            , fXChannelSelector(xChannelSelector)
            , fYChannelSelector(yChannelSelector)
            , fScale(scale) {}

    SkRect computeFastBounds(const SkRect& src) const override;

    virtual SkIRect onFilterBounds(const SkIRect& src, const SkMatrix& ctm,
                                   MapDirection, const SkIRect* inputRect) const override;
    SkIRect onFilterNodeBounds(const SkIRect&, const SkMatrix& ctm,
                               MapDirection, const SkIRect* inputRect) const override;

protected:
    sk_sp<SkSpecialImage> onFilterImage(const Context&, SkIPoint* offset) const override;

    void flatten(SkWriteBuffer&) const override;

private:
    friend void SkDisplacementMapEffect::RegisterFlattenables();
    SK_FLATTENABLE_HOOKS(SkDisplacementMapEffectImpl)

    SkColorChannel fXChannelSelector;
    SkColorChannel fYChannelSelector;
    SkScalar fScale;

    const SkImageFilter* getDisplacementInput() const { return getInput(0); }
    const SkImageFilter* getColorInput() const { return getInput(1); }

    typedef SkImageFilter_Base INHERITED;
};

// Shift values to extract channels from an SkColor (SkColorGetR, SkColorGetG, etc)
const uint8_t gChannelTypeToShift[] = {
    16,  // R
     8,  // G
     0,  // B
    24,  // A
};
struct Extractor {
    Extractor(SkColorChannel typeX,
              SkColorChannel typeY)
        : fShiftX(gChannelTypeToShift[static_cast<int>(typeX)])
        , fShiftY(gChannelTypeToShift[static_cast<int>(typeY)])
    {}

    unsigned fShiftX, fShiftY;

    unsigned getX(SkColor c) const { return (c >> fShiftX) & 0xFF; }
    unsigned getY(SkColor c) const { return (c >> fShiftY) & 0xFF; }
};

static bool channel_selector_type_is_valid(SkColorChannel cst) {
    switch (cst) {
        case SkColorChannel::kR:
        case SkColorChannel::kG:
        case SkColorChannel::kB:
        case SkColorChannel::kA:
            return true;
        default:
            break;
    }
    return false;
}

static SkColorChannel convert_channel_type(SkDisplacementMapEffect::ChannelSelectorType c) {
    switch(c) {
        case SkDisplacementMapEffect::kR_ChannelSelectorType:
            return SkColorChannel::kR;
        case SkDisplacementMapEffect::kG_ChannelSelectorType:
            return SkColorChannel::kG;
        case SkDisplacementMapEffect::kB_ChannelSelectorType:
            return SkColorChannel::kB;
        case SkDisplacementMapEffect::kA_ChannelSelectorType:
            return SkColorChannel::kA;
        case SkDisplacementMapEffect::kUnknown_ChannelSelectorType:
        default:
            // Raster backend historically treated this as B, GPU backend would fail when generating
            // shader code. Just return B without aborting in debug-builds in order to keep fuzzers
            // happy when they pass in the technically still valid kUnknown_ChannelSelectorType.
            return SkColorChannel::kB;
    }
}

}  // anonymous namespace

///////////////////////////////////////////////////////////////////////////////

sk_sp<SkImageFilter> SkDisplacementMapEffect::Make(ChannelSelectorType xChannelSelector,
                                                   ChannelSelectorType yChannelSelector,
                                                   SkScalar scale,
                                                   sk_sp<SkImageFilter> displacement,
                                                   sk_sp<SkImageFilter> color,
                                                   const SkImageFilter::CropRect* cropRect) {
    return Make(convert_channel_type(xChannelSelector), convert_channel_type(yChannelSelector),
                scale, std::move(displacement), std::move(color), cropRect);
}

sk_sp<SkImageFilter> SkDisplacementMapEffect::Make(SkColorChannel xChannelSelector,
                                                   SkColorChannel yChannelSelector,
                                                   SkScalar scale,
                                                   sk_sp<SkImageFilter> displacement,
                                                   sk_sp<SkImageFilter> color,
                                                   const SkImageFilter::CropRect* cropRect) {
    if (!channel_selector_type_is_valid(xChannelSelector) ||
        !channel_selector_type_is_valid(yChannelSelector)) {
        return nullptr;
    }

    sk_sp<SkImageFilter> inputs[2] = { std::move(displacement), std::move(color) };
    return sk_sp<SkImageFilter>(new SkDisplacementMapEffectImpl(xChannelSelector, yChannelSelector,
                                                                scale, inputs, cropRect));
}

void SkDisplacementMapEffect::RegisterFlattenables() {
    SK_REGISTER_FLATTENABLE(SkDisplacementMapEffectImpl);
    // TODO (michaelludwig) - Remove after grace period for SKPs to stop using old name
    SkFlattenable::Register("SkDisplacementMapEffect", SkDisplacementMapEffectImpl::CreateProc);
}

///////////////////////////////////////////////////////////////////////////////

sk_sp<SkFlattenable> SkDisplacementMapEffectImpl::CreateProc(SkReadBuffer& buffer) {
    SK_IMAGEFILTER_UNFLATTEN_COMMON(common, 2);

    SkColorChannel xsel, ysel;
    if (buffer.isVersionLT(SkPicturePriv::kCleanupImageFilterEnums_Version)) {
        xsel = convert_channel_type(buffer.read32LE(
                SkDisplacementMapEffect::kLast_ChannelSelectorType));
        ysel = convert_channel_type(buffer.read32LE(
                SkDisplacementMapEffect::kLast_ChannelSelectorType));
    } else {
        xsel = buffer.read32LE(SkColorChannel::kLastEnum);
        ysel = buffer.read32LE(SkColorChannel::kLastEnum);
    }

    SkScalar scale = buffer.readScalar();

    return SkDisplacementMapEffect::Make(xsel, ysel, scale, common.getInput(0), common.getInput(1),
                                         &common.cropRect());
}

void SkDisplacementMapEffectImpl::flatten(SkWriteBuffer& buffer) const {
    this->INHERITED::flatten(buffer);
    buffer.writeInt((int) fXChannelSelector);
    buffer.writeInt((int) fYChannelSelector);
    buffer.writeScalar(fScale);
}

#if SK_SUPPORT_GPU

namespace {

class GrDisplacementMapEffect : public GrFragmentProcessor {
public:
    static std::unique_ptr<GrFragmentProcessor> Make(SkColorChannel xChannelSelector,
                                                     SkColorChannel yChannelSelector,
                                                     SkVector scale,
                                                     GrSurfaceProxyView displacement,
                                                     const SkIRect& displSubset,
                                                     const SkMatrix& offsetMatrix,
                                                     GrSurfaceProxyView color,
                                                     const SkIRect& colorSubset,
                                                     const GrCaps&);

    ~GrDisplacementMapEffect() override;

    SkColorChannel xChannelSelector() const { return fXChannelSelector; }
    SkColorChannel yChannelSelector() const { return fYChannelSelector; }
    const SkVector& scale() const { return fScale; }

    const char* name() const override { return "DisplacementMap"; }

    std::unique_ptr<GrFragmentProcessor> clone() const override;

private:
    class Impl;

    explicit GrDisplacementMapEffect(const GrDisplacementMapEffect&);

    GrGLSLFragmentProcessor* onCreateGLSLInstance() const override;

    void onGetGLSLProcessorKey(const GrShaderCaps& caps, GrProcessorKeyBuilder* b) const override;

    bool onIsEqual(const GrFragmentProcessor&) const override;

    GrDisplacementMapEffect(SkColorChannel xChannelSelector,
                            SkColorChannel yChannelSelector,
                            const SkVector& scale,
                            std::unique_ptr<GrFragmentProcessor> displacement,
                            std::unique_ptr<GrFragmentProcessor> color);

    GR_DECLARE_FRAGMENT_PROCESSOR_TEST

    // We really just want the unaltered local coords, but the only way to get that right now is
    // an identity coord transform.
    GrCoordTransform fCoordTransform = {};
    SkColorChannel fXChannelSelector;
    SkColorChannel fYChannelSelector;
    SkVector fScale;

    typedef GrFragmentProcessor INHERITED;
};

}  // anonymous namespace
#endif

static void compute_displacement(Extractor ex, const SkVector& scale, SkBitmap* dst,
                                 const SkBitmap& displ, const SkIPoint& offset,
                                 const SkBitmap& src,
                                 const SkIRect& bounds) {
    static const SkScalar Inv8bit = SkScalarInvert(255);
    const int srcW = src.width();
    const int srcH = src.height();
    const SkVector scaleForColor = SkVector::Make(scale.fX * Inv8bit, scale.fY * Inv8bit);
    const SkVector scaleAdj = SkVector::Make(SK_ScalarHalf - scale.fX * SK_ScalarHalf,
                                             SK_ScalarHalf - scale.fY * SK_ScalarHalf);
    SkPMColor* dstPtr = dst->getAddr32(0, 0);
    for (int y = bounds.top(); y < bounds.bottom(); ++y) {
        const SkPMColor* displPtr = displ.getAddr32(bounds.left() + offset.fX, y + offset.fY);
        for (int x = bounds.left(); x < bounds.right(); ++x, ++displPtr) {
            SkColor c = SkUnPreMultiply::PMColorToColor(*displPtr);

            SkScalar displX = scaleForColor.fX * ex.getX(c) + scaleAdj.fX;
            SkScalar displY = scaleForColor.fY * ex.getY(c) + scaleAdj.fY;
            // Truncate the displacement values
            const int32_t srcX = Sk32_sat_add(x, SkScalarTruncToInt(displX));
            const int32_t srcY = Sk32_sat_add(y, SkScalarTruncToInt(displY));
            *dstPtr++ = ((srcX < 0) || (srcX >= srcW) || (srcY < 0) || (srcY >= srcH)) ?
                      0 : *(src.getAddr32(srcX, srcY));
        }
    }
}

sk_sp<SkSpecialImage> SkDisplacementMapEffectImpl::onFilterImage(const Context& ctx,
                                                                 SkIPoint* offset) const {
    SkIPoint colorOffset = SkIPoint::Make(0, 0);
    sk_sp<SkSpecialImage> color(this->filterInput(1, ctx, &colorOffset));
    if (!color) {
        return nullptr;
    }

    SkIPoint displOffset = SkIPoint::Make(0, 0);
    // Creation of the displacement map should happen in a non-colorspace aware context. This
    // texture is a purely mathematical construct, so we want to just operate on the stored
    // values. Consider:
    // User supplies an sRGB displacement map. If we're rendering to a wider gamut, then we could
    // end up filtering the displacement map into that gamut, which has the effect of reducing
    // the amount of displacement that it represents (as encoded values move away from the
    // primaries).
    // With a more complex DAG attached to this input, it's not clear that working in ANY specific
    // color space makes sense, so we ignore color spaces (and gamma) entirely. This may not be
    // ideal, but it's at least consistent and predictable.
    Context displContext(ctx.mapping(), ctx.desiredOutput(), ctx.cache(),
                         kN32_SkColorType, nullptr, ctx.source());
    sk_sp<SkSpecialImage> displ(this->filterInput(0, displContext, &displOffset));
    if (!displ) {
        return nullptr;
    }

    const SkIRect srcBounds = SkIRect::MakeXYWH(colorOffset.x(), colorOffset.y(),
                                                color->width(), color->height());

    // Both paths do bounds checking on color pixel access, we don't need to
    // pad the color bitmap to bounds here.
    SkIRect bounds;
    if (!this->applyCropRect(ctx, srcBounds, &bounds)) {
        return nullptr;
    }

    SkIRect displBounds;
    displ = this->applyCropRectAndPad(ctx, displ.get(), &displOffset, &displBounds);
    if (!displ) {
        return nullptr;
    }

    if (!bounds.intersect(displBounds)) {
        return nullptr;
    }

    const SkIRect colorBounds = bounds.makeOffset(-colorOffset);
    // If the offset overflowed (saturated) then we have to abort, as we need their
    // dimensions to be equal. See https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=7209
    if (colorBounds.size() != bounds.size()) {
        return nullptr;
    }

    SkVector scale = SkVector::Make(fScale, fScale);
    ctx.ctm().mapVectors(&scale, 1);

#if SK_SUPPORT_GPU
    if (ctx.gpuBacked()) {
        auto context = ctx.getContext();

        GrSurfaceProxyView colorView = color->view(context);
        GrSurfaceProxyView displView = displ->view(context);
        if (!colorView.proxy() || !displView.proxy()) {
            return nullptr;
        }
        const auto isProtected = colorView.proxy()->isProtected();

        SkMatrix offsetMatrix = SkMatrix::MakeTrans(SkIntToScalar(colorOffset.fX - displOffset.fX),
                                                    SkIntToScalar(colorOffset.fY - displOffset.fY));

        std::unique_ptr<GrFragmentProcessor> fp =
                GrDisplacementMapEffect::Make(fXChannelSelector,
                                              fYChannelSelector,
                                              scale,
                                              std::move(displView),
                                              displ->subset(),
                                              offsetMatrix,
                                              std::move(colorView),
                                              color->subset(),
                                              *context->priv().caps());
        fp = GrColorSpaceXformEffect::Make(std::move(fp), color->getColorSpace(),
                                           color->alphaType(), ctx.colorSpace());

        GrPaint paint;
        paint.addColorFragmentProcessor(std::move(fp));
        paint.setPorterDuffXPFactory(SkBlendMode::kSrc);
        SkMatrix matrix;
        matrix.setTranslate(-SkIntToScalar(colorBounds.x()), -SkIntToScalar(colorBounds.y()));

        auto renderTargetContext = GrRenderTargetContext::Make(
                context, ctx.grColorType(), ctx.refColorSpace(), SkBackingFit::kApprox,
                bounds.size(), 1, GrMipMapped::kNo, isProtected, kBottomLeft_GrSurfaceOrigin);
        if (!renderTargetContext) {
            return nullptr;
        }

        renderTargetContext->drawRect(GrNoClip(), std::move(paint), GrAA::kNo, matrix,
                                      SkRect::Make(colorBounds));

        offset->fX = bounds.left();
        offset->fY = bounds.top();
        return SkSpecialImage::MakeDeferredFromGpu(
                context,
                SkIRect::MakeWH(bounds.width(), bounds.height()),
                kNeedNewImageUniqueID_SpecialImage,
                renderTargetContext->readSurfaceView(),
                renderTargetContext->colorInfo().colorType(),
                renderTargetContext->colorInfo().refColorSpace());
    }
#endif

    SkBitmap colorBM, displBM;

    if (!color->getROPixels(&colorBM) || !displ->getROPixels(&displBM)) {
        return nullptr;
    }

    if ((colorBM.colorType() != kN32_SkColorType) ||
        (displBM.colorType() != kN32_SkColorType)) {
        return nullptr;
    }

    if (!colorBM.getPixels() || !displBM.getPixels()) {
        return nullptr;
    }

    SkImageInfo info = SkImageInfo::MakeN32(bounds.width(), bounds.height(),
                                            colorBM.alphaType());

    SkBitmap dst;
    if (!dst.tryAllocPixels(info)) {
        return nullptr;
    }

    compute_displacement(Extractor(fXChannelSelector, fYChannelSelector), scale, &dst,
                         displBM, colorOffset - displOffset, colorBM, colorBounds);

    offset->fX = bounds.left();
    offset->fY = bounds.top();
    return SkSpecialImage::MakeFromRaster(SkIRect::MakeWH(bounds.width(), bounds.height()),
                                          dst);
}

SkRect SkDisplacementMapEffectImpl::computeFastBounds(const SkRect& src) const {
    SkRect bounds = this->getColorInput() ? this->getColorInput()->computeFastBounds(src) : src;
    bounds.outset(SkScalarAbs(fScale) * SK_ScalarHalf, SkScalarAbs(fScale) * SK_ScalarHalf);
    return bounds;
}

SkIRect SkDisplacementMapEffectImpl::onFilterNodeBounds(
        const SkIRect& src, const SkMatrix& ctm, MapDirection, const SkIRect* inputRect) const {
    SkVector scale = SkVector::Make(fScale, fScale);
    ctm.mapVectors(&scale, 1);
    return src.makeOutset(SkScalarCeilToInt(SkScalarAbs(scale.fX) * SK_ScalarHalf),
                          SkScalarCeilToInt(SkScalarAbs(scale.fY) * SK_ScalarHalf));
}

SkIRect SkDisplacementMapEffectImpl::onFilterBounds(
        const SkIRect& src, const SkMatrix& ctm, MapDirection dir, const SkIRect* inputRect) const {
    // Recurse only into color input.
    if (this->getColorInput()) {
        return this->getColorInput()->filterBounds(src, ctm, dir, inputRect);
    }
    return src;
}

///////////////////////////////////////////////////////////////////////////////

#if SK_SUPPORT_GPU
class GrDisplacementMapEffect::Impl : public GrGLSLFragmentProcessor {
public:
    void emitCode(EmitArgs&) override;

    static inline void GenKey(const GrProcessor&, const GrShaderCaps&, GrProcessorKeyBuilder*);

protected:
    void onSetData(const GrGLSLProgramDataManager&, const GrFragmentProcessor&) override;

private:
    typedef GrGLSLProgramDataManager::UniformHandle UniformHandle;

    UniformHandle fScaleUni;

    typedef GrGLSLFragmentProcessor INHERITED;
};

///////////////////////////////////////////////////////////////////////////////

std::unique_ptr<GrFragmentProcessor> GrDisplacementMapEffect::Make(SkColorChannel xChannelSelector,
                                                                   SkColorChannel yChannelSelector,
                                                                   SkVector scale,
                                                                   GrSurfaceProxyView displacement,
                                                                   const SkIRect& displSubset,
                                                                   const SkMatrix& offsetMatrix,
                                                                   GrSurfaceProxyView color,
                                                                   const SkIRect& colorSubset,
                                                                   const GrCaps& caps) {
    static constexpr GrSamplerState kColorSampler(GrSamplerState::WrapMode::kClampToBorder,
                                                  GrSamplerState::Filter::kNearest);
    auto colorEffect = GrTextureEffect::MakeSubset(std::move(color),
                                                   kPremul_SkAlphaType,
                                                   SkMatrix::MakeTrans(colorSubset.topLeft()),
                                                   kColorSampler,
                                                   SkRect::Make(colorSubset),
                                                   caps);

    auto dispM = SkMatrix::Concat(SkMatrix::MakeTrans(displSubset.topLeft()), offsetMatrix);
    auto dispEffect = GrTextureEffect::Make(std::move(displacement),
                                            kPremul_SkAlphaType,
                                            dispM,
                                            GrSamplerState::Filter::kNearest);

    return std::unique_ptr<GrFragmentProcessor>(
            new GrDisplacementMapEffect(xChannelSelector,
                                        yChannelSelector,
                                        scale,
                                        std::move(dispEffect),
                                        std::move(colorEffect)));
}

GrGLSLFragmentProcessor* GrDisplacementMapEffect::onCreateGLSLInstance() const {
    return new Impl();
}

void GrDisplacementMapEffect::onGetGLSLProcessorKey(const GrShaderCaps& caps,
                                                    GrProcessorKeyBuilder* b) const {
    Impl::GenKey(*this, caps, b);
}

GrDisplacementMapEffect::GrDisplacementMapEffect(SkColorChannel xChannelSelector,
                                                 SkColorChannel yChannelSelector,
                                                 const SkVector& scale,
                                                 std::unique_ptr<GrFragmentProcessor> displacement,
                                                 std::unique_ptr<GrFragmentProcessor> color)
        : INHERITED(kGrDisplacementMapEffect_ClassID, GrFragmentProcessor::kNone_OptimizationFlags)
        , fXChannelSelector(xChannelSelector)
        , fYChannelSelector(yChannelSelector)
        , fScale(scale) {
    this->registerChildProcessor(std::move(displacement));
    color->setSampledWithExplicitCoords();
    this->registerChildProcessor(std::move(color));
    this->addCoordTransform(&fCoordTransform);
}

GrDisplacementMapEffect::GrDisplacementMapEffect(const GrDisplacementMapEffect& that)
        : INHERITED(kGrDisplacementMapEffect_ClassID, that.optimizationFlags())
        , fXChannelSelector(that.fXChannelSelector)
        , fYChannelSelector(that.fYChannelSelector)
        , fScale(that.fScale) {
    auto displacement = that.childProcessor(0).clone();
    if (that.childProcessor(0).isSampledWithExplicitCoords()) {
        displacement->setSampledWithExplicitCoords();
    }
    this->registerChildProcessor(std::move(displacement));

    auto color = that.childProcessor(1).clone();
    color->setSampledWithExplicitCoords();
    this->registerChildProcessor(std::move(color));
    this->addCoordTransform(&fCoordTransform);
}

GrDisplacementMapEffect::~GrDisplacementMapEffect() {}

std::unique_ptr<GrFragmentProcessor> GrDisplacementMapEffect::clone() const {
    return std::unique_ptr<GrFragmentProcessor>(new GrDisplacementMapEffect(*this));
}

bool GrDisplacementMapEffect::onIsEqual(const GrFragmentProcessor& sBase) const {
    const GrDisplacementMapEffect& s = sBase.cast<GrDisplacementMapEffect>();
    return fXChannelSelector == s.fXChannelSelector &&
           fYChannelSelector == s.fYChannelSelector &&
           fScale == s.fScale;
}

///////////////////////////////////////////////////////////////////////////////

GR_DEFINE_FRAGMENT_PROCESSOR_TEST(GrDisplacementMapEffect);

#if GR_TEST_UTILS
std::unique_ptr<GrFragmentProcessor> GrDisplacementMapEffect::TestCreate(GrProcessorTestData* d) {
    auto [dispView,  ct1, at1] = d->randomView();
    auto [colorView, ct2, at2] = d->randomView();
    static const int kMaxComponent = static_cast<int>(SkColorChannel::kLastEnum);
    SkColorChannel xChannelSelector =
        static_cast<SkColorChannel>(d->fRandom->nextRangeU(1, kMaxComponent));
    SkColorChannel yChannelSelector =
        static_cast<SkColorChannel>(d->fRandom->nextRangeU(1, kMaxComponent));
    SkVector scale = SkVector::Make(d->fRandom->nextRangeScalar(0, 100.0f),
                                    d->fRandom->nextRangeScalar(0, 100.0f));
    SkISize colorDimensions;
    colorDimensions.fWidth = d->fRandom->nextRangeU(0, colorView.width());
    colorDimensions.fHeight = d->fRandom->nextRangeU(0, colorView.height());
    SkIRect dispRect = SkIRect::MakeSize(dispView.dimensions());

    return GrDisplacementMapEffect::Make(xChannelSelector,
                                         yChannelSelector,
                                         scale,
                                         std::move(dispView),
                                         dispRect,
                                         SkMatrix::I(),
                                         std::move(colorView),
                                         SkIRect::MakeSize(colorDimensions),
                                         *d->caps());
}

#endif

///////////////////////////////////////////////////////////////////////////////

void GrDisplacementMapEffect::Impl::emitCode(EmitArgs& args) {
    const GrDisplacementMapEffect& displacementMap = args.fFp.cast<GrDisplacementMapEffect>();

    fScaleUni = args.fUniformHandler->addUniform(&displacementMap, kFragment_GrShaderFlag,
                                                 kHalf2_GrSLType, "Scale");
    const char* scaleUni = args.fUniformHandler->getUniformCStr(fScaleUni);
    static constexpr const char* dColor = "dColor";
    static constexpr const char* cCoords = "cCoords";
    static constexpr const char* nearZero = "1e-6";  // Since 6.10352e-5 is the smallest half float,
                                                     // use a number smaller than that to
                                                     // approximate 0, but leave room for 32-bit
                                                     // float GPU rounding errors.

    GrGLSLFPFragmentBuilder* fragBuilder = args.fFragBuilder;
    auto displacementSample = this->invokeChild(0, args);
    fragBuilder->codeAppendf("half4 %s = %s;", dColor, displacementSample.c_str());

    // Unpremultiply the displacement
    fragBuilder->codeAppendf("%s.rgb = (%s.a < %s) ? half3(0.0) : saturate(%s.rgb / %s.a);",
                             dColor, dColor, nearZero, dColor, dColor);
    SkString coords2D = fragBuilder->ensureCoords2D(args.fTransformedCoords[0].fVaryingPoint,
                                                    args.fFp.sampleMatrix());
    auto chanChar = [](SkColorChannel c) {
        switch(c) {
            case SkColorChannel::kR: return 'r';
            case SkColorChannel::kG: return 'g';
            case SkColorChannel::kB: return 'b';
            case SkColorChannel::kA: return 'a';
            default: SkUNREACHABLE;
        }
    };
    fragBuilder->codeAppendf("float2 %s = %s + %s*(%s.%c%c - half2(0.5));",
                             cCoords, coords2D.c_str(), scaleUni, dColor,
                             chanChar(displacementMap.xChannelSelector()),
                             chanChar(displacementMap.yChannelSelector()));

    auto colorSample = this->invokeChild(1, args, cCoords);

    fragBuilder->codeAppendf("%s = %s;", args.fOutputColor, colorSample.c_str());
}

void GrDisplacementMapEffect::Impl::onSetData(const GrGLSLProgramDataManager& pdman,
                                              const GrFragmentProcessor& proc) {
    const auto& displacementMap = proc.cast<GrDisplacementMapEffect>();
    const SkVector& scale = displacementMap.scale();
    pdman.set2f(fScaleUni, scale.x(), scale.y());
}

void GrDisplacementMapEffect::Impl::GenKey(const GrProcessor& proc,
                                           const GrShaderCaps&,
                                           GrProcessorKeyBuilder* b) {
    const GrDisplacementMapEffect& displacementMap = proc.cast<GrDisplacementMapEffect>();

    static constexpr int kChannelSelectorKeyBits = 2;  // Max value is 3, so 2 bits are required

    uint32_t xKey = static_cast<uint32_t>(displacementMap.xChannelSelector());
    uint32_t yKey = static_cast<uint32_t>(displacementMap.yChannelSelector())
            << kChannelSelectorKeyBits;

    b->add32(xKey | yKey);
}
#endif
