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

#include "core/css/PseudoStyleRequest.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/editing/FrameSelection.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/page/EventHandler.h"
#include "core/page/Frame.h"
#include "core/page/FrameView.h"
#include "core/page/Page.h"
#include "core/page/scrolling/ScrollingCoordinator.h"
#include "core/platform/ScrollAnimator.h"
#include "core/platform/graphics/GraphicsLayer.h"
#include "core/rendering/RenderLayerCompositor.h"
#include "core/rendering/RenderScrollbar.h"
#include "core/rendering/RenderScrollbarPart.h"
#include "core/rendering/RenderView.h"

namespace WebCore {

RenderLayerScrollableArea::RenderLayerScrollableArea(RenderLayer* layer)
    : m_layer(layer)
    , m_scrollDimensionsDirty(true)
    , m_inOverflowRelayout(false)
    , m_scrollCorner(0)
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

    destroyScrollbar(HorizontalScrollbar);
    destroyScrollbar(VerticalScrollbar);

    if (m_scrollCorner)
        m_scrollCorner->destroy();
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
    if (scrollbar == m_vBar.get()) {
        if (GraphicsLayer* layer = layerForVerticalScrollbar()) {
            layer->setNeedsDisplayInRect(rect);
            return;
        }
    } else {
        if (GraphicsLayer* layer = layerForHorizontalScrollbar()) {
            layer->setNeedsDisplayInRect(rect);
            return;
        }
    }

    IntRect scrollRect = rect;
    RenderBox* box = toRenderBox(renderer());
    // If we are not yet inserted into the tree, there is no need to repaint.
    if (!box->parent())
        return;

    if (scrollbar == m_vBar.get())
        scrollRect.move(verticalScrollbarStart(0, box->width()), box->borderTop());
    else
        scrollRect.move(horizontalScrollbarStart(0), box->height() - box->borderBottom() - scrollbar->height());
    renderer()->repaintRectangle(scrollRect);
}

