/*
 * Copyright 2018 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "gm.h"
#include "sk_tool_utils.h"
#include "SkTextUtils.h"

#if SK_SUPPORT_GPU

#include "GrContext.h"
#include "GrGpuCommandBuffer.h"
#include "GrMemoryPool.h"
#include "GrRenderTargetContext.h"
#include "GrRenderTargetContextPriv.h"
#include "glsl/GrGLSLFragmentShaderBuilder.h"
#include "glsl/GrGLSLGeometryProcessor.h"
#include "glsl/GrGLSLVarying.h"
#include "glsl/GrGLSLVertexGeoBuilder.h"

namespace skiagm {

static constexpr GrGeometryProcessor::Attribute gVertex =
        {"bboxcoord", kFloat2_GrVertexAttribType, kFloat2_GrSLType};

/**
 * This ensures that fwidth() works properly on GPU configs by drawing a squircle.
 */
class FwidthSquircleGM : public GM {
private:
    SkString onShortName() final { return SkString("fwidth_squircle"); }
    SkISize onISize() override { return SkISize::Make(200, 200); }
    void onDraw(SkCanvas*) override;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// SkSL code.

class FwidthSquircleTestProcessor : public GrGeometryProcessor {
public:
    FwidthSquircleTestProcessor(const SkMatrix& viewMatrix)
            : GrGeometryProcessor(kFwidthSquircleTestProcessor_ClassID)
            , fViewMatrix(viewMatrix) {
        this->setVertexAttributes(&gVertex, 1);
    }
    const char* name() const override { return "FwidthSquircleTestProcessor"; }
    void getGLSLProcessorKey(const GrShaderCaps&, GrProcessorKeyBuilder* b) const final {}
    GrGLSLPrimitiveProcessor* createGLSLInstance(const GrShaderCaps&) const final;

private:
    const SkMatrix fViewMatrix;

    class Impl;
};

class FwidthSquircleTestProcessor::Impl : public GrGLSLGeometryProcessor {
    void onEmitCode(EmitArgs& args, GrGPArgs* gpArgs) override {
        const auto& proc = args.fGP.cast<FwidthSquircleTestProcessor>();

        auto* uniforms = args.fUniformHandler;
        fViewMatrixHandle =
                uniforms->addUniform(kVertex_GrShaderFlag, kFloat3x3_GrSLType, "viewmatrix");

        auto* varyings = args.fVaryingHandler;
        varyings->emitAttributes(proc);

        GrGLSLVarying squircleCoord(kFloat2_GrSLType);
        varyings->addVarying("bboxcoord", &squircleCoord);

        auto* v = args.fVertBuilder;
        v->codeAppendf("float2x2 R = float2x2(cos(.05), sin(.05), -sin(.05), cos(.05));");

        v->codeAppendf("%s = bboxcoord * 1.25;", squircleCoord.vsOut());
        v->codeAppendf("float3 vertexpos = float3(bboxcoord * 100 * R + 100, 1);");
        v->codeAppendf("vertexpos = %s * vertexpos;", uniforms->getUniformCStr(fViewMatrixHandle));
        gpArgs->fPositionVar.set(kFloat3_GrSLType, "vertexpos");

        auto* f = args.fFragBuilder;
        f->codeAppendf("float golden_ratio = 1.61803398875;");
        f->codeAppendf("float pi = 3.141592653589793;");
        f->codeAppendf("float x = abs(%s.x), y = abs(%s.y);",
                       squircleCoord.fsIn(), squircleCoord.fsIn());

        // Squircle function!
        f->codeAppendf("float fn = pow(x, golden_ratio*pi) + pow(y, golden_ratio*pi) - 1;");
        f->codeAppendf("float fnwidth = fwidth(fn);");
        f->codeAppendf("fnwidth += 1e-10;");  // Guard against divide-by-zero.
        f->codeAppendf("half coverage = clamp(.5 - fn/fnwidth, 0, 1);");

        f->codeAppendf("%s = half4(.51, .42, .71, 1) * .89;", args.fOutputColor);
        f->codeAppendf("%s = half4(coverage);", args.fOutputCoverage);
    }

