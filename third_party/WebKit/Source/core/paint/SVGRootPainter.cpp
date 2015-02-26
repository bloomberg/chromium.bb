// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/SVGRootPainter.h"

#include "core/layout/PaintInfo.h"
#include "core/layout/svg/LayoutSVGRoot.h"
#include "core/layout/svg/SVGResources.h"
#include "core/layout/svg/SVGResourcesCache.h"
#include "core/paint/BoxPainter.h"
#include "core/paint/SVGPaintContext.h"
#include "core/paint/TransformRecorder.h"
#include "core/svg/SVGSVGElement.h"
#include "platform/graphics/paint/ClipRecorder.h"

namespace blink {

void SVGRootPainter::paint(const PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    // An empty viewport disables rendering.
    if (m_renderSVGRoot.pixelSnappedBorderBoxRect().isEmpty())
        return;

    // SVG outlines are painted during PaintPhaseForeground.
    if (paintInfo.phase == PaintPhaseOutline || paintInfo.phase == PaintPhaseSelfOutline)
        return;

    // An empty viewBox also disables rendering.
    // (http://www.w3.org/TR/SVG/coords.html#ViewBoxAttribute)
    SVGSVGElement* svg = toSVGSVGElement(m_renderSVGRoot.node());
    ASSERT(svg);
    if (svg->hasEmptyViewBox())
        return;

    // Don't paint if we don't have kids, except if we have filters we should paint those.
    if (!m_renderSVGRoot.firstChild()) {
        SVGResources* resources = SVGResourcesCache::cachedResourcesForLayoutObject(&m_renderSVGRoot);
        if (!resources || !resources->filter())
            return;
    }

    PaintInfo paintInfoBeforeFiltering(paintInfo);

    // Apply initial viewport clip.
    OwnPtr<ClipRecorder> clipRecorder;
    if (m_renderSVGRoot.shouldApplyViewportClip())
        clipRecorder = adoptPtr(new ClipRecorder(m_renderSVGRoot.displayItemClient(), paintInfoBeforeFiltering.context, paintInfoBeforeFiltering.displayItemTypeForClipping(), LayoutRect(pixelSnappedIntRect(m_renderSVGRoot.overflowClipRect(paintOffset)))));

    // Convert from container offsets (html renderers) to a relative transform (svg renderers).
    // Transform from our paint container's coordinate system to our local coords.
    IntPoint adjustedPaintOffset = roundedIntPoint(paintOffset);
    TransformRecorder transformRecorder(*paintInfoBeforeFiltering.context, m_renderSVGRoot.displayItemClient(), AffineTransform::translation(adjustedPaintOffset.x(), adjustedPaintOffset.y()) * m_renderSVGRoot.localToBorderBoxTransform());

    // SVG doesn't use paintOffset internally but we need to bake it into the paint rect.
    paintInfoBeforeFiltering.rect.move(-adjustedPaintOffset.x(), -adjustedPaintOffset.y());

    SVGPaintContext paintContext(m_renderSVGRoot, paintInfoBeforeFiltering);
    if (paintContext.paintInfo().phase == PaintPhaseForeground && !paintContext.applyClipMaskAndFilterIfNecessary())
        return;

    BoxPainter(m_renderSVGRoot).paint(paintContext.paintInfo(), LayoutPoint());
}

} // namespace blink
