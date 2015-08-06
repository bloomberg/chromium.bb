// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/gesture_detection/gesture_configuration.h"

#include "base/memory/singleton.h"
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
    set_double_tap_enabled(true);
    set_double_tap_timeout_in_ms(ViewConfiguration::GetDoubleTapTimeoutInMs());
    set_gesture_begin_end_types_enabled(false);
    set_long_press_time_in_ms(ViewConfiguration::GetLongPressTimeoutInMs());
    set_max_distance_between_taps_for_double_tap(
        ViewConfiguration::GetDoubleTapSlopInDips());
    set_max_fling_velocity(
        ViewConfiguration::GetMaximumFlingVelocityInDipsPerSecond());
    set_max_gesture_bounds_length(kMaxGestureBoundsLengthDips);
    set_max_touch_move_in_pixels_for_click(
        ViewConfiguration::GetTouchSlopInDips());
    set_min_fling_velocity(
        ViewConfiguration::GetMinimumFlingVelocityInDipsPerSecond());
    set_min_gesture_bounds_length(kMinGestureBoundsLengthDips);
    set_min_pinch_update_span_delta(0.f);
    set_min_scaling_span_in_pixels(
        ViewConfiguration::GetMinScalingSpanInDips());
    set_min_scaling_touch_major(
        ViewConfiguration::GetMinScalingTouchMajorInDips());
    set_show_press_delay_in_ms(ViewConfiguration::GetTapTimeoutInMs());
    set_span_slop(ViewConfiguration::GetTouchSlopInDips() * 2.f);
    set_fling_touchscreen_tap_suppression_enabled(true);
    set_fling_touchpad_tap_suppression_enabled(false);
    set_fling_max_cancel_to_down_time_in_ms(
        ViewConfiguration::GetTapTimeoutInMs());
    set_fling_max_tap_gap_time_in_ms(
        ViewConfiguration::GetLongPressTimeoutInMs());
  }

  friend struct DefaultSingletonTraits<GestureConfigurationAndroid>;
  DISALLOW_COPY_AND_ASSIGN(GestureConfigurationAndroid);
};

}  // namespace

// Create a GestureConfigurationAura singleton instance when using Android.
GestureConfiguration* GestureConfiguration::GetPlatformSpecificInstance() {
  return GestureConfigurationAndroid::GetInstance();
}

}  // namespace ui
