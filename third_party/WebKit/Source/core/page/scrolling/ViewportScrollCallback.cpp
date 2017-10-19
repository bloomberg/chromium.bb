// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/page/scrolling/ViewportScrollCallback.h"

#include "core/frame/BrowserControls.h"
#include "core/frame/LocalFrameView.h"
#include "core/frame/RootFrameViewport.h"
#include "core/frame/Settings.h"
#include "core/page/scrolling/OverscrollController.h"
#include "core/page/scrolling/ScrollState.h"
#include "platform/geometry/FloatSize.h"
#include "platform/scroll/ScrollableArea.h"

namespace blink {

ViewportScrollCallback::ViewportScrollCallback(
    BrowserControls* browser_controls,
    OverscrollController* overscroll_controller,
    RootFrameViewport& root_frame_viewport)
    : browser_controls_(browser_controls),
      overscroll_controller_(overscroll_controller),
      root_frame_viewport_(&root_frame_viewport) {}

ViewportScrollCallback::~ViewportScrollCallback() {}

void ViewportScrollCallback::Trace(blink::Visitor* visitor) {
  visitor->Trace(browser_controls_);
  visitor->Trace(overscroll_controller_);
  visitor->Trace(root_frame_viewport_);
  ScrollStateCallback::Trace(visitor);
}

bool ViewportScrollCallback::ShouldScrollBrowserControls(
    const ScrollOffset& delta,
    ScrollGranularity granularity) const {
  if (granularity != kScrollByPixel && granularity != kScrollByPrecisePixel)
    return false;

  if (!root_frame_viewport_)
    return false;

  ScrollOffset max_scroll = root_frame_viewport_->MaximumScrollOffset();
  ScrollOffset scroll_offset = root_frame_viewport_->GetScrollOffset();

  // Always give the delta to the browser controls if the scroll is in
  // the direction to show the browser controls. If it's in the
  // direction to hide the browser controls, only give the delta to the
  // browser controls when the frame can scroll.
  return delta.Height() < 0 || scroll_offset.Height() < max_scroll.Height();
}

bool ViewportScrollCallback::ScrollBrowserControls(ScrollState& state) {
  // Scroll browser controls.
  if (browser_controls_) {
    if (state.isBeginning())
      browser_controls_->ScrollBegin();

    FloatSize delta(state.deltaX(), state.deltaY());
    ScrollGranularity granularity =
        ScrollGranularity(static_cast<int>(state.deltaGranularity()));
    if (ShouldScrollBrowserControls(delta, granularity)) {
      FloatSize remaining_delta = browser_controls_->ScrollBy(delta);
      FloatSize consumed = delta - remaining_delta;
      state.ConsumeDeltaNative(consumed.Width(), consumed.Height());
      return !consumed.IsZero();
    }
  }

  return false;
}

void ViewportScrollCallback::handleEvent(ScrollState* state) {
  DCHECK(state);
  if (!root_frame_viewport_)
    return;

  bool browser_controls_did_scroll = ScrollBrowserControls(*state);

  ScrollResult result = PerformNativeScroll(*state);

  // We consider browser controls movement to be scrolling.
  result.did_scroll_y |= browser_controls_did_scroll;

  // Handle Overscroll.
  if (overscroll_controller_) {
    FloatPoint position(state->positionX(), state->positionY());
    FloatSize velocity(state->velocityX(), state->velocityY());
    overscroll_controller_->HandleOverscroll(result, position, velocity);
  }
}

void ViewportScrollCallback::SetScroller(ScrollableArea* scroller) {
  DCHECK(scroller);
  root_frame_viewport_->SetLayoutViewport(*scroller);
}

ScrollResult ViewportScrollCallback::PerformNativeScroll(ScrollState& state) {
  DCHECK(root_frame_viewport_);

  FloatSize delta(state.deltaX(), state.deltaY());
  ScrollGranularity granularity =
      ScrollGranularity(static_cast<int>(state.deltaGranularity()));

  ScrollResult result = root_frame_viewport_->UserScroll(granularity, delta);

  // The viewport consumes everything.
  // TODO(bokan): This isn't actually consuming everything but doing so breaks
  // the main thread pull-to-refresh action. crbug.com/607210.
  state.ConsumeDeltaNative(delta.Width() - result.unused_scroll_delta_x,
                           delta.Height() - result.unused_scroll_delta_y);

  return result;
}

}  // namespace blink
