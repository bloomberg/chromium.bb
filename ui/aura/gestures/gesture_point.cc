// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/gestures/gesture_point.h"

#include "ui/aura/event.h"
#include "ui/base/events.h"

namespace {

// TODO(girard): Make these configurable in sync with this CL
//               http://crbug.com/100773
const double kMaximumTouchDownDurationInSecondsForClick = 0.8;
const double kMinimumTouchDownDurationInSecondsForClick = 0.01;
const double kMaximumSecondsBetweenDoubleClick = 0.7;
const int kMaximumTouchMoveInPixelsForClick = 20;
const float kMinFlickSpeedSquared = 550.f * 550.f;

}  // namespace aura

namespace aura {

GesturePoint::GesturePoint()
    : first_touch_time_(0.0),
      last_touch_time_(0.0),
      last_tap_time_(0.0),
      x_velocity_(0.0),
      y_velocity_(0.0) {
}

void GesturePoint::Reset() {
  first_touch_time_ = last_touch_time_ = 0.0;
  x_velocity_ = y_velocity_ = 0.0;
}

void GesturePoint::UpdateValues(const TouchEvent& event, GestureState state) {
  if (state != GS_NO_GESTURE && event.type() == ui::ET_TOUCH_MOVED) {
    double interval(event.time_stamp().InSecondsF() - last_touch_time_);
    x_velocity_ = (event.x() - last_touch_position_.x()) / interval;
    y_velocity_ = (event.y() - last_touch_position_.y()) / interval;
  }

  last_touch_time_ = event.time_stamp().InSecondsF();
  last_touch_position_ = event.location();

  if (state == GS_NO_GESTURE) {
    first_touch_time_ = last_touch_time_;
    first_touch_position_ = event.location();
    x_velocity_ = 0.0;
    y_velocity_ = 0.0;
  }
}

void GesturePoint::UpdateForTap() {
  // Update the tap-position and time, and reset every other state.
  last_tap_time_ = last_touch_time_;
  last_tap_position_ = last_touch_position_;
  Reset();
}

void GesturePoint::UpdateForScroll() {
  // Update the first-touch position and time so that the scroll-delta and
  // scroll-velocity can be computed correctly for the next scroll gesture
  // event.
  first_touch_position_ = last_touch_position_;
  first_touch_time_ = last_touch_time_;
}

bool GesturePoint::IsInClickWindow(const TouchEvent& event) const {
  return IsInClickTimeWindow() && IsInsideManhattanSquare(event);
}

bool GesturePoint::IsInDoubleClickWindow(const TouchEvent& event) const {
  return IsInSecondClickTimeWindow() &&
         IsSecondClickInsideManhattanSquare(event);
}

bool GesturePoint::IsInScrollWindow(const TouchEvent& event) const {
  return event.type() == ui::ET_TOUCH_MOVED &&
         !IsInsideManhattanSquare(event);
}

bool GesturePoint::IsInFlickWindow(const TouchEvent& event) const {
  return IsOverMinFlickSpeed() && event.type() != ui::ET_TOUCH_CANCELLED;
}

bool GesturePoint::IsInClickTimeWindow() const {
  double duration = last_touch_time_ - first_touch_time_;
  return duration >= kMinimumTouchDownDurationInSecondsForClick &&
         duration < kMaximumTouchDownDurationInSecondsForClick;
}

bool GesturePoint::IsInSecondClickTimeWindow() const {
  double duration =  last_touch_time_ - last_tap_time_;
  return duration < kMaximumSecondsBetweenDoubleClick;
}

bool GesturePoint::IsInsideManhattanSquare(const TouchEvent& event) const {
  int manhattanDistance = abs(event.x() - first_touch_position_.x()) +
                          abs(event.y() - first_touch_position_.y());
  return manhattanDistance < kMaximumTouchMoveInPixelsForClick;
}

bool GesturePoint::IsSecondClickInsideManhattanSquare(
    const TouchEvent& event) const {
  int manhattanDistance = abs(event.x() - last_tap_position_.x()) +
                          abs(event.y() - last_tap_position_.y());
  return manhattanDistance < kMaximumTouchMoveInPixelsForClick;
}

bool GesturePoint::IsOverMinFlickSpeed() const {
  return (x_velocity_ * x_velocity_ + y_velocity_ * y_velocity_) >
          kMinFlickSpeedSquared;
}

}  // namespace aura
