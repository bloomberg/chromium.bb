/*
 * Copyright 2019 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "include/core/SkColorFilter.h"
#include "include/core/SkData.h"
#include "include/core/SkSurface.h"
#include "include/effects/SkRuntimeEffect.h"
#include "include/private/SkChecksum.h"
#include "include/private/SkMutex.h"
#include "src/core/SkCanvasPriv.h"
#include "src/core/SkColorFilterBase.h"
#include "src/core/SkColorSpacePriv.h"
#include "src/core/SkColorSpaceXformSteps.h"
#include "src/core/SkLRUCache.h"
#include "src/core/SkMatrixProvider.h"
#include "src/core/SkOpts.h"
#include "src/core/SkRasterPipeline.h"
#include "src/core/SkReadBuffer.h"
#include "src/core/SkRuntimeEffectPriv.h"
#include "src/core/SkUtils.h"
#include "src/core/SkVM.h"
#include "src/core/SkWriteBuffer.h"
#include "src/sksl/SkSLAnalysis.h"
#include "src/sksl/SkSLCompiler.h"
#include "src/sksl/SkSLUtil.h"
#include "src/sksl/codegen/SkSLVMCodeGenerator.h"
#include "src/sksl/ir/SkSLFunctionDefinition.h"
#include "src/sksl/ir/SkSLVarDeclarations.h"

#if SK_SUPPORT_GPU
#include "include/gpu/GrRecordingContext.h"
#include "src/gpu/GrColorInfo.h"
#include "src/gpu/GrFPArgs.h"
#include "src/gpu/GrImageInfo.h"
#include "src/gpu/GrSurfaceFillContext.h"
#include "src/gpu/effects/GrMatrixEffect.h"
#include "src/gpu/effects/GrSkSLFP.h"
#include "src/image/SkImage_Gpu.h"
#endif

#include <algorithm>

namespace SkSL {
class SharedCompiler {
public:
    SharedCompiler() : fLock(compiler_mutex()) {
        if (!gImpl) {
            gImpl = new Impl();
        }
    }

    SkSL::Compiler* operator->() const { return gImpl->fCompiler; }

private:
    SkAutoMutexExclusive fLock;

    static SkMutex& compiler_mutex() {
        static SkMutex& mutex = *(new SkMutex);
        return mutex;
    }

    struct Impl {
        Impl() {
            // These caps are configured to apply *no* workarounds. This avoids changes that are
            // unnecessary (GLSL intrinsic rewrites), or possibly incorrect (adding do-while loops).
            // We may apply other "neutral" transformations to the user's SkSL, including inlining.
            // Anything determined by the device caps is deferred to the GPU backend. The processor
            // set produces the final program (including our re-emitted SkSL), and the backend's
            // compiler resolves any necessary workarounds.
            fCaps = ShaderCapsFactory::Standalone();
            fCaps->fBuiltinFMASupport = true;
            fCaps->fBuiltinDeterminantSupport = true;
            // Don't inline if it would require a do loop, some devices don't support them.
            fCaps->fCanUseDoLoops = false;

            fCompiler = new SkSL::Compiler(fCaps.get());
        }

        SkSL::ShaderCapsPointer fCaps;
        SkSL::Compiler*         fCompiler;
    };

    static Impl* gImpl;
};

SharedCompiler::Impl* SharedCompiler::gImpl = nullptr;

}  // namespace SkSL

static bool init_uniform_type(const SkSL::Context& ctx,
                              const SkSL::Type* type,
                              SkRuntimeEffect::Uniform* v) {
    using Type = SkRuntimeEffect::Uniform::Type;
    if (*type == *ctx.fTypes.fFloat)    { v->type = Type::kFloat;    return true; }
    if (*type == *ctx.fTypes.fHalf)     { v->type = Type::kFloat;    return true; }
    if (*type == *ctx.fTypes.fFloat2)   { v->type = Type::kFloat2;   return true; }
    if (*type == *ctx.fTypes.fHalf2)    { v->type = Type::kFloat2;   return true; }
    if (*type == *ctx.fTypes.fFloat3)   { v->type = Type::kFloat3;   return true; }
    if (*type == *ctx.fTypes.fHalf3)    { v->type = Type::kFloat3;   return true; }
    if (*type == *ctx.fTypes.fFloat4)   { v->type = Type::kFloat4;   return true; }
    if (*type == *ctx.fTypes.fHalf4)    { v->type = Type::kFloat4;   return true; }
    if (*type == *ctx.fTypes.fFloat2x2) { v->type = Type::kFloat2x2; return true; }
    if (*type == *ctx.fTypes.fHalf2x2)  { v->type = Type::kFloat2x2; return true; }
    if (*type == *ctx.fTypes.fFloat3x3) { v->type = Type::kFloat3x3; return true; }
    if (*type == *ctx.fTypes.fHalf3x3)  { v->type = Type::kFloat3x3; return true; }
    if (*type == *ctx.fTypes.fFloat4x4) { v->type = Type::kFloat4x4; return true; }
    if (*type == *ctx.fTypes.fHalf4x4)  { v->type = Type::kFloat4x4; return true; }

    if (*type == *ctx.fTypes.fInt)  { v->type = Type::kInt;  return true; }
    if (*type == *ctx.fTypes.fInt2) { v->type = Type::kInt2; return true; }
    if (*type == *ctx.fTypes.fInt3) { v->type = Type::kInt3; return true; }
    if (*type == *ctx.fTypes.fInt4) { v->type = Type::kInt4; return true; }

    return false;
}

static SkRuntimeEffect::Child::Type child_type(const SkSL::Type& type) {
    switch (type.typeKind()) {
        case SkSL::Type::TypeKind::kColorFilter: return SkRuntimeEffect::Child::Type::kColorFilter;
        case SkSL::Type::TypeKind::kShader:      return SkRuntimeEffect::Child::Type::kShader;
        default: SkUNREACHABLE;
    }
}

// TODO: Many errors aren't caught until we process the generated Program here. Catching those
// in the IR generator would provide better errors messages (with locations).
#define RETURN_FAILURE(...) return Result{nullptr, SkStringPrintf(__VA_ARGS__)}

SkRuntimeEffect::Result SkRuntimeEffect::Make(SkString sksl, const Options& options,
                                              SkSL::ProgramKind kind) {
    std::unique_ptr<SkSL::Program> program;
    {
        // We keep this SharedCompiler in a separate scope to make sure it's destroyed before
        // calling the Make overload at the end, which creates its own (non-reentrant)
        // SharedCompiler instance
        SkSL::SharedCompiler compiler;
        SkSL::Program::Settings settings;
        settings.fInlineThreshold = 0;
        settings.fForceNoInline = options.forceNoInline;
#if GR_TEST_UTILS
        settings.fEnforceES2Restrictions = options.enforceES2Restrictions;
#endif
        settings.fAllowNarrowingConversions = true;
        program = compiler->convertProgram(kind, SkSL::String(sksl.c_str(), sksl.size()), settings);

        if (!program) {
            RETURN_FAILURE("%s", compiler->errorText().c_str());
        }
    }
    return Make(std::move(sksl), std::move(program), options, kind);
}

SkRuntimeEffect::Result SkRuntimeEffect::Make(std::unique_ptr<SkSL::Program> program,
                                              SkSL::ProgramKind kind) {
    SkString source(program->description().c_str());
    return Make(std::move(source), std::move(program), Options{}, kind);
}

SkRuntimeEffect::Result SkRuntimeEffect::Make(SkString sksl,
                                              std::unique_ptr<SkSL::Program> program,
                                              const Options& options,
                                              SkSL::ProgramKind kind) {
    SkSL::SharedCompiler compiler;
    SkSL::Program::Settings settings;
    settings.fInlineThreshold = 0;
    settings.fForceNoInline = options.forceNoInline;
    settings.fAllowNarrowingConversions = true;

    // Find 'main', then locate the sample coords parameter. (It might not be present.)
    const SkSL::FunctionDefinition* main = SkSL::Program_GetFunction(*program, "main");
    if (!main) {
        RETURN_FAILURE("missing 'main' function");
    }
    const auto& mainParams = main->declaration().parameters();
    auto iter = std::find_if(mainParams.begin(), mainParams.end(), [](const SkSL::Variable* p) {
        return p->modifiers().fLayout.fBuiltin == SK_MAIN_COORDS_BUILTIN;
    });
    const SkSL::ProgramUsage::VariableCounts sampleCoordsUsage =
            iter != mainParams.end() ? program->usage()->get(**iter)
                                     : SkSL::ProgramUsage::VariableCounts{};

    uint32_t flags = 0;
    switch (kind) {
        case SkSL::ProgramKind::kRuntimeColorFilter: flags |= kAllowColorFilter_Flag; break;
        case SkSL::ProgramKind::kRuntimeShader:      flags |= kAllowShader_Flag;      break;
        default: SkUNREACHABLE;
    }


    if (sampleCoordsUsage.fRead || sampleCoordsUsage.fWrite) {
        flags |= kUsesSampleCoords_Flag;
    }

    // Color filters are not allowed to depend on position (local or device) in any way.
    // The signature of main, and the declarations in sksl_rt_colorfilter should guarantee this.
    if (flags & kAllowColorFilter_Flag) {
        SkASSERT(!(flags & kUsesSampleCoords_Flag));
        SkASSERT(!SkSL::Analysis::ReferencesFragCoords(*program));
    }

    size_t offset = 0;
    std::vector<Uniform> uniforms;
    std::vector<Child> children;
    std::vector<SkSL::SampleUsage> sampleUsages;
    const SkSL::Context& ctx(compiler->context());

    // Go through program elements, pulling out information that we need
    for (const SkSL::ProgramElement* elem : program->elements()) {
        // Variables (uniform, etc.)
        if (elem->is<SkSL::GlobalVarDeclaration>()) {
            const SkSL::GlobalVarDeclaration& global = elem->as<SkSL::GlobalVarDeclaration>();
            const SkSL::VarDeclaration& varDecl = global.declaration()->as<SkSL::VarDeclaration>();

            const SkSL::Variable& var = varDecl.var();
            const SkSL::Type& varType = var.type();

            // Child effects that can be sampled ('shader' or 'colorFilter')
            if (varType.isEffectChild()) {
                Child c;
                c.name  = var.name();
                c.type  = child_type(varType);
                c.index = children.size();
                children.push_back(c);
                sampleUsages.push_back(SkSL::Analysis::GetSampleUsage(
                        *program, var, sampleCoordsUsage.fWrite != 0));
            }
            // 'uniform' variables
            else if (var.modifiers().fFlags & SkSL::Modifiers::kUniform_Flag) {
                Uniform uni;
                uni.name = var.name();
                uni.flags = 0;
                uni.count = 1;

                const SkSL::Type* type = &var.type();
                if (type->isArray()) {
                    uni.flags |= Uniform::kArray_Flag;
                    uni.count = type->columns();
                    type = &type->componentType();
                }

                if (!init_uniform_type(ctx, type, &uni)) {
                    RETURN_FAILURE("Invalid uniform type: '%s'", type->displayName().c_str());
                }

                if (var.modifiers().fLayout.fFlags & SkSL::Layout::Flag::kSRGBUnpremul_Flag) {
                    uni.flags |= Uniform::kSRGBUnpremul_Flag;
                }

                uni.offset = offset;
                offset += uni.sizeInBytes();
                SkASSERT(SkIsAlign4(offset));

                uniforms.push_back(uni);
            }
        }
    }

#undef RETURN_FAILURE

    sk_sp<SkRuntimeEffect> effect(new SkRuntimeEffect(std::move(sksl),
                                                      std::move(program),
                                                      options,
                                                      *main,
                                                      std::move(uniforms),
                                                      std::move(children),
                                                      std::move(sampleUsages),
                                                      flags));
    return Result{std::move(effect), SkString()};
}

SkRuntimeEffect::Result SkRuntimeEffect::MakeForColorFilter(SkString sksl, const Options& options) {
    auto result = Make(std::move(sksl), options, SkSL::ProgramKind::kRuntimeColorFilter);
    SkASSERT(!result.effect || result.effect->allowColorFilter());
    return result;
}

SkRuntimeEffect::Result SkRuntimeEffect::MakeForShader(SkString sksl, const Options& options) {
    auto result = Make(std::move(sksl), options, SkSL::ProgramKind::kRuntimeShader);
    SkASSERT(!result.effect || result.effect->allowShader());
    return result;
}

SkRuntimeEffect::Result SkRuntimeEffect::MakeForColorFilter(std::unique_ptr<SkSL::Program> program) {
    auto result = Make(std::move(program), SkSL::ProgramKind::kRuntimeColorFilter);
    SkASSERT(!result.effect || result.effect->allowColorFilter());
    return result;
}

SkRuntimeEffect::Result SkRuntimeEffect::MakeForShader(std::unique_ptr<SkSL::Program> program) {
    auto result = Make(std::move(program), SkSL::ProgramKind::kRuntimeShader);
    SkASSERT(!result.effect || result.effect->allowShader());
    return result;
}

sk_sp<SkRuntimeEffect> SkMakeCachedRuntimeEffect(SkRuntimeEffect::Result (*make)(SkString sksl),
                                                 SkString sksl) {
    SK_BEGIN_REQUIRE_DENSE
    struct Key {
        uint32_t skslHashA;
        uint32_t skslHashB;

        bool operator==(const Key& that) const {
            return this->skslHashA == that.skslHashA
                && this->skslHashB == that.skslHashB;
        }

        explicit Key(const SkString& sksl)
            : skslHashA(SkOpts::hash(sksl.c_str(), sksl.size(), 0))
            , skslHashB(SkOpts::hash(sksl.c_str(), sksl.size(), 1)) {}
    };
    SK_END_REQUIRE_DENSE

    static auto* mutex = new SkMutex;
    static auto* cache = new SkLRUCache<Key, sk_sp<SkRuntimeEffect>>(11/*totally arbitrary*/);

    Key key(sksl);
    {
        SkAutoMutexExclusive _(*mutex);
        if (sk_sp<SkRuntimeEffect>* found = cache->find(key)) {
            return *found;
        }
    }

    auto [effect, err] = make(std::move(sksl));
    if (!effect) {
        return nullptr;
    }
    SkASSERT(err.isEmpty());

    {
        SkAutoMutexExclusive _(*mutex);
        cache->insert_or_update(key, effect);
    }
    return effect;
}

