// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/test/aura_test_base.h"

#include "ui/aura/test/aura_test_helper.h"
#include "ui/base/gestures/gesture_configuration.h"
#include "ui/base/ime/text_input_test_support.h"

namespace aura {
namespace test {

AuraTestBase::AuraTestBase() {
}

AuraTestBase::~AuraTestBase() {
}

void AuraTestBase::SetUp() {
  testing::Test::SetUp();
  ui::TextInputTestSupport::Initilaize();

  // Changing the parameters for gesture recognition shouldn't cause
  // tests to fail, so we use a separate set of parameters for unit
  // testing.
  ui::GestureConfiguration::set_long_press_time_in_seconds(1.0);
  ui::GestureConfiguration::set_semi_long_press_time_in_seconds(0.5);
  ui::GestureConfiguration::set_max_distance_for_two_finger_tap_in_pixels(300);
  ui::GestureConfiguration::set_max_seconds_between_double_click(0.7);
  ui::GestureConfiguration::
      set_max_separation_for_gesture_touches_in_pixels(150);
  ui::GestureConfiguration::
      set_max_touch_down_duration_in_seconds_for_click(0.8);
  ui::GestureConfiguration::set_max_touch_move_in_pixels_for_click(20);
  ui::GestureConfiguration::set_min_distance_for_pinch_scroll_in_pixels(20);
  ui::GestureConfiguration::set_min_flick_speed_squared(550.f * 550.f);
  ui::GestureConfiguration::set_min_pinch_update_distance_in_pixels(5);
  ui::GestureConfiguration::set_min_rail_break_velocity(200);
  ui::GestureConfiguration::set_min_scroll_delta_squared(5 * 5);
  ui::GestureConfiguration::
      set_min_touch_down_duration_in_seconds_for_click(0.01);
  ui::GestureConfiguration::set_points_buffered_for_velocity(10);
  ui::GestureConfiguration::set_rail_break_proportion(15);
  ui::GestureConfiguration::set_rail_start_proportion(2);
  helper_.reset(new AuraTestHelper(&message_loop_));
  helper_->SetUp();
}

void AuraTestBase::TearDown() {
  // Flush the message loop because we have pending release tasks
  // and these tasks if un-executed would upset Valgrind.
  RunAllPendingInMessageLoop();

  helper_->TearDown();
  ui::TextInputTestSupport::Shutdown();
  testing::Test::TearDown();
}

void AuraTestBase::RunAllPendingInMessageLoop() {
  helper_->RunAllPendingInMessageLoop();
}

}  // namespace test
}  // namespace aura
