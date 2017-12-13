// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/wayland_window.h"

#include <wayland-client.h>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "ui/base/cursor/ozone/bitmap_cursor_factory_ozone.h"
#include "ui/events/event.h"
#include "ui/events/ozone/events_ozone.h"
#include "ui/ozone/platform/wayland/wayland_connection.h"
#include "ui/ozone/platform/wayland/xdg_surface_wrapper_v5.h"
#include "ui/ozone/platform/wayland/xdg_surface_wrapper_v6.h"

namespace ui {

namespace {

// Factory, which decides which version type of xdg object to build.
class XDGShellObjectFactory {
 public:
  XDGShellObjectFactory() = default;
  ~XDGShellObjectFactory() = default;

  std::unique_ptr<XDGSurfaceWrapper> CreateXDGSurface(
      WaylandConnection* connection,
      WaylandWindow* wayland_window) {
    if (connection->shell_v6())
      return std::make_unique<XDGSurfaceWrapperV6>(wayland_window);

    DCHECK(connection->shell());
    return std::make_unique<XDGSurfaceWrapperV5>(wayland_window);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(XDGShellObjectFactory);
};

}  // namespace

WaylandWindow::WaylandWindow(PlatformWindowDelegate* delegate,
                             WaylandConnection* connection,
                             const gfx::Rect& bounds)
    : delegate_(delegate),
      connection_(connection),
      xdg_shell_objects_factory_(new XDGShellObjectFactory()),
      bounds_(bounds),
      state_(ui::PlatformWindowState::PLATFORM_WINDOW_STATE_UNKNOWN) {}

WaylandWindow::~WaylandWindow() {
  if (xdg_surface_) {
    PlatformEventSource::GetInstance()->RemovePlatformEventDispatcher(this);
    connection_->RemoveWindow(surface_.id());
  }
}

// static
WaylandWindow* WaylandWindow::FromSurface(wl_surface* surface) {
  return static_cast<WaylandWindow*>(
      wl_proxy_get_user_data(reinterpret_cast<wl_proxy*>(surface)));
}

bool WaylandWindow::Initialize() {
  DCHECK(xdg_shell_objects_factory_);

  surface_.reset(wl_compositor_create_surface(connection_->compositor()));
  if (!surface_) {
    LOG(ERROR) << "Failed to create wl_surface";
    return false;
  }
  wl_surface_set_user_data(surface_.get(), this);

  CreateXdgSurface();

  connection_->ScheduleFlush();

  connection_->AddWindow(surface_.id(), this);
  PlatformEventSource::GetInstance()->AddPlatformEventDispatcher(this);
  delegate_->OnAcceleratedWidgetAvailable(surface_.id(), 1.f);

  return true;
}

void WaylandWindow::CreateXdgSurface() {
  xdg_surface_ =
      xdg_shell_objects_factory_->CreateXDGSurface(connection_, this);
  if (!xdg_surface_ || !xdg_surface_->Initialize(connection_, surface_.get())) {
    CHECK(false) << "Failed to create xdg_surface";
  }
}

void WaylandWindow::ApplyPendingBounds() {
  if (pending_bounds_.IsEmpty())
    return;

  SetBounds(pending_bounds_);
  DCHECK(xdg_surface_);
  xdg_surface_->SetWindowGeometry(bounds_);
  xdg_surface_->AckConfigure();
  pending_bounds_ = gfx::Rect();
  connection_->ScheduleFlush();
}

void WaylandWindow::Show() {}

void WaylandWindow::Hide() {
  NOTIMPLEMENTED();
}

void WaylandWindow::Close() {
  NOTIMPLEMENTED();
}

void WaylandWindow::PrepareForShutdown() {}

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
  xdg_surface_->SetTitle(title);
  connection_->ScheduleFlush();
}

void WaylandWindow::SetCapture() {
  NOTIMPLEMENTED();
}

void WaylandWindow::ReleaseCapture() {
  NOTIMPLEMENTED();
}

void WaylandWindow::ToggleFullscreen() {
  DCHECK(xdg_surface_);
  DCHECK(!IsMinimized());

  // TODO(msisov, tonikitoo): add multiscreen support. As the documentation says
  // if xdg_surface_set_fullscreen() is not provided with wl_output, it's up to
  // the compositor to choose which display will be used to map this surface.
  if (!IsFullscreen())
    xdg_surface_->SetFullscreen();
  else
    xdg_surface_->UnSetFullscreen();
  connection_->ScheduleFlush();
}

void WaylandWindow::Maximize() {
  DCHECK(xdg_surface_);
  DCHECK(!IsMaximized());

  if (IsFullscreen())
    ToggleFullscreen();

  xdg_surface_->SetMaximized();
  connection_->ScheduleFlush();
}

void WaylandWindow::Minimize() {
  DCHECK(xdg_surface_);
  DCHECK(!IsMinimized());

  DCHECK(xdg_surface_);
  xdg_surface_->SetMinimized();
  connection_->ScheduleFlush();

  // Wayland doesn't say if a window is minimized. Handle this case manually
  // here. We can track if the window was unminimized once wayland sends the
  // window is activated, and the previous state was minimized.
  state_ = ui::PlatformWindowState::PLATFORM_WINDOW_STATE_MINIMIZED;
}

void WaylandWindow::Restore() {
  DCHECK(xdg_surface_);

  // Unfullscreen the window if it is fullscreen.
  if (IsFullscreen())
    ToggleFullscreen();

  xdg_surface_->UnSetMaximized();
  connection_->ScheduleFlush();
}

void WaylandWindow::SetCursor(PlatformCursor cursor) {
  scoped_refptr<BitmapCursorOzone> bitmap =
      BitmapCursorFactoryOzone::GetBitmapCursor(cursor);
  if (bitmap_ == bitmap)
    return;

  bitmap_ = bitmap;

  if (bitmap_) {
    connection_->SetCursorBitmap(bitmap_->bitmaps(), bitmap_->hotspot());
  } else {
    connection_->SetCursorBitmap(std::vector<SkBitmap>(), gfx::Point());
  }
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

bool WaylandWindow::CanDispatchEvent(const PlatformEvent& native_event) {
  Event* event = static_cast<Event*>(native_event);
  if (event->IsMouseEvent())
    return has_pointer_focus_;
  if (event->IsKeyEvent())
    return has_keyboard_focus_;
  return false;
}

uint32_t WaylandWindow::DispatchEvent(const PlatformEvent& native_event) {
  DispatchEventFromNativeUiEvent(
      native_event, base::BindOnce(&PlatformWindowDelegate::DispatchEvent,
                                   base::Unretained(delegate_)));
  return POST_DISPATCH_STOP_PROPAGATION;
}

void WaylandWindow::HandleSurfaceConfigure(int32_t width,
                                           int32_t height,
                                           bool is_maximized,
                                           bool is_fullscreen,
                                           bool is_activated) {
  // Change the window state only if the window is activated, because it's the
  // only way to know if the window is not minimized.
  if (is_activated) {
    bool was_minimized = IsMinimized();

    state_ = ui::PlatformWindowState::PLATFORM_WINDOW_STATE_NORMAL;
    if (is_maximized && !is_fullscreen)
      state_ = ui::PlatformWindowState::PLATFORM_WINDOW_STATE_MAXIMIZED;
    else if (is_fullscreen)
      state_ = ui::PlatformWindowState::PLATFORM_WINDOW_STATE_FULLSCREEN;

    // Do not flood the WindowServer unless the previous state was minimized.
    if (was_minimized)
      delegate_->OnWindowStateChanged(state_);
  }

  // Width or height set 0 means that we should decide on width and height by
  // ourselves, but we don't want to set to anything else. Use previous size.
  if (width == 0 || height == 0) {
    width = GetBounds().width();
    height = GetBounds().height();
  }

  // Rather than call SetBounds here for every configure event, just save the
  // most recent bounds, and have WaylandConnection call ApplyPendingBounds
  // when it has finished processing events. We may get many configure events
  // in a row during an interactive resize, and only the last one matters.
  pending_bounds_ = gfx::Rect(0, 0, width, height);
}

void WaylandWindow::OnCloseRequest() {
  NOTIMPLEMENTED();
}

bool WaylandWindow::IsMinimized() const {
  return state_ == ui::PlatformWindowState::PLATFORM_WINDOW_STATE_MINIMIZED;
}

bool WaylandWindow::IsMaximized() const {
  return state_ == ui::PlatformWindowState::PLATFORM_WINDOW_STATE_MAXIMIZED;
}

bool WaylandWindow::IsFullscreen() const {
  return state_ == ui::PlatformWindowState::PLATFORM_WINDOW_STATE_FULLSCREEN;
}

}  // namespace ui
