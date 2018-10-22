/*
 * Copyright 2018 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "gm.h"

#include "GrClip.h"
#include "GrContext.h"
#include "GrGpuCommandBuffer.h"
#include "GrMemoryPool.h"
#include "GrOpFlushState.h"
#include "GrRenderTargetContext.h"
#include "GrRenderTargetContextPriv.h"
#include "GrRenderTarget.h"
#include "glsl/GrGLSLFragmentShaderBuilder.h"
#include "glsl/GrGLSLGeometryProcessor.h"
#include "glsl/GrGLSLVarying.h"
#include "glsl/GrGLSLVertexGeoBuilder.h"

namespace skiagm {

static constexpr GrGeometryProcessor::Attribute gVertex{"vertex", kFloat2_GrVertexAttribType};

/**
 * This is a GPU-backend specific test. It ensures that SkSL properly identifies clockwise-winding
 * triangles (sk_Clockwise), in terms of to Skia device space, in all backends and with all render
 * target origins. We draw clockwise triangles green and counter-clockwise red.
 */
class ClockwiseGM : public GM {
private:
    SkString onShortName() final { return SkString("clockwise"); }
    SkISize onISize() override { return SkISize::Make(300, 200); }
    void onDraw(SkCanvas*) override;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// SkSL code.

class ClockwiseTestProcessor : public GrGeometryProcessor {
public:
    ClockwiseTestProcessor(bool readSkFragCoord)
            : GrGeometryProcessor(kClockwiseTestProcessor_ClassID)
            , fReadSkFragCoord(readSkFragCoord) {
        this->setVertexAttributeCnt(1);
    }
    const char* name() const override { return "ClockwiseTestProcessor"; }
    void getGLSLProcessorKey(const GrShaderCaps&, GrProcessorKeyBuilder* b) const final {
        b->add32(fReadSkFragCoord);
    }
    GrGLSLPrimitiveProcessor* createGLSLInstance(const GrShaderCaps&) const final;

private:
    const Attribute& onVertexAttribute(int i) const override { return gVertex; }

    const bool fReadSkFragCoord;

    friend class GLSLClockwiseTestProcessor;
};

class GLSLClockwiseTestProcessor : public GrGLSLGeometryProcessor {
    void setData(const GrGLSLProgramDataManager& pdman, const GrPrimitiveProcessor&,
                 FPCoordTransformIter&& transformIter) override {}

