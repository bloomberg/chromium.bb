// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/ScrollableAreaPainter.h"

#include "core/layout/Layer.h"
#include "core/layout/LayerScrollableArea.h"
#include "core/layout/LayoutView.h"
#include "core/layout/PaintInfo.h"
#include "core/page/Page.h"
#include "core/paint/ScrollbarPainter.h"
#include "core/paint/TransformRecorder.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/GraphicsContextStateSaver.h"
#include "platform/graphics/paint/DrawingRecorder.h"

namespace blink {

void ScrollableAreaPainter::paintResizer(GraphicsContext* context, const IntPoint& paintOffset, const IntRect& damageRect)
{
    if (m_renderLayerScrollableArea.box().style()->resize() == RESIZE_NONE)
        return;

    IntRect absRect = m_renderLayerScrollableArea.resizerCornerRect(m_renderLayerScrollableArea.box().pixelSnappedBorderBoxRect(), ResizerForPointer);
    absRect.moveBy(paintOffset);
    if (!absRect.intersects(damageRect))
        return;

    if (m_renderLayerScrollableArea.resizer()) {
        ScrollbarPainter::paintIntoRect(m_renderLayerScrollableArea.resizer(), context, paintOffset, LayoutRect(absRect));
        return;
    }

    DrawingRecorder recorder(context, m_renderLayerScrollableArea.displayItemClient(), DisplayItem::Resizer, damageRect);
    if (recorder.canUseCachedDrawing())
        return;

    drawPlatformResizerImage(context, absRect);

    // Draw a frame around the resizer (1px grey line) if there are any scrollbars present.
    // Clipping will exclude the right and bottom edges of this frame.
    if (!m_renderLayerScrollableArea.hasOverlayScrollbars() && m_renderLayerScrollableArea.hasScrollbar()) {
        GraphicsContextStateSaver stateSaver(*context);
        context->clip(absRect);
        IntRect largerCorner = absRect;
        largerCorner.setSize(IntSize(largerCorner.width() + 1, largerCorner.height() + 1));
        context->setStrokeColor(Color(217, 217, 217));
        context->setStrokeThickness(1.0f);
        context->setFillColor(Color::transparent);
        context->drawRect(largerCorner);
    }
}

void ScrollableAreaPainter::drawPlatformResizerImage(GraphicsContext* context, IntRect resizerCornerRect)
{
    float deviceScaleFactor = blink::deviceScaleFactor(m_renderLayerScrollableArea.box().frame());

    RefPtr<Image> resizeCornerImage;
    IntSize cornerResizerSize;
    if (deviceScaleFactor >= 2) {
        DEFINE_STATIC_REF(Image, resizeCornerImageHiRes, (Image::loadPlatformResource("textAreaResizeCorner@2x")));
        resizeCornerImage = resizeCornerImageHiRes;
        cornerResizerSize = resizeCornerImage->size();
        cornerResizerSize.scale(0.5f);
    } else {
        DEFINE_STATIC_REF(Image, resizeCornerImageLoRes, (Image::loadPlatformResource("textAreaResizeCorner")));
        resizeCornerImage = resizeCornerImageLoRes;
        cornerResizerSize = resizeCornerImage->size();
    }

    if (m_renderLayerScrollableArea.box().style()->shouldPlaceBlockDirectionScrollbarOnLogicalLeft()) {
        context->save();
        context->translate(resizerCornerRect.x() + cornerResizerSize.width(), resizerCornerRect.y() + resizerCornerRect.height() - cornerResizerSize.height());
        context->scale(-1.0, 1.0);
        context->drawImage(resizeCornerImage.get(), IntRect(IntPoint(), cornerResizerSize));
        context->restore();
        return;
    }
    IntRect imageRect(resizerCornerRect.maxXMaxYCorner() - cornerResizerSize, cornerResizerSize);
    context->drawImage(resizeCornerImage.get(), imageRect);
}

void ScrollableAreaPainter::paintOverflowControls(GraphicsContext* context, const IntPoint& paintOffset, const IntRect& damageRect, bool paintingOverlayControls)
{
    // Don't do anything if we have no overflow.
    if (!m_renderLayerScrollableArea.box().hasOverflowClip())
        return;

    IntPoint adjustedPaintOffset = paintOffset;
    if (paintingOverlayControls)
        adjustedPaintOffset = m_renderLayerScrollableArea.cachedOverlayScrollbarOffset();

    IntRect localDamageRect = damageRect;
    localDamageRect.moveBy(-adjustedPaintOffset);

    // Overlay scrollbars paint in a second pass through the layer tree so that they will paint
    // on top of everything else. If this is the normal painting pass, paintingOverlayControls
    // will be false, and we should just tell the root layer that there are overlay scrollbars
    // that need to be painted. That will cause the second pass through the layer tree to run,
    // and we'll paint the scrollbars then. In the meantime, cache tx and ty so that the
    // second pass doesn't need to re-enter the RenderTree to get it right.
    if (m_renderLayerScrollableArea.hasOverlayScrollbars() && !paintingOverlayControls) {
        m_renderLayerScrollableArea.setCachedOverlayScrollbarOffset(paintOffset);
        // It's not necessary to do the second pass if the scrollbars paint into layers.
        if ((m_renderLayerScrollableArea.horizontalScrollbar() && m_renderLayerScrollableArea.layerForHorizontalScrollbar()) || (m_renderLayerScrollableArea.verticalScrollbar() && m_renderLayerScrollableArea.layerForVerticalScrollbar()))
            return;
        if (!overflowControlsIntersectRect(localDamageRect))
            return;

        LayoutView* layoutView = m_renderLayerScrollableArea.box().view();

        Layer* paintingRoot = m_renderLayerScrollableArea.layer()->enclosingLayerWithCompositedLayerMapping(IncludeSelf);
        if (!paintingRoot)
            paintingRoot = layoutView->layer();

        paintingRoot->setContainsDirtyOverlayScrollbars(true);
        return;
    }

    // This check is required to avoid painting custom CSS scrollbars twice.
    if (paintingOverlayControls && !m_renderLayerScrollableArea.hasOverlayScrollbars())
        return;

    {
        TransformRecorder translateRecorder(*context, m_renderLayerScrollableArea.displayItemClient(), AffineTransform::translation(adjustedPaintOffset.x(), adjustedPaintOffset.y()));
        if (m_renderLayerScrollableArea.horizontalScrollbar() && !m_renderLayerScrollableArea.layerForHorizontalScrollbar())
            m_renderLayerScrollableArea.horizontalScrollbar()->paint(context, localDamageRect);
        if (m_renderLayerScrollableArea.verticalScrollbar() && !m_renderLayerScrollableArea.layerForVerticalScrollbar())
            m_renderLayerScrollableArea.verticalScrollbar()->paint(context, localDamageRect);
    }

    if (m_renderLayerScrollableArea.layerForScrollCorner())
        return;

    // We fill our scroll corner with white if we have a scrollbar that doesn't run all the way up to the
    // edge of the box.
    paintScrollCorner(context, adjustedPaintOffset, damageRect);

    // Paint our resizer last, since it sits on top of the scroll corner.
    paintResizer(context, adjustedPaintOffset, damageRect);
}

bool ScrollableAreaPainter::overflowControlsIntersectRect(const IntRect& localRect) const
{
    const IntRect borderBox = m_renderLayerScrollableArea.box().pixelSnappedBorderBoxRect();

    if (m_renderLayerScrollableArea.rectForHorizontalScrollbar(borderBox).intersects(localRect))
        return true;

    if (m_renderLayerScrollableArea.rectForVerticalScrollbar(borderBox).intersects(localRect))
        return true;

    if (m_renderLayerScrollableArea.scrollCornerRect().intersects(localRect))
        return true;

    if (m_renderLayerScrollableArea.resizerCornerRect(borderBox, ResizerForPointer).intersects(localRect))
        return true;

    return false;
}


void ScrollableAreaPainter::paintScrollCorner(GraphicsContext* context, const IntPoint& paintOffset, const IntRect& damageRect)
{
    IntRect absRect = m_renderLayerScrollableArea.scrollCornerRect();
    absRect.moveBy(paintOffset);
    if (!absRect.intersects(damageRect))
        return;

    if (m_renderLayerScrollableArea.scrollCorner()) {
        ScrollbarPainter::paintIntoRect(m_renderLayerScrollableArea.scrollCorner(), context, paintOffset, LayoutRect(absRect));
        return;
    }

    DrawingRecorder recorder(context, m_renderLayerScrollableArea.displayItemClient(), DisplayItem::ScrollbarCorner, damageRect);
    if (recorder.canUseCachedDrawing())
        return;

    // We don't want to paint white if we have overlay scrollbars, since we need
    // to see what is behind it.
    if (!m_renderLayerScrollableArea.hasOverlayScrollbars())
        context->fillRect(absRect, Color::white);
}

} // namespace blink
