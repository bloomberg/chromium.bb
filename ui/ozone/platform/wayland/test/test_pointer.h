// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_POINTER_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_POINTER_H_

#include <wayland-server-protocol.h>

#include "base/macros.h"
#include "ui/ozone/platform/wayland/test/server_object.h"

struct wl_resource;

namespace wl {

extern const struct wl_pointer_interface kTestPointerImpl;

class TestPointer : public ServerObject {
 public:
  explicit TestPointer(wl_resource* resource);
  ~TestPointer() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestPointer);
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_MOCK_POINTER_H_
