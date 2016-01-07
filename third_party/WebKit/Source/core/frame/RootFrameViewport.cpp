// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
    scrollAnimator().setCurrentPosition(toFloatPoint(scrollOffsetFromScrollAnimators()));
}

DoublePoint RootFrameViewport::scrollOffsetFromScrollAnimators() const
{
    return visualViewport().scrollAnimator().currentPosition() + layoutViewport().scrollAnimator().currentPosition();
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
    // Fractionals are floored in the ScrollAnimatorBase but it's important that the ScrollAnimators of the
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

void RootFrameViewport::setScrollPosition(const DoublePoint& position, ScrollType scrollType, ScrollBehavior scrollBehavior)
{
    updateScrollAnimator();

    if (scrollBehavior == ScrollBehaviorAuto)
        scrollBehavior = scrollBehaviorStyle();

    if (scrollType == ProgrammaticScroll && !layoutViewport().isProgrammaticallyScrollable())
        return;

    if (scrollBehavior == ScrollBehaviorSmooth) {
        distributeScrollBetweenViewports(position, scrollType, scrollBehavior);
        return;
    }

    DoublePoint clampedPosition = clampScrollPosition(position);
    ScrollableArea::setScrollPosition(clampedPosition, scrollType, scrollBehavior);
}

ScrollBehavior RootFrameViewport::scrollBehaviorStyle() const
{
    return layoutViewport().scrollBehaviorStyle();
}

LayoutRect RootFrameViewport::scrollIntoView(const LayoutRect& rectInContent, const ScrollAlignment& alignX, const ScrollAlignment& alignY, ScrollType scrollType)
{
    // We want to move the rect into the viewport that excludes the scrollbars so we intersect
    // the visual viewport with the scrollbar-excluded frameView content rect. However, we don't
    // use visibleContentRect directly since it floors the scroll position. Instead, we use
    // ScrollAnimatorBase::currentPosition and construct a LayoutRect from that.

    LayoutRect frameRectInContent = LayoutRect(
        layoutViewport().scrollAnimator().currentPosition(),
        layoutViewport().visibleContentRect().size());
    LayoutRect visualRectInContent = LayoutRect(
        scrollOffsetFromScrollAnimators(),
        visualViewport().visibleContentRect().size());

    // Intersect layout and visual rects to exclude the scrollbar from the view rect.
    LayoutRect viewRectInContent = intersection(visualRectInContent, frameRectInContent);
    LayoutRect targetViewport =
        ScrollAlignment::getRectToExpose(viewRectInContent, rectInContent, alignX, alignY);
    DoublePoint targetOffset(targetViewport.x(), targetViewport.y());

    setScrollPosition(targetOffset, scrollType, ScrollBehaviorInstant);

    // RootFrameViewport only changes the viewport relative to the document so we can't change the input
    // rect's location relative to the document origin.
    return rectInContent;
}

void RootFrameViewport::setScrollOffset(const IntPoint& offset, ScrollType scrollType)
{
    setScrollOffset(DoublePoint(offset), scrollType);
}

void RootFrameViewport::setScrollOffset(const DoublePoint& offset, ScrollType scrollType)
{
    distributeScrollBetweenViewports(DoublePoint(offset), scrollType, ScrollBehaviorInstant);
}

void RootFrameViewport::distributeScrollBetweenViewports(const DoublePoint& offset, ScrollType scrollType, ScrollBehavior behavior)
{
    // Make sure we use the scroll positions as reported by each viewport's ScrollAnimatorBase, since its
    // ScrollableArea's position may have the fractional part truncated off.
    DoublePoint oldPosition = scrollOffsetFromScrollAnimators();

    DoubleSize delta = offset - oldPosition;

    if (delta.isZero())
        return;

    DoublePoint targetPosition = visualViewport().clampScrollPosition(
        visualViewport().scrollAnimator().currentPosition() + delta);

    visualViewport().setScrollPosition(targetPosition, scrollType, behavior);

    // Scroll the secondary viewport if all of the scroll was not applied to the
    // primary viewport.
    DoublePoint updatedPosition = layoutViewport().scrollAnimator().currentPosition() + FloatPoint(targetPosition);
    DoubleSize applied = updatedPosition - oldPosition;
    delta -= applied;

    if (delta.isZero())
        return;

    targetPosition = layoutViewport().clampScrollPosition(layoutViewport().scrollAnimator().currentPosition() + delta);
    layoutViewport().setScrollPosition(targetPosition, scrollType, behavior);
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
    return IntPoint(layoutViewport().minimumScrollPosition() + visualViewport().minimumScrollPosition());
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

void RootFrameViewport::scrollControlWasSetNeedsPaintInvalidation()
{
    layoutViewport().scrollControlWasSetNeedsPaintInvalidation();
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

ScrollResultOneDimensional RootFrameViewport::userScroll(ScrollDirectionPhysical direction, ScrollGranularity granularity, float delta)
{
    updateScrollAnimator();

    ScrollbarOrientation orientation = scrollbarOrientationFromDirection(direction);

    if (layoutViewport().userInputScrollable(orientation) && visualViewport().userInputScrollable(orientation)) {
        // Distribute the scroll between the visual and layout viewport.
        float step = scrollStep(granularity, orientation);

        if (direction == ScrollUp || direction == ScrollLeft)
            delta = -delta;

        // This is the total amount we need to scroll. Instead of passing step
        // to the scroll animator and letting it compute the total delta, we
        // give it the delta it should scroll. This way we can apply the
        // unused delta from the visual viewport to the layout viewport.
        // TODO(ymalik): ScrollAnimator should return unused delta in pixles
        // rather than in terms of steps.
        delta *= step;

        cancelProgrammaticScrollAnimation();

        float visualUsedDelta = visualViewport().scrollAnimator().computeDeltaToConsume(orientation, delta);
        ScrollResultOneDimensional visualResult = visualViewport().scrollAnimator().userScroll(
            orientation, granularity, 1 /* step */, visualUsedDelta);

        // Scroll the layout viewport if all of the scroll was not applied to the
        // visual viewport.
        if (visualUsedDelta == delta)
            return visualResult;

        ScrollResultOneDimensional layoutResult = layoutViewport().scrollAnimator().userScroll(
            orientation, granularity, 1 /* step */, delta - visualUsedDelta);

        return ScrollResultOneDimensional(visualResult.didScroll || layoutResult.didScroll,
            layoutResult.unusedScrollDelta / step);
    }

    if (visualViewport().userInputScrollable(orientation))
        return visualViewport().userScroll(direction, granularity, delta);

    if (layoutViewport().userInputScrollable(orientation))
        return layoutViewport().userScroll(direction, granularity, delta);

    return ScrollResultOneDimensional(false, delta);
}

bool RootFrameViewport::scrollAnimatorEnabled() const
{
    return layoutViewport().scrollAnimatorEnabled();
}

HostWindow* RootFrameViewport::hostWindow() const
{
    return layoutViewport().hostWindow();
}

void RootFrameViewport::serviceScrollAnimations(double monotonicTime)
{
    ScrollableArea::serviceScrollAnimations(monotonicTime);
    layoutViewport().serviceScrollAnimations(monotonicTime);
    visualViewport().serviceScrollAnimations(monotonicTime);
}

void RootFrameViewport::updateCompositorScrollAnimations()
{
    ScrollableArea::updateCompositorScrollAnimations();
    layoutViewport().updateCompositorScrollAnimations();
    visualViewport().updateCompositorScrollAnimations();
}

void RootFrameViewport::cancelProgrammaticScrollAnimation()
{
    ScrollableArea::cancelProgrammaticScrollAnimation();
    layoutViewport().cancelProgrammaticScrollAnimation();
    visualViewport().cancelProgrammaticScrollAnimation();
}

Widget* RootFrameViewport::widget()
{
    return visualViewport().widget();
}

DEFINE_TRACE(RootFrameViewport)
{
    visitor->trace(m_visualViewport);
    visitor->trace(m_layoutViewport);
    ScrollableArea::trace(visitor);
}

} // namespace blink