size_t SkRuntimeEffect::Uniform::sizeInBytes() const {
    static_assert(sizeof(int) == sizeof(float));
    auto element_size = [](Type type) -> size_t {
        switch (type) {
            case Type::kFloat:  return sizeof(float);
            case Type::kFloat2: return sizeof(float) * 2;
            case Type::kFloat3: return sizeof(float) * 3;
            case Type::kFloat4: return sizeof(float) * 4;

            case Type::kFloat2x2: return sizeof(float) * 4;
            case Type::kFloat3x3: return sizeof(float) * 9;
            case Type::kFloat4x4: return sizeof(float) * 16;

            case Type::kInt:  return sizeof(int);
            case Type::kInt2: return sizeof(int) * 2;
            case Type::kInt3: return sizeof(int) * 3;
            case Type::kInt4: return sizeof(int) * 4;
            default: SkUNREACHABLE;
        }
    };
    return element_size(this->type) * this->count;
}

SkRuntimeEffect::SkRuntimeEffect(SkString sksl,
                                 std::unique_ptr<SkSL::Program> baseProgram,
                                 const Options& options,
                                 const SkSL::FunctionDefinition& main,
                                 std::vector<Uniform>&& uniforms,
                                 std::vector<Child>&& children,
                                 std::vector<SkSL::SampleUsage>&& sampleUsages,
                                 uint32_t flags)
        : fHash(SkGoodHash()(sksl))
        , fSkSL(std::move(sksl))
        , fBaseProgram(std::move(baseProgram))
        , fMain(main)
        , fUniforms(std::move(uniforms))
        , fChildren(std::move(children))
        , fSampleUsages(std::move(sampleUsages))
        , fFlags(flags) {
    SkASSERT(fBaseProgram);
    SkASSERT(fChildren.size() == fSampleUsages.size());

    // Everything from SkRuntimeEffect::Options which could influence the compiled result needs to
    // be accounted for in `fHash`. If you've added a new field to Options and caused the static-
    // assert below to trigger, please incorporate your field into `fHash` and update KnownOptions
    // to match the layout of Options.
    struct KnownOptions { bool a, b; };
    static_assert(sizeof(Options) == sizeof(KnownOptions));
    fHash = SkOpts::hash_fn(&options.forceNoInline,
                      sizeof(options.forceNoInline), fHash);
    fHash = SkOpts::hash_fn(&options.enforceES2Restrictions,
                      sizeof(options.enforceES2Restrictions), fHash);

    this->initFilterColorInfo();
}

