/*
 * Copyright 2022 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "src/gpu/graphite/render/TessellateStrokesRenderStep.h"

#include "src/core/SkGeometry.h"
#include "src/core/SkPipelineData.h"

#include "src/gpu/graphite/DrawGeometry.h"
#include "src/gpu/graphite/DrawTypes.h"
#include "src/gpu/graphite/DrawWriter.h"
#include "src/gpu/graphite/render/DynamicInstancesPatchAllocator.h"

#include "src/gpu/tessellate/FixedCountBufferUtils.h"
#include "src/gpu/tessellate/PatchWriter.h"
#include "src/gpu/tessellate/StrokeIterator.h"


namespace skgpu::graphite {

namespace {

using namespace skgpu::tess;

// TODO: De-duplicate with the same settings that's currently used for convex path rendering
// in StencilAndFillPathRenderer.cpp
static constexpr DepthStencilSettings kDirectShadingPass = {
        /*frontStencil=*/{},
        /*backStencil=*/ {},
        /*refValue=*/    0,
        /*stencilTest=*/ false,
        /*depthCompare=*/CompareOp::kGreater,
        /*depthTest=*/   true,
        /*depthWrite=*/  true
};

// Always use dynamic stroke params and join control points, track the join control point in
// PatchWriter and replicate line end points (match Ganesh's shader behavior).
//
// No explicit curve type, since we assume infinity is supported on GPUs using graphite
// No color or wide color attribs, since it might always be part of the PaintParams
// or we'll add a color-only fast path to RenderStep later.
static constexpr PatchAttribs kAttribs = PatchAttribs::kJoinControlPoint |
                                         PatchAttribs::kStrokeParams |
                                         PatchAttribs::kPaintDepth;
using Writer = PatchWriter<DynamicInstancesPatchAllocator<FixedCountStrokes>,
                           Required<PatchAttribs::kJoinControlPoint>,
                           Required<PatchAttribs::kStrokeParams>,
                           Required<PatchAttribs::kPaintDepth>,
                           ReplicateLineEndPoints,
                           TrackJoinControlPoints>;

}  // namespace

TessellateStrokesRenderStep::TessellateStrokesRenderStep()
        : RenderStep("TessellateStrokeRenderStep",
                     "",
                     Flags::kRequiresMSAA | Flags::kPerformsShading,
                     /*uniforms=*/{{"affineMatrix", SkSLType::kFloat4},
                                   {"translate", SkSLType::kFloat2},
                                   {"maxScale", SkSLType::kFloat}},
                     PrimitiveType::kTriangleStrip,
                     kDirectShadingPass,
                     /*vertexAttrs=*/  {},
                     /*instanceAttrs=*/{{"p01", VertexAttribType::kFloat4, SkSLType::kFloat4},
                                        {"p23", VertexAttribType::kFloat4, SkSLType::kFloat4},
                                        {"prevPoint", VertexAttribType::kFloat2, SkSLType::kFloat2},
                                        {"stroke", VertexAttribType::kFloat2, SkSLType::kFloat2},
                                        {"depth", VertexAttribType::kFloat, SkSLType::kFloat}}) {}

TessellateStrokesRenderStep::~TessellateStrokesRenderStep() {}

const char* TessellateStrokesRenderStep::vertexSkSL() const {
    // TODO: Assumes vertex ID support for now, max edges must equal
    // skgpu::tess::FixedCountStrokes::kMaxEdges -> (2^14 - 1) -> 16383
    return R"(
        float edgeID = float(sk_VertexID >> 1);
        if ((sk_VertexID & 1) != 0) {
            edgeID = -edgeID;
        }
        float2x2 affine = float2x2(affineMatrix);
        float4 devPosition = float4(
                tessellate_stroked_curve(edgeID, 16383, affine, translate,
                                         maxScale, p01, p23, prevPoint, stroke),
                depth, 1.0);)";
}

