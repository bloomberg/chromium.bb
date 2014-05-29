// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/chromeos/touch_exploration_controller.h"

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
  TouchExplorationTest() {}
  virtual ~TouchExplorationTest() {}

  virtual void SetUp() OVERRIDE {
    if (gfx::GetGLImplementation() == gfx::kGLImplementationNone)
      gfx::GLSurface::InitializeOneOffForTests();
    aura::test::AuraTestBase::SetUp();
    cursor_client_.reset(new aura::test::TestCursorClient(root_window()));
    root_window()->AddPreTargetHandler(&event_capturer_);
  }

  virtual void TearDown() OVERRIDE {
    root_window()->RemovePreTargetHandler(&event_capturer_);
    SwitchTouchExplorationMode(false);
    cursor_client_.reset();
    aura::test::AuraTestBase::TearDown();
  }

  const ScopedVector<ui::LocatedEvent>& GetCapturedEvents() {
    return event_capturer_.captured_events();
  }

  void ClearCapturedEvents() {
    event_capturer_.Reset();
  }

 protected:
  aura::client::CursorClient* cursor_client() { return cursor_client_.get(); }

  void SwitchTouchExplorationMode(bool on) {
    if (!on && touch_exploration_controller_.get())
      touch_exploration_controller_.reset();
    else if (on && !touch_exploration_controller_.get())
      touch_exploration_controller_.reset(
          new ui::TouchExplorationController(root_window()));
  }

  bool IsInTouchToMouseMode() {
    aura::client::CursorClient* cursor_client =
        aura::client::GetCursorClient(root_window());
    return cursor_client &&
           cursor_client->IsMouseEventsEnabled() &&
           !cursor_client->IsCursorVisible();
  }

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

// Simple test to confirm one-finger touches are transformed into mouse moves.
TEST_F(TouchExplorationTest, OneFingerTouch) {
  SwitchTouchExplorationMode(true);
  cursor_client()->ShowCursor();
  cursor_client()->DisableMouseEvents();
  aura::test::EventGenerator generator(root_window());
  gfx::Point location_start = generator.current_location();
  gfx::Point location_end(11, 12);
  generator.PressTouch();
  EXPECT_TRUE(IsInTouchToMouseMode());
  generator.MoveTouch(location_end);
  // Confirm the actual mouse moves are unaffected.
  ui::MouseEvent mouse_move(ui::ET_MOUSE_MOVED,
                            gfx::Point(13, 14),
                            gfx::Point(13, 14),
                            0,
                            0);
  generator.Dispatch(&mouse_move);
  generator.ReleaseTouch();

  const ScopedVector<ui::LocatedEvent>& captured_events = GetCapturedEvents();
  ScopedVector<ui::LocatedEvent>::const_iterator it;
  // TODO(mfomitchev): mouse enter/exit events
  int num_mouse_moves = 0;
  for (it = captured_events.begin(); it != captured_events.end(); ++it) {
    int type = (*it)->type();
    // Ignore enter and exit mouse events synthesized when the mouse cursor is
    // shown or hidden.
    if (type == ui::ET_MOUSE_ENTERED || type == ui::ET_MOUSE_EXITED)
      continue;
    EXPECT_EQ(ui::ET_MOUSE_MOVED, (*it)->type());
    if (num_mouse_moves == 0)
      EXPECT_EQ(location_start, (*it)->location());
    if (num_mouse_moves == 1 || num_mouse_moves == 3)
      EXPECT_EQ(location_end, (*it)->location());
    if (num_mouse_moves == 2)
      CONFIRM_EVENTS_ARE_MOUSE_AND_EQUAL(*it, &mouse_move);
    if (num_mouse_moves != 2) {
      EXPECT_TRUE((*it)->flags() & ui::EF_IS_SYNTHESIZED);
      EXPECT_TRUE((*it)->flags() & ui::EF_TOUCH_ACCESSIBILITY);
    }
    num_mouse_moves++;
  }
  EXPECT_EQ(4, num_mouse_moves);
}

