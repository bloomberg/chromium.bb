// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/RootFrameViewport.h"

#include "core/frame/LocalFrameView.h"
#include "core/layout/ScrollAlignment.h"
#include "core/layout/ScrollAnchor.h"
#include "platform/geometry/DoubleRect.h"
#include "platform/geometry/FloatRect.h"
#include "platform/geometry/LayoutRect.h"

namespace blink {

RootFrameViewport::RootFrameViewport(ScrollableArea& visual_viewport,
                                     ScrollableArea& layout_viewport)
    : visual_viewport_(visual_viewport) {
  SetLayoutViewport(layout_viewport);
}

void RootFrameViewport::SetLayoutViewport(ScrollableArea& new_layout_viewport) {
  if (layout_viewport_.Get() == &new_layout_viewport)
    return;

  if (layout_viewport_ && layout_viewport_->GetScrollAnchor())
    layout_viewport_->GetScrollAnchor()->SetScroller(layout_viewport_.Get());

  layout_viewport_ = &new_layout_viewport;

  if (layout_viewport_->GetScrollAnchor())
    layout_viewport_->GetScrollAnchor()->SetScroller(this);
}

ScrollableArea& RootFrameViewport::LayoutViewport() const {
  DCHECK(layout_viewport_);
  return *layout_viewport_;
}

LayoutRect RootFrameViewport::RootContentsToLayoutViewportContents(
    LocalFrameView& root_frame_view,
    const LayoutRect& rect) const {
  LayoutRect ret(rect);

  // If the root LocalFrameView is the layout viewport then coordinates in the
  // root LocalFrameView's content space are already in the layout viewport's
  // content space.
  if (root_frame_view.LayoutViewportScrollableArea() == &LayoutViewport())
    return ret;

  // Make the given rect relative to the top of the layout viewport's content
  // by adding the scroll position.
  // TODO(bokan): This will have to be revisited if we ever remove the
  // restriction that a root scroller must be exactly screen filling.
  ret.Move(LayoutSize(LayoutViewport().GetScrollOffset()));

  return ret;
}

void RootFrameViewport::RestoreToAnchor(const ScrollOffset& target_offset) {
  // Clamp the scroll offset of each viewport now so that we force any invalid
  // offsets to become valid so we can compute the correct deltas.
  VisualViewport().SetScrollOffset(VisualViewport().GetScrollOffset(),
                                   kProgrammaticScroll);
  LayoutViewport().SetScrollOffset(LayoutViewport().GetScrollOffset(),
                                   kProgrammaticScroll);

  ScrollOffset delta = target_offset - GetScrollOffset();

  VisualViewport().SetScrollOffset(VisualViewport().GetScrollOffset() + delta,
                                   kProgrammaticScroll);

  delta = target_offset - GetScrollOffset();

  // Since the main thread LocalFrameView has integer scroll offsets, scroll it
  // to the next pixel and then we'll scroll the visual viewport again to
  // compensate for the sub-pixel offset. We need this "overscroll" to ensure
  // the pixel of which we want to be partially in appears fully inside the
  // LocalFrameView since the VisualViewport is bounded by the LocalFrameView.
  IntSize layout_delta = IntSize(
      delta.Width() < 0 ? floor(delta.Width()) : ceil(delta.Width()),
      delta.Height() < 0 ? floor(delta.Height()) : ceil(delta.Height()));

  LayoutViewport().SetScrollOffset(
      ScrollOffset(LayoutViewport().ScrollOffsetInt() + layout_delta),
      kProgrammaticScroll);

  delta = target_offset - GetScrollOffset();
  VisualViewport().SetScrollOffset(VisualViewport().GetScrollOffset() + delta,
                                   kProgrammaticScroll);
}

void RootFrameViewport::DidUpdateVisualViewport() {
  if (RuntimeEnabledFeatures::ScrollAnchoringEnabled()) {
    if (ScrollAnchor* anchor = LayoutViewport().GetScrollAnchor())
      anchor->Clear();
  }
}

LayoutBox* RootFrameViewport::GetLayoutBox() const {
  return LayoutViewport().GetLayoutBox();
}

FloatQuad RootFrameViewport::LocalToVisibleContentQuad(
    const FloatQuad& quad,
    const LayoutObject* local_object,
    unsigned flags) const {
  if (!layout_viewport_)
    return quad;
  FloatQuad viewport_quad =
      layout_viewport_->LocalToVisibleContentQuad(quad, local_object, flags);
  if (visual_viewport_) {
    viewport_quad = visual_viewport_->LocalToVisibleContentQuad(
        viewport_quad, local_object, flags);
  }
  return viewport_quad;
}

scoped_refptr<WebTaskRunner> RootFrameViewport::GetTimerTaskRunner() const {
  return LayoutViewport().GetTimerTaskRunner();
}

int RootFrameViewport::HorizontalScrollbarHeight(
    OverlayScrollbarClipBehavior behavior) const {
  return LayoutViewport().HorizontalScrollbarHeight(behavior);
}

int RootFrameViewport::VerticalScrollbarWidth(
    OverlayScrollbarClipBehavior behavior) const {
  return LayoutViewport().VerticalScrollbarWidth(behavior);
}

void RootFrameViewport::UpdateScrollAnimator() {
  GetScrollAnimator().SetCurrentOffset(
      ToFloatSize(ScrollOffsetFromScrollAnimators()));
}

ScrollOffset RootFrameViewport::ScrollOffsetFromScrollAnimators() const {
  return VisualViewport().GetScrollAnimator().CurrentOffset() +
         LayoutViewport().GetScrollAnimator().CurrentOffset();
}

IntRect RootFrameViewport::VisibleContentRect(
    IncludeScrollbarsInRect scrollbar_inclusion) const {
  return IntRect(
      IntPoint(ScrollOffsetInt()),
      VisualViewport().VisibleContentRect(scrollbar_inclusion).Size());
}

bool RootFrameViewport::ShouldUseIntegerScrollOffset() const {
  // Fractionals are floored in the ScrollAnimatorBase but it's important that
  // the ScrollAnimators of the visual and layout viewports get the precise
  // fractional number so never use integer scrolling for RootFrameViewport,
  // we'll let the truncation happen in the subviewports.
  return false;
}

bool RootFrameViewport::IsActive() const {
  return LayoutViewport().IsActive();
}

int RootFrameViewport::ScrollSize(ScrollbarOrientation orientation) const {
  IntSize scroll_dimensions =
      MaximumScrollOffsetInt() - MinimumScrollOffsetInt();
  return (orientation == kHorizontalScrollbar) ? scroll_dimensions.Width()
                                               : scroll_dimensions.Height();
}

bool RootFrameViewport::IsScrollCornerVisible() const {
  return LayoutViewport().IsScrollCornerVisible();
}

IntRect RootFrameViewport::ScrollCornerRect() const {
  return LayoutViewport().ScrollCornerRect();
}

void RootFrameViewport::SetScrollOffset(const ScrollOffset& offset,
                                        ScrollType scroll_type,
                                        ScrollBehavior scroll_behavior) {
  UpdateScrollAnimator();

  if (scroll_behavior == kScrollBehaviorAuto)
    scroll_behavior = ScrollBehaviorStyle();

  if (scroll_type == kProgrammaticScroll &&
      !LayoutViewport().IsProgrammaticallyScrollable())
    return;

  if (scroll_type == kAnchoringScroll) {
    DistributeScrollBetweenViewports(offset, scroll_type, scroll_behavior,
                                     kLayoutViewport);
    return;
  }

  if (scroll_behavior == kScrollBehaviorSmooth) {
    DistributeScrollBetweenViewports(offset, scroll_type, scroll_behavior,
                                     kVisualViewport);
    return;
  }

  ScrollOffset clamped_offset = ClampScrollOffset(offset);
  ScrollableArea::SetScrollOffset(clamped_offset, scroll_type, scroll_behavior);
}

ScrollBehavior RootFrameViewport::ScrollBehaviorStyle() const {
  return LayoutViewport().ScrollBehaviorStyle();
}

LayoutRect RootFrameViewport::ScrollIntoView(const LayoutRect& rect_in_content,
                                             const ScrollAlignment& align_x,
                                             const ScrollAlignment& align_y,
                                             bool is_smooth,
                                             ScrollType scroll_type,
                                             bool is_for_scroll_sequence) {
  // We want to move the rect into the viewport that excludes the scrollbars so
  // we intersect the visual viewport with the scrollbar-excluded frameView
  // content rect. However, we don't use visibleContentRect directly since it
  // floors the scroll offset. Instead, we use ScrollAnimatorBase::currentOffset
  // and construct a LayoutRect from that.
  LayoutRect frame_rect_in_content = LayoutRect(
      FloatPoint(LayoutViewport().GetScrollAnimator().CurrentOffset()),
      FloatSize(LayoutViewport().VisibleContentRect().Size()));
  LayoutRect visual_rect_in_content =
      LayoutRect(FloatPoint(ScrollOffsetFromScrollAnimators()),
                 FloatSize(VisualViewport().VisibleContentRect().Size()));

  // Intersect layout and visual rects to exclude the scrollbar from the view
  // rect.
  LayoutRect view_rect_in_content =
      Intersection(visual_rect_in_content, frame_rect_in_content);
  LayoutRect target_viewport = ScrollAlignment::GetRectToExpose(
      view_rect_in_content, rect_in_content, align_x, align_y);
  if (target_viewport != view_rect_in_content) {
    ScrollOffset target_offset(target_viewport.X(), target_viewport.Y());
    if (is_for_scroll_sequence) {
      DCHECK(scroll_type == kProgrammaticScroll);
      ScrollBehavior behavior =
          is_smooth ? kScrollBehaviorSmooth : kScrollBehaviorInstant;
      GetSmoothScrollSequencer()->QueueAnimation(this, target_offset, behavior);
    } else {
      SetScrollOffset(target_offset, scroll_type);
    }
  }

  // RootFrameViewport only changes the viewport relative to the document so we
  // can't change the input rect's location relative to the document origin.
  // TODO(szager): PaintLayerScrollableArea::ScrollIntoView clips the return
  // value to the visible content rect, but this does not.
  return rect_in_content;
}

void RootFrameViewport::UpdateScrollOffset(const ScrollOffset& offset,
                                           ScrollType scroll_type) {
  DistributeScrollBetweenViewports(offset, scroll_type, kScrollBehaviorInstant,
                                   kVisualViewport);
}

void RootFrameViewport::DistributeScrollBetweenViewports(
    const ScrollOffset& offset,
    ScrollType scroll_type,
    ScrollBehavior behavior,
    ViewportToScrollFirst scroll_first) {
  // Make sure we use the scroll offsets as reported by each viewport's
  // ScrollAnimatorBase, since its ScrollableArea's offset may have the
  // fractional part truncated off.
  // TODO(szager): Now that scroll offsets are stored as floats, can we take the
  // scroll offset directly from the ScrollableArea's rather than the animators?
  ScrollOffset old_offset = ScrollOffsetFromScrollAnimators();

  ScrollOffset delta = offset - old_offset;

  if (delta.IsZero())
    return;

  ScrollableArea& primary =
      scroll_first == kVisualViewport ? VisualViewport() : LayoutViewport();
  ScrollableArea& secondary =
      scroll_first == kVisualViewport ? LayoutViewport() : VisualViewport();

  ScrollOffset target_offset = primary.ClampScrollOffset(
      primary.GetScrollAnimator().CurrentOffset() + delta);

  // DistributeScrollBetweenViewports can be called from SetScrollOffset,
  // so we assume that aborting sequenced smooth scrolls has been handled.
  // It can also be called from inside an animation to set the offset in
  // each frame. In that case, we shouldn't abort sequenced smooth scrolls.
  primary.SetScrollOffset(target_offset, scroll_type, behavior);

  // Scroll the secondary viewport if all of the scroll was not applied to the
  // primary viewport.
  ScrollOffset updated_offset =
      secondary.GetScrollAnimator().CurrentOffset() + FloatSize(target_offset);
  ScrollOffset applied = updated_offset - old_offset;
  delta -= applied;

  if (delta.IsZero())
    return;

  target_offset = secondary.ClampScrollOffset(
      secondary.GetScrollAnimator().CurrentOffset() + delta);
  secondary.SetScrollOffset(target_offset, scroll_type, behavior);
}

IntSize RootFrameViewport::ScrollOffsetInt() const {
  return FlooredIntSize(GetScrollOffset());
}

ScrollOffset RootFrameViewport::GetScrollOffset() const {
  return LayoutViewport().GetScrollOffset() +
         VisualViewport().GetScrollOffset();
}

IntSize RootFrameViewport::MinimumScrollOffsetInt() const {
  return IntSize(LayoutViewport().MinimumScrollOffsetInt() +
                 VisualViewport().MinimumScrollOffsetInt());
}

IntSize RootFrameViewport::MaximumScrollOffsetInt() const {
  return LayoutViewport().MaximumScrollOffsetInt() +
         VisualViewport().MaximumScrollOffsetInt();
}

ScrollOffset RootFrameViewport::MaximumScrollOffset() const {
  return LayoutViewport().MaximumScrollOffset() +
         VisualViewport().MaximumScrollOffset();
}

IntSize RootFrameViewport::ContentsSize() const {
  return LayoutViewport().ContentsSize();
}

bool RootFrameViewport::ScrollbarsCanBeActive() const {
  return LayoutViewport().ScrollbarsCanBeActive();
}

IntRect RootFrameViewport::ScrollableAreaBoundingBox() const {
  return LayoutViewport().ScrollableAreaBoundingBox();
}

bool RootFrameViewport::UserInputScrollable(
    ScrollbarOrientation orientation) const {
  return VisualViewport().UserInputScrollable(orientation) ||
         LayoutViewport().UserInputScrollable(orientation);
}

bool RootFrameViewport::ShouldPlaceVerticalScrollbarOnLeft() const {
  return LayoutViewport().ShouldPlaceVerticalScrollbarOnLeft();
}

void RootFrameViewport::ScrollControlWasSetNeedsPaintInvalidation() {
  LayoutViewport().ScrollControlWasSetNeedsPaintInvalidation();
}

GraphicsLayer* RootFrameViewport::LayerForContainer() const {
  return LayoutViewport().LayerForContainer();
}

GraphicsLayer* RootFrameViewport::LayerForScrolling() const {
  return LayoutViewport().LayerForScrolling();
}

GraphicsLayer* RootFrameViewport::LayerForHorizontalScrollbar() const {
  return LayoutViewport().LayerForHorizontalScrollbar();
}

GraphicsLayer* RootFrameViewport::LayerForVerticalScrollbar() const {
  return LayoutViewport().LayerForVerticalScrollbar();
}

GraphicsLayer* RootFrameViewport::LayerForScrollCorner() const {
  return LayoutViewport().LayerForScrollCorner();
}

ScrollResult RootFrameViewport::UserScroll(ScrollGranularity granularity,
                                           const FloatSize& delta) {
  // TODO(bokan/ymalik): Once smooth scrolling is permanently enabled we
  // should be able to remove this method override and use the base class
  // version: ScrollableArea::userScroll.

  UpdateScrollAnimator();

  // Distribute the scroll between the visual and layout viewport.

  float step_x = ScrollStep(granularity, kHorizontalScrollbar);
  float step_y = ScrollStep(granularity, kVerticalScrollbar);

  FloatSize pixel_delta(delta);
  pixel_delta.Scale(step_x, step_y);

  // Precompute the amount of possible scrolling since, when animated,
  // ScrollAnimator::userScroll will report having consumed the total given
  // scroll delta, regardless of how much will actually scroll, but we need to
  // know how much to leave for the layout viewport.
  FloatSize visual_consumed_delta =
      VisualViewport().GetScrollAnimator().ComputeDeltaToConsume(pixel_delta);

  // Split the remaining delta between scrollable and unscrollable axes of the
  // layout viewport. We only pass a delta to the scrollable axes and remember
  // how much was held back so we can add it to the unused delta in the
  // result.
  FloatSize layout_delta = pixel_delta - visual_consumed_delta;
  FloatSize scrollable_axis_delta(
      LayoutViewport().UserInputScrollable(kHorizontalScrollbar)
          ? layout_delta.Width()
          : 0,
      LayoutViewport().UserInputScrollable(kVerticalScrollbar)
          ? layout_delta.Height()
          : 0);

  // If there won't be any scrolling, bail early so we don't produce any side
  // effects like cancelling existing animations.
  if (visual_consumed_delta.IsZero() && scrollable_axis_delta.IsZero()) {
    return ScrollResult(false, false, pixel_delta.Width(),
                        pixel_delta.Height());
  }

  CancelProgrammaticScrollAnimation();
  if (SmoothScrollSequencer* sequencer = GetSmoothScrollSequencer())
    sequencer->AbortAnimations();

  // TODO(bokan): Why do we call userScroll on the animators directly and
  // not through the ScrollableAreas?
  ScrollResult visual_result = VisualViewport().GetScrollAnimator().UserScroll(
      granularity, visual_consumed_delta);

  if (visual_consumed_delta == pixel_delta)
    return visual_result;

  ScrollResult layout_result = LayoutViewport().GetScrollAnimator().UserScroll(
      granularity, scrollable_axis_delta);

  // Remember to add any delta not used because of !userInputScrollable to the
  // unusedScrollDelta in the result.
  FloatSize unscrollable_axis_delta = layout_delta - scrollable_axis_delta;

  return ScrollResult(
      visual_result.did_scroll_x || layout_result.did_scroll_x,
      visual_result.did_scroll_y || layout_result.did_scroll_y,
      layout_result.unused_scroll_delta_x + unscrollable_axis_delta.Width(),
      layout_result.unused_scroll_delta_y + unscrollable_axis_delta.Height());
}

bool RootFrameViewport::ScrollAnimatorEnabled() const {
  return LayoutViewport().ScrollAnimatorEnabled();
}

CompositorElementId RootFrameViewport::GetCompositorElementId() const {
  return LayoutViewport().GetCompositorElementId();
}

PlatformChromeClient* RootFrameViewport::GetChromeClient() const {
  return LayoutViewport().GetChromeClient();
}

SmoothScrollSequencer* RootFrameViewport::GetSmoothScrollSequencer() const {
  return LayoutViewport().GetSmoothScrollSequencer();
}

void RootFrameViewport::ServiceScrollAnimations(double monotonic_time) {
  ScrollableArea::ServiceScrollAnimations(monotonic_time);
  LayoutViewport().ServiceScrollAnimations(monotonic_time);
  VisualViewport().ServiceScrollAnimations(monotonic_time);
}

void RootFrameViewport::UpdateCompositorScrollAnimations() {
  ScrollableArea::UpdateCompositorScrollAnimations();
  LayoutViewport().UpdateCompositorScrollAnimations();
  VisualViewport().UpdateCompositorScrollAnimations();
}

void RootFrameViewport::CancelProgrammaticScrollAnimation() {
  ScrollableArea::CancelProgrammaticScrollAnimation();
  LayoutViewport().CancelProgrammaticScrollAnimation();
  VisualViewport().CancelProgrammaticScrollAnimation();
}

void RootFrameViewport::ClearScrollableArea() {
  ScrollableArea::ClearScrollableArea();
  LayoutViewport().ClearScrollableArea();
  VisualViewport().ClearScrollableArea();
}

void RootFrameViewport::Trace(blink::Visitor* visitor) {
  visitor->Trace(visual_viewport_);
  visitor->Trace(layout_viewport_);
  ScrollableArea::Trace(visitor);
}

}  // namespace blink
