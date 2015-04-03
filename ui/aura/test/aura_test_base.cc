// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/test/aura_test_base.h"

#include "ui/aura/client/window_tree_client.h"
#include "ui/aura/test/aura_test_helper.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window.h"
#include "ui/base/ime/input_method_initializer.h"
#include "ui/compositor/test/context_factories_for_test.h"
#include "ui/events/event_dispatcher.h"
#include "ui/events/event_processor.h"
#include "ui/events/gesture_detection/gesture_configuration.h"

namespace aura {
namespace test {

AuraTestBase::AuraTestBase()
    : setup_called_(false),
      teardown_called_(false) {
}

AuraTestBase::~AuraTestBase() {
  CHECK(setup_called_)
      << "You have overridden SetUp but never called super class's SetUp";
  CHECK(teardown_called_)
      << "You have overridden TearDown but never called super class's TearDown";
}

void AuraTestBase::SetUp() {
  setup_called_ = true;
  testing::Test::SetUp();
  ui::InitializeInputMethodForTesting();
  ui::GestureConfiguration* gesture_config =
      ui::GestureConfiguration::GetInstance();
  // Changing the parameters for gesture recognition shouldn't cause
  // tests to fail, so we use a separate set of parameters for unit
  // testing.
  gesture_config->set_default_radius(0);
  gesture_config->set_fling_max_cancel_to_down_time_in_ms(400);
  gesture_config->set_fling_max_tap_gap_time_in_ms(200);
  gesture_config->set_gesture_begin_end_types_enabled(true);
  gesture_config->set_long_press_time_in_ms(1000);
  gesture_config->set_max_distance_between_taps_for_double_tap(20);
  gesture_config->set_max_distance_for_two_finger_tap_in_pixels(300);
  gesture_config->set_max_fling_velocity(15000);
  gesture_config->set_max_gesture_bounds_length(0);
  gesture_config->set_max_separation_for_gesture_touches_in_pixels(150);
  gesture_config->set_max_swipe_deviation_angle(20);
  gesture_config->set_max_time_between_double_click_in_ms(700);
  gesture_config->set_max_touch_down_duration_for_click_in_ms(800);
  gesture_config->set_max_touch_move_in_pixels_for_click(5);
  gesture_config->set_min_distance_for_pinch_scroll_in_pixels(20);
  gesture_config->set_min_fling_velocity(30.0f);
  gesture_config->set_min_pinch_update_span_delta(0);
  gesture_config->set_min_scaling_span_in_pixels(125);
  gesture_config->set_min_swipe_velocity(10);
  gesture_config->set_scroll_debounce_interval_in_ms(0);
  gesture_config->set_semi_long_press_time_in_ms(400);
  gesture_config->set_show_press_delay_in_ms(5);
  gesture_config->set_swipe_enabled(true);
  gesture_config->set_tab_scrub_activation_delay_in_ms(200);
  gesture_config->set_two_finger_tap_enabled(true);
  gesture_config->set_velocity_tracker_strategy(
      ui::VelocityTracker::Strategy::LSQ2_RESTRICTED);

  // The ContextFactory must exist before any Compositors are created.
  bool enable_pixel_output = false;
  ui::ContextFactory* context_factory =
      ui::InitializeContextFactoryForTests(enable_pixel_output);

  helper_.reset(new AuraTestHelper(&message_loop_));
  helper_->SetUp(context_factory);
}

void AuraTestBase::TearDown() {
  teardown_called_ = true;

  // Flush the message loop because we have pending release tasks
  // and these tasks if un-executed would upset Valgrind.
  RunAllPendingInMessageLoop();

  helper_->TearDown();
  ui::TerminateContextFactoryForTests();
  ui::ShutdownInputMethodForTesting();
  testing::Test::TearDown();
}

Window* AuraTestBase::CreateNormalWindow(int id, Window* parent,
                                         WindowDelegate* delegate) {
  Window* window = new Window(
      delegate ? delegate :
      test::TestWindowDelegate::CreateSelfDestroyingDelegate());
  window->set_id(id);
  window->Init(ui::LAYER_TEXTURED);
  parent->AddChild(window);
  window->SetBounds(gfx::Rect(0, 0, 100, 100));
  window->Show();
  return window;
}

void AuraTestBase::RunAllPendingInMessageLoop() {
  helper_->RunAllPendingInMessageLoop();
}

void AuraTestBase::ParentWindow(Window* window) {
  client::ParentWindowWithContext(window, root_window(), gfx::Rect());
}

bool AuraTestBase::DispatchEventUsingWindowDispatcher(ui::Event* event) {
  ui::EventDispatchDetails details =
      event_processor()->OnEventFromSource(event);
  CHECK(!details.dispatcher_destroyed);
  return event->handled();
}

}  // namespace test
}  // namespace aura
