/*
 * Copyright 2019 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "gm/gm.h"

#include "src/gpu/GrCaps.h"
#include "src/gpu/GrContextPriv.h"
#include "src/gpu/GrMemoryPool.h"
#include "src/gpu/GrOpFlushState.h"
#include "src/gpu/GrOpsRenderPass.h"
#include "src/gpu/GrPipeline.h"
#include "src/gpu/GrPrimitiveProcessor.h"
#include "src/gpu/GrProgramInfo.h"
#include "src/gpu/GrRecordingContextPriv.h"
#include "src/gpu/GrRenderTargetContext.h"
#include "src/gpu/GrRenderTargetContextPriv.h"
#include "src/gpu/GrShaderCaps.h"
#include "src/gpu/GrShaderVar.h"
#include "src/gpu/glsl/GrGLSLFragmentShaderBuilder.h"
#include "src/gpu/glsl/GrGLSLGeometryProcessor.h"
#include "src/gpu/glsl/GrGLSLPrimitiveProcessor.h"
#include "src/gpu/glsl/GrGLSLVarying.h"
#include "src/gpu/glsl/GrGLSLVertexGeoBuilder.h"
#include "src/gpu/ops/GrDrawOp.h"

namespace skiagm {

constexpr static GrGeometryProcessor::Attribute kPositionAttrib =
        {"position", kFloat3_GrVertexAttribType, kFloat3_GrSLType};

constexpr static std::array<float, 3> kTri1[3] = {
        {20.5f,20.5f,1}, {170.5f,280.5f,4}, {320.5f,20.5f,1}};
constexpr static std::array<float, 3> kTri2[3] = {
        {640.5f,280.5f,3}, {490.5f,20.5f,1}, {340.5f,280.5f,6}};
constexpr static SkRect kRect = {20.5f, 340.5f, 640.5f, 480.5f};

constexpr static int kWidth = (int)kRect.fRight + 21;
constexpr static int kHeight = (int)kRect.fBottom + 21;

/**
 * This is a GPU-backend specific test. It ensures that tessellation works as expected by drawing
 * several triangles. The test passes as long as the triangle tessellations match the reference
 * images on gold.
 */
class TessellationGM : public GpuGM {
    SkString onShortName() override { return SkString("tessellation"); }
    SkISize onISize() override { return {kWidth, kHeight}; }
    DrawResult onDraw(GrContext*, GrRenderTargetContext*, SkCanvas*, SkString*) override;
};


class TessellationTestTriShader : public GrGeometryProcessor {
public:
    TessellationTestTriShader(const SkMatrix& viewMatrix)
            : GrGeometryProcessor(kTessellationTestTriShader_ClassID), fViewMatrix(viewMatrix) {
        this->setVertexAttributes(&kPositionAttrib, 1);
        this->setWillUseTessellationShaders();
    }

private:
    const char* name() const final { return "TessellationTestTriShader"; }
    void getGLSLProcessorKey(const GrShaderCaps&, GrProcessorKeyBuilder* b) const final {}

    class Impl : public GrGLSLGeometryProcessor {
        void onEmitCode(EmitArgs& args, GrGPArgs*) override {
            args.fVaryingHandler->emitAttributes(args.fGP.cast<TessellationTestTriShader>());
            const char* viewMatrix;
            fViewMatrixUniform = args.fUniformHandler->addUniform(
                    nullptr, kVertex_GrShaderFlag, kFloat3x3_GrSLType, "view_matrix", &viewMatrix);
            args.fVertBuilder->declareGlobal(
                    GrShaderVar("P_", kFloat3_GrSLType, GrShaderVar::TypeModifier::Out));
            args.fVertBuilder->codeAppendf(R"(
                    P_.xy = (%s * float3(position.xy, 1)).xy;
                    P_.z = position.z;)", viewMatrix);
            // GrGLProgramBuilder will call writeTess*ShaderGLSL when it is compiling.
            this->writeFragmentShader(args.fFragBuilder, args.fOutputColor, args.fOutputCoverage);
        }
        void writeFragmentShader(GrGLSLFPFragmentBuilder*, const char* color, const char* coverage);
        void setData(const GrGLSLProgramDataManager& pdman, const GrPrimitiveProcessor& proc,
                     const CoordTransformRange&) override {
            pdman.setSkMatrix(fViewMatrixUniform,
                              proc.cast<TessellationTestTriShader>().fViewMatrix);
        }
        GrGLSLUniformHandler::UniformHandle fViewMatrixUniform;
    };