void RenderLayerScrollableArea::invalidateScrollCornerRect(const IntRect& rect)
{
    if (GraphicsLayer* layer = layerForScrollCorner()) {
        layer->setNeedsDisplayInRect(rect);
        return;
    }

    if (m_scrollCorner)
        m_scrollCorner->repaintRectangle(rect);
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

IntRect RenderLayerScrollableArea::convertFromScrollbarToContainingView(const Scrollbar* scrollbar, const IntRect& scrollbarRect) const
{
    RenderView* view = renderer()->view();
    if (!view)
        return scrollbarRect;

    IntRect rect = scrollbarRect;
    rect.move(scrollbarOffset(scrollbar));

    return view->frameView()->convertFromRenderer(renderer(), rect);
}

IntRect RenderLayerScrollableArea::convertFromContainingViewToScrollbar(const Scrollbar* scrollbar, const IntRect& parentRect) const
{
    RenderView* view = renderer()->view();
    if (!view)
        return parentRect;

    IntRect rect = view->frameView()->convertToRenderer(renderer(), parentRect);
    rect.move(-scrollbarOffset(scrollbar));
    return rect;
}

IntPoint RenderLayerScrollableArea::convertFromScrollbarToContainingView(const Scrollbar* scrollbar, const IntPoint& scrollbarPoint) const
{
    RenderView* view = renderer()->view();
    if (!view)
        return scrollbarPoint;

    IntPoint point = scrollbarPoint;
    point.move(scrollbarOffset(scrollbar));
    return view->frameView()->convertFromRenderer(renderer(), point);
}

IntPoint RenderLayerScrollableArea::convertFromContainingViewToScrollbar(const Scrollbar* scrollbar, const IntPoint& parentPoint) const
{
    RenderView* view = renderer()->view();
    if (!view)
        return parentPoint;

    IntPoint point = view->frameView()->convertToRenderer(renderer(), parentPoint);

    point.move(-scrollbarOffset(scrollbar));
    return point;
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
    RenderBox* box = toRenderBox(renderer());
    // List box parts handle the scrollbars by themselves so we have nothing to do.
    if (box->style()->appearance() == ListboxPart)
        return;

    m_scrollDimensionsDirty = true;
    IntSize originalScrollOffset = adjustedScrollOffset();

    computeScrollDimensions();

    if (!box->isMarquee()) {
        // Layout may cause us to be at an invalid scroll position. In this case we need
        // to pull our scroll offsets back to the max (or push them up to the min).
        IntSize clampedScrollOffset = clampScrollOffset(adjustedScrollOffset());
        if (clampedScrollOffset != adjustedScrollOffset())
            scrollToOffset(clampedScrollOffset);
    }

    if (originalScrollOffset != adjustedScrollOffset())
        scrollToOffsetWithoutAnimation(-scrollOrigin() + adjustedScrollOffset());

    bool hasHorizontalOverflow = this->hasHorizontalOverflow();
    bool hasVerticalOverflow = this->hasVerticalOverflow();

    // overflow:scroll should just enable/disable.
    if (renderer()->style()->overflowX() == OSCROLL)
        horizontalScrollbar()->setEnabled(hasHorizontalOverflow);
    if (renderer()->style()->overflowY() == OSCROLL)
        verticalScrollbar()->setEnabled(hasVerticalOverflow);

    // overflow:auto may need to lay out again if scrollbars got added/removed.
    bool autoHorizontalScrollBarChanged = box->hasAutoHorizontalScrollbar() && (hasHorizontalScrollbar() != hasHorizontalOverflow);
    bool autoVerticalScrollBarChanged = box->hasAutoVerticalScrollbar() && (hasVerticalScrollbar() != hasVerticalOverflow);

    if (autoHorizontalScrollBarChanged || autoVerticalScrollBarChanged) {
        if (box->hasAutoHorizontalScrollbar())
            setHasHorizontalScrollbar(hasHorizontalOverflow);
        if (box->hasAutoVerticalScrollbar())
            setHasVerticalScrollbar(hasVerticalOverflow);

        m_layer->updateSelfPaintingLayer();

        // Force an update since we know the scrollbars have changed things.
        if (renderer()->document().hasAnnotatedRegions())
            renderer()->document().setAnnotatedRegionsDirty(true);

        renderer()->repaint();

        if (renderer()->style()->overflowX() == OAUTO || renderer()->style()->overflowY() == OAUTO) {
            if (!m_inOverflowRelayout) {
                // Our proprietary overflow: overlay value doesn't trigger a layout.
                m_inOverflowRelayout = true;
                SubtreeLayoutScope layoutScope(renderer());
                layoutScope.setNeedsLayout(renderer());
                if (renderer()->isRenderBlock()) {
                    RenderBlock* block = toRenderBlock(renderer());
                    block->scrollbarsChanged(autoHorizontalScrollBarChanged, autoVerticalScrollBarChanged);
                    block->layoutBlock(true);
                } else {
                    renderer()->layout();
                }
                m_inOverflowRelayout = false;
            }
        }
    }

    // Set up the range (and page step/line step).
    if (Scrollbar* horizontalScrollbar = this->horizontalScrollbar()) {
        int clientWidth = box->pixelSnappedClientWidth();
        horizontalScrollbar->setProportion(clientWidth, overflowRect().width());
    }
    if (Scrollbar* verticalScrollbar = this->verticalScrollbar()) {
        int clientHeight = box->pixelSnappedClientHeight();
        verticalScrollbar->setProportion(clientHeight, overflowRect().height());
    }

    m_layer->updateScrollableAreaSet(hasScrollableHorizontalOverflow() || hasScrollableVerticalOverflow());
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

static bool overflowRequiresScrollbar(EOverflow overflow)
{
    return overflow == OSCROLL;
}

static bool overflowDefinesAutomaticScrollbar(EOverflow overflow)
{
    return overflow == OAUTO || overflow == OOVERLAY;
}

void RenderLayerScrollableArea::updateAfterStyleChange(const RenderStyle* oldStyle)
{
    // Overflow are a box concept.
    if (!renderer()->isBox())
        return;

    RenderBox* box = toRenderBox(renderer());

    // List box parts handle the scrollbars by themselves so we have nothing to do.
    if (box->style()->appearance() == ListboxPart)
        return;

    if (!m_scrollDimensionsDirty)
        m_layer->updateScrollableAreaSet(hasScrollableHorizontalOverflow() || hasScrollableVerticalOverflow());

    EOverflow overflowX = box->style()->overflowX();
    EOverflow overflowY = box->style()->overflowY();

    // To avoid doing a relayout in updateScrollbarsAfterLayout, we try to keep any automatic scrollbar that was already present.
    bool needsHorizontalScrollbar = (hasHorizontalScrollbar() && overflowDefinesAutomaticScrollbar(overflowX)) || overflowRequiresScrollbar(overflowX);
    bool needsVerticalScrollbar = (hasVerticalScrollbar() && overflowDefinesAutomaticScrollbar(overflowY)) || overflowRequiresScrollbar(overflowY);
    setHasHorizontalScrollbar(needsHorizontalScrollbar);
    setHasVerticalScrollbar(needsVerticalScrollbar);

    // With overflow: scroll, scrollbars are always visible but may be disabled.
    // When switching to another value, we need to re-enable them (see bug 11985).
    if (needsHorizontalScrollbar && oldStyle && oldStyle->overflowX() == OSCROLL && overflowX != OSCROLL) {
        ASSERT(hasHorizontalScrollbar());
        m_hBar->setEnabled(true);
    }

    if (needsVerticalScrollbar && oldStyle && oldStyle->overflowY() == OSCROLL && overflowY != OSCROLL) {
        ASSERT(hasVerticalScrollbar());
        m_vBar->setEnabled(true);
    }

    // FIXME: Need to detect a swap from custom to native scrollbars (and vice versa).
    if (m_hBar)
        m_hBar->styleChanged();
    if (m_vBar)
        m_vBar->styleChanged();

    updateScrollCornerStyle();
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

IntRect RenderLayerScrollableArea::rectForHorizontalScrollbar(const IntRect& borderBoxRect) const
{
    if (!m_hBar)
        return IntRect();

    const RenderBox* box = toRenderBox(renderer());
    const IntRect& scrollCorner = scrollCornerRect();

    return IntRect(horizontalScrollbarStart(borderBoxRect.x()),
        borderBoxRect.maxY() - box->borderBottom() - m_hBar->height(),
        borderBoxRect.width() - (box->borderLeft() + box->borderRight()) - scrollCorner.width(),
        m_hBar->height());
}

IntRect RenderLayerScrollableArea::rectForVerticalScrollbar(const IntRect& borderBoxRect) const
{
    if (!m_vBar)
        return IntRect();

    const RenderBox* box = toRenderBox(renderer());
    const IntRect& scrollCorner = scrollCornerRect();

    return IntRect(verticalScrollbarStart(borderBoxRect.x(), borderBoxRect.maxX()),
        borderBoxRect.y() + box->borderTop(),
        m_vBar->width(),
        borderBoxRect.height() - (box->borderTop() + box->borderBottom()) - scrollCorner.height());
}

LayoutUnit RenderLayerScrollableArea::verticalScrollbarStart(int minX, int maxX) const
{
    const RenderBox* box = toRenderBox(renderer());
    if (renderer()->style()->shouldPlaceBlockDirectionScrollbarOnLogicalLeft())
        return minX + box->borderLeft();
    return maxX - box->borderRight() - m_vBar->width();
}

LayoutUnit RenderLayerScrollableArea::horizontalScrollbarStart(int minX) const
{
    const RenderBox* box = toRenderBox(renderer());
    int x = minX + box->borderLeft();
    if (renderer()->style()->shouldPlaceBlockDirectionScrollbarOnLogicalLeft())
        x += m_vBar ? m_vBar->width() : m_layer->resizerCornerRect(box->pixelSnappedBorderBoxRect(), ResizerForPointer).width();
    return x;
}

IntSize RenderLayerScrollableArea::scrollbarOffset(const Scrollbar* scrollbar) const
{
    RenderBox* box = toRenderBox(renderer());

    if (scrollbar == m_vBar.get())
        return IntSize(verticalScrollbarStart(0, box->width()), box->borderTop());

    if (scrollbar == m_hBar.get())
        return IntSize(horizontalScrollbarStart(0), box->height() - box->borderBottom() - scrollbar->height());

    ASSERT_NOT_REACHED();
    return IntSize();
}

static inline RenderObject* rendererForScrollbar(RenderObject* renderer)
{
    if (Node* node = renderer->node()) {
        if (ShadowRoot* shadowRoot = node->containingShadowRoot()) {
            if (shadowRoot->type() == ShadowRoot::UserAgentShadowRoot)
                return shadowRoot->host()->renderer();
        }
    }

    return renderer;
}

PassRefPtr<Scrollbar> RenderLayerScrollableArea::createScrollbar(ScrollbarOrientation orientation)
{
    RefPtr<Scrollbar> widget;
    RenderObject* actualRenderer = rendererForScrollbar(renderer());
    bool hasCustomScrollbarStyle = actualRenderer->isBox() && actualRenderer->style()->hasPseudoStyle(SCROLLBAR);
    if (hasCustomScrollbarStyle) {
        widget = RenderScrollbar::createCustomScrollbar(this, orientation, actualRenderer->node());
    } else {
        widget = Scrollbar::create(this, orientation, RegularScrollbar);
        if (orientation == HorizontalScrollbar)
            didAddHorizontalScrollbar(widget.get());
        else
            didAddVerticalScrollbar(widget.get());
    }
    renderer()->document().view()->addChild(widget.get());
    return widget.release();
}

void RenderLayerScrollableArea::destroyScrollbar(ScrollbarOrientation orientation)
{
    RefPtr<Scrollbar>& scrollbar = orientation == HorizontalScrollbar ? m_hBar : m_vBar;
    if (!scrollbar)
        return;

    if (!scrollbar->isCustomScrollbar()) {
        if (orientation == HorizontalScrollbar)
            willRemoveHorizontalScrollbar(scrollbar.get());
        else
            willRemoveVerticalScrollbar(scrollbar.get());
    }

    scrollbar->removeFromParent();
    scrollbar->disconnectFromScrollableArea();
    scrollbar = 0;
}

void RenderLayerScrollableArea::setHasHorizontalScrollbar(bool hasScrollbar)
{
    if (hasScrollbar == hasHorizontalScrollbar())
        return;

    if (hasScrollbar)
        m_hBar = createScrollbar(HorizontalScrollbar);
    else
        destroyScrollbar(HorizontalScrollbar);

    // Destroying or creating one bar can cause our scrollbar corner to come and go. We need to update the opposite scrollbar's style.
    if (m_hBar)
        m_hBar->styleChanged();
    if (m_vBar)
        m_vBar->styleChanged();

    // Force an update since we know the scrollbars have changed things.
    if (renderer()->document().hasAnnotatedRegions())
        renderer()->document().setAnnotatedRegionsDirty(true);
}

void RenderLayerScrollableArea::setHasVerticalScrollbar(bool hasScrollbar)
{
    if (hasScrollbar == hasVerticalScrollbar())
        return;

    if (hasScrollbar)
        m_vBar = createScrollbar(VerticalScrollbar);
    else
        destroyScrollbar(VerticalScrollbar);

    // Destroying or creating one bar can cause our scrollbar corner to come and go. We need to update the opposite scrollbar's style.
    if (m_hBar)
        m_hBar->styleChanged();
    if (m_vBar)
        m_vBar->styleChanged();

    // Force an update since we know the scrollbars have changed things.
    if (renderer()->document().hasAnnotatedRegions())
        renderer()->document().setAnnotatedRegionsDirty(true);
}

int RenderLayerScrollableArea::verticalScrollbarWidth(OverlayScrollbarSizeRelevancy relevancy) const
{
    if (!m_vBar || (m_vBar->isOverlayScrollbar() && (relevancy == IgnoreOverlayScrollbarSize || !m_vBar->shouldParticipateInHitTesting())))
        return 0;
    return m_vBar->width();
}

int RenderLayerScrollableArea::horizontalScrollbarHeight(OverlayScrollbarSizeRelevancy relevancy) const
{
    if (!m_hBar || (m_hBar->isOverlayScrollbar() && (relevancy == IgnoreOverlayScrollbarSize || !m_hBar->shouldParticipateInHitTesting())))
        return 0;
    return m_hBar->height();
}


void RenderLayerScrollableArea::positionOverflowControls(const IntSize& offsetFromRoot)
{
    const IntRect borderBox = toRenderBox(renderer())->pixelSnappedBorderBoxRect();
    if (Scrollbar* verticalScrollbar = this->verticalScrollbar()) {
        IntRect vBarRect = rectForVerticalScrollbar(borderBox);
        vBarRect.move(offsetFromRoot);
        verticalScrollbar->setFrameRect(vBarRect);
    }

    if (Scrollbar* horizontalScrollbar = this->horizontalScrollbar()) {
        IntRect hBarRect = rectForHorizontalScrollbar(borderBox);
        hBarRect.move(offsetFromRoot);
        horizontalScrollbar->setFrameRect(hBarRect);
    }

    const IntRect& scrollCorner = scrollCornerRect();
    if (m_scrollCorner)
        m_scrollCorner->setFrameRect(scrollCorner);
}

void RenderLayerScrollableArea::updateScrollCornerStyle()
{
    RenderObject* actualRenderer = rendererForScrollbar(renderer());
    RefPtr<RenderStyle> corner = renderer()->hasOverflowClip() ? actualRenderer->getUncachedPseudoStyle(PseudoStyleRequest(SCROLLBAR_CORNER), actualRenderer->style()) : PassRefPtr<RenderStyle>(0);
    if (corner) {
        if (!m_scrollCorner) {
            m_scrollCorner = RenderScrollbarPart::createAnonymous(&renderer()->document());
            m_scrollCorner->setParent(renderer());
        }
        m_scrollCorner->setStyle(corner.release());
    } else if (m_scrollCorner) {
        m_scrollCorner->destroy();
        m_scrollCorner = 0;
    }
}

// FIXME: Move m_cachedOverlayScrollbarOffset.
void RenderLayerScrollableArea::paintOverflowControls(GraphicsContext* context, const IntPoint& paintOffset, const IntRect& damageRect, bool paintingOverlayControls)
{
    // Overlay scrollbars paint in a second pass through the layer tree so that they will paint
    // on top of everything else. If this is the normal painting pass, paintingOverlayControls
    // will be false, and we should just tell the root layer that there are overlay scrollbars
    // that need to be painted. That will cause the second pass through the layer tree to run,
    // and we'll paint the scrollbars then. In the meantime, cache tx and ty so that the
    // second pass doesn't need to re-enter the RenderTree to get it right.
    if (hasOverlayScrollbars() && !paintingOverlayControls) {
        m_layer->m_cachedOverlayScrollbarOffset = paintOffset;
        // It's not necessary to do the second pass if the scrollbars paint into layers.
        if ((m_hBar && layerForHorizontalScrollbar()) || (m_vBar && layerForVerticalScrollbar()))
            return;
        IntRect localDamgeRect = damageRect;
        localDamgeRect.moveBy(-paintOffset);
        if (!m_layer->overflowControlsIntersectRect(localDamgeRect))
            return;

        RenderView* renderView = renderer()->view();

        RenderLayer* paintingRoot = m_layer->enclosingCompositingLayer();
        if (!paintingRoot)
            paintingRoot = renderView->layer();

        paintingRoot->setContainsDirtyOverlayScrollbars(true);
        return;
    }

    // This check is required to avoid painting custom CSS scrollbars twice.
    if (paintingOverlayControls && !hasOverlayScrollbars())
        return;

    // Now that we're sure the scrollbars are in the right place, paint them.
    if (m_hBar && !layerForHorizontalScrollbar())
        m_hBar->paint(context, damageRect);
    if (m_vBar && !layerForVerticalScrollbar())
        m_vBar->paint(context, damageRect);

    if (layerForScrollCorner())
        return;

    // We fill our scroll corner with white if we have a scrollbar that doesn't run all the way up to the
    // edge of the box.
    IntPoint adjustedPaintOffset = paintOffset;
    if (paintingOverlayControls)
        adjustedPaintOffset = m_layer->m_cachedOverlayScrollbarOffset;

    paintScrollCorner(context, adjustedPaintOffset, damageRect);
}

void RenderLayerScrollableArea::paintScrollCorner(GraphicsContext* context, const IntPoint& paintOffset, const IntRect& damageRect)
{
    ASSERT(renderer()->isBox());

    IntRect absRect = scrollCornerRect();
    absRect.moveBy(paintOffset);
    if (!absRect.intersects(damageRect))
        return;

    if (context->updatingControlTints()) {
        updateScrollCornerStyle();
        return;
    }

    if (m_scrollCorner) {
        m_scrollCorner->paintIntoRect(context, paintOffset, absRect);
        return;
    }

    // We don't want to paint white if we have overlay scrollbars, since we need
    // to see what is behind it.
    if (!hasOverlayScrollbars())
        context->fillRect(absRect, Color::white);
}

bool RenderLayerScrollableArea::hitTestOverflowControls(HitTestResult& result, const IntPoint& localPoint, const IntRect& resizeControlRect)
{
    RenderBox* box = toRenderBox(renderer());

    int resizeControlSize = max(resizeControlRect.height(), 0);
    if (m_vBar && m_vBar->shouldParticipateInHitTesting()) {
        LayoutRect vBarRect(verticalScrollbarStart(0, box->width()),
            box->borderTop(),
            m_vBar->width(),
            box->height() - (box->borderTop() + box->borderBottom()) - (m_hBar ? m_hBar->height() : resizeControlSize));
        if (vBarRect.contains(localPoint)) {
            result.setScrollbar(m_vBar.get());
            return true;
        }
    }

    resizeControlSize = max(resizeControlRect.width(), 0);
    if (m_hBar && m_hBar->shouldParticipateInHitTesting()) {
        LayoutRect hBarRect(horizontalScrollbarStart(0),
            box->height() - box->borderBottom() - m_hBar->height(),
            box->width() - (box->borderLeft() + box->borderRight()) - (m_vBar ? m_vBar->width() : resizeControlSize),
            m_hBar->height());
        if (hBarRect.contains(localPoint)) {
            result.setScrollbar(m_hBar.get());
            return true;
        }
    }

    // FIXME: We should hit test the m_scrollCorner and pass it back through the result.

    return false;
}

} // Namespace WebCore
