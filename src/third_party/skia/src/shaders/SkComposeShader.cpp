/*
 * Copyright 2006 The Android Open Source Project
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "include/core/SkColorFilter.h"
#include "include/core/SkString.h"
#include "include/private/SkColorData.h"
#include "src/core/SkArenaAlloc.h"
#include "src/core/SkBlendModePriv.h"
#include "src/core/SkRasterPipeline.h"
#include "src/core/SkReadBuffer.h"
#include "src/core/SkRuntimeEffectPriv.h"
#include "src/core/SkVM.h"
#include "src/core/SkWriteBuffer.h"
#include "src/shaders/SkColorShader.h"
#include "src/shaders/SkComposeShader.h"

namespace {

struct LocalMatrixStageRec final : public SkStageRec {
    LocalMatrixStageRec(const SkStageRec& rec, const SkMatrix& lm)
        : INHERITED(rec) {
        if (!lm.isIdentity()) {
            if (fLocalM) {
                fStorage.setConcat(lm, *fLocalM);
                fLocalM = fStorage.isIdentity() ? nullptr : &fStorage;
            } else {
                fLocalM = &lm;
            }
        }
    }

private:
    SkMatrix fStorage;

    using INHERITED = SkStageRec;
};

} // namespace

sk_sp<SkShader> SkShaders::Blend(SkBlendMode mode, sk_sp<SkShader> dst, sk_sp<SkShader> src) {
    switch (mode) {
        case SkBlendMode::kClear: return Color(0);
        case SkBlendMode::kDst:   return dst;
        case SkBlendMode::kSrc:   return src;
        default: break;
    }
    return sk_sp<SkShader>(new SkShader_Blend(mode, std::move(dst), std::move(src)));
}

sk_sp<SkShader> SkShaders::Lerp(float weight, sk_sp<SkShader> dst, sk_sp<SkShader> src) {
    if (SkScalarIsNaN(weight)) {
        return nullptr;
    }
    if (dst == src || weight <= 0) {
        return dst;
    }
    if (weight >= 1) {
        return src;
    }

    sk_sp<SkRuntimeEffect> effect = SkMakeCachedRuntimeEffect(
        SkRuntimeEffect::MakeForShader,
        "uniform shader a;"
        "uniform shader b;"
        "uniform half w;"
        "half4 main(float2 xy) { return mix(sample(a, xy), sample(b, xy), w); }"
    );
    SkASSERT(effect);

    sk_sp<SkShader> inputs[] = {dst, src};
    return effect->makeShader(SkData::MakeWithCopy(&weight, sizeof(weight)),
                              inputs,
                              SK_ARRAY_COUNT(inputs),
                              nullptr,
                              false);
}

///////////////////////////////////////////////////////////////////////////////

static bool append_shader_or_paint(const SkStageRec& rec, SkShader* shader) {
    if (shader) {
        if (!as_SB(shader)->appendStages(rec)) {
            return false;
        }
    } else {
        rec.fPipeline->append_constant_color(rec.fAlloc, rec.fPaint.getColor4f().premul().vec());
    }
    return true;
}

// Returns the output of e0, and leaves the output of e1 in r,g,b,a
static float* append_two_shaders(const SkStageRec& rec, SkShader* s0, SkShader* s1) {
    struct Storage {
        float   fRes0[4 * SkRasterPipeline_kMaxStride];
    };
    auto storage = rec.fAlloc->make<Storage>();

    if (!append_shader_or_paint(rec, s0)) {
        return nullptr;
    }
    rec.fPipeline->append(SkRasterPipeline::store_src, storage->fRes0);

    if (!append_shader_or_paint(rec, s1)) {
        return nullptr;
    }
    return storage->fRes0;
}

///////////////////////////////////////////////////////////////////////////////

sk_sp<SkFlattenable> SkShader_Blend::CreateProc(SkReadBuffer& buffer) {
    sk_sp<SkShader> dst(buffer.readShader());
    sk_sp<SkShader> src(buffer.readShader());
    unsigned        mode = buffer.read32();

    // check for valid mode before we cast to the enum type
    if (!buffer.validate(mode <= (unsigned)SkBlendMode::kLastMode)) {
        return nullptr;
    }
    return SkShaders::Blend(static_cast<SkBlendMode>(mode), std::move(dst), std::move(src));
}

void SkShader_Blend::flatten(SkWriteBuffer& buffer) const {
    buffer.writeFlattenable(fDst.get());
    buffer.writeFlattenable(fSrc.get());
    buffer.write32((int)fMode);
}

bool SkShader_Blend::onAppendStages(const SkStageRec& orig_rec) const {
    const LocalMatrixStageRec rec(orig_rec, this->getLocalMatrix());

    float* res0 = append_two_shaders(rec, fDst.get(), fSrc.get());
    if (!res0) {
        return false;
    }

    rec.fPipeline->append(SkRasterPipeline::load_dst, res0);
    SkBlendMode_AppendStages(fMode, rec.fPipeline);
    return true;
}

static skvm::Color program_or_paint(const sk_sp<SkShader>& sh, skvm::Builder* p,
                                    skvm::Coord device, skvm::Coord local, skvm::Color paint,
                                    const SkMatrixProvider& mats, const SkMatrix* localM,
                                    const SkColorInfo& dst,
                                    skvm::Uniforms* uniforms, SkArenaAlloc* alloc) {
    return sh ? as_SB(sh)->program(p, device,local, paint, mats,localM, dst, uniforms,alloc)
              : p->premul(paint);
}

skvm::Color SkShader_Blend::onProgram(skvm::Builder* p,
                                      skvm::Coord device, skvm::Coord local, skvm::Color paint,
                                      const SkMatrixProvider& mats, const SkMatrix* localM,
                                      const SkColorInfo& dst,
                                      skvm::Uniforms* uniforms, SkArenaAlloc* alloc) const {
    skvm::Color d,s;
    if ((d = program_or_paint(fDst, p, device,local, paint, mats,localM, dst, uniforms,alloc)) &&
        (s = program_or_paint(fSrc, p, device,local, paint, mats,localM, dst, uniforms,alloc)))
    {
        return p->blend(fMode, s,d);
    }
    return {};
}

#if SK_SUPPORT_GPU

#include "include/gpu/GrRecordingContext.h"
#include "src/gpu/GrFragmentProcessor.h"
#include "src/gpu/effects/GrBlendFragmentProcessor.h"

static std::unique_ptr<GrFragmentProcessor> as_fp(const GrFPArgs& args, SkShader* shader) {
    return shader ? as_SB(shader)->asFragmentProcessor(args) : nullptr;
}

std::unique_ptr<GrFragmentProcessor> SkShader_Blend::asFragmentProcessor(
        const GrFPArgs& orig_args) const {
    const GrFPArgs::WithPreLocalMatrix args(orig_args, this->getLocalMatrix());
    auto fpA = as_fp(args, fDst.get());
    auto fpB = as_fp(args, fSrc.get());
    return GrBlendFragmentProcessor::Make(std::move(fpB), std::move(fpA), fMode);
}
#endif