    GrGLSLPrimitiveProcessor* createGLSLInstance(const GrShaderCaps&) const override {
        return new Impl;
    }

    SkString getTessControlShaderGLSL(const char* versionAndExtensionDecls,
                                      const GrShaderCaps&) const override;
    SkString getTessEvaluationShaderGLSL(const char* versionAndExtensionDecls,
                                         const GrShaderCaps&) const override;

    const SkMatrix fViewMatrix;
};

SkString TessellationTestTriShader::getTessControlShaderGLSL(
        const char* versionAndExtensionDecls, const GrShaderCaps&) const {
    SkString code(versionAndExtensionDecls);
    code.append(R"(
            layout(vertices = 3) out;

            in vec3 P_[];
            out vec3 P[];

            void main() {
                P[gl_InvocationID] = P_[gl_InvocationID];
                gl_TessLevelOuter[gl_InvocationID] = P_[gl_InvocationID].z;
                gl_TessLevelInner[0] = 2.0;
            })");

    return code;
}

SkString TessellationTestTriShader::getTessEvaluationShaderGLSL(
        const char* versionAndExtensionDecls, const GrShaderCaps&) const {
    SkString code(versionAndExtensionDecls);
    code.append(R"(
            layout(triangles, equal_spacing, cw) in;

            uniform vec4 sk_RTAdjust;

            in vec3 P[];
            out vec3 barycentric_coord;

            void main() {
                vec2 devcoord = mat3x2(P[0].xy, P[1].xy, P[2].xy) * gl_TessCoord.xyz;
                devcoord = round(devcoord - .5) + .5;  // Make horz and vert lines on px bounds.
                gl_Position = vec4(devcoord.xy * sk_RTAdjust.xz + sk_RTAdjust.yw, 0.0, 1.0);

                float i = 0.0;
                if (gl_TessCoord.y == 0.0) {
                    i += gl_TessCoord.z * P[1].z;
                } else {
                    i += P[1].z;
                    if (gl_TessCoord.x == 0.0) {
                        i += gl_TessCoord.y * P[0].z;
                    } else {
                        i += P[0].z;
                        if (gl_TessCoord.z == 0.0) {
                            i += gl_TessCoord.x * P[2].z;
                        } else {
                            barycentric_coord = vec3(0, 1, 0);
                            return;
                        }
                    }
                }
                i = abs(mod(i, 2.0) - 1.0);
                barycentric_coord = vec3(i, 0, 1.0 - i);
            })");

    return code;
}

void TessellationTestTriShader::Impl::writeFragmentShader(
        GrGLSLFPFragmentBuilder* f, const char* color, const char* coverage) {
    f->declareGlobal(
            GrShaderVar("barycentric_coord", kFloat3_GrSLType, GrShaderVar::TypeModifier::In));
    f->codeAppendf(R"(
            half3 d = half3(1 - barycentric_coord/fwidth(barycentric_coord));
            half coverage = max(max(d.x, d.y), d.z);
            %s = half4(0, coverage, coverage, 1);
            %s = half4(1);)", color, coverage);
}

class TessellationTestRectShader : public GrGeometryProcessor {
public:
    TessellationTestRectShader(const SkMatrix& viewMatrix)
            : GrGeometryProcessor(kTessellationTestTriShader_ClassID), fViewMatrix(viewMatrix) {
        this->setWillUseTessellationShaders();
    }

private:
    const char* name() const final { return "TessellationTestRectShader"; }
    void getGLSLProcessorKey(const GrShaderCaps&, GrProcessorKeyBuilder* b) const final {}

    class Impl : public GrGLSLGeometryProcessor {
        void onEmitCode(EmitArgs& args, GrGPArgs* gpArgs) override {
            const char* viewMatrix;
            fViewMatrixUniform = args.fUniformHandler->addUniform(
                    nullptr, kVertex_GrShaderFlag, kFloat3x3_GrSLType, "view_matrix", &viewMatrix);
            args.fVertBuilder->declareGlobal(
                    GrShaderVar("M_", kFloat3x3_GrSLType, GrShaderVar::TypeModifier::Out));
            args.fVertBuilder->codeAppendf("M_ = %s;", viewMatrix);
            // GrGLProgramBuilder will call writeTess*ShaderGLSL when it is compiling.
            this->writeFragmentShader(args.fFragBuilder, args.fOutputColor, args.fOutputCoverage);
        }
        void writeFragmentShader(GrGLSLFPFragmentBuilder*, const char* color, const char* coverage);
        void setData(const GrGLSLProgramDataManager& pdman, const GrPrimitiveProcessor& proc,
                     const CoordTransformRange&) override {
            pdman.setSkMatrix(fViewMatrixUniform,
                              proc.cast<TessellationTestRectShader>().fViewMatrix);
        }
        GrGLSLUniformHandler::UniformHandle fViewMatrixUniform;
    };

