// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/window_tree_host_platform.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tree_host_observer.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/base/layout.h"
#include "ui/compositor/compositor.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/events/keyboard_hook.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/platform_window/platform_window.h"
#include "ui/platform_window/platform_window_init_properties.h"

#if defined(USE_OZONE)
#include "ui/events/keycodes/dom/dom_keyboard_layout_map.h"
#include "ui/ozone/public/ozone_platform.h"
#endif

#if defined(OS_WIN)
#include "ui/platform_window/win/win_window.h"
#endif

namespace aura {

// static
std::unique_ptr<WindowTreeHost> WindowTreeHost::Create(
    ui::PlatformWindowInitProperties properties) {
  return std::make_unique<WindowTreeHostPlatform>(
      std::move(properties),
      std::make_unique<aura::Window>(nullptr, client::WINDOW_TYPE_UNKNOWN));
}

WindowTreeHostPlatform::WindowTreeHostPlatform(
    ui::PlatformWindowInitProperties properties,
    std::unique_ptr<Window> window)
    : WindowTreeHost(std::move(window)) {
  bounds_in_pixels_ = properties.bounds;
  CreateCompositor(false, false,
                   properties.enable_compositing_based_throttling);
  CreateAndSetPlatformWindow(std::move(properties));
}

WindowTreeHostPlatform::WindowTreeHostPlatform(std::unique_ptr<Window> window)
    : WindowTreeHost(std::move(window)),
      widget_(gfx::kNullAcceleratedWidget),
      current_cursor_(ui::mojom::CursorType::kNull) {}

void WindowTreeHostPlatform::CreateAndSetPlatformWindow(
    ui::PlatformWindowInitProperties properties) {
  // Cache initial bounds used to create |platform_window_| so that it does not
  // end up propagating unneeded bounds change event when it is first notified
  // through OnBoundsChanged, which may lead to unneeded re-layouts, etc.
  bounds_in_pixels_ = properties.bounds;

#if defined(USE_OZONE)
  platform_window_ = ui::OzonePlatform::GetInstance()->CreatePlatformWindow(
      this, std::move(properties));
#elif defined(OS_WIN)
  platform_window_ = std::make_unique<ui::WinWindow>(this, properties.bounds);
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

  // |platform_window_| may not exist yet.
  if (platform_window_)
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
  pending_size_ = bounds.size();
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
    absl::optional<base::flat_set<ui::DomCode>> dom_codes) {
  // Only one KeyboardHook should be active at a time, otherwise there will be
  // problems with event routing (i.e. which Hook takes precedence) and
  // destruction ordering.
  DCHECK(!keyboard_hook_);
  keyboard_hook_ = ui::KeyboardHook::CreateModifierKeyboardHook(
      std::move(dom_codes), GetAcceleratedWidget(),
      base::BindRepeating(
          [](ui::PlatformWindowDelegate* delegate, ui::KeyEvent* event) {
            delegate->DispatchEvent(event);
          },
          base::Unretained(this)));

  return keyboard_hook_ != nullptr;
}

void WindowTreeHostPlatform::ReleaseSystemKeyEventCapture() {
  keyboard_hook_.reset();
}

bool WindowTreeHostPlatform::IsKeyLocked(ui::DomCode dom_code) {
  return keyboard_hook_ && keyboard_hook_->IsKeyLocked(dom_code);
}

base::flat_map<std::string, std::string>
WindowTreeHostPlatform::GetKeyboardLayoutMap() {
#if defined(USE_OZONE)
  return ui::GenerateDomKeyboardLayoutMap();
#else
  NOTIMPLEMENTED();
  return {};
#endif
}

void WindowTreeHostPlatform::SetCursorNative(gfx::NativeCursor cursor) {
  if (cursor == current_cursor_)
    return;
  current_cursor_ = cursor;

  platform_window_->SetCursor(cursor.platform());
}

void WindowTreeHostPlatform::MoveCursorToScreenLocationInPixels(
    const gfx::Point& location_in_pixels) {
  platform_window_->MoveCursorTo(location_in_pixels);
}

void WindowTreeHostPlatform::OnCursorVisibilityChangedNative(bool show) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void WindowTreeHostPlatform::LockMouse(Window* window) {
  window->SetCapture();
  WindowTreeHost::LockMouse(window);
}

void WindowTreeHostPlatform::OnBoundsChanged(const BoundsChange& change) {
  // It's possible this function may be called recursively. Only notify
  // observers on initial entry. This way observers can safely assume that
  // OnHostDidProcessBoundsChange() is called when all bounds changes have
  // completed.
  if (++on_bounds_changed_recursion_depth_ == 1) {
    for (WindowTreeHostObserver& observer : observers())
      observer.OnHostWillProcessBoundsChange(this);
  }
  float current_scale = compositor()->device_scale_factor();
  float new_scale = ui::GetScaleFactorForNativeView(window());
  gfx::Rect old_bounds = bounds_in_pixels_;
  auto weak_ref = GetWeakPtr();
  bounds_in_pixels_ = change.bounds;
  if (bounds_in_pixels_.origin() != old_bounds.origin()) {
    OnHostMovedInPixels(bounds_in_pixels_.origin());
    // Changing the bounds may destroy this.
    if (!weak_ref)
      return;
  }
  if (bounds_in_pixels_.size() != old_bounds.size() ||
      current_scale != new_scale) {
    pending_size_ = gfx::Size();
    OnHostResizedInPixels(bounds_in_pixels_.size());
    // Changing the size may destroy this.
    if (!weak_ref)
      return;
  }
  DCHECK_GT(on_bounds_changed_recursion_depth_, 0);
  if (--on_bounds_changed_recursion_depth_ == 0) {
    for (WindowTreeHostObserver& observer : observers())
      observer.OnHostDidProcessBoundsChange(this);
  }
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
  OnHostCloseRequested();
}

void WindowTreeHostPlatform::OnClosed() {}

void WindowTreeHostPlatform::OnWindowStateChanged(
    ui::PlatformWindowState old_state,
    ui::PlatformWindowState new_state) {}

void WindowTreeHostPlatform::OnLostCapture() {
  OnHostLostWindowCapture();
}

void WindowTreeHostPlatform::OnAcceleratedWidgetAvailable(
    gfx::AcceleratedWidget widget) {
  widget_ = widget;
  // This may be called before the Compositor has been created.
  if (compositor())
    WindowTreeHost::OnAcceleratedWidgetAvailable();
}

void WindowTreeHostPlatform::OnWillDestroyAcceleratedWidget() {}

void WindowTreeHostPlatform::OnAcceleratedWidgetDestroyed() {
  gfx::AcceleratedWidget widget = compositor()->ReleaseAcceleratedWidget();
  DCHECK_EQ(widget, widget_);
  widget_ = gfx::kNullAcceleratedWidget;
}

void WindowTreeHostPlatform::OnActivationChanged(bool active) {}

void WindowTreeHostPlatform::OnMouseEnter() {
  client::CursorClient* cursor_client = client::GetCursorClient(window());
  if (cursor_client) {
    auto display =
        display::Screen::GetScreen()->GetDisplayNearestWindow(window());
    DCHECK(display.is_valid());
    cursor_client->SetDisplay(display);
  }
}

void WindowTreeHostPlatform::OnOcclusionStateChanged(
    ui::PlatformWindowOcclusionState occlusion_state) {
  auto aura_occlusion_state = Window::OcclusionState::UNKNOWN;
  switch (occlusion_state) {
    case ui::PlatformWindowOcclusionState::kUnknown:
      aura_occlusion_state = Window::OcclusionState::UNKNOWN;
      break;
    case ui::PlatformWindowOcclusionState::kVisible:
      aura_occlusion_state = Window::OcclusionState::VISIBLE;
      break;
    case ui::PlatformWindowOcclusionState::kOccluded:
      aura_occlusion_state = Window::OcclusionState::OCCLUDED;
      break;
    case ui::PlatformWindowOcclusionState::kHidden:
      aura_occlusion_state = Window::OcclusionState::HIDDEN;
      break;
  }
  SetNativeWindowOcclusionState(aura_occlusion_state, {});
}

}  // namespace aura
