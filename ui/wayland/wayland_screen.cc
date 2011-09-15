// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wayland/wayland_screen.h"

#include <wayland-client.h>

#include "ui/wayland/wayland_display.h"

namespace ui {

WaylandScreen::WaylandScreen(WaylandDisplay* display, uint32_t id)
    : output_(NULL),
      display_(display) {
  static const wl_output_listener kOutputListener = {
    WaylandScreen::OutputHandleGeometry,
    WaylandScreen::OutputHandleMode,
  };

  output_ = static_cast<wl_output*>(
      wl_display_bind(display_->display(), id, &wl_output_interface));
  wl_output_add_listener(output_, &kOutputListener, this);
}

WaylandScreen::~WaylandScreen() {
  if (output_)
    wl_output_destroy(output_);
}

gfx::Rect WaylandScreen::GetAllocation() const {
  gfx::Rect allocation;
  allocation.set_origin(position_);

  // Find the active mode and pass its dimensions.
  for (Modes::const_iterator i = modes_.begin(); i != modes_.end(); ++i) {
    if ((*i).flags & WL_OUTPUT_MODE_CURRENT) {
      allocation.set_width((*i).width);
      allocation.set_height((*i).height);
      break;
    }
  }

  return allocation;
}

// static
void WaylandScreen::OutputHandleGeometry(void* data,
                                         wl_output* output,
                                         int32_t x,
                                         int32_t y,
                                         int32_t physical_width,
                                         int32_t physical_height,
                                         int32_t subpixel,
                                         const char* make,
                                         const char* model) {
  WaylandScreen* screen = static_cast<WaylandScreen*>(data);
  screen->position_.SetPoint(x, y);
}

// static
void WaylandScreen::OutputHandleMode(void* data,
                                     wl_output* wl_output,
                                     uint32_t flags,
                                     int32_t width,
                                     int32_t height,
                                     int32_t refresh) {
  WaylandScreen* screen = static_cast<WaylandScreen*>(data);

  Mode mode;
  mode.width = width;
  mode.height = height;
  mode.refresh = refresh;
  mode.flags = flags;

  screen->modes_.push_back(mode);
}

}  // namespace ui