    void onEmitCode(EmitArgs& args, GrGPArgs* gpArgs) override {
        const ClockwiseTestProcessor& proc = args.fGP.cast<ClockwiseTestProcessor>();
        args.fVaryingHandler->emitAttributes(proc);
        gpArgs->fPositionVar.set(kFloat2_GrSLType, "vertex");
        args.fFragBuilder->codeAppendf(
                "%s = sk_Clockwise ? half4(0,1,0,1) : half4(1,0,0,1);", args.fOutputColor);
        if (!proc.fReadSkFragCoord) {
            args.fFragBuilder->codeAppendf("%s = half4(1);", args.fOutputCoverage);
        } else {
            // Verify layout(origin_upper_left) on gl_FragCoord does not affect gl_FrontFacing.
            args.fFragBuilder->codeAppendf("%s = half4(min(sk_FragCoord.y, 1));",
                                           args.fOutputCoverage);
        }
    }
};

GrGLSLPrimitiveProcessor* ClockwiseTestProcessor::createGLSLInstance(
        const GrShaderCaps&) const {
    return new GLSLClockwiseTestProcessor;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Draw Op.

class ClockwiseTestOp : public GrDrawOp {
public:
    DEFINE_OP_CLASS_ID

    static std::unique_ptr<GrDrawOp> Make(GrContext* context, bool readSkFragCoord, int y = 0) {
        GrOpMemoryPool* pool = context->contextPriv().opMemoryPool();
        return pool->allocate<ClockwiseTestOp>(readSkFragCoord, y);
    }

private:
    ClockwiseTestOp(bool readSkFragCoord, float y)
            : GrDrawOp(ClassID()), fReadSkFragCoord(readSkFragCoord), fY(y) {
        this->setBounds(SkRect::MakeIWH(300, 100), HasAABloat::kNo, IsZeroArea::kNo);
    }

    const char* name() const override { return "ClockwiseTestOp"; }
    FixedFunctionFlags fixedFunctionFlags() const override { return FixedFunctionFlags::kNone; }
    RequiresDstTexture finalize(const GrCaps&, const GrAppliedClip*) override {
        return RequiresDstTexture::kNo;
    }
    void onPrepare(GrOpFlushState*) override {}
    void onExecute(GrOpFlushState* flushState) override {
        SkPoint vertices[4] = {
            {100, fY},
            {0, fY+100},
            {0, fY},
            {100, fY+100},
        };
        sk_sp<GrBuffer> vertexBuffer(flushState->resourceProvider()->createBuffer(
                sizeof(vertices), kVertex_GrBufferType, kStatic_GrAccessPattern,
                GrResourceProvider::kNone_Flag, vertices));
        if (!vertexBuffer) {
            return;
        }
        GrPipeline pipeline(flushState->drawOpArgs().fProxy, GrScissorTest::kDisabled,
                            SkBlendMode::kPlus);
        GrMesh mesh(GrPrimitiveType::kTriangleStrip);
        mesh.setNonIndexedNonInstanced(4);
        mesh.setVertexData(vertexBuffer.get());
        flushState->rtCommandBuffer()->draw(ClockwiseTestProcessor(fReadSkFragCoord), pipeline,
                                            nullptr, nullptr, &mesh, 1, SkRect::MakeIWH(100, 100));
    }

    const bool fReadSkFragCoord;
    const float fY;

    friend class ::GrOpMemoryPool; // for ctor
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// Test.

void ClockwiseGM::onDraw(SkCanvas* canvas) {
    GrContext* ctx = canvas->getGrContext();
    GrRenderTargetContext* rtc = canvas->internal_private_accessTopLayerRenderTargetContext();
    if (!ctx || !rtc) {
        DrawGpuOnlyMessage(canvas);
        return;
    }

    rtc->clear(nullptr, GrColorPackRGBA(0,0,0,255),
               GrRenderTargetContext::CanClearFullscreen::kYes);

    // Draw the test directly to the frame buffer.
    rtc->priv().testingOnly_addDrawOp(ClockwiseTestOp::Make(ctx, false, 0));
    rtc->priv().testingOnly_addDrawOp(ClockwiseTestOp::Make(ctx, true, 100));

    // Draw the test to an off-screen, top-down render target.
    if (auto topLeftRTC = ctx->contextPriv().makeDeferredRenderTargetContext(
                    SkBackingFit::kExact, 100, 200, rtc->asSurfaceProxy()->config(),
                    nullptr, 1, GrMipMapped::kNo, kTopLeft_GrSurfaceOrigin, nullptr,
                    SkBudgeted::kYes)) {
        topLeftRTC->clear(nullptr, 0, GrRenderTargetContext::CanClearFullscreen::kYes);
        topLeftRTC->priv().testingOnly_addDrawOp(ClockwiseTestOp::Make(ctx, false, 0));
        topLeftRTC->priv().testingOnly_addDrawOp(ClockwiseTestOp::Make(ctx, true, 100));
        rtc->drawTexture(GrNoClip(), sk_ref_sp(topLeftRTC->asTextureProxy()),
                         GrSamplerState::Filter::kNearest, 0xffffffff, {0,0,100,200},
                         {100,0,200,200}, GrAA::kNo,
                         SkCanvas::SrcRectConstraint::kStrict_SrcRectConstraint, SkMatrix::I(),
                         nullptr, nullptr);
    }

    // Draw the test to an off-screen, bottom-up render target.
    if (auto topLeftRTC = ctx->contextPriv().makeDeferredRenderTargetContext(
                    SkBackingFit::kExact, 100, 200, rtc->asSurfaceProxy()->config(),
                    nullptr, 1, GrMipMapped::kNo, kBottomLeft_GrSurfaceOrigin, nullptr,
                    SkBudgeted::kYes)) {
        topLeftRTC->clear(nullptr, 0, GrRenderTargetContext::CanClearFullscreen::kYes);
        topLeftRTC->priv().testingOnly_addDrawOp(ClockwiseTestOp::Make(ctx, false, 0));
        topLeftRTC->priv().testingOnly_addDrawOp(ClockwiseTestOp::Make(ctx, true, 100));
        rtc->drawTexture(GrNoClip(), sk_ref_sp(topLeftRTC->asTextureProxy()),
                         GrSamplerState::Filter::kNearest, 0xffffffff, {0,0,100,200},
                         {200,0,300,200}, GrAA::kNo,
                         SkCanvas::SrcRectConstraint::kStrict_SrcRectConstraint, SkMatrix::I(),
                         nullptr, nullptr);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////

DEF_GM( return new ClockwiseGM(); )

}
