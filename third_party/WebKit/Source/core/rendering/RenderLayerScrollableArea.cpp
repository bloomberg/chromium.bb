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

RenderLayerScrollableArea::RenderLayerScrollableArea(RenderBox* box)
    : m_box(box)
    , m_scrollDimensionsDirty(true)
    , m_inOverflowRelayout(false)
    , m_scrollCorner(0)
{
    ScrollableArea::setConstrainsScrollingToContentEdge(false);

    Node* node = m_box->node();
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
    if (Frame* frame = m_box->frame()) {
        if (FrameView* frameView = frame->view()) {
            frameView->removeScrollableArea(this);
        }
    }

    if (m_box->frame() && m_box->frame()->page()) {
        if (ScrollingCoordinator* scrollingCoordinator = m_box->frame()->page()->scrollingCoordinator())
            scrollingCoordinator->willDestroyScrollableArea(this);
    }

    if (!m_box->documentBeingDestroyed()) {
        Node* node = m_box->node();
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
    return layer()->enclosingScrollableArea();
}

void RenderLayerScrollableArea::updateNeedsCompositedScrolling()
{
    layer()->updateNeedsCompositedScrolling();
}

GraphicsLayer* RenderLayerScrollableArea::layerForScrolling() const
{
    return layer()->layerForScrolling();
}

GraphicsLayer* RenderLayerScrollableArea::layerForHorizontalScrollbar() const
{
    return layer()->layerForHorizontalScrollbar();
}

GraphicsLayer* RenderLayerScrollableArea::layerForVerticalScrollbar() const
{
    return layer()->layerForVerticalScrollbar();
}

GraphicsLayer* RenderLayerScrollableArea::layerForScrollCorner() const
{
    return layer()->layerForScrollCorner();
}

bool RenderLayerScrollableArea::usesCompositedScrolling() const
{
    return layer()->usesCompositedScrolling();
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
    // If we are not yet inserted into the tree, there is no need to repaint.
    if (!m_box->parent())
        return;

    if (scrollbar == m_vBar.get())
        scrollRect.move(verticalScrollbarStart(0, m_box->width()), m_box->borderTop());
    else
        scrollRect.move(horizontalScrollbarStart(0), m_box->height() - m_box->borderBottom() - scrollbar->height());
    m_box->repaintRectangle(scrollRect);
}

void RenderLayerScrollableArea::invalidateScrollCornerRect(const IntRect& rect)
{
    if (GraphicsLayer* layer = layerForScrollCorner()) {
        layer->setNeedsDisplayInRect(rect);
        return;
    }

    if (m_scrollCorner)
        m_scrollCorner->repaintRectangle(rect);
    layer()->invalidateScrollCornerRect(rect);
}

bool RenderLayerScrollableArea::isActive() const
{
    return layer()->isActive();
}

bool RenderLayerScrollableArea::isScrollCornerVisible() const
{
    return layer()->isScrollCornerVisible();
}

IntRect RenderLayerScrollableArea::scrollCornerRect() const
{
    return layer()->scrollCornerRect();
}

IntRect RenderLayerScrollableArea::convertFromScrollbarToContainingView(const Scrollbar* scrollbar, const IntRect& scrollbarRect) const
{
    RenderView* view = m_box->view();
    if (!view)
        return scrollbarRect;

    IntRect rect = scrollbarRect;
    rect.move(scrollbarOffset(scrollbar));

    return view->frameView()->convertFromRenderer(m_box, rect);
}

IntRect RenderLayerScrollableArea::convertFromContainingViewToScrollbar(const Scrollbar* scrollbar, const IntRect& parentRect) const
{
    RenderView* view = m_box->view();
    if (!view)
        return parentRect;

    IntRect rect = view->frameView()->convertToRenderer(m_box, parentRect);
    rect.move(-scrollbarOffset(scrollbar));
    return rect;
}

IntPoint RenderLayerScrollableArea::convertFromScrollbarToContainingView(const Scrollbar* scrollbar, const IntPoint& scrollbarPoint) const
{
    RenderView* view = m_box->view();
    if (!view)
        return scrollbarPoint;

    IntPoint point = scrollbarPoint;
    point.move(scrollbarOffset(scrollbar));
    return view->frameView()->convertFromRenderer(m_box, point);
}

IntPoint RenderLayerScrollableArea::convertFromContainingViewToScrollbar(const Scrollbar* scrollbar, const IntPoint& parentPoint) const
{
    RenderView* view = m_box->view();
    if (!view)
        return parentPoint;

    IntPoint point = view->frameView()->convertToRenderer(m_box, parentPoint);

    point.move(-scrollbarOffset(scrollbar));
    return point;
}

int RenderLayerScrollableArea::scrollSize(ScrollbarOrientation orientation) const
{
    return layer()->scrollSize(orientation);
}

void RenderLayerScrollableArea::setScrollOffset(const IntPoint& newScrollOffset)
{
    if (!m_box->isMarquee()) {
        // Ensure that the dimensions will be computed if they need to be (for overflow:hidden blocks).
        if (m_scrollDimensionsDirty)
            computeScrollDimensions();
    }

    if (scrollOffset() == toIntSize(newScrollOffset))
        return;

    setScrollOffset(toIntSize(newScrollOffset));

    Frame* frame = m_box->frame();
    InspectorInstrumentation::willScrollLayer(m_box);

    RenderView* view = m_box->view();

    // We should have a RenderView if we're trying to scroll.
    ASSERT(view);

    // Update the positions of our child layers (if needed as only fixed layers should be impacted by a scroll).
    // We don't update compositing layers, because we need to do a deep update from the compositing ancestor.
    bool inLayout = view ? view->frameView()->isInLayout() : false;
    if (!inLayout) {
        // If we're in the middle of layout, we'll just update layers once layout has finished.
        layer()->updateLayerPositionsAfterOverflowScroll();
        if (view) {
            // Update regions, scrolling may change the clip of a particular region.
            view->frameView()->updateAnnotatedRegions();
            view->updateWidgetPositions();
        }

        layer()->updateCompositingLayersAfterScroll();
    }

    RenderLayerModelObject* repaintContainer = m_box->containerForRepaint();
    if (frame) {
        // The caret rect needs to be invalidated after scrolling
        frame->selection().setCaretRectNeedsUpdate();

        FloatQuad quadForFakeMouseMoveEvent = FloatQuad(layer()->m_repaintRect);
        if (repaintContainer)
            quadForFakeMouseMoveEvent = repaintContainer->localToAbsoluteQuad(quadForFakeMouseMoveEvent);
        frame->eventHandler()->dispatchFakeMouseMoveEventSoonInQuad(quadForFakeMouseMoveEvent);
    }

    bool requiresRepaint = true;

    if (layer()->compositor()->inCompositingMode() && layer()->usesCompositedScrolling())
        requiresRepaint = false;

    // Just schedule a full repaint of our object.
    if (view && requiresRepaint)
        m_box->repaintUsingContainer(repaintContainer, pixelSnappedIntRect(layer()->m_repaintRect));

    // Schedule the scroll DOM event.
    if (m_box->node())
        m_box->node()->document().eventQueue()->enqueueOrDispatchScrollEvent(m_box->node(), DocumentEventQueue::ScrollEventElementTarget);

    InspectorInstrumentation::didScrollLayer(m_box);
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
    if (!m_box->hasOverflowClip())
        return -scrollOrigin();

    return -scrollOrigin() + enclosingIntRect(m_overflowRect).size() - enclosingIntRect(m_box->clientBoxRect()).size();
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
        IntSize(max(0, layer()->size().width() - verticalScrollbarWidth), max(0, layer()->size().height() - horizontalScrollbarHeight)));
}

