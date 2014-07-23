// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/chromeos/touch_exploration_controller.h"

#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/test/event_generator.h"
#include "ui/aura/test/test_cursor_client.h"
#include "ui/aura/window.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/gestures/gesture_provider_aura.h"
#include "ui/events/test/events_test_utils.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface.h"

namespace ui {

namespace {

// Records all mouse, touch, gesture, and key events.
class EventCapturer : public ui::EventHandler {
 public:
  EventCapturer() {}
  virtual ~EventCapturer() {}

  void Reset() {
    events_.clear();
  }

  virtual void OnEvent(ui::Event* event) OVERRIDE {
    if (event->IsMouseEvent()) {
      events_.push_back(
          new ui::MouseEvent(static_cast<ui::MouseEvent&>(*event)));
    } else if (event->IsTouchEvent()) {
      events_.push_back(
          new ui::TouchEvent(static_cast<ui::TouchEvent&>(*event)));
    } else if (event->IsGestureEvent()) {
      events_.push_back(
          new ui::GestureEvent(static_cast<ui::GestureEvent&>(*event)));
    } else if (event->IsKeyEvent()) {
      events_.push_back(new ui::KeyEvent(static_cast<ui::KeyEvent&>(*event)));
    } else {
      return;
    }
    // Stop event propagation so we don't click on random stuff that
    // might break test assumptions.
    event->StopPropagation();
    // If there is a possibility that we're in an infinite loop, we should
    // exit early with a sensible error rather than letting the test time out.
    ASSERT_LT(events_.size(), 100u);
  }

  const ScopedVector<ui::Event>& captured_events() const { return events_; }

 private:
  ScopedVector<ui::Event> events_;

  DISALLOW_COPY_AND_ASSIGN(EventCapturer);
};

int Factorial(int n) {
  if (n <= 0)
    return 0;
  if (n == 1)
    return 1;
  return n * Factorial(n - 1);
}

class MockTouchExplorationControllerDelegate
    : public ui::TouchExplorationControllerDelegate {
 public:
  virtual void PlayVolumeAdjustSound() OVERRIDE {
    ++num_times_adjust_sound_played_;
  }
  virtual void SetOutputLevel(int volume) OVERRIDE {
    volume_changes_.push_back(volume);
  }

  const std::vector<float> VolumeChanges() { return volume_changes_; }
  const size_t NumAdjustSounds() { return num_times_adjust_sound_played_; }

 private:
  std::vector<float> volume_changes_;
  size_t num_times_adjust_sound_played_ = 0;
};

}  // namespace

class TouchExplorationControllerTestApi {
 public:
  TouchExplorationControllerTestApi(
      TouchExplorationController* touch_exploration_controller) {
    touch_exploration_controller_.reset(touch_exploration_controller);
  }

  void CallTapTimerNowForTesting() {
    DCHECK(touch_exploration_controller_->tap_timer_.IsRunning());
    touch_exploration_controller_->tap_timer_.Stop();
    touch_exploration_controller_->OnTapTimerFired();
  }

  void CallTapTimerNowIfRunningForTesting() {
    if (touch_exploration_controller_->tap_timer_.IsRunning()) {
      touch_exploration_controller_->tap_timer_.Stop();
      touch_exploration_controller_->OnTapTimerFired();
    }
  }

  void SetEventHandlerForTesting(
      ui::EventHandler* event_handler_for_testing) {
    touch_exploration_controller_->event_handler_for_testing_ =
        event_handler_for_testing;
  }

  bool IsInNoFingersDownStateForTesting() const {
    return touch_exploration_controller_->state_ ==
           touch_exploration_controller_->NO_FINGERS_DOWN;
  }

  bool IsInGestureInProgressStateForTesting() const {
    return touch_exploration_controller_->state_ ==
           touch_exploration_controller_->GESTURE_IN_PROGRESS;
  }

  bool IsInSlideGestureStateForTesting() const {
    return touch_exploration_controller_->state_ ==
           touch_exploration_controller_->SLIDE_GESTURE;
  }

  gfx::Rect BoundsOfRootWindowInDIPForTesting() const {
    return touch_exploration_controller_->root_window_->GetBoundsInScreen();
  }

  // VLOGs should be suppressed in tests that generate a lot of logs,
  // for example permutations of nine touch events.
  void SuppressVLOGsForTesting(bool suppress) {
    touch_exploration_controller_->VLOG_on_ = !suppress;
  }

  float GetMaxDistanceFromEdge() const {
    return touch_exploration_controller_->kMaxDistanceFromEdge;
  }

  float GetSlopDistanceFromEdge() const {
    return touch_exploration_controller_->kSlopDistanceFromEdge;
  }

 private:
  scoped_ptr<TouchExplorationController> touch_exploration_controller_;

  DISALLOW_COPY_AND_ASSIGN(TouchExplorationControllerTestApi);
};

class TouchExplorationTest : public aura::test::AuraTestBase {
 public:
  TouchExplorationTest() : simulated_clock_(new base::SimpleTestTickClock()) {
    // Tests fail if time is ever 0.
    simulated_clock_->Advance(base::TimeDelta::FromMilliseconds(10));
  }
  virtual ~TouchExplorationTest() {}

  virtual void SetUp() OVERRIDE {
    if (gfx::GetGLImplementation() == gfx::kGLImplementationNone)
      gfx::GLSurface::InitializeOneOffForTests();
    aura::test::AuraTestBase::SetUp();
    cursor_client_.reset(new aura::test::TestCursorClient(root_window()));
    root_window()->AddPreTargetHandler(&event_capturer_);
    generator_.reset(new aura::test::EventGenerator(root_window()));
    // The generator takes ownership of the clock.
    generator_->SetTickClock(scoped_ptr<base::TickClock>(simulated_clock_));
    cursor_client()->ShowCursor();
    cursor_client()->DisableMouseEvents();
  }

  virtual void TearDown() OVERRIDE {
    root_window()->RemovePreTargetHandler(&event_capturer_);
    SwitchTouchExplorationMode(false);
    cursor_client_.reset();
    aura::test::AuraTestBase::TearDown();
  }

 protected:
  aura::client::CursorClient* cursor_client() { return cursor_client_.get(); }

  const ScopedVector<ui::Event>& GetCapturedEvents() {
    return event_capturer_.captured_events();
  }

  std::vector<ui::LocatedEvent*> GetCapturedLocatedEvents() {
    const ScopedVector<ui::Event>& all_events = GetCapturedEvents();
    std::vector<ui::LocatedEvent*> located_events;
    for (size_t i = 0; i < all_events.size(); ++i) {
      if (all_events[i]->IsMouseEvent() ||
          all_events[i]->IsTouchEvent() ||
          all_events[i]->IsGestureEvent()) {
        located_events.push_back(static_cast<ui::LocatedEvent*>(all_events[i]));
      }
    }
    return located_events;
  }

  std::vector<ui::Event*> GetCapturedEventsOfType(int type) {
    const ScopedVector<ui::Event>& all_events = GetCapturedEvents();
    std::vector<ui::Event*> events;
    for (size_t i = 0; i < all_events.size(); ++i) {
      if (type == all_events[i]->type())
        events.push_back(all_events[i]);
    }
    return events;
  }

  std::vector<ui::LocatedEvent*> GetCapturedLocatedEventsOfType(int type) {
    std::vector<ui::LocatedEvent*> located_events = GetCapturedLocatedEvents();
    std::vector<ui::LocatedEvent*> events;
    for (size_t i = 0; i < located_events.size(); ++i) {
      if (type == located_events[i]->type())
        events.push_back(located_events[i]);
    }
    return events;
  }

  void ClearCapturedEvents() {
    event_capturer_.Reset();
  }

  void AdvanceSimulatedTimePastTapDelay() {
    simulated_clock_->Advance(base::TimeDelta::FromMilliseconds(1000));
    touch_exploration_controller_->CallTapTimerNowForTesting();
  }

  void AdvanceSimulatedTimePastPotentialTapDelay() {
    simulated_clock_->Advance(base::TimeDelta::FromMilliseconds(1000));
    touch_exploration_controller_->CallTapTimerNowIfRunningForTesting();
  }

  void SuppressVLOGs(bool suppress) {
    touch_exploration_controller_->SuppressVLOGsForTesting(suppress);
  }

  void SwitchTouchExplorationMode(bool on) {
    if (!on && touch_exploration_controller_.get()) {
      touch_exploration_controller_.reset();
    } else if (on && !touch_exploration_controller_.get()) {
      touch_exploration_controller_.reset(
          new ui::TouchExplorationControllerTestApi(
              new TouchExplorationController(root_window(), &delegate_)));
      touch_exploration_controller_->SetEventHandlerForTesting(
          &event_capturer_);
      cursor_client()->ShowCursor();
      cursor_client()->DisableMouseEvents();
    }
  }

  void EnterTouchExplorationModeAtLocation(gfx::Point tap_location) {
    ui::TouchEvent touch_press(ui::ET_TOUCH_PRESSED, tap_location, 0, Now());
    generator_->Dispatch(&touch_press);
    AdvanceSimulatedTimePastTapDelay();
    EXPECT_TRUE(IsInTouchToMouseMode());
  }

  void EnterTwoToOne(gfx::Point first_touch_location,
                     gfx::Point second_touch_location) {
    SwitchTouchExplorationMode(true);
    ui::TouchEvent first_touch_press(
        ui::ET_TOUCH_PRESSED, first_touch_location, 0, Now());
    generator_->Dispatch(&first_touch_press);
    ui::TouchEvent second_touch_press(
        ui::ET_TOUCH_PRESSED, second_touch_location, 1, Now());
    generator_->Dispatch(&second_touch_press);
  }

