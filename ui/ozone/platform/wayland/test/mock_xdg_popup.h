// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_TEST_MOCK_XDG_POPUP_H_
#define UI_OZONE_PLATFORM_WAYLAND_TEST_MOCK_XDG_POPUP_H_

#include <xdg-shell-unstable-v5-server-protocol.h>
#include <xdg-shell-unstable-v6-server-protocol.h>

#include "base/macros.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/ozone/platform/wayland/test/server_object.h"

struct wl_resource;

namespace wl {

extern const struct xdg_popup_interface kXdgPopupImpl;
extern const struct zxdg_popup_v6_interface kZxdgPopupV6Impl;

class MockXdgPopup : public ServerObject {
 public:
  MockXdgPopup(wl_resource* resource);
  ~MockXdgPopup() override;

  MOCK_METHOD1(Grab, void(uint32_t serial));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockXdgPopup);
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_TEST_MOCK_XDG_POPUP_H_
