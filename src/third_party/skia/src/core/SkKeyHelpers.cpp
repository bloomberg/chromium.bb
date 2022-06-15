/*
 * Copyright 2022 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "src/core/SkKeyHelpers.h"

#include "include/core/SkCombinationBuilder.h"
#include "src/core/SkDebugUtils.h"
#include "src/core/SkKeyContext.h"
#include "src/core/SkPaintParamsKey.h"
#include "src/core/SkPipelineData.h"
#include "src/core/SkShaderCodeDictionary.h"
#include "src/core/SkUniform.h"

#ifdef SK_GRAPHITE_ENABLED
#include "src/gpu/Blend.h"
#include "src/gpu/graphite/Texture.h"
#include "src/gpu/graphite/TextureProxy.h"
#include "src/gpu/graphite/UniformManager.h"
#endif

#define VALIDATE_UNIFORMS(gatherer, dict, codeSnippetID) \
    SkDEBUGCODE(UniformExpectationsValidator uev(gatherer, dict->getUniforms(codeSnippetID));)

constexpr SkPMColor4f kErrorColor = { 1, 0, 0, 1 };

//--------------------------------------------------------------------------------------------------

#ifdef SK_GRAPHITE_ENABLED

namespace {

void add_solid_uniform_data(const SkShaderCodeDictionary* dict,
                            const SkPMColor4f& premulColor,
                            SkPipelineDataGatherer* gatherer) {
    VALIDATE_UNIFORMS(gatherer, dict, SkBuiltInCodeSnippetID::kSolidColorShader)
    gatherer->write(premulColor);

    gatherer->addFlags(dict->getSnippetRequirementFlags(SkBuiltInCodeSnippetID::kSolidColorShader));
}

} // anonymous namespace

#endif // SK_GRAPHITE_ENABLED

void SolidColorShaderBlock::BeginBlock(const SkKeyContext& keyContext,
                                       SkPaintParamsKeyBuilder* builder,
                                       SkPipelineDataGatherer* gatherer,
                                       const SkPMColor4f& premulColor) {

#ifdef SK_GRAPHITE_ENABLED
    if (builder->backend() == SkBackend::kGraphite) {
        if (gatherer) {
            auto dict = keyContext.dict();

            add_solid_uniform_data(dict, premulColor, gatherer);
        }

        builder->beginBlock(SkBuiltInCodeSnippetID::kSolidColorShader);
        return;
    }
#endif // SK_GRAPHITE_ENABLED

    if (builder->backend() == SkBackend::kSkVM || builder->backend() == SkBackend::kGanesh) {
        // TODO: add implementation of other backends
    }

}

//--------------------------------------------------------------------------------------------------

#ifdef SK_GRAPHITE_ENABLED

namespace {

void add_linear_gradient_uniform_data(const SkShaderCodeDictionary* dict,
                                      SkBuiltInCodeSnippetID codeSnippetID,
                                      const GradientShaderBlocks::GradientData& gradData,
                                      SkPipelineDataGatherer* gatherer) {
    VALIDATE_UNIFORMS(gatherer, dict, codeSnippetID)
    int stops = codeSnippetID == SkBuiltInCodeSnippetID::kLinearGradientShader4 ? 4 : 8;

    SkM44 lmInverse;
    bool wasInverted = gradData.fLocalMatrix.invert(&lmInverse);  // TODO: handle failure up stack
    if (!wasInverted) {
        lmInverse.setIdentity();
    }

    gatherer->write(lmInverse);
    gatherer->write(gradData.fColor4fs, stops);
    gatherer->write(gradData.fOffsets, stops);
    gatherer->write(gradData.fPoints[0]);
    gatherer->write(gradData.fPoints[1]);
    gatherer->write(static_cast<int>(gradData.fTM));
    gatherer->write(0.0f);  // padding
    gatherer->write(0.0f);
    gatherer->write(0.0f);

    gatherer->addFlags(dict->getSnippetRequirementFlags(codeSnippetID));
};

void add_radial_gradient_uniform_data(const SkShaderCodeDictionary* dict,
                                      SkBuiltInCodeSnippetID codeSnippetID,
                                      const GradientShaderBlocks::GradientData& gradData,
                                      SkPipelineDataGatherer* gatherer) {
    VALIDATE_UNIFORMS(gatherer, dict, codeSnippetID)
    int stops = codeSnippetID == SkBuiltInCodeSnippetID::kRadialGradientShader4 ? 4 : 8;

    SkM44 lmInverse;
    bool wasInverted = gradData.fLocalMatrix.invert(&lmInverse);  // TODO: handle failure up stack
    if (!wasInverted) {
        lmInverse.setIdentity();
    }

    gatherer->write(lmInverse);
    gatherer->write(gradData.fColor4fs, stops);
    gatherer->write(gradData.fOffsets, stops);
    gatherer->write(gradData.fPoints[0]);
    gatherer->write(gradData.fRadii[0]);
    gatherer->write(static_cast<int>(gradData.fTM));

    gatherer->addFlags(dict->getSnippetRequirementFlags(codeSnippetID));
};

void add_sweep_gradient_uniform_data(const SkShaderCodeDictionary* dict,
                                     SkBuiltInCodeSnippetID codeSnippetID,
                                     const GradientShaderBlocks::GradientData& gradData,
                                     SkPipelineDataGatherer* gatherer) {
    VALIDATE_UNIFORMS(gatherer, dict, codeSnippetID)
    int stops = codeSnippetID == SkBuiltInCodeSnippetID::kSweepGradientShader4 ? 4 : 8;

    SkM44 lmInverse;
    bool wasInverted = gradData.fLocalMatrix.invert(&lmInverse);  // TODO: handle failure up stack
    if (!wasInverted) {
        lmInverse.setIdentity();
    }

    gatherer->write(lmInverse);
    gatherer->write(gradData.fColor4fs, stops);
    gatherer->write(gradData.fOffsets, stops);
    gatherer->write(gradData.fPoints[0]);
    gatherer->write(gradData.fBias);
    gatherer->write(gradData.fScale);
    gatherer->write(static_cast<int>(gradData.fTM));
    gatherer->write(0.0f);  // padding
    gatherer->write(0.0f);
    gatherer->write(0.0f);

    gatherer->addFlags(dict->getSnippetRequirementFlags(codeSnippetID));
};

void add_conical_gradient_uniform_data(const SkShaderCodeDictionary* dict,
                                       SkBuiltInCodeSnippetID codeSnippetID,
                                       const GradientShaderBlocks::GradientData& gradData,
                                       SkPipelineDataGatherer* gatherer) {
    VALIDATE_UNIFORMS(gatherer, dict, codeSnippetID)
    int stops = codeSnippetID == SkBuiltInCodeSnippetID::kConicalGradientShader4 ? 4 : 8;

    SkM44 lmInverse;
    bool wasInverted = gradData.fLocalMatrix.invert(&lmInverse);  // TODO: handle failure up stack
    if (!wasInverted) {
        lmInverse.setIdentity();
    }

    gatherer->write(lmInverse);
    gatherer->write(gradData.fColor4fs, stops);
    gatherer->write(gradData.fOffsets, stops);
    gatherer->write(gradData.fPoints[0]);
    gatherer->write(gradData.fPoints[1]);
    gatherer->write(gradData.fRadii[0]);
    gatherer->write(gradData.fRadii[1]);
    gatherer->write(static_cast<int>(gradData.fTM));
    gatherer->write(0.0f);  // padding

    gatherer->addFlags(dict->getSnippetRequirementFlags(codeSnippetID));
};

} // anonymous namespace

#endif // SK_GRAPHITE_ENABLED

GradientShaderBlocks::GradientData::GradientData(SkShader::GradientType type,
                                                 int numStops)
        : fType(type)
        , fPoints{{0.0f, 0.0f}, {0.0f, 0.0f}}
        , fRadii{0.0f, 0.0f}
        , fBias(0.0f)
        , fScale(0.0f)
        , fTM(SkTileMode::kClamp)
        , fNumStops(numStops) {
    sk_bzero(fColor4fs, sizeof(fColor4fs));
    sk_bzero(fOffsets, sizeof(fOffsets));
}

GradientShaderBlocks::GradientData::GradientData(SkShader::GradientType type,
                                                 const SkM44& localMatrix,
                                                 SkPoint point0, SkPoint point1,
                                                 float radius0, float radius1,
                                                 float bias, float scale,
                                                 SkTileMode tm,
                                                 int numStops,
                                                 SkColor4f* color4fs,
                                                 float* offsets)
        : fType(type)
        , fLocalMatrix(localMatrix)
        , fBias(bias)
        , fScale(scale)
        , fTM(tm)
        , fNumStops(std::min(numStops, kMaxStops)) {
    SkASSERT(fNumStops >= 1);

    fPoints[0] = point0;
    fPoints[1] = point1;
    fRadii[0] = radius0;
    fRadii[1] = radius1;
    memcpy(fColor4fs, color4fs, fNumStops * sizeof(SkColor4f));
    if (offsets) {
        memcpy(fOffsets, offsets, fNumStops * sizeof(float));
    } else {
        for (int i = 0; i < fNumStops; ++i) {
            fOffsets[i] = SkIntToFloat(i) / (fNumStops-1);
        }
    }

    // Extend the colors and offset, if necessary, to fill out the arrays
    // TODO: this should be done later when the actual code snippet has been selected!!
    for (int i = fNumStops ; i < kMaxStops; ++i) {
        fColor4fs[i] = fColor4fs[fNumStops-1];
        fOffsets[i] = fOffsets[fNumStops-1];
    }
}

void GradientShaderBlocks::BeginBlock(const SkKeyContext& keyContext,
                                      SkPaintParamsKeyBuilder *builder,
                                      SkPipelineDataGatherer* gatherer,
                                      const GradientData& gradData) {

#ifdef SK_GRAPHITE_ENABLED
    if (builder->backend() == SkBackend::kGraphite) {
        auto dict = keyContext.dict();
        SkBuiltInCodeSnippetID codeSnippetID = SkBuiltInCodeSnippetID::kSolidColorShader;
        switch (gradData.fType) {
            case SkShader::kLinear_GradientType:
                codeSnippetID = gradData.fNumStops <= 4
                                        ? SkBuiltInCodeSnippetID::kLinearGradientShader4
                                        : SkBuiltInCodeSnippetID::kLinearGradientShader8;
                if (gatherer) {
                    add_linear_gradient_uniform_data(dict, codeSnippetID, gradData, gatherer);
                }
                break;
            case SkShader::kRadial_GradientType:
                codeSnippetID = gradData.fNumStops <= 4
                                        ? SkBuiltInCodeSnippetID::kRadialGradientShader4
                                        : SkBuiltInCodeSnippetID::kRadialGradientShader8;
                if (gatherer) {
                    add_radial_gradient_uniform_data(dict, codeSnippetID, gradData, gatherer);
                }
                break;
            case SkShader::kSweep_GradientType:
                codeSnippetID = gradData.fNumStops <= 4
                                        ? SkBuiltInCodeSnippetID::kSweepGradientShader4
                                        : SkBuiltInCodeSnippetID::kSweepGradientShader8;
                if (gatherer) {
                    add_sweep_gradient_uniform_data(dict, codeSnippetID, gradData, gatherer);
                }
                break;
            case SkShader::GradientType::kConical_GradientType:
                codeSnippetID = gradData.fNumStops <= 4
                                        ? SkBuiltInCodeSnippetID::kConicalGradientShader4
                                        : SkBuiltInCodeSnippetID::kConicalGradientShader8;
                if (gatherer) {
                    add_conical_gradient_uniform_data(dict, codeSnippetID, gradData, gatherer);
                }
                break;
            case SkShader::GradientType::kColor_GradientType:
            case SkShader::GradientType::kNone_GradientType:
            default:
                SkASSERT(0);
                break;
        }

        builder->beginBlock(codeSnippetID);
        return;
    }
#endif // SK_GRAPHITE_ENABLED

    if (builder->backend() == SkBackend::kSkVM || builder->backend() == SkBackend::kGanesh) {
        // TODO: add implementation of other backends
        SolidColorShaderBlock::BeginBlock(keyContext, builder, gatherer, kErrorColor);
    }
}

//--------------------------------------------------------------------------------------------------

#ifdef SK_GRAPHITE_ENABLED

namespace {

void add_localmatrixshader_uniform_data(const SkShaderCodeDictionary* dict,
                                        const SkM44& localMatrix,
                                        SkPipelineDataGatherer* gatherer) {
    VALIDATE_UNIFORMS(gatherer, dict, SkBuiltInCodeSnippetID::kLocalMatrixShader)

    SkM44 lmInverse;
    bool wasInverted = localMatrix.invert(&lmInverse);  // TODO: handle failure up stack
    if (!wasInverted) {
        lmInverse.setIdentity();
    }

    gatherer->write(lmInverse);

    gatherer->addFlags(
            dict->getSnippetRequirementFlags(SkBuiltInCodeSnippetID::kLocalMatrixShader));
}

} // anonymous namespace

#endif // SK_GRAPHITE_ENABLED

void LocalMatrixShaderBlock::BeginBlock(const SkKeyContext& keyContext,
                                        SkPaintParamsKeyBuilder* builder,
                                        SkPipelineDataGatherer* gatherer,
                                        const LMShaderData& lmShaderData) {

#ifdef SK_GRAPHITE_ENABLED
    if (builder->backend() == SkBackend::kGraphite) {
        auto dict = keyContext.dict();
        // When extracted into SkShaderInfo::SnippetEntries the children will appear after their
        // parent. Thus, the parent's uniform data must appear in the uniform block before the
        // uniform data of the children.
        if (gatherer) {
            add_localmatrixshader_uniform_data(dict, lmShaderData.fLocalMatrix, gatherer);
        }

        builder->beginBlock(SkBuiltInCodeSnippetID::kLocalMatrixShader);
        return;
    }
#endif // SK_GRAPHITE_ENABLED

    if (builder->backend() == SkBackend::kSkVM || builder->backend() == SkBackend::kGanesh) {
        // TODO: add implementation for other backends
        SolidColorShaderBlock::BeginBlock(keyContext, builder, gatherer, kErrorColor);
    }
}

//--------------------------------------------------------------------------------------------------

#ifdef SK_GRAPHITE_ENABLED

namespace {

void add_image_uniform_data(const SkShaderCodeDictionary* dict,
                            const ImageShaderBlock::ImageData& imgData,
                            SkPipelineDataGatherer* gatherer) {
    VALIDATE_UNIFORMS(gatherer, dict, SkBuiltInCodeSnippetID::kImageShader)

    SkMatrix lmInverse;
    bool wasInverted = imgData.fLocalMatrix.invert(&lmInverse);  // TODO: handle failure up stack
    if (!wasInverted) {
        lmInverse.setIdentity();
    }

    gatherer->write(SkM44(lmInverse));
    gatherer->write(imgData.fSubset);
    gatherer->write(static_cast<int>(imgData.fTileModes[0]));
    gatherer->write(static_cast<int>(imgData.fTileModes[1]));
    gatherer->write(imgData.fTextureProxy->dimensions().fWidth);
    gatherer->write(imgData.fTextureProxy->dimensions().fHeight);

    gatherer->addFlags(dict->getSnippetRequirementFlags(SkBuiltInCodeSnippetID::kImageShader));
}

} // anonymous namespace

#endif // SK_GRAPHITE_ENABLED

ImageShaderBlock::ImageData::ImageData(const SkSamplingOptions& sampling,
                                       SkTileMode tileModeX,
                                       SkTileMode tileModeY,
                                       SkRect subset,
                                       const SkMatrix& localMatrix)
    : fSampling(sampling)
    , fTileModes{tileModeX, tileModeY}
    , fSubset(subset)
    , fLocalMatrix(localMatrix) {
}

void ImageShaderBlock::BeginBlock(const SkKeyContext& keyContext,
                                  SkPaintParamsKeyBuilder* builder,
                                  SkPipelineDataGatherer* gatherer,
                                  const ImageData& imgData) {

#ifdef SK_GRAPHITE_ENABLED
    if (builder->backend() == SkBackend::kGraphite) {
        // TODO: allow through lazy proxies
        if (gatherer && !imgData.fTextureProxy) {
            // We're dropping the ImageShader here. This could be an instance of trying to draw
            // a raster-backed image w/ a Graphite-backed canvas.
            // TODO: At some point the pre-compile path should also be creating a texture
            // proxy (i.e., we can remove the 'pipelineData' in the above test).
            SolidColorShaderBlock::BeginBlock(keyContext, builder, gatherer, kErrorColor);
            return;
        }

        auto dict = keyContext.dict();
        if (gatherer) {
            gatherer->add(imgData.fSampling,
                          imgData.fTileModes,
                          imgData.fTextureProxy);

            add_image_uniform_data(dict, imgData, gatherer);
        }

        builder->beginBlock(SkBuiltInCodeSnippetID::kImageShader);
        return;
    }
#endif // SK_GRAPHITE_ENABLED

    if (builder->backend() == SkBackend::kSkVM || builder->backend() == SkBackend::kGanesh) {
        // TODO: add implementation for other backends
        SolidColorShaderBlock::BeginBlock(keyContext, builder, gatherer, kErrorColor);
    }
}

//--------------------------------------------------------------------------------------------------

#ifdef SK_GRAPHITE_ENABLED

namespace {

void add_blendshader_uniform_data(const SkShaderCodeDictionary* dict,
                                  SkBlendMode bm,
                                  SkPipelineDataGatherer* gatherer) {
    VALIDATE_UNIFORMS(gatherer, dict, SkBuiltInCodeSnippetID::kBlendShader)
    gatherer->write(SkTo<int>(bm));
    gatherer->write(0); // padding - remove
    gatherer->write(0); // padding - remove
    gatherer->write(0); // padding - remove

    gatherer->addFlags(dict->getSnippetRequirementFlags(SkBuiltInCodeSnippetID::kBlendShader));
}

} // anonymous namespace

#endif // SK_GRAPHITE_ENABLED

void BlendShaderBlock::BeginBlock(const SkKeyContext& keyContext,
                                  SkPaintParamsKeyBuilder *builder,
                                  SkPipelineDataGatherer* gatherer,
                                  const BlendShaderData& blendData) {

#ifdef SK_GRAPHITE_ENABLED
    if (builder->backend() == SkBackend::kGraphite) {
        auto dict = keyContext.dict();
        // When extracted into SkShaderInfo::SnippetEntries the children will appear after their
        // parent. Thus, the parent's uniform data must appear in the uniform block before the
        // uniform data of the children.
        if (gatherer) {
            add_blendshader_uniform_data(dict, blendData.fBM, gatherer);
        }

        builder->beginBlock(SkBuiltInCodeSnippetID::kBlendShader);
    }
#endif // SK_GRAPHITE_ENABLED

    if (builder->backend() == SkBackend::kSkVM || builder->backend() == SkBackend::kGanesh) {
        // TODO: add implementation for other backends
        SolidColorShaderBlock::BeginBlock(keyContext, builder, gatherer, kErrorColor);
    }
}

//--------------------------------------------------------------------------------------------------
#ifdef SK_GRAPHITE_ENABLED
namespace {

constexpr skgpu::BlendInfo make_simple_blendInfo(skgpu::BlendCoeff srcCoeff,
                                                 skgpu::BlendCoeff dstCoeff) {
    return { skgpu::BlendEquation::kAdd,
             srcCoeff,
             dstCoeff,
             SK_PMColor4fTRANSPARENT,
             skgpu::BlendModifiesDst(skgpu::BlendEquation::kAdd, srcCoeff, dstCoeff) };
}

static constexpr int kNumCoeffModes = (int)SkBlendMode::kLastCoeffMode + 1;
/*>> No coverage, input color unknown <<*/
static constexpr skgpu::BlendInfo gBlendTable[kNumCoeffModes] = {
        /* clear */      make_simple_blendInfo(skgpu::BlendCoeff::kZero, skgpu::BlendCoeff::kZero),
        /* src */        make_simple_blendInfo(skgpu::BlendCoeff::kOne,  skgpu::BlendCoeff::kZero),
        /* dst */        make_simple_blendInfo(skgpu::BlendCoeff::kZero, skgpu::BlendCoeff::kOne),
        /* src-over */   make_simple_blendInfo(skgpu::BlendCoeff::kOne,  skgpu::BlendCoeff::kISA),
        /* dst-over */   make_simple_blendInfo(skgpu::BlendCoeff::kIDA,  skgpu::BlendCoeff::kOne),
        /* src-in */     make_simple_blendInfo(skgpu::BlendCoeff::kDA,   skgpu::BlendCoeff::kZero),
        /* dst-in */     make_simple_blendInfo(skgpu::BlendCoeff::kZero, skgpu::BlendCoeff::kSA),
        /* src-out */    make_simple_blendInfo(skgpu::BlendCoeff::kIDA,  skgpu::BlendCoeff::kZero),
        /* dst-out */    make_simple_blendInfo(skgpu::BlendCoeff::kZero, skgpu::BlendCoeff::kISA),
        /* src-atop */   make_simple_blendInfo(skgpu::BlendCoeff::kDA,   skgpu::BlendCoeff::kISA),
        /* dst-atop */   make_simple_blendInfo(skgpu::BlendCoeff::kIDA,  skgpu::BlendCoeff::kSA),
        /* xor */        make_simple_blendInfo(skgpu::BlendCoeff::kIDA,  skgpu::BlendCoeff::kISA),
        /* plus */       make_simple_blendInfo(skgpu::BlendCoeff::kOne,  skgpu::BlendCoeff::kOne),
        /* modulate */   make_simple_blendInfo(skgpu::BlendCoeff::kZero, skgpu::BlendCoeff::kSC),
        /* screen */     make_simple_blendInfo(skgpu::BlendCoeff::kOne,  skgpu::BlendCoeff::kISC)
};

