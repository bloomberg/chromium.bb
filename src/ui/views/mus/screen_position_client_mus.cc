// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/mus/screen_position_client_mus.h"

#include "ui/aura/mus/window_tree_host_mus.h"
#include "ui/aura/window.h"

namespace views {

ScreenPositionClientMus::ScreenPositionClientMus(aura::WindowTreeHostMus* host)
    : DesktopScreenPositionClient(host->window()), host_(host) {}

ScreenPositionClientMus::~ScreenPositionClientMus() = default;

gfx::Point ScreenPositionClientMus::GetOriginInScreen(
    const aura::Window* window) {
  DCHECK_EQ(window, host_->window());
  return host_->bounds_in_dip().origin();
}

}  // namespace views
