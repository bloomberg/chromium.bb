// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/test/aura_test_base.h"

#include "ui/aura/env.h"
#include "ui/aura/gestures/gesture_configuration.h"
#include "ui/aura/monitor_manager.h"
#include "ui/aura/single_monitor_manager.h"
#include "ui/aura/root_window.h"
#include "ui/aura/test/test_screen.h"
#include "ui/aura/test/test_stacking_client.h"
#include "ui/aura/ui_controls_aura.h"
#include "ui/ui_controls/ui_controls.h"

namespace aura {
namespace test {

AuraTestBase::AuraTestBase() {
}

AuraTestBase::~AuraTestBase() {
}

void AuraTestBase::SetUp() {
  testing::Test::SetUp();

  // Changing the parameters for gesture recognition shouldn't cause
  // tests to fail, so we use a separate set of parameters for unit
  // testing.
  GestureConfiguration::set_long_press_time_in_seconds(0.5);
  GestureConfiguration::set_max_seconds_between_double_click(0.7);
  GestureConfiguration::set_max_separation_for_gesture_touches_in_pixels(150);
  GestureConfiguration::set_max_touch_down_duration_in_seconds_for_click(0.8);
  GestureConfiguration::set_max_touch_move_in_pixels_for_click(20);
  GestureConfiguration::set_min_distance_for_pinch_scroll_in_pixels(20);
  GestureConfiguration::set_min_flick_speed_squared(550.f * 550.f);
  GestureConfiguration::set_min_pinch_update_distance_in_pixels(5);
  GestureConfiguration::set_min_rail_break_velocity(200);
  GestureConfiguration::set_min_scroll_delta_squared(5 * 5);
  GestureConfiguration::set_min_touch_down_duration_in_seconds_for_click(0.01);
  GestureConfiguration::set_points_buffered_for_velocity(10);
  GestureConfiguration::set_rail_break_proportion(15);
  GestureConfiguration::set_rail_start_proportion(2);

  root_window_.reset(Env::GetInstance()->monitor_manager()->
                     CreateRootWindowForPrimaryMonitor());
  gfx::Screen::SetInstance(new aura::TestScreen(root_window_.get()));
  ui_controls::InstallUIControlsAura(CreateUIControlsAura(root_window_.get()));
  helper_.InitRootWindow(root_window());
  helper_.SetUp();
  stacking_client_.reset(new TestStackingClient(root_window()));
}

void AuraTestBase::TearDown() {
  // Flush the message loop because we have pending release tasks
  // and these tasks if un-executed would upset Valgrind.
  RunAllPendingInMessageLoop();

  stacking_client_.reset();
  helper_.TearDown();
  root_window_.reset();
  testing::Test::TearDown();
}

void AuraTestBase::RunAllPendingInMessageLoop() {
  helper_.RunAllPendingInMessageLoop(root_window());
}

}  // namespace test
}  // namespace aura