SkRuntimeEffect::~SkRuntimeEffect() = default;

size_t SkRuntimeEffect::uniformSize() const {
    return fUniforms.empty() ? 0
                             : SkAlign4(fUniforms.back().offset + fUniforms.back().sizeInBytes());
}

const SkRuntimeEffect::Uniform* SkRuntimeEffect::findUniform(const char* name) const {
    auto iter = std::find_if(fUniforms.begin(), fUniforms.end(),
                             [name](const Uniform& u) { return u.name.equals(name); });
    return iter == fUniforms.end() ? nullptr : &(*iter);
}

const SkRuntimeEffect::Child* SkRuntimeEffect::findChild(const char* name) const {
    auto iter = std::find_if(fChildren.begin(), fChildren.end(),
                             [name](const Child& c) { return c.name.equals(name); });
    return iter == fChildren.end() ? nullptr : &(*iter);
}

void SkRuntimeEffect::initFilterColorInfo() {
    // Runtime effects are often long lived & cached. So: build and save a program that can
    // filter a single color, without baking in anything tied to a particular instance
    // (uniforms or children). This isn't possible (or needed) for shaders.
    if (!this->allowColorFilter()) {
        return;
    }

    // We allocate a uniform color for the input color, and for each child in the SkSL. When we run
    // this program later, these uniform values are replaced with either the results of the child,
    // or the input color (if the child is nullptr). These Uniform ids are loads from the *first*
    // arg ptr.
    //
    // This scheme only works if every child is sampled using the original input color. If we detect
    // a sampleChild call where a different color is being supplied, we bail out, and the returned
    // info will have a null program. (Callers will need to fall back to another implementation.)
    skvm::Builder p;
    skvm::Uniforms childColorUniforms{p.uniform(), 0};
    skvm::Color inputColor = p.uniformColor(/*placeholder*/ SkColors::kWhite, &childColorUniforms);
    std::vector<skvm::Color> childColors;
    for (size_t i = 0; i < fChildren.size(); ++i) {
        childColors.push_back(
                p.uniformColor(/*placeholder*/ SkColors::kWhite, &childColorUniforms));
    }
    bool allSampleCallsPassInputColor = true;
    auto sampleChild = [&](int ix, skvm::Coord, skvm::Color color) {
        if (color.r.id != inputColor.r.id ||
            color.g.id != inputColor.g.id ||
            color.b.id != inputColor.b.id ||
            color.a.id != inputColor.a.id) {
            allSampleCallsPassInputColor = false;
        }
        return childColors[ix];
    };

    // For SkSL uniforms, we reserve space and allocate skvm Uniform ids for each one. When we run
    // the program, these ids will be loads from the *second* arg ptr, the uniform data of the
    // specific color filter instance.
    skvm::Uniforms skslUniforms{p.uniform(), 0};
    const size_t uniformCount = this->uniformSize() / 4;
    std::vector<skvm::Val> uniform;
    uniform.reserve(uniformCount);
    for (size_t i = 0; i < uniformCount; i++) {
        uniform.push_back(p.uniform32(skslUniforms.push(/*placeholder*/ 0)).id);
    }

    // Emit the skvm instructions for the SkSL
    skvm::Coord zeroCoord = {p.splat(0.0f), p.splat(0.0f)};
    skvm::Color result = SkSL::ProgramToSkVM(*fBaseProgram,
                                             fMain,
                                             &p,
                                             SkMakeSpan(uniform),
                                             /*device=*/zeroCoord,
                                             /*local=*/zeroCoord,
                                             inputColor,
                                             sampleChild);

    // Then store the result to the *third* arg ptr
    p.store({skvm::PixelFormat::FLOAT, 32, 32, 32, 32, 0, 32, 64, 96}, p.arg(16), result);

    // This is conservative. If a filter gets the input color by sampling a null child, we'll
    // return an (acceptable) false negative. All internal runtime color filters should work.
    fColorFilterProgramLeavesAlphaUnchanged = (inputColor.a.id == result.a.id);

    // We'll use this program to filter one color at a time, don't bother with jit
    fColorFilterProgram = allSampleCallsPassInputColor
                                  ? std::make_unique<skvm::Program>(
                                            p.done(/*debug_name=*/nullptr, /*allow_jit=*/false))
                                  : nullptr;
}

