// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/shared/compound_event_filter.h"

#include "ui/aura/client/activation_client.h"
#include "ui/aura/cursor_manager.h"
#include "ui/aura/env.h"
#include "ui/aura/root_window.h"
#include "ui/aura/shared/root_window_capture_client.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/test/test_activation_client.h"
#include "ui/aura/test/test_windows.h"

namespace {
base::TimeDelta GetTime() {
  return base::Time::NowFromSystemTime() - base::Time();
}
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
  CursorManager* cursor_manager = aura::Env::GetInstance()->cursor_manager();

  MouseEvent mouse(ui::ET_MOUSE_MOVED, gfx::Point(10, 10),
      gfx::Point(15, 15), 0);
  root_window()->DispatchMouseEvent(&mouse);
  EXPECT_TRUE(cursor_manager->cursor_visible());

  // This press is required for the GestureRecognizer to associate a target
  // with kTouchId
  TouchEvent press(ui::ET_TOUCH_PRESSED, gfx::Point(90, 90), 1, GetTime());
  root_window()->DispatchTouchEvent(&press);
  EXPECT_FALSE(cursor_manager->cursor_visible());

  TouchEvent move(ui::ET_TOUCH_MOVED, gfx::Point(10, 10), 1,
                  GetTime());
  root_window()->DispatchTouchEvent(&move);
  EXPECT_FALSE(cursor_manager->cursor_visible());

  TouchEvent release(ui::ET_TOUCH_RELEASED, gfx::Point(10, 10), 1, GetTime());
  root_window()->DispatchTouchEvent(&release);
  EXPECT_FALSE(cursor_manager->cursor_visible());

  root_window()->DispatchMouseEvent(&mouse);
  EXPECT_TRUE(cursor_manager->cursor_visible());
}

}  // namespace test
}  // namespace aura
