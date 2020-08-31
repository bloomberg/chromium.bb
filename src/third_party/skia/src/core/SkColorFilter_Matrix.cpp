/*
 * Copyright 2011 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "include/core/SkRefCnt.h"
#include "include/core/SkString.h"
#include "include/core/SkUnPreMultiply.h"
#include "include/effects/SkColorMatrix.h"
#include "include/private/SkColorData.h"
#include "include/private/SkNx.h"
#include "src/core/SkColorFilter_Matrix.h"
#include "src/core/SkColorSpacePriv.h"
#include "src/core/SkRasterPipeline.h"
#include "src/core/SkReadBuffer.h"
#include "src/core/SkVM.h"
#include "src/core/SkWriteBuffer.h"

static uint16_t ComputeFlags(const float matrix[20]) {
    const float* srcA = matrix + 15;

    return SkScalarNearlyZero (srcA[0])
        && SkScalarNearlyZero (srcA[1])
        && SkScalarNearlyZero (srcA[2])
        && SkScalarNearlyEqual(srcA[3], 1)
        && SkScalarNearlyZero (srcA[4])
            ? SkColorFilter::kAlphaUnchanged_Flag : 0;
}

SkColorFilter_Matrix::SkColorFilter_Matrix(const float array[20], Domain domain)
    : fFlags(ComputeFlags(array))
    , fDomain(domain) {
    memcpy(fMatrix, array, 20 * sizeof(float));
}

uint32_t SkColorFilter_Matrix::getFlags() const {
    return this->INHERITED::getFlags() | fFlags;
}

void SkColorFilter_Matrix::flatten(SkWriteBuffer& buffer) const {
    SkASSERT(sizeof(fMatrix)/sizeof(float) == 20);
    buffer.writeScalarArray(fMatrix, 20);

    // RGBA flag
    buffer.writeBool(fDomain == Domain::kRGBA);
}

sk_sp<SkFlattenable> SkColorFilter_Matrix::CreateProc(SkReadBuffer& buffer) {
    float matrix[20];
    if (!buffer.readScalarArray(matrix, 20)) {
        return nullptr;
    }

    auto   is_rgba = buffer.isVersionLT(SkPicturePriv::kMatrixColorFilterDomain_Version) ||
                     buffer.readBool();
    return is_rgba ? SkColorFilters::Matrix(matrix)
                   : SkColorFilters::HSLAMatrix(matrix);
}

bool SkColorFilter_Matrix::onAsAColorMatrix(float matrix[20]) const {
    if (matrix) {
        memcpy(matrix, fMatrix, 20 * sizeof(float));
    }
    return true;
}

bool SkColorFilter_Matrix::onAppendStages(const SkStageRec& rec, bool shaderIsOpaque) const {
    const bool willStayOpaque = shaderIsOpaque && (fFlags & kAlphaUnchanged_Flag),
                         hsla = fDomain == Domain::kHSLA;

    SkRasterPipeline* p = rec.fPipeline;
    if (!shaderIsOpaque) { p->append(SkRasterPipeline::unpremul); }
    if (           hsla) { p->append(SkRasterPipeline::rgb_to_hsl); }
    if (           true) { p->append(SkRasterPipeline::matrix_4x5, fMatrix); }
    if (           hsla) { p->append(SkRasterPipeline::hsl_to_rgb); }
    if (           true) { p->append(SkRasterPipeline::clamp_0); }
    if (           true) { p->append(SkRasterPipeline::clamp_1); }
    if (!willStayOpaque) { p->append(SkRasterPipeline::premul); }
    return true;
}


skvm::Color SkColorFilter_Matrix::onProgram(skvm::Builder* p, skvm::Color c,
                                            SkColorSpace* /*dstCS*/,
                                            skvm::Uniforms* uniforms, SkArenaAlloc*) const {
    auto apply_matrix = [&](auto xyzw) {
        auto dot = [&](int j) {
            auto custom_mad = [&](float f, skvm::F32 m, skvm::F32 a) {
                // skvm::Builder won't fold f*0 == 0, but we shouldn't encounter NaN here.
                // While looking, also simplify f == ±1.  Anything else becomes a uniform.
                return f ==  0.0f ? a
                     : f == +1.0f ? a + m
                     : f == -1.0f ? a - m
                     : m * p->uniformF(uniforms->pushF(f)) + a;
            };

            // Similarly, let skvm::Builder fold away the additive bias when zero.
            const float b = fMatrix[4+j*5];
            skvm::F32 bias = b == 0.0f ? p->splat(0.0f)
                                       : p->uniformF(uniforms->pushF(b));

            auto [x,y,z,w] = xyzw;
            return custom_mad(fMatrix[0+j*5], x,
                   custom_mad(fMatrix[1+j*5], y,
                   custom_mad(fMatrix[2+j*5], z,
                   custom_mad(fMatrix[3+j*5], w, bias))));
        };
        return std::make_tuple(dot(0), dot(1), dot(2), dot(3));
    };

    c = unpremul(c);

    if (fDomain == Domain::kHSLA) {
        auto [h,s,l,a] = apply_matrix(p->to_hsla(c));
        c = p->to_rgba({h,s,l,a});
    } else {
        auto [r,g,b,a] = apply_matrix(c);
        c = {r,g,b,a};
    }

    return premul(c);    // note: rasterpipeline version does clamp01 first
}

