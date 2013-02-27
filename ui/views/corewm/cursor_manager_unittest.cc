// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/corewm/cursor_manager.h"

#include "ui/views/corewm/native_cursor_manager.h"
#include "ui/views/test/views_test_base.h"

namespace {

class TestingCursorManager : public views::corewm::NativeCursorManager {
 public:
  gfx::NativeCursor current_cursor() { return cursor_; }

  // Overridden from views::corewm::NativeCursorManager:
  virtual void SetDeviceScaleFactor(
      float device_scale_factor,
      views::corewm::NativeCursorManagerDelegate* delegate) OVERRIDE {}

  virtual void SetCursor(
      gfx::NativeCursor cursor,
      views::corewm::NativeCursorManagerDelegate* delegate) OVERRIDE {
    cursor_ = cursor;
    delegate->CommitCursor(cursor);
  }

  virtual void SetVisibility(
      bool visible,
      views::corewm::NativeCursorManagerDelegate* delegate) OVERRIDE {
    delegate->CommitVisibility(visible);
  }

  virtual void SetMouseEventsEnabled(
      bool enabled,
      views::corewm::NativeCursorManagerDelegate* delegate) OVERRIDE {
    delegate->CommitMouseEventsEnabled(enabled);
  }

 private:
  gfx::NativeCursor cursor_;
};

}  // namespace

class CursorManagerTest : public views::ViewsTestBase {
 protected:
  CursorManagerTest()
      : delegate_(new TestingCursorManager),
        cursor_manager_(scoped_ptr<views::corewm::NativeCursorManager>(
            delegate_)) {
  }

  gfx::NativeCursor current_cursor() { return delegate_->current_cursor(); }

  TestingCursorManager* delegate_;
  views::corewm::CursorManager cursor_manager_;
};

TEST_F(CursorManagerTest, ShowHideCursor) {
  cursor_manager_.SetCursor(ui::kCursorCopy);
  EXPECT_EQ(ui::kCursorCopy, current_cursor().native_type());

  cursor_manager_.ShowCursor();
  EXPECT_TRUE(cursor_manager_.IsCursorVisible());
  cursor_manager_.HideCursor();
  EXPECT_FALSE(cursor_manager_.IsCursorVisible());
  // The current cursor does not change even when the cursor is not shown.
  EXPECT_EQ(ui::kCursorCopy, current_cursor().native_type());

  // Check if cursor visibility is locked.
  cursor_manager_.LockCursor();
  EXPECT_FALSE(cursor_manager_.IsCursorVisible());
  cursor_manager_.ShowCursor();
  EXPECT_FALSE(cursor_manager_.IsCursorVisible());
  cursor_manager_.UnlockCursor();
  EXPECT_TRUE(cursor_manager_.IsCursorVisible());

  cursor_manager_.LockCursor();
  EXPECT_TRUE(cursor_manager_.IsCursorVisible());
  cursor_manager_.HideCursor();
  EXPECT_TRUE(cursor_manager_.IsCursorVisible());
  cursor_manager_.UnlockCursor();
  EXPECT_FALSE(cursor_manager_.IsCursorVisible());

  // Checks setting visiblity while cursor is locked does not affect the
  // subsequent uses of UnlockCursor.
  cursor_manager_.LockCursor();
  cursor_manager_.HideCursor();
  cursor_manager_.UnlockCursor();
  EXPECT_FALSE(cursor_manager_.IsCursorVisible());

  cursor_manager_.ShowCursor();
  cursor_manager_.LockCursor();
  cursor_manager_.UnlockCursor();
  EXPECT_TRUE(cursor_manager_.IsCursorVisible());

  cursor_manager_.LockCursor();
  cursor_manager_.ShowCursor();
  cursor_manager_.UnlockCursor();
  EXPECT_TRUE(cursor_manager_.IsCursorVisible());

  cursor_manager_.HideCursor();
  cursor_manager_.LockCursor();
  cursor_manager_.UnlockCursor();
  EXPECT_FALSE(cursor_manager_.IsCursorVisible());
}

// Verifies that LockCursor/UnlockCursor work correctly with
// EnableMouseEvents and DisableMouseEvents
TEST_F(CursorManagerTest, EnableDisableMouseEvents) {
  cursor_manager_.SetCursor(ui::kCursorCopy);
  EXPECT_EQ(ui::kCursorCopy, current_cursor().native_type());

  cursor_manager_.EnableMouseEvents();
  EXPECT_TRUE(cursor_manager_.IsMouseEventsEnabled());
  cursor_manager_.DisableMouseEvents();
  EXPECT_FALSE(cursor_manager_.IsMouseEventsEnabled());
  // The current cursor does not change even when the cursor is not shown.
  EXPECT_EQ(ui::kCursorCopy, current_cursor().native_type());

  // Check if cursor enable state is locked.
  cursor_manager_.LockCursor();
  EXPECT_FALSE(cursor_manager_.IsMouseEventsEnabled());
  cursor_manager_.EnableMouseEvents();
  EXPECT_FALSE(cursor_manager_.IsMouseEventsEnabled());
  cursor_manager_.UnlockCursor();
  EXPECT_TRUE(cursor_manager_.IsMouseEventsEnabled());

  cursor_manager_.LockCursor();
  EXPECT_TRUE(cursor_manager_.IsMouseEventsEnabled());
  cursor_manager_.DisableMouseEvents();
  EXPECT_TRUE(cursor_manager_.IsMouseEventsEnabled());
  cursor_manager_.UnlockCursor();
  EXPECT_FALSE(cursor_manager_.IsMouseEventsEnabled());

  // Checks enabling cursor while cursor is locked does not affect the
  // subsequent uses of UnlockCursor.
  cursor_manager_.LockCursor();
  cursor_manager_.DisableMouseEvents();
  cursor_manager_.UnlockCursor();
  EXPECT_FALSE(cursor_manager_.IsMouseEventsEnabled());

  cursor_manager_.EnableMouseEvents();
  cursor_manager_.LockCursor();
  cursor_manager_.UnlockCursor();
  EXPECT_TRUE(cursor_manager_.IsMouseEventsEnabled());

  cursor_manager_.LockCursor();
  cursor_manager_.EnableMouseEvents();
  cursor_manager_.UnlockCursor();
  EXPECT_TRUE(cursor_manager_.IsMouseEventsEnabled());

  cursor_manager_.DisableMouseEvents();
  cursor_manager_.LockCursor();
  cursor_manager_.UnlockCursor();
  EXPECT_FALSE(cursor_manager_.IsMouseEventsEnabled());
}

