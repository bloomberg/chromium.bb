// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_GESTURES_GESTURE_POINT_H_
#define UI_AURA_GESTURES_GESTURE_POINT_H_
#pragma once

#include "base/basictypes.h"
#include "ui/gfx/point.h"

namespace aura {
class TouchEvent;

// Gesture state.
enum GestureState {
  GS_NO_GESTURE,
  GS_PENDING_SYNTHETIC_CLICK,
  GS_SCROLL,
};

// A GesturePoint represents a single touch-point/finger during a gesture
// recognition process.
class GesturePoint {
 public:
  GesturePoint();
  virtual ~GesturePoint() {}

  // Resets various states.
  void Reset();

  // Updates some states when a Tap gesture has been recognized for this point.
  void UpdateForTap();

  // Updates some states when a Scroll gesture has been recognized for this
  // point.
  void UpdateForScroll();

  // Updates states depending on the event and the gesture-state.
  void UpdateValues(const TouchEvent& event, GestureState state);

  // Responds according to the state of the gesture point (i.e. the point can
  // represent a click or scroll etc.)
  bool IsInClickWindow(const TouchEvent& event) const;
  bool IsInDoubleClickWindow(const TouchEvent& event) const;
  bool IsInScrollWindow(const TouchEvent& event) const;
  bool IsInFlickWindow(const TouchEvent& event) const;

  const gfx::Point& first_touch_position() const {
    return first_touch_position_;
  }

  double last_touch_time() const { return last_touch_time_; }
  const gfx::Point& last_touch_position() const { return last_touch_position_; }

  double x_delta() const {
    return last_touch_position_.x() - first_touch_position_.x();
  }

  double y_delta() const {
    return last_touch_position_.y() - first_touch_position_.y();
  }

  float x_velocity() const { return x_velocity_; }
  float y_velocity() const { return y_velocity_; }

 private:
  // Various statistical functions to manipulate gestures.
  bool IsInClickTimeWindow() const;
  bool IsInSecondClickTimeWindow() const;
  bool IsInsideManhattanSquare(const TouchEvent& event) const;
  bool IsSecondClickInsideManhattanSquare(const TouchEvent& event) const;
  bool IsOverMinFlickSpeed() const;

  gfx::Point first_touch_position_;
  double first_touch_time_;
  gfx::Point last_touch_position_;
  double last_touch_time_;

  double last_tap_time_;
  gfx::Point last_tap_position_;

  float x_velocity_;
  float y_velocity_;

  DISALLOW_COPY_AND_ASSIGN(GesturePoint);
};

}  // namespace aura

#endif  // UI_AURA_GESTURES_GESTURE_POINT_H_
