// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/test_seat.h"

#include "ui/ozone/platform/wayland/test/test_keyboard.h"
#include "ui/ozone/platform/wayland/test/test_pointer.h"
#include "ui/ozone/platform/wayland/test/test_touch.h"

namespace wl {

namespace {

constexpr uint32_t kSeatVersion = 4;

void GetPointer(wl_client* client, wl_resource* resource, uint32_t id) {
  wl_resource* pointer_resource = wl_resource_create(
      client, &wl_pointer_interface, wl_resource_get_version(resource), id);
  if (!pointer_resource) {
    wl_client_post_no_memory(client);
    return;
  }
  SetImplementation(pointer_resource, &kTestPointerImpl,
                    std::make_unique<TestPointer>(pointer_resource));

  auto* seat = GetUserDataAs<TestSeat>(resource);
  seat->set_pointer(GetUserDataAs<TestPointer>(pointer_resource));
}

void GetKeyboard(wl_client* client, wl_resource* resource, uint32_t id) {
  wl_resource* keyboard_resource = wl_resource_create(
      client, &wl_keyboard_interface, wl_resource_get_version(resource), id);
  if (!keyboard_resource) {
    wl_client_post_no_memory(client);
    return;
  }
  SetImplementation(keyboard_resource, &kTestKeyboardImpl,
                    std::make_unique<TestKeyboard>(keyboard_resource));

  auto* seat = GetUserDataAs<TestSeat>(resource);
  seat->set_keyboard(GetUserDataAs<TestKeyboard>(keyboard_resource));
}

void GetTouch(wl_client* client, wl_resource* resource, uint32_t id) {
  wl_resource* touch_resource = wl_resource_create(
      client, &wl_touch_interface, wl_resource_get_version(resource), id);
  if (!touch_resource) {
    wl_client_post_no_memory(client);
    return;
  }
  SetImplementation(touch_resource, &kTestTouchImpl,
                    std::make_unique<TestTouch>(touch_resource));

  auto* seat = GetUserDataAs<TestSeat>(resource);
  seat->set_touch(GetUserDataAs<TestTouch>(touch_resource));
}

}  // namespace

const struct wl_seat_interface kTestSeatImpl = {
    &GetPointer,       // get_pointer
    &GetKeyboard,      // get_keyboard
    &GetTouch,         // get_touch,
    &DestroyResource,  // release
};

TestSeat::TestSeat()
    : GlobalObject(&wl_seat_interface, &kTestSeatImpl, kSeatVersion) {}

TestSeat::~TestSeat() {}

}  // namespace wl
