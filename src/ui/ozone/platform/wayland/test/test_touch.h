// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_TOUCH_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_TOUCH_H_

#include <wayland-server-protocol.h>

#include "ui/ozone/platform/wayland/test/server_object.h"

struct wl_resource;

namespace wl {

extern const struct wl_pointer_interface kTestTouchImpl;

class TestTouch : public ServerObject {
 public:
  explicit TestTouch(wl_resource* resource);

  TestTouch(const TestTouch&) = delete;
  TestTouch& operator=(const TestTouch&) = delete;

  ~TestTouch() override;
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_TOUCH_H_
