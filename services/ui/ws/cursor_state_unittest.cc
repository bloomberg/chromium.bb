// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ws/cursor_state.h"

#include "services/ui/ws/display_manager.h"
#include "services/ui/ws/test_utils.h"
#include "services/ui/ws/window_server.h"
#include "services/ui/ws/window_server_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/cursor/cursor.h"

namespace ui {
namespace ws {
namespace test {
namespace {

const UserId kTestId1 = "2";

}  // namespace

class CursorStateTest : public testing::Test {
 public:
  CursorStateTest() {}
  ~CursorStateTest() override {}

  WindowServer* window_server() { return ws_test_helper_.window_server(); }
  DisplayManager* display_manager() {
    return window_server()->display_manager();
  }
  TestScreenManager& screen_manager() { return screen_manager_; }
  CursorState* cursor_state() { return cursor_state_.get(); }
  const ui::CursorData& cursor() { return ws_test_helper_.cursor(); }

 protected:
  // testing::Test:
  void SetUp() override {
    screen_manager_.Init(window_server()->display_manager());
    window_server()->user_id_tracker()->AddUserId(kTestId1);
    cursor_state_ = base::MakeUnique<CursorState>(display_manager());

    AddWindowManager(window_server(), kTestId1);
    screen_manager().AddDisplay(MakeDisplay(0, 0, 1024, 768, 1.0f));
    ASSERT_EQ(1u, display_manager()->displays().size());
  }

 private:
  WindowServerTestHelper ws_test_helper_;
  TestScreenManager screen_manager_;

  std::unique_ptr<CursorState> cursor_state_;

  DISALLOW_COPY_AND_ASSIGN(CursorStateTest);
};

TEST_F(CursorStateTest, CursorLockTest) {
  cursor_state()->SetCurrentWindowCursor(ui::CursorData(ui::CursorType::kWait));
  EXPECT_TRUE(cursor().IsType(ui::CursorType::kWait));

  cursor_state()->LockCursor();
  cursor_state()->SetCurrentWindowCursor(ui::CursorData(ui::CursorType::kCell));
  EXPECT_TRUE(cursor().IsType(ui::CursorType::kWait));

  cursor_state()->UnlockCursor();
  EXPECT_TRUE(cursor().IsType(ui::CursorType::kCell));
}

TEST_F(CursorStateTest, CursorVisibilityTest) {
  cursor_state()->SetCurrentWindowCursor(ui::CursorData(ui::CursorType::kWait));
  EXPECT_TRUE(cursor().IsType(ui::CursorType::kWait));

  cursor_state()->SetCursorVisible(false);
  EXPECT_TRUE(cursor().IsType(ui::CursorType::kNone));

  cursor_state()->SetCursorVisible(true);
  EXPECT_TRUE(cursor().IsType(ui::CursorType::kWait));

  cursor_state()->SetCursorVisible(false);
  cursor_state()->SetCurrentWindowCursor(ui::CursorData(ui::CursorType::kCell));
  EXPECT_TRUE(cursor().IsType(ui::CursorType::kNone));

  cursor_state()->SetCursorVisible(true);
  EXPECT_TRUE(cursor().IsType(ui::CursorType::kCell));
}

TEST_F(CursorStateTest, CursorOverrideTest) {
  cursor_state()->SetCurrentWindowCursor(ui::CursorData(ui::CursorType::kWait));
  EXPECT_TRUE(cursor().IsType(ui::CursorType::kWait));

  cursor_state()->SetGlobalOverrideCursor(
      ui::CursorData(ui::CursorType::kCell));
  EXPECT_TRUE(cursor().IsType(ui::CursorType::kCell));

  cursor_state()->SetGlobalOverrideCursor(base::nullopt);
  EXPECT_TRUE(cursor().IsType(ui::CursorType::kWait));
}

TEST_F(CursorStateTest, CursorOverrideLockTest) {
  // This test is meant to mimic the calls in ScreenshotController when it sets
  // a cursor.
  cursor_state()->SetCurrentWindowCursor(ui::CursorData(ui::CursorType::kWait));
  EXPECT_TRUE(cursor().IsType(ui::CursorType::kWait));

  cursor_state()->SetGlobalOverrideCursor(
      ui::CursorData(ui::CursorType::kCross));
  cursor_state()->LockCursor();
  cursor_state()->SetGlobalOverrideCursor(base::nullopt);
  EXPECT_TRUE(cursor().IsType(ui::CursorType::kCross));

  cursor_state()->UnlockCursor();
  EXPECT_TRUE(cursor().IsType(ui::CursorType::kWait));
}

TEST_F(CursorStateTest, CursorOverrideVisibilityTest) {
  // This test is meant to mimic the calls in ScreenshotController when it
  // hides the cursor.
  cursor_state()->SetCurrentWindowCursor(ui::CursorData(ui::CursorType::kWait));
  EXPECT_TRUE(cursor().IsType(ui::CursorType::kWait));

  cursor_state()->SetCursorVisible(false);
  cursor_state()->LockCursor();
  cursor_state()->SetGlobalOverrideCursor(base::nullopt);
  cursor_state()->SetCursorVisible(true);
  EXPECT_TRUE(cursor().IsType(ui::CursorType::kNone));

  cursor_state()->UnlockCursor();
  EXPECT_TRUE(cursor().IsType(ui::CursorType::kWait));
}

}  // namespace test
}  // namespace ws
}  // namespace ui
