/*
* Copyright 2015 Google Inc.
*
* Use of this source code is governed by a BSD-style license that can be
* found in the LICENSE file.
*/

#include "src/core/SkRuntimeEffectPriv.h"
#include "src/gpu/GrFragmentProcessor.h"
#include "src/gpu/GrPipeline.h"
#include "src/gpu/GrProcessorAnalysis.h"
#include "src/gpu/effects/GrBlendFragmentProcessor.h"
#include "src/gpu/effects/GrSkSLFP.h"
#include "src/gpu/effects/GrTextureEffect.h"
#include "src/gpu/glsl/GrGLSLFragmentProcessor.h"
#include "src/gpu/glsl/GrGLSLFragmentShaderBuilder.h"
#include "src/gpu/glsl/GrGLSLProgramDataManager.h"
#include "src/gpu/glsl/GrGLSLUniformHandler.h"

bool GrFragmentProcessor::isEqual(const GrFragmentProcessor& that) const {
    if (this->classID() != that.classID()) {
        return false;
    }
    if (this->usesVaryingCoordsDirectly() != that.usesVaryingCoordsDirectly()) {
        return false;
    }
    if (!this->onIsEqual(that)) {
        return false;
    }
    if (this->numChildProcessors() != that.numChildProcessors()) {
        return false;
    }
    for (int i = 0; i < this->numChildProcessors(); ++i) {
        auto thisChild = this->childProcessor(i),
             thatChild = that .childProcessor(i);
        if (SkToBool(thisChild) != SkToBool(thatChild)) {
            return false;
        }
        if (thisChild && !thisChild->isEqual(*thatChild)) {
            return false;
        }
    }
    return true;
}

void GrFragmentProcessor::visitProxies(const GrVisitProxyFunc& func) const {
    this->visitTextureEffects([&func](const GrTextureEffect& te) {
        func(te.view().proxy(), te.samplerState().mipmapped());
    });
}

void GrFragmentProcessor::visitTextureEffects(
        const std::function<void(const GrTextureEffect&)>& func) const {
    if (auto* te = this->asTextureEffect()) {
        func(*te);
    }
    for (auto& child : fChildProcessors) {
        if (child) {
            child->visitTextureEffects(func);
        }
    }
}

GrTextureEffect* GrFragmentProcessor::asTextureEffect() {
    if (this->classID() == kGrTextureEffect_ClassID) {
        return static_cast<GrTextureEffect*>(this);
    }
    return nullptr;
}

const GrTextureEffect* GrFragmentProcessor::asTextureEffect() const {
    if (this->classID() == kGrTextureEffect_ClassID) {
        return static_cast<const GrTextureEffect*>(this);
    }
    return nullptr;
}

#if GR_TEST_UTILS
static void recursive_dump_tree_info(const GrFragmentProcessor& fp,
                                     SkString indent,
                                     SkString* text) {
    for (int index = 0; index < fp.numChildProcessors(); ++index) {
        text->appendf("\n%s(#%d) -> ", indent.c_str(), index);
        if (const GrFragmentProcessor* childFP = fp.childProcessor(index)) {
            text->append(childFP->dumpInfo());
            indent.append("\t");
            recursive_dump_tree_info(*childFP, indent, text);
        } else {
            text->append("null");
        }
    }
}

SkString GrFragmentProcessor::dumpTreeInfo() const {
    SkString text = this->dumpInfo();
    recursive_dump_tree_info(*this, SkString("\t"), &text);
    text.append("\n");
    return text;
}
#endif

std::unique_ptr<GrGLSLFragmentProcessor> GrFragmentProcessor::makeProgramImpl() const {
    std::unique_ptr<GrGLSLFragmentProcessor> glFragProc = this->onMakeProgramImpl();
    glFragProc->fChildProcessors.push_back_n(fChildProcessors.count());
    for (int i = 0; i < fChildProcessors.count(); ++i) {
        glFragProc->fChildProcessors[i] = fChildProcessors[i]
                                                  ? fChildProcessors[i]->makeProgramImpl()
                                                  : nullptr;
    }
    return glFragProc;
}

void GrFragmentProcessor::addAndPushFlagToChildren(PrivateFlags flag) {
    // This propagates down, so if we've already marked it, all our children should have it too
    if (!(fFlags & flag)) {
        fFlags |= flag;
        for (auto& child : fChildProcessors) {
            if (child) {
                child->addAndPushFlagToChildren(flag);
            }
        }
    }
#ifdef SK_DEBUG
    for (auto& child : fChildProcessors) {
        SkASSERT(!child || (child->fFlags & flag));
    }
#endif
}

int GrFragmentProcessor::numNonNullChildProcessors() const {
    return std::count_if(fChildProcessors.begin(), fChildProcessors.end(),
                         [](const auto& c) { return c != nullptr; });
}

#ifdef SK_DEBUG
bool GrFragmentProcessor::isInstantiated() const {
    bool result = true;
    this->visitTextureEffects([&result](const GrTextureEffect& te) {
        if (!te.texture()) {
            result = false;
        }
    });
    return result;
}
#endif