  bool IsInTouchToMouseMode() {
    aura::client::CursorClient* cursor_client =
        aura::client::GetCursorClient(root_window());
    return cursor_client &&
           cursor_client->IsMouseEventsEnabled() &&
           !cursor_client->IsCursorVisible();
  }

  bool IsInNoFingersDownState() {
    return touch_exploration_controller_->IsInNoFingersDownStateForTesting();
  }

  bool IsInGestureInProgressState() {
    return touch_exploration_controller_
        ->IsInGestureInProgressStateForTesting();
  }

  bool IsInSlideGestureState() {
    return touch_exploration_controller_->IsInSlideGestureStateForTesting();
  }

  gfx::Rect BoundsOfRootWindowInDIP() {
    return touch_exploration_controller_->BoundsOfRootWindowInDIPForTesting();
  }

  float GetMaxDistanceFromEdge() const{
    return touch_exploration_controller_->GetMaxDistanceFromEdge();
  }

  float GetSlopDistanceFromEdge() const{
    return touch_exploration_controller_->GetSlopDistanceFromEdge();
  }

  base::TimeDelta Now() {
    // This is the same as what EventTimeForNow() does, but here we do it
    // with our simulated clock.
    return base::TimeDelta::FromInternalValue(
        simulated_clock_->NowTicks().ToInternalValue());
  }

  scoped_ptr<aura::test::EventGenerator> generator_;
  ui::GestureDetector::Config gesture_detector_config_;
  // Owned by |generator_|.
  base::SimpleTestTickClock* simulated_clock_;
  MockTouchExplorationControllerDelegate delegate_;

 private:
  EventCapturer event_capturer_;
  scoped_ptr<TouchExplorationControllerTestApi>
      touch_exploration_controller_;
  scoped_ptr<aura::test::TestCursorClient> cursor_client_;