SkRuntimeEffect::FilterColorInfo SkRuntimeEffect::getFilterColorInfo() {
    return {fColorFilterProgram.get(), fColorFilterProgramLeavesAlphaUnchanged};
}

///////////////////////////////////////////////////////////////////////////////////////////////////

static sk_sp<SkData> get_xformed_uniforms(const SkRuntimeEffect* effect,
                                          sk_sp<SkData> baseUniforms,
                                          const SkColorSpace* dstCS) {
    using Flags = SkRuntimeEffect::Uniform::Flags;
    using Type = SkRuntimeEffect::Uniform::Type;
    SkColorSpaceXformSteps steps(sk_srgb_singleton(), kUnpremul_SkAlphaType,
                                 dstCS,               kUnpremul_SkAlphaType);

    sk_sp<SkData> uniforms = nullptr;
    auto writableData = [&]() {
        if (!uniforms) {
            uniforms =  SkData::MakeWithCopy(baseUniforms->data(), baseUniforms->size());
        }
        return uniforms->writable_data();
    };

    for (const auto& v : effect->uniforms()) {
        if (v.flags & Flags::kSRGBUnpremul_Flag) {
            SkASSERT(v.type == Type::kFloat3 || v.type == Type::kFloat4);
            if (steps.flags.mask()) {
                float* color = SkTAddOffset<float>(writableData(), v.offset);
                if (v.type == Type::kFloat4) {
                    // RGBA, easy case
                    for (int i = 0; i < v.count; ++i) {
                        steps.apply(color);
                        color += 4;
                    }
                } else {
                    // RGB, need to pad out to include alpha. Technically, this isn't necessary,
                    // because steps shouldn't include unpremul or premul, and thus shouldn't
                    // read or write the fourth element. But let's be safe.
                    float rgba[4];
                    for (int i = 0; i < v.count; ++i) {
                        memcpy(rgba, color, 3 * sizeof(float));
                        rgba[3] = 1.0f;
                        steps.apply(rgba);
                        memcpy(color, rgba, 3 * sizeof(float));
                        color += 3;
                    }
                }
            }
        }
    }
    return uniforms ? uniforms : baseUniforms;
}

