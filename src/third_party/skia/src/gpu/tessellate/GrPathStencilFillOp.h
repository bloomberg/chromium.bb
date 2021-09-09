/*
 * Copyright 2021 Google LLC.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef GrPathStencilFillOp_DEFINED
#define GrPathStencilFillOp_DEFINED

#include "src/gpu/ops/GrDrawOp.h"
#include "src/gpu/tessellate/GrPathShader.h"
#include "src/gpu/tessellate/GrTessellationPathRenderer.h"

class GrPathTessellator;

// Draws paths using a standard Redbook "stencil then fill" method. Curves get linearized by either
// GPU tessellation shaders or indirect draws. This Op doesn't apply analytic AA, so it requires
// MSAA if AA is desired.
class GrPathStencilFillOp : public GrDrawOp {
private:
    DEFINE_OP_CLASS_ID

    GrPathStencilFillOp(const SkMatrix& viewMatrix, const SkPath& path, GrPaint&& paint,
                        GrAAType aaType, GrTessellationPathRenderer::OpFlags opFlags)
            : GrDrawOp(ClassID())
            , fOpFlags(opFlags)
            , fViewMatrix(viewMatrix)
            , fPath(path)
            , fAAType(aaType)
            , fColor(paint.getColor4f())
            , fProcessors(std::move(paint)) {
        SkRect devBounds;
        fViewMatrix.mapRect(&devBounds, path.getBounds());
        this->setBounds(devBounds, HasAABloat::kNo, IsHairline::kNo);
    }

    const char* name() const override { return "GrPathStencilFillOp"; }
    void visitProxies(const VisitProxyFunc& fn) const override;
    FixedFunctionFlags fixedFunctionFlags() const override;
    GrProcessorSet::Analysis finalize(const GrCaps&, const GrAppliedClip*, GrClampType) override;

    // Chooses the rendering method we will use and creates the corresponding tessellator and
    // stencil/fill programs.
    void prePreparePrograms(const GrPathShader::ProgramArgs&, GrAppliedClip&& clip);

    void onPrePrepare(GrRecordingContext*, const GrSurfaceProxyView&, GrAppliedClip*,
                      const GrXferProcessor::DstProxyView&, GrXferBarrierFlags,
                      GrLoadOp colorLoadOp) override;
    void onPrepare(GrOpFlushState*) override;
    void onExecute(GrOpFlushState*, const SkRect& chainBounds) override;

    const GrTessellationPathRenderer::OpFlags fOpFlags;
    const SkMatrix fViewMatrix;
    const SkPath fPath;
    const GrAAType fAAType;
    SkPMColor4f fColor;
    GrProcessorSet fProcessors;

    // Decided during prePreparePrograms.
    GrPathTessellator* fTessellator = nullptr;
    const GrProgramInfo* fStencilFanProgram = nullptr;
    const GrProgramInfo* fStencilPathProgram = nullptr;
    const GrProgramInfo* fFillBBoxProgram = nullptr;

    // Filled during onPrepare.
    sk_sp<const GrBuffer> fFanBuffer;
    int fFanBaseVertex = 0;
    int fFanVertexCount = 0;

    friend class GrOp;  // For ctor.
};

#endif
