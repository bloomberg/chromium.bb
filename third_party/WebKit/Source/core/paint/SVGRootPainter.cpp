// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/SVGRootPainter.h"

#include "core/paint/BoxPainter.h"
#include "core/rendering/PaintInfo.h"
#include "core/rendering/svg/RenderSVGRoot.h"
#include "core/rendering/svg/SVGRenderingContext.h"
#include "core/rendering/svg/SVGResources.h"
#include "core/rendering/svg/SVGResourcesCache.h"
#include "core/svg/SVGSVGElement.h"

namespace blink {

void SVGRootPainter::paint(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
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
        SVGResources* resources = SVGResourcesCache::cachedResourcesForRenderObject(&m_renderSVGRoot);
        if (!resources || !resources->filter())
            return;
    }

    // Make a copy of the PaintInfo because applyTransform will modify the damage rect.
    PaintInfo childPaintInfo(paintInfo);
    GraphicsContextStateSaver stateSaver(*childPaintInfo.context);

    // Apply initial viewport clip.
    if (m_renderSVGRoot.shouldApplyViewportClip())
        childPaintInfo.context->clip(pixelSnappedIntRect(m_renderSVGRoot.overflowClipRect(paintOffset)));

    // Convert from container offsets (html renderers) to a relative transform (svg renderers).
    // Transform from our paint container's coordinate system to our local coords.
    IntPoint adjustedPaintOffset = roundedIntPoint(paintOffset);
    childPaintInfo.applyTransform(AffineTransform::translation(adjustedPaintOffset.x(), adjustedPaintOffset.y()) * m_renderSVGRoot.localToBorderBoxTransform());

    SVGRenderingContext renderingContext;
    if (childPaintInfo.phase == PaintPhaseForeground) {
        renderingContext.prepareToRenderSVGContent(&m_renderSVGRoot, childPaintInfo);
        if (!renderingContext.isRenderingPrepared())
            return;
    }

    BoxPainter(m_renderSVGRoot).paint(childPaintInfo, LayoutPoint());
}

} // namespace blink
