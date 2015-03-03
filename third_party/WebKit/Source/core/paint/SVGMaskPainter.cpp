// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/SVGMaskPainter.h"

#include "core/layout/PaintInfo.h"
#include "core/layout/svg/LayoutSVGResourceMasker.h"
#include "core/paint/CompositingRecorder.h"
#include "core/paint/TransformRecorder.h"
#include "platform/graphics/paint/CompositingDisplayItem.h"
#include "platform/graphics/paint/DisplayItemList.h"
#include "platform/graphics/paint/DrawingDisplayItem.h"

namespace blink {

bool SVGMaskPainter::prepareEffect(LayoutObject* object, GraphicsContext* context)
{
    ASSERT(object);
    ASSERT(context);
    ASSERT(m_mask.style());
    ASSERT_WITH_SECURITY_IMPLICATION(!m_mask.needsLayout());

    m_mask.clearInvalidationMask();

    FloatRect paintInvalidationRect = object->paintInvalidationRectInLocalCoordinates();
    if (paintInvalidationRect.isEmpty() || !m_mask.element()->hasChildren())
        return false;

    if (RuntimeEnabledFeatures::slimmingPaintEnabled()) {
        ASSERT(context->displayItemList());
        context->displayItemList()->add(BeginCompositingDisplayItem::create(object->displayItemClient(), WebCoreCompositeToSkiaComposite(context->compositeOperationDeprecated(), WebBlendModeNormal), 1, &paintInvalidationRect));
    } else {
        BeginCompositingDisplayItem beginCompositingContent(object->displayItemClient(), WebCoreCompositeToSkiaComposite(context->compositeOperationDeprecated(), WebBlendModeNormal), 1, &paintInvalidationRect);
        beginCompositingContent.replay(context);
    }

    return true;
}

void SVGMaskPainter::finishEffect(LayoutObject* object, GraphicsContext* context)
{
    ASSERT(object);
    ASSERT(context);
    ASSERT(m_mask.style());
    ASSERT_WITH_SECURITY_IMPLICATION(!m_mask.needsLayout());

    FloatRect paintInvalidationRect = object->paintInvalidationRectInLocalCoordinates();
    {
        ColorFilter maskLayerFilter = m_mask.style()->svgStyle().maskType() == MT_LUMINANCE
            ? ColorFilterLuminanceToAlpha : ColorFilterNone;
        CompositingRecorder maskCompositing(context, object->displayItemClient(), SkXfermode::kDstIn_Mode, 1, &paintInvalidationRect, maskLayerFilter);
        drawMaskForRenderer(context, object->displayItemClient(), object->objectBoundingBox());
    }

    if (RuntimeEnabledFeatures::slimmingPaintEnabled()) {
        ASSERT(context->displayItemList());
        context->displayItemList()->add(EndCompositingDisplayItem::create(object->displayItemClient()));
    } else {
        EndCompositingDisplayItem endCompositingContent(object->displayItemClient());
        endCompositingContent.replay(context);
    }
}

void SVGMaskPainter::drawMaskForRenderer(GraphicsContext* context, DisplayItemClient client, const FloatRect& targetBoundingBox)
{
    ASSERT(context);

    AffineTransform contentTransformation;
    RefPtr<const SkPicture> maskContentPicture = m_mask.getContentPicture(contentTransformation, targetBoundingBox);

    TransformRecorder recorder(*context, client, contentTransformation);

    if (RuntimeEnabledFeatures::slimmingPaintEnabled()) {
        ASSERT(context->displayItemList());
        context->displayItemList()->add(DrawingDisplayItem::create(client, DisplayItem::SVGMask, maskContentPicture));
    } else {
        DrawingDisplayItem maskPicture(client, DisplayItem::SVGMask, maskContentPicture);
        maskPicture.replay(context);
    }
}

}
