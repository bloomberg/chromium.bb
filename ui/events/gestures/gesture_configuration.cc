// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/gestures/gesture_configuration.h"

namespace ui {

int GestureConfiguration::default_radius_ = 25;
int GestureConfiguration::fling_max_cancel_to_down_time_in_ms_ = 400;
int GestureConfiguration::fling_max_tap_gap_time_in_ms_ = 200;
float GestureConfiguration::fling_velocity_cap_ = 17000.0f;
int GestureConfiguration::tab_scrub_activation_delay_in_ms_ = 200;
double GestureConfiguration::long_press_time_in_seconds_ = 1.0;
double GestureConfiguration::semi_long_press_time_in_seconds_ = 0.4;
double GestureConfiguration::max_distance_for_two_finger_tap_in_pixels_ = 300;
double GestureConfiguration::max_seconds_between_double_click_ = 0.7;
double
  GestureConfiguration::max_separation_for_gesture_touches_in_pixels_ = 150;
float GestureConfiguration::max_swipe_deviation_angle_ = 20;
double
  GestureConfiguration::max_touch_down_duration_in_seconds_for_click_ = 0.8;
double GestureConfiguration::max_touch_move_in_pixels_for_click_ = 15;
double GestureConfiguration::max_distance_between_taps_for_double_tap_ = 20;
double GestureConfiguration::min_distance_for_pinch_scroll_in_pixels_ = 20;
double GestureConfiguration::min_pinch_update_distance_in_pixels_ = 5;
float GestureConfiguration::min_scroll_velocity_ = 30.0f;
double GestureConfiguration::min_swipe_speed_ = 20;

// If this is too small, we currently can get single finger pinch zoom. See
// crbug.com/357237 for details.
int GestureConfiguration::min_scaling_span_in_pixels_ = 125;
int GestureConfiguration::show_press_delay_in_ms_ = 150;

// TODO(jdduke): Disable and remove entirely when issues with intermittent
// scroll end detection on the Pixel are resolved, crbug.com/353702.
#if defined(OS_CHROMEOS)
int GestureConfiguration::scroll_debounce_interval_in_ms_ = 30;
#else
int GestureConfiguration::scroll_debounce_interval_in_ms_ = 0;
#endif

}  // namespace ui