    GrGLSLPrimitiveProcessor* createGLSLInstance(const GrShaderCaps&) const override {
        return new Impl;
    }

    SkString getTessControlShaderGLSL(const char* versionAndExtensionDecls,
                                      const GrShaderCaps&) const override;
    SkString getTessEvaluationShaderGLSL(const char* versionAndExtensionDecls,
                                         const GrShaderCaps&) const override;

    const SkMatrix fViewMatrix;
};

SkString TessellationTestRectShader::getTessControlShaderGLSL(
        const char* versionAndExtensionDecls, const GrShaderCaps& caps) const {
    SkString code(versionAndExtensionDecls);
    code.append(R"(
            layout(vertices = 1) out;

            in mat3 M_[];
            out mat3 M[];

            void main() {
                M[gl_InvocationID] = M_[gl_InvocationID];
                gl_TessLevelInner[0] = 8.0;
                gl_TessLevelInner[1] = 2.0;
                gl_TessLevelOuter[0] = 2.0;
                gl_TessLevelOuter[1] = 8.0;
                gl_TessLevelOuter[2] = 2.0;
                gl_TessLevelOuter[3] = 8.0;
            })");

    return code;
}

SkString TessellationTestRectShader::getTessEvaluationShaderGLSL(
        const char* versionAndExtensionDecls, const GrShaderCaps& caps) const {
    SkString code(versionAndExtensionDecls);
    code.appendf(R"(
            layout(quads, equal_spacing, cw) in;

            uniform vec4 sk_RTAdjust;

            in mat3 M[];
            out vec4 barycentric_coord;

            void main() {
                vec4 R = vec4(%f, %f, %f, %f);
                vec2 localcoord = mix(R.xy, R.zw, gl_TessCoord.xy);
                vec2 devcoord = (M[0] * vec3(localcoord, 1)).xy;
                devcoord = round(devcoord - .5) + .5;  // Make horz and vert lines on px bounds.
                gl_Position = vec4(devcoord.xy * sk_RTAdjust.xz + sk_RTAdjust.yw, 0.0, 1.0);

                float i = gl_TessCoord.x * 8.0;
                i = abs(mod(i, 2.0) - 1.0);
                if (gl_TessCoord.y == 0.0 || gl_TessCoord.y == 1.0) {
                    barycentric_coord = vec4(i, 1.0 - i, 0, 0);
                } else {
                    barycentric_coord = vec4(0, 0, i, 1.0 - i);
                }
            })", kRect.left(), kRect.top(), kRect.right(), kRect.bottom());

    return code;
}

void TessellationTestRectShader::Impl::writeFragmentShader(
        GrGLSLFPFragmentBuilder* f, const char* color, const char* coverage) {
    f->declareGlobal(GrShaderVar("barycentric_coord", kFloat4_GrSLType,
                                 GrShaderVar::TypeModifier::In));
    f->codeAppendf(R"(
            float4 fwidths = fwidth(barycentric_coord);
            half coverage = 0;
            for (int i = 0; i < 4; ++i) {
                if (fwidths[i] != 0) {
                    coverage = half(max(coverage, 1 - barycentric_coord[i]/fwidths[i]));
                }
            }
            %s = half4(coverage, 0, coverage, 1);
            %s = half4(1);)", color, coverage);
}


class TessellationTestOp : public GrDrawOp {
    DEFINE_OP_CLASS_ID

public:
    TessellationTestOp(const SkMatrix& viewMatrix, const std::array<float, 3>* triPositions)
            : GrDrawOp(ClassID()), fViewMatrix(viewMatrix), fTriPositions(triPositions) {
        this->setBounds(SkRect::MakeIWH(kWidth, kHeight), HasAABloat::kNo, IsHairline::kNo);
    }

private:
    const char* name() const override { return "TessellationTestOp"; }
    FixedFunctionFlags fixedFunctionFlags() const override { return FixedFunctionFlags::kNone; }
    GrProcessorSet::Analysis finalize(const GrCaps&, const GrAppliedClip*,
                                      bool hasMixedSampledCoverage, GrClampType) override {
        return GrProcessorSet::EmptySetAnalysis();
    }