// Turn the touch exploration mode on in the middle of the touch gesture.
// Confirm that events from the finger which was touching when the mode was
// turned on don't get rewritten.
TEST_F(TouchExplorationTest, TurnOnMidTouch) {
  SwitchTouchExplorationMode(false);
  cursor_client()->ShowCursor();
  cursor_client()->DisableMouseEvents();
  aura::test::EventGenerator generator(root_window());
  generator.PressTouchId(1);
  EXPECT_TRUE(cursor_client()->IsCursorVisible());
  EXPECT_FALSE(cursor_client()->IsMouseEventsEnabled());
  ClearCapturedEvents();

  // Enable touch exploration mode while the first finger is touching the
  // screen. Ensure that subsequent events from that first finger are not
  // affected by the touch exploration mode, while the touch events from another
  // finger get rewritten.
  SwitchTouchExplorationMode(true);
  ui::TouchEvent touch_move(ui::ET_TOUCH_MOVED,
                            gfx::Point(11, 12),
                            1,
                            ui::EventTimeForNow());
  generator.Dispatch(&touch_move);
  EXPECT_TRUE(cursor_client()->IsCursorVisible());
  EXPECT_FALSE(cursor_client()->IsMouseEventsEnabled());
  const ScopedVector<ui::LocatedEvent>& captured_events = GetCapturedEvents();
  ASSERT_EQ(1u, captured_events.size());
  CONFIRM_EVENTS_ARE_TOUCH_AND_EQUAL(captured_events[0], &touch_move);
  ClearCapturedEvents();

  // The press from the second finger should get rewritten.
  generator.PressTouchId(2);
  EXPECT_TRUE(IsInTouchToMouseMode());
  // TODO(mfomitchev): mouse enter/exit events
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
                               ui::EventTimeForNow());
  generator.Dispatch(&touch_release);
  ASSERT_EQ(1u, captured_events.size());
  CONFIRM_EVENTS_ARE_TOUCH_AND_EQUAL(captured_events[0], &touch_release);
  ClearCapturedEvents();

  // The move and release from the second finger should get rewritten.
  generator.MoveTouchId(gfx::Point(13, 14), 2);
  generator.ReleaseTouchId(2);
  ASSERT_EQ(2u, captured_events.size());
  EXPECT_EQ(ui::ET_MOUSE_MOVED, captured_events[0]->type());
  EXPECT_EQ(ui::ET_MOUSE_MOVED, captured_events[1]->type());
}

TEST_F(TouchExplorationTest, TwoFingerTouch) {
  SwitchTouchExplorationMode(true);
  aura::test::EventGenerator generator(root_window());
  generator.PressTouchId(1);
  ClearCapturedEvents();

  // Confirm events from the second finger go through as is.
  cursor_client()->ShowCursor();
  cursor_client()->DisableMouseEvents();
  ui::TouchEvent touch_press(ui::ET_TOUCH_PRESSED,
                             gfx::Point(10, 11),
                             2,
                             ui::EventTimeForNow());
  generator.Dispatch(&touch_press);
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
  ui::TouchEvent touch_move(ui::ET_TOUCH_MOVED,
                            gfx::Point(20, 21),
                            2,
                            ui::EventTimeForNow());
  generator.Dispatch(&touch_move);
  ASSERT_EQ(1u, captured_events.size());
  CONFIRM_EVENTS_ARE_TOUCH_AND_EQUAL(captured_events[0], &touch_move);
  ClearCapturedEvents();

  // Confirm mouse moves go through unaffected.
  ui::MouseEvent mouse_move(ui::ET_MOUSE_MOVED,
                            gfx::Point(13, 14),
                            gfx::Point(13, 14),
                            0,
                            0);
  generator.Dispatch(&mouse_move);
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

  // Have some other fingers touch/move/release
  generator.PressTouchId(3);
  generator.PressTouchId(4);
  generator.MoveTouchId(gfx::Point(30, 31), 3);
  generator.ReleaseTouchId(3);
  generator.ReleaseTouchId(4);
  ClearCapturedEvents();

  // Events from the first finger should not go through while the second finger
  // is touching.
  gfx::Point touch1_location = gfx::Point(15, 16);
  generator.MoveTouchId(touch1_location, 1);
  EXPECT_EQ(0u, GetCapturedEvents().size());

  EXPECT_TRUE(cursor_client()->IsCursorVisible());
  EXPECT_FALSE(cursor_client()->IsMouseEventsEnabled());

  // A release of the second finger should go through, plus there should be a
  // mouse move at |touch1_location| generated.
  ui::TouchEvent touch_release(ui::ET_TOUCH_RELEASED,
                               gfx::Point(25, 26),
                               2,
                               ui::EventTimeForNow());
  generator.Dispatch(&touch_release);
  EXPECT_TRUE(IsInTouchToMouseMode());
  ASSERT_GE(captured_events.size(), 2u);
  CONFIRM_EVENTS_ARE_TOUCH_AND_EQUAL(captured_events[0], &touch_release);
  // TODO(mfomitchev): mouse enter/exit events
  // Ignore synthesized ET_MOUSE_ENTERED/ET_MOUSE_EXITED
  for (it = captured_events.begin(); it != captured_events.end(); ++it) {
    if ((*it)->type() == ui::ET_MOUSE_MOVED) {
      EXPECT_EQ(touch1_location, (*it)->location());
      break;
    }
  }
  EXPECT_NE(captured_events.end(), it);
}

