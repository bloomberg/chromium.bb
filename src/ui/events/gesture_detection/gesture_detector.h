// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_GESTURE_DETECTION_GESTURE_DETECTOR_H_
#define UI_EVENTS_GESTURE_DETECTION_GESTURE_DETECTOR_H_

#include <memory>

#include "base/logging.h"
#include "base/macros.h"
#include "ui/events/gesture_detection/gesture_detection_export.h"
#include "ui/events/gesture_detection/velocity_tracker_state.h"

namespace ui {

class DoubleTapListener;
class GestureListener;
class MotionEvent;

// Port of GestureDetector.java from Android
// * platform/frameworks/base/core/java/android/view/GestureDetector.java
// * Change-Id: Ib470735ec929b0b358fca4597e92dc81084e675f
// * Please update the Change-Id as upstream Android changes are pulled.
class GESTURE_DETECTION_EXPORT GestureDetector {
 public:
  struct GESTURE_DETECTION_EXPORT Config {
    Config();
    Config(const Config& other);
    ~Config();

    base::TimeDelta longpress_timeout;
    base::TimeDelta showpress_timeout;
    base::TimeDelta double_tap_timeout;

    // The minimum duration between the first tap's up event and the second
    // tap's down event for an interaction to be considered a double-tap.
    base::TimeDelta double_tap_min_time;

    // Distance a touch can wander before a scroll will occur (in dips).
    float touch_slop;

    // Distance the first touch can wander before it is no longer considered a
    // double tap (in dips).
    float double_tap_slop;

    // Minimum velocity to initiate a fling (in dips/second).
    float minimum_fling_velocity;

    // Maximum velocity of an initiated fling (in dips/second).
    float maximum_fling_velocity;

    // Whether |OnSwipe| should be called after a secondary touch is released
    // while a logical swipe gesture is active. Defaults to false.
    bool swipe_enabled;

    // Minimum velocity to initiate a swipe (in dips/second).
    float minimum_swipe_velocity;

    // Maximum angle of the swipe from its dominant component axis, between
    // (0, 45] degrees. The closer this is to 0, the closer the dominant
    // direction of the swipe must be to up, down left or right.
    float maximum_swipe_deviation_angle;

    // Whether |OnTwoFingerTap| should be called for two finger tap
    // gestures. Defaults to false.
    bool two_finger_tap_enabled;

    // Maximum distance between pointers for a two finger tap.
    float two_finger_tap_max_separation;

    // Maximum time the second pointer can be active for a two finger tap.
    base::TimeDelta two_finger_tap_timeout;

    // Single tap count repetition length. Defaults to 1 (no repetition count).
    // Note that when double-tap detection is enabled, the single tap repeat
    // count will always be 1.
    int single_tap_repeat_interval;

    // Whether a longpress should be generated immediately when a stylus button
    // is pressed, given that the longpress timeout is still active.
    bool stylus_button_accelerated_longpress_enabled;

    VelocityTracker::Strategy velocity_tracker_strategy;
  };

  GestureDetector(const Config& config,
                  GestureListener* listener,
                  DoubleTapListener* optional_double_tap_listener);
  ~GestureDetector();

  bool OnTouchEvent(const MotionEvent& ev, bool should_process_double_tap);

  // Setting a valid |double_tap_listener| will enable double-tap detection,
  // wherein calls to |OnSimpleTapConfirmed| are delayed by the tap timeout.
  // Note: The listener must never be changed while |is_double_tapping| is true.
  void SetDoubleTapListener(DoubleTapListener* double_tap_listener);

  bool has_doubletap_listener() const { return double_tap_listener_ != NULL; }

  bool is_double_tapping() const { return is_double_tapping_; }

  void set_longpress_enabled(bool enabled) { longpress_enabled_ = enabled; }
  void set_showpress_enabled(bool enabled) { showpress_enabled_ = enabled; }

  // Returns the event storing the initial position of the pointer with given
  // pointer ID. This returns nullptr if the source event isn't
  // current_down_event_ or secondary_pointer_down_event_.
  const MotionEvent* GetSourcePointerDownEvent(
      const MotionEvent& current_down_event,
      const MotionEvent* secondary_pointer_down_event,
      const int pointer_id);

 private:
  void Init(const Config& config);
  void OnShowPressTimeout();
  void OnLongPressTimeout();
  void OnTapTimeout();
  void OnStylusButtonPress(const MotionEvent& ev);
  void Cancel();
  void CancelTaps();
  bool IsRepeatedTap(const MotionEvent& first_down,
                     const MotionEvent& first_up,
                     const MotionEvent& second_down,
                     bool should_process_double_tap) const;
  bool HandleSwipeIfNeeded(const MotionEvent& up, float vx, float vy);
  bool IsWithinTouchSlop(const MotionEvent& ev);

  class TimeoutGestureHandler;
  std::unique_ptr<TimeoutGestureHandler> timeout_handler_;
  GestureListener* const listener_;
  DoubleTapListener* double_tap_listener_;

  float touch_slop_square_;
  float double_tap_touch_slop_square_;
  float double_tap_slop_square_;
  float two_finger_tap_distance_square_;
  float min_fling_velocity_;
  float max_fling_velocity_;
  float min_swipe_velocity_;
  float min_swipe_direction_component_ratio_;
  base::TimeDelta double_tap_timeout_;
  base::TimeDelta two_finger_tap_timeout_;
  base::TimeDelta double_tap_min_time_;

  bool still_down_;
  bool defer_confirm_single_tap_;
  bool all_pointers_within_slop_regions_;
  bool always_in_bigger_tap_region_;
  bool two_finger_tap_allowed_for_gesture_;

  std::unique_ptr<MotionEvent> current_down_event_;
  std::unique_ptr<MotionEvent> previous_up_event_;
  std::unique_ptr<MotionEvent> secondary_pointer_down_event_;

  // True when the user is still touching for the second tap (down, move, and
  // up events). Can only be true if there is a double tap listener attached.
  bool is_double_tapping_;

  // Whether the current ACTION_DOWN event meets the criteria for being a
  // repeated tap. Note that it will be considered a repeated tap only if the
  // corresponding ACTION_UP yields a valid tap and double-tap detection is
  // disabled.
  bool is_down_candidate_for_repeated_single_tap_;

  // Stores the maximum number of pointers that have been down simultaneously
  // during the current touch sequence.
  int maximum_pointer_count_;

  // The number of repeated taps in the current sequence, i.e., for the initial
  // tap this is 0, for the first *repeated* tap 1, etc...
  int current_single_tap_repeat_count_;
  int single_tap_repeat_interval_;

  float last_focus_x_;
  float last_focus_y_;
  float down_focus_x_;
  float down_focus_y_;

  bool stylus_button_accelerated_longpress_enabled_;
  bool longpress_enabled_;
  bool showpress_enabled_;
  bool swipe_enabled_;
  bool two_finger_tap_enabled_;

  // Determines speed during touch scrolling.
  VelocityTrackerState velocity_tracker_;

  DISALLOW_COPY_AND_ASSIGN(GestureDetector);
};

}  // namespace ui

#endif  // UI_EVENTS_GESTURE_DETECTION_GESTURE_DETECTOR_H_
