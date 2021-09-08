/*
 * Copyright 2015 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "src/gpu/GrAuditTrail.h"
#include "src/gpu/GrGpu.h"
#include "src/gpu/GrSurfaceDrawContext.h"
#include "src/gpu/geometry/GrStyledShape.h"
#include "src/gpu/ops/GrDashLinePathRenderer.h"
#include "src/gpu/ops/GrDashOp.h"
#include "src/gpu/ops/GrMeshDrawOp.h"

GrPathRenderer::CanDrawPath
GrDashLinePathRenderer::onCanDrawPath(const CanDrawPathArgs& args) const {
    SkPoint pts[2];
    bool inverted;
    if (args.fShape->style().isDashed() && args.fShape->asLine(pts, &inverted)) {
        // We should never have an inverse dashed case.
        SkASSERT(!inverted);
        if (!GrDashOp::CanDrawDashLine(pts, args.fShape->style(), *args.fViewMatrix)) {
            return CanDrawPath::kNo;
        }
        return CanDrawPath::kYes;
    }
    return CanDrawPath::kNo;
}

bool GrDashLinePathRenderer::onDrawPath(const DrawPathArgs& args) {
    GR_AUDIT_TRAIL_AUTO_FRAME(args.fRenderTargetContext->auditTrail(),
                              "GrDashLinePathRenderer::onDrawPath");
    GrDashOp::AAMode aaMode;
    switch (args.fAAType) {
        case GrAAType::kNone:
            aaMode = GrDashOp::AAMode::kNone;
            break;
        case GrAAType::kMSAA:
            if (args.fRenderTargetContext->canUseDynamicMSAA()) {
                // In DMSAA we avoid using MSAA, in order to reduce the number of MSAA triggers.
                aaMode = GrDashOp::AAMode::kCoverage;
            } else {
                // In this mode we will use aa between dashes but the outer border uses MSAA.
                // Otherwise, we can wind up with external edges antialiased and internal edges
                // unantialiased.
                aaMode = GrDashOp::AAMode::kCoverageWithMSAA;
            }
            break;
        case GrAAType::kCoverage:
            aaMode = GrDashOp::AAMode::kCoverage;
            break;
    }
    SkPoint pts[2];
    SkAssertResult(args.fShape->asLine(pts, nullptr));
    GrOp::Owner op =
            GrDashOp::MakeDashLineOp(args.fContext, std::move(args.fPaint), *args.fViewMatrix, pts,
                                     aaMode, args.fShape->style(), args.fUserStencilSettings);
    if (!op) {
        return false;
    }
    args.fRenderTargetContext->addDrawOp(args.fClip, std::move(op));
    return true;
}