void TessellateStrokesRenderStep::writeVertices(DrawWriter* dw, const DrawGeometry& geom) const {
    SkPath path = geom.shape().asPath(); // TODO: Iterate the Shape directly

    int patchReserveCount = FixedCountStrokes::PreallocCount(path.countVerbs());
    // Stroke tessellation does not use fixed indices or vertex data, and only needs the vertex ID
    static const BindBufferInfo kNullBinding = {};
    // TODO: All HW that Graphite will run on should support instancing ith sk_VertexID, but when
    // we support Vulkan+Swiftshader, we will need the vertex buffer ID fallback unless Swiftshader
    // has figured out how to support vertex IDs before then.
    Writer writer{kAttribs, *dw, kNullBinding, kNullBinding, patchReserveCount};
    writer.updatePaintDepthAttrib(geom.order().depthAsFloat());

    // The vector xform approximates how the control points are transformed by the shader to
    // more accurately compute how many *parametric* segments are needed.
    // getMaxScale() returns -1 if it can't compute a scale factor (e.g. perspective), taking the
    // absolute value automatically converts that to an identity scale factor for our purposes.
    writer.setShaderTransform(wangs_formula::VectorXform{geom.transform()},
                              geom.transform().maxScaleFactor());

    SkASSERT(geom.isStroke());
    writer.updateStrokeParamsAttrib({geom.strokeStyle().halfWidth(),
                                     geom.strokeStyle().joinLimit()});

    // TODO: If PatchWriter can handle adding caps to its deferred patches, and we can convert
    // hairlines to use round caps instead of square, then StrokeIterator can be deleted entirely.
    // Besides being simpler, PatchWriter already has what it needs from the shader matrix and
    // stroke params, so we don't have to re-extract them here.
    SkMatrix shaderMatrix = geom.transform();
    SkStrokeRec stroke{SkStrokeRec::kHairline_InitStyle};
    stroke.setStrokeStyle(geom.strokeStyle().width());
    stroke.setStrokeParams(geom.strokeStyle().cap(),
                           geom.strokeStyle().join(),
                           geom.strokeStyle().miterLimit());
    StrokeIterator strokeIter(path, &stroke, &shaderMatrix);
    while (strokeIter.next()) {
        using Verb = StrokeIterator::Verb;
        const SkPoint* p = strokeIter.pts();
        int numChops;

        // TODO: The cusp detection logic should be moved into PatchWriter and shared between
        // this and StrokeTessellator.cpp, but that will require updating a lot of SkGeometry to
        // operate on float2 (skvx) instead of the legacy SkNx or SkPoint.
        switch (strokeIter.verb()) {
            case Verb::kContourFinished:
                writer.writeDeferredStrokePatch();
                break;
            case Verb::kCircle:
                // Round cap or else an empty stroke that is specified to be drawn as a circle.
                writer.writeCircle(p[0]);
                [[fallthrough]];
            case Verb::kMoveWithinContour:
                // A regular kMove invalidates the previous control point; the stroke iterator
                // tells us a new value to use.
                writer.updateJoinControlPointAttrib(p[0]);
                break;
            case Verb::kLine:
                writer.writeLine(p[0], p[1]);
                break;
            case Verb::kQuad:
                if (ConicHasCusp(p)) {
                    // The cusp is always at the midtandent.
                    SkPoint cusp = SkEvalQuadAt(p, SkFindQuadMidTangent(p));
                    writer.writeCircle(cusp);
                    // A quad can only have a cusp if it's flat with a 180-degree turnaround.
                    writer.writeLine(p[0], cusp);
                    writer.writeLine(cusp, p[2]);
                } else {
                    writer.writeQuadratic(p);
                }
                break;
            case Verb::kConic:
                if (ConicHasCusp(p)) {
                    // The cusp is always at the midtandent.
                    SkConic conic(p, strokeIter.w());
                    SkPoint cusp = conic.evalAt(conic.findMidTangent());
                    writer.writeCircle(cusp);
                    // A conic can only have a cusp if it's flat with a 180-degree turnaround.
                    writer.writeLine(p[0], cusp);
                    writer.writeLine(cusp, p[2]);
                } else {
                    writer.writeConic(p, strokeIter.w());
                }
                break;
            case Verb::kCubic:
                SkPoint chops[10];
                float T[2];
                bool areCusps;
                numChops = FindCubicConvex180Chops(p, T, &areCusps);
                if (numChops == 0) {
                    writer.writeCubic(p);
                } else if (numChops == 1) {
                    SkChopCubicAt(p, chops, T[0]);
                    if (areCusps) {
                        writer.writeCircle(chops[3]);
                        // In a perfect world, these 3 points would be be equal after chopping
                        // on a cusp.
                        chops[2] = chops[4] = chops[3];
                    }
                    writer.writeCubic(chops);
                    writer.writeCubic(chops + 3);
                } else {
                    SkASSERT(numChops == 2);
                    SkChopCubicAt(p, chops, T[0], T[1]);
                    if (areCusps) {
                        writer.writeCircle(chops[3]);
                        writer.writeCircle(chops[6]);
                        // Two cusps are only possible if it's a flat line with two 180-degree
                        // turnarounds.
                        writer.writeLine(chops[0], chops[3]);
                        writer.writeLine(chops[3], chops[6]);
                        writer.writeLine(chops[6], chops[9]);
                    } else {
                        writer.writeCubic(chops);
                        writer.writeCubic(chops + 3);
                        writer.writeCubic(chops + 6);
                    }
                }
                break;
        }
    }
}

void TessellateStrokesRenderStep::writeUniforms(const DrawGeometry& geom,
                                                SkPipelineDataGatherer* gatherer) const {
    SkASSERT(geom.transform().type() < Transform::Type::kProjection); // TODO: Implement perspective

    SkDEBUGCODE(UniformExpectationsValidator uev(gatherer, this->uniforms());)

    // affineMatrix = float4 (2x2 of transform), translate = float2, maxScale = float
    // Column-major 2x2 of the transform.
    skvx::float4 upper = {geom.transform().matrix().rc(0, 0), geom.transform().matrix().rc(1, 0),
                          geom.transform().matrix().rc(0, 1), geom.transform().matrix().rc(1, 1)};
    gatherer->write(upper);

    gatherer->write(SkPoint{geom.transform().matrix().rc(0, 3),
                            geom.transform().matrix().rc(1, 3)});

    gatherer->write(geom.transform().maxScaleFactor());
}

const Renderer& Renderer::TessellatedStrokes() {
    static const TessellateStrokesRenderStep kStrokeStep{};
    static const Renderer kTessellatedStrokeRenderer{"TessellatedStrokes", &kStrokeStep};
    return kTessellatedStrokeRenderer;
}

}  // namespace skgpu::graphite