  DISALLOW_COPY_AND_ASSIGN(TouchExplorationTest);
};

// Executes a number of assertions to confirm that |e1| and |e2| are touch
// events and are equal to each other.
void ConfirmEventsAreTouchAndEqual(ui::Event* e1, ui::Event* e2) {
  ASSERT_TRUE(e1->IsTouchEvent());
  ASSERT_TRUE(e2->IsTouchEvent());
  ui::TouchEvent* touch_event1 = static_cast<ui::TouchEvent*>(e1);
  ui::TouchEvent* touch_event2 = static_cast<ui::TouchEvent*>(e2);
  EXPECT_EQ(touch_event1->type(), touch_event2->type());
  EXPECT_EQ(touch_event1->location(), touch_event2->location());
  EXPECT_EQ(touch_event1->touch_id(), touch_event2->touch_id());
  EXPECT_EQ(touch_event1->flags(), touch_event2->flags());
  EXPECT_EQ(touch_event1->time_stamp(), touch_event2->time_stamp());
}

// Executes a number of assertions to confirm that |e1| and |e2| are mouse
// events and are equal to each other.
void ConfirmEventsAreMouseAndEqual(ui::Event* e1, ui::Event* e2) {
  ASSERT_TRUE(e1->IsMouseEvent());
  ASSERT_TRUE(e2->IsMouseEvent());
  ui::MouseEvent* mouse_event1 = static_cast<ui::MouseEvent*>(e1);
  ui::MouseEvent* mouse_event2 = static_cast<ui::MouseEvent*>(e2);
  EXPECT_EQ(mouse_event1->type(), mouse_event2->type());
  EXPECT_EQ(mouse_event1->location(), mouse_event2->location());
  EXPECT_EQ(mouse_event1->root_location(), mouse_event2->root_location());
  EXPECT_EQ(mouse_event1->flags(), mouse_event2->flags());
}

// Executes a number of assertions to confirm that |e1| and |e2| are key events
// and are equal to each other.
void ConfirmEventsAreKeyAndEqual(ui::Event* e1, ui::Event* e2) {
  ASSERT_TRUE(e1->IsKeyEvent());
  ASSERT_TRUE(e2->IsKeyEvent());
  ui::KeyEvent* key_event1 = static_cast<ui::KeyEvent*>(e1);
  ui::KeyEvent* key_event2 = static_cast<ui::KeyEvent*>(e2);
  EXPECT_EQ(key_event1->type(), key_event2->type());
  EXPECT_EQ(key_event1->key_code(), key_event2->key_code());
  EXPECT_EQ(key_event1->code(), key_event2->code());
  EXPECT_EQ(key_event1->flags(), key_event2->flags());
}

#define CONFIRM_EVENTS_ARE_TOUCH_AND_EQUAL(e1, e2) \
  ASSERT_NO_FATAL_FAILURE(ConfirmEventsAreTouchAndEqual(e1, e2))

#define CONFIRM_EVENTS_ARE_MOUSE_AND_EQUAL(e1, e2) \
  ASSERT_NO_FATAL_FAILURE(ConfirmEventsAreMouseAndEqual(e1, e2))

#define CONFIRM_EVENTS_ARE_KEY_AND_EQUAL(e1, e2) \
  ASSERT_NO_FATAL_FAILURE(ConfirmEventsAreKeyAndEqual(e1, e2))

// TODO(mfomitchev): Need to investigate why we don't get mouse enter/exit
// events when running these tests as part of ui_unittests. We do get them when
// the tests are run as part of ash unit tests.

// If a swipe has been successfully completed, then six key events will be
// dispatched that correspond to shift+search+direction
void AssertDirectionalNavigationEvents(const ScopedVector<ui::Event>& events,
                                       ui::KeyboardCode direction) {
  ASSERT_EQ(6U, events.size());
  ui::KeyEvent shift_pressed(
      ui::ET_KEY_PRESSED, ui::VKEY_SHIFT, ui::EF_SHIFT_DOWN, false);
  ui::KeyEvent search_pressed(
      ui::ET_KEY_PRESSED, ui::VKEY_LWIN, ui::EF_SHIFT_DOWN, false);
  ui::KeyEvent direction_pressed(
      ui::ET_KEY_PRESSED, direction, ui::EF_SHIFT_DOWN, false);
  ui::KeyEvent direction_released(
      ui::ET_KEY_RELEASED, direction, ui::EF_SHIFT_DOWN, false);
  ui::KeyEvent search_released(
      ui::ET_KEY_RELEASED, VKEY_LWIN, ui::EF_SHIFT_DOWN, false);
  ui::KeyEvent shift_released(
      ui::ET_KEY_RELEASED, ui::VKEY_SHIFT, ui::EF_NONE, false);
  CONFIRM_EVENTS_ARE_KEY_AND_EQUAL(&shift_pressed, events[0]);
  CONFIRM_EVENTS_ARE_KEY_AND_EQUAL(&search_pressed, events[1]);
  CONFIRM_EVENTS_ARE_KEY_AND_EQUAL(&direction_pressed, events[2]);
  CONFIRM_EVENTS_ARE_KEY_AND_EQUAL(&direction_released, events[3]);
  CONFIRM_EVENTS_ARE_KEY_AND_EQUAL(&search_released, events[4]);
  CONFIRM_EVENTS_ARE_KEY_AND_EQUAL(&shift_released, events[5]);
}

TEST_F(TouchExplorationTest, EntersTouchToMouseModeAfterPressAndDelay) {
  SwitchTouchExplorationMode(true);
  EXPECT_FALSE(IsInTouchToMouseMode());
  generator_->PressTouch();
  AdvanceSimulatedTimePastTapDelay();
  EXPECT_TRUE(IsInTouchToMouseMode());
}

TEST_F(TouchExplorationTest, EntersTouchToMouseModeAfterMoveOutsideSlop) {
  int slop = gesture_detector_config_.touch_slop;
  int half_slop = slop / 2;

  SwitchTouchExplorationMode(true);
  EXPECT_FALSE(IsInTouchToMouseMode());
  generator_->set_current_location(gfx::Point(11, 12));
  generator_->PressTouch();
  generator_->MoveTouch(gfx::Point(11 + half_slop, 12));
  EXPECT_FALSE(IsInTouchToMouseMode());
  generator_->MoveTouch(gfx::Point(11, 12 + half_slop));
  EXPECT_FALSE(IsInTouchToMouseMode());
  AdvanceSimulatedTimePastTapDelay();
  generator_->MoveTouch(gfx::Point(11 + slop + 1, 12));
  EXPECT_TRUE(IsInTouchToMouseMode());
}

TEST_F(TouchExplorationTest, OneFingerTap) {
  SwitchTouchExplorationMode(true);
  gfx::Point location(11, 12);
  generator_->set_current_location(location);
  generator_->PressTouch();
  generator_->ReleaseTouch();
  AdvanceSimulatedTimePastTapDelay();

  std::vector<ui::LocatedEvent*> events =
      GetCapturedLocatedEventsOfType(ui::ET_MOUSE_MOVED);
  ASSERT_EQ(1U, events.size());

  EXPECT_EQ(location, events[0]->location());
  EXPECT_TRUE(events[0]->flags() & ui::EF_IS_SYNTHESIZED);
  EXPECT_TRUE(events[0]->flags() & ui::EF_TOUCH_ACCESSIBILITY);
  EXPECT_TRUE(IsInNoFingersDownState());
}

TEST_F(TouchExplorationTest, ActualMouseMovesUnaffected) {
  SwitchTouchExplorationMode(true);

  gfx::Point location_start(11, 12);
  gfx::Point location_end(13, 14);
  generator_->set_current_location(location_start);
  generator_->PressTouch();
  AdvanceSimulatedTimePastTapDelay();
  generator_->MoveTouch(location_end);

  gfx::Point location_real_mouse_move(15, 16);
  ui::MouseEvent mouse_move(ui::ET_MOUSE_MOVED,
                            location_real_mouse_move,
                            location_real_mouse_move,
                            0,
                            0);
  generator_->Dispatch(&mouse_move);
  generator_->ReleaseTouch();
  AdvanceSimulatedTimePastTapDelay();

  std::vector<ui::LocatedEvent*> events =
      GetCapturedLocatedEventsOfType(ui::ET_MOUSE_MOVED);
  ASSERT_EQ(4U, events.size());

  EXPECT_EQ(location_start, events[0]->location());
  EXPECT_TRUE(events[0]->flags() & ui::EF_IS_SYNTHESIZED);
  EXPECT_TRUE(events[0]->flags() & ui::EF_TOUCH_ACCESSIBILITY);

  EXPECT_EQ(location_end, events[1]->location());
  EXPECT_TRUE(events[1]->flags() & ui::EF_IS_SYNTHESIZED);
  EXPECT_TRUE(events[1]->flags() & ui::EF_TOUCH_ACCESSIBILITY);

  // The real mouse move goes through.
  EXPECT_EQ(location_real_mouse_move, events[2]->location());
  CONFIRM_EVENTS_ARE_MOUSE_AND_EQUAL(events[2], &mouse_move);
  EXPECT_FALSE(events[2]->flags() & ui::EF_IS_SYNTHESIZED);
  EXPECT_FALSE(events[2]->flags() & ui::EF_TOUCH_ACCESSIBILITY);

  // The touch release gets written as a mouse move.
  EXPECT_EQ(location_end, events[3]->location());
  EXPECT_TRUE(events[3]->flags() & ui::EF_IS_SYNTHESIZED);
  EXPECT_TRUE(events[3]->flags() & ui::EF_TOUCH_ACCESSIBILITY);
  EXPECT_TRUE(IsInNoFingersDownState());
}

// Turn the touch exploration mode on in the middle of the touch gesture.
// Confirm that events from the finger which was touching when the mode was
// turned on don't get rewritten.
TEST_F(TouchExplorationTest, TurnOnMidTouch) {
  SwitchTouchExplorationMode(false);
  generator_->PressTouchId(1);
  EXPECT_TRUE(cursor_client()->IsCursorVisible());
  ClearCapturedEvents();

  // Enable touch exploration mode while the first finger is touching the
  // screen. Ensure that subsequent events from that first finger are not
  // affected by the touch exploration mode, while the touch events from another
  // finger get rewritten.
  SwitchTouchExplorationMode(true);
  ui::TouchEvent touch_move(ui::ET_TOUCH_MOVED,
                            gfx::Point(11, 12),
                            1,
                            Now());
  generator_->Dispatch(&touch_move);
  EXPECT_TRUE(cursor_client()->IsCursorVisible());
  EXPECT_FALSE(cursor_client()->IsMouseEventsEnabled());
  std::vector<ui::LocatedEvent*> captured_events = GetCapturedLocatedEvents();
  ASSERT_EQ(1u, captured_events.size());
  CONFIRM_EVENTS_ARE_TOUCH_AND_EQUAL(captured_events[0], &touch_move);
  ClearCapturedEvents();

  // The press from the second finger should get rewritten.
  generator_->PressTouchId(2);
  AdvanceSimulatedTimePastTapDelay();
  EXPECT_TRUE(IsInTouchToMouseMode());
  captured_events = GetCapturedLocatedEvents();
  std::vector<ui::LocatedEvent*>::const_iterator it;
  for (it = captured_events.begin(); it != captured_events.end(); ++it) {
    if ((*it)->type() == ui::ET_MOUSE_MOVED)
      break;
  }
  EXPECT_NE(captured_events.end(), it);
  ClearCapturedEvents();

  // The release of the first finger shouldn't be affected.
  ui::TouchEvent touch_release(ui::ET_TOUCH_RELEASED,
                               gfx::Point(11, 12),
                               1,
                               Now());
  generator_->Dispatch(&touch_release);
  captured_events = GetCapturedLocatedEvents();
  ASSERT_EQ(1u, captured_events.size());
  CONFIRM_EVENTS_ARE_TOUCH_AND_EQUAL(captured_events[0], &touch_release);
  ClearCapturedEvents();

  // The move and release from the second finger should get rewritten.
  generator_->MoveTouchId(gfx::Point(13, 14), 2);
  generator_->ReleaseTouchId(2);
  AdvanceSimulatedTimePastTapDelay();
  captured_events = GetCapturedLocatedEvents();
  ASSERT_EQ(2u, captured_events.size());
  EXPECT_EQ(ui::ET_MOUSE_MOVED, captured_events[0]->type());
  EXPECT_EQ(ui::ET_MOUSE_MOVED, captured_events[1]->type());
  EXPECT_TRUE(IsInNoFingersDownState());
}

// If an event is received after the double-tap timeout has elapsed, but
// before the timer has fired, a mouse move should still be generated.
TEST_F(TouchExplorationTest, TimerFiresLateDuringTouchExploration) {
  SwitchTouchExplorationMode(true);

  // Make sure the touch is not in a corner of the screen.
  generator_->MoveTouch(gfx::Point(100, 200));

  // Send a press, then add another finger after the double-tap timeout.
  generator_->PressTouchId(1);
  simulated_clock_->Advance(base::TimeDelta::FromMilliseconds(1000));
  generator_->PressTouchId(2);
  std::vector<ui::LocatedEvent*> events =
      GetCapturedLocatedEventsOfType(ui::ET_MOUSE_MOVED);
  ASSERT_EQ(1U, events.size());
  EXPECT_TRUE(events[0]->flags() & ui::EF_IS_SYNTHESIZED);
  EXPECT_TRUE(events[0]->flags() & ui::EF_TOUCH_ACCESSIBILITY);

  generator_->ReleaseTouchId(2);
  generator_->ReleaseTouchId(1);
  AdvanceSimulatedTimePastTapDelay();
  EXPECT_TRUE(IsInNoFingersDownState());
}

// If a new tap is received after the double-tap timeout has elapsed from
// a previous tap, but before the timer has fired, a mouse move should
// still be generated from the old tap.
TEST_F(TouchExplorationTest, TimerFiresLateAfterTap) {
  SwitchTouchExplorationMode(true);

  // Send a tap at location1.
  gfx::Point location0(11, 12);
  generator_->set_current_location(location0);
  generator_->PressTouch();
  generator_->ReleaseTouch();

  // Send a tap at location2, after the double-tap timeout, but before the
  // timer fires.
  gfx::Point location1(33, 34);
  generator_->set_current_location(location1);
  simulated_clock_->Advance(base::TimeDelta::FromMilliseconds(301));
  generator_->PressTouch();
  generator_->ReleaseTouch();
  AdvanceSimulatedTimePastTapDelay();

  std::vector<ui::LocatedEvent*> events =
      GetCapturedLocatedEventsOfType(ui::ET_MOUSE_MOVED);
  ASSERT_EQ(2U, events.size());
  EXPECT_EQ(location0, events[0]->location());
  EXPECT_TRUE(events[0]->flags() & ui::EF_IS_SYNTHESIZED);
  EXPECT_TRUE(events[0]->flags() & ui::EF_TOUCH_ACCESSIBILITY);
  EXPECT_EQ(location1, events[1]->location());
  EXPECT_TRUE(events[1]->flags() & ui::EF_IS_SYNTHESIZED);
  EXPECT_TRUE(events[1]->flags() & ui::EF_TOUCH_ACCESSIBILITY);
  EXPECT_TRUE(IsInNoFingersDownState());
}

// Double-tapping should send a touch press and release through to the location
// of the last successful touch exploration.
TEST_F(TouchExplorationTest, DoubleTap) {
  SwitchTouchExplorationMode(true);

  // Tap at one location, and get a mouse move event.
  gfx::Point tap_location(11, 12);
  generator_->set_current_location(tap_location);
  generator_->PressTouch();
  generator_->ReleaseTouch();
  AdvanceSimulatedTimePastTapDelay();

  std::vector<ui::LocatedEvent*> events =
      GetCapturedLocatedEventsOfType(ui::ET_MOUSE_MOVED);
  ASSERT_EQ(1U, events.size());

  EXPECT_EQ(tap_location, events[0]->location());
  EXPECT_TRUE(events[0]->flags() & ui::EF_IS_SYNTHESIZED);
  EXPECT_TRUE(events[0]->flags() & ui::EF_TOUCH_ACCESSIBILITY);
  ClearCapturedEvents();

  // Now double-tap at a different location. This should result in
  // a single touch press and release at the location of the tap,
  // not at the location of the double-tap.
  gfx::Point double_tap_location(33, 34);
  generator_->set_current_location(double_tap_location);
  generator_->PressTouch();
  generator_->ReleaseTouch();
  generator_->PressTouch();
  generator_->ReleaseTouch();

  std::vector<ui::LocatedEvent*> captured_events = GetCapturedLocatedEvents();
  ASSERT_EQ(2U, captured_events.size());
  EXPECT_EQ(ui::ET_TOUCH_PRESSED, captured_events[0]->type());
  EXPECT_EQ(tap_location, captured_events[0]->location());
  EXPECT_EQ(ui::ET_TOUCH_RELEASED, captured_events[1]->type());
  EXPECT_EQ(tap_location, captured_events[1]->location());
  EXPECT_TRUE(IsInNoFingersDownState());
}

// Double-tapping where the user holds their finger down for the second time
// for a longer press should send a touch press and released (right click)
// to the location of the last successful touch exploration.
TEST_F(TouchExplorationTest, DoubleTapLongPress) {
  SwitchTouchExplorationMode(true);

  // Tap at one location, and get a mouse move event.
  gfx::Point tap_location(11, 12);
  generator_->set_current_location(tap_location);
  generator_->PressTouch();
  generator_->ReleaseTouch();
  AdvanceSimulatedTimePastTapDelay();

  std::vector<ui::LocatedEvent*> events =
      GetCapturedLocatedEventsOfType(ui::ET_MOUSE_MOVED);
  ASSERT_EQ(1U, events.size());

  EXPECT_EQ(tap_location, events[0]->location());
  EXPECT_TRUE(events[0]->flags() & ui::EF_IS_SYNTHESIZED);
  EXPECT_TRUE(events[0]->flags() & ui::EF_TOUCH_ACCESSIBILITY);
  ClearCapturedEvents();

  // Now double-tap and hold at a different location.
  // This should result in a single touch long press and release
  // at the location of the tap, not at the location of the double-tap.
  // There should be a time delay between the touch press and release.
  gfx::Point first_tap_location(33, 34);
  generator_->set_current_location(first_tap_location);
  generator_->PressTouch();
  generator_->ReleaseTouch();
  gfx::Point second_tap_location(23, 24);
  generator_->set_current_location(second_tap_location);
  generator_->PressTouch();
  simulated_clock_->Advance(gesture_detector_config_.longpress_timeout);
  generator_->ReleaseTouch();

  std::vector<ui::LocatedEvent*> captured_events = GetCapturedLocatedEvents();
  ASSERT_EQ(2U, captured_events.size());
  EXPECT_EQ(ui::ET_TOUCH_PRESSED, captured_events[0]->type());
  EXPECT_EQ(tap_location, captured_events[0]->location());
  base::TimeDelta pressed_time = captured_events[0]->time_stamp();
  EXPECT_EQ(ui::ET_TOUCH_RELEASED, captured_events[1]->type());
  EXPECT_EQ(tap_location, captured_events[1]->location());
  base::TimeDelta released_time = captured_events[1]->time_stamp();
  EXPECT_EQ(gesture_detector_config_.longpress_timeout,
            released_time - pressed_time);
}

// Single-tapping should send a touch press and release through to the location
// of the last successful touch exploration if the grace period has not
// elapsed.
TEST_F(TouchExplorationTest, SingleTap) {
  SwitchTouchExplorationMode(true);

  // Tap once to simulate a mouse moved event.
  gfx::Point initial_location(11, 12);
  generator_->set_current_location(initial_location);
  generator_->PressTouch();
  AdvanceSimulatedTimePastTapDelay();
  ClearCapturedEvents();

  // Move to another location for single tap
  gfx::Point tap_location(22, 23);
  generator_->MoveTouch(tap_location);
  generator_->ReleaseTouch();

  // Allow time to pass within the grace period of releasing before
  // tapping again.
  gfx::Point final_location(33, 34);
  generator_->set_current_location(final_location);
  simulated_clock_->Advance(base::TimeDelta::FromMilliseconds(250));
  generator_->PressTouch();
  generator_->ReleaseTouch();

  std::vector<ui::LocatedEvent*> captured_events = GetCapturedLocatedEvents();
  ASSERT_EQ(4U, captured_events.size());
  EXPECT_EQ(ui::ET_MOUSE_MOVED, captured_events[0]->type());
  EXPECT_EQ(ui::ET_MOUSE_MOVED, captured_events[1]->type());
  EXPECT_EQ(ui::ET_TOUCH_PRESSED, captured_events[2]->type());
  EXPECT_EQ(tap_location, captured_events[2]->location());
  EXPECT_EQ(ui::ET_TOUCH_RELEASED, captured_events[3]->type());
  EXPECT_EQ(tap_location, captured_events[3]->location());
}

// Double-tapping without coming from touch exploration (no previous touch
// exploration event) should not generate any events.
TEST_F(TouchExplorationTest, DoubleTapNoTouchExplore) {
  SwitchTouchExplorationMode(true);

  // Double-tap without any previous touch.
  // Touch exploration mode has not been entered, so there is no previous
  // touch exploration event. The double-tap should be discarded, and no events
  // should be generated at all.
  gfx::Point double_tap_location(33, 34);
  generator_->set_current_location(double_tap_location);
  generator_->PressTouch();
  generator_->ReleaseTouch();
  generator_->PressTouch();
  generator_->ReleaseTouch();

  std::vector<ui::LocatedEvent*> captured_events = GetCapturedLocatedEvents();
  ASSERT_EQ(0U, captured_events.size());
}

// Tapping and releasing with a second finger when in touch exploration mode
// should send a touch press and released to the location of the last
// successful touch exploration and return to touch explore.
TEST_F(TouchExplorationTest, SplitTap) {
  SwitchTouchExplorationMode(true);
  gfx::Point initial_touch_location(11, 12);
  gfx::Point second_touch_location(33, 34);

  // Tap and hold at one location, and get a mouse move event in touch explore.
  EnterTouchExplorationModeAtLocation(initial_touch_location);
  std::vector<ui::LocatedEvent*> events =
      GetCapturedLocatedEventsOfType(ui::ET_MOUSE_MOVED);
  ASSERT_EQ(1U, events.size());

  EXPECT_EQ(initial_touch_location, events[0]->location());
  EXPECT_TRUE(events[0]->flags() & ui::EF_IS_SYNTHESIZED);
  EXPECT_TRUE(events[0]->flags() & ui::EF_TOUCH_ACCESSIBILITY);
  ClearCapturedEvents();
  EXPECT_TRUE(IsInTouchToMouseMode());

  // Now tap and release at a different location. This should result in a
  // single touch and release at the location of the first (held) tap,
  // not at the location of the second tap and release.
  // After the release, there is still a finger in touch explore mode.
  ui::TouchEvent split_tap_press(
      ui::ET_TOUCH_PRESSED, second_touch_location, 1, Now());
  generator_->Dispatch(&split_tap_press);
  // To simulate the behavior of the real device, we manually disable
  // mouse events. To not rely on manually setting the state, this is also
  // tested in touch_exploration_controller_browsertest.
  cursor_client()->DisableMouseEvents();
  EXPECT_FALSE(cursor_client()->IsMouseEventsEnabled());
  EXPECT_FALSE(cursor_client()->IsCursorVisible());
  EXPECT_FALSE(IsInGestureInProgressState());
  ui::TouchEvent split_tap_release(
      ui::ET_TOUCH_RELEASED, second_touch_location, 1, Now());
  generator_->Dispatch(&split_tap_release);
  // Releasing the second finger should re-enable mouse events putting us
  // back into the touch exploration mode.
  EXPECT_TRUE(IsInTouchToMouseMode());
  EXPECT_FALSE(IsInNoFingersDownState());

  std::vector<ui::LocatedEvent*> captured_events = GetCapturedLocatedEvents();
  ASSERT_EQ(2U, captured_events.size());
  EXPECT_EQ(ui::ET_TOUCH_PRESSED, captured_events[0]->type());
  EXPECT_EQ(initial_touch_location, captured_events[0]->location());
  EXPECT_EQ(ui::ET_TOUCH_RELEASED, captured_events[1]->type());
  EXPECT_EQ(initial_touch_location, captured_events[1]->location());
  ClearCapturedEvents();

  ui::TouchEvent touch_explore_release(
      ui::ET_TOUCH_RELEASED, initial_touch_location, 0, Now());
  generator_->Dispatch(&touch_explore_release);
  AdvanceSimulatedTimePastTapDelay();
  EXPECT_TRUE(IsInNoFingersDownState());
}

// If split tap is started but the touch explore finger is released first,
// there should still be a touch press and release sent to the location of
// the last successful touch exploration.
// Both fingers should be released after the click goes through.
TEST_F(TouchExplorationTest, SplitTapRelease) {
  SwitchTouchExplorationMode(true);

  gfx::Point initial_touch_location(11, 12);
  gfx::Point second_touch_location(33, 34);

  // Tap and hold at one location, and get a mouse move event in touch explore.
  EnterTouchExplorationModeAtLocation(initial_touch_location);

  std::vector<ui::LocatedEvent*> events =
      GetCapturedLocatedEventsOfType(ui::ET_MOUSE_MOVED);
  ASSERT_EQ(1U, events.size());

  ClearCapturedEvents();

  // Now tap at a different location. Release at the first location,
  // then release at the second. This should result in a
  // single touch and release at the location of the first (held) tap,
  // not at the location of the second tap and release.
  ui::TouchEvent split_tap_press(
      ui::ET_TOUCH_PRESSED, second_touch_location, 1, Now());
  generator_->Dispatch(&split_tap_press);
  ui::TouchEvent touch_explore_release(
      ui::ET_TOUCH_RELEASED, initial_touch_location, 0, Now());
  generator_->Dispatch(&touch_explore_release);
  ui::TouchEvent split_tap_release(
      ui::ET_TOUCH_RELEASED, second_touch_location , 1, Now());
  generator_->Dispatch(&split_tap_release);
  EXPECT_TRUE(IsInNoFingersDownState());

  std::vector<ui::LocatedEvent*> captured_events = GetCapturedLocatedEvents();
  ASSERT_EQ(2U, captured_events.size());
  EXPECT_EQ(ui::ET_TOUCH_PRESSED, captured_events[0]->type());
  EXPECT_EQ(initial_touch_location, captured_events[0]->location());
  EXPECT_EQ(ui::ET_TOUCH_RELEASED, captured_events[1]->type());
  EXPECT_EQ(initial_touch_location, captured_events[1]->location());
}

// When in touch exploration mode, making a long press with a second finger
// should send a touch press and released to the location of the last
// successful touch exploration. There should be a delay between the
// touch and release events (right click).
TEST_F(TouchExplorationTest, SplitTapLongPress) {
  SwitchTouchExplorationMode(true);
  gfx::Point initial_touch_location(11, 12);
  gfx::Point second_touch_location(33, 34);

  // Tap and hold at one location, and get a mouse move event in touch explore.
  EnterTouchExplorationModeAtLocation(initial_touch_location);
  std::vector<ui::LocatedEvent*> events =
      GetCapturedLocatedEventsOfType(ui::ET_MOUSE_MOVED);
  ASSERT_EQ(1U, events.size());

  ClearCapturedEvents();

  // Now tap, hold, and release at a different location. This should result
  // in a single touch and release (long press) at the location of the first
  // (held) tap, not at the location of the second tap and release.
  ui::TouchEvent split_tap_press(
      ui::ET_TOUCH_PRESSED, second_touch_location, 1, Now());
  generator_->Dispatch(&split_tap_press);
  // To simulate the behavior of the real device, we manually disable
  // mouse events, like in the SplitTap test.
  cursor_client()->DisableMouseEvents();
  EXPECT_FALSE(cursor_client()->IsMouseEventsEnabled());
  EXPECT_FALSE(cursor_client()->IsCursorVisible());
  EXPECT_FALSE(IsInGestureInProgressState());
  simulated_clock_->Advance(gesture_detector_config_.longpress_timeout);
  // After the release, there is still a finger in touch exploration, and
  // mouse events should be enabled again.
  ui::TouchEvent split_tap_release(
      ui::ET_TOUCH_RELEASED, second_touch_location, 1, Now());
  generator_->Dispatch(&split_tap_release);
  EXPECT_FALSE(IsInNoFingersDownState());
  EXPECT_TRUE(IsInTouchToMouseMode());

  std::vector<ui::LocatedEvent*> captured_events = GetCapturedLocatedEvents();
  ASSERT_EQ(2U, captured_events.size());
  EXPECT_EQ(ui::ET_TOUCH_PRESSED, captured_events[0]->type());
  EXPECT_EQ(initial_touch_location, captured_events[0]->location());
  base::TimeDelta pressed_time = captured_events[0]->time_stamp();
  EXPECT_EQ(ui::ET_TOUCH_RELEASED, captured_events[1]->type());
  EXPECT_EQ(initial_touch_location, captured_events[1]->location());
  base::TimeDelta released_time = captured_events[1]->time_stamp();
  EXPECT_EQ(gesture_detector_config_.longpress_timeout,
            released_time - pressed_time);
}

// If split tap is started but the touch explore finger is released first,
// there should still be a touch press and release sent to the location of
// the last successful touch exploration. If the remaining finger is held
// as a longpress, there should be a delay between the sent touch and release
// events (right click).All fingers should be released after the click
// goes through.
TEST_F(TouchExplorationTest, SplitTapReleaseLongPress) {
  SwitchTouchExplorationMode(true);
  gfx::Point initial_touch_location(11, 12);
  gfx::Point second_touch_location(33, 34);

  // Tap and hold at one location, and get a mouse move event in touch explore.
  EnterTouchExplorationModeAtLocation(initial_touch_location);
  std::vector<ui::LocatedEvent*> events =
      GetCapturedLocatedEventsOfType(ui::ET_MOUSE_MOVED);
  ASSERT_EQ(1U, events.size());
  ClearCapturedEvents();

  // Now tap at a different location. Release at the first location,
  // then release at the second. This should result in a
  // single touch and release at the location of the first (held) tap,
  // not at the location of the second tap and release.
  // After the release, TouchToMouseMode should still be on.
  ui::TouchEvent split_tap_press(
      ui::ET_TOUCH_PRESSED, second_touch_location, 1, Now());
  generator_->Dispatch(&split_tap_press);
  ui::TouchEvent touch_explore_release(
      ui::ET_TOUCH_RELEASED, initial_touch_location, 0, Now());
  generator_->Dispatch(&touch_explore_release);
  simulated_clock_->Advance(gesture_detector_config_.longpress_timeout);
  ui::TouchEvent split_tap_release(
      ui::ET_TOUCH_RELEASED, second_touch_location, 1, Now());
  generator_->Dispatch(&split_tap_release);
  EXPECT_TRUE(IsInTouchToMouseMode());

  std::vector<ui::LocatedEvent*> captured_events = GetCapturedLocatedEvents();
  ASSERT_EQ(2U, captured_events.size());
  EXPECT_EQ(ui::ET_TOUCH_PRESSED, captured_events[0]->type());
  EXPECT_EQ(initial_touch_location, captured_events[0]->location());
  base::TimeDelta pressed_time = captured_events[0]->time_stamp();
  EXPECT_EQ(ui::ET_TOUCH_RELEASED, captured_events[1]->type());
  EXPECT_EQ(initial_touch_location, captured_events[1]->location());
  base::TimeDelta released_time = captured_events[1]->time_stamp();
  EXPECT_EQ(gesture_detector_config_.longpress_timeout,
            released_time - pressed_time);
 }

TEST_F(TouchExplorationTest, SplitTapLongPressMultiFinger) {
  SwitchTouchExplorationMode(true);
  gfx::Point initial_touch_location(11, 12);
  gfx::Point second_touch_location(33, 34);
  gfx::Point third_touch_location(16, 17);

  // Tap and hold at one location, and get a mouse move event in touch explore.
  EnterTouchExplorationModeAtLocation(initial_touch_location);

  std::vector<ui::LocatedEvent*> events =
      GetCapturedLocatedEventsOfType(ui::ET_MOUSE_MOVED);
  ASSERT_EQ(1U, events.size());

  EXPECT_EQ(initial_touch_location, events[0]->location());
  EXPECT_TRUE(events[0]->flags() & ui::EF_IS_SYNTHESIZED);
  EXPECT_TRUE(events[0]->flags() & ui::EF_TOUCH_ACCESSIBILITY);
  ClearCapturedEvents();

  // Now tap at a different location and hold for long press.
  ui::TouchEvent split_tap_press(
      ui::ET_TOUCH_PRESSED, second_touch_location, 1, Now());
  generator_->Dispatch(&split_tap_press);
  simulated_clock_->Advance(gesture_detector_config_.longpress_timeout);

  // Placing a third finger on the screen should be discarded and not affect
  // the events passed through.
  ui::TouchEvent third_press(
      ui::ET_TOUCH_PRESSED, third_touch_location, 2, Now());
  generator_->Dispatch(&third_press);

  // When all three fingers are released, there should be only two captured
  // events: touch press and touch release. All fingers should then be up.
  ui::TouchEvent touch_explore_release(
      ui::ET_TOUCH_RELEASED, initial_touch_location, 0, Now());
  generator_->Dispatch(&touch_explore_release);
  ui::TouchEvent split_tap_release(
      ui::ET_TOUCH_RELEASED, second_touch_location, 1, Now());
  generator_->Dispatch(&split_tap_release);
  ui::TouchEvent third_tap_release(
      ui::ET_TOUCH_RELEASED, third_touch_location, 2, Now());
  generator_->Dispatch(&third_tap_release);

  std::vector<ui::LocatedEvent*> captured_events = GetCapturedLocatedEvents();
  ASSERT_EQ(2U, captured_events.size());
  EXPECT_EQ(ui::ET_TOUCH_PRESSED, captured_events[0]->type());
  EXPECT_EQ(initial_touch_location, captured_events[0]->location());
  base::TimeDelta pressed_time = captured_events[0]->time_stamp();
  EXPECT_EQ(ui::ET_TOUCH_RELEASED, captured_events[1]->type());
  EXPECT_EQ(initial_touch_location, captured_events[1]->location());
  base::TimeDelta released_time = captured_events[1]->time_stamp();
  EXPECT_EQ(gesture_detector_config_.longpress_timeout,
            released_time - pressed_time);
  EXPECT_TRUE(IsInNoFingersDownState());
}

// If the second finger is pressed soon after the first, the second
// finger passes through and the first does not. However, The press is
// released after the second finger is lifted to go into the wait state.
TEST_F(TouchExplorationTest, TwoToOneFingerReleaseSecond) {
  gfx::Point first_touch_location = gfx::Point(7, 7);
  gfx::Point second_touch_location = gfx::Point(10, 11);
  EnterTwoToOne(first_touch_location, second_touch_location);
  std::vector<ui::LocatedEvent*> captured_events = GetCapturedLocatedEvents();
  ASSERT_EQ(captured_events.size(), 1u);
  ClearCapturedEvents();

  // Confirm events from the second finger go through as is.
  gfx::Point second_touch_move_location = gfx::Point(20, 21);
  ui::TouchEvent second_touch_move(
      ui::ET_TOUCH_MOVED,
      second_touch_move_location,
      1,
      Now());
  generator_->Dispatch(&second_touch_move);
  captured_events = GetCapturedLocatedEvents();
  ASSERT_EQ(1u, captured_events.size());
  ClearCapturedEvents();

  // Events from the first finger should still not go through while the second
  // finger is touching.
  gfx::Point first_touch_move_location = gfx::Point(15, 16);
  generator_->MoveTouchId(first_touch_move_location, 0);
  captured_events = GetCapturedLocatedEvents();
  EXPECT_EQ(0u, captured_events.size());
  EXPECT_TRUE(cursor_client()->IsCursorVisible());
  EXPECT_FALSE(cursor_client()->IsMouseEventsEnabled());

  // A release of the second finger should send an event, as the state
  // changes to the wait state.
  ui::TouchEvent second_touch_release(
      ui::ET_TOUCH_RELEASED, second_touch_move_location, 1, Now());
  generator_->Dispatch(&second_touch_release);
  EXPECT_FALSE(IsInTouchToMouseMode());
  captured_events = GetCapturedLocatedEvents();
  ASSERT_EQ(captured_events.size(), 1u);
  ClearCapturedEvents();

  // No events should be sent after the second finger is lifted.

  ui::TouchEvent unsent_move(ui::ET_TOUCH_MOVED, gfx::Point(21, 22), 0, Now());
  generator_->Dispatch(&unsent_move);

  ui::TouchEvent first_touch_release(
      ui::ET_TOUCH_RELEASED, first_touch_move_location, 0, Now());
  generator_->Dispatch(&first_touch_release);
  captured_events = GetCapturedLocatedEvents();
  ASSERT_EQ(captured_events.size(), 0u);
  EXPECT_TRUE(IsInNoFingersDownState());
}

// The press should also be released if the first finger is lifted
// in TwoToOneFinger.
TEST_F(TouchExplorationTest, TwoToOneFingerRelaseFirst) {
  gfx::Point first_touch_location = gfx::Point(11,12);
  gfx::Point second_touch_location = gfx::Point(21, 22);
  EnterTwoToOne(first_touch_location, second_touch_location);
  std::vector<ui::LocatedEvent*> captured_events = GetCapturedLocatedEvents();
  ASSERT_EQ(captured_events.size(), 1u);
  ClearCapturedEvents();

  // Actions before release have already been tested in the previous test.

  // A release of the first finger should send an event, as the state
  // changes to the wait state.
  ui::TouchEvent first_touch_release(
      ui::ET_TOUCH_RELEASED, first_touch_location, 0, Now());
  generator_->Dispatch(&first_touch_release);
  captured_events = GetCapturedLocatedEvents();
  ASSERT_EQ(captured_events.size(), 1u);
  ClearCapturedEvents();

  // No events should be sent after the second finger is lifted.

  ui::TouchEvent unsent_move(ui::ET_TOUCH_MOVED, gfx::Point(21, 22), 1, Now());
  generator_->Dispatch(&unsent_move);

  ui::TouchEvent second_touch_release(
      ui::ET_TOUCH_RELEASED, second_touch_location, 1, Now());
  generator_->Dispatch(&second_touch_release);
  captured_events = GetCapturedLocatedEvents();
  ASSERT_EQ(captured_events.size(), 0u);
  EXPECT_TRUE(IsInNoFingersDownState());
}

// Placing three fingers should start passthrough, and all fingers should
// continue to be passed through until the last one is released.
TEST_F(TouchExplorationTest, Passthrough) {
  std::vector<ui::LocatedEvent*> captured_events = GetCapturedLocatedEvents();

  gfx::Point first_touch_location = gfx::Point(11,12);
  gfx::Point second_touch_location = gfx::Point(21, 22);
  EnterTwoToOne(first_touch_location, second_touch_location);
  captured_events = GetCapturedLocatedEvents();
  ASSERT_EQ(captured_events.size(), 1u);

  gfx::Point third_touch_location = gfx::Point(31, 32);
  ui::TouchEvent third_touch_press(
      ui::ET_TOUCH_PRESSED, third_touch_location, 2, Now());
  generator_->Dispatch(&third_touch_press);
  captured_events = GetCapturedLocatedEvents();
  // Now all fingers are registered as pressed.
  ASSERT_EQ(captured_events.size(), 3u);
  ClearCapturedEvents();

  // All fingers should be passed through.
  first_touch_location = gfx::Point(13, 14);
  second_touch_location = gfx::Point(23, 24);
  third_touch_location = gfx::Point(33, 34);
  ui::TouchEvent first_touch_first_move(
      ui::ET_TOUCH_MOVED, first_touch_location, 0, Now());
  ui::TouchEvent second_touch_first_move(
      ui::ET_TOUCH_MOVED, second_touch_location, 1, Now());
  ui::TouchEvent third_touch_first_move(
      ui::ET_TOUCH_MOVED, third_touch_location, 2, Now());
  generator_->Dispatch(&first_touch_first_move);
  generator_->Dispatch(&second_touch_first_move);
  generator_->Dispatch(&third_touch_first_move);
  captured_events = GetCapturedLocatedEvents();
  ASSERT_EQ(captured_events.size(), 3u);
  EXPECT_EQ(ui::ET_TOUCH_MOVED, captured_events[0]->type());
  EXPECT_EQ(first_touch_location, captured_events[0]->location());
  EXPECT_EQ(ui::ET_TOUCH_MOVED, captured_events[1]->type());
  EXPECT_EQ(second_touch_location, captured_events[1]->location());
  EXPECT_EQ(ui::ET_TOUCH_MOVED, captured_events[2]->type());
  EXPECT_EQ(third_touch_location, captured_events[2]->location());
  ClearCapturedEvents();

  // When we release the third finger, the other fingers should still be
  // passed through.
  ui::TouchEvent third_touch_release(
      ui::ET_TOUCH_RELEASED, third_touch_location, 2, Now());
  generator_->Dispatch(&third_touch_release);
  captured_events = GetCapturedLocatedEvents();
  ASSERT_EQ(captured_events.size(), 1u);
  ClearCapturedEvents();
  first_touch_location = gfx::Point(15, 16);
  second_touch_location = gfx::Point(25, 26);
  ui::TouchEvent first_touch_second_move(
      ui::ET_TOUCH_MOVED, first_touch_location, 0, Now());
  ui::TouchEvent second_touch_second_move(
      ui::ET_TOUCH_MOVED, second_touch_location, 1, Now());
  generator_->Dispatch(&first_touch_second_move);
  generator_->Dispatch(&second_touch_second_move);
  captured_events = GetCapturedLocatedEvents();
  ASSERT_EQ(captured_events.size(), 2u);
  EXPECT_EQ(ui::ET_TOUCH_MOVED, captured_events[0]->type());
  EXPECT_EQ(first_touch_location, captured_events[0]->location());
  EXPECT_EQ(ui::ET_TOUCH_MOVED, captured_events[1]->type());
  EXPECT_EQ(second_touch_location, captured_events[1]->location());
  ClearCapturedEvents();

  // When we release the second finger, the first finger should still be
  // passed through.
  ui::TouchEvent second_touch_release(
      ui::ET_TOUCH_RELEASED, second_touch_location, 1, Now());
  generator_->Dispatch(&second_touch_release);
  captured_events = GetCapturedLocatedEvents();
  ASSERT_EQ(captured_events.size(), 1u);
  ClearCapturedEvents();
  first_touch_location = gfx::Point(17, 18);
  ui::TouchEvent first_touch_third_move(
      ui::ET_TOUCH_MOVED, first_touch_location, 0, Now());
  generator_->Dispatch(&first_touch_third_move);
  captured_events = GetCapturedLocatedEvents();
  ASSERT_EQ(captured_events.size(), 1u);
  EXPECT_EQ(ui::ET_TOUCH_MOVED, captured_events[0]->type());
  EXPECT_EQ(first_touch_location, captured_events[0]->location());
  ClearCapturedEvents();

  ui::TouchEvent first_touch_release(
      ui::ET_TOUCH_RELEASED, first_touch_location, 0, Now());
  generator_->Dispatch(&first_touch_release);
  captured_events = GetCapturedLocatedEvents();
  ASSERT_EQ(captured_events.size(), 1u);
  EXPECT_TRUE(IsInNoFingersDownState());
}

// Finger must have moved more than slop, faster than the minimum swipe
// velocity, and before the tap timer fires in order to enter
// GestureInProgress state. Otherwise, if the tap timer fires before the a
// gesture is completed, enter touch exploration.
TEST_F(TouchExplorationTest, EnterGestureInProgressState) {
  SwitchTouchExplorationMode(true);
  EXPECT_FALSE(IsInTouchToMouseMode());
  EXPECT_FALSE(IsInGestureInProgressState());

  float distance = gesture_detector_config_.touch_slop + 1;
  ui::TouchEvent first_press(ui::ET_TOUCH_PRESSED, gfx::Point(0, 1), 0, Now());
  gfx::Point second_location(distance / 2, 1);
  gfx::Point third_location(distance, 1);

  generator_->Dispatch(&first_press);
  simulated_clock_->Advance(base::TimeDelta::FromMilliseconds(10));
  // Since we are not out of the touch slop yet, we should not be in gesture in
  // progress.
  generator_->MoveTouch(second_location);
  EXPECT_FALSE(IsInTouchToMouseMode());
  EXPECT_FALSE(IsInGestureInProgressState());
  simulated_clock_->Advance(base::TimeDelta::FromMilliseconds(10));

  // Once we are out of slop, we should be in GestureInProgress.
  generator_->MoveTouch(third_location);
  EXPECT_TRUE(IsInGestureInProgressState());
  EXPECT_FALSE(IsInTouchToMouseMode());
  const ScopedVector<ui::Event>& captured_events = GetCapturedEvents();
  ASSERT_EQ(0U, captured_events.size());

  // Exit out of gesture mode once grace period is over and enter touch
  // exploration.
  AdvanceSimulatedTimePastTapDelay();
  ASSERT_EQ(1U, captured_events.size());
  EXPECT_EQ(ui::ET_MOUSE_MOVED, captured_events[0]->type());
  EXPECT_TRUE(IsInTouchToMouseMode());
  EXPECT_FALSE(IsInGestureInProgressState());
}

// A swipe+direction gesture should trigger a Shift+Search+Direction
// keyboard event.
TEST_F(TouchExplorationTest, GestureSwipe) {
  SwitchTouchExplorationMode(true);
  std::vector<ui::KeyboardCode> directions;
  directions.push_back(ui::VKEY_RIGHT);
  directions.push_back(ui::VKEY_LEFT);
  directions.push_back(ui::VKEY_UP);
  directions.push_back(ui::VKEY_DOWN);

  for (std::vector<ui::KeyboardCode>::const_iterator it = directions.begin();
       it != directions.end();
       ++it) {
    int x = 30;
    int y = 31;
    ui::TouchEvent origin(ui::ET_TOUCH_PRESSED, gfx::Point(x, y), 0, Now());
    generator_->Dispatch(&origin);

    ui::KeyboardCode direction = *it;
    float distance = gesture_detector_config_.touch_slop + 1;
    scoped_ptr<gfx::Point> swipe;
    switch (direction) {
      case ui::VKEY_RIGHT:
        swipe.reset(new gfx::Point(x + distance, y));
        break;
      case ui::VKEY_LEFT:
        swipe.reset(new gfx::Point(x - distance, y));
        break;
      case ui::VKEY_UP:
        swipe.reset(new gfx::Point(x, y - distance));
        break;
      case ui::VKEY_DOWN:
        swipe.reset(new gfx::Point(x, y + distance));
        break;
      default:
        return;
    }

    // A swipe is made when a fling starts
    float delta_time =
        distance / gesture_detector_config_.maximum_fling_velocity;
    simulated_clock_->Advance(base::TimeDelta::FromSecondsD(delta_time));
    generator_->MoveTouch(*swipe);
    EXPECT_TRUE(IsInGestureInProgressState());
    EXPECT_FALSE(IsInTouchToMouseMode());
    const ScopedVector<ui::Event>& captured_events = GetCapturedEvents();
    ASSERT_EQ(0U, captured_events.size());
    generator_->ReleaseTouch();

    // The swipe registered and sent the appropriate key events.
    AssertDirectionalNavigationEvents(captured_events, direction);
    EXPECT_TRUE(IsInNoFingersDownState());
    EXPECT_FALSE(IsInTouchToMouseMode());
    EXPECT_FALSE(IsInGestureInProgressState());
    ClearCapturedEvents();
  }
}

// Since there are so many permutations, this test is too slow in debug
// mode, so it will only be run in release mode.
#if defined(NDEBUG) && !defined(DCHECK_ALWAYS_ON)
#define MAYBE_AllFingerPermutations AllFingerPermutations
#else
#define MAYBE_AllFingerPermutations DISABLED_AllFingerPermutations
#endif

TEST_F(TouchExplorationTest, MAYBE_AllFingerPermutations) {
  SwitchTouchExplorationMode(true);
  SuppressVLOGs(true);
  // We will test all permutations of events from three different fingers
  // to ensure that we return to NO_FINGERS_DOWN when fingers have been
  // released.
  ScopedVector<ui::TouchEvent> all_events;
  for (int touch_id = 0; touch_id < 3; touch_id++){
    int x = 10*touch_id + 1;
    int y = 10*touch_id + 2;
    all_events.push_back(new TouchEvent(
        ui::ET_TOUCH_PRESSED, gfx::Point(x++, y++), touch_id, Now()));
    all_events.push_back(new TouchEvent(
        ui::ET_TOUCH_MOVED, gfx::Point(x++, y++), touch_id, Now()));
    all_events.push_back(new TouchEvent(
        ui::ET_TOUCH_RELEASED, gfx::Point(x, y), touch_id, Now()));
  }

  // I'm going to explain this algorithm, and use an example in parentheses.
  // The example will be all permutations of a b c d.
  // There are four letters and 4! = 24 permutations.
  const int num_events = all_events.size();
  const int num_permutations = Factorial(num_events);

  for (int p = 0; p < num_permutations; p++) {
    std::vector<ui::TouchEvent*> queued_events = all_events.get();
    std::vector<bool> fingers_pressed(3, false);

    int current_num_permutations = num_permutations;
    for (int events_left = num_events; events_left > 0; events_left--) {
      // |p| indexes to each permutation when there are num_permutations
      // permutations. (e.g. 0 is abcd, 1 is abdc, 2 is acbd, 3 is acdb...)
      // But how do we find the index for the current number of permutations?
      // To find the permutation within the part of the sequence we're
      // currently looking at, we need a number between 0 and
      // |current_num_permutations| - 1.
      // (e.g. if we already chose the first letter, there are 3! = 6
      // options left, so we do p % 6. So |current_permutation| would go
      // from 0 to 5 and then reset to 0 again, for all combinations of
      // whichever three letters are remaining, as we loop through the
      // permutations)
      int current_permutation = p % current_num_permutations;

      // Since this is is the total number of permutations starting with
      // this event and including future events, there could be multiple
      // values of current_permutation that will generate the same event
      // in this iteration.
      // (e.g. If we chose 'a' but have b c d to choose from, we choose b when
      // |current_permutation| = 0, 1 and c when |current_permutation| = 2, 3.
      // Note that each letter gets two numbers, which is the next
      // current_num_permutations, 2! for the two letters left.)

      // Branching out from the first event, there are num_permutations
      // permutations, and each value of |p| is associated with one of these
      // permutations. However, once the first event is chosen, there
      // are now |num_events| - 1 events left, so the number of permutations
      // for the rest of the events changes, and will always be equal to
      // the factorial of the events_left.
      // (e.g. There are 3! = 6 permutations that start with 'a', so if we
      // start with 'a' there will be 6 ways to then choose from b c d.)
      // So we now set-up for the next iteration by setting
      // current_num_permutations to the factorial of the next number of
      // events left.
      current_num_permutations /= events_left;

      // To figure out what current event we want to choose, we integer
      // divide the current permutation by the next current_num_permutations.
      // (e.g. If there are 4 letters a b c d and 24 permutations, we divide
      // by 24/4 = 6. Values 0 to 5 when divided by 6 equals 0, so the first
      // 6 permutations start with 'a', and the last 6 will start with 'd'.
      // Note that there are 6 that start with 'a' because there are 6
      // permutations for the next three letters that follow 'a'.)
      int index = current_permutation / current_num_permutations;

      ui::TouchEvent* next_dispatch = queued_events[index];
      ASSERT_TRUE(next_dispatch != NULL);

      // |next_dispatch| has to be put in this container so that its time
      // stamp can be changed to this point in the test, when it is being
      // dispatched..
      EventTestApi test_dispatch(next_dispatch);
      test_dispatch.set_time_stamp(Now());
      generator_->Dispatch(next_dispatch);
      queued_events.erase(queued_events.begin() + index);

      // Keep track of what fingers have been pressed, to release
      // only those fingers at the end, so the check for being in
      // no fingers down can be accurate.
      if (next_dispatch->type() == ET_TOUCH_PRESSED) {
        fingers_pressed[next_dispatch->touch_id()] = true;
      } else if (next_dispatch->type() == ET_TOUCH_RELEASED) {
        fingers_pressed[next_dispatch->touch_id()] = false;
      }
    }
    ASSERT_EQ(queued_events.size(), 0u);

    // Release fingers recorded as pressed.
    for(int j = 0; j < int(fingers_pressed.size()); j++){
      if (fingers_pressed[j] == true) {
        generator_->ReleaseTouchId(j);
        fingers_pressed[j] = false;
      }
    }
    AdvanceSimulatedTimePastPotentialTapDelay();
    EXPECT_TRUE(IsInNoFingersDownState());
    ClearCapturedEvents();
  }
}

// With the simple swipe gestures, if additional fingers are added, then the
// state should change to passthrough.
TEST_F(TouchExplorationTest, FromGestureToPassthrough) {
  SwitchTouchExplorationMode(true);
  EXPECT_FALSE(IsInTouchToMouseMode());
  EXPECT_FALSE(IsInGestureInProgressState());

  float distance = gesture_detector_config_.touch_slop + 1;
  ui::TouchEvent first_press(
      ui::ET_TOUCH_PRESSED, gfx::Point(100, 200), 0, Now());
  generator_->Dispatch(&first_press);
  simulated_clock_->Advance(base::TimeDelta::FromMilliseconds(10));
  gfx::Point second_location(100 + distance, 200);
  generator_->MoveTouch(second_location);
  EXPECT_TRUE(IsInGestureInProgressState());
  EXPECT_FALSE(IsInTouchToMouseMode());
  const ScopedVector<ui::Event>& captured_events = GetCapturedEvents();
  ASSERT_EQ(0U, captured_events.size());

  // Generate a second press that should go through as is.
  ui::TouchEvent second_press(
      ui::ET_TOUCH_PRESSED, gfx::Point(20, 21), 1, Now());
  generator_->Dispatch(&second_press);
  EXPECT_FALSE(IsInGestureInProgressState());
  EXPECT_FALSE(IsInTouchToMouseMode());
  std::vector<ui::LocatedEvent*> captured_located_events =
      GetCapturedLocatedEvents();
  ASSERT_EQ(1U, captured_events.size());
  CONFIRM_EVENTS_ARE_TOUCH_AND_EQUAL(captured_located_events[0], &second_press);
  ClearCapturedEvents();

  // The rest of the events should occur in passthrough.
  generator_->ReleaseTouchId(0);
  ASSERT_EQ(1U, captured_events.size());
  EXPECT_EQ(ui::ET_TOUCH_RELEASED, captured_events[0]->type());
  ClearCapturedEvents();
  generator_->ReleaseTouchId(1);
  ASSERT_EQ(0U, captured_events.size());
}

TEST_F(TouchExplorationTest, EnterSlideGestureState) {
  SwitchTouchExplorationMode(true);
  EXPECT_FALSE(IsInTouchToMouseMode());
  EXPECT_FALSE(IsInGestureInProgressState());

  gfx::Rect window = BoundsOfRootWindowInDIP();
  float distance = gesture_detector_config_.touch_slop + 1;
  ui::TouchEvent first_press(
      ui::ET_TOUCH_PRESSED, gfx::Point(window.right(), 1), 0, Now());
  gfx::Point second_location(window.right(), 1 + distance / 2);
  gfx::Point third_location(window.right(), 1 + distance);

  generator_->Dispatch(&first_press);
  simulated_clock_->Advance(base::TimeDelta::FromMilliseconds(10));

  // Since we haven't moved past slop yet, we should not be in slide gesture.
  generator_->MoveTouch(second_location);
  EXPECT_FALSE(IsInTouchToMouseMode());
  EXPECT_FALSE(IsInGestureInProgressState());
  EXPECT_FALSE(IsInSlideGestureState());
  simulated_clock_->Advance(base::TimeDelta::FromMilliseconds(10));

  // Once we are out of slop, we should be in slide gesture since we are along
  // the edge of the screen.
  generator_->MoveTouch(third_location);
  EXPECT_FALSE(IsInGestureInProgressState());
  EXPECT_TRUE(IsInSlideGestureState());
  EXPECT_FALSE(IsInTouchToMouseMode());
  const ScopedVector<ui::Event>& captured_events = GetCapturedEvents();
  ASSERT_EQ(0U, captured_events.size());

  // Since we are at the right edge of the screen, but the sound timer has not
  // elapsed, there should have two sounds that fired and two volume
  // changes (one for each movement).
  size_t num_adjust_sounds = delegate_.NumAdjustSounds();
  ASSERT_EQ(2U, num_adjust_sounds);
  ASSERT_EQ(2U, delegate_.VolumeChanges().size());

  // Exit out of slide gesture once touch is lifted, but not before even if the
  // grace period is over.

  AdvanceSimulatedTimePastPotentialTapDelay();
  ASSERT_EQ(0U, captured_events.size());
  EXPECT_FALSE(IsInTouchToMouseMode());
  EXPECT_FALSE(IsInGestureInProgressState());
  EXPECT_TRUE(IsInSlideGestureState());

  generator_->ReleaseTouch();
  ASSERT_EQ(0U, captured_events.size());
  EXPECT_FALSE(IsInTouchToMouseMode());
  EXPECT_FALSE(IsInGestureInProgressState());
  EXPECT_FALSE(IsInSlideGestureState());
}

// If a press + move occurred outside the boundaries, but within the slop
// boundaries and then moved into the boundaries of an edge, there still should
// not be a slide gesture.
TEST_F(TouchExplorationTest, AvoidEnteringSlideGesture) {
  SwitchTouchExplorationMode(true);

  gfx::Rect window = BoundsOfRootWindowInDIP();
  float distance = gesture_detector_config_.touch_slop + 1;
  ui::TouchEvent first_press(
      ui::ET_TOUCH_PRESSED,
      gfx::Point(window.right() - GetSlopDistanceFromEdge(), 1),
      0,
      Now());
  gfx::Point out_of_slop(window.right() - GetSlopDistanceFromEdge() + distance,
                         1);
  gfx::Point into_boundaries(window.right() - GetMaxDistanceFromEdge() / 2, 1);

  generator_->Dispatch(&first_press);
  simulated_clock_->Advance(base::TimeDelta::FromMilliseconds(10));

  generator_->MoveTouch(out_of_slop);
  EXPECT_FALSE(IsInTouchToMouseMode());
  EXPECT_TRUE(IsInGestureInProgressState());
  EXPECT_FALSE(IsInSlideGestureState());
  simulated_clock_->Advance(base::TimeDelta::FromMilliseconds(10));

  // Since we did not start moving while in the boundaries, we should not be in
  // slide gestures.
  generator_->MoveTouch(into_boundaries);
  EXPECT_TRUE(IsInGestureInProgressState());
  EXPECT_FALSE(IsInSlideGestureState());
  EXPECT_FALSE(IsInTouchToMouseMode());
  const ScopedVector<ui::Event>& captured_events = GetCapturedEvents();
  ASSERT_EQ(0U, captured_events.size());

  generator_->ReleaseTouch();
}

// If the slide gesture begins within the boundaries and then moves
// SlopDistanceFromEdge there should still be a sound change. If the finger
// moves into the center screen, there should no longer be a sound change but it
// should still be in slide gesture. If the finger moves back into the edges
// without lifting, it should start changing sound again.
TEST_F(TouchExplorationTest, TestingBoundaries) {
  SwitchTouchExplorationMode(true);

  gfx::Rect window = BoundsOfRootWindowInDIP();
  gfx::Point initial_press(window.right() - GetMaxDistanceFromEdge() / 2, 1);
  ui::TouchEvent first_press(ui::ET_TOUCH_PRESSED, initial_press, 0, Now());
  gfx::Point touch_move(initial_press.x() + gesture_detector_config_.touch_slop,
                        1);
  gfx::Point into_slop_boundaries(
      window.right() - GetSlopDistanceFromEdge() / 2, 1);
  gfx::Point center_screen(window.right() / 2, window.bottom() / 2);

  generator_->Dispatch(&first_press);
  simulated_clock_->Advance(base::TimeDelta::FromMilliseconds(10));

  generator_->MoveTouch(touch_move);
  EXPECT_FALSE(IsInTouchToMouseMode());
  EXPECT_FALSE(IsInGestureInProgressState());
  EXPECT_FALSE(IsInSlideGestureState());
  simulated_clock_->Advance(base::TimeDelta::FromMilliseconds(10));

  // Move the touch into slop boundaries. It should stil be in slide gestures
  // and adjust the volume.
  generator_->MoveTouch(into_slop_boundaries);
  EXPECT_FALSE(IsInGestureInProgressState());
  EXPECT_TRUE(IsInSlideGestureState());
  EXPECT_FALSE(IsInTouchToMouseMode());

  // The sound is rate limiting so it only activates every 150ms.
  simulated_clock_->Advance(base::TimeDelta::FromMilliseconds(200));

  size_t num_adjust_sounds = delegate_.NumAdjustSounds();
  ASSERT_EQ(2U, num_adjust_sounds);
  ASSERT_EQ(2U, delegate_.VolumeChanges().size());

  // Move the touch into the center of the window. It should still be in slide
  // gestures, but there should not be anymore volume adjustments.
  generator_->MoveTouch(center_screen);
  EXPECT_FALSE(IsInGestureInProgressState());
  EXPECT_TRUE(IsInSlideGestureState());
  EXPECT_FALSE(IsInTouchToMouseMode());

  simulated_clock_->Advance(base::TimeDelta::FromMilliseconds(200));
  num_adjust_sounds = delegate_.NumAdjustSounds();
  ASSERT_EQ(2U, num_adjust_sounds);
  ASSERT_EQ(2U, delegate_.VolumeChanges().size());

  // Move the touch back into slop edge distance and volume should be changing
  // again.
  generator_->MoveTouch(into_slop_boundaries);
  EXPECT_FALSE(IsInGestureInProgressState());
  EXPECT_TRUE(IsInSlideGestureState());
  EXPECT_FALSE(IsInTouchToMouseMode());

  generator_->MoveTouch(
      gfx::Point(into_slop_boundaries.x() + gesture_detector_config_.touch_slop,
                 into_slop_boundaries.y()));
  simulated_clock_->Advance(base::TimeDelta::FromMilliseconds(200));

  num_adjust_sounds = delegate_.NumAdjustSounds();
  ASSERT_EQ(3U, num_adjust_sounds);
  ASSERT_EQ(3U, delegate_.VolumeChanges().size());

  const ScopedVector<ui::Event>& captured_events = GetCapturedEvents();
  ASSERT_EQ(0U, captured_events.size());

  generator_->ReleaseTouch();
}

// Even if the gesture starts within bounds, if it has not moved past slop
// within the grace period, it should go to touch exploration.
TEST_F(TouchExplorationTest, InBoundariesTouchExploration) {
  SwitchTouchExplorationMode(true);

  gfx::Rect window = BoundsOfRootWindowInDIP();
  gfx::Point initial_press(window.right() - GetMaxDistanceFromEdge() / 2, 1);
  ui::TouchEvent first_press(
      ui::ET_TOUCH_PRESSED,
      initial_press,
      0,
      Now());
  generator_->Dispatch(&first_press);
  EXPECT_FALSE(IsInGestureInProgressState());
  EXPECT_FALSE(IsInSlideGestureState());
  EXPECT_FALSE(IsInTouchToMouseMode());

  AdvanceSimulatedTimePastTapDelay();
  EXPECT_FALSE(IsInGestureInProgressState());
  EXPECT_FALSE(IsInSlideGestureState());
  EXPECT_TRUE(IsInTouchToMouseMode());
}

}  // namespace ui