void GrFragmentProcessor::registerChild(std::unique_ptr<GrFragmentProcessor> child,
                                        SkSL::SampleUsage sampleUsage) {
    if (!child) {
        fChildProcessors.push_back(nullptr);
        return;
    }

    // The child should not have been attached to another FP already and not had any sampling
    // strategy set on it.
    SkASSERT(!child->fParent && !child->sampleUsage().isSampled() &&
             !child->isSampledWithExplicitCoords() && !child->hasPerspectiveTransform());

    // Configure child's sampling state first
    child->fUsage = sampleUsage;

    if (sampleUsage.isExplicit()) {
        child->addAndPushFlagToChildren(kSampledWithExplicitCoords_Flag);
    }

    // Push perspective matrix type to children
    if (sampleUsage.fHasPerspective) {
        child->addAndPushFlagToChildren(kNetTransformHasPerspective_Flag);
    }

    // Propagate the "will read dest-color" flag up to parent FPs.
    if (child->willReadDstColor()) {
        this->setWillReadDstColor();
    }

    // If the child is not sampled explicitly and not already accessing sample coords directly
    // (through reference or variable matrix expansion), then mark that this FP tree relies on
    // coordinates at a lower level. If the child is sampled with explicit coordinates and
    // there isn't any other direct reference to the sample coords, we halt the upwards propagation
    // because it means this FP is determining coordinates on its own.
    if (!child->isSampledWithExplicitCoords()) {
        if ((child->fFlags & kUsesSampleCoordsDirectly_Flag ||
             child->fFlags & kUsesSampleCoordsIndirectly_Flag)) {
            fFlags |= kUsesSampleCoordsIndirectly_Flag;
        }
    }

    fRequestedFeatures |= child->fRequestedFeatures;

    // Record that the child is attached to us; this FP is the source of any uniform data needed
    // to evaluate the child sample matrix.
    child->fParent = this;
    fChildProcessors.push_back(std::move(child));

    // Validate: our sample strategy comes from a parent we shouldn't have yet.
    SkASSERT(!this->isSampledWithExplicitCoords() && !this->hasPerspectiveTransform() &&
             !fUsage.isSampled() && !fParent);
}

void GrFragmentProcessor::cloneAndRegisterAllChildProcessors(const GrFragmentProcessor& src) {
    for (int i = 0; i < src.numChildProcessors(); ++i) {
        if (auto fp = src.childProcessor(i)) {
            this->registerChild(fp->clone(), fp->sampleUsage());
        } else {
            this->registerChild(nullptr);
        }
    }
}

std::unique_ptr<GrFragmentProcessor> GrFragmentProcessor::MakeColor(SkPMColor4f color) {
    // Use ColorFilter signature/factory to get the constant output for constant input optimization
    static auto effect = SkMakeRuntimeEffect(SkRuntimeEffect::MakeForColorFilter, R"(
        uniform half4 color;
        half4 main(half4 inColor) { return color; }
    )");
    SkASSERT(SkRuntimeEffectPriv::SupportsConstantOutputForConstantInput(effect));
    return GrSkSLFP::Make(effect, "color_fp", /*inputFP=*/nullptr,
                          color.isOpaque() ? GrSkSLFP::OptFlags::kPreservesOpaqueInput
                                           : GrSkSLFP::OptFlags::kNone,
                          "color", color);
}

std::unique_ptr<GrFragmentProcessor> GrFragmentProcessor::MulChildByInputAlpha(
        std::unique_ptr<GrFragmentProcessor> fp) {
    if (!fp) {
        return nullptr;
    }
    return GrBlendFragmentProcessor::Make(/*src=*/nullptr,
                                          OverrideInput(std::move(fp), SK_PMColor4fWHITE),
                                          SkBlendMode::kDstIn);
}

std::unique_ptr<GrFragmentProcessor> GrFragmentProcessor::MulInputByChildAlpha(
        std::unique_ptr<GrFragmentProcessor> fp) {
    if (!fp) {
        return nullptr;
    }
    return GrBlendFragmentProcessor::Make(/*src=*/nullptr,
                                          OverrideInput(std::move(fp), SK_PMColor4fWHITE),
                                          SkBlendMode::kSrcIn);
}

std::unique_ptr<GrFragmentProcessor> GrFragmentProcessor::ModulateAlpha(
        std::unique_ptr<GrFragmentProcessor> inputFP, const SkPMColor4f& color) {
    auto colorFP = MakeColor(color);
    return GrBlendFragmentProcessor::Make(std::move(colorFP),
                                          std::move(inputFP),
                                          SkBlendMode::kSrcIn);
}

std::unique_ptr<GrFragmentProcessor> GrFragmentProcessor::ModulateRGBA(
        std::unique_ptr<GrFragmentProcessor> inputFP, const SkPMColor4f& color) {
    auto colorFP = MakeColor(color);
    return GrBlendFragmentProcessor::Make(std::move(colorFP),
                                          std::move(inputFP),
                                          SkBlendMode::kModulate);
}