const skgpu::BlendInfo& get_blend_info(SkBlendMode bm) {
    if (bm <= SkBlendMode::kLastCoeffMode) {
        return gBlendTable[(int) bm];
    }

    return gBlendTable[(int) SkBlendMode::kSrc];
}

void add_shaderbasedblender_uniform_data(const SkShaderCodeDictionary* dict,
                                         SkBlendMode bm,
                                         SkPipelineDataGatherer* gatherer) {
    VALIDATE_UNIFORMS(gatherer, dict, SkBuiltInCodeSnippetID::kShaderBasedBlender)
    gatherer->write(SkTo<int>(bm));
    gatherer->write(0); // padding - remove
    gatherer->write(0); // padding - remove
    gatherer->write(0); // padding - remove

    gatherer->addFlags(
            dict->getSnippetRequirementFlags(SkBuiltInCodeSnippetID::kShaderBasedBlender));
}

} // anonymous namespace

#endif // SK_GRAPHITE_ENABLED

void BlendModeBlock::BeginBlock(const SkKeyContext& keyContext,
                                SkPaintParamsKeyBuilder *builder,
                                SkPipelineDataGatherer* gatherer,
                                SkBlendMode bm) {

#ifdef SK_GRAPHITE_ENABLED
    if (builder->backend() == SkBackend::kGraphite) {
        auto dict = keyContext.dict();

        if (bm <= SkBlendMode::kLastCoeffMode) {
            builder->setBlendInfo(get_blend_info(bm));

            builder->beginBlock(SkBuiltInCodeSnippetID::kFixedFunctionBlender);
        } else {
            // TODO: set up the correct blend info
            builder->setBlendInfo({});

            if (gatherer) {
                add_shaderbasedblender_uniform_data(dict, bm, gatherer);
            }

            builder->beginBlock(SkBuiltInCodeSnippetID::kShaderBasedBlender);
        }
        return;
    }
#endif// SK_GRAPHITE_ENABLED

    if (builder->backend() == SkBackend::kSkVM || builder->backend() == SkBackend::kGanesh) {
        // TODO: add implementation for other backends
        SolidColorShaderBlock::BeginBlock(keyContext, builder, gatherer, kErrorColor);
    }
}

