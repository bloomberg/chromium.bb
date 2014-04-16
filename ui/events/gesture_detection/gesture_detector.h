// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_GESTURE_DETECTION_GESTURE_DETECTOR_H_
#define UI_EVENTS_GESTURE_DETECTION_GESTURE_DETECTOR_H_

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "ui/events/gesture_detection/gesture_detection_export.h"
#include "ui/events/gesture_detection/velocity_tracker_state.h"

namespace ui {

class MotionEvent;

// Port of GestureDetector.java from Android
// * platform/frameworks/base/core/java/android/view/GestureDetector.java
// * Change-Id: Ib470735ec929b0b358fca4597e92dc81084e675f
// * Please update the Change-Id as upstream Android changes are pulled.
class GestureDetector {
 public:
  struct GESTURE_DETECTION_EXPORT Config {
    Config();
    ~Config();

    base::TimeDelta longpress_timeout;
    base::TimeDelta showpress_timeout;
    base::TimeDelta double_tap_timeout;

    // Distance a touch can wander before a scroll will occur (in dips).
    float touch_slop;

    // Distance the first touch can wander before it is no longer considered a
    // double tap (in dips).
    float double_tap_slop;

    // Minimum velocity to initiate a fling (in dips/second).
    float minimum_fling_velocity;

    // Maximum velocity of an initiated fling (in dips/second).
    float maximum_fling_velocity;
  };

  class GestureListener {
   public:
    virtual ~GestureListener() {}
    virtual bool OnDown(const MotionEvent& e) = 0;
    virtual void OnShowPress(const MotionEvent& e) = 0;
    virtual bool OnSingleTapUp(const MotionEvent& e) = 0;
    virtual bool OnLongPress(const MotionEvent& e) = 0;
    virtual bool OnScroll(const MotionEvent& e1, const MotionEvent& e2,
                          float distance_x, float distance_y) = 0;
    virtual bool OnFling(const MotionEvent& e1, const MotionEvent& e2,
                         float velocity_x, float velocity_y) = 0;
  };

  class DoubleTapListener {
   public:
    virtual ~DoubleTapListener() {}
    virtual bool OnSingleTapConfirmed(const MotionEvent& e) = 0;
    virtual bool OnDoubleTap(const MotionEvent& e) = 0;
    virtual bool OnDoubleTapEvent(const MotionEvent& e) = 0;
  };

  // A convenience class to extend when you only want to listen for a subset
  // of all the gestures. This implements all methods in the
  // |GestureListener| and |DoubleTapListener| but does
  // nothing and returns false for all applicable methods.
  class SimpleGestureListener : public GestureListener,
                                public DoubleTapListener {
   public:
    // GestureListener implementation.
    virtual bool OnDown(const MotionEvent& e) OVERRIDE;
    virtual void OnShowPress(const MotionEvent& e) OVERRIDE;
    virtual bool OnSingleTapUp(const MotionEvent& e) OVERRIDE;
    virtual bool OnLongPress(const MotionEvent& e) OVERRIDE;
    virtual bool OnScroll(const MotionEvent& e1, const MotionEvent& e2,
                          float distance_x, float distance_y) OVERRIDE;
    virtual bool OnFling(const MotionEvent& e1, const MotionEvent& e2,
                         float velocity_x, float velocity_y) OVERRIDE;

    // DoubleTapListener implementation.
    virtual bool OnSingleTapConfirmed(const MotionEvent& e) OVERRIDE;
    virtual bool OnDoubleTap(const MotionEvent& e) OVERRIDE;
    virtual bool OnDoubleTapEvent(const MotionEvent& e) OVERRIDE;
  };

  GestureDetector(const Config& config,
                  GestureListener* listener,
                  DoubleTapListener* optional_double_tap_listener);
  ~GestureDetector();

  bool OnTouchEvent(const MotionEvent& ev);

  // Setting a valid |double_tap_listener| will enable double-tap detection,
  // wherein calls to |OnSimpleTapConfirmed| are delayed by the tap timeout.
  // Note: The listener must never be changed while |is_double_tapping| is true.
  void SetDoubleTapListener(DoubleTapListener* double_tap_listener);

  bool has_doubletap_listener() const { return double_tap_listener_ != NULL; }

  bool is_double_tapping() const { return is_double_tapping_; }

  void set_is_longpress_enabled(bool is_longpress_enabled) {
    is_longpress_enabled_ = is_longpress_enabled;
  }

  bool is_longpress_enabled() const { return is_longpress_enabled_; }

 private:
  void Init(const Config& config);
  void OnShowPressTimeout();
  void OnLongPressTimeout();
  void OnTapTimeout();
  void Cancel();
  void CancelTaps();
  bool IsConsideredDoubleTap(const MotionEvent& first_down,
                             const MotionEvent& first_up,
                             const MotionEvent& second_down) const;

  class TimeoutGestureHandler;
  scoped_ptr<TimeoutGestureHandler> timeout_handler_;
  GestureListener* const listener_;
  DoubleTapListener* double_tap_listener_;

  float touch_slop_square_;
  float double_tap_touch_slop_square_;
  float double_tap_slop_square_;
  float min_fling_velocity_;
  float max_fling_velocity_;
  base::TimeDelta double_tap_timeout_;

  bool still_down_;
  bool defer_confirm_single_tap_;
  bool in_longpress_;
  bool always_in_tap_region_;
  bool always_in_bigger_tap_region_;

  scoped_ptr<MotionEvent> current_down_event_;
  scoped_ptr<MotionEvent> previous_up_event_;

  // True when the user is still touching for the second tap (down, move, and
  // up events). Can only be true if there is a double tap listener attached.
  bool is_double_tapping_;

  float last_focus_x_;
  float last_focus_y_;
  float down_focus_x_;
  float down_focus_y_;

  bool is_longpress_enabled_;

  // Determines speed during touch scrolling.
  VelocityTrackerState velocity_tracker_;

  DISALLOW_COPY_AND_ASSIGN(GestureDetector);
};

}  // namespace ui

#endif  // UI_EVENTS_GESTURE_DETECTION_GESTURE_DETECTOR_H_
