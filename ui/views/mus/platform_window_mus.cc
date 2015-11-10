// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/mus/platform_window_mus.h"

#include "components/mus/public/cpp/property_type_converters.h"
#include "components/mus/public/cpp/window_property.h"
#include "components/mus/public/interfaces/window_manager.mojom.h"
#include "mojo/converters/input_events/input_events_type_converters.h"
#include "ui/platform_window/platform_window_delegate.h"
#include "ui/views/mus/window_manager_connection.h"

namespace views {

namespace {
void WindowManagerCallback(mus::mojom::WindowManagerErrorCode error_code) {}
}  // namespace

PlatformWindowMus::PlatformWindowMus(ui::PlatformWindowDelegate* delegate,
                                     mus::Window* mus_window)
    : delegate_(delegate),
      mus_window_(mus_window),
      show_state_(mus::mojom::SHOW_STATE_RESTORED) {
  DCHECK(delegate_);
  DCHECK(mus_window_);
  mus_window_->AddObserver(this);

  delegate_->OnAcceleratedWidgetAvailable(
      gfx::kNullAcceleratedWidget,
      mus_window_->viewport_metrics().device_pixel_ratio);
}

PlatformWindowMus::~PlatformWindowMus() {
  if (!mus_window_)
    return;
  mus_window_->RemoveObserver(this);
  mus_window_->Destroy();
}


void PlatformWindowMus::Activate() {
  mus_window_->SetFocus();
}

void PlatformWindowMus::Show() {
  mus_window_->SetVisible(true);
}

void PlatformWindowMus::Hide() {
  mus_window_->SetVisible(false);
}

void PlatformWindowMus::Close() {
  NOTIMPLEMENTED();
}

void PlatformWindowMus::SetBounds(const gfx::Rect& bounds) {
  mus_window_->SetBounds(bounds);
}

gfx::Rect PlatformWindowMus::GetBounds() {
  return mus_window_->bounds();
}

void PlatformWindowMus::SetTitle(const base::string16& title) {
  NOTIMPLEMENTED();
}

void PlatformWindowMus::SetCapture() {
  NOTIMPLEMENTED();
}

void PlatformWindowMus::ReleaseCapture() {
  NOTIMPLEMENTED();
}

void PlatformWindowMus::ToggleFullscreen() {
  NOTIMPLEMENTED();
}

void PlatformWindowMus::Maximize() {
  SetShowState(mus::mojom::SHOW_STATE_MAXIMIZED);
}

void PlatformWindowMus::Minimize() {
  SetShowState(mus::mojom::SHOW_STATE_MINIMIZED);
}

void PlatformWindowMus::Restore() {
  SetShowState(mus::mojom::SHOW_STATE_RESTORED);
}

void PlatformWindowMus::SetCursor(ui::PlatformCursor cursor) {
  NOTIMPLEMENTED();
}

void PlatformWindowMus::MoveCursorTo(const gfx::Point& location) {
  NOTIMPLEMENTED();
}

void PlatformWindowMus::ConfineCursorToBounds(const gfx::Rect& bounds) {
  NOTIMPLEMENTED();
}

ui::PlatformImeController* PlatformWindowMus::GetPlatformImeController() {
  return nullptr;
}

void PlatformWindowMus::SetShowState(mus::mojom::ShowState show_state) {
  WindowManagerConnection::Get()->window_manager()->SetShowState(
      mus_window_->id(), show_state, base::Bind(&WindowManagerCallback));
}

void PlatformWindowMus::OnWindowDestroyed(mus::Window* window) {
  DCHECK_EQ(mus_window_, window);
  delegate_->OnClosed();
  mus_window_ = nullptr;
}

void PlatformWindowMus::OnWindowBoundsChanged(mus::Window* window,
                                              const gfx::Rect& old_bounds,
                                              const gfx::Rect& new_bounds) {
  delegate_->OnBoundsChanged(new_bounds);
}

void PlatformWindowMus::OnWindowFocusChanged(mus::Window* gained_focus,
                                             mus::Window* lost_focus) {
  if (gained_focus == mus_window_)
    delegate_->OnActivationChanged(true);
  else if (lost_focus == mus_window_)
    delegate_->OnActivationChanged(false);
}

void PlatformWindowMus::OnWindowInputEvent(mus::Window* view,
                                           const mus::mojom::EventPtr& event) {
  scoped_ptr<ui::Event> ui_event(event.To<scoped_ptr<ui::Event>>());
  delegate_->DispatchEvent(ui_event.get());
}

void PlatformWindowMus::OnWindowSharedPropertyChanged(
    mus::Window* window,
    const std::string& name,
    const std::vector<uint8_t>* old_data,
    const std::vector<uint8_t>* new_data) {
  if (name != mus::mojom::WindowManager::kShowState_Property)
    return;
  mus::mojom::ShowState show_state =
      static_cast<mus::mojom::ShowState>(window->GetSharedProperty<int32_t>(
          mus::mojom::WindowManager::kShowState_Property));
  if (show_state == show_state_)
    return;
  show_state_ = show_state;
  ui::PlatformWindowState state = ui::PLATFORM_WINDOW_STATE_UNKNOWN;
  switch (show_state_) {
    case mus::mojom::SHOW_STATE_MINIMIZED:
      state = ui::PLATFORM_WINDOW_STATE_MINIMIZED;
      break;
    case mus::mojom::SHOW_STATE_MAXIMIZED:
      state = ui::PLATFORM_WINDOW_STATE_MAXIMIZED;
      break;
    case mus::mojom::SHOW_STATE_RESTORED:
      state = ui::PLATFORM_WINDOW_STATE_NORMAL;
      break;
    case mus::mojom::SHOW_STATE_IMMERSIVE:
    case mus::mojom::SHOW_STATE_PRESENTATION:
      // This may not be sufficient.
      state = ui::PLATFORM_WINDOW_STATE_FULLSCREEN;
      break;
  }
  delegate_->OnWindowStateChanged(state);
}

}  // namespace views
