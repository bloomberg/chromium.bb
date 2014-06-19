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
#include "ui/gfx/geometry/point.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface.h"

namespace ui {

namespace {
// Records all mouse and touch events.
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
  const ScopedVector<ui::LocatedEvent>& captured_events() const {
    return events_;
  }

 private:
  ScopedVector<ui::LocatedEvent> events_;

  DISALLOW_COPY_AND_ASSIGN(EventCapturer);
};

}  // namespace

class TouchExplorationTest : public aura::test::AuraTestBase {
 public:
  TouchExplorationTest()
      : simulated_clock_(new base::SimpleTestTickClock()) {}
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

  const ScopedVector<ui::LocatedEvent>& GetCapturedEvents() {
    return event_capturer_.captured_events();
  }

  std::vector<ui::LocatedEvent*> GetCapturedEventsOfType(int type) {
    const ScopedVector<ui::LocatedEvent>& all_events = GetCapturedEvents();
    std::vector<ui::LocatedEvent*> events;
    for (size_t i = 0; i < all_events.size(); ++i) {
      if (type == all_events[i]->type())
        events.push_back(all_events[i]);
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

  void SwitchTouchExplorationMode(bool on) {
    if (!on && touch_exploration_controller_.get()) {
      touch_exploration_controller_.reset();
    } else if (on && !touch_exploration_controller_.get()) {
      touch_exploration_controller_.reset(
          new ui::TouchExplorationController(root_window()));
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

 private:
  EventCapturer event_capturer_;
  scoped_ptr<ui::TouchExplorationController> touch_exploration_controller_;
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

#define CONFIRM_EVENTS_ARE_TOUCH_AND_EQUAL(e1, e2) \
  ASSERT_NO_FATAL_FAILURE(ConfirmEventsAreTouchAndEqual(e1, e2))

#define CONFIRM_EVENTS_ARE_MOUSE_AND_EQUAL(e1, e2) \
  ASSERT_NO_FATAL_FAILURE(ConfirmEventsAreMouseAndEqual(e1, e2))

// TODO(mfomitchev): Need to investigate why we don't get mouse enter/exit
// events when running these tests as part of ui_unittests. We do get them when
// the tests are run as part of ash unit tests.

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
      GetCapturedEventsOfType(ui::ET_MOUSE_MOVED);
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

  std::vector<ui::LocatedEvent*> events =
      GetCapturedEventsOfType(ui::ET_MOUSE_MOVED);
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
  const ScopedVector<ui::LocatedEvent>& captured_events = GetCapturedEvents();
  ASSERT_EQ(1u, captured_events.size());
  CONFIRM_EVENTS_ARE_TOUCH_AND_EQUAL(captured_events[0], &touch_move);
  ClearCapturedEvents();

  // The press from the second finger should get rewritten.
  generator_->PressTouchId(2);
  AdvanceSimulatedTimePastTapDelay();
  EXPECT_TRUE(IsInTouchToMouseMode());
  ScopedVector<ui::LocatedEvent>::const_iterator it;
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
  ASSERT_EQ(1u, captured_events.size());
  CONFIRM_EVENTS_ARE_TOUCH_AND_EQUAL(captured_events[0], &touch_release);
  ClearCapturedEvents();

  // The move and release from the second finger should get rewritten.
  generator_->MoveTouchId(gfx::Point(13, 14), 2);
  generator_->ReleaseTouchId(2);
  ASSERT_EQ(2u, captured_events.size());
  EXPECT_EQ(ui::ET_MOUSE_MOVED, captured_events[0]->type());
  EXPECT_EQ(ui::ET_MOUSE_MOVED, captured_events[1]->type());
  EXPECT_TRUE(IsInNoFingersDownState());
}

TEST_F(TouchExplorationTest, TwoFingerTouch) {
  SwitchTouchExplorationMode(true);
  generator_->PressTouchId(1);
  ClearCapturedEvents();

  // Confirm events from the second finger go through as is.
  ui::TouchEvent touch_press(
      ui::ET_TOUCH_PRESSED,
      gfx::Point(10, 11),
      2,
      Now());
  generator_->Dispatch(&touch_press);
  EXPECT_TRUE(cursor_client()->IsCursorVisible());
  EXPECT_FALSE(cursor_client()->IsMouseEventsEnabled());
  const ScopedVector<ui::LocatedEvent>& captured_events = GetCapturedEvents();
  // TODO(mfomitchev): mouse enter/exit events
  // There will be a ET_MOUSE_EXITED event synthesized when the mouse cursor is
  // hidden - ignore it.
  ScopedVector<ui::LocatedEvent>::const_iterator it;
  for (it = captured_events.begin(); it != captured_events.end(); ++it) {
    if ((*it)->type() == ui::ET_TOUCH_PRESSED) {
      CONFIRM_EVENTS_ARE_TOUCH_AND_EQUAL(*it, &touch_press);
      break;
    }
  }
  EXPECT_NE(captured_events.end(), it);
  ClearCapturedEvents();
  ui::TouchEvent touch_move(
      ui::ET_TOUCH_MOVED,
      gfx::Point(20, 21),
      2,
      Now());
  generator_->Dispatch(&touch_move);
  ASSERT_EQ(1u, captured_events.size());
  CONFIRM_EVENTS_ARE_TOUCH_AND_EQUAL(captured_events[0], &touch_move);
  ClearCapturedEvents();

  // Confirm mouse moves go through unaffected.
  ui::MouseEvent mouse_move(ui::ET_MOUSE_MOVED,
                            gfx::Point(13, 14),
                            gfx::Point(13, 14),
                            0,
                            0);
  generator_->Dispatch(&mouse_move);
  // TODO(mfomitchev): mouse enter/exit events
  // Ignore synthesized ET_MOUSE_ENTERED/ET_MOUSE_EXITED
  for (it = captured_events.begin(); it != captured_events.end(); ++it) {
    if ((*it)->type() == ui::ET_MOUSE_MOVED) {
      CONFIRM_EVENTS_ARE_MOUSE_AND_EQUAL(*it, &mouse_move);
      break;
    }
  }
  EXPECT_NE(captured_events.end(), it);
  ClearCapturedEvents();

  // Events from the first finger should not go through while the second finger
  // is touching.
  gfx::Point touch1_location = gfx::Point(15, 16);
  generator_->MoveTouchId(touch1_location, 1);
  EXPECT_EQ(0u, GetCapturedEvents().size());

  EXPECT_TRUE(cursor_client()->IsCursorVisible());
  EXPECT_FALSE(cursor_client()->IsMouseEventsEnabled());

  // A release of the second finger should be rewritten as a mouse move
  // of that finger to the |touch1_location| and we stay in passthrough
  // mode.
  ui::TouchEvent touch_release(
      ui::ET_TOUCH_RELEASED,
      gfx::Point(25, 26),
      2,
      Now());
  generator_->Dispatch(&touch_release);
  EXPECT_FALSE(IsInTouchToMouseMode());
  ASSERT_EQ(captured_events.size(), 1u);
  EXPECT_EQ(touch1_location, captured_events[0]->location());
}

TEST_F(TouchExplorationTest, MultiFingerTouch) {
  SwitchTouchExplorationMode(true);
  generator_->PressTouchId(1);
  generator_->PressTouchId(2);
  ClearCapturedEvents();

  // Confirm events from other fingers go through as is.
  ui::TouchEvent touch3_press(ui::ET_TOUCH_PRESSED,
                              gfx::Point(10, 11),
                              3,
                              Now());
  ui::TouchEvent touch3_move1(ui::ET_TOUCH_MOVED,
                              gfx::Point(12, 13),
                              3,
                              Now());
  ui::TouchEvent touch4_press(ui::ET_TOUCH_PRESSED,
                              gfx::Point(20, 21),
                              4,
                              Now());
  ui::TouchEvent touch3_move2(ui::ET_TOUCH_MOVED,
                              gfx::Point(14, 15),
                              3,
                              Now());
  ui::TouchEvent touch4_move(ui::ET_TOUCH_MOVED,
                             gfx::Point(22, 23),
                             4,
                             Now());
  ui::TouchEvent touch3_release(ui::ET_TOUCH_RELEASED,
                                gfx::Point(14, 15),
                                3,
                                Now());
  ui::TouchEvent touch4_release(ui::ET_TOUCH_RELEASED,
                                gfx::Point(22, 23),
                                4,
                                Now());
  generator_->Dispatch(&touch3_press);
  generator_->Dispatch(&touch3_move1);
  generator_->Dispatch(&touch4_press);
  generator_->Dispatch(&touch3_move2);
  generator_->Dispatch(&touch4_move);
  generator_->Dispatch(&touch3_release);
  generator_->Dispatch(&touch4_release);

  const ScopedVector<ui::LocatedEvent>& captured_events = GetCapturedEvents();
  ASSERT_EQ(7u, captured_events.size());
  CONFIRM_EVENTS_ARE_TOUCH_AND_EQUAL(captured_events[0], &touch3_press);
  CONFIRM_EVENTS_ARE_TOUCH_AND_EQUAL(captured_events[1], &touch3_move1);
  CONFIRM_EVENTS_ARE_TOUCH_AND_EQUAL(captured_events[2], &touch4_press);
  CONFIRM_EVENTS_ARE_TOUCH_AND_EQUAL(captured_events[3], &touch3_move2);
  CONFIRM_EVENTS_ARE_TOUCH_AND_EQUAL(captured_events[4], &touch4_move);

  // The release of finger 3 is rewritten as a move to the former location
  // of finger 1.
  EXPECT_EQ(ui::ET_TOUCH_MOVED, captured_events[5]->type());
  CONFIRM_EVENTS_ARE_TOUCH_AND_EQUAL(captured_events[6], &touch4_release);
}

// Test the case when there are multiple fingers on the screen and the first
// finger is released. This should be ignored, but then the second finger
// release should be passed through.
TEST_F(TouchExplorationTest, FirstFingerLifted) {
  SwitchTouchExplorationMode(true);
  generator_->PressTouchId(1);
  generator_->PressTouchId(2);
  gfx::Point touch2_location(10, 11);
  generator_->MoveTouchId(touch2_location, 2);
  generator_->PressTouchId(3);
  gfx::Point touch3_location(20, 21);
  generator_->MoveTouchId(touch3_location, 3);
  ClearCapturedEvents();

  // Release of finger 1 should be ignored.
  generator_->ReleaseTouchId(1);
  const ScopedVector<ui::LocatedEvent>& captured_events = GetCapturedEvents();
  ASSERT_EQ(0u, captured_events.size());

  // Move of finger 2 should be passed through.
  gfx::Point touch2_new_location(20, 11);
  generator_->MoveTouchId(touch2_new_location, 2);
  ASSERT_EQ(1u, captured_events.size());
  EXPECT_EQ(ui::ET_TOUCH_MOVED, captured_events[0]->type());
  EXPECT_EQ(touch2_new_location, captured_events[0]->location());
  ClearCapturedEvents();

  // Release of finger 2 should be passed through.
  ui::TouchEvent touch2_release(
      ui::ET_TOUCH_RELEASED,
      gfx::Point(14, 15),
      2,
      Now());
  generator_->Dispatch(&touch2_release);
  ASSERT_EQ(1u, captured_events.size());
  CONFIRM_EVENTS_ARE_TOUCH_AND_EQUAL(captured_events[0], &touch2_release);
}

// Test the case when there are multiple fingers on the screen and the
// second finger is released. This should be rewritten as a move to the
// location of the first finger.
TEST_F(TouchExplorationTest, SecondFingerLifted) {
  SwitchTouchExplorationMode(true);
  gfx::Point touch1_location(0, 11);
  generator_->set_current_location(touch1_location);
  generator_->PressTouchId(1);
  generator_->PressTouchId(2);
  gfx::Point touch2_location(10, 11);
  generator_->MoveTouchId(touch2_location, 2);
  generator_->PressTouchId(3);
  gfx::Point touch3_location(20, 21);
  generator_->MoveTouchId(touch3_location, 3);
  ClearCapturedEvents();

  // Release of finger 2 should be rewritten as a move to the location
  // of the first finger.
  generator_->ReleaseTouchId(2);
  const ScopedVector<ui::LocatedEvent>& captured_events = GetCapturedEvents();
  ASSERT_EQ(1u, captured_events.size());
  EXPECT_EQ(ui::ET_TOUCH_MOVED, captured_events[0]->type());
  EXPECT_EQ(2, static_cast<ui::TouchEvent*>(captured_events[0])->touch_id());
  EXPECT_EQ(touch1_location, captured_events[0]->location());
  ClearCapturedEvents();

  // Move of finger 1 should be rewritten as a move of finger 2.
  gfx::Point touch1_new_location(0, 41);
  generator_->MoveTouchId(touch1_new_location, 1);
  ASSERT_EQ(1u, captured_events.size());
  EXPECT_EQ(ui::ET_TOUCH_MOVED, captured_events[0]->type());
  EXPECT_EQ(2, static_cast<ui::TouchEvent*>(captured_events[0])->touch_id());
  EXPECT_EQ(touch1_new_location, captured_events[0]->location());
  ClearCapturedEvents();

  // Release of finger 1 should be rewritten as release of finger 2.
  gfx::Point touch1_final_location(0, 41);
  ui::TouchEvent touch1_release(
      ui::ET_TOUCH_RELEASED,
      touch1_final_location,
      1,
      Now());
  generator_->Dispatch(&touch1_release);
  ASSERT_EQ(1u, captured_events.size());
  EXPECT_EQ(ui::ET_TOUCH_RELEASED, captured_events[0]->type());
  EXPECT_EQ(2, static_cast<ui::TouchEvent*>(captured_events[0])->touch_id());
  EXPECT_EQ(touch1_final_location, captured_events[0]->location());
}

// If an event is received after the double-tap timeout has elapsed, but
// before the timer has fired, a mouse move should still be generated.
TEST_F(TouchExplorationTest, TimerFiresLateDuringTouchExploration) {
  SwitchTouchExplorationMode(true);

  // Send a press, then add another finger after the double-tap timeout.
  generator_->PressTouchId(1);
  simulated_clock_->Advance(base::TimeDelta::FromMilliseconds(1000));
  generator_->PressTouchId(2);
  std::vector<ui::LocatedEvent*> events =
      GetCapturedEventsOfType(ui::ET_MOUSE_MOVED);
  ASSERT_EQ(1U, events.size());
  EXPECT_TRUE(events[0]->flags() & ui::EF_IS_SYNTHESIZED);
  EXPECT_TRUE(events[0]->flags() & ui::EF_TOUCH_ACCESSIBILITY);

  generator_->ReleaseTouchId(2);
  generator_->ReleaseTouchId(1);
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
      GetCapturedEventsOfType(ui::ET_MOUSE_MOVED);
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
      GetCapturedEventsOfType(ui::ET_MOUSE_MOVED);
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

  const ScopedVector<ui::LocatedEvent>& captured_events = GetCapturedEvents();
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
      GetCapturedEventsOfType(ui::ET_MOUSE_MOVED);
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

  const ScopedVector<ui::LocatedEvent>& captured_events = GetCapturedEvents();
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

  const ScopedVector<ui::LocatedEvent>& captured_events = GetCapturedEvents();
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
      GetCapturedEventsOfType(ui::ET_MOUSE_MOVED);
  ASSERT_EQ(1U, events.size());

  EXPECT_EQ(initial_touch_location, events[0]->location());
  EXPECT_TRUE(events[0]->flags() & ui::EF_IS_SYNTHESIZED);
  EXPECT_TRUE(events[0]->flags() & ui::EF_TOUCH_ACCESSIBILITY);
  ClearCapturedEvents();

  // Now tap and release at a different location. This should result in a
  // single touch and release at the location of the first (held) tap,
  // not at the location of the second tap and release.
  // After the release, there is still a finger in touch explore mode.
  ui::TouchEvent split_tap_press(
      ui::ET_TOUCH_PRESSED, second_touch_location, 1, Now());
  generator_->Dispatch(&split_tap_press);
  ui::TouchEvent split_tap_release(
      ui::ET_TOUCH_RELEASED, second_touch_location, 1, Now());
  generator_->Dispatch(&split_tap_release);
  EXPECT_FALSE(IsInNoFingersDownState());

  const ScopedVector<ui::LocatedEvent>& captured_events = GetCapturedEvents();
  ASSERT_EQ(2U, captured_events.size());
  EXPECT_EQ(ui::ET_TOUCH_PRESSED, captured_events[0]->type());
  EXPECT_EQ(initial_touch_location, captured_events[0]->location());
  EXPECT_EQ(ui::ET_TOUCH_RELEASED, captured_events[1]->type());
  EXPECT_EQ(initial_touch_location, captured_events[1]->location());
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
      GetCapturedEventsOfType(ui::ET_MOUSE_MOVED);
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

  const ScopedVector<ui::LocatedEvent>& captured_events = GetCapturedEvents();
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
      GetCapturedEventsOfType(ui::ET_MOUSE_MOVED);
  ASSERT_EQ(1U, events.size());

  ClearCapturedEvents();

  // Now tap and release at a different location. This should result in a
  // single touch and release at the location of the first (held) tap,
  // not at the location of the second tap and release.
  // After the release, there is still a finger in touch explore mode.
  ui::TouchEvent split_tap_press(
      ui::ET_TOUCH_PRESSED, second_touch_location, 1, Now());
  generator_->Dispatch(&split_tap_press);
  simulated_clock_->Advance(gesture_detector_config_.longpress_timeout);
  ui::TouchEvent split_tap_release(
      ui::ET_TOUCH_RELEASED, second_touch_location, 1, Now());
  generator_->Dispatch(&split_tap_release);
  EXPECT_FALSE(IsInNoFingersDownState());

  const ScopedVector<ui::LocatedEvent>& captured_events = GetCapturedEvents();
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
      GetCapturedEventsOfType(ui::ET_MOUSE_MOVED);
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

  const ScopedVector<ui::LocatedEvent>& captured_events = GetCapturedEvents();
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
      GetCapturedEventsOfType(ui::ET_MOUSE_MOVED);
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

  const ScopedVector<ui::LocatedEvent>& captured_events = GetCapturedEvents();
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

}  // namespace ui
