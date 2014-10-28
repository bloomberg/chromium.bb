// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/gesture_detection/gesture_provider_config_helper.h"

#include "ui/events/gesture_detection/gesture_configuration.h"
#include "ui/gfx/screen.h"

namespace ui {
namespace {

GestureDetector::Config DefaultGestureDetectorConfig(
    GestureConfiguration* gesture_config) {
  GestureDetector::Config config;
  config.longpress_timeout = base::TimeDelta::FromMilliseconds(
      gesture_config->long_press_time_in_ms());
  config.showpress_timeout = base::TimeDelta::FromMilliseconds(
      gesture_config->show_press_delay_in_ms());
  config.double_tap_timeout = base::TimeDelta::FromMilliseconds(
      gesture_config->double_tap_timeout_in_ms());
  config.touch_slop = gesture_config->max_touch_move_in_pixels_for_click();
  config.double_tap_slop =
      gesture_config->max_distance_between_taps_for_double_tap();
  config.minimum_fling_velocity = gesture_config->min_fling_velocity();
  config.maximum_fling_velocity = gesture_config->max_fling_velocity();
  config.swipe_enabled = gesture_config->swipe_enabled();
  config.minimum_swipe_velocity = gesture_config->min_swipe_velocity();
  config.maximum_swipe_deviation_angle =
      gesture_config->max_swipe_deviation_angle();
  config.two_finger_tap_enabled = gesture_config->two_finger_tap_enabled();
  config.two_finger_tap_max_separation =
      gesture_config->max_distance_for_two_finger_tap_in_pixels();
  config.two_finger_tap_timeout = base::TimeDelta::FromMilliseconds(
      gesture_config->max_touch_down_duration_for_click_in_ms());
  config.velocity_tracker_strategy =
      gesture_config->velocity_tracker_strategy();
  return config;
}

ScaleGestureDetector::Config DefaultScaleGestureDetectorConfig(
    GestureConfiguration* gesture_config) {
  ScaleGestureDetector::Config config;
  config.span_slop = gesture_config->span_slop();
  config.min_scaling_touch_major = gesture_config->min_scaling_touch_major();
  config.min_scaling_span = gesture_config->min_scaling_span_in_pixels();
  config.min_pinch_update_span_delta =
      gesture_config->min_pinch_update_span_delta();
  return config;
}

}  // namespace

GestureProvider::Config DefaultGestureProviderConfig() {
  GestureConfiguration* gesture_config = GestureConfiguration::GetInstance();
  GestureProvider::Config config;
  gfx::Screen* screen = gfx::Screen::GetScreenByType(gfx::SCREEN_TYPE_NATIVE);
  // |screen| is sometimes NULL during tests.
  if (screen)
    config.display = screen->GetPrimaryDisplay();
  config.gesture_detector_config = DefaultGestureDetectorConfig(gesture_config);
  config.scale_gesture_detector_config =
      DefaultScaleGestureDetectorConfig(gesture_config);
  config.gesture_begin_end_types_enabled =
      gesture_config->gesture_begin_end_types_enabled();
  config.min_gesture_bounds_length =
      gesture_config->min_gesture_bounds_length();
  config.max_gesture_bounds_length =
      gesture_config->max_gesture_bounds_length();
  return config;
}

}  // namespace ui
