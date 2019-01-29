// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_POSITIONER_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_POSITIONER_H_

#include <xdg-shell-unstable-v6-server-protocol.h>

#include "base/macros.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/ozone/platform/wayland/test/server_object.h"

struct wl_resource;

namespace wl {

extern const struct zxdg_positioner_v6_interface kTestZxdgPositionerV6Impl;

// A simple positioner object that provides a collection of rules of a child
// surface relative to a parent surface.
class TestPositioner : public ServerObject {
 public:
  explicit TestPositioner(wl_resource* resource);
  ~TestPositioner() override;

  void set_size(gfx::Size size) { size_ = size; }
  gfx::Size size() const { return size_; }

  void set_anchor_rect(gfx::Rect anchor_rect) { anchor_rect_ = anchor_rect; }
  gfx::Rect anchor_rect() const { return anchor_rect_; }

  void set_anchor(uint32_t anchor) { anchor_ = anchor; }

  void set_gravity(uint32_t gravity) { gravity_ = gravity; }

 private:
  gfx::Rect anchor_rect_;
  gfx::Size size_;
  uint32_t anchor_;
  uint32_t gravity_;

  DISALLOW_COPY_AND_ASSIGN(TestPositioner);
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_TEST_POSITIONER_H_
