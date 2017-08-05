// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/ws/display_manager.h"
#include "services/ui/ws/test_utils.h"
#include "services/ui/ws/window_server.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ui {
namespace ws {
namespace test {

class WindowServerHitTestTest : public testing::Test {
 public:
  WindowServerHitTestTest() = default;
  ~WindowServerHitTestTest() override = default;

 protected:
  WindowServer* window_server() { return ws_test_helper_.window_server(); }
  DisplayManager* display_manager() {
    return window_server()->display_manager();
  }
  TestScreenManager& screen_manager() { return screen_manager_; }

 private:
  // testing::Test:
  void SetUp() override {
    screen_manager_.Init(window_server()->display_manager());
  }
  void TearDown() override {}

  WindowServerTestHelper ws_test_helper_;
  TestScreenManager screen_manager_;

  DISALLOW_COPY_AND_ASSIGN(WindowServerHitTestTest);
};

TEST_F(WindowServerHitTestTest, CreateHitTestQuery) {
  EXPECT_EQ(window_server()->display_hit_test_query().size(), 0u);
  AddWindowManager(window_server(), "1");
  const int64_t display_id =
      screen_manager().AddDisplay(MakeDisplay(0, 0, 1024, 768, 1.0f));

  ASSERT_EQ(display_manager()->displays().size(), 1u);
  Display* display = display_manager()->GetDisplayById(display_id);

  // WindowServer should have one HitTestQuery associated with root's
  // FrameSinkId.
  ASSERT_NE(display->root_window(), nullptr);
  viz::FrameSinkId frame_sink_id = display->root_window()->frame_sink_id();
  EXPECT_EQ(window_server()->display_hit_test_query().size(), 1u);
  EXPECT_EQ(window_server()->display_hit_test_query().count(frame_sink_id), 1u);

  const int64_t display_id2 =
      screen_manager().AddDisplay(MakeDisplay(100, 100, 1024, 768, 1.0f));

  ASSERT_EQ(display_manager()->displays().size(), 2u);
  Display* display2 = display_manager()->GetDisplayById(display_id2);

  // WindowServer should have two HitTestQuery.
  ASSERT_NE(display2->root_window(), nullptr);
  viz::FrameSinkId frame_sink_id2 = display2->root_window()->frame_sink_id();
  EXPECT_EQ(window_server()->display_hit_test_query().size(), 2u);
  EXPECT_EQ(window_server()->display_hit_test_query().count(frame_sink_id), 1u);
  EXPECT_EQ(window_server()->display_hit_test_query().count(frame_sink_id2),
            1u);
}

}  // namespace test
}  // namespace ws
}  // namespace ui