std::unique_ptr<GrFragmentProcessor> GrFragmentProcessor::ClampOutput(
        std::unique_ptr<GrFragmentProcessor> fp) {
    SkASSERT(fp);
    static auto effect = SkMakeRuntimeEffect(SkRuntimeEffect::MakeForColorFilter, R"(
        half4 main(half4 inColor) {
            return saturate(inColor);
        }
    )");
    SkASSERT(SkRuntimeEffectPriv::SupportsConstantOutputForConstantInput(effect));
    return GrSkSLFP::Make(
            effect, "Clamp", std::move(fp), GrSkSLFP::OptFlags::kPreservesOpaqueInput);
}

std::unique_ptr<GrFragmentProcessor> GrFragmentProcessor::ClampPremulOutput(
        std::unique_ptr<GrFragmentProcessor> fp) {
    SkASSERT(fp);
    static auto effect = SkMakeRuntimeEffect(SkRuntimeEffect::MakeForColorFilter, R"(
        half4 main(half4 inColor) {
            half alpha = saturate(inColor.a);
            return half4(clamp(inColor.rgb, 0, alpha), alpha);
        }
    )");
    SkASSERT(SkRuntimeEffectPriv::SupportsConstantOutputForConstantInput(effect));
    return GrSkSLFP::Make(
            effect, "ClampPremul", std::move(fp), GrSkSLFP::OptFlags::kPreservesOpaqueInput);
}

std::unique_ptr<GrFragmentProcessor> GrFragmentProcessor::SwizzleOutput(
        std::unique_ptr<GrFragmentProcessor> fp, const GrSwizzle& swizzle) {
    class SwizzleFragmentProcessor : public GrFragmentProcessor {
    public:
        static std::unique_ptr<GrFragmentProcessor> Make(std::unique_ptr<GrFragmentProcessor> fp,
                                                         const GrSwizzle& swizzle) {
            return std::unique_ptr<GrFragmentProcessor>(
                    new SwizzleFragmentProcessor(std::move(fp), swizzle));
        }

        const char* name() const override { return "Swizzle"; }
        const GrSwizzle& swizzle() const { return fSwizzle; }

        std::unique_ptr<GrFragmentProcessor> clone() const override {
            return Make(this->childProcessor(0)->clone(), fSwizzle);
        }

    private:
        SwizzleFragmentProcessor(std::unique_ptr<GrFragmentProcessor> fp, const GrSwizzle& swizzle)
                : INHERITED(kSwizzleFragmentProcessor_ClassID, ProcessorOptimizationFlags(fp.get()))
                , fSwizzle(swizzle) {
            this->registerChild(std::move(fp));
        }

        std::unique_ptr<GrGLSLFragmentProcessor> onMakeProgramImpl() const override {
            class GLFP : public GrGLSLFragmentProcessor {
            public:
                void emitCode(EmitArgs& args) override {
                    SkString childColor = this->invokeChild(0, args);

                    const SwizzleFragmentProcessor& sfp = args.fFp.cast<SwizzleFragmentProcessor>();
                    const GrSwizzle& swizzle = sfp.swizzle();
                    GrGLSLFPFragmentBuilder* fragBuilder = args.fFragBuilder;

                    fragBuilder->codeAppendf("return %s.%s;",
                                             childColor.c_str(), swizzle.asString().c_str());
                }
            };
            return std::make_unique<GLFP>();
        }

        void onGetGLSLProcessorKey(const GrShaderCaps&, GrProcessorKeyBuilder* b) const override {
            b->add32(fSwizzle.asKey());
        }

        bool onIsEqual(const GrFragmentProcessor& other) const override {
            const SwizzleFragmentProcessor& sfp = other.cast<SwizzleFragmentProcessor>();
            return fSwizzle == sfp.fSwizzle;
        }

        SkPMColor4f constantOutputForConstantInput(const SkPMColor4f& input) const override {
            return fSwizzle.applyTo(ConstantOutputForConstantInput(this->childProcessor(0), input));
        }

        GrSwizzle fSwizzle;

        using INHERITED = GrFragmentProcessor;
    };

    if (!fp) {
        return nullptr;
    }
    if (GrSwizzle::RGBA() == swizzle) {
        return fp;
    }
    return SwizzleFragmentProcessor::Make(std::move(fp), swizzle);
}