class SkRuntimeColorFilter : public SkColorFilterBase {
public:
    SkRuntimeColorFilter(sk_sp<SkRuntimeEffect> effect,
                         sk_sp<SkData> uniforms,
                         sk_sp<SkColorFilter> children[],
                         size_t childCount)
            : fEffect(std::move(effect))
            , fUniforms(std::move(uniforms))
            , fChildren(children, children + childCount) {}

#if SK_SUPPORT_GPU
    GrFPResult asFragmentProcessor(std::unique_ptr<GrFragmentProcessor> inputFP,
                                   GrRecordingContext* context,
                                   const GrColorInfo& colorInfo) const override {
        sk_sp<SkData> uniforms =
                get_xformed_uniforms(fEffect.get(), fUniforms, colorInfo.colorSpace());
        SkASSERT(uniforms);

        auto fp = GrSkSLFP::Make(fEffect, "Runtime_Color_Filter", std::move(uniforms));
        for (const auto& child : fChildren) {
            std::unique_ptr<GrFragmentProcessor> childFP;
            if (child) {
                bool success;
                std::tie(success, childFP) = as_CFB(child)->asFragmentProcessor(
                        /*inputFP=*/nullptr, context, colorInfo);
                if (!success) {
                    return GrFPFailure(std::move(inputFP));
                }
            }
            fp->addChild(std::move(childFP));
        }

        // Runtime effect scripts are written to take an input color, not a fragment processor.
        // We need to pass the input to the runtime filter using Compose. This ensures that it will
        // be invoked exactly once, and the result will be returned when null children are sampled,
        // or as the (default) input color for non-null children.
        return GrFPSuccess(GrFragmentProcessor::Compose(std::move(fp), std::move(inputFP)));
    }
#endif

    bool onAppendStages(const SkStageRec& rec, bool shaderIsOpaque) const override {
        return false;
    }

    skvm::Color onProgram(skvm::Builder* p, skvm::Color c,
                          SkColorSpace* dstCS,
                          skvm::Uniforms* uniforms, SkArenaAlloc* alloc) const override {
        sk_sp<SkData> inputs = get_xformed_uniforms(fEffect.get(), fUniforms, dstCS);
        SkASSERT(inputs);

        // There should be no way for the color filter to use device coords, but we need to supply
        // something. (Uninitialized values can trigger asserts in skvm::Builder).
        skvm::Coord zeroCoord = { p->splat(0.0f), p->splat(0.0f) };

        auto sampleChild = [&](int ix, skvm::Coord /*coord*/, skvm::Color color) {
            if (fChildren[ix]) {
                return as_CFB(fChildren[ix])->program(p, color, dstCS, uniforms, alloc);
            } else {
                return color;
            }
        };

        const size_t uniformCount = fEffect->uniformSize() / 4;
        std::vector<skvm::Val> uniform;
        uniform.reserve(uniformCount);
        for (size_t i = 0; i < uniformCount; i++) {
            int bits;
            memcpy(&bits, (const char*)inputs->data() + 4*i, 4);
            uniform.push_back(p->uniform32(uniforms->push(bits)).id);
        }

        return SkSL::ProgramToSkVM(*fEffect->fBaseProgram, fEffect->fMain, p, SkMakeSpan(uniform),
                                   /*device=*/zeroCoord, /*local=*/zeroCoord, c, sampleChild);
    }

    SkPMColor4f onFilterColor4f(const SkPMColor4f& color, SkColorSpace* dstCS) const override {
        // Get the generic program for filtering a single color
        const skvm::Program* program = fEffect->getFilterColorInfo().program;
        if (!program) {
            // We were unable to build a cached (per-effect) program. Use the base-class fallback,
            // which builds a program for the specific filter instance.
            return SkColorFilterBase::onFilterColor4f(color, dstCS);
        }

        // Get our specific uniform values
        sk_sp<SkData> inputs = get_xformed_uniforms(fEffect.get(), fUniforms, dstCS);
        SkASSERT(inputs);

        // 'program' defines sampling any child as returning a uniform color. Assemble a buffer
        // containing those colors. The first entry is always the input color. Subsequent entries
        // are for children. For any null children, the sample result is just the input color.
        // For non-null children, it's the result of that child filtering the input color.
        SkSTArray<1, SkPMColor4f, true> inputColors;
        inputColors.push_back(color);
        for (const auto &child : fChildren) {
            inputColors.push_back(child ? as_CFB(child)->onFilterColor4f(color, dstCS) : color);
        }

        SkPMColor4f result;
        program->eval(1, inputColors.begin(), inputs->data(), result.vec());
        return result;
    }

