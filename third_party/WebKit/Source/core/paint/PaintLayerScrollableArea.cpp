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

#include "core/paint/PaintLayerScrollableArea.h"

#include "core/css/PseudoStyleRequest.h"
#include "core/dom/AXObjectCache.h"
#include "core/dom/Node.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/editing/FrameSelection.h"
#include "core/frame/FrameHost.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/input/EventHandler.h"
#include "core/layout/LayoutGeometryMap.h"
#include "core/layout/LayoutPart.h"
#include "core/layout/LayoutScrollbar.h"
#include "core/layout/LayoutScrollbarPart.h"
#include "core/layout/LayoutTheme.h"
#include "core/layout/LayoutView.h"
#include "core/layout/compositing/CompositedLayerMapping.h"
#include "core/layout/compositing/PaintLayerCompositor.h"
#include "core/loader/FrameLoaderClient.h"
#include "core/page/ChromeClient.h"
#include "core/page/FocusController.h"
#include "core/page/Page.h"
#include "core/page/scrolling/ScrollingCoordinator.h"
#include "core/paint/PaintLayerFragment.h"
#include "platform/PlatformGestureEvent.h"
#include "platform/PlatformMouseEvent.h"
#include "platform/graphics/CompositorMutableProperties.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/graphics/paint/DrawingRecorder.h"
#include "platform/scroll/ScrollAnimatorBase.h"
#include "platform/scroll/ScrollbarTheme.h"
#include "public/platform/Platform.h"