std::unique_ptr<GrFragmentProcessor> GrFragmentProcessor::MakeInputPremulAndMulByOutput(
        std::unique_ptr<GrFragmentProcessor> fp) {
    class PremulFragmentProcessor : public GrFragmentProcessor {
    public:
        static std::unique_ptr<GrFragmentProcessor> Make(
                std::unique_ptr<GrFragmentProcessor> processor) {
            return std::unique_ptr<GrFragmentProcessor>(
                    new PremulFragmentProcessor(std::move(processor)));
        }

        const char* name() const override { return "Premultiply"; }

        std::unique_ptr<GrFragmentProcessor> clone() const override {
            return Make(this->childProcessor(0)->clone());
        }

    private:
        PremulFragmentProcessor(std::unique_ptr<GrFragmentProcessor> processor)
                : INHERITED(kPremulFragmentProcessor_ClassID, OptFlags(processor.get())) {
            this->registerChild(std::move(processor));
        }

        std::unique_ptr<GrGLSLFragmentProcessor> onMakeProgramImpl() const override {
            class GLFP : public GrGLSLFragmentProcessor {
            public:
                void emitCode(EmitArgs& args) override {
                    GrGLSLFPFragmentBuilder* fragBuilder = args.fFragBuilder;
                    SkString temp = this->invokeChild(/*childIndex=*/0, "half4(1)", args);
                    fragBuilder->codeAppendf("half4 color = %s;", temp.c_str());
                    fragBuilder->codeAppendf("color.rgb *= %s.rgb;", args.fInputColor);
                    fragBuilder->codeAppendf("return color * %s.a;", args.fInputColor);
                }
            };
            return std::make_unique<GLFP>();
        }

        void onGetGLSLProcessorKey(const GrShaderCaps&, GrProcessorKeyBuilder*) const override {}

        bool onIsEqual(const GrFragmentProcessor&) const override { return true; }

        static OptimizationFlags OptFlags(const GrFragmentProcessor* inner) {
            OptimizationFlags flags = kNone_OptimizationFlags;
            if (inner->preservesOpaqueInput()) {
                flags |= kPreservesOpaqueInput_OptimizationFlag;
            }
            if (inner->hasConstantOutputForConstantInput()) {
                flags |= kConstantOutputForConstantInput_OptimizationFlag;
            }
            return flags;
        }

        SkPMColor4f constantOutputForConstantInput(const SkPMColor4f& input) const override {
            SkPMColor4f childColor = ConstantOutputForConstantInput(this->childProcessor(0),
                                                                    SK_PMColor4fWHITE);
            SkPMColor4f premulInput = SkColor4f{ input.fR, input.fG, input.fB, input.fA }.premul();
            return premulInput * childColor;
        }

        using INHERITED = GrFragmentProcessor;
    };
    if (!fp) {
        return nullptr;
    }
    return PremulFragmentProcessor::Make(std::move(fp));
}

//////////////////////////////////////////////////////////////////////////////

std::unique_ptr<GrFragmentProcessor> GrFragmentProcessor::OverrideInput(
        std::unique_ptr<GrFragmentProcessor> fp, const SkPMColor4f& color, bool useUniform) {
    if (!fp) {
        return nullptr;
    }
    static auto effect = SkMakeRuntimeEffect(SkRuntimeEffect::MakeForColorFilter, R"(
        uniform colorFilter fp;  // Declared as colorFilter so we can use sample(..., color)
        uniform half4 color;
        half4 main(half4 inColor) {
            return sample(fp, color);
        }
    )");
    SkASSERT(SkRuntimeEffectPriv::SupportsConstantOutputForConstantInput(effect));
    return GrSkSLFP::Make(effect, "OverrideInput", /*inputFP=*/nullptr,
                          color.isOpaque() ? GrSkSLFP::OptFlags::kPreservesOpaqueInput
                                           : GrSkSLFP::OptFlags::kNone,
                          "fp", std::move(fp),
                          "color", GrSkSLFP::SpecializeIf(!useUniform, color));
}

//////////////////////////////////////////////////////////////////////////////

std::unique_ptr<GrFragmentProcessor> GrFragmentProcessor::MakeInputOpaqueAndPostApplyAlpha(
        std::unique_ptr<GrFragmentProcessor> fp) {
    if (!fp) {
        return nullptr;
    }
    static auto effect = SkMakeRuntimeEffect(SkRuntimeEffect::MakeForColorFilter, R"(
        uniform colorFilter fp;  // Declared as colorFilter so we can use sample(..., color)
        half4 main(half4 inColor) {
            return inColor.a * sample(fp, unpremul(inColor).rgb1);
        }
    )");
    return GrSkSLFP::Make(effect,
                          "MakeInputOpaque",
                          /*inputFP=*/nullptr,
                          GrSkSLFP::OptFlags::kPreservesOpaqueInput,
                          "fp", std::move(fp));
}

//////////////////////////////////////////////////////////////////////////////

