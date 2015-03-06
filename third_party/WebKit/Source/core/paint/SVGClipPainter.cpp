// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/SVGClipPainter.h"

#include "core/dom/ElementTraversal.h"
#include "core/layout/PaintInfo.h"
#include "core/layout/svg/LayoutSVGResourceClipper.h"
#include "core/layout/svg/SVGResources.h"
#include "core/layout/svg/SVGResourcesCache.h"
#include "core/paint/CompositingRecorder.h"
#include "core/paint/TransformRecorder.h"
#include "platform/graphics/paint/ClipPathDisplayItem.h"
#include "platform/graphics/paint/CompositingDisplayItem.h"
#include "platform/graphics/paint/DisplayItemList.h"
#include "platform/graphics/paint/DrawingDisplayItem.h"
#include "wtf/TemporaryChange.h"

namespace blink {

bool SVGClipPainter::applyStatefulResource(LayoutObject* object, GraphicsContext* context, ClipperState& clipperState)
{
    ASSERT(object);
    ASSERT(context);

    m_clip.clearInvalidationMask();

    return applyClippingToContext(object, object->objectBoundingBox(), object->paintInvalidationRectInLocalCoordinates(), context, clipperState);
}

class SVGClipExpansionCycleHelper {
public:
    SVGClipExpansionCycleHelper(LayoutSVGResourceClipper& clip) : m_clip(clip) { clip.beginClipExpansion(); }
    ~SVGClipExpansionCycleHelper() { m_clip.endClipExpansion(); }
private:
    LayoutSVGResourceClipper& m_clip;
};

bool SVGClipPainter::applyClippingToContext(LayoutObject* target, const FloatRect& targetBoundingBox,
    const FloatRect& paintInvalidationRect, GraphicsContext* context, ClipperState& clipperState)
{
    ASSERT(target);
    ASSERT(context);
    ASSERT(clipperState == ClipperNotApplied);
    ASSERT_WITH_SECURITY_IMPLICATION(!m_clip.needsLayout());

    if (paintInvalidationRect.isEmpty() || m_clip.hasCycle())
        return false;

    SVGClipExpansionCycleHelper inClipExpansionChange(m_clip);

    AffineTransform animatedLocalTransform = toSVGClipPathElement(m_clip.element())->calculateAnimatedLocalTransform();
    // When drawing a clip for non-SVG elements, the CTM does not include the zoom factor.
    // In this case, we need to apply the zoom scale explicitly - but only for clips with
    // userSpaceOnUse units (the zoom is accounted for objectBoundingBox-resolved lengths).
    if (!target->isSVG() && m_clip.clipPathUnits() == SVGUnitTypes::SVG_UNIT_TYPE_USERSPACEONUSE) {
        ASSERT(m_clip.style());
        animatedLocalTransform.scale(m_clip.style()->effectiveZoom());
    }

    // First, try to apply the clip as a clipPath.
    if (m_clip.tryPathOnlyClipping(target->displayItemClient(), context, animatedLocalTransform, targetBoundingBox)) {
        clipperState = ClipperAppliedPath;
        return true;
    }

    // Fall back to masking.
    clipperState = ClipperAppliedMask;

    // Begin compositing the clip mask.
    CompositingRecorder::beginCompositing(context, target->displayItemClient(), SkXfermode::kSrcOver_Mode, 1, &paintInvalidationRect);
    {
        TransformRecorder recorder(*context, target->displayItemClient(), animatedLocalTransform);

        // clipPath can also be clipped by another clipPath.
        SVGResources* resources = SVGResourcesCache::cachedResourcesForLayoutObject(&m_clip);
        LayoutSVGResourceClipper* clipPathClipper = resources ? resources->clipper() : 0;
        ClipperState clipPathClipperState = ClipperNotApplied;
        if (clipPathClipper && !SVGClipPainter(*clipPathClipper).applyClippingToContext(&m_clip, targetBoundingBox, paintInvalidationRect, context, clipPathClipperState)) {
            // End the clip mask's compositor.
            CompositingRecorder::endCompositing(context, target->displayItemClient());
            return false;
        }

        drawClipMaskContent(context, target->displayItemClient(), targetBoundingBox);

        if (clipPathClipper)
            SVGClipPainter(*clipPathClipper).postApplyStatefulResource(&m_clip, context, clipPathClipperState);
    }

    // Masked content layer start.
    CompositingRecorder::beginCompositing(context, target->displayItemClient(), SkXfermode::kSrcIn_Mode, 1, &paintInvalidationRect);

    return true;
}

void SVGClipPainter::postApplyStatefulResource(LayoutObject* target, GraphicsContext* context, ClipperState& clipperState)
{
    switch (clipperState) {
    case ClipperAppliedPath:
        // Path-only clipping, no layers to restore but we need to emit an end to the clip path display item.
        if (RuntimeEnabledFeatures::slimmingPaintEnabled()) {
            context->displayItemList()->add(EndClipPathDisplayItem::create(target->displayItemClient()));
        } else {
            EndClipPathDisplayItem endClipPathDisplayItem(target->displayItemClient());
            endClipPathDisplayItem.replay(context);
        }
        break;
    case ClipperAppliedMask:
        // Transfer content -> clip mask (SrcIn)
        CompositingRecorder::endCompositing(context, target->displayItemClient());

        // Transfer clip mask -> bg (SrcOver)
        CompositingRecorder::endCompositing(context, target->displayItemClient());
        break;
    default:
        ASSERT_NOT_REACHED();
    }
}

void SVGClipPainter::drawClipMaskContent(GraphicsContext* context, DisplayItemClient client, const FloatRect& targetBoundingBox)
{
    ASSERT(context);

    AffineTransform contentTransformation;
    RefPtr<const SkPicture> clipContentPicture = m_clip.createContentPicture(contentTransformation, targetBoundingBox);

    TransformRecorder recorder(*context, client, contentTransformation);
    if (RuntimeEnabledFeatures::slimmingPaintEnabled()) {
        ASSERT(context->displayItemList());
        context->displayItemList()->add(DrawingDisplayItem::create(client, DisplayItem::SVGClip, clipContentPicture));
    } else {
        DrawingDisplayItem clipPicture(client, DisplayItem::SVGClip, clipContentPicture);
        clipPicture.replay(context);
    }
}

}
