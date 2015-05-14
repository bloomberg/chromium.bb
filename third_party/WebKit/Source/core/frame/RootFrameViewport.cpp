// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/frame/RootFrameViewport.h"

#include "core/frame/FrameView.h"
#include "core/layout/ScrollAlignment.h"
#include "platform/geometry/DoubleRect.h"
#include "platform/geometry/FloatRect.h"
#include "platform/geometry/LayoutRect.h"

namespace blink {

RootFrameViewport::RootFrameViewport(ScrollableArea& visualViewport, ScrollableArea& layoutViewport)
    : m_visualViewport(visualViewport)
    , m_layoutViewport(layoutViewport)
{
}

void RootFrameViewport::updateScrollAnimator()
{
    scrollAnimator()->setCurrentPosition(toFloatPoint(scrollOffsetFromScrollAnimators()));
}

DoublePoint RootFrameViewport::scrollOffsetFromScrollAnimators() const
{
    return visualViewport().scrollAnimator()->currentPosition() + layoutViewport().scrollAnimator()->currentPosition();
}

DoubleRect RootFrameViewport::visibleContentRectDouble(IncludeScrollbarsInRect scrollbarInclusion) const
{
    return DoubleRect(scrollPositionDouble(), visualViewport().visibleContentRectDouble(scrollbarInclusion).size());
}

IntRect RootFrameViewport::visibleContentRect(IncludeScrollbarsInRect scrollbarInclusion) const
{
    return enclosingIntRect(visibleContentRectDouble(scrollbarInclusion));
}

bool RootFrameViewport::shouldUseIntegerScrollOffset() const
{
    // Fractionals are floored in the ScrollAnimator but it's important that the ScrollAnimators of the
    // visual and layout viewports get the precise fractional number so never use integer scrolling for
    // RootFrameViewport, we'll let the truncation happen in the subviewports.
    return false;
}

bool RootFrameViewport::isActive() const
{
    return layoutViewport().isActive();
}

int RootFrameViewport::scrollSize(ScrollbarOrientation orientation) const
{
    IntSize scrollDimensions = maximumScrollPosition() - minimumScrollPosition();
    return (orientation == HorizontalScrollbar) ? scrollDimensions.width() : scrollDimensions.height();
}

bool RootFrameViewport::isScrollCornerVisible() const
{
    return layoutViewport().isScrollCornerVisible();
}

IntRect RootFrameViewport::scrollCornerRect() const
{
    return layoutViewport().scrollCornerRect();
}

void RootFrameViewport::setScrollPosition(const DoublePoint& position, ScrollBehavior scrollBehavior)
{
    updateScrollAnimator();

    // TODO(bokan): Support smooth scrolling the visual viewport.
    if (scrollBehavior == ScrollBehaviorAuto)
        scrollBehavior = layoutViewport().scrollBehaviorStyle();
    if (scrollBehavior == ScrollBehaviorSmooth) {
        layoutViewport().setScrollPosition(position, scrollBehavior);
        return;
    }

    if (!layoutViewport().isProgrammaticallyScrollable())
        return;

    DoublePoint clampedPosition = clampScrollPosition(position);
    scrollToOffsetWithoutAnimation(toFloatPoint(clampedPosition));
}

ScrollResult RootFrameViewport::handleWheel(const PlatformWheelEvent& event)
{
    updateScrollAnimator();

    ScrollResult viewScrollResult(false);
    if (layoutViewport().isScrollable())
        viewScrollResult = layoutViewport().handleWheel(event);

    // The visual viewport will only accept pixel scrolls.
    if (!event.canScroll() || event.granularity() == ScrollByPageWheelEvent)
        return viewScrollResult;

    // Move the location by the negative of the remaining scroll delta.
    DoublePoint oldOffset = visualViewport().scrollPositionDouble();
    DoublePoint locationDelta;
    if (viewScrollResult.didScroll) {
        locationDelta = -DoublePoint(viewScrollResult.unusedScrollDeltaX, viewScrollResult.unusedScrollDeltaY);
    } else {
        if (event.railsMode() != PlatformEvent::RailsModeVertical)
            locationDelta.setX(-event.deltaX());
        if (event.railsMode() != PlatformEvent::RailsModeHorizontal)
            locationDelta.setY(-event.deltaY());
    }

    DoublePoint targetPosition = visualViewport().adjustScrollPositionWithinRange(
        visualViewport().scrollPositionDouble() + toDoubleSize(locationDelta));
    visualViewport().scrollToOffsetWithoutAnimation(FloatPoint(targetPosition));

    DoublePoint usedLocationDelta(visualViewport().scrollPositionDouble() - oldOffset);
    if (!viewScrollResult.didScroll && usedLocationDelta == DoublePoint::zero())
        return ScrollResult(false);

    DoubleSize unusedLocationDelta(locationDelta - usedLocationDelta);
    return ScrollResult(true, -unusedLocationDelta.width(), -unusedLocationDelta.height());
}

LayoutRect RootFrameViewport::scrollIntoView(const LayoutRect& rectInContent, const ScrollAlignment& alignX, const ScrollAlignment& alignY)
{
    // We want to move the rect into the viewport that excludes the scrollbars so we intersect
    // the pinch viewport with the scrollbar-excluded frameView content rect. However, we don't
    // use visibleContentRect directly since it floors the scroll position. Instead, we use
    // FrameView::scrollPositionDouble and construct a LayoutRect from that (the FrameView size
    // is always integer sized.

    LayoutRect frameRectInContent = LayoutRect(
        layoutViewport().scrollPositionDouble(),
        layoutViewport().visibleContentRect().size());
    LayoutRect pinchRectInContent = LayoutRect(
        layoutViewport().scrollPositionDouble() + toDoubleSize(visualViewport().scrollPositionDouble()),
        visualViewport().visibleContentRect().size());

    LayoutRect viewRectInContent = intersection(pinchRectInContent, frameRectInContent);
    LayoutRect targetViewport =
        ScrollAlignment::getRectToExpose(viewRectInContent, rectInContent, alignX, alignY);

    // pinchViewport.scrollIntoView will attempt to center the given rect within the viewport
    // so to prevent it from adjusting r's coordinates the rect must match the viewport's size
    // i.e. add the subtracted scrollbars from above back in.
    // FIXME: This is hacky and required because getRectToExpose doesn't naturally account
    // for the two viewports. crbug.com/449340.
    targetViewport.setSize(LayoutSize(visualViewport().visibleContentRect().size()));

    // Snap the visible rect to layout units to match the calculated target viewport rect.
    FloatRect visible =
        LayoutRect(visualViewport().scrollPositionDouble(), visualViewport().visibleContentRect().size());

    float centeringOffsetX = (visible.width() - targetViewport.width()) / 2;
    float centeringOffsetY = (visible.height() - targetViewport.height()) / 2;

    DoublePoint targetOffset(
        targetViewport.x() - centeringOffsetX,
        targetViewport.y() - centeringOffsetY);

    setScrollPosition(targetOffset);

    // RootFrameViewport only changes the viewport relative to the document so we can't change the input
    // rect's location relative to the document origin.
    return rectInContent;
}

void RootFrameViewport::setScrollOffset(const IntPoint& offset)
{
    setScrollOffset(DoublePoint(offset));
}

void RootFrameViewport::setScrollOffset(const DoublePoint& offset)
{
    // Make sure we use the scroll positions as reported by each viewport's ScrollAnimator, since its
    // ScrollableArea's position may have the fractional part truncated off.
    DoublePoint oldPosition = scrollOffsetFromScrollAnimators();

    DoubleSize delta = offset - oldPosition;

    if (delta.isZero())
        return;

    DoublePoint targetPosition = layoutViewport().adjustScrollPositionWithinRange(layoutViewport().scrollAnimator()->currentPosition() + delta);
    layoutViewport().scrollToOffsetWithoutAnimation(toFloatPoint(targetPosition));

    DoubleSize applied = scrollOffsetFromScrollAnimators() - oldPosition;
    delta -= applied;

    if (delta.isZero())
        return;

    targetPosition = visualViewport().adjustScrollPositionWithinRange(visualViewport().scrollAnimator()->currentPosition() + delta);
    visualViewport().scrollToOffsetWithoutAnimation(toFloatPoint(targetPosition));
}

IntPoint RootFrameViewport::scrollPosition() const
{
    return flooredIntPoint(scrollPositionDouble());
}

DoublePoint RootFrameViewport::scrollPositionDouble() const
{
    return layoutViewport().scrollPositionDouble() + toDoubleSize(visualViewport().scrollPositionDouble());
}

IntPoint RootFrameViewport::minimumScrollPosition() const
{
    return IntPoint(layoutViewport().minimumScrollPosition() - visualViewport().minimumScrollPosition());
}

IntPoint RootFrameViewport::maximumScrollPosition() const
{
    return layoutViewport().maximumScrollPosition() + visualViewport().maximumScrollPosition();
}

DoublePoint RootFrameViewport::maximumScrollPositionDouble() const
{
    return layoutViewport().maximumScrollPositionDouble() + toDoubleSize(visualViewport().maximumScrollPositionDouble());
}

IntSize RootFrameViewport::contentsSize() const
{
    return layoutViewport().contentsSize();
}

bool RootFrameViewport::scrollbarsCanBeActive() const
{
    return layoutViewport().scrollbarsCanBeActive();
}

IntRect RootFrameViewport::scrollableAreaBoundingBox() const
{
    return layoutViewport().scrollableAreaBoundingBox();
}

bool RootFrameViewport::userInputScrollable(ScrollbarOrientation orientation) const
{
    return visualViewport().userInputScrollable(orientation) || layoutViewport().userInputScrollable(orientation);
}

bool RootFrameViewport::shouldPlaceVerticalScrollbarOnLeft() const
{
    return layoutViewport().shouldPlaceVerticalScrollbarOnLeft();
}

void RootFrameViewport::invalidateScrollbarRect(Scrollbar* scrollbar, const IntRect& rect)
{
    layoutViewport().invalidateScrollbarRect(scrollbar, rect);
}

void RootFrameViewport::invalidateScrollCornerRect(const IntRect& rect)
{
    layoutViewport().invalidateScrollCornerRect(rect);
}

GraphicsLayer* RootFrameViewport::layerForContainer() const
{
    return layoutViewport().layerForContainer();
}

GraphicsLayer* RootFrameViewport::layerForScrolling() const
{
    return layoutViewport().layerForScrolling();
}

GraphicsLayer* RootFrameViewport::layerForHorizontalScrollbar() const
{
    return layoutViewport().layerForHorizontalScrollbar();
}

GraphicsLayer* RootFrameViewport::layerForVerticalScrollbar() const
{
    return layoutViewport().layerForVerticalScrollbar();
}

bool RootFrameViewport::scroll(ScrollDirection direction, ScrollGranularity granularity, float delta)
{
    ASSERT(!isLogical(direction));

    updateScrollAnimator();

    ScrollbarOrientation orientation;

    if (direction == ScrollUp || direction == ScrollDown)
        orientation = VerticalScrollbar;
    else
        orientation = HorizontalScrollbar;

    if (layoutViewport().userInputScrollable(orientation) && visualViewport().userInputScrollable(orientation))
        return ScrollableArea::scroll(direction, granularity, delta);

    if (visualViewport().userInputScrollable(orientation))
        return visualViewport().scroll(direction, granularity, delta);

    if (layoutViewport().userInputScrollable(orientation))
        return layoutViewport().scroll(direction, granularity, delta);

    return false;
}

} // namespace blink