int RenderLayerScrollableArea::visibleHeight() const
{
    return layer()->visibleHeight();
}

int RenderLayerScrollableArea::visibleWidth() const
{
    return layer()->visibleWidth();
}

IntSize RenderLayerScrollableArea::contentsSize() const
{
    return IntSize(scrollWidth(), scrollHeight());
}

IntSize RenderLayerScrollableArea::overhangAmount() const
{
    return layer()->overhangAmount();
}

IntPoint RenderLayerScrollableArea::lastKnownMousePosition() const
{
    return layer()->lastKnownMousePosition();
}

bool RenderLayerScrollableArea::shouldSuspendScrollAnimations() const
{
    return layer()->shouldSuspendScrollAnimations();
}

bool RenderLayerScrollableArea::scrollbarsCanBeActive() const
{
    return layer()->scrollbarsCanBeActive();
}

IntRect RenderLayerScrollableArea::scrollableAreaBoundingBox() const
{
    return layer()->scrollableAreaBoundingBox();
}

bool RenderLayerScrollableArea::userInputScrollable(ScrollbarOrientation orientation) const
{
    return layer()->userInputScrollable(orientation);
}

bool RenderLayerScrollableArea::shouldPlaceVerticalScrollbarOnLeft() const
{
    return layer()->shouldPlaceVerticalScrollbarOnLeft();
}