TEST_F(TouchExplorationTest, MultiFingerTouch) {
  SwitchTouchExplorationMode(true);
  aura::test::EventGenerator generator(root_window());
  generator.PressTouchId(1);
  generator.PressTouchId(2);
  ClearCapturedEvents();

  // Confirm events from other fingers go through as is.
  ui::TouchEvent touch3_press(ui::ET_TOUCH_PRESSED,
                              gfx::Point(10, 11),
                              3,
                              ui::EventTimeForNow());
  ui::TouchEvent touch3_move1(ui::ET_TOUCH_MOVED,
                              gfx::Point(12, 13),
                              3,
                              ui::EventTimeForNow());
  ui::TouchEvent touch4_press(ui::ET_TOUCH_PRESSED,
                              gfx::Point(20, 21),
                              4,
                              ui::EventTimeForNow());
  ui::TouchEvent touch3_move2(ui::ET_TOUCH_MOVED,
                              gfx::Point(14, 15),
                              3,
                              ui::EventTimeForNow());
  ui::TouchEvent touch4_move(ui::ET_TOUCH_MOVED,
                             gfx::Point(22, 23),
                             4,
                             ui::EventTimeForNow());
  ui::TouchEvent touch3_release(ui::ET_TOUCH_RELEASED,
                                gfx::Point(14, 15),
                                3,
                                ui::EventTimeForNow());
  ui::TouchEvent touch4_release(ui::ET_TOUCH_RELEASED,
                                gfx::Point(22, 23),
                                4,
                                ui::EventTimeForNow());
  generator.Dispatch(&touch3_press);
  generator.Dispatch(&touch3_move1);
  generator.Dispatch(&touch4_press);
  generator.Dispatch(&touch3_move2);
  generator.Dispatch(&touch4_move);
  generator.Dispatch(&touch3_release);
  generator.Dispatch(&touch4_release);

  const ScopedVector<ui::LocatedEvent>& captured_events = GetCapturedEvents();
  ASSERT_EQ(7u, captured_events.size());
  CONFIRM_EVENTS_ARE_TOUCH_AND_EQUAL(captured_events[0], &touch3_press);
  CONFIRM_EVENTS_ARE_TOUCH_AND_EQUAL(captured_events[1], &touch3_move1);
  CONFIRM_EVENTS_ARE_TOUCH_AND_EQUAL(captured_events[2], &touch4_press);
  CONFIRM_EVENTS_ARE_TOUCH_AND_EQUAL(captured_events[3], &touch3_move2);
  CONFIRM_EVENTS_ARE_TOUCH_AND_EQUAL(captured_events[4], &touch4_move);
  CONFIRM_EVENTS_ARE_TOUCH_AND_EQUAL(captured_events[5], &touch3_release);
  CONFIRM_EVENTS_ARE_TOUCH_AND_EQUAL(captured_events[6], &touch4_release);
}

// Test the case when there are multiple fingers on the screen and the first
// finger is released. This should be rewritten as a release of the second
// finger. Additionally, if the second finger is the only finger left touching,
// we should enter a mouse move mode, and a mouse move event should be
// dispatched.
TEST_F(TouchExplorationTest, FirstFingerLifted) {
  SwitchTouchExplorationMode(true);
  aura::test::EventGenerator generator(root_window());
  generator.PressTouchId(1);
  generator.PressTouchId(2);
  gfx::Point touch2_location(10, 11);
  generator.MoveTouchId(touch2_location, 2);
  generator.PressTouchId(3);
  gfx::Point touch3_location(20, 21);
  generator.MoveTouchId(touch3_location, 3);
  ClearCapturedEvents();

  // Release of finger 1 should be rewritten as a release of finger 2.
  generator.ReleaseTouchId(1);
  const ScopedVector<ui::LocatedEvent>& captured_events = GetCapturedEvents();
  ASSERT_EQ(1u, captured_events.size());
  EXPECT_EQ(ui::ET_TOUCH_RELEASED, captured_events[0]->type());
  ui::TouchEvent* touch_event =
      static_cast<ui::TouchEvent*>(captured_events[0]);
  EXPECT_EQ(2, touch_event->touch_id());
  EXPECT_EQ(touch2_location, touch_event->location());
  ClearCapturedEvents();

  // Release of finger 2 should be rewritten as a release of finger 3, plus
  // we should enter the mouse move mode and a mouse move event should be
  // dispatched.
  cursor_client()->ShowCursor();
  cursor_client()->DisableMouseEvents();
  generator.ReleaseTouchId(2);
  EXPECT_TRUE(IsInTouchToMouseMode());
  ASSERT_GE(2u, captured_events.size());
  EXPECT_EQ(ui::ET_TOUCH_RELEASED, captured_events[0]->type());
  touch_event = static_cast<ui::TouchEvent*>(captured_events[0]);
  EXPECT_EQ(3, touch_event->touch_id());
  EXPECT_EQ(touch3_location, touch_event->location());
  // TODO(mfomitchev): mouse enter/exit events
  ScopedVector<ui::LocatedEvent>::const_iterator it;
  for (it = captured_events.begin(); it != captured_events.end(); ++it) {
    if ((*it)->type() == ui::ET_MOUSE_MOVED) {
      EXPECT_EQ(touch3_location, (*it)->location());
      break;
    }
  }
  EXPECT_NE(captured_events.end(), it);
}

}  // namespace ui
