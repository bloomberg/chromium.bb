// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura_shell/desktop_event_filter.h"

#include "ui/aura/cursor.h"
#include "ui/aura/desktop.h"
#include "ui/aura/event.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/test/event_generator.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/test/test_stacking_client.h"
#include "ui/aura_shell/shell_window_ids.h"
#include "ui/base/hit_test.h"

namespace aura_shell {
namespace test {

class DefaultEventFilterTest : public aura::test::AuraTestBase {
 public:
  DefaultEventFilterTest() {
    aura::Desktop::GetInstance()->SetEventFilter(
        new internal::DesktopEventFilter);

    aura::test::TestStackingClient* stacking_client =
        static_cast<aura::test::TestStackingClient*>(
            aura::Desktop::GetInstance()->stacking_client());
    stacking_client->default_container()->set_id(
        internal::kShellWindowId_DefaultContainer);
  }
  virtual ~DefaultEventFilterTest() {
    aura::Desktop::GetInstance()->SetEventFilter(NULL);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DefaultEventFilterTest);
};

class HitTestWindowDelegate : public aura::test::TestWindowDelegate {
 public:
  HitTestWindowDelegate()
      : hittest_code_(HTNOWHERE) {
  }
  virtual ~HitTestWindowDelegate() {}
  void set_hittest_code(int hittest_code) { hittest_code_ = hittest_code; }

 private:
  // Overridden from TestWindowDelegate:
  virtual int GetNonClientComponent(const gfx::Point& point) const OVERRIDE {
    return hittest_code_;
  }

  int hittest_code_;

