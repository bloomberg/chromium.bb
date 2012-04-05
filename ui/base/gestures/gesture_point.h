// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_GESTURES_GESTURE_POINT_H_
#define UI_BASE_GESTURES_GESTURE_POINT_H_
#pragma once

#include "base/basictypes.h"
#include "ui/base/gestures/velocity_calculator.h"
#include "ui/gfx/point.h"

namespace ui {
class TouchEvent;

// A GesturePoint represents a single touch-point/finger during a gesture
// recognition process.
class GesturePoint {
 public:
  GesturePoint();
  ~GesturePoint();

  // Resets various states.
  void Reset();

  // Updates some states when a Tap gesture has been recognized for this point.
  void UpdateForTap();

  // Updates some states when a Scroll gesture has been recognized for this
  // point.
  void UpdateForScroll();

  // Updates states depending on the event and the gesture-state.
  void UpdateValues(const TouchEvent& event);

  // Responds according to the state of the gesture point (i.e. the point can
  // represent a click or scroll etc.)
  bool IsInClickWindow(const TouchEvent& event) const;
  bool IsInDoubleClickWindow(const TouchEvent& event) const;
  bool IsInScrollWindow(const TouchEvent& event) const;
  bool IsInFlickWindow(const TouchEvent& event);
  bool IsInHorizontalRailWindow() const;
  bool IsInVerticalRailWindow() const;
  bool HasEnoughDataToEstablishRail() const;
  bool BreaksHorizontalRail();
  bool BreaksVerticalRail();
  bool DidScroll(const TouchEvent& event, int distance) const;

  const gfx::Point& first_touch_position() const {
    return first_touch_position_;
  }

  double last_touch_time() const { return last_touch_time_; }
  const gfx::Point& last_touch_position() const { return last_touch_position_; }

  // point_id_ is used to drive GestureSequence::ProcessTouchEventForGesture.
  // point_ids are maintained such that the set of point_ids is always
  // contiguous, from 0 to the number of current touches.
  // A lower point_id indicates that a touch occurred first.
  // A negative point_id indicates that the GesturePoint is not currently
  // associated with a touch.
  void set_point_id(int point_id) { point_id_ = point_id; }
  int point_id() const { return point_id_; }

  void set_touch_id(int touch_id) { touch_id_ = touch_id; }
  int touch_id() const { return touch_id_; }

  bool in_use() const { return point_id_ >= 0; }

  double x_delta() const {
    return last_touch_position_.x() - first_touch_position_.x();
  }

  double y_delta() const {
    return last_touch_position_.y() - first_touch_position_.y();
  }

  float XVelocity() { return velocity_calculator_.XVelocity(); }
  float YVelocity() { return velocity_calculator_.YVelocity(); }

  float Distance(const GesturePoint& point) const;

 private:
  // Various statistical functions to manipulate gestures.
  bool IsInClickTimeWindow() const;
  bool IsInSecondClickTimeWindow() const;
  bool IsInsideManhattanSquare(const TouchEvent& event) const;
  bool IsSecondClickInsideManhattanSquare(const TouchEvent& event) const;
  bool IsOverMinFlickSpeed();

  gfx::Point first_touch_position_;
  double first_touch_time_;
  gfx::Point last_touch_position_;
  double last_touch_time_;

  double last_tap_time_;
  gfx::Point last_tap_position_;

  VelocityCalculator velocity_calculator_;

  int point_id_;
  int touch_id_;
  DISALLOW_COPY_AND_ASSIGN(GesturePoint);
};

}  // namespace ui

#endif  // UI_BASE_GESTURES_GESTURE_POINT_H_
