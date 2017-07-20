// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ws/user_display_manager.h"

#include <stdint.h>

#include <string>

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_number_conversions.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/ui/common/task_runner_test_base.h"
#include "services/ui/common/types.h"
#include "services/ui/common/util.h"
#include "services/ui/display/screen_manager.h"
#include "services/ui/ws/display_manager.h"
#include "services/ui/ws/ids.h"
#include "services/ui/ws/server_window.h"
#include "services/ui/ws/test_utils.h"
#include "services/ui/ws/window_manager_state.h"
#include "services/ui/ws/window_manager_window_tree_factory_set.h"
#include "services/ui/ws/window_server.h"
#include "services/ui/ws/window_server_delegate.h"
#include "services/ui/ws/window_tree.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"

namespace ui {
namespace ws {
namespace test {
namespace {

const char kUserId1[] = "123";

mojom::FrameDecorationValuesPtr CreateDefaultFrameDecorationValues() {
  return mojom::FrameDecorationValues::New();
}

}  // namespace

// -----------------------------------------------------------------------------

class UserDisplayManagerTest : public TaskRunnerTestBase {
 public:
  UserDisplayManagerTest() {}
  ~UserDisplayManagerTest() override {}

  WindowServer* window_server() { return ws_test_helper_.window_server(); }
  TestWindowServerDelegate* window_server_delegate() {
    return ws_test_helper_.window_server_delegate();
  }

  TestScreenManager& screen_manager() { return screen_manager_; }

 private:
  // testing::Test:
  void SetUp() override {
    TaskRunnerTestBase::SetUp();
    screen_manager_.Init(window_server()->display_manager());
  }

  WindowServerTestHelper ws_test_helper_;
  TestScreenManager screen_manager_;
  DISALLOW_COPY_AND_ASSIGN(UserDisplayManagerTest);
};

TEST_F(UserDisplayManagerTest, OnlyNotifyWhenFrameDecorationsSet) {
  screen_manager().AddDisplay();

  TestDisplayManagerObserver display_manager_observer1;
  DisplayManager* display_manager = window_server()->display_manager();
  AddWindowManager(window_server(), kUserId1);
  UserDisplayManager* user_display_manager1 =
      display_manager->GetUserDisplayManager(kUserId1);
  ASSERT_TRUE(user_display_manager1);
  user_display_manager1->AddObserver(display_manager_observer1.GetPtr());
  RunUntilIdle();

  // Observer should not have been notified yet.
  EXPECT_EQ(std::string(),
            display_manager_observer1.GetAndClearObserverCalls());

  // Set the frame decoration values, which should trigger sending immediately.
  ASSERT_EQ(1u, display_manager->displays().size());
  window_server()
      ->window_manager_window_tree_factory_set()
      ->GetWindowManagerStateForUser(kUserId1)
      ->SetFrameDecorationValues(CreateDefaultFrameDecorationValues());
  RunUntilIdle();

  EXPECT_EQ("OnDisplaysChanged 1 -1",
            display_manager_observer1.GetAndClearObserverCalls());
}

TEST_F(UserDisplayManagerTest, AddObserverAfterFrameDecorationsSet) {
  screen_manager().AddDisplay();

  TestDisplayManagerObserver display_manager_observer1;
  DisplayManager* display_manager = window_server()->display_manager();
  AddWindowManager(window_server(), kUserId1);
  UserDisplayManager* user_display_manager1 =
      display_manager->GetUserDisplayManager(kUserId1);
  ASSERT_TRUE(user_display_manager1);
  ASSERT_EQ(1u, display_manager->displays().size());
  window_server()
      ->window_manager_window_tree_factory_set()
      ->GetWindowManagerStateForUser(kUserId1)
      ->SetFrameDecorationValues(CreateDefaultFrameDecorationValues());

  user_display_manager1->AddObserver(display_manager_observer1.GetPtr());
  RunUntilIdle();

  EXPECT_EQ("OnDisplaysChanged 1 -1",
            display_manager_observer1.GetAndClearObserverCalls());
}

TEST_F(UserDisplayManagerTest, AddRemoveDisplay) {
  screen_manager().AddDisplay();

  TestDisplayManagerObserver display_manager_observer1;
  DisplayManager* display_manager = window_server()->display_manager();
  AddWindowManager(window_server(), kUserId1);
  UserDisplayManager* user_display_manager1 =
      display_manager->GetUserDisplayManager(kUserId1);
  ASSERT_TRUE(user_display_manager1);
  ASSERT_EQ(1u, display_manager->displays().size());
  window_server()
      ->window_manager_window_tree_factory_set()
      ->GetWindowManagerStateForUser(kUserId1)
      ->SetFrameDecorationValues(CreateDefaultFrameDecorationValues());
  user_display_manager1->AddObserver(display_manager_observer1.GetPtr());
  RunUntilIdle();

  EXPECT_EQ("OnDisplaysChanged 1 -1",
            display_manager_observer1.GetAndClearObserverCalls());

  // Add another display.
  const int64_t second_display_id = screen_manager().AddDisplay();
  RunUntilIdle();

  // Observer should be notified immediately as frame decorations were set.
  EXPECT_EQ("OnDisplaysChanged 1 2 -1",
            display_manager_observer1.GetAndClearObserverCalls());

  // Remove the display and verify observer is notified.
  screen_manager().RemoveDisplay(second_display_id);
  RunUntilIdle();

  EXPECT_EQ("OnDisplaysChanged 1 -1",
            display_manager_observer1.GetAndClearObserverCalls());
}

}  // namespace test
}  // namespace ws
}  // namespace ui
