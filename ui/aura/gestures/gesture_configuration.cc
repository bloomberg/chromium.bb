// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/gestures/gesture_configuration.h"

namespace aura {

double
  GestureConfiguration::max_touch_down_duration_in_seconds_for_click_ = 0.8;
double
  GestureConfiguration::min_touch_down_duration_in_seconds_for_click_ = 0.01;
double GestureConfiguration::max_seconds_between_double_click_ = 0.7;
double GestureConfiguration::max_touch_move_in_pixels_for_click_ = 20;
double GestureConfiguration::min_flick_speed_squared_ = 550.f * 550.f;
double GestureConfiguration::minimum_pinch_update_distance_in_pixels_ = 5;
double GestureConfiguration::minimum_distance_for_pinch_scroll_in_pixels_ = 20;

}  // namespace aura