    void setData(const GrGLSLProgramDataManager& pdman, const GrPrimitiveProcessor& primProc,
                 FPCoordTransformIter&& transformIter) override {
        const auto& proc = primProc.cast<FwidthSquircleTestProcessor>();
        pdman.setSkMatrix(fViewMatrixHandle, proc.fViewMatrix);
    }

    UniformHandle fViewMatrixHandle;
};

GrGLSLPrimitiveProcessor* FwidthSquircleTestProcessor::createGLSLInstance(
        const GrShaderCaps&) const {
    return new Impl();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Draw Op.

class FwidthSquircleTestOp : public GrDrawOp {
public:
    DEFINE_OP_CLASS_ID

    static std::unique_ptr<GrDrawOp> Make(GrContext* ctx, const SkMatrix& viewMatrix) {
        GrOpMemoryPool* pool = ctx->contextPriv().opMemoryPool();
        return pool->allocate<FwidthSquircleTestOp>(viewMatrix);
    }

private:
    FwidthSquircleTestOp(const SkMatrix& viewMatrix)
            : GrDrawOp(ClassID())
            , fViewMatrix(viewMatrix) {
        this->setBounds(SkRect::MakeIWH(200, 200), HasAABloat::kNo, IsZeroArea::kNo);
    }

    const char* name() const override { return "ClockwiseTestOp"; }
    FixedFunctionFlags fixedFunctionFlags() const override { return FixedFunctionFlags::kNone; }
    RequiresDstTexture finalize(const GrCaps&, const GrAppliedClip*) override {
        return RequiresDstTexture::kNo;
    }
    void onPrepare(GrOpFlushState*) override {}
    void onExecute(GrOpFlushState* flushState, const SkRect& chainBounds) override {
        SkPoint vertices[4] = {
            {-1, -1},
            {+1, -1},
            {-1, +1},
            {+1, +1},
        };
        sk_sp<GrBuffer> vertexBuffer(flushState->resourceProvider()->createBuffer(
                sizeof(vertices), kVertex_GrBufferType, kStatic_GrAccessPattern,
                GrResourceProvider::Flags::kNone, vertices));
        if (!vertexBuffer) {
            return;
        }
        GrPipeline pipeline(flushState->drawOpArgs().fProxy, GrScissorTest::kDisabled,
                            SkBlendMode::kSrcOver);
        GrMesh mesh(GrPrimitiveType::kTriangleStrip);
        mesh.setNonIndexedNonInstanced(4);
        mesh.setVertexData(vertexBuffer.get());
        flushState->rtCommandBuffer()->draw(FwidthSquircleTestProcessor(fViewMatrix), pipeline,
                                            nullptr, nullptr, &mesh, 1, SkRect::MakeIWH(100, 100));
    }

    const SkMatrix fViewMatrix;

    friend class ::GrOpMemoryPool; // for ctor
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// Test.

void FwidthSquircleGM::onDraw(SkCanvas* canvas) {
    GrContext* ctx = canvas->getGrContext();
    GrRenderTargetContext* rtc = canvas->internal_private_accessTopLayerRenderTargetContext();

    canvas->clear(SK_ColorWHITE);

    if (!ctx || !rtc) {
        DrawGpuOnlyMessage(canvas);
        return;
    }

    if (!ctx->contextPriv().caps()->shaderCaps()->shaderDerivativeSupport()) {
        SkPaint paint;
        paint.setAntiAlias(true);
        paint.setTextSize(15);
        sk_tool_utils::set_portable_typeface(&paint);
        SkTextUtils::DrawString(canvas, "Shader derivatives not supported.", 150,
                                150 - 8, paint, SkTextUtils::kCenter_Align);
        return;
    }

    // Draw the test directly to the frame buffer.
    rtc->priv().testingOnly_addDrawOp(FwidthSquircleTestOp::Make(ctx, canvas->getTotalMatrix()));
}

DEF_GM( return new FwidthSquircleGM(); )

}

#endif  // SK_SUPPORT_GPU
