// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/desktop.h"

#include "ui/aura/event.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/base/events.h"
#include "ui/base/hit_test.h"
#include "ui/gfx/point.h"

namespace aura {
namespace test {

namespace {

Window* CreateTestWindowWithDelegate(WindowDelegate* delegate,
                                     const gfx::Rect& bounds,
                                     Window* parent) {
  Window* window = new Window(delegate);
  window->Init(ui::Layer::LAYER_HAS_TEXTURE);
  window->SetBounds(bounds);
  window->Show();
  window->SetParent(parent);
  return window;
}

class HitTestWindowDelegate : public TestWindowDelegate {
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

}  // namespace

class DesktopTest : public AuraTestBase {
 public:
  DesktopTest() {}
  virtual ~DesktopTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(DesktopTest);
};

TEST_F(DesktopTest, MouseEventCursors) {
  Desktop* desktop = Desktop::GetInstance();

  // Create a window.
  const int kWindowLeft = 123;
  const int kWindowTop = 45;
  HitTestWindowDelegate window_delegate;
  scoped_ptr<Window> window(CreateTestWindowWithDelegate(
      &window_delegate,
      gfx::Rect(kWindowLeft, kWindowTop, 640, 480),
      NULL));

  // Create two mouse movement events we can switch between.
  gfx::Point point1(kWindowLeft, kWindowTop);
  Window::ConvertPointToWindow(window->parent(), desktop, &point1);
  MouseEvent move1(ui::ET_MOUSE_MOVED, point1, 0x0);

  gfx::Point point2(kWindowLeft + 1, kWindowTop + 1);
  Window::ConvertPointToWindow(window->parent(), desktop, &point2);
  MouseEvent move2(ui::ET_MOUSE_MOVED, point2, 0x0);

  // Cursor starts as null.
  EXPECT_EQ(kCursorNull, desktop->last_cursor());

  // Resize edges and corners show proper cursors.
  window_delegate.set_hittest_code(HTBOTTOM);
  desktop->DispatchMouseEvent(&move1);
  EXPECT_EQ(kCursorSouthResize, desktop->last_cursor());

  window_delegate.set_hittest_code(HTBOTTOMLEFT);
  desktop->DispatchMouseEvent(&move2);
  EXPECT_EQ(kCursorSouthWestResize, desktop->last_cursor());

  window_delegate.set_hittest_code(HTBOTTOMRIGHT);
  desktop->DispatchMouseEvent(&move1);
  EXPECT_EQ(kCursorSouthEastResize, desktop->last_cursor());

  window_delegate.set_hittest_code(HTLEFT);
  desktop->DispatchMouseEvent(&move2);
  EXPECT_EQ(kCursorWestResize, desktop->last_cursor());

  window_delegate.set_hittest_code(HTRIGHT);
  desktop->DispatchMouseEvent(&move1);
  EXPECT_EQ(kCursorEastResize, desktop->last_cursor());

  window_delegate.set_hittest_code(HTTOP);
  desktop->DispatchMouseEvent(&move2);
  EXPECT_EQ(kCursorNorthResize, desktop->last_cursor());

  window_delegate.set_hittest_code(HTTOPLEFT);
  desktop->DispatchMouseEvent(&move1);
  EXPECT_EQ(kCursorNorthWestResize, desktop->last_cursor());

  window_delegate.set_hittest_code(HTTOPRIGHT);
  desktop->DispatchMouseEvent(&move2);
  EXPECT_EQ(kCursorNorthEastResize, desktop->last_cursor());

  // Client area uses null cursor.
  window_delegate.set_hittest_code(HTCLIENT);
  desktop->DispatchMouseEvent(&move1);
  EXPECT_EQ(kCursorNull, desktop->last_cursor());
}

}  // namespace test
}  // namespace aura
