// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/pagination_controller.h"

#include "base/metrics/histogram_macros.h"
#include "ui/app_list/app_list_constants.h"
#include "ui/app_list/pagination_model.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d.h"

namespace app_list {

namespace {

// Constants for dealing with scroll events.
const int kMinScrollToSwitchPage = 20;
const int kMinHorizVelocityToSwitchPage = 800;

const double kFinishTransitionThreshold = 0.33;

}  // namespace

PaginationController::PaginationController(PaginationModel* model,
                                           ScrollAxis scroll_axis)
    : pagination_model_(model), scroll_axis_(scroll_axis) {}

bool PaginationController::OnScroll(const gfx::Vector2d& offset,
                                    ui::EventType type) {
  int offset_magnitude;
  if (scroll_axis_ == SCROLL_AXIS_HORIZONTAL) {
    // If the view scrolls horizontally, both horizontal and vertical scroll
    // events are valid (since most mouse wheels only have vertical scrolling).
    offset_magnitude =
        abs(offset.x()) > abs(offset.y()) ? offset.x() : offset.y();
  } else {
    // If the view scrolls vertically, only vertical scroll events are valid.
    offset_magnitude = offset.y();
  }

  // Do not scroll on very small events.
  // TODO(calamity): This should only apply on touchpad scroll but touchpad
  // events are coming in as mousewheel events. See https://crbug.com/594264.
  if (abs(offset_magnitude) > kMinScrollToSwitchPage &&
      !pagination_model_->has_transition()) {
    const int delta = offset_magnitude > 0 ? -1 : 1;
    if (pagination_model_->IsValidPageRelative(delta)) {
      UMA_HISTOGRAM_ENUMERATION(kAppListPageSwitcherSourceHistogram,
                                type == ui::EventType::ET_MOUSEWHEEL
                                    ? kMouseWheelScroll
                                    : kMousePadScroll,
                                kMaxAppListPageSwitcherSource);
    }
    pagination_model_->SelectPageRelative(delta, true);
    return true;
  }

  return false;
}

bool PaginationController::OnGestureEvent(const ui::GestureEvent& event,
                                          const gfx::Rect& bounds) {
  const ui::GestureEventDetails& details = event.details();
  switch (event.type()) {
    case ui::ET_GESTURE_SCROLL_BEGIN:
      // Reset |drag_moved_app_grid_| to give the AppListView a chance to
      // consume this scroll sequence.
      drag_moved_app_grid_ = false;
      pagination_model_->StartScroll();
      return true;
    case ui::ET_GESTURE_SCROLL_UPDATE: {
      float scroll = scroll_axis_ == SCROLL_AXIS_HORIZONTAL
                         ? details.scroll_x()
                         : details.scroll_y();
      int width = scroll_axis_ == SCROLL_AXIS_HORIZONTAL ? bounds.width()
                                                         : bounds.height();
      // scroll > 0 means moving contents right or down. That is, transitioning
      // to the previous page.
      const int delta = scroll < 0 ? 1 : -1;
      // If the scroll is not going to animate the app grid, return false early
      // and let the AppsGridView redirect the scroll sequence to the
      // AppListView.
      // TODO(759179): find a better way to handle this interaction.
      if (!pagination_model_->IsValidPageRelative(delta) && delta < 0 &&
          !drag_moved_app_grid_ && !pagination_model_->transition().progress) {
        return false;
      }
      pagination_model_->UpdateScroll(scroll / width);
      // If a scroll sequence animates the app grid, we do not want the
      // AppListView to be able to consume the scroll sequence.
      drag_moved_app_grid_ = true;
      return true;
    }
    case ui::ET_GESTURE_SCROLL_END: {
      const bool cancel_transition =
          pagination_model_->transition().progress < kFinishTransitionThreshold;
      pagination_model_->EndScroll(cancel_transition);
      if (!cancel_transition) {
        UMA_HISTOGRAM_ENUMERATION(kAppListPageSwitcherSourceHistogram,
                                  kSwipeAppGrid, kMaxAppListPageSwitcherSource);
      }
      return true;
    }
    case ui::ET_SCROLL_FLING_START: {
      float velocity = scroll_axis_ == SCROLL_AXIS_HORIZONTAL
                           ? details.velocity_x()
                           : details.velocity_y();
      pagination_model_->EndScroll(true);

      if (fabs(velocity) > kMinHorizVelocityToSwitchPage) {
        const int delta = velocity < 0 ? 1 : -1;
        if (pagination_model_->IsValidPageRelative(delta)) {
          UMA_HISTOGRAM_ENUMERATION(kAppListPageSwitcherSourceHistogram,
                                    kFlingAppGrid,
                                    kMaxAppListPageSwitcherSource);
        }
        pagination_model_->SelectPageRelative(delta, true);
      }
      return true;
    }
    default:
      return false;
  }
}

}  // namespace app_list