std::unique_ptr<GrFragmentProcessor> GrFragmentProcessor::Compose(
        std::unique_ptr<GrFragmentProcessor> f, std::unique_ptr<GrFragmentProcessor> g) {
    class ComposeProcessor : public GrFragmentProcessor {
    public:
        static std::unique_ptr<GrFragmentProcessor> Make(std::unique_ptr<GrFragmentProcessor> f,
                                                         std::unique_ptr<GrFragmentProcessor> g) {
            return std::unique_ptr<GrFragmentProcessor>(new ComposeProcessor(std::move(f),
                                                                             std::move(g)));
        }

        const char* name() const override { return "Compose"; }

        std::unique_ptr<GrFragmentProcessor> clone() const override {
            return std::unique_ptr<GrFragmentProcessor>(new ComposeProcessor(*this));
        }

    private:
        std::unique_ptr<GrGLSLFragmentProcessor> onMakeProgramImpl() const override {
            class GLFP : public GrGLSLFragmentProcessor {
            public:
                void emitCode(EmitArgs& args) override {
                    SkString result = this->invokeChild(1, args);         // g(x)
                    result = this->invokeChild(0, result.c_str(), args);  // f(g(x))
                    args.fFragBuilder->codeAppendf("return %s;", result.c_str());
                }
            };
            return std::make_unique<GLFP>();
        }

        ComposeProcessor(std::unique_ptr<GrFragmentProcessor> f,
                         std::unique_ptr<GrFragmentProcessor> g)
                : INHERITED(kSeriesFragmentProcessor_ClassID,
                            f->optimizationFlags() & g->optimizationFlags()) {
            this->registerChild(std::move(f));
            this->registerChild(std::move(g));
        }

        ComposeProcessor(const ComposeProcessor& that)
                : INHERITED(kSeriesFragmentProcessor_ClassID, that.optimizationFlags()) {
            this->cloneAndRegisterAllChildProcessors(that);
        }

        void onGetGLSLProcessorKey(const GrShaderCaps&, GrProcessorKeyBuilder*) const override {}

        bool onIsEqual(const GrFragmentProcessor&) const override { return true; }

        SkPMColor4f constantOutputForConstantInput(const SkPMColor4f& inColor) const override {
            SkPMColor4f color = inColor;
            color = ConstantOutputForConstantInput(this->childProcessor(1), color);
            color = ConstantOutputForConstantInput(this->childProcessor(0), color);
            return color;
        }

        using INHERITED = GrFragmentProcessor;
    };

    // Allow either of the composed functions to be null.
    if (f == nullptr) {
        return g;
    }
    if (g == nullptr) {
        return f;
    }

    // Run an optimization pass on this composition.
    GrProcessorAnalysisColor inputColor;
    inputColor.setToUnknown();

    std::unique_ptr<GrFragmentProcessor> series[2] = {std::move(g), std::move(f)};
    GrColorFragmentProcessorAnalysis info(inputColor, series, SK_ARRAY_COUNT(series));

    SkPMColor4f knownColor;
    int leadingFPsToEliminate = info.initialProcessorsToEliminate(&knownColor);
    switch (leadingFPsToEliminate) {
        default:
            // We shouldn't eliminate more than we started with.
            SkASSERT(leadingFPsToEliminate <= 2);
            [[fallthrough]];
        case 0:
            // Compose the two processors as requested.
            return ComposeProcessor::Make(/*f=*/std::move(series[1]), /*g=*/std::move(series[0]));
        case 1:
            // Replace the first processor with a constant color.
            return ComposeProcessor::Make(/*f=*/std::move(series[1]),
                                          /*g=*/MakeColor(knownColor));
        case 2:
            // Replace the entire composition with a constant color.
            return MakeColor(knownColor);
    }
}

//////////////////////////////////////////////////////////////////////////////

std::unique_ptr<GrFragmentProcessor> GrFragmentProcessor::ColorMatrix(
        std::unique_ptr<GrFragmentProcessor> child,
        const float matrix[20],
        bool unpremulInput,
        bool clampRGBOutput,
        bool premulOutput) {
    static auto effect = SkMakeRuntimeEffect(SkRuntimeEffect::MakeForColorFilter, R"(
        uniform half4x4 m;
        uniform half4   v;
        uniform int unpremulInput;   // always specialized
        uniform int clampRGBOutput;  // always specialized
        uniform int premulOutput;    // always specialized
        half4 main(half4 color) {
            if (bool(unpremulInput)) {
                color = unpremul(color);
            }
            color = m * color + v;
            if (bool(clampRGBOutput)) {
                color = saturate(color);
            } else {
                color.a = saturate(color.a);
            }
            if (bool(premulOutput)) {
                color.rgb *= color.a;
            }
            return color;
        }
    )");
    SkASSERT(SkRuntimeEffectPriv::SupportsConstantOutputForConstantInput(effect));

    SkM44 m44(matrix[ 0], matrix[ 1], matrix[ 2], matrix[ 3],
              matrix[ 5], matrix[ 6], matrix[ 7], matrix[ 8],
              matrix[10], matrix[11], matrix[12], matrix[13],
              matrix[15], matrix[16], matrix[17], matrix[18]);
    SkV4 v4 = {matrix[4], matrix[9], matrix[14], matrix[19]};
    return GrSkSLFP::Make(effect, "ColorMatrix", std::move(child), GrSkSLFP::OptFlags::kNone,
                          "m", m44,
                          "v", v4,
                          "unpremulInput",  GrSkSLFP::Specialize(unpremulInput  ? 1 : 0),
                          "clampRGBOutput", GrSkSLFP::Specialize(clampRGBOutput ? 1 : 0),
                          "premulOutput",   GrSkSLFP::Specialize(premulOutput   ? 1 : 0));
}

