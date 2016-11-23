// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ws/user_display_manager.h"

#include <stdint.h>

#include <string>

#include "base/atomicops.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_number_conversions.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "services/ui/common/task_runner_test_base.h"
#include "services/ui/common/types.h"
#include "services/ui/common/util.h"
#include "services/ui/display/platform_screen.h"
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

class TestDisplayManagerObserver : public mojom::DisplayManagerObserver {
 public:
  TestDisplayManagerObserver() : binding_(this) {}
  ~TestDisplayManagerObserver() override {}

  mojom::DisplayManagerObserverPtr GetPtr() {
    return binding_.CreateInterfacePtrAndBind();
  }

  std::string GetAndClearObserverCalls() {
    std::string result;
    std::swap(observer_calls_, result);
    return result;
  }

 private:
  void AddCall(const std::string& call) {
    if (!observer_calls_.empty())
      observer_calls_ += "\n";
    observer_calls_ += call;
  }

  std::string DisplayIdsToString(
      const std::vector<mojom::WsDisplayPtr>& wm_displays) {
    std::string display_ids;
    for (const auto& wm_display : wm_displays) {
      if (!display_ids.empty())
        display_ids += " ";
      display_ids += base::Int64ToString(wm_display->display.id());
    }
    return display_ids;
  }

  // mojom::DisplayManagerObserver:
  void OnDisplays(std::vector<mojom::WsDisplayPtr> displays,
                  int64_t primary_display_id,
                  int64_t internal_display_id) override {
    AddCall("OnDisplays " + DisplayIdsToString(displays));
  }
  void OnDisplaysChanged(std::vector<mojom::WsDisplayPtr> displays) override {
    AddCall("OnDisplaysChanged " + DisplayIdsToString(displays));
  }
  void OnDisplayRemoved(int64_t id) override {
    AddCall("OnDisplayRemoved " + base::Int64ToString(id));
  }
  void OnPrimaryDisplayChanged(int64_t id) override {
    AddCall("OnPrimaryDisplayChanged " + base::Int64ToString(id));
  }

  mojo::Binding<mojom::DisplayManagerObserver> binding_;
  std::string observer_calls_;

  DISALLOW_COPY_AND_ASSIGN(TestDisplayManagerObserver);
};

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

  TestPlatformScreen& platform_screen() { return platform_screen_; }

 private:
  // testing::Test:
  void SetUp() override {
    TaskRunnerTestBase::SetUp();
    platform_screen_.Init(window_server()->display_manager());
  }

  WindowServerTestHelper ws_test_helper_;
  TestPlatformScreen platform_screen_;
  DISALLOW_COPY_AND_ASSIGN(UserDisplayManagerTest);
};

TEST_F(UserDisplayManagerTest, OnlyNotifyWhenFrameDecorationsSet) {
  platform_screen().AddDisplay();

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

  EXPECT_EQ("OnDisplays 1",
            display_manager_observer1.GetAndClearObserverCalls());
}

TEST_F(UserDisplayManagerTest, AddObserverAfterFrameDecorationsSet) {
  platform_screen().AddDisplay();

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

  EXPECT_EQ("OnDisplays 1",
            display_manager_observer1.GetAndClearObserverCalls());
}

TEST_F(UserDisplayManagerTest, AddRemoveDisplay) {
  platform_screen().AddDisplay();

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

  EXPECT_EQ("OnDisplays 1",
            display_manager_observer1.GetAndClearObserverCalls());

  // Add another display.
  const int64_t second_display_id = platform_screen().AddDisplay();
  RunUntilIdle();

  // Observer should be notified immediately as frame decorations were set.
  EXPECT_EQ("OnDisplaysChanged 2",
            display_manager_observer1.GetAndClearObserverCalls());

  // Remove the display and verify observer is notified.
  platform_screen().RemoveDisplay(second_display_id);
  RunUntilIdle();

  EXPECT_EQ("OnDisplayRemoved 2",
            display_manager_observer1.GetAndClearObserverCalls());
}

TEST_F(UserDisplayManagerTest, NegativeCoordinates) {
  platform_screen().AddDisplay();

  TestDisplayManagerObserver display_manager_observer1;
  DisplayManager* display_manager = window_server()->display_manager();
  AddWindowManager(window_server(), kUserId1);
  UserDisplayManager* user_display_manager1 =
      display_manager->GetUserDisplayManager(kUserId1);
  ASSERT_TRUE(user_display_manager1);

  user_display_manager1->OnMouseCursorLocationChanged(gfx::Point(-10, -11));

  base::subtle::Atomic32* cursor_location_memory = nullptr;
  mojo::ScopedSharedBufferHandle handle =
      user_display_manager1->GetCursorLocationMemory();
  mojo::ScopedSharedBufferMapping cursor_location_mapping =
      handle->Map(sizeof(base::subtle::Atomic32));
  ASSERT_TRUE(cursor_location_mapping);
  cursor_location_memory =
      reinterpret_cast<base::subtle::Atomic32*>(cursor_location_mapping.get());

  base::subtle::Atomic32 location =
      base::subtle::NoBarrier_Load(cursor_location_memory);
  EXPECT_EQ(gfx::Point(static_cast<int16_t>(location >> 16),
                       static_cast<int16_t>(location & 0xFFFF)),
            gfx::Point(-10, -11));
}

}  // namespace test
}  // namespace ws
}  // namespace ui