    bool onIsAlphaUnchanged() const override {
        return fEffect->getFilterColorInfo().alphaUnchanged;
    }

    void flatten(SkWriteBuffer& buffer) const override {
        buffer.writeString(fEffect->source().c_str());
        if (fUniforms) {
            buffer.writeDataAsByteArray(fUniforms.get());
        } else {
            buffer.writeByteArray(nullptr, 0);
        }
        buffer.write32(fChildren.size());
        for (const auto& child : fChildren) {
            buffer.writeFlattenable(child.get());
        }
    }

    SK_FLATTENABLE_HOOKS(SkRuntimeColorFilter)

private:
    sk_sp<SkRuntimeEffect> fEffect;
    sk_sp<SkData> fUniforms;
    std::vector<sk_sp<SkColorFilter>> fChildren;
};

sk_sp<SkFlattenable> SkRuntimeColorFilter::CreateProc(SkReadBuffer& buffer) {
    SkString sksl;
    buffer.readString(&sksl);
    sk_sp<SkData> uniforms = buffer.readByteArrayAsData();

    auto effect = SkMakeCachedRuntimeEffect(SkRuntimeEffect::MakeForColorFilter, std::move(sksl));
    if (!buffer.validate(effect != nullptr)) {
        return nullptr;
    }

    size_t childCount = buffer.read32();
    if (!buffer.validate(childCount == effect->children().count())) {
        return nullptr;
    }

    std::vector<sk_sp<SkColorFilter>> children(childCount);
    for (size_t i = 0; i < children.size(); ++i) {
        children[i] = buffer.readColorFilter();
    }

    return effect->makeColorFilter(std::move(uniforms), children.data(), children.size());
}

///////////////////////////////////////////////////////////////////////////////////////////////////

class SkRTShader : public SkShaderBase {
public:
    SkRTShader(sk_sp<SkRuntimeEffect> effect, sk_sp<SkData> uniforms, const SkMatrix* localMatrix,
               sk_sp<SkShader>* children, size_t childCount, bool isOpaque)
            : SkShaderBase(localMatrix)
            , fEffect(std::move(effect))
            , fIsOpaque(isOpaque)
            , fUniforms(std::move(uniforms))
            , fChildren(children, children + childCount) {}

    bool isOpaque() const override { return fIsOpaque; }

#if SK_SUPPORT_GPU
    std::unique_ptr<GrFragmentProcessor> asFragmentProcessor(const GrFPArgs& args) const override {
        SkMatrix matrix;
        if (!this->totalLocalMatrix(args.fPreLocalMatrix)->invert(&matrix)) {
            return nullptr;
        }

        sk_sp<SkData> uniforms =
                get_xformed_uniforms(fEffect.get(), fUniforms, args.fDstColorInfo->colorSpace());
        SkASSERT(uniforms);

        // If we sample children with explicit colors, this may not be true.
        // TODO: Determine this via analysis?
        GrFPArgs childArgs = args;
        childArgs.fInputColorIsOpaque = false;

        auto fp = GrSkSLFP::Make(fEffect, "runtime_shader", std::move(uniforms));
        for (const auto& child : fChildren) {
            auto childFP = child ? as_SB(child)->asFragmentProcessor(childArgs) : nullptr;
            fp->addChild(std::move(childFP));
        }
        std::unique_ptr<GrFragmentProcessor> result = std::move(fp);
        // If the shader was created with isOpaque = true, we *force* that result here.
        // CPU does the same thing (in SkShaderBase::program).
        if (fIsOpaque) {
            result = GrFragmentProcessor::SwizzleOutput(std::move(result), GrSwizzle::RGB1());
        }
        result = GrMatrixEffect::Make(matrix, std::move(result));
        // Three cases of GrClampType to think about:
        //   kAuto   - Normalized fixed-point. If fIsOpaque, then A is 1 (above), and the format's
        //             range ensures RGB must be no larger. If !fIsOpaque, we clamp here.
        //   kManual - Normalized floating point. Whether or not we set A above, the format's range
        //             means we need to clamp RGB.
        //   kNone   - Unclamped floating point. No clamping is done, ever.
        GrClampType clampType = GrColorTypeClampType(args.fDstColorInfo->colorType());
        if (clampType == GrClampType::kManual || (clampType == GrClampType::kAuto && !fIsOpaque)) {
            return GrFragmentProcessor::ClampPremulOutput(std::move(result));
        } else {
            return result;
        }
    }
#endif

    bool onAppendStages(const SkStageRec& rec) const override {
        return false;
    }