TEST_F(CursorManagerTest, IsMouseEventsEnabled) {
  cursor_manager_.EnableMouseEvents();
  EXPECT_TRUE(cursor_manager_.IsMouseEventsEnabled());
  cursor_manager_.DisableMouseEvents();
  EXPECT_FALSE(cursor_manager_.IsMouseEventsEnabled());
}

// Verifies that the mouse events enable state changes correctly when
// ShowCursor/HideCursor and EnableMouseEvents/DisableMouseEvents are used
// together.
TEST_F(CursorManagerTest, ShowAndEnable) {
  // Changing the visibility of the cursor does not affect the enable state.
  cursor_manager_.EnableMouseEvents();
  cursor_manager_.ShowCursor();
  EXPECT_TRUE(cursor_manager_.IsCursorVisible());
  EXPECT_TRUE(cursor_manager_.IsMouseEventsEnabled());
  cursor_manager_.HideCursor();
  EXPECT_FALSE(cursor_manager_.IsCursorVisible());
  EXPECT_TRUE(cursor_manager_.IsMouseEventsEnabled());
  cursor_manager_.ShowCursor();
  EXPECT_TRUE(cursor_manager_.IsCursorVisible());
  EXPECT_TRUE(cursor_manager_.IsMouseEventsEnabled());

  // When mouse events are disabled, it also gets invisible.
  EXPECT_TRUE(cursor_manager_.IsCursorVisible());
  cursor_manager_.DisableMouseEvents();
  EXPECT_FALSE(cursor_manager_.IsCursorVisible());
  EXPECT_FALSE(cursor_manager_.IsMouseEventsEnabled());

  // When mouse events are enabled, it restores the visibility state.
  cursor_manager_.EnableMouseEvents();
  EXPECT_TRUE(cursor_manager_.IsCursorVisible());
  EXPECT_TRUE(cursor_manager_.IsMouseEventsEnabled());

  cursor_manager_.ShowCursor();
  cursor_manager_.DisableMouseEvents();
  EXPECT_FALSE(cursor_manager_.IsCursorVisible());
  EXPECT_FALSE(cursor_manager_.IsMouseEventsEnabled());
  cursor_manager_.EnableMouseEvents();
  EXPECT_TRUE(cursor_manager_.IsCursorVisible());
  EXPECT_TRUE(cursor_manager_.IsMouseEventsEnabled());

  cursor_manager_.HideCursor();
  cursor_manager_.DisableMouseEvents();
  EXPECT_FALSE(cursor_manager_.IsCursorVisible());
  EXPECT_FALSE(cursor_manager_.IsMouseEventsEnabled());
  cursor_manager_.EnableMouseEvents();
  EXPECT_FALSE(cursor_manager_.IsCursorVisible());
  EXPECT_TRUE(cursor_manager_.IsMouseEventsEnabled());

  // When mouse events are disabled, ShowCursor is ignored.
  cursor_manager_.DisableMouseEvents();
  EXPECT_FALSE(cursor_manager_.IsCursorVisible());
  EXPECT_FALSE(cursor_manager_.IsMouseEventsEnabled());
  cursor_manager_.ShowCursor();
  EXPECT_FALSE(cursor_manager_.IsCursorVisible());
  EXPECT_FALSE(cursor_manager_.IsMouseEventsEnabled());
  cursor_manager_.DisableMouseEvents();
  EXPECT_FALSE(cursor_manager_.IsCursorVisible());
  EXPECT_FALSE(cursor_manager_.IsMouseEventsEnabled());
}

// Verifies that calling DisableMouseEvents multiple times in a row makes no
// difference compared with calling it once.
// This is a regression test for http://crbug.com/169404.
TEST_F(CursorManagerTest, MultipleDisableMouseEvents) {
  cursor_manager_.DisableMouseEvents();
  cursor_manager_.DisableMouseEvents();
  cursor_manager_.EnableMouseEvents();
  cursor_manager_.LockCursor();
  cursor_manager_.UnlockCursor();
  EXPECT_TRUE(cursor_manager_.IsCursorVisible());
}

// Verifies that calling EnableMouseEvents multiple times in a row makes no
// difference compared with calling it once.
TEST_F(CursorManagerTest, MultipleEnableMouseEvents) {
  cursor_manager_.DisableMouseEvents();
  cursor_manager_.EnableMouseEvents();
  cursor_manager_.EnableMouseEvents();
  cursor_manager_.LockCursor();
  cursor_manager_.UnlockCursor();
  EXPECT_TRUE(cursor_manager_.IsCursorVisible());
}
