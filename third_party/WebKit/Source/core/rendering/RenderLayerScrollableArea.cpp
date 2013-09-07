/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2010, 2011, 2012 Apple Inc. All rights reserved.
 *
 * Portions are Copyright (C) 1998 Netscape Communications Corporation.
 *
 * Other contributors:
 *   Robert O'Callahan <roc+@cs.cmu.edu>
 *   David Baron <dbaron@fas.harvard.edu>
 *   Christian Biesinger <cbiesinger@web.de>
 *   Randall Jesup <rjesup@wgate.com>
 *   Roland Mainz <roland.mainz@informatik.med.uni-giessen.de>
 *   Josh Soref <timeless@mac.com>
 *   Boris Zbarsky <bzbarsky@mit.edu>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Alternatively, the contents of this file may be used under the terms
 * of either the Mozilla Public License Version 1.1, found at
 * http://www.mozilla.org/MPL/ (the "MPL") or the GNU General Public
 * License Version 2.0, found at http://www.fsf.org/copyleft/gpl.html
 * (the "GPL"), in which case the provisions of the MPL or the GPL are
 * applicable instead of those above.  If you wish to allow use of your
 * version of this file only under the terms of one of those two
 * licenses (the MPL or the GPL) and not to allow others to use your
 * version of this file under the LGPL, indicate your decision by
 * deletingthe provisions above and replace them with the notice and
 * other provisions required by the MPL or the GPL, as the case may be.
 * If you do not delete the provisions above, a recipient may use your
 * version of this file under any of the LGPL, the MPL or the GPL.
 */

#include "config.h"
#include "core/rendering/RenderLayer.h"

#include "core/editing/FrameSelection.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/page/EventHandler.h"
#include "core/page/Frame.h"
#include "core/page/FrameView.h"
#include "core/page/Page.h"
#include "core/page/scrolling/ScrollingCoordinator.h"
#include "core/platform/ScrollAnimator.h"
#include "core/rendering/RenderLayerCompositor.h"
#include "core/rendering/RenderView.h"

