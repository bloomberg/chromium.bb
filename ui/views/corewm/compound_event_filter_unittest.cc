// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/corewm/compound_event_filter.h"

#include "ui/aura/client/activation_client.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/env.h"
#include "ui/aura/root_window.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/test/event_generator.h"
#include "ui/aura/test/test_cursor_client.h"
#include "ui/aura/test/test_windows.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"

namespace {

base::TimeDelta GetTime() {
  return ui::EventTimeForNow();
}

}

namespace views {
namespace corewm {

namespace {

// An event filter that consumes all gesture events.
class ConsumeGestureEventFilter : public ui::EventHandler {
 public:
  ConsumeGestureEventFilter() {}
  virtual ~ConsumeGestureEventFilter() {}

 private:
  // Overridden from ui::EventHandler:
  virtual void OnGestureEvent(ui::GestureEvent* e) OVERRIDE {
    e->StopPropagation();
  }

  DISALLOW_COPY_AND_ASSIGN(ConsumeGestureEventFilter);
};

}  // namespace

typedef aura::test::AuraTestBase CompoundEventFilterTest;

#if defined(OS_CHROMEOS)
// A keypress only hides the cursor on ChromeOS (crbug.com/304296).
TEST_F(CompoundEventFilterTest, CursorVisibilityChange) {
  scoped_ptr<CompoundEventFilter> compound_filter(new CompoundEventFilter);
  aura::Env::GetInstance()->AddPreTargetHandler(compound_filter.get());
  aura::test::TestWindowDelegate delegate;
  scoped_ptr<aura::Window> window(CreateTestWindowWithDelegate(&delegate, 1234,
      gfx::Rect(5, 5, 100, 100), root_window()));
  window->Show();
  window->SetCapture();

  aura::test::TestCursorClient cursor_client(root_window());

  // Send key event to hide the cursor.
  ui::KeyEvent key(ui::ET_KEY_PRESSED, ui::VKEY_A, 0, true);
  root_window()->AsRootWindowHostDelegate()->OnHostKeyEvent(&key);
  EXPECT_FALSE(cursor_client.IsCursorVisible());

  // Synthesized mouse event should not show the cursor.
  ui::MouseEvent enter(ui::ET_MOUSE_ENTERED, gfx::Point(10, 10),
                       gfx::Point(10, 10), 0);
  enter.set_flags(enter.flags() | ui::EF_IS_SYNTHESIZED);
  root_window()->AsRootWindowHostDelegate()->OnHostMouseEvent(&enter);
  EXPECT_FALSE(cursor_client.IsCursorVisible());

  ui::MouseEvent move(ui::ET_MOUSE_MOVED, gfx::Point(10, 10),
                      gfx::Point(10, 10), 0);
  move.set_flags(enter.flags() | ui::EF_IS_SYNTHESIZED);
  root_window()->AsRootWindowHostDelegate()->OnHostMouseEvent(&move);
  EXPECT_FALSE(cursor_client.IsCursorVisible());

  ui::MouseEvent real_move(ui::ET_MOUSE_MOVED, gfx::Point(10, 10),
                           gfx::Point(10, 10), 0);
  root_window()->AsRootWindowHostDelegate()->OnHostMouseEvent(&real_move);
  EXPECT_TRUE(cursor_client.IsCursorVisible());

  // Send key event to hide the cursor again.
  root_window()->AsRootWindowHostDelegate()->OnHostKeyEvent(&key);
  EXPECT_FALSE(cursor_client.IsCursorVisible());

  // Mouse synthesized exit event should not show the cursor.
  ui::MouseEvent exit(ui::ET_MOUSE_EXITED, gfx::Point(10, 10),
                      gfx::Point(10, 10), 0);
  exit.set_flags(enter.flags() | ui::EF_IS_SYNTHESIZED);
  root_window()->AsRootWindowHostDelegate()->OnHostMouseEvent(&exit);
  EXPECT_FALSE(cursor_client.IsCursorVisible());
}
#endif

TEST_F(CompoundEventFilterTest, TouchHidesCursor) {
  scoped_ptr<CompoundEventFilter> compound_filter(new CompoundEventFilter);
  aura::Env::GetInstance()->AddPreTargetHandler(compound_filter.get());
  aura::test::TestWindowDelegate delegate;
  scoped_ptr<aura::Window> window(CreateTestWindowWithDelegate(&delegate, 1234,
      gfx::Rect(5, 5, 100, 100), root_window()));
  window->Show();
  window->SetCapture();

  aura::test::TestCursorClient cursor_client(root_window());

  ui::MouseEvent mouse0(ui::ET_MOUSE_MOVED, gfx::Point(10, 10),
                        gfx::Point(10, 10), 0);
  root_window()->AsRootWindowHostDelegate()->OnHostMouseEvent(&mouse0);
  EXPECT_TRUE(cursor_client.IsMouseEventsEnabled());

  // This press is required for the GestureRecognizer to associate a target
  // with kTouchId
  ui::TouchEvent press0(
      ui::ET_TOUCH_PRESSED, gfx::Point(90, 90), 1, GetTime());
  root_window()->AsRootWindowHostDelegate()->OnHostTouchEvent(&press0);
  EXPECT_FALSE(cursor_client.IsMouseEventsEnabled());

  ui::TouchEvent move(ui::ET_TOUCH_MOVED, gfx::Point(10, 10), 1, GetTime());
  root_window()->AsRootWindowHostDelegate()->OnHostTouchEvent(&move);
  EXPECT_FALSE(cursor_client.IsMouseEventsEnabled());

  ui::TouchEvent release(
      ui::ET_TOUCH_RELEASED, gfx::Point(10, 10), 1, GetTime());
  root_window()->AsRootWindowHostDelegate()->OnHostTouchEvent(&release);
  EXPECT_FALSE(cursor_client.IsMouseEventsEnabled());

  ui::MouseEvent mouse1(ui::ET_MOUSE_MOVED, gfx::Point(10, 10),
                        gfx::Point(10, 10), 0);
  // Move the cursor again. The cursor should be visible.
  root_window()->AsRootWindowHostDelegate()->OnHostMouseEvent(&mouse1);
  EXPECT_TRUE(cursor_client.IsMouseEventsEnabled());

  // Now activate the window and press on it again.
  ui::TouchEvent press1(
      ui::ET_TOUCH_PRESSED, gfx::Point(90, 90), 1, GetTime());
  aura::client::GetActivationClient(
      root_window())->ActivateWindow(window.get());
  root_window()->AsRootWindowHostDelegate()->OnHostTouchEvent(&press1);
  EXPECT_FALSE(cursor_client.IsMouseEventsEnabled());
  aura::Env::GetInstance()->RemovePreTargetHandler(compound_filter.get());
}

// Tests that if an event filter consumes a gesture, then it doesn't focus the
// window.
TEST_F(CompoundEventFilterTest, FilterConsumedGesture) {
  scoped_ptr<CompoundEventFilter> compound_filter(new CompoundEventFilter);
  scoped_ptr<ui::EventHandler> gesure_handler(new ConsumeGestureEventFilter);
  compound_filter->AddHandler(gesure_handler.get());
  aura::Env::GetInstance()->AddPreTargetHandler(compound_filter.get());
  aura::test::TestWindowDelegate delegate;
  DCHECK(root_window());
  scoped_ptr<aura::Window> window(CreateTestWindowWithDelegate(&delegate, 1234,
      gfx::Rect(5, 5, 100, 100), root_window()));
  window->Show();

  EXPECT_TRUE(window->CanFocus());
  EXPECT_FALSE(window->HasFocus());

  // Tap on the window should not focus it since the filter will be consuming
  // the gestures.
  aura::test::EventGenerator generator(root_window(), gfx::Point(50, 50));
  generator.PressTouch();
  EXPECT_FALSE(window->HasFocus());

  compound_filter->RemoveHandler(gesure_handler.get());
  aura::Env::GetInstance()->AddPreTargetHandler(compound_filter.get());
}

// Verifies we don't attempt to hide the mouse when the mouse is down and a
// touch event comes in.
TEST_F(CompoundEventFilterTest, DontHideWhenMouseDown) {
  aura::test::EventGenerator event_generator(root_window());

  scoped_ptr<CompoundEventFilter> compound_filter(new CompoundEventFilter);
  aura::Env::GetInstance()->AddPreTargetHandler(compound_filter.get());
  aura::test::TestWindowDelegate delegate;
  scoped_ptr<aura::Window> window(CreateTestWindowWithDelegate(&delegate, 1234,
      gfx::Rect(5, 5, 100, 100), root_window()));
  window->Show();

  aura::test::TestCursorClient cursor_client(root_window());

  // Move and press the mouse over the window.
  event_generator.MoveMouseTo(10, 10);
  EXPECT_TRUE(cursor_client.IsMouseEventsEnabled());
  event_generator.PressLeftButton();
  EXPECT_TRUE(cursor_client.IsMouseEventsEnabled());
  EXPECT_TRUE(aura::Env::GetInstance()->IsMouseButtonDown());

  // Do a touch event. As the mouse button is down this should not disable mouse
  // events.
  event_generator.PressTouch();
  EXPECT_TRUE(cursor_client.IsMouseEventsEnabled());
  aura::Env::GetInstance()->RemovePreTargetHandler(compound_filter.get());
}

}  // namespace corewm
}  // namespace views
