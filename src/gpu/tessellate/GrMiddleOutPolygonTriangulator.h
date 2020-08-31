/*
 * Copyright 2020 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef GrMiddleOutPolygonTriangulator_DEFINED
#define GrMiddleOutPolygonTriangulator_DEFINED

#include "include/core/SkPoint.h"
#include "include/private/SkTemplates.h"
#include "src/core/SkMathPriv.h"

// This class emits a polygon triangulation with a "middle-out" topology. Conceptually, middle-out
// emits one large triangle with vertices on both endpoints and a middle point, then recurses on
// both sides of the new triangle. i.e.:
//
//     void emit_middle_out_triangulation(int startIdx, int endIdx) {
//         if (startIdx + 1 == endIdx) {
//             return;
//         }
//         int middleIdx = startIdx + SkNextPow2(endIdx - startIdx) / 2;
//
//         // Recurse on the left half.
//         emit_middle_out_triangulation(startIdx, middleIdx);
//
//         // Emit a large triangle with vertices on both endpoints and a middle point.
//         emit_triangle(vertices[startIdx], vertices[middleIdx], vertices[endIdx - 1]);
//
//         // Recurse on the right half.
//         emit_middle_out_triangulation(middleIdx, endIdx);
//     }
//
// Middle-out produces drastically less work for the rasterizer as compared a linear triangle strip
// or fan.
//
// This class is designed to not know or store all the vertices in the polygon at once. The caller
// pushes each vertex in linear order (perhaps while parsing a path), then rather than relying on
// recursion, we manipulate an O(log N) stack to determine the correct middle-out triangulation.
class GrMiddleOutPolygonTriangulator {
public:
    GrMiddleOutPolygonTriangulator(SkPoint* vertexData, int maxVertices)
            : fTriangleData(reinterpret_cast<std::array<SkPoint, 3>*>(vertexData)) {
        // Determine the deepest our stack can ever go.
        int maxStackDepth = SkNextLog2(maxVertices) + 1;
        if (maxStackDepth > kStackPreallocCount) {
            fVertexStack.reset(maxStackDepth);
        }
        SkDEBUGCODE(fStackAllocCount = maxStackDepth;)
        // The stack will always contain a starting point. This is an implicit moveTo(0, 0)
        // initially, but will be overridden if moveTo() gets called before adding geometry.
        fVertexStack[0] = {0, {0, 0}};
        fTop = fVertexStack;
    }

    void pushVertex(const SkPoint& pt) {
        if (pt == fVertexStack[0].fPoint) {
            this->close();
            return;
        }
        // This new vertex we are about to add is one vertex away from the top of the stack.
        // i.e., it is guaranteed to be the next vertex in the polygon after the one stored in fTop.
        int vertexIdxDelta = 1;
        // Our topology wants triangles that have the same vertexIdxDelta on both sides:
        // e.g., a run of 9 points should be triangulated as:
        //
        //    [0, 1, 2], [2, 3, 4], [4, 5, 6], [6, 7, 8]  // vertexIdxDelta == 1
        //    [0, 2, 4], [4, 6, 8]  // vertexIdxDelta == 2
        //    [0, 4, 8]  // vertexIdxDelta == 4
        //
        // Emit as many new triangles as we can with equal-delta sides and pop their vertices off
        // the stack before pushing this new vertex.
        //
        // (This is a stack-based implementation of the recursive example method from the class
        // comment.)
        while (vertexIdxDelta == fTop->fVertexIdxDelta) {
            *fTriangleData++ = {fTop[-1].fPoint, fTop->fPoint, pt};
            vertexIdxDelta *= 2;
            this->popTop();
        }
        this->pushTop();
        *fTop = {vertexIdxDelta, pt};
    }

    int close() {
        if (fTop == &fVertexStack[0]) {  // The stack only contains one point (the starting point).
            return fTotalClosedVertexCount;
        }
        // We will count vertices by walking the stack backwards.
        int finalVertexCount = 1;
        // Add an implicit line back to the starting point, then triangulate the rest of the
        // polygon. Since we simply have to finish now, we aren't picky anymore about making the
        // vertexIdxDeltas match.
        const SkPoint& p0 = fVertexStack[0].fPoint;
        SkASSERT(fTop->fPoint != p0);  // We should have detected and handled this case earlier.
        while (fTop > &fVertexStack[1]) {
            *fTriangleData++ = {fTop[-1].fPoint, fTop->fPoint, p0};
            finalVertexCount += fTop->fVertexIdxDelta;
            this->popTop();
        }
        finalVertexCount += fTop->fVertexIdxDelta;
        this->popTop();
        SkASSERT(fTop == &fVertexStack[0]);
        SkASSERT(fVertexStack[0].fVertexIdxDelta == 0);
        int numTriangles = finalVertexCount - 2;
        SkASSERT(numTriangles >= 0);
        fTotalClosedVertexCount += numTriangles * 3;
        return fTotalClosedVertexCount;
    }

    void closeAndMove(const SkPoint& startPt) {
        this->close();
        SkASSERT(fTop == &fVertexStack[0]);  // The stack should only contain a starting point now.
        fTop->fPoint = startPt;  // Modify the starting point.
        SkASSERT(fTop->fVertexIdxDelta == 0);  // Ensure we are in the initial stack state.
    }

private:
    struct StackVertex {
        // How many polygon vertices away is this vertex from the previous vertex on the stack?
        // i.e., the ith stack element's vertex index in the original polygon is:
        //
        //     fVertexStack[i].fVertexIdxDelta + fVertexStack[i - 1].fVertexIdxDelta + ... +
        //     fVertexStack[1].fVertexIdxDelta.
        //
        // NOTE: fVertexStack[0].fVertexIdxDelta always == 0.
        int fVertexIdxDelta;
        SkPoint fPoint;
    };

    void pushTop() {
        ++fTop;
        // We should never push deeper than fStackAllocCount.
        SkASSERT(fTop < &fVertexStack[fStackAllocCount]);
    }

    void popTop() {
        SkASSERT(fTop > &fVertexStack[0]);  // We should never pop the starting point.
        --fTop;
    }

    constexpr static int kStackPreallocCount = 32;
    SkAutoSTMalloc<kStackPreallocCount, StackVertex> fVertexStack;
    SkDEBUGCODE(int fStackAllocCount;)
    StackVertex* fTop;
    std::array<SkPoint, 3>* fTriangleData;
    int fTotalClosedVertexCount = 0;
};

#endif