    void onPrePrepare(GrRecordingContext*,
                      const GrSurfaceProxyView* writeView,
                      GrAppliedClip*,
                      const GrXferProcessor::DstProxyView&) override {}

    void onPrepare(GrOpFlushState* flushState) override {
        if (fTriPositions) {
            if (void* vertexData = flushState->makeVertexSpace(sizeof(float) * 3, 3, &fVertexBuffer,
                                                               &fBaseVertex)) {
                memcpy(vertexData, fTriPositions, sizeof(float) * 3 * 3);
            }
        }
    }

    void onExecute(GrOpFlushState* state, const SkRect& chainBounds) override {
        GrPipeline pipeline(GrScissorTest::kDisabled, SkBlendMode::kSrc,
                            state->drawOpArgs().writeSwizzle());
        int tessellationPatchVertexCount;
        std::unique_ptr<GrGeometryProcessor> shader;
        if (fTriPositions) {
            if (!fVertexBuffer) {
                return;
            }
            tessellationPatchVertexCount = 3;
            shader = std::make_unique<TessellationTestTriShader>(fViewMatrix);
        } else {
            // Use a mismatched number of vertices in the input patch vs output.
            // (The tessellation control shader will output one vertex per patch.)
            tessellationPatchVertexCount = 5;
            shader = std::make_unique<TessellationTestRectShader>(fViewMatrix);
        }

        GrProgramInfo programInfo(state->proxy()->numSamples(), state->proxy()->numStencilSamples(),
                                  state->proxy()->backendFormat(), state->writeView()->origin(),
                                  &pipeline, shader.get(), GrPrimitiveType::kPatches,
                                  tessellationPatchVertexCount);

        state->bindPipeline(programInfo, SkRect::MakeIWH(kWidth, kHeight));
        state->bindBuffers(nullptr, nullptr, fVertexBuffer.get());
        state->draw(tessellationPatchVertexCount, fBaseVertex);
    }

    const SkMatrix fViewMatrix;
    const std::array<float, 3>* const fTriPositions;
    sk_sp<const GrBuffer> fVertexBuffer;
    int fBaseVertex = 0;
};


static SkPath build_outset_triangle(const std::array<float, 3>* tri) {
    SkPath outset;
    for (int i = 0; i < 3; ++i) {
        SkPoint p = {tri[i][0], tri[i][1]};
        SkPoint left = {tri[(i + 2) % 3][0], tri[(i + 2) % 3][1]};
        SkPoint right = {tri[(i + 1) % 3][0], tri[(i + 1) % 3][1]};
        SkPoint n0, n1;
        n0.setNormalize(left.y() - p.y(), p.x() - left.x());
        n1.setNormalize(p.y() - right.y(), right.x() - p.x());
        p += (n0 + n1) * 3;
        if (0 == i) {
            outset.moveTo(p);
        } else {
            outset.lineTo(p);
        }
    }
    return outset;
}

DrawResult TessellationGM::onDraw(GrContext* ctx, GrRenderTargetContext* rtc, SkCanvas* canvas,
                                  SkString* errorMsg) {
    if (!ctx->priv().caps()->shaderCaps()->tessellationSupport()) {
        *errorMsg = "Requires GPU tessellation support.";
        return DrawResult::kSkip;
    }
    if (!ctx->priv().caps()->shaderCaps()->shaderDerivativeSupport()) {
        *errorMsg = "Requires shader derivatives."
                    "(These are expected to always be present when there is tessellation!!)";
        return DrawResult::kFail;
    }

    canvas->clear(SK_ColorBLACK);
    SkPaint borderPaint;
    borderPaint.setColor4f({0,1,1,1});
    borderPaint.setAntiAlias(true);
    canvas->drawPath(build_outset_triangle(kTri1), borderPaint);
    canvas->drawPath(build_outset_triangle(kTri2), borderPaint);

    borderPaint.setColor4f({1,0,1,1});
    canvas->drawRect(kRect.makeOutset(1.5f, 1.5f), borderPaint);

    GrOpMemoryPool* pool = ctx->priv().opMemoryPool();
    rtc->priv().testingOnly_addDrawOp(
            pool->allocate<TessellationTestOp>(canvas->getTotalMatrix(), kTri1));
    rtc->priv().testingOnly_addDrawOp(
            pool->allocate<TessellationTestOp>(canvas->getTotalMatrix(), kTri2));
    rtc->priv().testingOnly_addDrawOp(
            pool->allocate<TessellationTestOp>(canvas->getTotalMatrix(), nullptr));

    return skiagm::DrawResult::kOk;
}

DEF_GM( return new TessellationGM(); )

}
