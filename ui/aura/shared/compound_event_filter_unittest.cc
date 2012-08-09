// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/shared/compound_event_filter.h"

#include "ui/aura/client/activation_client.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/env.h"
#include "ui/aura/root_window.h"
#include "ui/aura/shared/root_window_capture_client.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/test/test_activation_client.h"
#include "ui/aura/test/test_windows.h"
#include "ui/base/event.h"

namespace {
base::TimeDelta GetTime() {
  return base::Time::NowFromSystemTime() - base::Time();
}

class TestVisibleClient : public aura::client::CursorClient {
 public:
  TestVisibleClient() : visible_(false) {}
  virtual ~TestVisibleClient() {}

  virtual void SetCursor(gfx::NativeCursor cursor) OVERRIDE {
  }

  virtual void ShowCursor(bool show) OVERRIDE {
    visible_ = show;
  }

  virtual bool IsCursorVisible() const OVERRIDE {
    return visible_;
  }

 private:
  bool visible_;
};

}

namespace aura {
namespace test {

typedef AuraTestBase CompoundEventFilterTest;

TEST_F(CompoundEventFilterTest, TouchHidesCursor) {
  aura::Env::GetInstance()->SetEventFilter(new shared::CompoundEventFilter());
  aura::client::SetActivationClient(root_window(),
                                    new TestActivationClient(root_window()));
  aura::client::SetCaptureClient(
      root_window(), new shared::RootWindowCaptureClient(root_window()));
  TestWindowDelegate delegate;
  scoped_ptr<Window> window(CreateTestWindowWithDelegate(&delegate, 1234,
      gfx::Rect(5, 5, 100, 100), NULL));
  window->Show();
  window->SetCapture();

  TestVisibleClient cursor_client;
  aura::client::SetCursorClient(root_window(), &cursor_client);

  ui::MouseEvent mouse(ui::ET_MOUSE_MOVED, gfx::Point(10, 10),
      gfx::Point(10, 10), 0);
  root_window()->AsRootWindowHostDelegate()->OnHostMouseEvent(&mouse);
  EXPECT_TRUE(cursor_client.IsCursorVisible());

  // This press is required for the GestureRecognizer to associate a target
  // with kTouchId
  ui::TouchEvent press(
      ui::ET_TOUCH_PRESSED, gfx::Point(90, 90), 1, GetTime());
  root_window()->AsRootWindowHostDelegate()->OnHostTouchEvent(&press);
  EXPECT_FALSE(cursor_client.IsCursorVisible());

  ui::TouchEvent move(ui::ET_TOUCH_MOVED, gfx::Point(10, 10), 1, GetTime());
  root_window()->AsRootWindowHostDelegate()->OnHostTouchEvent(&move);
  EXPECT_FALSE(cursor_client.IsCursorVisible());

  ui::TouchEvent release(
      ui::ET_TOUCH_RELEASED, gfx::Point(10, 10), 1, GetTime());
  root_window()->AsRootWindowHostDelegate()->OnHostTouchEvent(&release);
  EXPECT_FALSE(cursor_client.IsCursorVisible());

  // Move the cursor again. The cursor should be visible.
  root_window()->AsRootWindowHostDelegate()->OnHostMouseEvent(&mouse);
  EXPECT_TRUE(cursor_client.IsCursorVisible());

  // Now activate the window and press on it again.
  aura::client::GetActivationClient(
      root_window())->ActivateWindow(window.get());
  root_window()->AsRootWindowHostDelegate()->OnHostTouchEvent(&press);
  EXPECT_FALSE(cursor_client.IsCursorVisible());
}

}  // namespace test
}  // namespace aura
