// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/gesture_detection/gesture_config_helper.h"

#include "ui/events/gestures/gesture_configuration.h"
#include "ui/gfx/screen.h"

namespace ui {
namespace {

GestureDetector::Config DefaultGestureDetectorConfig() {
  GestureDetector::Config config;

  config.longpress_timeout = base::TimeDelta::FromMilliseconds(
      GestureConfiguration::long_press_time_in_seconds() * 1000.);
  config.showpress_timeout = base::TimeDelta::FromMilliseconds(
      GestureConfiguration::show_press_delay_in_ms());
  config.double_tap_timeout = base::TimeDelta::FromMilliseconds(
      GestureConfiguration::semi_long_press_time_in_seconds() * 1000.);
  config.touch_slop =
      GestureConfiguration::max_touch_move_in_pixels_for_click();
  config.double_tap_slop =
      GestureConfiguration::max_distance_between_taps_for_double_tap();
  config.minimum_fling_velocity =
      GestureConfiguration::min_scroll_velocity();
  config.maximum_fling_velocity =
      GestureConfiguration::fling_velocity_cap();

  return config;
}

ScaleGestureDetector::Config DefaultScaleGestureDetectorConfig() {
  ScaleGestureDetector::Config config;

  config.gesture_detector_config = DefaultGestureDetectorConfig();
  config.min_scaling_touch_major = GestureConfiguration::default_radius() * 2;
  config.min_scaling_span =
      GestureConfiguration::min_distance_for_pinch_scroll_in_pixels();
  return config;
}

}  // namespace

GestureProvider::Config DefaultGestureProviderConfig() {
  GestureProvider::Config config;
  config.display = gfx::Screen::GetNativeScreen()->GetPrimaryDisplay();
  config.gesture_detector_config = DefaultGestureDetectorConfig();
  config.scale_gesture_detector_config = DefaultScaleGestureDetectorConfig();
  config.gesture_begin_end_types_enabled = true;
  return config;
}

}  // namespace ui