//////////////////////////////////////////////////////////////////////////////

std::unique_ptr<GrFragmentProcessor> GrFragmentProcessor::DestColor() {
    class DestColorProcessor : public GrFragmentProcessor {
    public:
        static std::unique_ptr<GrFragmentProcessor> Make() {
            return std::unique_ptr<GrFragmentProcessor>(new DestColorProcessor());
        }

        std::unique_ptr<GrFragmentProcessor> clone() const override { return Make(); }

        const char* name() const override { return "DestColor"; }

    private:
        std::unique_ptr<GrGLSLFragmentProcessor> onMakeProgramImpl() const override {
            class GLFP : public GrGLSLFragmentProcessor {
            public:
                void emitCode(EmitArgs& args) override {
                    const char* destColor = args.fFragBuilder->dstColor();
                    args.fFragBuilder->codeAppendf("return %s;", destColor);
                }
            };
            return std::make_unique<GLFP>();
        }

        DestColorProcessor() : INHERITED(kDestColorProcessor_ClassID, kNone_OptimizationFlags) {
            this->setWillReadDstColor();
        }

        void onGetGLSLProcessorKey(const GrShaderCaps&, GrProcessorKeyBuilder*) const override {}

        bool onIsEqual(const GrFragmentProcessor&) const override { return true; }

        using INHERITED = GrFragmentProcessor;
    };

    return DestColorProcessor::Make();
}

//////////////////////////////////////////////////////////////////////////////

std::unique_ptr<GrFragmentProcessor> GrFragmentProcessor::DeviceSpace(
        std::unique_ptr<GrFragmentProcessor> fp) {
    static auto effect = SkMakeRuntimeEffect(SkRuntimeEffect::MakeForShader, R"(
        uniform shader fp;
        half4 main(float2 xy) {
            return sample(fp, sk_FragCoord.xy);
        }
    )");
    return GrSkSLFP::Make(effect, "DeviceSpace", /*inputFP=*/nullptr, GrSkSLFP::OptFlags::kAll,
                          "fp", std::move(fp));
}

//////////////////////////////////////////////////////////////////////////////

#define CLIP_EDGE_SKSL              \
    "const int kFillBW = 0;"        \
    "const int kFillAA = 1;"        \
    "const int kInverseFillBW = 2;" \
    "const int kInverseFillAA = 3;"

static_assert(static_cast<int>(GrClipEdgeType::kFillBW) == 0);
static_assert(static_cast<int>(GrClipEdgeType::kFillAA) == 1);
static_assert(static_cast<int>(GrClipEdgeType::kInverseFillBW) == 2);
static_assert(static_cast<int>(GrClipEdgeType::kInverseFillAA) == 3);

std::unique_ptr<GrFragmentProcessor> GrFragmentProcessor::Rect(
        std::unique_ptr<GrFragmentProcessor> inputFP, GrClipEdgeType edgeType, SkRect rect) {
    static auto effect = SkMakeRuntimeEffect(SkRuntimeEffect::MakeForShader, CLIP_EDGE_SKSL R"(
        uniform int edgeType;  // GrClipEdgeType, specialized
        uniform float4 rectUniform;

        half4 main(float2 xy, half4 inColor) {
            half coverage;
            if (edgeType == kFillBW || edgeType == kInverseFillBW) {
                // non-AA
                coverage = all(greaterThan(float4(sk_FragCoord.xy, rectUniform.zw),
                                           float4(rectUniform.xy, sk_FragCoord.xy))) ? 1 : 0;
            } else {
                // compute coverage relative to left and right edges, add, then subtract 1 to
                // account for double counting. And similar for top/bottom.
                half4 dists4 = clamp(half4(1, 1, -1, -1) *
                                     half4(sk_FragCoord.xyxy - rectUniform), 0, 1);
                half2 dists2 = dists4.xy + dists4.zw - 1;
                coverage = dists2.x * dists2.y;
            }

            if (edgeType == kInverseFillBW || edgeType == kInverseFillAA) {
                coverage = 1.0 - coverage;
            }

            return inColor * coverage;
        }
    )");

    SkASSERT(rect.isSorted());
    // The AA math in the shader evaluates to 0 at the uploaded coordinates, so outset by 0.5
    // to interpolate from 0 at a half pixel inset and 1 at a half pixel outset of rect.
    SkRect rectUniform = GrClipEdgeTypeIsAA(edgeType) ? rect.makeOutset(.5f, .5f) : rect;

    return GrSkSLFP::Make(effect, "Rect", std::move(inputFP),
                          GrSkSLFP::OptFlags::kCompatibleWithCoverageAsAlpha,
                          "edgeType", GrSkSLFP::Specialize(static_cast<int>(edgeType)),
                          "rectUniform", rectUniform);
}