int RenderLayerScrollableArea::pageStep(ScrollbarOrientation orientation) const
{
    return layer()->pageStep(orientation);
}

RenderLayer* RenderLayerScrollableArea::layer() const
{
    return m_box->layer();
}

int RenderLayerScrollableArea::scrollWidth() const
{
    if (m_scrollDimensionsDirty)
        const_cast<RenderLayerScrollableArea*>(this)->computeScrollDimensions();
    return snapSizeToPixel(m_overflowRect.width(), m_box->clientLeft() + m_box->x());
}

int RenderLayerScrollableArea::scrollHeight() const
{
    if (m_scrollDimensionsDirty)
        const_cast<RenderLayerScrollableArea*>(this)->computeScrollDimensions();
    return snapSizeToPixel(m_overflowRect.height(), m_box->clientTop() + m_box->y());
}

void RenderLayerScrollableArea::computeScrollDimensions()
{
    m_scrollDimensionsDirty = false;

    m_overflowRect = m_box->layoutOverflowRect();
    m_box->flipForWritingMode(m_overflowRect);

    int scrollableLeftOverflow = m_overflowRect.x() - m_box->borderLeft();
    int scrollableTopOverflow = m_overflowRect.y() - m_box->borderTop();
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
    // List box parts handle the scrollbars by themselves so we have nothing to do.
    if (m_box->style()->appearance() == ListboxPart)
        return;

    m_scrollDimensionsDirty = true;
    IntSize originalScrollOffset = adjustedScrollOffset();

    computeScrollDimensions();

    if (!m_box->isMarquee()) {
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
    if (m_box->style()->overflowX() == OSCROLL)
        horizontalScrollbar()->setEnabled(hasHorizontalOverflow);
    if (m_box->style()->overflowY() == OSCROLL)
        verticalScrollbar()->setEnabled(hasVerticalOverflow);

    // overflow:auto may need to lay out again if scrollbars got added/removed.
    bool autoHorizontalScrollBarChanged = m_box->hasAutoHorizontalScrollbar() && (hasHorizontalScrollbar() != hasHorizontalOverflow);
    bool autoVerticalScrollBarChanged = m_box->hasAutoVerticalScrollbar() && (hasVerticalScrollbar() != hasVerticalOverflow);

    if (autoHorizontalScrollBarChanged || autoVerticalScrollBarChanged) {
        if (m_box->hasAutoHorizontalScrollbar())
            setHasHorizontalScrollbar(hasHorizontalOverflow);
        if (m_box->hasAutoVerticalScrollbar())
            setHasVerticalScrollbar(hasVerticalOverflow);

        layer()->updateSelfPaintingLayer();

        // Force an update since we know the scrollbars have changed things.
        if (m_box->document().hasAnnotatedRegions())
            m_box->document().setAnnotatedRegionsDirty(true);

        m_box->repaint();

        if (m_box->style()->overflowX() == OAUTO || m_box->style()->overflowY() == OAUTO) {
            if (!m_inOverflowRelayout) {
                // Our proprietary overflow: overlay value doesn't trigger a layout.
                m_inOverflowRelayout = true;
                SubtreeLayoutScope layoutScope(m_box);
                layoutScope.setNeedsLayout(m_box);
                if (m_box->isRenderBlock()) {
                    RenderBlock* block = toRenderBlock(m_box);
                    block->scrollbarsChanged(autoHorizontalScrollBarChanged, autoVerticalScrollBarChanged);
                    block->layoutBlock(true);
                } else {
                    m_box->layout();
                }
                m_inOverflowRelayout = false;
            }
        }
    }

    // Set up the range (and page step/line step).
    if (Scrollbar* horizontalScrollbar = this->horizontalScrollbar()) {
        int clientWidth = m_box->pixelSnappedClientWidth();
        horizontalScrollbar->setProportion(clientWidth, overflowRect().width());
    }
    if (Scrollbar* verticalScrollbar = this->verticalScrollbar()) {
        int clientHeight = m_box->pixelSnappedClientHeight();
        verticalScrollbar->setProportion(clientHeight, overflowRect().height());
    }

    layer()->updateScrollableAreaSet(hasScrollableHorizontalOverflow() || hasScrollableVerticalOverflow());
}

bool RenderLayerScrollableArea::hasHorizontalOverflow() const
{
    ASSERT(!m_scrollDimensionsDirty);

    return scrollWidth() > m_box->pixelSnappedClientWidth();
}

bool RenderLayerScrollableArea::hasVerticalOverflow() const
{
    ASSERT(!m_scrollDimensionsDirty);

    return scrollHeight() > m_box->pixelSnappedClientHeight();
}

bool RenderLayerScrollableArea::hasScrollableHorizontalOverflow() const
{
    return hasHorizontalOverflow() && m_box->scrollsOverflowX();
}

bool RenderLayerScrollableArea::hasScrollableVerticalOverflow() const
{
    return hasVerticalOverflow() && m_box->scrollsOverflowY();
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
    // List box parts handle the scrollbars by themselves so we have nothing to do.
    if (m_box->style()->appearance() == ListboxPart)
        return;

    if (!m_scrollDimensionsDirty)
        layer()->updateScrollableAreaSet(hasScrollableHorizontalOverflow() || hasScrollableVerticalOverflow());

    EOverflow overflowX = m_box->style()->overflowX();
    EOverflow overflowY = m_box->style()->overflowY();

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
    int maxX = scrollWidth() - m_box->pixelSnappedClientWidth();
    int maxY = scrollHeight() - m_box->pixelSnappedClientHeight();

    int x = std::max(std::min(scrollOffset.width(), maxX), 0);
    int y = std::max(std::min(scrollOffset.height(), maxY), 0);
    return IntSize(x, y);
}

IntRect RenderLayerScrollableArea::rectForHorizontalScrollbar(const IntRect& borderBoxRect) const
{
    if (!m_hBar)
        return IntRect();

    const IntRect& scrollCorner = scrollCornerRect();

    return IntRect(horizontalScrollbarStart(borderBoxRect.x()),
        borderBoxRect.maxY() - m_box->borderBottom() - m_hBar->height(),
        borderBoxRect.width() - (m_box->borderLeft() + m_box->borderRight()) - scrollCorner.width(),
        m_hBar->height());
}

IntRect RenderLayerScrollableArea::rectForVerticalScrollbar(const IntRect& borderBoxRect) const
{
    if (!m_vBar)
        return IntRect();

    const IntRect& scrollCorner = scrollCornerRect();

    return IntRect(verticalScrollbarStart(borderBoxRect.x(), borderBoxRect.maxX()),
        borderBoxRect.y() + m_box->borderTop(),
        m_vBar->width(),
        borderBoxRect.height() - (m_box->borderTop() + m_box->borderBottom()) - scrollCorner.height());
}

LayoutUnit RenderLayerScrollableArea::verticalScrollbarStart(int minX, int maxX) const
{
    if (m_box->style()->shouldPlaceBlockDirectionScrollbarOnLogicalLeft())
        return minX + m_box->borderLeft();
    return maxX - m_box->borderRight() - m_vBar->width();
}

LayoutUnit RenderLayerScrollableArea::horizontalScrollbarStart(int minX) const
{
    int x = minX + m_box->borderLeft();
    if (m_box->style()->shouldPlaceBlockDirectionScrollbarOnLogicalLeft())
        x += m_vBar ? m_vBar->width() : layer()->resizerCornerRect(m_box->pixelSnappedBorderBoxRect(), ResizerForPointer).width();
    return x;
}

IntSize RenderLayerScrollableArea::scrollbarOffset(const Scrollbar* scrollbar) const
{
    if (scrollbar == m_vBar.get())
        return IntSize(verticalScrollbarStart(0, m_box->width()), m_box->borderTop());

    if (scrollbar == m_hBar.get())
        return IntSize(horizontalScrollbarStart(0), m_box->height() - m_box->borderBottom() - scrollbar->height());

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
    RenderObject* actualRenderer = rendererForScrollbar(m_box);
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
    m_box->document().view()->addChild(widget.get());
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
    if (m_box->document().hasAnnotatedRegions())
        m_box->document().setAnnotatedRegionsDirty(true);
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
    if (m_box->document().hasAnnotatedRegions())
        m_box->document().setAnnotatedRegionsDirty(true);
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
    const IntRect borderBox = m_box->pixelSnappedBorderBoxRect();
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
    RenderObject* actualRenderer = rendererForScrollbar(m_box);
    RefPtr<RenderStyle> corner = m_box->hasOverflowClip() ? actualRenderer->getUncachedPseudoStyle(PseudoStyleRequest(SCROLLBAR_CORNER), actualRenderer->style()) : PassRefPtr<RenderStyle>(0);
    if (corner) {
        if (!m_scrollCorner) {
            m_scrollCorner = RenderScrollbarPart::createAnonymous(&m_box->document());
            m_scrollCorner->setParent(m_box);
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
        layer()->m_cachedOverlayScrollbarOffset = paintOffset;
        // It's not necessary to do the second pass if the scrollbars paint into layers.
        if ((m_hBar && layerForHorizontalScrollbar()) || (m_vBar && layerForVerticalScrollbar()))
            return;
        IntRect localDamgeRect = damageRect;
        localDamgeRect.moveBy(-paintOffset);
        if (!layer()->overflowControlsIntersectRect(localDamgeRect))
            return;

        RenderView* renderView = m_box->view();

        RenderLayer* paintingRoot = layer()->enclosingCompositingLayer();
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
        adjustedPaintOffset = layer()->m_cachedOverlayScrollbarOffset;

    paintScrollCorner(context, adjustedPaintOffset, damageRect);
}

void RenderLayerScrollableArea::paintScrollCorner(GraphicsContext* context, const IntPoint& paintOffset, const IntRect& damageRect)
{
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
    int resizeControlSize = max(resizeControlRect.height(), 0);
    if (m_vBar && m_vBar->shouldParticipateInHitTesting()) {
        LayoutRect vBarRect(verticalScrollbarStart(0, m_box->width()),
            m_box->borderTop(),
            m_vBar->width(),
            m_box->height() - (m_box->borderTop() + m_box->borderBottom()) - (m_hBar ? m_hBar->height() : resizeControlSize));
        if (vBarRect.contains(localPoint)) {
            result.setScrollbar(m_vBar.get());
            return true;
        }
    }

    resizeControlSize = max(resizeControlRect.width(), 0);
    if (m_hBar && m_hBar->shouldParticipateInHitTesting()) {
        LayoutRect hBarRect(horizontalScrollbarStart(0),
            m_box->height() - m_box->borderBottom() - m_hBar->height(),
            m_box->width() - (m_box->borderLeft() + m_box->borderRight()) - (m_vBar ? m_vBar->width() : resizeControlSize),
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
