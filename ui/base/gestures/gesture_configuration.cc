// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/gestures/gesture_configuration.h"

namespace ui {

// Ordered alphabetically ignoring underscores, to align with the
// associated list of prefs in gesture_prefs_aura.cc.
int GestureConfiguration::default_radius_ = 15;
double GestureConfiguration::long_press_time_in_seconds_ = 0.5;
int GestureConfiguration::max_radius_ = 100;
double GestureConfiguration::max_seconds_between_double_click_ = 0.7;
double
  GestureConfiguration::max_separation_for_gesture_touches_in_pixels_ = 150;
double GestureConfiguration::max_swipe_deviation_ratio_ = 3;
double
  GestureConfiguration::max_touch_down_duration_in_seconds_for_click_ = 0.8;
double GestureConfiguration::max_touch_move_in_pixels_for_click_ = 20;
double GestureConfiguration::min_distance_for_pinch_scroll_in_pixels_ = 20;
double GestureConfiguration::min_flick_speed_squared_ = 550.f * 550.f;
double GestureConfiguration::min_pinch_update_distance_in_pixels_ = 5;
double GestureConfiguration::min_rail_break_velocity_ = 200;
double GestureConfiguration::min_scroll_delta_squared_ = 5 * 5;
double GestureConfiguration::min_swipe_speed_ = 20;
double
  GestureConfiguration::min_touch_down_duration_in_seconds_for_click_ = 0.01;

// The number of points used in the linear regression which determines
// touch velocity. If fewer than this number of points have been seen,
// velocity is reported as 0.
int GestureConfiguration::points_buffered_for_velocity_ = 3;
double GestureConfiguration::rail_break_proportion_ = 15;
double GestureConfiguration::rail_start_proportion_ = 2;

}  // namespace ui