    skvm::Color onProgram(skvm::Builder* p,
                          skvm::Coord device, skvm::Coord local, skvm::Color paint,
                          const SkMatrixProvider& matrices, const SkMatrix* localM,
                          const SkColorInfo& dst,
                          skvm::Uniforms* uniforms, SkArenaAlloc* alloc) const override {
        sk_sp<SkData> inputs = get_xformed_uniforms(fEffect.get(), fUniforms, dst.colorSpace());
        SkASSERT(inputs);

        SkMatrix inv;
        if (!this->computeTotalInverse(matrices.localToDevice(), localM, &inv)) {
            return {};
        }
        local = SkShaderBase::ApplyMatrix(p,inv,local,uniforms);

        auto sampleChild = [&](int ix, skvm::Coord coord, skvm::Color color) {
            if (fChildren[ix]) {
                SkOverrideDeviceMatrixProvider mats{matrices, SkMatrix::I()};
                return as_SB(fChildren[ix])->program(p, device, coord, color,
                                                     mats, nullptr, dst,
                                                     uniforms, alloc);
            } else {
                return color;
            }
        };

        const size_t uniformCount = fEffect->uniformSize() / 4;
        std::vector<skvm::Val> uniform;
        uniform.reserve(uniformCount);
        for (size_t i = 0; i < uniformCount; i++) {
            int bits;
            memcpy(&bits, (const char*)inputs->data() + 4*i, 4);
            uniform.push_back(p->uniform32(uniforms->push(bits)).id);
        }

        return SkSL::ProgramToSkVM(*fEffect->fBaseProgram, fEffect->fMain, p, SkMakeSpan(uniform),
                                   device, local, paint, sampleChild);
    }

    void flatten(SkWriteBuffer& buffer) const override {
        uint32_t flags = 0;
        if (fIsOpaque) {
            flags |= kIsOpaque_Flag;
        }
        if (!this->getLocalMatrix().isIdentity()) {
            flags |= kHasLocalMatrix_Flag;
        }

        buffer.writeString(fEffect->source().c_str());
        if (fUniforms) {
            buffer.writeDataAsByteArray(fUniforms.get());
        } else {
            buffer.writeByteArray(nullptr, 0);
        }
        buffer.write32(flags);
        if (flags & kHasLocalMatrix_Flag) {
            buffer.writeMatrix(this->getLocalMatrix());
        }
        buffer.write32(fChildren.size());
        for (const auto& child : fChildren) {
            buffer.writeFlattenable(child.get());
        }
    }

    SkRuntimeEffect* asRuntimeEffect() const override { return fEffect.get(); }

    SK_FLATTENABLE_HOOKS(SkRTShader)

private:
    enum Flags {
        kIsOpaque_Flag          = 1 << 0,
        kHasLocalMatrix_Flag    = 1 << 1,
    };

    sk_sp<SkRuntimeEffect> fEffect;
    bool fIsOpaque;

    sk_sp<SkData> fUniforms;
    std::vector<sk_sp<SkShader>> fChildren;
};

