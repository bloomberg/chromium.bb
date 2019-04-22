// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/touch_uma.h"

#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/optional.h"
#include "ui/aura/window.h"
#include "ui/base/class_property.h"
#include "ui/events/event.h"

namespace {

struct WindowTouchDetails {
  // Last time-stamp of the last touch-end event.
  base::Optional<base::TimeTicks> last_release_time_;

  // Stores the time of the last touch released on this window (if there was a
  // multi-touch gesture on the window, then this is the release-time of the
  // last touch on the window).
  base::Optional<base::TimeTicks> last_mt_time_;
};

DEFINE_OWNED_UI_CLASS_PROPERTY_KEY(WindowTouchDetails,
                                   kWindowTouchDetails,
                                   NULL)

}  // namespace

DEFINE_UI_CLASS_PROPERTY_TYPE(WindowTouchDetails*)

namespace ash {

namespace {

GestureActionType FindGestureActionType(aura::Window* window,
                                        const ui::GestureEvent& event) {
  if (!window || window->GetRootWindow() == window) {
    if (event.type() == ui::ET_GESTURE_SCROLL_BEGIN)
      return GESTURE_BEZEL_SCROLL;
    if (event.type() == ui::ET_GESTURE_BEGIN)
      return GESTURE_BEZEL_DOWN;
    return GESTURE_UNKNOWN;
  }

  std::string name = window ? window->GetName() : std::string();

  const char kWallpaperView[] = "WallpaperView";
  if (name == kWallpaperView) {
    if (event.type() == ui::ET_GESTURE_SCROLL_BEGIN)
      return GESTURE_DESKTOP_SCROLL;
    if (event.type() == ui::ET_GESTURE_PINCH_BEGIN)
      return GESTURE_DESKTOP_PINCH;
    return GESTURE_UNKNOWN;
  }

  const char kWebPage[] = "RenderWidgetHostViewAura";
  if (name == kWebPage) {
    if (event.type() == ui::ET_GESTURE_PINCH_BEGIN)
      return GESTURE_WEBPAGE_PINCH;
    if (event.type() == ui::ET_GESTURE_SCROLL_BEGIN)
      return GESTURE_WEBPAGE_SCROLL;
    if (event.type() == ui::ET_GESTURE_TAP)
      return GESTURE_WEBPAGE_TAP;
    return GESTURE_UNKNOWN;
  }

  return GESTURE_UNKNOWN;
}

}  // namespace

// static
void TouchUMA::RecordGestureEvent(aura::Window* target,
                                  const ui::GestureEvent& event) {
  GestureActionType action = FindGestureActionType(target, event);
  RecordGestureAction(action);

  if (event.type() == ui::ET_GESTURE_END &&
      event.details().touch_points() == 2) {
    WindowTouchDetails* details = target->GetProperty(kWindowTouchDetails);
    if (!details) {
      LOG(ERROR) << "Window received gesture events without receiving any touch"
                    " events";
      return;
    }
    details->last_mt_time_ = event.time_stamp();
  }
}

// static
void TouchUMA::RecordGestureAction(GestureActionType action) {
  if (action == GESTURE_UNKNOWN || action >= GESTURE_ACTION_COUNT)
    return;
  UMA_HISTOGRAM_ENUMERATION("Ash.GestureTarget", action, GESTURE_ACTION_COUNT);
}

// static
void TouchUMA::RecordTouchEvent(aura::Window* target,
                                const ui::TouchEvent& event) {
  WindowTouchDetails* details = target->GetProperty(kWindowTouchDetails);
  if (!details) {
    details = new WindowTouchDetails;
    target->SetProperty(kWindowTouchDetails, details);
  }

  if (event.type() == ui::ET_TOUCH_PRESSED) {
    base::RecordAction(base::UserMetricsAction("Touchscreen_Down"));

    if (details->last_release_time_) {
      // Measuring the interval between a touch-release and the next
      // touch-start is probably less useful when doing multi-touch (e.g.
      // gestures, or multi-touch friendly apps). So count this only if the user
      // hasn't done any multi-touch during the last 30 seconds.
      base::TimeDelta diff = event.time_stamp() -
                             details->last_mt_time_.value_or(base::TimeTicks());
      if (diff.InSeconds() > 30) {
        base::TimeDelta gap = event.time_stamp() - *details->last_release_time_;
        UMA_HISTOGRAM_COUNTS_10000("Ash.TouchStartAfterEnd",
                                   gap.InMilliseconds());
      }
    }
  } else if (event.type() == ui::ET_TOUCH_RELEASED) {
    details->last_release_time_ = event.time_stamp();
  }
}

}  // namespace ash