GrFPResult GrFragmentProcessor::Circle(std::unique_ptr<GrFragmentProcessor> inputFP,
                                       GrClipEdgeType edgeType,
                                       SkPoint center,
                                       float radius) {
    // A radius below half causes the implicit insetting done by this processor to become
    // inverted. We could handle this case by making the processor code more complicated.
    if (radius < .5f && GrClipEdgeTypeIsInverseFill(edgeType)) {
        return GrFPFailure(std::move(inputFP));
    }

    static auto effect = SkMakeRuntimeEffect(SkRuntimeEffect::MakeForShader, CLIP_EDGE_SKSL R"(
        uniform int edgeType;  // GrClipEdgeType, specialized
        // The circle uniform is (center.x, center.y, radius + 0.5, 1 / (radius + 0.5)) for regular
        // fills and (..., radius - 0.5, 1 / (radius - 0.5)) for inverse fills.
        uniform float4 circle;

        half4 main(float2 xy, half4 inColor) {
            // TODO: Right now the distance to circle calculation is performed in a space normalized
            // to the radius and then denormalized. This is to mitigate overflow on devices that
            // don't have full float.
            half d;
            if (edgeType == kInverseFillBW || edgeType == kInverseFillAA) {
                d = half((length((circle.xy - sk_FragCoord.xy) * circle.w) - 1.0) * circle.z);
            } else {
                d = half((1.0 - length((circle.xy - sk_FragCoord.xy) *  circle.w)) * circle.z);
            }
            if (edgeType == kFillAA || edgeType == kInverseFillAA) {
                return inColor * saturate(d);
            } else {
                return d > 0.5 ? inColor : half4(0);
            }
        }
    )");

    SkScalar effectiveRadius = radius;
    if (GrClipEdgeTypeIsInverseFill(edgeType)) {
        effectiveRadius -= 0.5f;
        // When the radius is 0.5 effectiveRadius is 0 which causes an inf * 0 in the shader.
        effectiveRadius = std::max(0.001f, effectiveRadius);
    } else {
        effectiveRadius += 0.5f;
    }
    SkV4 circle = {center.fX, center.fY, effectiveRadius, SkScalarInvert(effectiveRadius)};

    return GrFPSuccess(GrSkSLFP::Make(effect, "Circle", std::move(inputFP),
                                      GrSkSLFP::OptFlags::kCompatibleWithCoverageAsAlpha,
                                      "edgeType", GrSkSLFP::Specialize(static_cast<int>(edgeType)),
                                      "circle", circle));
}

GrFPResult GrFragmentProcessor::Ellipse(std::unique_ptr<GrFragmentProcessor> inputFP,
                                        GrClipEdgeType edgeType,
                                        SkPoint center,
                                        SkPoint radii,
                                        const GrShaderCaps& caps) {
    const bool medPrecision = !caps.floatIs32Bits();

    // Small radii produce bad results on devices without full float.
    if (medPrecision && (radii.fX < 0.5f || radii.fY < 0.5f)) {
        return GrFPFailure(std::move(inputFP));
    }
    // Very narrow ellipses produce bad results on devices without full float
    if (medPrecision && (radii.fX > 255*radii.fY || radii.fY > 255*radii.fX)) {
        return GrFPFailure(std::move(inputFP));
    }
    // Very large ellipses produce bad results on devices without full float
    if (medPrecision && (radii.fX > 16384 || radii.fY > 16384)) {
        return GrFPFailure(std::move(inputFP));
    }

    static auto effect = SkMakeRuntimeEffect(SkRuntimeEffect::MakeForShader, CLIP_EDGE_SKSL R"(
        uniform int edgeType;      // GrClipEdgeType, specialized
        uniform int medPrecision;  // !sk_Caps.floatIs32Bits, specialized

        uniform float4 ellipse;
        uniform float2 scale;    // only for medPrecision

        half4 main(float2 xy, half4 inColor) {
            // d is the offset to the ellipse center
            float2 d = sk_FragCoord.xy - ellipse.xy;
            // If we're on a device with a "real" mediump then we'll do the distance computation in
            // a space that is normalized by the larger radius or 128, whichever is smaller. The
            // scale uniform will be scale, 1/scale. The inverse squared radii uniform values are
            // already in this normalized space. The center is not.
            if (bool(medPrecision)) {
                d *= scale.y;
            }
            float2 Z = d * ellipse.zw;
            // implicit is the evaluation of (x/rx)^2 + (y/ry)^2 - 1.
            float implicit = dot(Z, d) - 1;
            // grad_dot is the squared length of the gradient of the implicit.
            float grad_dot = 4 * dot(Z, Z);
            // Avoid calling inversesqrt on zero.
            if (bool(medPrecision)) {
                grad_dot = max(grad_dot, 6.1036e-5);
            } else {
                grad_dot = max(grad_dot, 1.1755e-38);
            }
            float approx_dist = implicit * inversesqrt(grad_dot);
            if (bool(medPrecision)) {
                approx_dist *= scale.x;
            }

            half alpha;
            if (edgeType == kFillBW) {
                alpha = approx_dist > 0.0 ? 0.0 : 1.0;
            } else if (edgeType == kFillAA) {
                alpha = saturate(0.5 - half(approx_dist));
            } else if (edgeType == kInverseFillBW) {
                alpha = approx_dist > 0.0 ? 1.0 : 0.0;
            } else {  // edgeType == kInverseFillAA
                alpha = saturate(0.5 + half(approx_dist));
            }
            return inColor * alpha;
        }
    )");

    float invRXSqd;
    float invRYSqd;
    SkV2 scale = {1, 1};
    // If we're using a scale factor to work around precision issues, choose the larger radius as
    // the scale factor. The inv radii need to be pre-adjusted by the scale factor.
    if (medPrecision) {
        if (radii.fX > radii.fY) {
            invRXSqd = 1.f;
            invRYSqd = (radii.fX * radii.fX) / (radii.fY * radii.fY);
            scale = {radii.fX, 1.f / radii.fX};
        } else {
            invRXSqd = (radii.fY * radii.fY) / (radii.fX * radii.fX);
            invRYSqd = 1.f;
            scale = {radii.fY, 1.f / radii.fY};
        }
    } else {
        invRXSqd = 1.f / (radii.fX * radii.fX);
        invRYSqd = 1.f / (radii.fY * radii.fY);
    }
    SkV4 ellipse = {center.fX, center.fY, invRXSqd, invRYSqd};

    return GrFPSuccess(GrSkSLFP::Make(effect, "Ellipse", std::move(inputFP),
                                      GrSkSLFP::OptFlags::kCompatibleWithCoverageAsAlpha,
                                      "edgeType", GrSkSLFP::Specialize(static_cast<int>(edgeType)),
                                      "medPrecision",  GrSkSLFP::Specialize<int>(medPrecision),
                                      "ellipse", ellipse,
                                      "scale", scale));
}

