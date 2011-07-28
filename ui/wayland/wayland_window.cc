// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wayland/wayland_window.h"

#include <wayland-egl.h>

#include "ui/wayland/events/wayland_event.h"
#include "ui/wayland/wayland_display.h"
#include "ui/wayland/wayland_widget.h"

namespace ui {

WaylandWindow::WaylandWindow(WaylandWidget* widget, WaylandDisplay* display)
    : widget_(widget),
      display_(display),
      parent_window_(NULL),
      relative_position_(),
      surface_(display->CreateSurface()),
      fullscreen_(false) {
  wl_surface_set_user_data(surface_, this);
}

WaylandWindow::WaylandWindow(
    WaylandWidget* widget,
    WaylandDisplay* display,
    WaylandWindow* parent,
    int32_t x,
    int32_t y)
    : widget_(widget),
      display_(display),
      parent_window_(parent),
      relative_position_(x, y),
      surface_(display->CreateSurface()),
      fullscreen_(false) {
  wl_surface_set_user_data(surface_, this);
}

WaylandWindow::~WaylandWindow() {
  if (surface_)
    wl_surface_destroy(surface_);
}

void WaylandWindow::SetVisible(bool visible) {
  if (visible) {
    if (fullscreen_) {
      wl_shell_set_fullscreen(display_->shell(), surface_);
    } else if (!parent_window_) {
      wl_shell_set_toplevel(display_->shell(), surface_);
    } else {
      wl_shell_set_transient(display_->shell(),
                             surface_,
                             parent_window_->surface(),
                             relative_position_.x(),
                             relative_position_.y(),
                             0);
    }
  } else {
    // TODO(dnicoara) What is the correct way of hiding a wl_surface?
  }
}

bool WaylandWindow::IsVisible() const {
  return surface_ != NULL;
}

void WaylandWindow::Configure(uint32_t time,
                              uint32_t edges,
                              int32_t x,
                              int32_t y,
                              int32_t width,
                              int32_t height) {
  WaylandEvent event;
  event.geometry_change.time = time;
  event.geometry_change.x = x;
  event.geometry_change.y = y;
  event.geometry_change.width = width;
  event.geometry_change.height = height;

  widget_->OnGeometryChange(event);
}

}  // namespace ui