namespace WebCore {

RenderLayerScrollableArea::RenderLayerScrollableArea(RenderLayer* layer)
    : m_layer(layer)
    , m_scrollDimensionsDirty(true)
{
    ScrollableArea::setConstrainsScrollingToContentEdge(false);

    Node* node = renderer()->node();
    if (node && node->isElementNode()) {
        // We save and restore only the scrollOffset as the other scroll values are recalculated.
        Element* element = toElement(node);
        m_scrollOffset = element->savedLayerScrollOffset();
        if (!m_scrollOffset.isZero())
            scrollAnimator()->setCurrentPosition(FloatPoint(m_scrollOffset.width(), m_scrollOffset.height()));
        element->setSavedLayerScrollOffset(IntSize());
    }

}

RenderLayerScrollableArea::~RenderLayerScrollableArea()
{
    if (Frame* frame = renderer()->frame()) {
        if (FrameView* frameView = frame->view()) {
            frameView->removeScrollableArea(this);
        }
    }

    if (renderer()->frame() && renderer()->frame()->page()) {
        if (ScrollingCoordinator* scrollingCoordinator = renderer()->frame()->page()->scrollingCoordinator())
            scrollingCoordinator->willDestroyScrollableArea(this);
    }

    if (!renderer()->documentBeingDestroyed()) {
        Node* node = renderer()->node();
        if (node && node->isElementNode())
            toElement(node)->setSavedLayerScrollOffset(m_scrollOffset);
    }

}

Scrollbar* RenderLayerScrollableArea::horizontalScrollbar() const
{
    return m_layer->horizontalScrollbar();
}

Scrollbar* RenderLayerScrollableArea::verticalScrollbar() const
{
    return m_layer->verticalScrollbar();
}

ScrollableArea* RenderLayerScrollableArea::enclosingScrollableArea() const
{
    return m_layer->enclosingScrollableArea();
}

void RenderLayerScrollableArea::updateNeedsCompositedScrolling()
{
    m_layer->updateNeedsCompositedScrolling();
}

GraphicsLayer* RenderLayerScrollableArea::layerForScrolling() const
{
    return m_layer->layerForScrolling();
}

GraphicsLayer* RenderLayerScrollableArea::layerForHorizontalScrollbar() const
{
    return m_layer->layerForHorizontalScrollbar();
}

GraphicsLayer* RenderLayerScrollableArea::layerForVerticalScrollbar() const
{
    return m_layer->layerForVerticalScrollbar();
}

GraphicsLayer* RenderLayerScrollableArea::layerForScrollCorner() const
{
    return m_layer->layerForScrollCorner();
}

bool RenderLayerScrollableArea::usesCompositedScrolling() const
{
    return m_layer->usesCompositedScrolling();
}

void RenderLayerScrollableArea::invalidateScrollbarRect(Scrollbar* scrollbar, const IntRect& rect)
{
    m_layer->invalidateScrollbarRect(scrollbar, rect);
}

void RenderLayerScrollableArea::invalidateScrollCornerRect(const IntRect& rect)
{
    m_layer->invalidateScrollCornerRect(rect);
}

bool RenderLayerScrollableArea::isActive() const
{
    return m_layer->isActive();
}

bool RenderLayerScrollableArea::isScrollCornerVisible() const
{
    return m_layer->isScrollCornerVisible();
}

IntRect RenderLayerScrollableArea::scrollCornerRect() const
{
    return m_layer->scrollCornerRect();
}

IntRect RenderLayerScrollableArea::convertFromScrollbarToContainingView(const Scrollbar* scrollbar, const IntRect& rect) const
{
    return m_layer->convertFromScrollbarToContainingView(scrollbar, rect);
}

IntRect RenderLayerScrollableArea::convertFromContainingViewToScrollbar(const Scrollbar* scrollbar, const IntRect& rect) const
{
    return m_layer->convertFromContainingViewToScrollbar(scrollbar, rect);
}

IntPoint RenderLayerScrollableArea::convertFromScrollbarToContainingView(const Scrollbar* scrollbar, const IntPoint& point) const
{
    return m_layer->convertFromScrollbarToContainingView(scrollbar, point);
}

IntPoint RenderLayerScrollableArea::convertFromContainingViewToScrollbar(const Scrollbar* scrollbar, const IntPoint& point) const
{
    return m_layer->convertFromContainingViewToScrollbar(scrollbar, point);
}

int RenderLayerScrollableArea::scrollSize(ScrollbarOrientation orientation) const
{
    return m_layer->scrollSize(orientation);
}

void RenderLayerScrollableArea::setScrollOffset(const IntPoint& newScrollOffset)
{
    if (!toRenderBox(renderer())->isMarquee()) {
        // Ensure that the dimensions will be computed if they need to be (for overflow:hidden blocks).
        if (m_scrollDimensionsDirty)
            computeScrollDimensions();
    }

    if (scrollOffset() == toIntSize(newScrollOffset))
        return;

    setScrollOffset(toIntSize(newScrollOffset));

    Frame* frame = renderer()->frame();
    InspectorInstrumentation::willScrollLayer(renderer());

    RenderView* view = renderer()->view();

    // We should have a RenderView if we're trying to scroll.
    ASSERT(view);

    // Update the positions of our child layers (if needed as only fixed layers should be impacted by a scroll).
    // We don't update compositing layers, because we need to do a deep update from the compositing ancestor.
    bool inLayout = view ? view->frameView()->isInLayout() : false;
    if (!inLayout) {
        // If we're in the middle of layout, we'll just update layers once layout has finished.
        m_layer->updateLayerPositionsAfterOverflowScroll();
        if (view) {
            // Update regions, scrolling may change the clip of a particular region.
            view->frameView()->updateAnnotatedRegions();
            view->updateWidgetPositions();
        }

        m_layer->updateCompositingLayersAfterScroll();
    }

    RenderLayerModelObject* repaintContainer = renderer()->containerForRepaint();
    if (frame) {
        // The caret rect needs to be invalidated after scrolling
        frame->selection().setCaretRectNeedsUpdate();

        FloatQuad quadForFakeMouseMoveEvent = FloatQuad(m_layer->m_repaintRect);
        if (repaintContainer)
            quadForFakeMouseMoveEvent = repaintContainer->localToAbsoluteQuad(quadForFakeMouseMoveEvent);
        frame->eventHandler()->dispatchFakeMouseMoveEventSoonInQuad(quadForFakeMouseMoveEvent);
    }

    bool requiresRepaint = true;

    if (m_layer->compositor()->inCompositingMode() && m_layer->usesCompositedScrolling())
        requiresRepaint = false;

    // Just schedule a full repaint of our object.
    if (view && requiresRepaint)
        renderer()->repaintUsingContainer(repaintContainer, pixelSnappedIntRect(m_layer->m_repaintRect));

    // Schedule the scroll DOM event.
    if (renderer()->node())
        renderer()->node()->document().eventQueue()->enqueueOrDispatchScrollEvent(renderer()->node(), DocumentEventQueue::ScrollEventElementTarget);

    InspectorInstrumentation::didScrollLayer(renderer());
}

IntPoint RenderLayerScrollableArea::scrollPosition() const
{
    return IntPoint(m_scrollOffset);
}

IntPoint RenderLayerScrollableArea::minimumScrollPosition() const
{
    return -scrollOrigin();
}

IntPoint RenderLayerScrollableArea::maximumScrollPosition() const
{
    RenderBox* box = toRenderBox(renderer());

    if (!box->hasOverflowClip())
        return -scrollOrigin();

    return -scrollOrigin() + enclosingIntRect(m_overflowRect).size() - enclosingIntRect(box->clientBoxRect()).size();
}

IntRect RenderLayerScrollableArea::visibleContentRect(VisibleContentRectIncludesScrollbars scrollbarInclusion) const
{
    int verticalScrollbarWidth = 0;
    int horizontalScrollbarHeight = 0;
    if (scrollbarInclusion == IncludeScrollbars) {
        verticalScrollbarWidth = (verticalScrollbar() && !verticalScrollbar()->isOverlayScrollbar()) ? verticalScrollbar()->width() : 0;
        horizontalScrollbarHeight = (horizontalScrollbar() && !horizontalScrollbar()->isOverlayScrollbar()) ? horizontalScrollbar()->height() : 0;
    }

    return IntRect(IntPoint(scrollXOffset(), scrollYOffset()),
        IntSize(max(0, m_layer->size().width() - verticalScrollbarWidth), max(0, m_layer->size().height() - horizontalScrollbarHeight)));
}

int RenderLayerScrollableArea::visibleHeight() const
{
    return m_layer->visibleHeight();
}

int RenderLayerScrollableArea::visibleWidth() const
{
    return m_layer->visibleWidth();
}

IntSize RenderLayerScrollableArea::contentsSize() const
{
    return IntSize(scrollWidth(), scrollHeight());
}

IntSize RenderLayerScrollableArea::overhangAmount() const
{
    return m_layer->overhangAmount();
}

IntPoint RenderLayerScrollableArea::lastKnownMousePosition() const
{
    return m_layer->lastKnownMousePosition();
}

bool RenderLayerScrollableArea::shouldSuspendScrollAnimations() const
{
    return m_layer->shouldSuspendScrollAnimations();
}

bool RenderLayerScrollableArea::scrollbarsCanBeActive() const
{
    return m_layer->scrollbarsCanBeActive();
}

IntRect RenderLayerScrollableArea::scrollableAreaBoundingBox() const
{
    return m_layer->scrollableAreaBoundingBox();
}

bool RenderLayerScrollableArea::userInputScrollable(ScrollbarOrientation orientation) const
{
    return m_layer->userInputScrollable(orientation);
}

bool RenderLayerScrollableArea::shouldPlaceVerticalScrollbarOnLeft() const
{
    return m_layer->shouldPlaceVerticalScrollbarOnLeft();
}

int RenderLayerScrollableArea::pageStep(ScrollbarOrientation orientation) const
{
    return m_layer->pageStep(orientation);
}

RenderLayerModelObject* RenderLayerScrollableArea::renderer() const
{
    // Only RenderBoxes can have a scrollable area, however we allocate an
    // RenderLayerScrollableArea for any renderers (FIXME).
    return m_layer->renderer();
}

int RenderLayerScrollableArea::scrollWidth() const
{
    RenderBox* box = toRenderBox(renderer());
    if (m_scrollDimensionsDirty)
        const_cast<RenderLayerScrollableArea*>(this)->computeScrollDimensions();
    return snapSizeToPixel(m_overflowRect.width(), box->clientLeft() + box->x());
}

int RenderLayerScrollableArea::scrollHeight() const
{
    RenderBox* box = toRenderBox(renderer());
    if (m_scrollDimensionsDirty)
        const_cast<RenderLayerScrollableArea*>(this)->computeScrollDimensions();
    return snapSizeToPixel(m_overflowRect.height(), box->clientTop() + box->y());
}

void RenderLayerScrollableArea::computeScrollDimensions()
{
    RenderBox* box = toRenderBox(renderer());

    m_scrollDimensionsDirty = false;

    m_overflowRect = box->layoutOverflowRect();
    box->flipForWritingMode(m_overflowRect);

    int scrollableLeftOverflow = m_overflowRect.x() - box->borderLeft();
    int scrollableTopOverflow = m_overflowRect.y() - box->borderTop();
    setScrollOrigin(IntPoint(-scrollableLeftOverflow, -scrollableTopOverflow));
}

void RenderLayerScrollableArea::scrollToOffset(const IntSize& scrollOffset, ScrollOffsetClamping clamp)
{
    IntSize newScrollOffset = clamp == ScrollOffsetClamped ? clampScrollOffset(scrollOffset) : scrollOffset;
    if (newScrollOffset != adjustedScrollOffset())
        scrollToOffsetWithoutAnimation(-scrollOrigin() + newScrollOffset);
}

void RenderLayerScrollableArea::updateAfterLayout()
{
    m_scrollDimensionsDirty = true;
    IntSize originalScrollOffset = adjustedScrollOffset();

    computeScrollDimensions();

    if (!toRenderBox(renderer())->isMarquee()) {
        // Layout may cause us to be at an invalid scroll position. In this case we need
        // to pull our scroll offsets back to the max (or push them up to the min).
        IntSize clampedScrollOffset = clampScrollOffset(adjustedScrollOffset());
        if (clampedScrollOffset != adjustedScrollOffset())
            scrollToOffset(clampedScrollOffset);
    }

    if (originalScrollOffset != adjustedScrollOffset())
        scrollToOffsetWithoutAnimation(-scrollOrigin() + adjustedScrollOffset());
}

bool RenderLayerScrollableArea::hasHorizontalOverflow() const
{
    ASSERT(!m_scrollDimensionsDirty);

    return scrollWidth() > toRenderBox(renderer())->pixelSnappedClientWidth();
}

bool RenderLayerScrollableArea::hasVerticalOverflow() const
{
    ASSERT(!m_scrollDimensionsDirty);

    return scrollHeight() > toRenderBox(renderer())->pixelSnappedClientHeight();
}

bool RenderLayerScrollableArea::hasScrollableHorizontalOverflow() const
{
    return hasHorizontalOverflow() && toRenderBox(renderer())->scrollsOverflowX();
}

bool RenderLayerScrollableArea::hasScrollableVerticalOverflow() const
{
    return hasVerticalOverflow() && toRenderBox(renderer())->scrollsOverflowY();
}

void RenderLayerScrollableArea::updateAfterStyleChange(const RenderStyle*)
{
    if (!m_scrollDimensionsDirty)
        m_layer->updateScrollableAreaSet(hasScrollableHorizontalOverflow() || hasScrollableVerticalOverflow());
}

IntSize RenderLayerScrollableArea::clampScrollOffset(const IntSize& scrollOffset) const
{
    RenderBox* box = toRenderBox(renderer());

    int maxX = scrollWidth() - box->pixelSnappedClientWidth();
    int maxY = scrollHeight() - box->pixelSnappedClientHeight();

    int x = std::max(std::min(scrollOffset.width(), maxX), 0);
    int y = std::max(std::min(scrollOffset.height(), maxY), 0);
    return IntSize(x, y);
}

} // Namespace WebCore
