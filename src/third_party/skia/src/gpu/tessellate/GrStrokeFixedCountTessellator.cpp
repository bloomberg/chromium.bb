/*
 * Copyright 2021 Google LLC.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "src/gpu/tessellate/GrStrokeFixedCountTessellator.h"

#include "src/core/SkGeometry.h"
#include "src/gpu/geometry/GrPathUtils.h"
#include "src/gpu/geometry/GrWangsFormula.h"
#include "src/gpu/tessellate/GrCullTest.h"
#include "src/gpu/tessellate/GrStrokeIterator.h"

namespace {

constexpr static float kMaxParametricSegments_pow4 = 48*48*48*48;  // 48^4

// Writes out strokes to the given instance chunk array, chopping if necessary so that all instances
// require 48 parametric segments or less. (We don't consider radial segments here. The tessellator
// will just add enough additional segments to handle a worst-case 180 degree stroke.)
class InstanceWriter {
public:
    using ShaderFlags = GrStrokeTessellator::ShaderFlags;

    InstanceWriter(ShaderFlags shaderFlags, GrMeshDrawOp::Target* target, float matrixMaxScale,
                   const SkRect& strokeCullBounds, const SkMatrix& viewMatrix,
                   GrVertexChunkArray* patchChunks, size_t instanceStride, int minInstancesPerChunk)
            : fShaderFlags(shaderFlags)
            , fCullTest(strokeCullBounds, viewMatrix)
            , fChunkBuilder(target, patchChunks, instanceStride, minInstancesPerChunk)
            , fParametricPrecision(GrStrokeTolerances::CalcParametricPrecision(matrixMaxScale)) {
    }

    float parametricPrecision() const { return fParametricPrecision; }

    // maxParametricSegments^4, or the number of parametric segments, raised to the 4th power,
    // that are required by the single instance we've written that requires the most segments.
    float maxParametricSegments_pow4() const { return fMaxParametricSegments_pow4; }

    // Updates the dynamic stroke state that we will write out with each instance.
    void updateDynamicStroke(const SkStrokeRec& stroke) {
        SkASSERT(!fHasDeferredFirstStroke);
        SkASSERT(fShaderFlags & ShaderFlags::kDynamicStroke);
        fDynamicStroke.set(stroke);
    }

    // Updates the dynamic color state that we will write out with each instance.
    void updateDynamicColor(const SkPMColor4f& color) {
        SkASSERT(!fHasDeferredFirstStroke);
        SkASSERT(fShaderFlags & ShaderFlags::kDynamicColor);
        bool wideColor = fShaderFlags & ShaderFlags::kWideColor;
        SkASSERT(wideColor || color.fitsInBytes());
        fDynamicColor.set(color, wideColor);
    }

    void lineTo(SkPoint start, SkPoint end) {
        SkPoint cubic[] = {start, start, end, end};
        SkPoint endControlPoint = start;
        this->writeStroke(cubic, endControlPoint);
    }

    void quadraticTo(const SkPoint p[3]) {
        if (!fCullTest.areVisible3(p)) {
            // The stroke is out of view. Discard it.
            this->discardStroke(p, 3);
            return;
        }
        float numParametricSegments_pow4 = GrWangsFormula::quadratic_pow4(fParametricPrecision, p);
        if (numParametricSegments_pow4 > kMaxParametricSegments_pow4) {
            SkPoint chops[5];
            SkChopQuadAtHalf(p, chops);
            this->quadraticTo(chops);
            this->quadraticTo(chops + 2);
            return;
        }
        SkPoint cubic[4];
        GrPathUtils::convertQuadToCubic(p, cubic);
        SkPoint endControlPoint = cubic[2];
        this->writeStroke(cubic, endControlPoint);
        fMaxParametricSegments_pow4 = std::max(numParametricSegments_pow4,
                                               fMaxParametricSegments_pow4);
    }

    void conicTo(const SkPoint p[3], float w) {
        if (!fCullTest.areVisible3(p)) {
            // The stroke is out of view. Discard it.
            this->discardStroke(p, 3);
            return;
        }
        float numParametricSegments_pow2 = GrWangsFormula::conic_pow2(1/fParametricPrecision, p, w);
        float numParametricSegments_pow4 = numParametricSegments_pow2 * numParametricSegments_pow2;
        if (numParametricSegments_pow4 > kMaxParametricSegments_pow4) {
            SkConic chops[2];
            if (SkConic(p, w).chopAt(.5f, chops)) {
                this->conicTo(chops[0].fPts, chops[0].fW);
                this->conicTo(chops[1].fPts, chops[1].fW);
                return;
            }
            numParametricSegments_pow4 = kMaxParametricSegments_pow4;
        }
        SkPoint conic[4];
        GrPathShader::WriteConicPatch(p, w, conic);
        SkPoint endControlPoint = conic[1];
        this->writeStroke(conic, endControlPoint);
        fMaxParametricSegments_pow4 = std::max(numParametricSegments_pow4,
                                               fMaxParametricSegments_pow4);
    }

    void cubicConvex180To(const SkPoint p[4]) {
        if (!fCullTest.areVisible4(p)) {
            // The stroke is out of view. Discard it.
            this->discardStroke(p, 4);
            return;
        }
        float numParametricSegments_pow4 = GrWangsFormula::cubic_pow4(fParametricPrecision, p);
        if (numParametricSegments_pow4 > kMaxParametricSegments_pow4) {
            SkPoint chops[7];
            SkChopCubicAtHalf(p, chops);
            this->cubicConvex180To(chops);
            this->cubicConvex180To(chops + 3);
            return;
        }
        SkPoint endControlPoint = (p[3] != p[2]) ? p[2] : (p[2] != p[1]) ? p[1] : p[0];
        this->writeStroke(p, endControlPoint);
        fMaxParametricSegments_pow4 = std::max(numParametricSegments_pow4,
                                               fMaxParametricSegments_pow4);
    }

    // Called when we encounter the verb "kMoveWithinContour". Moves invalidate the previous control
    // point. The stroke iterator tells us the new value to use for the previous control point.
    void setLastControlPoint(SkPoint newLastControlPoint) {
        fLastControlPoint = newLastControlPoint;
        fHasLastControlPoint = true;
    }

    // Draws a circle whose diameter is equal to the stroke width. We emit circles at cusp points
    // round caps, and empty strokes that are specified to be drawn as circles.
    void writeCircle(SkPoint location) {
        if (GrVertexWriter writer = fChunkBuilder.appendVertex()) {
            // The shader interprets an empty stroke + empty join as a special case that denotes a
            // circle, or 180-degree point stroke.
            writer.fill(location, 5);
            this->writeDynamicAttribs(&writer);
        }
    }

    void finishContour() {
        if (fHasDeferredFirstStroke) {
            // We deferred the first stroke because we didn't know the previous control point to use
            // for its join. We write it out now.
            SkASSERT(fHasLastControlPoint);
            this->writeStroke(fDeferredFirstStroke, SkPoint());
            fHasDeferredFirstStroke = false;
        }
        fHasLastControlPoint = false;
    }

private:
    void writeStroke(const SkPoint p[4], SkPoint endControlPoint) {
        if (!fHasLastControlPoint) {
            // We don't know the previous control point yet to use for the join. Defer writing out
            // this stroke until the end.
            memcpy(fDeferredFirstStroke, p, sizeof(fDeferredFirstStroke));
            fHasDeferredFirstStroke = true;
            fHasLastControlPoint = true;
        } else if (GrVertexWriter writer = fChunkBuilder.appendVertex()) {
            writer.writeArray(p, 4);
            writer.write(fLastControlPoint);
            this->writeDynamicAttribs(&writer);
        }
        fLastControlPoint = endControlPoint;
    }

    void writeDynamicAttribs(GrVertexWriter* writer) {
        if (fShaderFlags & ShaderFlags::kDynamicStroke) {
            writer->write(fDynamicStroke);
        }
        if (fShaderFlags & ShaderFlags::kDynamicColor) {
            writer->write(fDynamicColor);
        }
    }

    void discardStroke(const SkPoint p[], int numPts) {
        // Set fLastControlPoint to the next stroke's p0 (which will be equal to the final point of
        // this stroke). This has the effect of disabling the next stroke's join.
        fLastControlPoint = p[numPts - 1];
        fHasLastControlPoint = true;
    }

    const ShaderFlags fShaderFlags;
    const GrCullTest fCullTest;
    GrVertexChunkBuilder fChunkBuilder;
    const float fParametricPrecision;
    float fMaxParametricSegments_pow4 = 1;

    // We can't write out the first stroke until we know the previous control point for its join.
    SkPoint fDeferredFirstStroke[4];
    SkPoint fLastControlPoint;  // Used to configure the joins in the instance data.
    bool fHasDeferredFirstStroke = false;
    bool fHasLastControlPoint = false;

    // Values for the current dynamic state (if any) that will get written out with each instance.
    GrStrokeShader::DynamicStroke fDynamicStroke;
    GrVertexColor fDynamicColor;
};

// Returns the worst-case number of edges we will need in order to draw a join of the given type.
static int worst_case_edges_in_join(SkPaint::Join joinType, float numRadialSegmentsPerRadian) {
    int numEdges = GrStrokeShader::NumFixedEdgesInJoin(joinType);
    if (joinType == SkPaint::kRound_Join) {
        // For round joins we need to count the radial edges on our own. Account for a worst-case
        // join of 180 degrees (SK_ScalarPI radians).
        numEdges += std::max(SkScalarCeilToInt(numRadialSegmentsPerRadian * SK_ScalarPI) - 1, 0);
    }
    return numEdges;
}

}  // namespace

void GrStrokeFixedCountTessellator::prepare(GrMeshDrawOp::Target* target,
                                            int totalCombinedVerbCnt) {
    int maxEdgesInJoin = 0;
    float maxRadialSegmentsPerRadian = 0;

    // Over-allocate enough patches for each stroke to chop once, and for 8 extra caps. Since we
    // have to chop at inflections, points of 180 degree rotation, and anywhere a stroke requires
    // too many parametric segments, many strokes will end up getting choppped.
    int strokePreallocCount = totalCombinedVerbCnt * 2;
    int capPreallocCount = 8;
    int minInstancesPerChunk = strokePreallocCount + capPreallocCount;
    InstanceWriter instanceWriter(fShader.flags(), target, fMatrixMinMaxScales[1],
                                  fStrokeCullBounds, fShader.viewMatrix(), &fInstanceChunks,
                                  fShader.instanceStride(), minInstancesPerChunk);

    if (!fShader.hasDynamicStroke()) {
        // Strokes are static. Calculate tolerances once.
        const SkStrokeRec& stroke = fPathStrokeList->fStroke;
        float localStrokeWidth = GrStrokeTolerances::GetLocalStrokeWidth(fMatrixMinMaxScales.data(),
                                                                         stroke.getWidth());
        float numRadialSegmentsPerRadian = GrStrokeTolerances::CalcNumRadialSegmentsPerRadian(
                instanceWriter.parametricPrecision(), localStrokeWidth);
        maxEdgesInJoin = worst_case_edges_in_join(stroke.getJoin(), numRadialSegmentsPerRadian);
        maxRadialSegmentsPerRadian = numRadialSegmentsPerRadian;
    }

    // Fast SIMD queue that buffers up values for "numRadialSegmentsPerRadian". Only used when we
    // have dynamic stroke.
    GrStrokeToleranceBuffer toleranceBuffer(instanceWriter.parametricPrecision());

    for (PathStrokeList* pathStroke = fPathStrokeList; pathStroke; pathStroke = pathStroke->fNext) {
        const SkStrokeRec& stroke = pathStroke->fStroke;
        if (fShader.hasDynamicStroke()) {
            // Strokes are dynamic. Calculate tolerances every time.
            float numRadialSegmentsPerRadian =
                    toleranceBuffer.fetchRadialSegmentsPerRadian(pathStroke);
            maxEdgesInJoin = std::max(
                    worst_case_edges_in_join(stroke.getJoin(), numRadialSegmentsPerRadian),
                    maxEdgesInJoin);
            maxRadialSegmentsPerRadian = std::max(numRadialSegmentsPerRadian,
                                                  maxRadialSegmentsPerRadian);
            instanceWriter.updateDynamicStroke(stroke);
        }
        if (fShader.hasDynamicColor()) {
            instanceWriter.updateDynamicColor(pathStroke->fColor);
        }
        GrStrokeIterator strokeIter(pathStroke->fPath, &pathStroke->fStroke, &fShader.viewMatrix());
        while (strokeIter.next()) {
            const SkPoint* p = strokeIter.pts();
            switch (strokeIter.verb()) {
                using Verb = GrStrokeIterator::Verb;
                int numChops;
                case Verb::kContourFinished:
                    instanceWriter.finishContour();
                    break;
                case Verb::kCircle:
                    // Round cap or else an empty stroke that is specified to be drawn as a circle.
                    instanceWriter.writeCircle(p[0]);
                    [[fallthrough]];
                case Verb::kMoveWithinContour:
                    instanceWriter.setLastControlPoint(p[0]);
                    break;
                case Verb::kLine:
                    instanceWriter.lineTo(p[0], p[1]);
                    break;
                case Verb::kQuad:
                    if (GrPathUtils::conicHasCusp(p)) {
                        // The cusp is always at the midtandent.
                        SkPoint cusp = SkEvalQuadAt(p, SkFindQuadMidTangent(p));
                        instanceWriter.writeCircle(cusp);
                        // A quad can only have a cusp if it's flat with a 180-degree turnaround.
                        instanceWriter.lineTo(p[0], cusp);
                        instanceWriter.lineTo(cusp, p[2]);
                    } else {
                        instanceWriter.quadraticTo(p);
                    }
                    break;
                case Verb::kConic:
                    if (GrPathUtils::conicHasCusp(p)) {
                        // The cusp is always at the midtandent.
                        SkConic conic(p, strokeIter.w());
                        SkPoint cusp = conic.evalAt(conic.findMidTangent());
                        instanceWriter.writeCircle(cusp);
                        // A conic can only have a cusp if it's flat with a 180-degree turnaround.
                        instanceWriter.lineTo(p[0], cusp);
                        instanceWriter.lineTo(cusp, p[2]);
                    } else {
                        instanceWriter.conicTo(p, strokeIter.w());
                    }
                    break;
                case Verb::kCubic:
                    SkPoint chops[10];
                    float T[2];
                    bool areCusps;
                    numChops = GrPathUtils::findCubicConvex180Chops(p, T, &areCusps);
                    if (numChops == 0) {
                        instanceWriter.cubicConvex180To(p);
                    } else if (numChops == 1) {
                        SkChopCubicAt(p, chops, T[0]);
                        if (areCusps) {
                            instanceWriter.writeCircle(chops[3]);
                            // In a perfect world, these 3 points would be be equal after chopping
                            // on a cusp.
                            chops[2] = chops[4] = chops[3];
                        }
                        instanceWriter.cubicConvex180To(chops);
                        instanceWriter.cubicConvex180To(chops + 3);
                    } else {
                        SkASSERT(numChops == 2);
                        SkChopCubicAt(p, chops, T[0], T[1]);
                        if (areCusps) {
                            instanceWriter.writeCircle(chops[3]);
                            instanceWriter.writeCircle(chops[6]);
                            // Two cusps are only possible if it's a flat line with two 180-degree
                            // turnarounds.
                            instanceWriter.lineTo(chops[0], chops[3]);
                            instanceWriter.lineTo(chops[3], chops[6]);
                            instanceWriter.lineTo(chops[6], chops[9]);
                        } else {
                            instanceWriter.cubicConvex180To(chops);
                            instanceWriter.cubicConvex180To(chops + 3);
                            instanceWriter.cubicConvex180To(chops + 6);
                        }
                    }
                    break;
            }
        }
    }

    // The maximum rotation we can have in a stroke is 180 degrees (SK_ScalarPI radians).
    int maxRadialSegmentsInStroke =
            std::max(SkScalarCeilToInt(maxRadialSegmentsPerRadian * SK_ScalarPI), 1);

    int maxParametricSegmentsInStroke = SkScalarCeilToInt(sqrtf(sqrtf(
            instanceWriter.maxParametricSegments_pow4())));
    SkASSERT(maxParametricSegmentsInStroke >= 1);  // maxParametricSegments_pow4 is always >= 1.

    // Now calculate the maximum number of edges we will need in the stroke portion of the instance.
    // The first and last edges in a stroke are shared by both the parametric and radial sets of
    // edges, so the total number of edges is:
    //
    //   numCombinedEdges = numParametricEdges + numRadialEdges - 2
    //
    // It's also important to differentiate between the number of edges and segments in a strip:
    //
    //   numSegments = numEdges - 1
    //
    // So the total number of combined edges in the stroke is:
    //
    //   numEdgesInStroke = numParametricSegments + 1 + numRadialSegments + 1 - 2
    //                    = numParametricSegments + numRadialSegments
    //
    int maxEdgesInStroke = maxRadialSegmentsInStroke + maxParametricSegmentsInStroke;

    // Each triangle strip has two sections: It starts with a join then transitions to a stroke. The
    // number of edges in an instance is the sum of edges from the join and stroke sections both.
    // NOTE: The final join edge and the first stroke edge are co-located, however we still need to
    // emit both because the join's edge is half-width and the stroke's is full-width.
    int fixedEdgeCount = maxEdgesInJoin + maxEdgesInStroke;

    fShader.setFixedCountNumTotalEdges(fixedEdgeCount);
    fFixedVertexCount = fixedEdgeCount * 2;
}

void GrStrokeFixedCountTessellator::draw(GrOpFlushState* flushState) const {
    if (fInstanceChunks.empty() || fFixedVertexCount <= 0) {
        return;
    }
    for (const auto& instanceChunk : fInstanceChunks) {
        flushState->bindBuffers(nullptr, instanceChunk.fBuffer, nullptr);
        flushState->drawInstanced(instanceChunk.fCount, instanceChunk.fBase, fFixedVertexCount, 0);
    }
}
