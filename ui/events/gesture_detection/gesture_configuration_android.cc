// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/gesture_detection/gesture_configuration.h"

#include "ui/gfx/android/view_configuration.h"
#include "ui/gfx/screen.h"

using gfx::ViewConfiguration;

namespace ui {
namespace {
// This was the minimum tap/press size used on Android before the new gesture
// detection pipeline.
const float kMinGestureBoundsLengthDips = 24.f;

// This value is somewhat arbitrary, but provides a reasonable maximum
// approximating a large thumb depression.
const float kMaxGestureBoundsLengthDips = kMinGestureBoundsLengthDips * 4.f;

class GestureConfigurationAndroid : public GestureConfiguration {
 public:
  ~GestureConfigurationAndroid() override {
  }

  static GestureConfigurationAndroid* GetInstance() {
    return Singleton<GestureConfigurationAndroid>::get();
  }

 private:
  GestureConfigurationAndroid() : GestureConfiguration() {
    float raw_pixel_to_dip_ratio = 1.f / gfx::Screen::GetNativeScreen()
                                       ->GetPrimaryDisplay()
                                       .device_scale_factor();
    set_double_tap_enabled(true);
    set_double_tap_timeout_in_ms(ViewConfiguration::GetDoubleTapTimeoutInMs());
    set_gesture_begin_end_types_enabled(false);
    set_long_press_time_in_ms(ViewConfiguration::GetLongPressTimeoutInMs());
    set_max_distance_between_taps_for_double_tap(
        ViewConfiguration::GetDoubleTapSlopInPixels() * raw_pixel_to_dip_ratio);
    set_max_fling_velocity(
        ViewConfiguration::GetMaximumFlingVelocityInPixelsPerSecond() *
        raw_pixel_to_dip_ratio);
    set_max_gesture_bounds_length(kMaxGestureBoundsLengthDips);
    set_max_touch_move_in_pixels_for_click(
        ViewConfiguration::GetTouchSlopInPixels() * raw_pixel_to_dip_ratio);
    set_min_fling_velocity(
        ViewConfiguration::GetMinimumFlingVelocityInPixelsPerSecond() *
        raw_pixel_to_dip_ratio);
    set_min_gesture_bounds_length(kMinGestureBoundsLengthDips);
    set_min_pinch_update_span_delta(0.f);
    set_min_scaling_span_in_pixels(
        ViewConfiguration::GetMinScalingSpanInPixels() *
        raw_pixel_to_dip_ratio);
    set_min_scaling_touch_major(
        ViewConfiguration::GetMinScalingTouchMajorInPixels() *
        raw_pixel_to_dip_ratio);
    set_show_press_delay_in_ms(ViewConfiguration::GetTapTimeoutInMs());
    set_span_slop(ViewConfiguration::GetTouchSlopInPixels() * 2.f *
                  raw_pixel_to_dip_ratio);
  }

  friend struct DefaultSingletonTraits<GestureConfigurationAndroid>;
  DISALLOW_COPY_AND_ASSIGN(GestureConfigurationAndroid);
};

}  // namespace

// Create a GestureConfigurationAura singleton instance when using Android.
GestureConfiguration* GestureConfiguration::GetInstance() {
  return GestureConfigurationAndroid::GetInstance();
}

}  // namespace ui