#if SK_SUPPORT_GPU
#include "src/gpu/effects/generated/GrColorMatrixFragmentProcessor.h"
#include "src/gpu/effects/generated/GrHSLToRGBFilterEffect.h"
#include "src/gpu/effects/generated/GrRGBToHSLFilterEffect.h"
std::unique_ptr<GrFragmentProcessor> SkColorFilter_Matrix::asFragmentProcessor(
        GrRecordingContext*, const GrColorInfo&) const {
    switch (fDomain) {
        case Domain::kRGBA:
            return GrColorMatrixFragmentProcessor::Make(fMatrix,
                                                        /* premulInput = */    true,
                                                        /* clampRGBOutput = */ true,
                                                        /* premulOutput = */   true);
        case Domain::kHSLA: {
            std::unique_ptr<GrFragmentProcessor> series[] = {
                GrRGBToHSLFilterEffect::Make(),
                GrColorMatrixFragmentProcessor::Make(fMatrix,
                                                     /* premulInput = */    false,
                                                     /* clampRGBOutput = */ false,
                                                     /* premulOutput = */   false),
                GrHSLToRGBFilterEffect::Make(),
            };
            return GrFragmentProcessor::RunInSeries(series, SK_ARRAY_COUNT(series));
        }
    }

    SkUNREACHABLE;
}

#endif

///////////////////////////////////////////////////////////////////////////////

static sk_sp<SkColorFilter> MakeMatrix(const float array[20],
                                       SkColorFilter_Matrix::Domain domain) {
    return sk_floats_are_finite(array, 20)
        ? sk_make_sp<SkColorFilter_Matrix>(array, domain)
        : nullptr;
}

sk_sp<SkColorFilter> SkColorFilters::Matrix(const float array[20]) {
    return MakeMatrix(array, SkColorFilter_Matrix::Domain::kRGBA);
}

sk_sp<SkColorFilter> SkColorFilters::Matrix(const SkColorMatrix& cm) {
    return MakeMatrix(cm.fMat.data(), SkColorFilter_Matrix::Domain::kRGBA);
}

sk_sp<SkColorFilter> SkColorFilters::HSLAMatrix(const float array[20]) {
    return MakeMatrix(array, SkColorFilter_Matrix::Domain::kHSLA);
}

void SkColorFilter_Matrix::RegisterFlattenables() {
    SK_REGISTER_FLATTENABLE(SkColorFilter_Matrix);

    // This subclass was removed 4/2019
    SkFlattenable::Register("SkColorMatrixFilterRowMajor255",
                            [](SkReadBuffer& buffer) -> sk_sp<SkFlattenable> {
        float matrix[20];
        if (buffer.readScalarArray(matrix, 20)) {
            matrix[ 4] *= (1.0f/255);
            matrix[ 9] *= (1.0f/255);
            matrix[14] *= (1.0f/255);
            matrix[19] *= (1.0f/255);
            return SkColorFilters::Matrix(matrix);
        }
        return nullptr;
    });
}