  DISALLOW_COPY_AND_ASSIGN(HitTestWindowDelegate);
};

TEST_F(DefaultEventFilterTest, Focus) {
  aura::Desktop* desktop = aura::Desktop::GetInstance();
  desktop->SetBounds(gfx::Rect(0, 0, 510, 510));

  // Supplied ids are negative so as not to collide with shell ids.
  // TODO(beng): maybe introduce a MAKE_SHELL_ID() macro that generates a safe
  //             id beyond shell id max?
  scoped_ptr<aura::Window> w1(aura::test::CreateTestWindow(
      SK_ColorWHITE, -1, gfx::Rect(10, 10, 500, 500), NULL));
  scoped_ptr<aura::Window> w11(aura::test::CreateTestWindow(
      SK_ColorGREEN, -11, gfx::Rect(5, 5, 100, 100), w1.get()));
  scoped_ptr<aura::Window> w111(aura::test::CreateTestWindow(
      SK_ColorCYAN, -111, gfx::Rect(5, 5, 75, 75), w11.get()));
  scoped_ptr<aura::Window> w1111(aura::test::CreateTestWindow(
      SK_ColorRED, -1111, gfx::Rect(5, 5, 50, 50), w111.get()));
  scoped_ptr<aura::Window> w12(aura::test::CreateTestWindow(
      SK_ColorMAGENTA, -12, gfx::Rect(10, 420, 25, 25), w1.get()));
  aura::test::ColorTestWindowDelegate* w121delegate =
      new aura::test::ColorTestWindowDelegate(SK_ColorYELLOW);
  scoped_ptr<aura::Window> w121(aura::test::CreateTestWindowWithDelegate(
      w121delegate, -121, gfx::Rect(5, 5, 5, 5), w12.get()));
  aura::test::ColorTestWindowDelegate* w122delegate =
      new aura::test::ColorTestWindowDelegate(SK_ColorRED);
  scoped_ptr<aura::Window> w122(aura::test::CreateTestWindowWithDelegate(
      w122delegate, -122, gfx::Rect(10, 5, 5, 5), w12.get()));
  scoped_ptr<aura::Window> w13(aura::test::CreateTestWindow(
      SK_ColorGRAY, -13, gfx::Rect(5, 470, 50, 50), w1.get()));

  // Click on a sub-window (w121) to focus it.
  gfx::Point click_point = w121->bounds().CenterPoint();
  aura::Window::ConvertPointToWindow(w121->parent(), desktop, &click_point);
  aura::MouseEvent mouse(ui::ET_MOUSE_PRESSED, click_point,
                         ui::EF_LEFT_BUTTON_DOWN);
  desktop->DispatchMouseEvent(&mouse);
  aura::internal::FocusManager* focus_manager = w121->GetFocusManager();
  EXPECT_EQ(w121.get(), focus_manager->GetFocusedWindow());

  // The key press should be sent to the focused sub-window.
  aura::KeyEvent keyev(ui::ET_KEY_PRESSED, ui::VKEY_E, 0);
  desktop->DispatchKeyEvent(&keyev);
  EXPECT_EQ(ui::VKEY_E, w121delegate->last_key_code());

  // Touch on a sub-window (w122) to focus it.
  click_point = w122->bounds().CenterPoint();
  aura::Window::ConvertPointToWindow(w122->parent(), desktop, &click_point);
  aura::TouchEvent touchev(ui::ET_TOUCH_PRESSED, click_point, 0);
  desktop->DispatchTouchEvent(&touchev);
  focus_manager = w122->GetFocusManager();
  EXPECT_EQ(w122.get(), focus_manager->GetFocusedWindow());

  // The key press should be sent to the focused sub-window.
  desktop->DispatchKeyEvent(&keyev);
  EXPECT_EQ(ui::VKEY_E, w122delegate->last_key_code());

  // Removing the focused window from parent should reset the focused window.
  w12->RemoveChild(w122.get());
  EXPECT_EQ(NULL, w122->GetFocusManager());
  EXPECT_EQ(NULL, w12->GetFocusManager()->GetFocusedWindow());
  EXPECT_FALSE(desktop->DispatchKeyEvent(&keyev));
}

// Various assertion testing for activating windows.
TEST_F(DefaultEventFilterTest, ActivateOnMouse) {
  aura::Desktop* desktop = aura::Desktop::GetInstance();

  aura::test::ActivateWindowDelegate d1;
  scoped_ptr<aura::Window> w1(aura::test::CreateTestWindowWithDelegate(
      &d1, 1, gfx::Rect(10, 10, 50, 50), NULL));
  aura::test::ActivateWindowDelegate d2;
  scoped_ptr<aura::Window> w2(aura::test::CreateTestWindowWithDelegate(
      &d2, 2, gfx::Rect(70, 70, 50, 50), NULL));
  aura::internal::FocusManager* focus_manager = w1->GetFocusManager();

  d1.Clear();
  d2.Clear();

  // Activate window1.
  desktop->SetActiveWindow(w1.get(), NULL);
  EXPECT_EQ(w1.get(), desktop->active_window());
  EXPECT_EQ(w1.get(), focus_manager->GetFocusedWindow());
  EXPECT_EQ(1, d1.activated_count());
  EXPECT_EQ(0, d1.lost_active_count());
  d1.Clear();

  // Click on window2.
  gfx::Point press_point = w2->bounds().CenterPoint();
  aura::Window::ConvertPointToWindow(w2->parent(), desktop, &press_point);
  aura::test::EventGenerator generator(press_point);
  generator.ClickLeftButton();

  // Window2 should have become active.
  EXPECT_EQ(w2.get(), desktop->active_window());
  EXPECT_EQ(w2.get(), focus_manager->GetFocusedWindow());
  EXPECT_EQ(0, d1.activated_count());
  EXPECT_EQ(1, d1.lost_active_count());
  EXPECT_EQ(1, d2.activated_count());
  EXPECT_EQ(0, d2.lost_active_count());
  d1.Clear();
  d2.Clear();

  // Click back on window1, but set it up so w1 doesn't activate on click.
  press_point = w1->bounds().CenterPoint();
  aura::Window::ConvertPointToWindow(w1->parent(), desktop, &press_point);
  d1.set_activate(false);
  generator.ClickLeftButton();

  // Window2 should still be active and focused.
  EXPECT_EQ(w2.get(), desktop->active_window());
  EXPECT_EQ(w2.get(), focus_manager->GetFocusedWindow());
  EXPECT_EQ(0, d1.activated_count());
  EXPECT_EQ(0, d1.lost_active_count());
  EXPECT_EQ(0, d2.activated_count());
  EXPECT_EQ(0, d2.lost_active_count());
  d1.Clear();
  d2.Clear();

  // Destroy window2, this should make window1 active.
  d1.set_activate(true);
  w2.reset();
  EXPECT_EQ(0, d2.activated_count());
  EXPECT_EQ(0, d2.lost_active_count());
  EXPECT_EQ(w1.get(), desktop->active_window());
  EXPECT_EQ(w1.get(), focus_manager->GetFocusedWindow());
  EXPECT_EQ(1, d1.activated_count());
  EXPECT_EQ(0, d1.lost_active_count());
}

// Essentially the same as ActivateOnMouse, but for touch events.
TEST_F(DefaultEventFilterTest, ActivateOnTouch) {
  aura::Desktop* desktop = aura::Desktop::GetInstance();

  aura::test::ActivateWindowDelegate d1;
  scoped_ptr<aura::Window> w1(aura::test::CreateTestWindowWithDelegate(
      &d1, -1, gfx::Rect(10, 10, 50, 50), NULL));
  aura::test::ActivateWindowDelegate d2;
  scoped_ptr<aura::Window> w2(aura::test::CreateTestWindowWithDelegate(
      &d2, -2, gfx::Rect(70, 70, 50, 50), NULL));
  aura::internal::FocusManager* focus_manager = w1->GetFocusManager();

  d1.Clear();
  d2.Clear();

  // Activate window1.
  desktop->SetActiveWindow(w1.get(), NULL);
  EXPECT_EQ(w1.get(), desktop->active_window());
  EXPECT_EQ(w1.get(), focus_manager->GetFocusedWindow());
  EXPECT_EQ(1, d1.activated_count());
  EXPECT_EQ(0, d1.lost_active_count());
  d1.Clear();

  // Touch window2.
  gfx::Point press_point = w2->bounds().CenterPoint();
  aura::Window::ConvertPointToWindow(w2->parent(), desktop, &press_point);
  aura::TouchEvent touchev1(ui::ET_TOUCH_PRESSED, press_point, 0);
  desktop->DispatchTouchEvent(&touchev1);

  // Window2 should have become active.
  EXPECT_EQ(w2.get(), desktop->active_window());
  EXPECT_EQ(w2.get(), focus_manager->GetFocusedWindow());
  EXPECT_EQ(0, d1.activated_count());
  EXPECT_EQ(1, d1.lost_active_count());
  EXPECT_EQ(1, d2.activated_count());
  EXPECT_EQ(0, d2.lost_active_count());
  d1.Clear();
  d2.Clear();

  // Touch window1, but set it up so w1 doesn't activate on touch.
  press_point = w1->bounds().CenterPoint();
  aura::Window::ConvertPointToWindow(w1->parent(), desktop, &press_point);
  d1.set_activate(false);
  aura::TouchEvent touchev2(ui::ET_TOUCH_PRESSED, press_point, 0);
  desktop->DispatchTouchEvent(&touchev2);

  // Window2 should still be active and focused.
  EXPECT_EQ(w2.get(), desktop->active_window());
  EXPECT_EQ(w2.get(), focus_manager->GetFocusedWindow());
  EXPECT_EQ(0, d1.activated_count());
  EXPECT_EQ(0, d1.lost_active_count());
  EXPECT_EQ(0, d2.activated_count());
  EXPECT_EQ(0, d2.lost_active_count());
  d1.Clear();
  d2.Clear();

  // Destroy window2, this should make window1 active.
  d1.set_activate(true);
  w2.reset();
  EXPECT_EQ(0, d2.activated_count());
  EXPECT_EQ(0, d2.lost_active_count());
  EXPECT_EQ(w1.get(), desktop->active_window());
  EXPECT_EQ(w1.get(), focus_manager->GetFocusedWindow());
  EXPECT_EQ(1, d1.activated_count());
  EXPECT_EQ(0, d1.lost_active_count());
}

TEST_F(DefaultEventFilterTest, MouseEventCursors) {
  aura::Desktop* desktop = aura::Desktop::GetInstance();

  // Create a window.
  const int kWindowLeft = 123;
  const int kWindowTop = 45;
  HitTestWindowDelegate window_delegate;
  scoped_ptr<aura::Window> window(aura::test::CreateTestWindowWithDelegate(
    &window_delegate,
    -1,
    gfx::Rect(kWindowLeft, kWindowTop, 640, 480),
    NULL));

  // Create two mouse movement events we can switch between.
  gfx::Point point1(kWindowLeft, kWindowTop);
  aura::Window::ConvertPointToWindow(window->parent(), desktop, &point1);
  aura::MouseEvent move1(ui::ET_MOUSE_MOVED, point1, 0x0);

  gfx::Point point2(kWindowLeft + 1, kWindowTop + 1);
  aura::Window::ConvertPointToWindow(window->parent(), desktop, &point2);
  aura::MouseEvent move2(ui::ET_MOUSE_MOVED, point2, 0x0);

  // Cursor starts as null.
  EXPECT_EQ(aura::kCursorNull, desktop->last_cursor());

  // Resize edges and corners show proper cursors.
  window_delegate.set_hittest_code(HTBOTTOM);
  desktop->DispatchMouseEvent(&move1);
  EXPECT_EQ(aura::kCursorSouthResize, desktop->last_cursor());

  window_delegate.set_hittest_code(HTBOTTOMLEFT);
  desktop->DispatchMouseEvent(&move2);
  EXPECT_EQ(aura::kCursorSouthWestResize, desktop->last_cursor());

  window_delegate.set_hittest_code(HTBOTTOMRIGHT);
  desktop->DispatchMouseEvent(&move1);
  EXPECT_EQ(aura::kCursorSouthEastResize, desktop->last_cursor());

  window_delegate.set_hittest_code(HTLEFT);
  desktop->DispatchMouseEvent(&move2);
  EXPECT_EQ(aura::kCursorWestResize, desktop->last_cursor());

  window_delegate.set_hittest_code(HTRIGHT);
  desktop->DispatchMouseEvent(&move1);
  EXPECT_EQ(aura::kCursorEastResize, desktop->last_cursor());

  window_delegate.set_hittest_code(HTTOP);
  desktop->DispatchMouseEvent(&move2);
  EXPECT_EQ(aura::kCursorNorthResize, desktop->last_cursor());

  window_delegate.set_hittest_code(HTTOPLEFT);
  desktop->DispatchMouseEvent(&move1);
  EXPECT_EQ(aura::kCursorNorthWestResize, desktop->last_cursor());

  window_delegate.set_hittest_code(HTTOPRIGHT);
  desktop->DispatchMouseEvent(&move2);
  EXPECT_EQ(aura::kCursorNorthEastResize, desktop->last_cursor());

  // Client area uses null cursor.
  window_delegate.set_hittest_code(HTCLIENT);
  desktop->DispatchMouseEvent(&move1);
  EXPECT_EQ(aura::kCursorNull, desktop->last_cursor());
}

}  // namespace test
}  // namespace aura_shell
