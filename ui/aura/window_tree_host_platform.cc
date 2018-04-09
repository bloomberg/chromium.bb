// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/window_tree_host_platform.h"

#include <utility>

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_port.h"
#include "ui/base/layout.h"
#include "ui/compositor/compositor.h"
#include "ui/events/event.h"

#if defined(OS_ANDROID)
#include "ui/platform_window/android/platform_window_android.h"
#endif

#if defined(USE_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

#if defined(OS_WIN)
#include "ui/base/cursor/cursor_loader_win.h"
#include "ui/platform_window/win/win_window.h"
#endif

#if defined(USE_X11)
#include "ui/platform_window/x11/x11_window.h"
#endif

namespace aura {

// static
WindowTreeHost* WindowTreeHost::Create(const gfx::Rect& bounds) {
  return new WindowTreeHostPlatform(bounds);
}

WindowTreeHostPlatform::WindowTreeHostPlatform(const gfx::Rect& bounds)
    : WindowTreeHostPlatform() {
  bounds_ = bounds;
  CreateCompositor();
  CreateAndSetDefaultPlatformWindow();
}

WindowTreeHostPlatform::WindowTreeHostPlatform()
    : WindowTreeHostPlatform(nullptr) {}

WindowTreeHostPlatform::WindowTreeHostPlatform(
    std::unique_ptr<WindowPort> window_port)
    : WindowTreeHost(std::move(window_port)),
      widget_(gfx::kNullAcceleratedWidget),
      current_cursor_(ui::CursorType::kNull) {}

void WindowTreeHostPlatform::CreateAndSetDefaultPlatformWindow() {
#if defined(USE_OZONE)
  platform_window_ =
      ui::OzonePlatform::GetInstance()->CreatePlatformWindow(this, bounds_);
#elif defined(OS_WIN)
  platform_window_.reset(new ui::WinWindow(this, bounds_));
#elif defined(OS_ANDROID)
  platform_window_.reset(new ui::PlatformWindowAndroid(this));
#elif defined(USE_X11)
  platform_window_.reset(new ui::X11Window(this, bounds_));
#else
  NOTIMPLEMENTED();
#endif
}

void WindowTreeHostPlatform::SetPlatformWindow(
    std::unique_ptr<ui::PlatformWindow> window) {
  platform_window_ = std::move(window);
}

WindowTreeHostPlatform::~WindowTreeHostPlatform() {
  DestroyCompositor();
  DestroyDispatcher();
  platform_window_->Close();
}

ui::EventSource* WindowTreeHostPlatform::GetEventSource() {
  return this;
}

gfx::AcceleratedWidget WindowTreeHostPlatform::GetAcceleratedWidget() {
  return widget_;
}

void WindowTreeHostPlatform::ShowImpl() {
  platform_window_->Show();
}

void WindowTreeHostPlatform::HideImpl() {
  platform_window_->Hide();
}

gfx::Rect WindowTreeHostPlatform::GetBoundsInPixels() const {
  return platform_window_ ? platform_window_->GetBounds() : gfx::Rect();
}

void WindowTreeHostPlatform::SetBoundsInPixels(const gfx::Rect& bounds) {
  platform_window_->SetBounds(bounds);
}

gfx::Point WindowTreeHostPlatform::GetLocationOnScreenInPixels() const {
  return platform_window_->GetBounds().origin();
}

void WindowTreeHostPlatform::SetCapture() {
  platform_window_->SetCapture();
}

void WindowTreeHostPlatform::ReleaseCapture() {
  platform_window_->ReleaseCapture();
}

bool WindowTreeHostPlatform::CaptureSystemKeyEventsImpl(
    base::Optional<base::flat_set<int>> native_key_codes) {
  // TODO(680809): Implement as part of the KeyboardLock feature work.
  NOTIMPLEMENTED();
  return false;
}

void WindowTreeHostPlatform::ReleaseSystemKeyEventCapture() {}

bool WindowTreeHostPlatform::IsKeyLocked(int native_key_code) {
  // TODO(680809): Implement as part of the KeyboardLock feature work.
  NOTIMPLEMENTED();
  return false;
}

void WindowTreeHostPlatform::SetCursorNative(gfx::NativeCursor cursor) {
  if (cursor == current_cursor_)
    return;
  current_cursor_ = cursor;

#if defined(OS_WIN)
  ui::CursorLoaderWin cursor_loader;
  cursor_loader.SetPlatformCursor(&cursor);
#endif

  platform_window_->SetCursor(cursor.platform());
}

void WindowTreeHostPlatform::MoveCursorToScreenLocationInPixels(
    const gfx::Point& location_in_pixels) {
  platform_window_->MoveCursorTo(location_in_pixels);
}

void WindowTreeHostPlatform::OnCursorVisibilityChangedNative(bool show) {
  NOTIMPLEMENTED();
}

void WindowTreeHostPlatform::OnBoundsChanged(const gfx::Rect& new_bounds) {
  float current_scale = compositor()->device_scale_factor();
  float new_scale = ui::GetScaleFactorForNativeView(window());
  gfx::Rect old_bounds = bounds_;
  bounds_ = new_bounds;
  if (bounds_.origin() != old_bounds.origin())
    OnHostMovedInPixels(bounds_.origin());
  if (bounds_.size() != old_bounds.size() || current_scale != new_scale)
    OnHostResizedInPixels(bounds_.size());
}

void WindowTreeHostPlatform::OnDamageRect(const gfx::Rect& damage_rect) {
  compositor()->ScheduleRedrawRect(damage_rect);
}

void WindowTreeHostPlatform::DispatchEvent(ui::Event* event) {
  TRACE_EVENT0("input", "WindowTreeHostPlatform::DispatchEvent");
  ui::EventDispatchDetails details = SendEventToSink(event);
  if (details.dispatcher_destroyed)
    event->SetHandled();
}

void WindowTreeHostPlatform::OnCloseRequest() {
#if defined(OS_WIN)
  // TODO: this obviously shouldn't be here.
  base::RunLoop::QuitCurrentWhenIdleDeprecated();
#else
  OnHostCloseRequested();
#endif
}

void WindowTreeHostPlatform::OnClosed() {}

void WindowTreeHostPlatform::OnWindowStateChanged(
    ui::PlatformWindowState new_state) {}

void WindowTreeHostPlatform::OnLostCapture() {
  OnHostLostWindowCapture();
}

void WindowTreeHostPlatform::OnAcceleratedWidgetAvailable(
    gfx::AcceleratedWidget widget,
    float device_pixel_ratio) {
  widget_ = widget;
  // This may be called before the Compositor has been created.
  if (compositor())
    WindowTreeHost::OnAcceleratedWidgetAvailable();
}

void WindowTreeHostPlatform::OnAcceleratedWidgetDestroyed() {
  gfx::AcceleratedWidget widget = compositor()->ReleaseAcceleratedWidget();
  DCHECK_EQ(widget, widget_);
  widget_ = gfx::kNullAcceleratedWidget;
}

void WindowTreeHostPlatform::OnActivationChanged(bool active) {
  if (active)
    OnHostActivated();
}

}  // namespace aura