//////////////////////////////////////////////////////////////////////////////

std::unique_ptr<GrFragmentProcessor> GrFragmentProcessor::HighPrecision(
        std::unique_ptr<GrFragmentProcessor> fp) {
    class HighPrecisionFragmentProcessor : public GrFragmentProcessor {
    public:
        static std::unique_ptr<GrFragmentProcessor> Make(std::unique_ptr<GrFragmentProcessor> fp) {
            return std::unique_ptr<GrFragmentProcessor>(
                    new HighPrecisionFragmentProcessor(std::move(fp)));
        }

        const char* name() const override { return "HighPrecision"; }

        std::unique_ptr<GrFragmentProcessor> clone() const override {
            return Make(this->childProcessor(0)->clone());
        }

    private:
        HighPrecisionFragmentProcessor(std::unique_ptr<GrFragmentProcessor> fp)
                : INHERITED(kHighPrecisionFragmentProcessor_ClassID,
                            ProcessorOptimizationFlags(fp.get())) {
            this->registerChild(std::move(fp));
        }

        std::unique_ptr<GrGLSLFragmentProcessor> onMakeProgramImpl() const override {
            class GLFP : public GrGLSLFragmentProcessor {
            public:
                void emitCode(EmitArgs& args) override {
                    SkString childColor = this->invokeChild(0, args);

                    args.fFragBuilder->forceHighPrecision();
                    args.fFragBuilder->codeAppendf("return %s;", childColor.c_str());
                }
            };
            return std::make_unique<GLFP>();
        }

        void onGetGLSLProcessorKey(const GrShaderCaps&, GrProcessorKeyBuilder* b) const override {}
        bool onIsEqual(const GrFragmentProcessor& other) const override { return true; }

        SkPMColor4f constantOutputForConstantInput(const SkPMColor4f& input) const override {
            return ConstantOutputForConstantInput(this->childProcessor(0), input);
        }

        using INHERITED = GrFragmentProcessor;
    };

    return HighPrecisionFragmentProcessor::Make(std::move(fp));
}

//////////////////////////////////////////////////////////////////////////////

GrFragmentProcessor::CIter::CIter(const GrPaint& paint) {
    if (paint.hasCoverageFragmentProcessor()) {
        fFPStack.push_back(paint.getCoverageFragmentProcessor());
    }
    if (paint.hasColorFragmentProcessor()) {
        fFPStack.push_back(paint.getColorFragmentProcessor());
    }
}

GrFragmentProcessor::CIter::CIter(const GrPipeline& pipeline) {
    for (int i = pipeline.numFragmentProcessors() - 1; i >= 0; --i) {
        fFPStack.push_back(&pipeline.getFragmentProcessor(i));
    }
}

GrFragmentProcessor::CIter& GrFragmentProcessor::CIter::operator++() {
    SkASSERT(!fFPStack.empty());
    const GrFragmentProcessor* back = fFPStack.back();
    fFPStack.pop_back();
    for (int i = back->numChildProcessors() - 1; i >= 0; --i) {
        if (auto child = back->childProcessor(i)) {
            fFPStack.push_back(child);
        }
    }
    return *this;
}

