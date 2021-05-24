/*
 * Copyright 2021 Google LLC.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "src/gpu/tessellate/GrPathTessellator.h"

#include "src/gpu/GrEagerVertexAllocator.h"
#include "src/gpu/GrGpu.h"
#include "src/gpu/geometry/GrPathUtils.h"
#include "src/gpu/tessellate/GrMiddleOutPolygonTriangulator.h"
#include "src/gpu/tessellate/GrMidpointContourParser.h"
#include "src/gpu/tessellate/GrStencilPathShader.h"
#include "src/gpu/tessellate/GrWangsFormula.h"

GrPathIndirectTessellator::GrPathIndirectTessellator(const SkMatrix& viewMatrix, const SkPath& path,
                                                     DrawInnerFan drawInnerFan)
        : fDrawInnerFan(drawInnerFan != DrawInnerFan::kNo) {
    // Count the number of instances at each resolveLevel.
    GrVectorXform xform(viewMatrix);
    for (auto [verb, pts, w] : SkPathPriv::Iterate(path)) {
        int level;
        switch (verb) {
            case SkPathVerb::kConic:
                level = GrWangsFormula::conic_log2(1.f / kLinearizationIntolerance, pts, *w, xform);
                break;
            case SkPathVerb::kQuad:
                level = GrWangsFormula::quadratic_log2(kLinearizationIntolerance, pts, xform);
                break;
            case SkPathVerb::kCubic:
                level = GrWangsFormula::cubic_log2(kLinearizationIntolerance, pts, xform);
                break;
            default:
                continue;
        }
        SkASSERT(level >= 0);
        // Instances with 2^0=1 segments are empty (zero area). We ignore them completely.
        if (level > 0) {
            level = std::min(level, kMaxResolveLevel);
            ++fResolveLevelCounts[level];
            ++fOuterCurveInstanceCount;
        }
    }
}

void GrPathIndirectTessellator::prepare(GrMeshDrawOp::Target* target, const SkMatrix& viewMatrix,
                                        const SkPath& path,
                                        const BreadcrumbTriangleList* breadcrumbTriangleList) {
    SkASSERT(fTotalInstanceCount == 0);
    SkASSERT(fIndirectDrawCount == 0);
    SkASSERT(target->caps().drawInstancedSupport());

    int instanceLockCount = fOuterCurveInstanceCount;
    if (fDrawInnerFan) {
        // No initial moveTo, plus an implicit close at the end; n-2 triangles fill an n-gon.
        int maxInnerTriangles = path.countVerbs() - 1;
        instanceLockCount += maxInnerTriangles;
    }
    if (breadcrumbTriangleList) {
        instanceLockCount += breadcrumbTriangleList->count();
    }
    if (instanceLockCount == 0) {
        return;
    }

    // Allocate a buffer to store the instance data.
    GrEagerDynamicVertexAllocator vertexAlloc(target, &fInstanceBuffer, &fBaseInstance);
    GrVertexWriter instanceWriter = static_cast<SkPoint*>(vertexAlloc.lock(sizeof(SkPoint) * 4,
                                                                           instanceLockCount));
    if (!instanceWriter) {
        return;
    }

    // Write out any triangles at the beginning of the cubic data. Since this shader draws curves,
    // output the triangles as conics with w=infinity (which is equivalent to a triangle).
    int numTrianglesAtBeginningOfData = 0;
    if (fDrawInnerFan) {
        numTrianglesAtBeginningOfData = GrMiddleOutPolygonTriangulator::WritePathInnerFan(
                &instanceWriter,
                GrMiddleOutPolygonTriangulator::OutputType::kConicsWithInfiniteWeight, path);
    }
    if (breadcrumbTriangleList) {
        SkDEBUGCODE(int count = 0;)
        for (const auto* tri = breadcrumbTriangleList->head(); tri; tri = tri->fNext) {
            SkDEBUGCODE(++count;)
            const SkPoint* p = tri->fPts;
            if ((p[0].fX == p[1].fX && p[1].fX == p[2].fX) ||
                (p[0].fY == p[1].fY && p[1].fY == p[2].fY)) {
                // Completely degenerate triangles have undefined winding. And T-junctions shouldn't
                // happen on axis-aligned edges.
                continue;
            }
            instanceWriter.writeArray(p, 3);
            // Mark this instance as a triangle by setting it to a conic with w=Inf.
            instanceWriter.fill(GrVertexWriter::kIEEE_32_infinity, 2);
            ++numTrianglesAtBeginningOfData;
        }
        SkASSERT(count == breadcrumbTriangleList->count());
    }

    // Allocate space for the GrDrawIndexedIndirectCommand structs. Allocate enough for each
    // possible resolve level (kMaxResolveLevel; resolveLevel=0 never has any instances), plus one
    // more for the optional inner fan triangles.
    int indirectLockCnt = kMaxResolveLevel + 1;
    GrDrawIndirectWriter indirectWriter = target->makeDrawIndirectSpace(indirectLockCnt,
                                                                        &fIndirectDrawBuffer,
                                                                        &fIndirectDrawOffset);
    if (!indirectWriter) {
        SkASSERT(!fIndirectDrawBuffer);
        vertexAlloc.unlock(0);
        return;
    }

    // Fill out the GrDrawIndexedIndirectCommand structs and determine the starting instance data
    // location at each resolve level.
    GrVertexWriter instanceLocations[kMaxResolveLevel + 1];
    int currentBaseInstance = fBaseInstance;
    SkASSERT(fResolveLevelCounts[0] == 0);
    for (int resolveLevel=1, numExtraInstances=numTrianglesAtBeginningOfData;
         resolveLevel <= kMaxResolveLevel;
         ++resolveLevel, numExtraInstances=0) {
        int instanceCountAtCurrLevel = fResolveLevelCounts[resolveLevel];
        if (!(instanceCountAtCurrLevel + numExtraInstances)) {
            SkDEBUGCODE(instanceLocations[resolveLevel] = nullptr;)
            continue;
        }
        instanceLocations[resolveLevel] = instanceWriter.makeOffset(0);
        SkASSERT(fIndirectDrawCount < indirectLockCnt);
        GrMiddleOutCubicShader::WriteDrawIndirectCmd(&indirectWriter, resolveLevel,
                                                     instanceCountAtCurrLevel + numExtraInstances,
                                                     currentBaseInstance);
        ++fIndirectDrawCount;
        currentBaseInstance += instanceCountAtCurrLevel + numExtraInstances;
        instanceWriter = instanceWriter.makeOffset(instanceCountAtCurrLevel * 4 * sizeof(SkPoint));
    }

    target->putBackIndirectDraws(indirectLockCnt - fIndirectDrawCount);

#ifdef SK_DEBUG
    SkASSERT(currentBaseInstance ==
             fBaseInstance + numTrianglesAtBeginningOfData + fOuterCurveInstanceCount);

    GrVertexWriter endLocations[kMaxResolveLevel + 1];
    int lastResolveLevel = 0;
    for (int resolveLevel = 1; resolveLevel <= kMaxResolveLevel; ++resolveLevel) {
        if (!instanceLocations[resolveLevel]) {
            endLocations[resolveLevel] = nullptr;
            continue;
        }
        endLocations[lastResolveLevel] = instanceLocations[resolveLevel].makeOffset(0);
        lastResolveLevel = resolveLevel;
    }
    endLocations[lastResolveLevel] = instanceWriter.makeOffset(0);
#endif

    fTotalInstanceCount = numTrianglesAtBeginningOfData;

    // Write out the cubic instances.
    if (fOuterCurveInstanceCount) {
        GrVectorXform xform(viewMatrix);
        for (auto [verb, pts, w] : SkPathPriv::Iterate(path)) {
            int level;
            switch (verb) {
                default:
                    continue;
                case SkPathVerb::kConic:
                    level = GrWangsFormula::conic_log2(1.f / kLinearizationIntolerance, pts, *w,
                                                       xform);
                    break;
                case SkPathVerb::kQuad:
                    level = GrWangsFormula::quadratic_log2(kLinearizationIntolerance, pts, xform);
                    break;
                case SkPathVerb::kCubic:
                    level = GrWangsFormula::cubic_log2(kLinearizationIntolerance, pts, xform);
                    break;
            }
            if (level == 0) {
                continue;
            }
            level = std::min(level, kMaxResolveLevel);
            switch (verb) {
                case SkPathVerb::kQuad:
                    GrPathUtils::writeQuadAsCubic(pts, &instanceLocations[level]);
                    break;
                case SkPathVerb::kCubic:
                    instanceLocations[level].writeArray(pts, 4);
                    break;
                case SkPathVerb::kConic:
                    GrPathShader::WriteConicPatch(pts, *w, &instanceLocations[level]);
                    break;
                default:
                    SkUNREACHABLE;
            }
            ++fTotalInstanceCount;
        }
    }

#ifdef SK_DEBUG
    for (int i = 1; i <= kMaxResolveLevel; ++i) {
        SkASSERT(instanceLocations[i] == endLocations[i]);
    }
    SkASSERT(fTotalInstanceCount == numTrianglesAtBeginningOfData + fOuterCurveInstanceCount);
#endif

    vertexAlloc.unlock(fTotalInstanceCount);
}

void GrPathIndirectTessellator::draw(GrOpFlushState* flushState) const {
    if (fIndirectDrawCount) {
        flushState->bindBuffers(nullptr, fInstanceBuffer, nullptr);
        flushState->drawIndirect(fIndirectDrawBuffer.get(), fIndirectDrawOffset,
                                 fIndirectDrawCount);
    }
}

void GrPathIndirectTessellator::drawHullInstances(GrOpFlushState* flushState) const {
    if (fTotalInstanceCount) {
        flushState->bindBuffers(nullptr, fInstanceBuffer, nullptr);
        flushState->drawInstanced(fTotalInstanceCount, fBaseInstance, 4, 0);
    }
}

void GrPathOuterCurveTessellator::prepare(GrMeshDrawOp::Target* target, const SkMatrix& matrix,
                                          const SkPath& path,
                                          const BreadcrumbTriangleList* breadcrumbTriangleList) {
    SkASSERT(target->caps().shaderCaps()->tessellationSupport());
    SkASSERT(!breadcrumbTriangleList);
    SkASSERT(!fPatchBuffer);
    SkASSERT(fPatchVertexCount == 0);

    int vertexLockCount = path.countVerbs() * 4;
    GrEagerDynamicVertexAllocator vertexAlloc(target, &fPatchBuffer, &fBasePatchVertex);
    auto* vertexData = vertexAlloc.lock<SkPoint>(vertexLockCount);
    if (!vertexData) {
        return;
    }

    for (auto [verb, pts, w] : SkPathPriv::Iterate(path)) {
        switch (verb) {
            default:
                continue;
            case SkPathVerb::kQuad:
                SkASSERT(fPatchVertexCount + 4 <= vertexLockCount);
                GrPathUtils::convertQuadToCubic(pts, vertexData + fPatchVertexCount);
                break;
            case SkPathVerb::kCubic:
                SkASSERT(fPatchVertexCount + 4 <= vertexLockCount);
                memcpy(vertexData + fPatchVertexCount, pts, sizeof(SkPoint) * 4);
                break;
            case SkPathVerb::kConic:
                SkASSERT(fPatchVertexCount + 4 <= vertexLockCount);
                GrPathShader::WriteConicPatch(pts, *w, vertexData + fPatchVertexCount);
                break;
        }
        fPatchVertexCount += 4;
    }

    vertexAlloc.unlock(fPatchVertexCount);
}

void GrPathWedgeTessellator::prepare(GrMeshDrawOp::Target* target, const SkMatrix& matrix,
                                     const SkPath& path,
                                     const BreadcrumbTriangleList* breadcrumbTriangleList) {
    SkASSERT(target->caps().shaderCaps()->tessellationSupport());
    SkASSERT(!breadcrumbTriangleList);
    SkASSERT(!fPatchBuffer);
    SkASSERT(fPatchVertexCount == 0);

    // No initial moveTo, one wedge per verb, plus an implicit close at the end.
    // Each wedge has 5 vertices.
    int maxVertices = (path.countVerbs() + 1) * 5;

    GrEagerDynamicVertexAllocator vertexAlloc(target, &fPatchBuffer, &fBasePatchVertex);
    auto* vertexData = vertexAlloc.lock<SkPoint>(maxVertices);
    if (!vertexData) {
        return;
    }

    GrMidpointContourParser parser(path);
    while (parser.parseNextContour()) {
        SkPoint midpoint = parser.currentMidpoint();
        SkPoint startPoint = {0, 0};
        SkPoint lastPoint = startPoint;
        for (auto [verb, pts, w] : parser.currentContour()) {
            switch (verb) {
                case SkPathVerb::kMove:
                    startPoint = lastPoint = pts[0];
                    continue;
                case SkPathVerb::kClose:
                    continue;  // Ignore. We can assume an implicit close at the end.
                case SkPathVerb::kLine:
                    GrPathUtils::convertLineToCubic(pts[0], pts[1], vertexData + fPatchVertexCount);
                    lastPoint = pts[1];
                    break;
                case SkPathVerb::kQuad:
                    GrPathUtils::convertQuadToCubic(pts, vertexData + fPatchVertexCount);
                    lastPoint = pts[2];
                    break;
                case SkPathVerb::kCubic:
                    memcpy(vertexData + fPatchVertexCount, pts, sizeof(SkPoint) * 4);
                    lastPoint = pts[3];
                    break;
                case SkPathVerb::kConic:
                    GrPathShader::WriteConicPatch(pts, *w, vertexData + fPatchVertexCount);
                    lastPoint = pts[2];
                    break;
            }
            vertexData[fPatchVertexCount + 4] = midpoint;
            fPatchVertexCount += 5;
        }
        if (lastPoint != startPoint) {
            GrPathUtils::convertLineToCubic(lastPoint, startPoint, vertexData + fPatchVertexCount);
            vertexData[fPatchVertexCount + 4] = midpoint;
            fPatchVertexCount += 5;
        }
    }

    vertexAlloc.unlock(fPatchVertexCount);
}

void GrPathHardwareTessellator::draw(GrOpFlushState* flushState) const {
    if (fPatchVertexCount) {
        flushState->bindBuffers(nullptr, nullptr, fPatchBuffer);
        flushState->draw(fPatchVertexCount, fBasePatchVertex);
        if (flushState->caps().requiresManualFBBarrierAfterTessellatedStencilDraw()) {
            flushState->gpu()->insertManualFramebufferBarrier();  // http://skbug.com/9739
        }
    }
}
