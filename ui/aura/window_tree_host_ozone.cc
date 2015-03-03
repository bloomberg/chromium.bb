// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/window_tree_host_ozone.h"

#include "base/trace_event/trace_event.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/platform_window/platform_window.h"

namespace aura {

WindowTreeHostOzone::WindowTreeHostOzone(const gfx::Rect& bounds)
    : widget_(gfx::kNullAcceleratedWidget), current_cursor_(ui::kCursorNull) {
  platform_window_ =
      ui::OzonePlatform::GetInstance()->CreatePlatformWindow(this, bounds);
}

WindowTreeHostOzone::~WindowTreeHostOzone() {
  DestroyCompositor();
  DestroyDispatcher();
}

ui::EventSource* WindowTreeHostOzone::GetEventSource() {
  return this;
}

gfx::AcceleratedWidget WindowTreeHostOzone::GetAcceleratedWidget() {
  return widget_;
}

void WindowTreeHostOzone::Show() {
  platform_window_->Show();
}

void WindowTreeHostOzone::Hide() {
  platform_window_->Hide();
}

gfx::Rect WindowTreeHostOzone::GetBounds() const {
  return platform_window_->GetBounds();
}

void WindowTreeHostOzone::SetBounds(const gfx::Rect& bounds) {
  platform_window_->SetBounds(bounds);
}

gfx::Point WindowTreeHostOzone::GetLocationOnNativeScreen() const {
  return platform_window_->GetBounds().origin();
}

void WindowTreeHostOzone::SetCapture() {
  platform_window_->SetCapture();
}

void WindowTreeHostOzone::ReleaseCapture() {
  platform_window_->ReleaseCapture();
}

void WindowTreeHostOzone::SetCursorNative(gfx::NativeCursor cursor) {
  if (cursor == current_cursor_)
    return;
  current_cursor_ = cursor;
  platform_window_->SetCursor(cursor.platform());
}

void WindowTreeHostOzone::MoveCursorToNative(const gfx::Point& location) {
  platform_window_->MoveCursorTo(location);
}

void WindowTreeHostOzone::OnCursorVisibilityChangedNative(bool show) {
}

void WindowTreeHostOzone::OnBoundsChanged(const gfx::Rect& new_bounds) {
  // TOOD(spang): Should we determine which parts changed?
  OnHostResized(new_bounds.size());
  OnHostMoved(new_bounds.origin());
}

void WindowTreeHostOzone::OnDamageRect(const gfx::Rect& damaged_region) {
}

void WindowTreeHostOzone::DispatchEvent(ui::Event* event) {
  TRACE_EVENT0("input", "WindowTreeHostOzone::DispatchEvent");
  SendEventToProcessor(event);
}

void WindowTreeHostOzone::OnCloseRequest() {
  OnHostCloseRequested();
}

void WindowTreeHostOzone::OnClosed() {
}

void WindowTreeHostOzone::OnWindowStateChanged(
    ui::PlatformWindowState new_state) {
}

void WindowTreeHostOzone::OnLostCapture() {
}

void WindowTreeHostOzone::OnAcceleratedWidgetAvailable(
    gfx::AcceleratedWidget widget) {
  widget_ = widget;
  CreateCompositor(widget_);
}

void WindowTreeHostOzone::OnActivationChanged(bool active) {
}

ui::EventProcessor* WindowTreeHostOzone::GetEventProcessor() {
  return dispatcher();
}

// static
WindowTreeHost* WindowTreeHost::Create(const gfx::Rect& bounds) {
  return new WindowTreeHostOzone(bounds);
}

// static
gfx::Size WindowTreeHost::GetNativeScreenSize() {
  NOTIMPLEMENTED();
  return gfx::Size();
}

}  // namespace aura
