// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/wayland_window.h"

#include "base/strings/utf_string_conversions.h"
#include "ui/ozone/platform/wayland/wayland_display.h"
#include "ui/platform_window/platform_window_delegate.h"

namespace ui {

WaylandWindow::WaylandWindow(PlatformWindowDelegate* delegate,
                             WaylandDisplay* display,
                             const gfx::Rect& bounds)
    : delegate_(delegate), display_(display), bounds_(bounds) {}

WaylandWindow::~WaylandWindow() {
  if (xdg_surface_) {
    display_->RemoveWindow(this);
  }
}

bool WaylandWindow::Initialize() {
  static const xdg_surface_listener xdg_surface_listener = {
      &WaylandWindow::Configure, &WaylandWindow::Close,
  };

  surface_.reset(wl_compositor_create_surface(display_->compositor()));
  if (!surface_) {
    LOG(ERROR) << "Failed to create wl_surface";
    return false;
  }
  wl_surface_set_user_data(surface_.get(), this);
  xdg_surface_.reset(
      xdg_shell_get_xdg_surface(display_->shell(), surface_.get()));
  if (!xdg_surface_) {
    LOG(ERROR) << "Failed to create xdg_surface";
    return false;
  }
  xdg_surface_add_listener(xdg_surface_.get(), &xdg_surface_listener, this);

  display_->AddWindow(this);
  delegate_->OnAcceleratedWidgetAvailable(surface_.id(), 1.f);

  return true;
}

wl_surface* WaylandWindow::GetSurface() {
  DCHECK(surface_);
  return surface_.get();
}

gfx::AcceleratedWidget WaylandWindow::GetWidget() {
  DCHECK(surface_);
  return surface_.id();
}

void WaylandWindow::ApplyPendingBounds() {
  if (pending_bounds_.IsEmpty())
    return;

  SetBounds(pending_bounds_);
  DCHECK(xdg_surface_);
  xdg_surface_ack_configure(xdg_surface_.get(), pending_configure_serial_);
  pending_bounds_ = gfx::Rect();
}

void WaylandWindow::Show() {}

void WaylandWindow::Hide() {
  NOTIMPLEMENTED();
}

void WaylandWindow::Close() {
  NOTIMPLEMENTED();
}

void WaylandWindow::SetBounds(const gfx::Rect& bounds) {
  if (bounds == bounds_)
    return;
  bounds_ = bounds;
  delegate_->OnBoundsChanged(bounds);
}

gfx::Rect WaylandWindow::GetBounds() {
  return bounds_;
}

void WaylandWindow::SetTitle(const base::string16& title) {
  DCHECK(xdg_surface_);
  xdg_surface_set_title(xdg_surface_.get(), UTF16ToUTF8(title).c_str());
}

void WaylandWindow::SetCapture() {
  NOTIMPLEMENTED();
}

void WaylandWindow::ReleaseCapture() {
  NOTIMPLEMENTED();
}

void WaylandWindow::ToggleFullscreen() {
  NOTIMPLEMENTED();
}

void WaylandWindow::Maximize() {
  DCHECK(xdg_surface_);
  xdg_surface_set_maximized(xdg_surface_.get());
}

void WaylandWindow::Minimize() {
  DCHECK(xdg_surface_);
  xdg_surface_set_minimized(xdg_surface_.get());
}

void WaylandWindow::Restore() {
  DCHECK(xdg_surface_);
  xdg_surface_unset_maximized(xdg_surface_.get());
}

void WaylandWindow::SetCursor(PlatformCursor cursor) {
  NOTIMPLEMENTED();
}

void WaylandWindow::MoveCursorTo(const gfx::Point& location) {
  NOTIMPLEMENTED();
}

void WaylandWindow::ConfineCursorToBounds(const gfx::Rect& bounds) {
  NOTIMPLEMENTED();
}

PlatformImeController* WaylandWindow::GetPlatformImeController() {
  NOTIMPLEMENTED();
  return nullptr;
}

void WaylandWindow::Configure(void* data,
                              xdg_surface* obj,
                              int32_t width,
                              int32_t height,
                              wl_array* states,
                              uint32_t serial) {
  WaylandWindow* window = static_cast<WaylandWindow*>(data);

  // Rather than call SetBounds here for every configure event, just save the
  // most recent bounds, and have WaylandDisplay call ApplyPendingBounds when it
  // has finished processing events. We may get many configure events in a row
  // during an interactive resize, and only the last one matters.
  window->pending_bounds_ = gfx::Rect(0, 0, width, height);
  window->pending_configure_serial_ = serial;
}

void WaylandWindow::Close(void* data, xdg_surface* obj) {
  NOTIMPLEMENTED();
}

}  // namespace ui
