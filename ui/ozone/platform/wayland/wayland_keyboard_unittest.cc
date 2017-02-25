// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <linux/input.h>
#include <wayland-server.h>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event.h"
#include "ui/ozone/platform/wayland/fake_server.h"
#include "ui/ozone/platform/wayland/mock_platform_window_delegate.h"
#include "ui/ozone/platform/wayland/wayland_test.h"
#include "ui/ozone/platform/wayland/wayland_window.h"

using ::testing::SaveArg;
using ::testing::_;

namespace ui {

class WaylandKeyboardTest : public WaylandTest {
 public:
  WaylandKeyboardTest() {}

  void SetUp() override {
    WaylandTest::SetUp();

    wl_seat_send_capabilities(server.seat()->resource(),
                              WL_SEAT_CAPABILITY_KEYBOARD);

    Sync();

    keyboard = server.seat()->keyboard.get();
    ASSERT_TRUE(keyboard);
  }

 protected:
  wl::MockKeyboard* keyboard;

 private:
  DISALLOW_COPY_AND_ASSIGN(WaylandKeyboardTest);
};

ACTION_P(CloneEvent, ptr) {
  *ptr = Event::Clone(*arg0);
}

TEST_F(WaylandKeyboardTest, Keypress) {
  struct wl_array empty;
  wl_array_init(&empty);
  wl_keyboard_send_enter(keyboard->resource(), 1, surface->resource(), &empty);
  wl_array_release(&empty);

  wl_keyboard_send_key(keyboard->resource(), 2, 0, 30 /* a */,
                       WL_KEYBOARD_KEY_STATE_PRESSED);

  std::unique_ptr<Event> event;
  EXPECT_CALL(delegate, DispatchEvent(_)).WillOnce(CloneEvent(&event));

  Sync();
  ASSERT_TRUE(event);
  ASSERT_TRUE(event->IsKeyEvent());

  auto* key_event = event->AsKeyEvent();
  EXPECT_EQ(ui::VKEY_A, key_event->key_code());
  EXPECT_EQ(ET_KEY_PRESSED, key_event->type());

  wl_keyboard_send_leave(keyboard->resource(), 3, surface->resource());

  Sync();

  wl_keyboard_send_key(keyboard->resource(), 3, 0, 30 /* a */,
                       WL_KEYBOARD_KEY_STATE_PRESSED);
  EXPECT_CALL(delegate, DispatchEvent(_)).Times(0);
}

}  // namespace ui
