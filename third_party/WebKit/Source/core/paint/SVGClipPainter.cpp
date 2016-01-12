// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/SVGClipPainter.h"

#include "core/dom/ElementTraversal.h"
#include "core/layout/svg/LayoutSVGResourceClipper.h"
#include "core/layout/svg/SVGResources.h"
#include "core/layout/svg/SVGResourcesCache.h"
#include "core/paint/LayoutObjectDrawingRecorder.h"
#include "core/paint/PaintInfo.h"
#include "core/paint/TransformRecorder.h"
#include "platform/graphics/paint/ClipPathDisplayItem.h"
#include "platform/graphics/paint/CompositingRecorder.h"
#include "platform/graphics/paint/DrawingDisplayItem.h"
#include "platform/graphics/paint/PaintController.h"

namespace blink {

namespace {

class SVGClipExpansionCycleHelper {
public:
    SVGClipExpansionCycleHelper(LayoutSVGResourceClipper& clip) : m_clip(clip) { clip.beginClipExpansion(); }
    ~SVGClipExpansionCycleHelper() { m_clip.endClipExpansion(); }
private:
    LayoutSVGResourceClipper& m_clip;
};

}

bool SVGClipPainter::prepareEffect(const LayoutObject& target, const FloatRect& targetBoundingBox,
    const FloatRect& paintInvalidationRect, GraphicsContext& context, ClipperState& clipperState)
{
    ASSERT(clipperState == ClipperNotApplied);
    ASSERT_WITH_SECURITY_IMPLICATION(!m_clip.needsLayout());

    m_clip.clearInvalidationMask();

    if (paintInvalidationRect.isEmpty() || m_clip.hasCycle())
        return false;

    SVGClipExpansionCycleHelper inClipExpansionChange(m_clip);

    AffineTransform animatedLocalTransform = toSVGClipPathElement(m_clip.element())->calculateAnimatedLocalTransform();
    // When drawing a clip for non-SVG elements, the CTM does not include the zoom factor.
    // In this case, we need to apply the zoom scale explicitly - but only for clips with
    // userSpaceOnUse units (the zoom is accounted for objectBoundingBox-resolved lengths).
    if (!target.isSVG() && m_clip.clipPathUnits() == SVGUnitTypes::SVG_UNIT_TYPE_USERSPACEONUSE) {
        ASSERT(m_clip.style());
        animatedLocalTransform.scale(m_clip.style()->effectiveZoom());
    }

    // First, try to apply the clip as a clipPath.
    Path clipPath;
    if (m_clip.asPath(animatedLocalTransform, targetBoundingBox, clipPath)) {
        clipperState = ClipperAppliedPath;
        context.paintController().createAndAppend<BeginClipPathDisplayItem>(target, clipPath);
        return true;
    }

    // Fall back to masking.
    clipperState = ClipperAppliedMask;

    // Begin compositing the clip mask.
    CompositingRecorder::beginCompositing(context, target, SkXfermode::kSrcOver_Mode, 1, &paintInvalidationRect);
    {
        TransformRecorder recorder(context, target, animatedLocalTransform);

        // clipPath can also be clipped by another clipPath.
        SVGResources* resources = SVGResourcesCache::cachedResourcesForLayoutObject(&m_clip);
        LayoutSVGResourceClipper* clipPathClipper = resources ? resources->clipper() : 0;
        ClipperState clipPathClipperState = ClipperNotApplied;
        if (clipPathClipper && !SVGClipPainter(*clipPathClipper).prepareEffect(m_clip, targetBoundingBox, paintInvalidationRect, context, clipPathClipperState)) {
            // End the clip mask's compositor.
            CompositingRecorder::endCompositing(context, target);
            return false;
        }

        drawClipMaskContent(context, target, targetBoundingBox, paintInvalidationRect);

        if (clipPathClipper)
            SVGClipPainter(*clipPathClipper).finishEffect(m_clip, context, clipPathClipperState);
    }

    // Masked content layer start.
    CompositingRecorder::beginCompositing(context, target, SkXfermode::kSrcIn_Mode, 1, &paintInvalidationRect);

    return true;
}

void SVGClipPainter::finishEffect(const LayoutObject& target, GraphicsContext& context, ClipperState& clipperState)
{
    switch (clipperState) {
    case ClipperAppliedPath:
        // Path-only clipping, no layers to restore but we need to emit an end to the clip path display item.
        context.paintController().endItem<EndClipPathDisplayItem>(target);
        break;
    case ClipperAppliedMask:
        // Transfer content -> clip mask (SrcIn)
        CompositingRecorder::endCompositing(context, target);

        // Transfer clip mask -> bg (SrcOver)
        CompositingRecorder::endCompositing(context, target);
        break;
    default:
        ASSERT_NOT_REACHED();
    }
}

void SVGClipPainter::drawClipMaskContent(GraphicsContext& context, const LayoutObject& layoutObject, const FloatRect& targetBoundingBox, const FloatRect& targetPaintInvalidationRect)
{
    AffineTransform contentTransformation;
    RefPtr<const SkPicture> clipContentPicture = m_clip.createContentPicture(contentTransformation, targetBoundingBox, context);

    if (LayoutObjectDrawingRecorder::useCachedDrawingIfPossible(context, layoutObject, DisplayItem::SVGClip, LayoutPoint()))
        return;

    LayoutObjectDrawingRecorder drawingRecorder(context, layoutObject, DisplayItem::SVGClip, targetPaintInvalidationRect, LayoutPoint());
    context.save();
    context.concatCTM(contentTransformation);
    context.drawPicture(clipContentPicture.get());
    context.restore();
}

}