namespace blink {

const int ResizerControlExpandRatioForTouch = 2;

PaintLayerScrollableArea::PaintLayerScrollableArea(PaintLayer& layer)
    : m_layer(layer)
    , m_inResizeMode(false)
    , m_scrollsOverflow(false)
    , m_inOverflowRelayout(false)
    , m_nextTopmostScrollChild(0)
    , m_topmostScrollChild(0)
    , m_needsCompositedScrolling(false)
    , m_scrollbarManager(*this)
    , m_scrollCorner(nullptr)
    , m_resizer(nullptr)
    , m_scrollAnchor(this)
#if ENABLE(ASSERT)
    , m_hasBeenDisposed(false)
#endif
{
    Node* node = box().node();
    if (node && node->isElementNode()) {
        // We save and restore only the scrollOffset as the other scroll values are recalculated.
        Element* element = toElement(node);
        m_scrollOffset = element->savedLayerScrollOffset();
        if (!m_scrollOffset.isZero())
            scrollAnimator().setCurrentPosition(FloatPoint(m_scrollOffset.width(), m_scrollOffset.height()));
        element->setSavedLayerScrollOffset(IntSize());
    }
    updateResizerAreaSet();
}

PaintLayerScrollableArea::~PaintLayerScrollableArea()
{
    ASSERT(m_hasBeenDisposed);
}

void PaintLayerScrollableArea::dispose()
{
    if (inResizeMode() && !box().documentBeingDestroyed()) {
        if (LocalFrame* frame = box().frame())
            frame->eventHandler().resizeScrollableAreaDestroyed();
    }

    if (LocalFrame* frame = box().frame()) {
        if (FrameView* frameView = frame->view()) {
            frameView->removeScrollableArea(this);
            frameView->removeAnimatingScrollableArea(this);
        }
    }

    if (box().frame() && box().frame()->page()) {
        if (ScrollingCoordinator* scrollingCoordinator = box().frame()->page()->scrollingCoordinator())
            scrollingCoordinator->willDestroyScrollableArea(this);
    }

    if (!box().documentBeingDestroyed()) {
        Node* node = box().node();
        // FIXME: Make setSavedLayerScrollOffset take DoubleSize. crbug.com/414283.
        if (node && node->isElementNode())
            toElement(node)->setSavedLayerScrollOffset(flooredIntSize(m_scrollOffset));
    }

    if (LocalFrame* frame = box().frame()) {
        if (FrameView* frameView = frame->view())
            frameView->removeResizerArea(box());
    }

    m_scrollbarManager.dispose();

    if (m_scrollCorner)
        m_scrollCorner->destroy();
    if (m_resizer)
        m_resizer->destroy();

    clearScrollAnimators();

    if (RuntimeEnabledFeatures::scrollAnchoringEnabled())
        m_scrollAnchor.clear();

#if ENABLE(ASSERT)
    m_hasBeenDisposed = true;
#endif
}

DEFINE_TRACE(PaintLayerScrollableArea)
{
    visitor->trace(m_scrollbarManager);
    visitor->trace(m_scrollAnchor);
    ScrollableArea::trace(visitor);
}

HostWindow* PaintLayerScrollableArea::hostWindow() const
{
    if (Page* page = box().frame()->page())
        return &page->chromeClient();
    return nullptr;
}

GraphicsLayer* PaintLayerScrollableArea::layerForScrolling() const
{
    return layer()->hasCompositedLayerMapping() ? layer()->compositedLayerMapping()->scrollingContentsLayer() : 0;
}

GraphicsLayer* PaintLayerScrollableArea::layerForHorizontalScrollbar() const
{
    // See crbug.com/343132.
    DisableCompositingQueryAsserts disabler;

    return layer()->hasCompositedLayerMapping() ? layer()->compositedLayerMapping()->layerForHorizontalScrollbar() : 0;
}

GraphicsLayer* PaintLayerScrollableArea::layerForVerticalScrollbar() const
{
    // See crbug.com/343132.
    DisableCompositingQueryAsserts disabler;

    return layer()->hasCompositedLayerMapping() ? layer()->compositedLayerMapping()->layerForVerticalScrollbar() : 0;
}

GraphicsLayer* PaintLayerScrollableArea::layerForScrollCorner() const
{
    // See crbug.com/343132.
    DisableCompositingQueryAsserts disabler;

    return layer()->hasCompositedLayerMapping() ? layer()->compositedLayerMapping()->layerForScrollCorner() : 0;
}

void PaintLayerScrollableArea::scrollControlWasSetNeedsPaintInvalidation()
{
    box().setMayNeedPaintInvalidation();
}

bool PaintLayerScrollableArea::shouldUseIntegerScrollOffset() const
{
    Frame* frame = box().frame();
    if (frame->settings() && !frame->settings()->preferCompositingToLCDTextEnabled())
        return true;

    return ScrollableArea::shouldUseIntegerScrollOffset();
}

bool PaintLayerScrollableArea::isActive() const
{
    Page* page = box().frame()->page();
    return page && page->focusController().isActive();
}

bool PaintLayerScrollableArea::isScrollCornerVisible() const
{
    return !scrollCornerRect().isEmpty();
}

static int cornerStart(const LayoutBox& box, int minX, int maxX, int thickness)
{
    if (box.shouldPlaceBlockDirectionScrollbarOnLogicalLeft())
        return minX + box.styleRef().borderLeftWidth();
    return maxX - thickness - box.styleRef().borderRightWidth();
}

static IntRect cornerRect(const LayoutBox& box, const Scrollbar* horizontalScrollbar, const Scrollbar* verticalScrollbar, const IntRect& bounds)
{
    int horizontalThickness;
    int verticalThickness;
    if (!verticalScrollbar && !horizontalScrollbar) {
        // FIXME: This isn't right. We need to know the thickness of custom scrollbars
        // even when they don't exist in order to set the resizer square size properly.
        horizontalThickness = ScrollbarTheme::theme().scrollbarThickness();
        verticalThickness = horizontalThickness;
    } else if (verticalScrollbar && !horizontalScrollbar) {
        horizontalThickness = verticalScrollbar->scrollbarThickness();
        verticalThickness = horizontalThickness;
    } else if (horizontalScrollbar && !verticalScrollbar) {
        verticalThickness = horizontalScrollbar->scrollbarThickness();
        horizontalThickness = verticalThickness;
    } else {
        horizontalThickness = verticalScrollbar->scrollbarThickness();
        verticalThickness = horizontalScrollbar->scrollbarThickness();
    }
    return IntRect(cornerStart(box, bounds.x(), bounds.maxX(), horizontalThickness),
        bounds.maxY() - verticalThickness - box.styleRef().borderBottomWidth(),
        horizontalThickness, verticalThickness);
}

IntRect PaintLayerScrollableArea::scrollCornerRect() const
{
    // We have a scrollbar corner when a scrollbar is visible and not filling the entire length of the box.
    // This happens when:
    // (a) A resizer is present and at least one scrollbar is present
    // (b) Both scrollbars are present.
    bool hasHorizontalBar = horizontalScrollbar();
    bool hasVerticalBar = verticalScrollbar();
    bool hasResizer = box().style()->resize() != RESIZE_NONE;
    if ((hasHorizontalBar && hasVerticalBar) || (hasResizer && (hasHorizontalBar || hasVerticalBar)))
        return cornerRect(box(), horizontalScrollbar(), verticalScrollbar(), box().pixelSnappedBorderBoxRect());
    return IntRect();
}

IntRect PaintLayerScrollableArea::convertFromScrollbarToContainingWidget(const Scrollbar& scrollbar, const IntRect& scrollbarRect) const
{
    LayoutView* view = box().view();
    if (!view)
        return scrollbarRect;

    IntRect rect = scrollbarRect;
    rect.move(scrollbarOffset(scrollbar));

    return view->frameView()->convertFromLayoutObject(box(), rect);
}

IntRect PaintLayerScrollableArea::convertFromContainingWidgetToScrollbar(const Scrollbar& scrollbar, const IntRect& parentRect) const
{
    LayoutView* view = box().view();
    if (!view)
        return parentRect;

    IntRect rect = view->frameView()->convertToLayoutObject(box(), parentRect);
    rect.move(-scrollbarOffset(scrollbar));
    return rect;
}

IntPoint PaintLayerScrollableArea::convertFromScrollbarToContainingWidget(const Scrollbar& scrollbar, const IntPoint& scrollbarPoint) const
{
    LayoutView* view = box().view();
    if (!view)
        return scrollbarPoint;

    IntPoint point = scrollbarPoint;
    point.move(scrollbarOffset(scrollbar));
    return view->frameView()->convertFromLayoutObject(box(), point);
}

IntPoint PaintLayerScrollableArea::convertFromContainingWidgetToScrollbar(const Scrollbar& scrollbar, const IntPoint& parentPoint) const
{
    LayoutView* view = box().view();
    if (!view)
        return parentPoint;

    IntPoint point = view->frameView()->convertToLayoutObject(box(), parentPoint);

    point.move(-scrollbarOffset(scrollbar));
    return point;
}

int PaintLayerScrollableArea::scrollSize(ScrollbarOrientation orientation) const
{
    IntSize scrollDimensions = maximumScrollPosition() - minimumScrollPosition();
    return (orientation == HorizontalScrollbar) ? scrollDimensions.width() : scrollDimensions.height();
}

void PaintLayerScrollableArea::setScrollOffset(const IntPoint& newScrollOffset, ScrollType scrollType)
{
    setScrollOffset(DoublePoint(newScrollOffset), scrollType);
}

void PaintLayerScrollableArea::setScrollOffset(const DoublePoint& newScrollOffset, ScrollType scrollType)
{
    if (scrollOffset() == toDoubleSize(newScrollOffset))
        return;

    DoubleSize scrollDelta = scrollOffset() - toDoubleSize(newScrollOffset);
    m_scrollOffset = toDoubleSize(newScrollOffset);

    LocalFrame* frame = box().frame();
    ASSERT(frame);

    RefPtrWillBeRawPtr<FrameView> frameView = box().frameView();

    TRACE_EVENT1("devtools.timeline", "ScrollLayer", "data", InspectorScrollLayerEvent::data(&box()));

    // FIXME(420741): Resolve circular dependency between scroll offset and
    // compositing state, and remove this disabler.
    DisableCompositingQueryAsserts disabler;

    // Update the positions of our child layers (if needed as only fixed layers should be impacted by a scroll).
    // We don't update compositing layers, because we need to do a deep update from the compositing ancestor.
    if (!frameView->isInPerformLayout()) {
        // If we're in the middle of layout, we'll just update layers once layout has finished.
        layer()->updateLayerPositionsAfterOverflowScroll(scrollDelta);
        // Update regions, scrolling may change the clip of a particular region.
        frameView->updateDocumentAnnotatedRegions();
        frameView->setNeedsUpdateWidgetGeometries();
        updateCompositingLayersAfterScroll();
    }

    const LayoutBoxModelObject& paintInvalidationContainer = box().containerForPaintInvalidation();
    // The caret rect needs to be invalidated after scrolling
    frame->selection().setCaretRectNeedsUpdate();

    FloatQuad quadForFakeMouseMoveEvent = FloatQuad(FloatRect(layer()->layoutObject()->previousPaintInvalidationRectIncludingCompositedScrolling(paintInvalidationContainer)));

    quadForFakeMouseMoveEvent = paintInvalidationContainer.localToAbsoluteQuad(quadForFakeMouseMoveEvent);
    frame->eventHandler().dispatchFakeMouseMoveEventSoonInQuad(quadForFakeMouseMoveEvent);

    bool requiresPaintInvalidation = true;

    if (box().view()->compositor()->inCompositingMode()) {
        bool onlyScrolledCompositedLayers = scrollsOverflow()
            && !layer()->hasVisibleNonLayerContent()
            && !layer()->hasNonCompositedChild()
            && box().style()->backgroundLayers().attachment() != LocalBackgroundAttachment;

        if (usesCompositedScrolling() || onlyScrolledCompositedLayers)
            requiresPaintInvalidation = false;
    }

    // Only the root layer can overlap non-composited fixed-position elements.
    if (!requiresPaintInvalidation && layer()->isRootLayer() && frameView->hasViewportConstrainedObjects()) {
        if (!frameView->invalidateViewportConstrainedObjects())
            requiresPaintInvalidation = true;
    }

    // Just schedule a full paint invalidation of our object.
    // FIXME: This invalidation will be unnecessary in slimming paint phase 2.
    if (requiresPaintInvalidation) {
        box().setShouldDoFullPaintInvalidationIncludingNonCompositingDescendants();
        frameView->setFrameTimingRequestsDirty(true);
    }

    // Schedule the scroll DOM event.
    if (box().node())
        box().node()->document().enqueueScrollEventForNode(box().node());

    if (AXObjectCache* cache = box().document().existingAXObjectCache())
        cache->handleScrollPositionChanged(&box());
    box().view()->clearHitTestCache();

    // Inform the FrameLoader of the new scroll position, so it can be restored when navigating back.
    if (layer()->isRootLayer()) {
        frameView->frame().loader().saveScrollState();
        frame->loader().client()->didChangeScrollOffset();
    }

    // All scrolls clear the fragment anchor.
    frameView->clearFragmentAnchor();

    // Clear the scroll anchor, unless it is the reason for this scroll.
    if (RuntimeEnabledFeatures::scrollAnchoringEnabled() && scrollType != AnchoringScroll)
        scrollAnchor().clear();
}

IntPoint PaintLayerScrollableArea::scrollPosition() const
{
    return IntPoint(flooredIntSize(m_scrollOffset));
}

DoublePoint PaintLayerScrollableArea::scrollPositionDouble() const
{
    return DoublePoint(m_scrollOffset);
}

IntPoint PaintLayerScrollableArea::minimumScrollPosition() const
{
    return -scrollOrigin();
}

IntPoint PaintLayerScrollableArea::maximumScrollPosition() const
{
    IntSize contentSize;
    IntSize visibleSize;
    if (box().hasOverflowClip()) {
        contentSize = IntSize(pixelSnappedScrollWidth(), pixelSnappedScrollHeight());
        visibleSize = pixelSnappedIntRect(box().overflowClipRect(box().location())).size();

        // TODO(skobes): We should really ASSERT that contentSize >= visibleSize
        // when we are not the root layer, but we can't because contentSize is
        // based on stale layout overflow data (http://crbug.com/576933).
        contentSize = contentSize.expandedTo(visibleSize);
    }
    return -scrollOrigin() + (contentSize - visibleSize);
}

IntRect PaintLayerScrollableArea::visibleContentRect(IncludeScrollbarsInRect scrollbarInclusion) const
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

int PaintLayerScrollableArea::visibleHeight() const
{
    return layer()->size().height();
}

int PaintLayerScrollableArea::visibleWidth() const
{
    return layer()->size().width();
}

IntSize PaintLayerScrollableArea::contentsSize() const
{
    return IntSize(scrollWidth(), scrollHeight());
}

IntPoint PaintLayerScrollableArea::lastKnownMousePosition() const
{
    return box().frame() ? box().frame()->eventHandler().lastKnownMousePosition() : IntPoint();
}

bool PaintLayerScrollableArea::scrollAnimatorEnabled() const
{
    if (Settings* settings = box().frame()->settings())
        return settings->scrollAnimatorEnabled();
    return false;
}

bool PaintLayerScrollableArea::shouldSuspendScrollAnimations() const
{
    LayoutView* view = box().view();
    if (!view)
        return true;
    return view->frameView()->shouldSuspendScrollAnimations();
}

void PaintLayerScrollableArea::scrollbarVisibilityChanged()
{
    if (LayoutView* view = box().view())
        return view->clearHitTestCache();
}

bool PaintLayerScrollableArea::scrollbarsCanBeActive() const
{
    LayoutView* view = box().view();
    if (!view)
        return false;
    return view->frameView()->scrollbarsCanBeActive();
}

IntRect PaintLayerScrollableArea::scrollableAreaBoundingBox() const
{
    return box().absoluteBoundingBoxRect();
}

void PaintLayerScrollableArea::registerForAnimation()
{
    if (LocalFrame* frame = box().frame()) {
        if (FrameView* frameView = frame->view())
            frameView->addAnimatingScrollableArea(this);
    }
}

void PaintLayerScrollableArea::deregisterForAnimation()
{
    if (LocalFrame* frame = box().frame()) {
        if (FrameView* frameView = frame->view())
            frameView->removeAnimatingScrollableArea(this);
    }
}

bool PaintLayerScrollableArea::userInputScrollable(ScrollbarOrientation orientation) const
{
    if (box().isIntrinsicallyScrollable(orientation))
        return true;

    EOverflow overflowStyle = (orientation == HorizontalScrollbar) ?
        box().style()->overflowX() : box().style()->overflowY();
    return (overflowStyle == OSCROLL || overflowStyle == OAUTO || overflowStyle == OOVERLAY);
}

bool PaintLayerScrollableArea::shouldPlaceVerticalScrollbarOnLeft() const
{
    return box().shouldPlaceBlockDirectionScrollbarOnLogicalLeft();
}

int PaintLayerScrollableArea::pageStep(ScrollbarOrientation orientation) const
{
    int length = (orientation == HorizontalScrollbar) ?
        box().pixelSnappedClientWidth() : box().pixelSnappedClientHeight();
    int minPageStep = static_cast<float>(length) * ScrollableArea::minFractionToStepWhenPaging();
    int pageStep = max(minPageStep, length - ScrollableArea::maxOverlapBetweenPages());

    return max(pageStep, 1);
}

LayoutBox& PaintLayerScrollableArea::box() const
{
    return *m_layer.layoutBox();
}

PaintLayer* PaintLayerScrollableArea::layer() const
{
    return &m_layer;
}

LayoutUnit PaintLayerScrollableArea::scrollWidth() const
{
    return m_overflowRect.width();
}

LayoutUnit PaintLayerScrollableArea::scrollHeight() const
{
    return m_overflowRect.height();
}

int PaintLayerScrollableArea::pixelSnappedScrollWidth() const
{
    return snapSizeToPixel(scrollWidth(), box().clientLeft() + box().location().x());
}

int PaintLayerScrollableArea::pixelSnappedScrollHeight() const
{
    return snapSizeToPixel(scrollHeight(), box().clientTop() + box().location().y());
}

void PaintLayerScrollableArea::computeScrollDimensions()
{
    m_overflowRect = box().layoutOverflowRect();
    box().flipForWritingMode(m_overflowRect);

    int scrollableLeftOverflow = m_overflowRect.x() - box().borderLeft() - (box().shouldPlaceBlockDirectionScrollbarOnLogicalLeft() ? box().verticalScrollbarWidth() : 0);
    int scrollableTopOverflow = m_overflowRect.y() - box().borderTop();
    setScrollOrigin(IntPoint(-scrollableLeftOverflow, -scrollableTopOverflow));
}

void PaintLayerScrollableArea::scrollToPosition(const DoublePoint& scrollPosition, ScrollOffsetClamping clamp, ScrollBehavior scrollBehavior, ScrollType scrollType)
{
    cancelProgrammaticScrollAnimation();

    DoublePoint newScrollPosition = clamp == ScrollOffsetClamped ? clampScrollPosition(scrollPosition) : scrollPosition;
    if (newScrollPosition != scrollPositionDouble())
        ScrollableArea::setScrollPosition(newScrollPosition, scrollType, scrollBehavior);
}

void PaintLayerScrollableArea::updateScrollDimensions(DoubleSize& scrollOffset, bool& autoHorizontalScrollBarChanged, bool& autoVerticalScrollBarChanged)
{
    ASSERT(box().hasOverflowClip());

    if (needsScrollbarReconstruction()) {
        m_scrollbarManager.setCanDetachScrollbars(false);
        setHasHorizontalScrollbar(false);
        setHasVerticalScrollbar(false);
    }

    m_scrollbarManager.setCanDetachScrollbars(true);

    scrollOffset = adjustedScrollOffset();
    computeScrollDimensions();
    bool hasHorizontalOverflow = this->hasHorizontalOverflow();
    bool hasVerticalOverflow = this->hasVerticalOverflow();
    if (hasOverlayScrollbars()) {
        if (!scrollSize(HorizontalScrollbar))
            setHasHorizontalScrollbar(false);
        if (!scrollSize(VerticalScrollbar))
            setHasVerticalScrollbar(false);
    }

    // overflow:auto may need to lay out again if scrollbars got added/removed.
    autoHorizontalScrollBarChanged |= (box().hasAutoHorizontalScrollbar() && (hasHorizontalScrollbar() != hasHorizontalOverflow)) || (box().style()->overflowX() == OSCROLL && !horizontalScrollbar());
    autoVerticalScrollBarChanged |= (box().hasAutoVerticalScrollbar() && (hasVerticalScrollbar() != hasVerticalOverflow)) || (box().style()->overflowY() == OSCROLL && !verticalScrollbar());
    if (!visualViewportSuppliesScrollbars() && (autoHorizontalScrollBarChanged || autoVerticalScrollBarChanged)) {
        if (box().hasAutoHorizontalScrollbar())
            setHasHorizontalScrollbar(hasHorizontalOverflow);
        else if (box().style()->overflowX() == OSCROLL)
            setHasHorizontalScrollbar(true);
        if (box().hasAutoVerticalScrollbar())
            setHasVerticalScrollbar(hasVerticalOverflow);
        else if (box().style()->overflowY() == OSCROLL)
            setHasVerticalScrollbar(true);
        // If vertical scrollbar existence has changed, LTR blocks need to update scroll origin to account
        // for left-hand vertical scrollbar width.
        computeScrollDimensions();
    }
}

void PaintLayerScrollableArea::finalizeScrollDimensions(const DoubleSize& originalScrollOffset, bool autoHorizontalScrollBarChanged, bool autoVerticalScrollBarChanged)
{
    ASSERT(box().hasOverflowClip());

    // Layout may cause us to be at an invalid scroll position. In this case we need
    // to pull our scroll offsets back to the max (or push them up to the min).
    DoublePoint clampedScrollPosition = clampScrollPosition(scrollPositionDouble());
    if (clampedScrollPosition != scrollPositionDouble())
        scrollToPosition(clampedScrollPosition);

    if (originalScrollOffset != adjustedScrollOffset()) {
        DoublePoint origin(scrollOrigin());
        scrollPositionChanged(-origin + adjustedScrollOffset(), ProgrammaticScroll);
    }

    m_scrollbarManager.setCanDetachScrollbars(false);

    bool hasHorizontalOverflow = this->hasHorizontalOverflow();
    bool hasVerticalOverflow = this->hasVerticalOverflow();

    {
        // Hits in compositing/overflow/automatically-opt-into-composited-scrolling-after-style-change.html.
        DisableCompositingQueryAsserts disabler;

        // overflow:scroll should just enable/disable.
        if (box().style()->overflowX() == OSCROLL && horizontalScrollbar())
            horizontalScrollbar()->setEnabled(hasHorizontalOverflow);
        if (box().style()->overflowY() == OSCROLL && verticalScrollbar())
            verticalScrollbar()->setEnabled(hasVerticalOverflow);
    }

    if (!visualViewportSuppliesScrollbars() && (autoHorizontalScrollBarChanged || autoVerticalScrollBarChanged)) {
        if (hasVerticalOverflow || hasHorizontalOverflow)
            updateScrollCornerStyle();

        layer()->updateSelfPaintingLayer();

        // Force an update since we know the scrollbars have changed things.
        if (box().document().hasAnnotatedRegions())
            box().document().setAnnotatedRegionsDirty(true);

        // Our proprietary overflow: overlay value doesn't trigger a layout.
        if ((autoHorizontalScrollBarChanged && box().style()->overflowX() != OOVERLAY) || (autoVerticalScrollBarChanged && box().style()->overflowY() != OOVERLAY)) {
            if (!m_inOverflowRelayout) {
                m_inOverflowRelayout = true;
                SubtreeLayoutScope layoutScope(box());
                layoutScope.setNeedsLayout(&box(), LayoutInvalidationReason::ScrollbarChanged);
                if (box().isLayoutBlock()) {
                    LayoutBlock& block = toLayoutBlock(box());
                    block.scrollbarsChanged(autoHorizontalScrollBarChanged, autoVerticalScrollBarChanged);
                    block.layoutBlock(true);
                } else {
                    box().layout();
                }
                m_inOverflowRelayout = false;
            }
        }
    }

    {
        // Hits in compositing/overflow/automatically-opt-into-composited-scrolling-after-style-change.html.
        DisableCompositingQueryAsserts disabler;

        // Set up the range (and page step/line step).
        if (Scrollbar* horizontalScrollbar = this->horizontalScrollbar()) {
            int clientWidth = box().pixelSnappedClientWidth();
            horizontalScrollbar->setProportion(clientWidth, overflowRect().width());
        }
        if (Scrollbar* verticalScrollbar = this->verticalScrollbar()) {
            int clientHeight = box().pixelSnappedClientHeight();
            verticalScrollbar->setProportion(clientHeight, overflowRect().height());
        }
    }

    if (hasOverlayScrollbars()) {
        if (!scrollSize(HorizontalScrollbar))
            setHasHorizontalScrollbar(false);
        if (!scrollSize(VerticalScrollbar))
            setHasVerticalScrollbar(false);
    }

    bool hasOverflow = hasScrollableHorizontalOverflow() || hasScrollableVerticalOverflow();
    updateScrollableAreaSet(hasOverflow);

    DisableCompositingQueryAsserts disabler;
    positionOverflowControls();
}

void PaintLayerScrollableArea::updateAfterLayout()
{
    DoubleSize originalScrollOffset;
    bool autoHorizontalScrollBarChanged = false;
    bool autoVerticalScrollBarChanged = false;
    updateScrollDimensions(originalScrollOffset, autoHorizontalScrollBarChanged, autoVerticalScrollBarChanged);
    finalizeScrollDimensions(originalScrollOffset, autoHorizontalScrollBarChanged, autoVerticalScrollBarChanged);
}

ScrollBehavior PaintLayerScrollableArea::scrollBehaviorStyle() const
{
    return box().style()->scrollBehavior();
}

bool PaintLayerScrollableArea::hasHorizontalOverflow() const
{
    return pixelSnappedScrollWidth() > box().pixelSnappedClientWidth();
}

bool PaintLayerScrollableArea::hasVerticalOverflow() const
{
    return pixelSnappedScrollHeight() > box().pixelSnappedClientHeight();
}

bool PaintLayerScrollableArea::hasScrollableHorizontalOverflow() const
{
    return hasHorizontalOverflow() && box().scrollsOverflowX();
}

bool PaintLayerScrollableArea::hasScrollableVerticalOverflow() const
{
    return hasVerticalOverflow() && box().scrollsOverflowY();
}

static bool overflowRequiresScrollbar(EOverflow overflow)
{
    return overflow == OSCROLL;
}

static bool overflowDefinesAutomaticScrollbar(EOverflow overflow)
{
    return overflow == OAUTO || overflow == OOVERLAY;
}

// This function returns true if the given box requires overflow scrollbars (as
// opposed to the 'viewport' scrollbars managed by the PaintLayerCompositor).
// FIXME: we should use the same scrolling machinery for both the viewport and
// overflow. Currently, we need to avoid producing scrollbars here if they'll be
// handled externally in the RLC.
static bool canHaveOverflowScrollbars(const LayoutBox& box)
{
    bool rootLayerScrolls = box.document().settings() && box.document().settings()->rootLayerScrolls();
    return (rootLayerScrolls || !box.isLayoutView()) && box.document().viewportDefiningElement() != box.node();
}

void PaintLayerScrollableArea::updateAfterStyleChange(const ComputedStyle* oldStyle)
{
    // Don't do this on first style recalc, before layout has ever happened.
    if (!overflowRect().size().isZero())
        updateScrollableAreaSet(hasScrollableHorizontalOverflow() || hasScrollableVerticalOverflow());

    if (!canHaveOverflowScrollbars(box()))
        return;

    // Avoid drawing two sets of scrollbars when one is provided by the visual viewport.
    if (visualViewportSuppliesScrollbars()) {
        setHasHorizontalScrollbar(false);
        setHasVerticalScrollbar(false);
        return;
    }

    EOverflow overflowX = box().style()->overflowX();
    EOverflow overflowY = box().style()->overflowY();

    // To avoid doing a relayout in updateScrollbarsAfterLayout, we try to keep any automatic scrollbar that was already present.
    bool needsHorizontalScrollbar = (hasHorizontalScrollbar() && overflowDefinesAutomaticScrollbar(overflowX)) || overflowRequiresScrollbar(overflowX);
    bool needsVerticalScrollbar = (hasVerticalScrollbar() && overflowDefinesAutomaticScrollbar(overflowY)) || overflowRequiresScrollbar(overflowY);

    // Look for the scrollbarModes and reset the needs Horizontal & vertical Scrollbar values based on scrollbarModes, as during force style change
    // StyleResolver::styleForDocument returns documentStyle with  no overflow values, due to which we are destorying the scrollbars that was
    // already present.
    if (box().isLayoutView()) {
        if (LocalFrame* frame = box().frame()) {
            if (FrameView* frameView = frame->view()) {
                ScrollbarMode hMode;
                ScrollbarMode vMode;
                frameView->calculateScrollbarModes(hMode, vMode);
                if (hMode == ScrollbarAlwaysOn && !needsHorizontalScrollbar)
                    needsHorizontalScrollbar = true;
                if (vMode == ScrollbarAlwaysOn && !needsVerticalScrollbar)
                    needsVerticalScrollbar = true;
            }
        }
    }

    setHasHorizontalScrollbar(needsHorizontalScrollbar);
    setHasVerticalScrollbar(needsVerticalScrollbar);

    // With overflow: scroll, scrollbars are always visible but may be disabled.
    // When switching to another value, we need to re-enable them (see bug 11985).
    if (needsHorizontalScrollbar && oldStyle && oldStyle->overflowX() == OSCROLL && overflowX != OSCROLL) {
        ASSERT(hasHorizontalScrollbar());
        horizontalScrollbar()->setEnabled(true);
    }

    if (needsVerticalScrollbar && oldStyle && oldStyle->overflowY() == OSCROLL && overflowY != OSCROLL) {
        ASSERT(hasVerticalScrollbar());
        verticalScrollbar()->setEnabled(true);
    }

    // FIXME: Need to detect a swap from custom to native scrollbars (and vice versa).
    if (horizontalScrollbar())
        horizontalScrollbar()->styleChanged();
    if (verticalScrollbar())
        verticalScrollbar()->styleChanged();

    updateScrollCornerStyle();
    updateResizerAreaSet();
    updateResizerStyle();
}

bool PaintLayerScrollableArea::updateAfterCompositingChange()
{
    layer()->updateScrollingStateAfterCompositingChange();
    const bool layersChanged = m_topmostScrollChild != m_nextTopmostScrollChild;
    m_topmostScrollChild = m_nextTopmostScrollChild;
    m_nextTopmostScrollChild = nullptr;
    return layersChanged;
}

void PaintLayerScrollableArea::updateAfterOverflowRecalc()
{
    computeScrollDimensions();
    if (Scrollbar* horizontalScrollbar = this->horizontalScrollbar()) {
        int clientWidth = box().pixelSnappedClientWidth();
        horizontalScrollbar->setProportion(clientWidth, overflowRect().width());
    }
    if (Scrollbar* verticalScrollbar = this->verticalScrollbar()) {
        int clientHeight = box().pixelSnappedClientHeight();
        verticalScrollbar->setProportion(clientHeight, overflowRect().height());
    }

    bool hasHorizontalOverflow = this->hasHorizontalOverflow();
    bool hasVerticalOverflow = this->hasVerticalOverflow();
    bool autoHorizontalScrollBarChanged = box().hasAutoHorizontalScrollbar() && (hasHorizontalScrollbar() != hasHorizontalOverflow);
    bool autoVerticalScrollBarChanged = box().hasAutoVerticalScrollbar() && (hasVerticalScrollbar() != hasVerticalOverflow);
    if (autoHorizontalScrollBarChanged || autoVerticalScrollBarChanged)
        box().setNeedsLayoutAndFullPaintInvalidation(LayoutInvalidationReason::Unknown);
}

IntRect PaintLayerScrollableArea::rectForHorizontalScrollbar(const IntRect& borderBoxRect) const
{
    if (!hasHorizontalScrollbar())
        return IntRect();

    const IntRect& scrollCorner = scrollCornerRect();

    return IntRect(horizontalScrollbarStart(borderBoxRect.x()),
        borderBoxRect.maxY() - box().borderBottom() - horizontalScrollbar()->height(),
        borderBoxRect.width() - (box().borderLeft() + box().borderRight()) - scrollCorner.width(),
        horizontalScrollbar()->height());
}

IntRect PaintLayerScrollableArea::rectForVerticalScrollbar(const IntRect& borderBoxRect) const
{
    if (!hasVerticalScrollbar())
        return IntRect();

    const IntRect& scrollCorner = scrollCornerRect();

    return IntRect(verticalScrollbarStart(borderBoxRect.x(), borderBoxRect.maxX()),
        borderBoxRect.y() + box().borderTop(),
        verticalScrollbar()->width(),
        borderBoxRect.height() - (box().borderTop() + box().borderBottom()) - scrollCorner.height());
}

int PaintLayerScrollableArea::verticalScrollbarStart(int minX, int maxX) const
{
    if (box().shouldPlaceBlockDirectionScrollbarOnLogicalLeft())
        return minX + box().borderLeft();
    return maxX - box().borderRight() - verticalScrollbar()->width();
}

int PaintLayerScrollableArea::horizontalScrollbarStart(int minX) const
{
    int x = minX + box().borderLeft();
    if (box().shouldPlaceBlockDirectionScrollbarOnLogicalLeft())
        x += hasVerticalScrollbar() ? verticalScrollbar()->width() : resizerCornerRect(box().pixelSnappedBorderBoxRect(), ResizerForPointer).width();
    return x;
}

IntSize PaintLayerScrollableArea::scrollbarOffset(const Scrollbar& scrollbar) const
{
    if (&scrollbar == verticalScrollbar())
        return IntSize(verticalScrollbarStart(0, box().size().width()), box().borderTop());

    if (&scrollbar == horizontalScrollbar())
        return IntSize(horizontalScrollbarStart(0), box().size().height() - box().borderBottom() - scrollbar.height());

    ASSERT_NOT_REACHED();
    return IntSize();
}

static inline const LayoutObject& layoutObjectForScrollbar(const LayoutObject& layoutObject)
{
    if (Node* node = layoutObject.node()) {
        if (layoutObject.isLayoutView()) {
            Document& doc = node->document();
            if (Settings* settings = doc.settings()) {
                if (!settings->allowCustomScrollbarInMainFrame() && layoutObject.frame() && layoutObject.frame()->isMainFrame())
                    return layoutObject;
            }

            // Try the <body> element first as a scrollbar source.
            Element* body = doc.body();
            if (body && body->layoutObject() && body->layoutObject()->style()->hasPseudoStyle(SCROLLBAR))
                return *body->layoutObject();

            // If the <body> didn't have a custom style, then the root element might.
            Element* docElement = doc.documentElement();
            if (docElement && docElement->layoutObject() && docElement->layoutObject()->style()->hasPseudoStyle(SCROLLBAR))
                return *docElement->layoutObject();

            // If we have an owning ipage/LocalFrame element, then it can set the custom scrollbar also.
            LayoutPart* frameLayoutObject = node->document().frame()->ownerLayoutObject();
            if (frameLayoutObject && frameLayoutObject->style()->hasPseudoStyle(SCROLLBAR))
                return *frameLayoutObject;
        }

        if (layoutObject.styleRef().hasPseudoStyle(SCROLLBAR))
            return layoutObject;

        if (ShadowRoot* shadowRoot = node->containingShadowRoot()) {
            if (shadowRoot->type() == ShadowRootType::UserAgent)
                return *shadowRoot->host()->layoutObject();
        }
    }

    return layoutObject;
}

bool PaintLayerScrollableArea::needsScrollbarReconstruction() const
{
    const LayoutObject& actualLayoutObject = layoutObjectForScrollbar(box());
    bool shouldUseCustom = actualLayoutObject.isBox() && actualLayoutObject.styleRef().hasPseudoStyle(SCROLLBAR);
    bool hasAnyScrollbar = hasScrollbar();
    bool hasCustom = (hasHorizontalScrollbar() && horizontalScrollbar()->isCustomScrollbar()) || (hasVerticalScrollbar() && verticalScrollbar()->isCustomScrollbar());
    bool didCustomScrollbarOwnerChanged = false;

    if (hasHorizontalScrollbar() && horizontalScrollbar()->isCustomScrollbar()) {
        if (actualLayoutObject != toLayoutScrollbar(horizontalScrollbar())->owningLayoutObject())
            didCustomScrollbarOwnerChanged = true;
    }

    if (hasVerticalScrollbar() && verticalScrollbar()->isCustomScrollbar()) {
        if (actualLayoutObject != toLayoutScrollbar(verticalScrollbar())->owningLayoutObject())
            didCustomScrollbarOwnerChanged = true;
    }

    return hasAnyScrollbar && ((shouldUseCustom != hasCustom) || (shouldUseCustom && didCustomScrollbarOwnerChanged));
}

void PaintLayerScrollableArea::setHasHorizontalScrollbar(bool hasScrollbar)
{
    if (hasScrollbar == hasHorizontalScrollbar())
        return;

    setScrollbarNeedsPaintInvalidation(HorizontalScrollbar);

    m_scrollbarManager.setHasHorizontalScrollbar(hasScrollbar);

    // Destroying or creating one bar can cause our scrollbar corner to come and go. We need to update the opposite scrollbar's style.
    if (hasHorizontalScrollbar())
        horizontalScrollbar()->styleChanged();
    if (hasVerticalScrollbar())
        verticalScrollbar()->styleChanged();

    setScrollCornerNeedsPaintInvalidation();

    // Force an update since we know the scrollbars have changed things.
    if (box().document().hasAnnotatedRegions())
        box().document().setAnnotatedRegionsDirty(true);
}

void PaintLayerScrollableArea::setHasVerticalScrollbar(bool hasScrollbar)
{
    if (hasScrollbar == hasVerticalScrollbar())
        return;

    setScrollbarNeedsPaintInvalidation(VerticalScrollbar);

    m_scrollbarManager.setHasVerticalScrollbar(hasScrollbar);

    // Destroying or creating one bar can cause our scrollbar corner to come and go. We need to update the opposite scrollbar's style.
    if (hasHorizontalScrollbar())
        horizontalScrollbar()->styleChanged();
    if (hasVerticalScrollbar())
        verticalScrollbar()->styleChanged();

    setScrollCornerNeedsPaintInvalidation();

    // Force an update since we know the scrollbars have changed things.
    if (box().document().hasAnnotatedRegions())
        box().document().setAnnotatedRegionsDirty(true);
}

int PaintLayerScrollableArea::verticalScrollbarWidth(OverlayScrollbarSizeRelevancy relevancy) const
{
    if (!hasVerticalScrollbar() || (verticalScrollbar()->isOverlayScrollbar() && (relevancy == IgnoreOverlayScrollbarSize || !verticalScrollbar()->shouldParticipateInHitTesting())))
        return 0;
    return verticalScrollbar()->width();
}

int PaintLayerScrollableArea::horizontalScrollbarHeight(OverlayScrollbarSizeRelevancy relevancy) const
{
    if (!hasHorizontalScrollbar() || (horizontalScrollbar()->isOverlayScrollbar() && (relevancy == IgnoreOverlayScrollbarSize || !horizontalScrollbar()->shouldParticipateInHitTesting())))
        return 0;
    return horizontalScrollbar()->height();
}

void PaintLayerScrollableArea::positionOverflowControls()
{
    if (!hasScrollbar() && !box().canResize())
        return;

    const IntRect borderBox = box().pixelSnappedBorderBoxRect();
    if (Scrollbar* verticalScrollbar = this->verticalScrollbar())
        verticalScrollbar->setFrameRect(rectForVerticalScrollbar(borderBox));

    if (Scrollbar* horizontalScrollbar = this->horizontalScrollbar())
        horizontalScrollbar->setFrameRect(rectForHorizontalScrollbar(borderBox));

    const IntRect& scrollCorner = scrollCornerRect();
    if (m_scrollCorner)
        m_scrollCorner->setFrameRect(LayoutRect(scrollCorner));

    if (m_resizer)
        m_resizer->setFrameRect(LayoutRect(resizerCornerRect(borderBox, ResizerForPointer)));

    // FIXME, this should eventually be removed, once we are certain that composited
    // controls get correctly positioned on a compositor update. For now, conservatively
    // leaving this unchanged.
    if (layer()->hasCompositedLayerMapping())
        layer()->compositedLayerMapping()->positionOverflowControlsLayers();
}

void PaintLayerScrollableArea::updateScrollCornerStyle()
{
    if (!m_scrollCorner && !hasScrollbar())
        return;
    if (!m_scrollCorner && hasOverlayScrollbars())
        return;

    const LayoutObject& actualLayoutObject = layoutObjectForScrollbar(box());
    RefPtr<ComputedStyle> corner = box().hasOverflowClip() ? actualLayoutObject.getUncachedPseudoStyle(PseudoStyleRequest(SCROLLBAR_CORNER), actualLayoutObject.style()) : PassRefPtr<ComputedStyle>(nullptr);
    if (corner) {
        if (!m_scrollCorner) {
            m_scrollCorner = LayoutScrollbarPart::createAnonymous(&box().document());
            m_scrollCorner->setDangerousOneWayParent(&box());
        }
        m_scrollCorner->setStyle(corner.release());
    } else if (m_scrollCorner) {
        m_scrollCorner->destroy();
        m_scrollCorner = nullptr;
    }
}

bool PaintLayerScrollableArea::hitTestOverflowControls(HitTestResult& result, const IntPoint& localPoint)
{
    if (!hasScrollbar() && !box().canResize())
        return false;

    IntRect resizeControlRect;
    if (box().style()->resize() != RESIZE_NONE) {
        resizeControlRect = resizerCornerRect(box().pixelSnappedBorderBoxRect(), ResizerForPointer);
        if (resizeControlRect.contains(localPoint))
            return true;
    }

    int resizeControlSize = max(resizeControlRect.height(), 0);
    if (hasVerticalScrollbar() && verticalScrollbar()->shouldParticipateInHitTesting()) {
        LayoutRect vBarRect(verticalScrollbarStart(0, box().size().width()),
            LayoutUnit(box().borderTop()),
            verticalScrollbar()->width(),
            box().size().height() - (box().borderTop() + box().borderBottom()) - (hasHorizontalScrollbar() ? horizontalScrollbar()->height() : resizeControlSize));
        if (vBarRect.contains(localPoint)) {
            result.setScrollbar(verticalScrollbar());
            return true;
        }
    }

    resizeControlSize = max(resizeControlRect.width(), 0);
    if (hasHorizontalScrollbar() && horizontalScrollbar()->shouldParticipateInHitTesting()) {
        LayoutRect hBarRect(horizontalScrollbarStart(LayoutUnit()),
            box().size().height() - box().borderBottom() - horizontalScrollbar()->height(),
            box().size().width() - (box().borderLeft() + box().borderRight()) - (hasVerticalScrollbar() ? verticalScrollbar()->width() : resizeControlSize),
            horizontalScrollbar()->height());
        if (hBarRect.contains(localPoint)) {
            result.setScrollbar(horizontalScrollbar());
            return true;
        }
    }

    // FIXME: We should hit test the m_scrollCorner and pass it back through the result.

    return false;
}

IntRect PaintLayerScrollableArea::resizerCornerRect(const IntRect& bounds, ResizerHitTestType resizerHitTestType) const
{
    if (box().style()->resize() == RESIZE_NONE)
        return IntRect();
    IntRect corner = cornerRect(box(), horizontalScrollbar(), verticalScrollbar(), bounds);

    if (resizerHitTestType == ResizerForTouch) {
        // We make the resizer virtually larger for touch hit testing. With the
        // expanding ratio k = ResizerControlExpandRatioForTouch, we first move
        // the resizer rect (of width w & height h), by (-w * (k-1), -h * (k-1)),
        // then expand the rect by new_w/h = w/h * k.
        int expandRatio = ResizerControlExpandRatioForTouch - 1;
        corner.move(-corner.width() * expandRatio, -corner.height() * expandRatio);
        corner.expand(corner.width() * expandRatio, corner.height() * expandRatio);
    }

    return corner;
}

IntRect PaintLayerScrollableArea::scrollCornerAndResizerRect() const
{
    IntRect scrollCornerAndResizer = scrollCornerRect();
    if (scrollCornerAndResizer.isEmpty())
        scrollCornerAndResizer = resizerCornerRect(box().pixelSnappedBorderBoxRect(), ResizerForPointer);
    return scrollCornerAndResizer;
}

bool PaintLayerScrollableArea::isPointInResizeControl(const IntPoint& absolutePoint, ResizerHitTestType resizerHitTestType) const
{
    if (!box().canResize())
        return false;

    IntPoint localPoint = roundedIntPoint(box().absoluteToLocal(absolutePoint, UseTransforms));
    IntRect localBounds(0, 0, box().pixelSnappedWidth(), box().pixelSnappedHeight());
    return resizerCornerRect(localBounds, resizerHitTestType).contains(localPoint);
}

bool PaintLayerScrollableArea::hitTestResizerInFragments(const PaintLayerFragments& layerFragments, const HitTestLocation& hitTestLocation) const
{
    if (!box().canResize())
        return false;

    if (layerFragments.isEmpty())
        return false;

    for (int i = layerFragments.size() - 1; i >= 0; --i) {
        const PaintLayerFragment& fragment = layerFragments.at(i);
        if (fragment.backgroundRect.intersects(hitTestLocation) && resizerCornerRect(pixelSnappedIntRect(fragment.layerBounds), ResizerForPointer).contains(hitTestLocation.roundedPoint()))
            return true;
    }

    return false;
}

void PaintLayerScrollableArea::updateResizerAreaSet()
{
    LocalFrame* frame = box().frame();
    if (!frame)
        return;
    FrameView* frameView = frame->view();
    if (!frameView)
        return;
    if (box().canResize())
        frameView->addResizerArea(box());
    else
        frameView->removeResizerArea(box());
}

void PaintLayerScrollableArea::updateResizerStyle()
{
    if (!m_resizer && !box().canResize())
        return;

    const LayoutObject& actualLayoutObject = layoutObjectForScrollbar(box());
    RefPtr<ComputedStyle> resizer = box().hasOverflowClip() ? actualLayoutObject.getUncachedPseudoStyle(PseudoStyleRequest(RESIZER), actualLayoutObject.style()) : PassRefPtr<ComputedStyle>(nullptr);
    if (resizer) {
        if (!m_resizer) {
            m_resizer = LayoutScrollbarPart::createAnonymous(&box().document());
            m_resizer->setDangerousOneWayParent(&box());
        }
        m_resizer->setStyle(resizer.release());
    } else if (m_resizer) {
        m_resizer->destroy();
        m_resizer = nullptr;
    }
}

IntSize PaintLayerScrollableArea::offsetFromResizeCorner(const IntPoint& absolutePoint) const
{
    // Currently the resize corner is either the bottom right corner or the bottom left corner.
    // FIXME: This assumes the location is 0, 0. Is this guaranteed to always be the case?
    IntSize elementSize = layer()->size();
    if (box().shouldPlaceBlockDirectionScrollbarOnLogicalLeft())
        elementSize.setWidth(0);
    IntPoint resizerPoint = IntPoint(elementSize);
    IntPoint localPoint = roundedIntPoint(box().absoluteToLocal(absolutePoint, UseTransforms));
    return localPoint - resizerPoint;
}

void PaintLayerScrollableArea::resize(const PlatformEvent& evt, const LayoutSize& oldOffset)
{
    // FIXME: This should be possible on generated content but is not right now.
    if (!inResizeMode() || !box().canResize() || !box().node())
        return;

    ASSERT(box().node()->isElementNode());
    Element* element = toElement(box().node());

    Document& document = element->document();

    IntPoint pos;
    const PlatformGestureEvent* gevt = 0;

    switch (evt.type()) {
    case PlatformEvent::MouseMoved:
        if (!document.frame()->eventHandler().mousePressed())
            return;
        pos = static_cast<const PlatformMouseEvent*>(&evt)->position();
        break;
    case PlatformEvent::GestureScrollUpdate:
        pos = static_cast<const PlatformGestureEvent*>(&evt)->position();
        gevt = static_cast<const PlatformGestureEvent*>(&evt);
        pos = gevt->position();
        pos.move(gevt->deltaX(), gevt->deltaY());
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    float zoomFactor = box().style()->effectiveZoom();

    IntSize newOffset = offsetFromResizeCorner(document.view()->rootFrameToContents(pos));
    newOffset.setWidth(newOffset.width() / zoomFactor);
    newOffset.setHeight(newOffset.height() / zoomFactor);

    LayoutSize currentSize = box().size();
    currentSize.scale(1 / zoomFactor);
    LayoutSize minimumSize = element->minimumSizeForResizing().shrunkTo(currentSize);
    element->setMinimumSizeForResizing(minimumSize);

    LayoutSize adjustedOldOffset = LayoutSize(oldOffset.width() / zoomFactor, oldOffset.height() / zoomFactor);
    if (box().shouldPlaceBlockDirectionScrollbarOnLogicalLeft()) {
        newOffset.setWidth(-newOffset.width());
        adjustedOldOffset.setWidth(-adjustedOldOffset.width());
    }

    LayoutSize difference((currentSize + newOffset - adjustedOldOffset).expandedTo(minimumSize) - currentSize);

    bool isBoxSizingBorder = box().style()->boxSizing() == BORDER_BOX;

    EResize resize = box().style()->resize();
    if (resize != RESIZE_VERTICAL && difference.width()) {
        if (element->isFormControlElement()) {
            // Make implicit margins from the theme explicit (see <http://bugs.webkit.org/show_bug.cgi?id=9547>).
            element->setInlineStyleProperty(CSSPropertyMarginLeft, box().marginLeft() / zoomFactor, CSSPrimitiveValue::UnitType::Pixels);
            element->setInlineStyleProperty(CSSPropertyMarginRight, box().marginRight() / zoomFactor, CSSPrimitiveValue::UnitType::Pixels);
        }
        LayoutUnit baseWidth = box().size().width() - (isBoxSizingBorder ? LayoutUnit() : box().borderAndPaddingWidth());
        baseWidth = baseWidth / zoomFactor;
        element->setInlineStyleProperty(CSSPropertyWidth, roundToInt(baseWidth + difference.width()), CSSPrimitiveValue::UnitType::Pixels);
    }

    if (resize != RESIZE_HORIZONTAL && difference.height()) {
        if (element->isFormControlElement()) {
            // Make implicit margins from the theme explicit (see <http://bugs.webkit.org/show_bug.cgi?id=9547>).
            element->setInlineStyleProperty(CSSPropertyMarginTop, box().marginTop() / zoomFactor, CSSPrimitiveValue::UnitType::Pixels);
            element->setInlineStyleProperty(CSSPropertyMarginBottom, box().marginBottom() / zoomFactor, CSSPrimitiveValue::UnitType::Pixels);
        }
        LayoutUnit baseHeight = box().size().height() - (isBoxSizingBorder ? LayoutUnit() : box().borderAndPaddingHeight());
        baseHeight = baseHeight / zoomFactor;
        element->setInlineStyleProperty(CSSPropertyHeight, roundToInt(baseHeight + difference.height()), CSSPrimitiveValue::UnitType::Pixels);
    }

    document.updateLayout();

    // FIXME (Radar 4118564): We should also autoscroll the window as necessary to keep the point under the cursor in view.
}

LayoutRect PaintLayerScrollableArea::scrollIntoView(const LayoutRect& rect, const ScrollAlignment& alignX, const ScrollAlignment& alignY, ScrollType scrollType)
{
    LayoutRect localExposeRect(box().absoluteToLocalQuad(FloatQuad(FloatRect(rect)), UseTransforms).boundingBox());
    localExposeRect.move(-box().borderLeft(), -box().borderTop());
    LayoutRect layerBounds(LayoutPoint(), LayoutSize(box().clientWidth(), box().clientHeight()));
    LayoutRect r = ScrollAlignment::getRectToExpose(layerBounds, localExposeRect, alignX, alignY);

    DoublePoint clampedScrollPosition = clampScrollPosition(scrollPositionDouble() + roundedIntSize(r.location()));
    if (clampedScrollPosition == scrollPositionDouble())
        return rect;

    DoubleSize oldScrollOffset = adjustedScrollOffset();
    scrollToPosition(clampedScrollPosition, ScrollOffsetUnclamped, ScrollBehaviorInstant, scrollType);
    DoubleSize scrollOffsetDifference = adjustedScrollOffset() - oldScrollOffset;
    localExposeRect.move(-LayoutSize(scrollOffsetDifference));
    return LayoutRect(box().localToAbsoluteQuad(FloatQuad(FloatRect(localExposeRect)), UseTransforms).boundingBox());
}

void PaintLayerScrollableArea::updateScrollableAreaSet(bool hasOverflow)
{
    LocalFrame* frame = box().frame();
    if (!frame)
        return;

    FrameView* frameView = frame->view();
    if (!frameView)
        return;

    // FIXME: Does this need to be fixed later for OOPI?
    bool isVisibleToHitTest = box().visibleToHitTesting();
    if (HTMLFrameOwnerElement* owner = frame->deprecatedLocalOwner())
        isVisibleToHitTest &= owner->layoutObject() && owner->layoutObject()->visibleToHitTesting();

    bool didScrollOverflow = m_scrollsOverflow;

    m_scrollsOverflow = hasOverflow && isVisibleToHitTest;
    if (didScrollOverflow == scrollsOverflow())
        return;

    if (m_scrollsOverflow) {
        ASSERT(canHaveOverflowScrollbars(box()));
        frameView->addScrollableArea(this);
    } else {
        frameView->removeScrollableArea(this);
    }
}

void PaintLayerScrollableArea::updateCompositingLayersAfterScroll()
{
    PaintLayerCompositor* compositor = box().view()->compositor();
    if (compositor->inCompositingMode()) {
        if (usesCompositedScrolling()) {
            ASSERT(layer()->hasCompositedLayerMapping());
            layer()->compositedLayerMapping()->setNeedsGraphicsLayerUpdate(GraphicsLayerUpdateSubtree);
            compositor->setNeedsCompositingUpdate(CompositingUpdateAfterGeometryChange);
        } else {
            layer()->setNeedsCompositingInputsUpdate();
        }
    }
}

bool PaintLayerScrollableArea::usesCompositedScrolling() const
{
    // Scroll form controls on the main thread so they exhibit correct touch scroll event bubbling
    if (box().isIntrinsicallyScrollable(VerticalScrollbar) || box().isIntrinsicallyScrollable(HorizontalScrollbar))
        return false;

    // See https://codereview.chromium.org/176633003/ for the tests that fail without this disabler.
    DisableCompositingQueryAsserts disabler;
    return layer()->hasCompositedLayerMapping() && layer()->compositedLayerMapping()->scrollingLayer();
}

bool PaintLayerScrollableArea::shouldScrollOnMainThread() const
{
    if (LocalFrame* frame = box().frame()) {
        if (Page* page = frame->page()) {
            if (page->scrollingCoordinator()->shouldUpdateScrollLayerPositionOnMainThread())
                return true;
        }
    }
    return ScrollableArea::shouldScrollOnMainThread();
}

static bool layerNeedsCompositedScrolling(PaintLayerScrollableArea::LCDTextMode mode, const PaintLayer* layer)
{
    if (!layer->scrollsOverflow())
        return false;

    Node* node = layer->enclosingNode();
    if (node && node->isElementNode() && (toElement(node)->compositorMutableProperties() & (CompositorMutableProperty::kScrollTop | CompositorMutableProperty::kScrollLeft)))
        return true;

    if (mode == PaintLayerScrollableArea::ConsiderLCDText && !layer->compositor()->preferCompositingToLCDTextEnabled())
        return false;

    return !layer->hasDescendantWithClipPath()
        && !layer->hasAncestorWithClipPath()
        && !layer->layoutObject()->style()->hasBorderRadius();
}

void PaintLayerScrollableArea::updateNeedsCompositedScrolling(LCDTextMode mode)
{
    const bool needsCompositedScrolling = layerNeedsCompositedScrolling(mode, layer());
    if (static_cast<bool>(m_needsCompositedScrolling) != needsCompositedScrolling) {
        m_needsCompositedScrolling = needsCompositedScrolling;
        layer()->didUpdateNeedsCompositedScrolling();
    }
}

void PaintLayerScrollableArea::setTopmostScrollChild(PaintLayer* scrollChild)
{
    // We only want to track the topmost scroll child for scrollable areas with
    // overlay scrollbars.
    if (!hasOverlayScrollbars())
        return;
    m_nextTopmostScrollChild = scrollChild;
}

bool PaintLayerScrollableArea::visualViewportSuppliesScrollbars() const
{
    if (!layer()->isRootLayer())
        return false;

    LocalFrame* frame = box().frame();
    if (!frame || !frame->isMainFrame() || !frame->settings())
        return false;

    return frame->settings()->viewportMetaEnabled();
}

Widget* PaintLayerScrollableArea::widget()
{
    return box().frame()->view();
}

PaintLayerScrollableArea::ScrollbarManager::ScrollbarManager(PaintLayerScrollableArea& scrollableArea)
    : m_scrollableArea(&scrollableArea)
    , m_canDetachScrollbars(0)
    , m_hBarIsAttached(0)
    , m_vBarIsAttached(0)
{
}

void PaintLayerScrollableArea::ScrollbarManager::dispose()
{
    m_canDetachScrollbars = m_hBarIsAttached = m_vBarIsAttached = 0;
    destroyScrollbar(HorizontalScrollbar);
    destroyScrollbar(VerticalScrollbar);
}

void PaintLayerScrollableArea::ScrollbarManager::setCanDetachScrollbars(bool detach)
{
    ASSERT(!m_hBarIsAttached || m_hBar);
    ASSERT(!m_vBarIsAttached || m_vBar);
    m_canDetachScrollbars = detach ? 1 : 0;
    if (!detach) {
        if (m_hBar && !m_hBarIsAttached)
            destroyScrollbar(HorizontalScrollbar);
        if (m_vBar && !m_vBarIsAttached)
            destroyScrollbar(VerticalScrollbar);
    }
}

void PaintLayerScrollableArea::ScrollbarManager::setHasHorizontalScrollbar(bool hasScrollbar)
{
    if (hasScrollbar) {
        // This doesn't hit in any tests, but since the equivalent code in setHasVerticalScrollbar
        // does, presumably this code does as well.
        DisableCompositingQueryAsserts disabler;
        if (!m_hBar)
            m_hBar = createScrollbar(HorizontalScrollbar);
        m_hBarIsAttached = 1;
    } else {
        m_hBarIsAttached = 0;
        if (!m_canDetachScrollbars)
            destroyScrollbar(HorizontalScrollbar);
    }
}

void PaintLayerScrollableArea::ScrollbarManager::setHasVerticalScrollbar(bool hasScrollbar)
{
    if (hasScrollbar) {
        DisableCompositingQueryAsserts disabler;
        if (!m_vBar)
            m_vBar = createScrollbar(VerticalScrollbar);
        m_vBarIsAttached = 1;
    } else {
        m_vBarIsAttached = 0;
        if (!m_canDetachScrollbars)
            destroyScrollbar(VerticalScrollbar);
    }
}

PassRefPtrWillBeRawPtr<Scrollbar> PaintLayerScrollableArea::ScrollbarManager::createScrollbar(ScrollbarOrientation orientation)
{
    ASSERT(orientation == HorizontalScrollbar ? !m_hBarIsAttached : !m_vBarIsAttached);
    RefPtrWillBeRawPtr<Scrollbar> scrollbar = nullptr;
    const LayoutObject& actualLayoutObject = layoutObjectForScrollbar(m_scrollableArea->box());
    bool hasCustomScrollbarStyle = actualLayoutObject.isBox() && actualLayoutObject.styleRef().hasPseudoStyle(SCROLLBAR);
    if (hasCustomScrollbarStyle) {
        scrollbar = LayoutScrollbar::createCustomScrollbar(m_scrollableArea.get(), orientation, actualLayoutObject.node());
    } else {
        ScrollbarControlSize scrollbarSize = RegularScrollbar;
        if (actualLayoutObject.styleRef().hasAppearance())
            scrollbarSize = LayoutTheme::theme().scrollbarControlSizeForPart(actualLayoutObject.styleRef().appearance());
        scrollbar = Scrollbar::create(m_scrollableArea.get(), orientation, scrollbarSize, &m_scrollableArea->box().frame()->page()->chromeClient());
        if (orientation == HorizontalScrollbar)
            m_scrollableArea->didAddScrollbar(*scrollbar, HorizontalScrollbar);
        else
            m_scrollableArea->didAddScrollbar(*scrollbar, VerticalScrollbar);
    }
    m_scrollableArea->box().document().view()->addChild(scrollbar.get());
    return scrollbar.release();
}

void PaintLayerScrollableArea::ScrollbarManager::destroyScrollbar(ScrollbarOrientation orientation)
{
    RefPtrWillBeMember<Scrollbar>& scrollbar = orientation == HorizontalScrollbar ? m_hBar : m_vBar;
    ASSERT(orientation == HorizontalScrollbar ? !m_hBarIsAttached: !m_vBarIsAttached);
    if (!scrollbar)
        return;

    m_scrollableArea->setScrollbarNeedsPaintInvalidation(orientation);

    if (!scrollbar->isCustomScrollbar())
        m_scrollableArea->willRemoveScrollbar(*scrollbar, orientation);

    toFrameView(scrollbar->parent())->removeChild(scrollbar.get());
    scrollbar->disconnectFromScrollableArea();
    scrollbar = nullptr;
}

DEFINE_TRACE(PaintLayerScrollableArea::ScrollbarManager)
{
    visitor->trace(m_scrollableArea);
    visitor->trace(m_hBar);
    visitor->trace(m_vBar);
}

} // namespace blink
