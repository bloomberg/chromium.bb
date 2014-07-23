// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/window_tree_host_win.h"

#include <windows.h>

#include <algorithm>

#include "base/message_loop/message_loop.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/cursor/cursor_loader_win.h"
#include "ui/base/view_prop.h"
#include "ui/compositor/compositor.h"
#include "ui/events/event.h"
#include "ui/gfx/display.h"
#include "ui/gfx/insets.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/screen.h"
#include "ui/platform_window/win/win_window.h"

using std::max;
using std::min;

namespace aura {
namespace {

bool use_popup_as_root_window_for_test = false;

}  // namespace

// static
WindowTreeHost* WindowTreeHost::Create(const gfx::Rect& bounds) {
  return new WindowTreeHostWin(bounds);
}

// static
gfx::Size WindowTreeHost::GetNativeScreenSize() {
  return gfx::Size(GetSystemMetrics(SM_CXSCREEN),
                   GetSystemMetrics(SM_CYSCREEN));
}

WindowTreeHostWin::WindowTreeHostWin(const gfx::Rect& bounds)
    : has_capture_(false),
      widget_(gfx::kNullAcceleratedWidget),
      window_(new ui::WinWindow(this, bounds)) {
}

WindowTreeHostWin::~WindowTreeHostWin() {
  DestroyCompositor();
  DestroyDispatcher();
  window_.reset();
}

ui::EventSource* WindowTreeHostWin::GetEventSource() {
  return this;
}

gfx::AcceleratedWidget WindowTreeHostWin::GetAcceleratedWidget() {
  return widget_;
}

void WindowTreeHostWin::Show() {
  window_->Show();
}

void WindowTreeHostWin::Hide() {
  window_->Hide();
}

gfx::Rect WindowTreeHostWin::GetBounds() const {
  return window_->GetBounds();
}

void WindowTreeHostWin::SetBounds(const gfx::Rect& bounds) {
  window_->SetBounds(bounds);
}

gfx::Point WindowTreeHostWin::GetLocationOnNativeScreen() const {
  return window_->GetBounds().origin();
}

void WindowTreeHostWin::SetCapture() {
  if (!has_capture_) {
    has_capture_ = true;
    window_->SetCapture();
  }
}

void WindowTreeHostWin::ReleaseCapture() {
  if (has_capture_)
    window_->ReleaseCapture();
}

void WindowTreeHostWin::SetCursorNative(gfx::NativeCursor native_cursor) {
  // Custom web cursors are handled directly.
  if (native_cursor == ui::kCursorCustom)
    return;

  ui::CursorLoaderWin cursor_loader;
  cursor_loader.SetPlatformCursor(&native_cursor);
  ::SetCursor(native_cursor.platform());
}

void WindowTreeHostWin::MoveCursorToNative(const gfx::Point& location) {
  // Deliberately not implemented.
}

void WindowTreeHostWin::OnCursorVisibilityChangedNative(bool show) {
  NOTIMPLEMENTED();
}

void WindowTreeHostWin::PostNativeEvent(const base::NativeEvent& native_event) {
  ::PostMessage(
      widget_, native_event.message, native_event.wParam, native_event.lParam);
}

ui::EventProcessor* WindowTreeHostWin::GetEventProcessor() {
  return dispatcher();
}

void WindowTreeHostWin::OnBoundsChanged(const gfx::Rect& new_bounds) {
  gfx::Rect old_bounds = bounds_;
  bounds_ = new_bounds;
  if (bounds_.origin() != old_bounds.origin())
    OnHostMoved(bounds_.origin());
  if (bounds_.size() != old_bounds.size())
    OnHostResized(bounds_.size());
}

void WindowTreeHostWin::OnDamageRect(const gfx::Rect& damage_rect) {
  compositor()->ScheduleRedrawRect(damage_rect);
}

void WindowTreeHostWin::DispatchEvent(ui::Event* event) {
  ui::EventDispatchDetails details = SendEventToProcessor(event);
  if (details.dispatcher_destroyed)
    event->SetHandled();
}

void WindowTreeHostWin::OnCloseRequest() {
  // TODO: this obviously shouldn't be here.
  base::MessageLoopForUI::current()->Quit();
}

void WindowTreeHostWin::OnClosed() {
}

void WindowTreeHostWin::OnWindowStateChanged(
    ui::PlatformWindowState new_state) {
}

void WindowTreeHostWin::OnLostCapture() {
  if (has_capture_) {
    has_capture_ = false;
    OnHostLostWindowCapture();
  }
}

void WindowTreeHostWin::OnAcceleratedWidgetAvailable(
    gfx::AcceleratedWidget widget) {
  widget_ = widget;
  CreateCompositor(widget);
}

void WindowTreeHostWin::OnActivationChanged(bool active) {
  if (active)
    OnHostActivated();
}

}  // namespace aura
