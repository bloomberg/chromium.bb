// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_GESTURES_GESTURE_CONFIGURATION_H_
#define UI_AURA_GESTURES_GESTURE_CONFIGURATION_H_
#pragma once

#include "base/basictypes.h"
#include "ui/aura/aura_export.h"

namespace aura {

// TODO: Expand this design to support multiple OS configuration
// approaches (windows, chrome, others).  This would turn into an
// abstract base class.

class AURA_EXPORT GestureConfiguration {
 public:
  static double max_touch_down_duration_in_seconds_for_click() {
    return max_touch_down_duration_in_seconds_for_click_;
  }
  static void set_max_touch_down_duration_in_seconds_for_click(double val) {
    max_touch_down_duration_in_seconds_for_click_ = val;
  }
  static double min_touch_down_duration_in_seconds_for_click() {
    return min_touch_down_duration_in_seconds_for_click_;
  }
  static void set_min_touch_down_duration_in_seconds_for_click(double val) {
    min_touch_down_duration_in_seconds_for_click_ = val;
  }
  static double max_seconds_between_double_click() {
    return max_seconds_between_double_click_;
  }
  static void set_max_seconds_between_double_click(double val) {
    max_seconds_between_double_click_ = val;
  }
  static double max_touch_move_in_pixels_for_click() {
    return max_touch_move_in_pixels_for_click_;
  }
  static void set_max_touch_move_in_pixels_for_click(double val) {
    max_touch_move_in_pixels_for_click_ = val;
  }
  static double min_flick_speed_squared() {
    return min_flick_speed_squared_;
  }
  static void set_min_flick_speed_squared(double val) {
    min_flick_speed_squared_ = val;
  }
  static double minimum_pinch_update_distance_in_pixels() {
    return minimum_pinch_update_distance_in_pixels_;
  }
  static void set_minimum_pinch_update_distance_in_pixels(double val) {
    minimum_pinch_update_distance_in_pixels_ = val;
  }
  static double minimum_distance_for_pinch_scroll_in_pixels() {
    return minimum_distance_for_pinch_scroll_in_pixels_;
  }
  static void set_minimum_distance_for_pinch_scroll_in_pixels(double val) {
    minimum_distance_for_pinch_scroll_in_pixels_ = val;
  }
 private:
  static double max_touch_down_duration_in_seconds_for_click_;
  static double min_touch_down_duration_in_seconds_for_click_;
  static double max_seconds_between_double_click_;
  static double max_touch_move_in_pixels_for_click_;
  static double min_flick_speed_squared_;
  static double minimum_pinch_update_distance_in_pixels_;
  static double minimum_distance_for_pinch_scroll_in_pixels_;

  DISALLOW_COPY_AND_ASSIGN(GestureConfiguration);
};

}  // namespace aura

#endif  // UI_AURA_GESTURES_GESTURE_CONFIGURATION_H_
