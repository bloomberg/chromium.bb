// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <linux/input.h>
#include <wayland-server.h>
#include <memory>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"
#include "ui/ozone/platform/wayland/test/mock_surface.h"
#include "ui/ozone/platform/wayland/test/test_touch.h"
#include "ui/ozone/platform/wayland/test/test_wayland_server_thread.h"
#include "ui/ozone/platform/wayland/test/wayland_test.h"
#include "ui/ozone/test/mock_platform_window_delegate.h"

using ::testing::_;
using ::testing::SaveArg;

namespace ui {

namespace {

ACTION_P(CloneEvent, ptr) {
  *ptr = Event::Clone(*arg0);
}

}  // namespace

class WaylandTouchTest : public WaylandTest {
 public:
  WaylandTouchTest() {}

  void SetUp() override {
    WaylandTest::SetUp();

    wl_seat_send_capabilities(server_.seat()->resource(),
                              WL_SEAT_CAPABILITY_TOUCH);

    Sync();

    touch_ = server_.seat()->touch();
    ASSERT_TRUE(touch_);
  }

 protected:
  void CheckEventType(ui::EventType event_type, ui::Event* event) {
    ASSERT_TRUE(event);
    ASSERT_TRUE(event->IsTouchEvent());

    auto* key_event = event->AsTouchEvent();
    EXPECT_EQ(event_type, key_event->type());
  }

  wl::TestTouch* touch_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WaylandTouchTest);
};

TEST_P(WaylandTouchTest, KeypressAndMotion) {
  std::unique_ptr<Event> event;
  EXPECT_CALL(delegate_, DispatchEvent(_)).WillRepeatedly(CloneEvent(&event));

  wl_touch_send_down(touch_->resource(), 1, 0, surface_->resource(), 0 /* id */,
                     wl_fixed_from_int(50), wl_fixed_from_int(100));

  Sync();
  CheckEventType(ui::ET_TOUCH_PRESSED, event.get());

  wl_touch_send_motion(touch_->resource(), 500, 0 /* id */,
                       wl_fixed_from_int(100), wl_fixed_from_int(100));

  Sync();
  CheckEventType(ui::ET_TOUCH_MOVED, event.get());

  wl_touch_send_up(touch_->resource(), 1, 1000, 0 /* id */);

  Sync();
  CheckEventType(ui::ET_TOUCH_RELEASED, event.get());
}

// Tests that touch focus is correctly set and released.
TEST_P(WaylandTouchTest, CheckTouchFocus) {
  uint32_t serial = 0;
  uint32_t time = 0;
  constexpr uint32_t touch_id1 = 1;
  constexpr uint32_t touch_id2 = 2;
  constexpr uint32_t touch_id3 = 3;

  wl_touch_send_down(touch_->resource(), ++serial, ++time, surface_->resource(),
                     touch_id1, wl_fixed_from_int(50), wl_fixed_from_int(100));

  Sync();

  EXPECT_TRUE(window_->has_touch_focus());

  wl_touch_send_up(touch_->resource(), ++serial, ++time, touch_id1);

  Sync();

  EXPECT_FALSE(window_->has_touch_focus());

  wl_touch_send_down(touch_->resource(), ++serial, ++time, surface_->resource(),
                     touch_id1, wl_fixed_from_int(30), wl_fixed_from_int(40));

  Sync();

  EXPECT_TRUE(window_->has_touch_focus());

  wl_touch_send_down(touch_->resource(), ++serial, ++time, surface_->resource(),
                     touch_id2, wl_fixed_from_int(30), wl_fixed_from_int(40));
  wl_touch_send_down(touch_->resource(), ++serial, ++time, surface_->resource(),
                     touch_id3, wl_fixed_from_int(30), wl_fixed_from_int(40));

  Sync();

  EXPECT_TRUE(window_->has_touch_focus());

  wl_touch_send_up(touch_->resource(), ++serial, ++time, touch_id2);

  Sync();

  EXPECT_TRUE(window_->has_touch_focus());

  wl_touch_send_up(touch_->resource(), ++serial, ++time, touch_id1);

  Sync();

  EXPECT_TRUE(window_->has_touch_focus());

  wl_touch_send_up(touch_->resource(), ++serial, ++time, touch_id3);

  Sync();

  EXPECT_FALSE(window_->has_touch_focus());

  // Now send many touches and cancel them.
  wl_touch_send_down(touch_->resource(), ++serial, ++time, surface_->resource(),
                     touch_id1, wl_fixed_from_int(30), wl_fixed_from_int(40));
  wl_touch_send_down(touch_->resource(), ++serial, ++time, surface_->resource(),
                     touch_id2, wl_fixed_from_int(30), wl_fixed_from_int(40));
  wl_touch_send_down(touch_->resource(), ++serial, ++time, surface_->resource(),
                     touch_id3, wl_fixed_from_int(30), wl_fixed_from_int(40));

  Sync();

  EXPECT_TRUE(window_->has_touch_focus());

  wl_touch_send_cancel(touch_->resource());

  Sync();

  EXPECT_FALSE(window_->has_touch_focus());
}

INSTANTIATE_TEST_SUITE_P(XdgVersionStableTest,
                         WaylandTouchTest,
                         ::testing::Values(kXdgShellStable));
INSTANTIATE_TEST_SUITE_P(XdgVersionV6Test,
                         WaylandTouchTest,
                         ::testing::Values(kXdgShellV6));

}  // namespace ui
