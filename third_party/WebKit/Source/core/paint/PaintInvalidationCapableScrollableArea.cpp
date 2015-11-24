// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/PaintInvalidationCapableScrollableArea.h"

#include "core/frame/Settings.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/layout/LayoutBox.h"
#include "core/layout/LayoutScrollbar.h"
#include "core/layout/LayoutScrollbarPart.h"
#include "core/layout/PaintInvalidationState.h"
#include "core/paint/PaintLayer.h"

namespace blink {

void PaintInvalidationCapableScrollableArea::willRemoveScrollbar(Scrollbar* scrollbar, ScrollbarOrientation orientation)
{
    if (!scrollbar->isCustomScrollbar()
        && !(orientation == HorizontalScrollbar ? layerForHorizontalScrollbar() : layerForVerticalScrollbar()))
        boxForScrollControlPaintInvalidation().invalidateDisplayItemClient(*scrollbar);

    ScrollableArea::willRemoveScrollbar(scrollbar, orientation);
}

// Returns true if the scroll control is invalidated.
static bool invalidatePaintOfScrollControlIfNeeded(const IntRect& scrollControlRect, LayoutRect& previousPaintInvalidationRect, bool needsPaintInvalidation, LayoutBox& box, const PaintInvalidationState& paintInvalidationState, const LayoutBoxModelObject& paintInvalidationContainer)
{
    LayoutRect paintInvalidationRect(scrollControlRect);
    if (!paintInvalidationRect.isEmpty())
        PaintLayer::mapRectToPaintInvalidationBacking(&box, &paintInvalidationContainer, paintInvalidationRect, &paintInvalidationState);

    if (paintInvalidationRect != previousPaintInvalidationRect) {
        box.invalidatePaintUsingContainer(paintInvalidationContainer, previousPaintInvalidationRect, PaintInvalidationScroll);
        previousPaintInvalidationRect = paintInvalidationRect;
        needsPaintInvalidation = true;
    }

    if (needsPaintInvalidation) {
        box.invalidatePaintUsingContainer(paintInvalidationContainer, paintInvalidationRect, PaintInvalidationScroll);
        return true;
    }

    return false;
}

static void invalidatePaintOfScrollbarIfNeeded(Scrollbar* scrollbar, GraphicsLayer* scrollbarGraphicsLayer, LayoutRect& previousPaintInvalidationRect, bool needsPaintInvalidation, LayoutBox& box, const PaintInvalidationState& paintInvalidationState, const LayoutBoxModelObject& paintInvalidationContainer)
{
    IntRect scrollbarRect = scrollbar && !scrollbarGraphicsLayer ? scrollbar->frameRect() : IntRect();
    if (!invalidatePaintOfScrollControlIfNeeded(scrollbarRect, previousPaintInvalidationRect, needsPaintInvalidation, box, paintInvalidationState, paintInvalidationContainer))
        return;
    if (!scrollbar)
        return;

    paintInvalidationContainer.invalidateDisplayItemClientOnBacking(*scrollbar, PaintInvalidationScroll, &previousPaintInvalidationRect);
    box.enclosingLayer()->setNeedsRepaint();
    if (scrollbar->isCustomScrollbar())
        toLayoutScrollbar(scrollbar)->invalidateDisplayItemClientsOfScrollbarParts(paintInvalidationContainer, previousPaintInvalidationRect);
}

void PaintInvalidationCapableScrollableArea::invalidatePaintOfScrollControlsIfNeeded(const PaintInvalidationState& paintInvalidationState, const LayoutBoxModelObject& paintInvalidationContainer)
{
    LayoutBox& box = boxForScrollControlPaintInvalidation();
    LayoutRect oldRect = m_horizontalScrollbarPreviousPaintInvalidationRect;
    invalidatePaintOfScrollbarIfNeeded(horizontalScrollbar(), layerForHorizontalScrollbar(), m_horizontalScrollbarPreviousPaintInvalidationRect, horizontalScrollbarNeedsPaintInvalidation(), box, paintInvalidationState, paintInvalidationContainer);
    bool scrollbarGeometryChanged = oldRect != m_horizontalScrollbarPreviousPaintInvalidationRect;

    oldRect = m_verticalScrollbarPreviousPaintInvalidationRect;
    invalidatePaintOfScrollbarIfNeeded(verticalScrollbar(), layerForVerticalScrollbar(), m_verticalScrollbarPreviousPaintInvalidationRect, verticalScrollbarNeedsPaintInvalidation(), box, paintInvalidationState, paintInvalidationContainer);
    scrollbarGeometryChanged |= oldRect != m_verticalScrollbarPreviousPaintInvalidationRect;

    if (scrollbarGeometryChanged)
        paintInvalidationContainer.invalidateDisplayItemClientOnBacking(box, PaintInvalidationScroll, nullptr);

    if (invalidatePaintOfScrollControlIfNeeded(scrollCornerRect(), m_scrollCornerPreviousPaintInvalidationRect, scrollCornerNeedsPaintInvalidation(), box, paintInvalidationState, paintInvalidationContainer)) {
        if (LayoutScrollbarPart* scrollCorner = this->scrollCorner())
            scrollCorner->invalidateDisplayItemClientsIncludingNonCompositingDescendants(&paintInvalidationContainer, PaintInvalidationScroll, &m_scrollCornerPreviousPaintInvalidationRect);
        if (LayoutScrollbarPart* resizer = this->resizer())
            resizer->invalidateDisplayItemClientsIncludingNonCompositingDescendants(&paintInvalidationContainer, PaintInvalidationScroll, &m_scrollCornerPreviousPaintInvalidationRect);
    }

    clearNeedsPaintInvalidationForScrollControls();
}

} // namespace blink
