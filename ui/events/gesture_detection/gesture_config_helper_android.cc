// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/gesture_detection/gesture_config_helper.h"

#include "ui/gfx/android/view_configuration.h"
#include "ui/gfx/screen.h"

using gfx::ViewConfiguration;

namespace ui {

// TODO(jdduke): Adopt GestureConfiguration on Android, crbug/339203.

GestureDetector::Config DefaultGestureDetectorConfig() {
  GestureDetector::Config config;

  config.longpress_timeout = base::TimeDelta::FromMilliseconds(
      ViewConfiguration::GetLongPressTimeoutInMs());
  config.showpress_timeout = base::TimeDelta::FromMilliseconds(
      ViewConfiguration::GetTapTimeoutInMs());
  config.double_tap_timeout = base::TimeDelta::FromMilliseconds(
      ViewConfiguration::GetDoubleTapTimeoutInMs());

  config.scaled_touch_slop = ViewConfiguration::GetTouchSlopInPixels();
  config.scaled_double_tap_slop = ViewConfiguration::GetDoubleTapSlopInPixels();
  config.scaled_minimum_fling_velocity =
      ViewConfiguration::GetMinimumFlingVelocityInPixelsPerSecond();
  config.scaled_maximum_fling_velocity =
      ViewConfiguration::GetMaximumFlingVelocityInPixelsPerSecond();

  return config;
}

ScaleGestureDetector::Config DefaultScaleGestureDetectorConfig() {
  ScaleGestureDetector::Config config;

  config.gesture_detector_config = DefaultGestureDetectorConfig();
  config.quick_scale_enabled = true;
  config.min_scaling_touch_major =
      ViewConfiguration::GetMinScalingTouchMajorInPixels();
  config.min_scaling_span = ViewConfiguration::GetMinScalingSpanInPixels();

  return config;
}

SnapScrollController::Config DefaultSnapScrollControllerConfig() {
  SnapScrollController::Config config;

  const gfx::Display& display =
      gfx::Screen::GetNativeScreen()->GetPrimaryDisplay();

  config.screen_width_pixels = display.GetSizeInPixel().width();
  config.screen_height_pixels = display.GetSizeInPixel().height();
  config.device_scale_factor = display.device_scale_factor();

  return config;
}

GestureProvider::Config DefaultGestureProviderConfig() {
  GestureProvider::Config config;
  config.gesture_detector_config = DefaultGestureDetectorConfig();
  config.scale_gesture_detector_config = DefaultScaleGestureDetectorConfig();
  config.snap_scroll_controller_config = DefaultSnapScrollControllerConfig();
  return config;
}

}  // namespace ui
