// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_SUBSURFACE_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_SUBSURFACE_H_

#include <wayland-server-protocol.h>

#include <memory>
#include <utility>

#include "testing/gmock/include/gmock/gmock.h"
#include "ui/gfx/geometry/point.h"
#include "ui/ozone/platform/wayland/test/server_object.h"

struct wl_resource;

namespace wl {

extern const struct wl_subsurface_interface kTestSubSurfaceImpl;

class TestSubSurface : public ServerObject {
 public:
  explicit TestSubSurface(wl_resource* resource,
                          wl_resource* surface,
                          wl_resource* parent_resource);
  ~TestSubSurface() override;
  TestSubSurface(const TestSubSurface& rhs) = delete;
  TestSubSurface& operator=(const TestSubSurface& rhs) = delete;

  void SetPosition(int x, int y);
  gfx::Point position() const { return position_; }

  void set_sync(bool sync) { sync_ = sync; }
  bool sync() const { return sync_; }

  wl_resource* parent_resource() const { return parent_resource_; }

 private:
  gfx::Point position_;
  bool sync_ = false;

  // Surface resource that is the ground for this subsurface.
  wl_resource* surface_ = nullptr;

  // Parent surface resource.
  wl_resource* parent_resource_ = nullptr;
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_SUBSURFACE_H_
