/*
 * Copyright 2022 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SkKeyHelpers_DEFINED
#define SkKeyHelpers_DEFINED

#ifdef SK_GRAPHITE_ENABLED
#include "include/gpu/graphite/Context.h"
#endif

#include "include/core/SkBlendMode.h"
#include "include/core/SkM44.h"
#include "include/core/SkSamplingOptions.h"
#include "include/core/SkShader.h"
#include "include/core/SkTileMode.h"
#include "include/private/SkColorData.h"

enum class SkBackend : uint8_t;
enum class SkShaderType : uint32_t;
class SkPaintParamsKeyBuilder;
class SkPipelineDataGatherer;
class SkUniquePaintParamsID;
class SkKeyContext;

namespace skgpu::graphite { class TextureProxy; }

// The KeyHelpers can be used to manually construct an SkPaintParamsKey

struct SolidColorShaderBlock {

    static void BeginBlock(const SkKeyContext&,
                           SkPaintParamsKeyBuilder*,
                           SkPipelineDataGatherer*,
                           const SkPMColor4f&);

};

struct GradientShaderBlocks {

    struct GradientData {
        // TODO: For the sprint we only support 8 stops in the gradients
        static constexpr int kMaxStops = 8;

        // This ctor is used during pre-compilation when we don't have enough information to
        // extract uniform data. However, we must be able to provide enough data to make all the
        // relevant decisions about which code snippets to use.
        GradientData(SkShader::GradientType, int numStops);

        // This ctor is used when extracting information from PaintParams. It must provide
        // enough data to generate the uniform data the selected code snippet will require.
        GradientData(SkShader::GradientType,
                     const SkM44& localMatrix,
                     SkPoint point0, SkPoint point1,
                     float radius0, float radius1,
                     float bias, float scale,
                     SkTileMode,
                     int numStops,
                     SkColor4f* colors,
                     float* offsets);

        bool operator==(const GradientData& rhs) const {
            return fType == rhs.fType &&
                   fLocalMatrix == rhs.fLocalMatrix &&
                   fPoints[0] == rhs.fPoints[0] &&
                   fPoints[1] == rhs.fPoints[1] &&
                   fRadii[0] == rhs.fRadii[0] &&
                   fRadii[1] == rhs.fRadii[1] &&
                   fBias == rhs.fBias &&
                   fScale == rhs.fScale &&
                   fTM == rhs.fTM &&
                   fNumStops == rhs.fNumStops &&
                   !memcmp(fColor4fs, rhs.fColor4fs, sizeof(fColor4fs)) &&
                   !memcmp(fOffsets, rhs.fOffsets, sizeof(fOffsets));
        }
        bool operator!=(const GradientData& rhs) const { return !(*this == rhs); }

        // Layout options.
        SkShader::GradientType fType;
        SkM44                  fLocalMatrix;
        SkPoint                fPoints[2];
        float                  fRadii[2];

        // Layout options for sweep gradient.
        float                  fBias;
        float                  fScale;

        SkTileMode             fTM;
        int                    fNumStops;
        SkColor4f              fColor4fs[kMaxStops];
        float                  fOffsets[kMaxStops];
    };

    static void BeginBlock(const SkKeyContext&,
                           SkPaintParamsKeyBuilder*,
                           SkPipelineDataGatherer*,
                           const GradientData&);

};

struct LocalMatrixShaderBlock {

    struct LMShaderData {
        LMShaderData(const SkMatrix& localMatrix)
                : fLocalMatrix(localMatrix) {
        }

        const SkM44 fLocalMatrix;
    };

    static void BeginBlock(const SkKeyContext&,
                           SkPaintParamsKeyBuilder*,
                           SkPipelineDataGatherer*,
                           const LMShaderData&);

};

struct ImageShaderBlock {

    struct ImageData {
        ImageData(const SkSamplingOptions& sampling,
                  SkTileMode tileModeX,
                  SkTileMode tileModeY,
                  SkRect subset,
                  const SkMatrix& localMatrix);

        SkSamplingOptions fSampling;
        SkTileMode fTileModes[2];
        SkRect fSubset;
        const SkMatrix& fLocalMatrix;

#ifdef SK_GRAPHITE_ENABLED
        // TODO: Currently this is only filled in when we're generating the key from an actual
        // SkImageShader. In the pre-compile case we will need to create a Graphite promise
        // image which holds the appropriate data.
        sk_sp<skgpu::graphite::TextureProxy> fTextureProxy;
#endif
    };

    static void BeginBlock(const SkKeyContext&,
                           SkPaintParamsKeyBuilder*,
                           SkPipelineDataGatherer*,
                           const ImageData&);

};

struct BlendShaderBlock {
    struct BlendShaderData {
        // TODO: add support for blenders
        SkBlendMode fBM;
    };

    static void BeginBlock(const SkKeyContext&,
                           SkPaintParamsKeyBuilder*,
                           SkPipelineDataGatherer*,
                           const BlendShaderData&);

};

struct BlendModeBlock {
    static void BeginBlock(const SkKeyContext&,
                           SkPaintParamsKeyBuilder*,
                           SkPipelineDataGatherer*,
                           SkBlendMode);

};

// Bridge between the combinations system and the SkPaintParamsKey
SkUniquePaintParamsID CreateKey(const SkKeyContext&,
                                SkPaintParamsKeyBuilder*,
                                SkShaderType,
                                SkBlendMode);

#endif // SkKeyHelpers_DEFINED