//--------------------------------------------------------------------------------------------------
// TODO: we need to feed the number of stops in the gradients into this method from the
// combination code
SkUniquePaintParamsID CreateKey(const SkKeyContext& keyContext,
                                SkPaintParamsKeyBuilder* builder,
                                SkShaderType s,
                                SkBlendMode bm) {
    SkDEBUGCODE(builder->checkReset());

    // TODO: split out the portion of the block data that is always required from the portion
    // that is only required to gather uniforms. Right now we're passing in a lot of unused
    // data and it is unclear what is actually used.
    switch (s) {
        case SkShaderType::kSolidColor:
            SolidColorShaderBlock::BeginBlock(keyContext, builder, nullptr,
                                              /* unused */ kErrorColor);
            builder->endBlock();
            break;
        case SkShaderType::kLinearGradient:
            GradientShaderBlocks::BeginBlock(keyContext, builder, nullptr,
                                             // only the type and numStops are used
                                             { SkShader::kLinear_GradientType, 0 });
            builder->endBlock();
            break;
        case SkShaderType::kRadialGradient:
            GradientShaderBlocks::BeginBlock(keyContext, builder, nullptr,
                                             // only the type and numStops are used
                                             { SkShader::kRadial_GradientType, 0 });
            builder->endBlock();
            break;
        case SkShaderType::kSweepGradient:
            GradientShaderBlocks::BeginBlock(keyContext, builder, nullptr,
                                             // only the type and numStops are used
                                             { SkShader::kSweep_GradientType, 0 });
            builder->endBlock();
            break;
        case SkShaderType::kConicalGradient:
            GradientShaderBlocks::BeginBlock(keyContext, builder, nullptr,
                                             // only the type and numStops are used
                                             { SkShader::kConical_GradientType, 0 });
            builder->endBlock();
            break;
        case SkShaderType::kLocalMatrix:
            LocalMatrixShaderBlock::BeginBlock(keyContext, builder, nullptr,
                                               // matrix is unused
                                               { SkMatrix::I() });

            {
                // proxy shader
                SolidColorShaderBlock::BeginBlock(keyContext, builder, nullptr,
                                                  /* unused */ kErrorColor);
                builder->endBlock();
            }

            builder->endBlock();
            break;
        case SkShaderType::kImage:
            ImageShaderBlock::BeginBlock(keyContext, builder, nullptr,
                                         // none of the ImageData is used
                                         { SkSamplingOptions(),
                                           SkTileMode::kClamp, SkTileMode::kClamp,
                                           SkRect::MakeEmpty(), SkMatrix::I() });
            builder->endBlock();
            break;
        case SkShaderType::kBlendShader:
            BlendShaderBlock::BeginBlock(keyContext, builder, nullptr,
                                         { SkBlendMode::kSrc });

            {
                // dst
                SolidColorShaderBlock::BeginBlock(keyContext, builder, nullptr,
                                                  /* unused */ kErrorColor);
                builder->endBlock();

                // src
                SolidColorShaderBlock::BeginBlock(keyContext, builder, nullptr,
                                                  /* unused */ kErrorColor);
                builder->endBlock();
            }

            builder->endBlock();
            break;
    }

    BlendModeBlock::BeginBlock(keyContext, builder, /* pipelineData*/ nullptr, bm); // 'bm' is used
    builder->endBlock();

    SkPaintParamsKey key = builder->lockAsKey();

    auto dict = keyContext.dict();

    auto entry = dict->findOrCreate(
            key
#ifdef SK_GRAPHITE_ENABLED
            , builder->blendInfo()
#endif
            );

    return  entry->uniqueID();
}
