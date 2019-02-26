// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/wayland_display_output.h"

#include <wayland-server-core.h>
#include <wayland-server-protocol-core.h>

namespace exo {
namespace wayland {

WaylandDisplayOutput::WaylandDisplayOutput(int64_t id) : id_(id) {}

WaylandDisplayOutput::~WaylandDisplayOutput() {
  if (global_)
    wl_global_destroy(global_);
}

int64_t WaylandDisplayOutput::id() const {
  return id_;
}

void WaylandDisplayOutput::set_global(wl_global* global) {
  global_ = global;
}

}  // namespace wayland
}  // namespace exo