sk_sp<SkFlattenable> SkRTShader::CreateProc(SkReadBuffer& buffer) {
    SkString sksl;
    buffer.readString(&sksl);
    sk_sp<SkData> uniforms = buffer.readByteArrayAsData();
    uint32_t flags = buffer.read32();

    bool isOpaque = SkToBool(flags & kIsOpaque_Flag);
    SkMatrix localM, *localMPtr = nullptr;
    if (flags & kHasLocalMatrix_Flag) {
        buffer.readMatrix(&localM);
        localMPtr = &localM;
    }

    auto effect = SkMakeCachedRuntimeEffect(SkRuntimeEffect::MakeForShader, std::move(sksl));
    if (!buffer.validate(effect != nullptr)) {
        return nullptr;
    }

    size_t childCount = buffer.read32();
    if (!buffer.validate(childCount == effect->children().count())) {
        return nullptr;
    }

    std::vector<sk_sp<SkShader>> children(childCount);
    for (size_t i = 0; i < children.size(); ++i) {
        children[i] = buffer.readShader();
    }

    return effect->makeShader(std::move(uniforms), children.data(), children.size(), localMPtr,
                              isOpaque);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

#if SK_SUPPORT_GPU
std::unique_ptr<GrFragmentProcessor> SkRuntimeEffect::makeFP(
        sk_sp<SkData> uniforms,
        std::unique_ptr<GrFragmentProcessor> children[],
        size_t childCount) const {
    if (!uniforms) {
        uniforms = SkData::MakeEmpty();
    }
    auto fp = GrSkSLFP::Make(sk_ref_sp(this), "make_fp", std::move(uniforms));
    for (size_t i = 0; i < childCount; ++i) {
        fp->addChild(std::move(children[i]));
    }
    return std::move(fp);
}
#endif

sk_sp<SkShader> SkRuntimeEffect::makeShader(sk_sp<SkData> uniforms,
                                            sk_sp<SkShader> children[],
                                            size_t childCount,
                                            const SkMatrix* localMatrix,
                                            bool isOpaque) const {
    if (!this->allowShader()) {
        return nullptr;
    }
    if (!uniforms) {
        uniforms = SkData::MakeEmpty();
    }
    // Verify that all child objects are shaders (to match the C++ types here).
    // TODO(skia:11813) When we support shader and colorFilter children (with different samplng
    // semantics), the 'children' parameter will contain both types, so this will be more complex.
    if (!std::all_of(fChildren.begin(), fChildren.end(), [](const Child& c) {
            return c.type == Child::Type::kShader;
        })) {
        return nullptr;
    }
    return uniforms->size() == this->uniformSize() && childCount == fChildren.size()
                   ? sk_sp<SkShader>(new SkRTShader(sk_ref_sp(this),
                                                    std::move(uniforms),
                                                    localMatrix,
                                                    children,
                                                    childCount,
                                                    isOpaque))
                   : nullptr;
}

sk_sp<SkImage> SkRuntimeEffect::makeImage(GrRecordingContext* recordingContext,
                                          sk_sp<SkData> uniforms,
                                          sk_sp<SkShader> children[],
                                          size_t childCount,
                                          const SkMatrix* localMatrix,
                                          SkImageInfo resultInfo,
                                          bool mipmapped) const {
    if (recordingContext) {
#if SK_SUPPORT_GPU
        if (!recordingContext->priv().caps()->mipmapSupport()) {
            mipmapped = false;
        }
        auto fillContext = GrSurfaceFillContext::Make(recordingContext,
                                                      resultInfo,
                                                      SkBackingFit::kExact,
                                                      /*sample count*/ 1,
                                                      GrMipmapped(mipmapped));
        if (!fillContext) {
            return nullptr;
        }
        uniforms = get_xformed_uniforms(this, std::move(uniforms), resultInfo.colorSpace());
        SkASSERT(uniforms);

        auto fp = GrSkSLFP::Make(sk_ref_sp(this),
                                 "runtime_image",
                                 std::move(uniforms));
        SkSimpleMatrixProvider matrixProvider(SkMatrix::I());
        GrColorInfo colorInfo(resultInfo.colorInfo());
        GrFPArgs args(recordingContext, matrixProvider, &colorInfo);
        for (size_t i = 0; i < childCount; ++i) {
            if (!children[i]) {
                return nullptr;
            }
            auto childFP = as_SB(children[i])->asFragmentProcessor(args);
            fp->addChild(std::move(childFP));
        }
        if (localMatrix) {
            SkMatrix invLM;
            if (!localMatrix->invert(&invLM)) {
                return nullptr;
            }
            fillContext->fillWithFP(invLM, std::move(fp));
        } else {
            fillContext->fillWithFP(std::move(fp));
        }
        return sk_sp<SkImage>(new SkImage_Gpu(sk_ref_sp(recordingContext),
                                              kNeedNewImageUniqueID,
                                              fillContext->readSurfaceView(),
                                              resultInfo.colorInfo()));
#else
        return nullptr;
#endif
    }
    if (resultInfo.alphaType() == kUnpremul_SkAlphaType) {
        // We don't have a good way of supporting this right now. In this case the runtime effect
        // will produce a unpremul value. The shader generated from it is assumed to produce
        // premul and RGB get pinned to A. Moreover, after the blend in premul the new dst is
        // unpremul'ed, producing a double unpremul result.
        return nullptr;
    }
    auto surf = SkSurface::MakeRaster(resultInfo);
    if (!surf) {
        return nullptr;
    }
    SkCanvas* canvas = surf->getCanvas();
    SkTLazy<SkCanvas> tempCanvas;
    auto shader = this->makeShader(std::move(uniforms), children, childCount, localMatrix, false);
    if (!shader) {
        return nullptr;
    }
    SkPaint paint;
    paint.setShader(std::move(shader));
    paint.setBlendMode(SkBlendMode::kSrc);
    canvas->drawPaint(paint);
    // TODO: Specify snapshot should have mip levels if mipmapped is true.
    return surf->makeImageSnapshot();
}

sk_sp<SkColorFilter> SkRuntimeEffect::makeColorFilter(sk_sp<SkData> uniforms,
                                                      sk_sp<SkColorFilter> children[],
                                                      size_t childCount) const {
    if (!this->allowColorFilter()) {
        return nullptr;
    }
    if (!uniforms) {
        uniforms = SkData::MakeEmpty();
    }
    // Verify that all child objects are color filters (to match the C++ types here).
    // TODO(skia:11813) When we support shader and colorFilter children (with different samplng
    // semantics), the 'children' parameter will contain both types, so this will be more complex.
    if (!std::all_of(fChildren.begin(), fChildren.end(), [](const Child& c) {
            return c.type == Child::Type::kColorFilter;
        })) {
        return nullptr;
    }
    return uniforms->size() == this->uniformSize() && childCount == fChildren.size()
                   ? sk_sp<SkColorFilter>(new SkRuntimeColorFilter(
                             sk_ref_sp(this), std::move(uniforms), children, childCount))
                   : nullptr;
}

sk_sp<SkColorFilter> SkRuntimeEffect::makeColorFilter(sk_sp<SkData> uniforms) const {
    return this->makeColorFilter(std::move(uniforms), nullptr, 0);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void SkRuntimeEffect::RegisterFlattenables() {
    SK_REGISTER_FLATTENABLE(SkRuntimeColorFilter);
    SK_REGISTER_FLATTENABLE(SkRTShader);
}

SkRuntimeShaderBuilder::SkRuntimeShaderBuilder(sk_sp<SkRuntimeEffect> effect)
        : INHERITED(std::move(effect)) {}

SkRuntimeShaderBuilder::~SkRuntimeShaderBuilder() = default;

sk_sp<SkImage> SkRuntimeShaderBuilder::makeImage(GrRecordingContext* recordingContext,
                                                 const SkMatrix* localMatrix,
                                                 SkImageInfo resultInfo,
                                                 bool mipmapped) {
    return this->effect()->makeImage(recordingContext,
                                     this->uniforms(),
                                     this->children(),
                                     this->numChildren(),
                                     localMatrix,
                                     resultInfo,
                                     mipmapped);
}

sk_sp<SkShader> SkRuntimeShaderBuilder::makeShader(const SkMatrix* localMatrix, bool isOpaque) {
    return this->effect()->makeShader(this->uniforms(),
                                      this->children(),
                                      this->numChildren(),
                                      localMatrix,
                                      isOpaque);
}
