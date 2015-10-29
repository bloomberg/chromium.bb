// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/mus/window_tree_host_mus.h"

#include "components/mus/public/cpp/property_type_converters.h"
#include "components/mus/public/cpp/window_property.h"
#include "components/mus/public/cpp/window_tree_connection.h"
#include "mojo/application/public/interfaces/shell.mojom.h"
#include "mojo/converters/geometry/geometry_type_converters.h"
#include "mojo/converters/input_events/input_events_type_converters.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/views/mus/input_method_mus.h"
#include "ui/views/mus/surface_context_factory.h"
#include "ui/views/mus/window_manager_connection.h"

namespace views {
namespace {

void WindowManagerCallback(mus::mojom::WindowManagerErrorCode error_code) {}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// WindowTreeHostMojo, public:

WindowTreeHostMojo::WindowTreeHostMojo(mojo::Shell* shell, mus::Window* window)
    : mus_window_(window) {
  mus_window_->AddObserver(this);

  context_factory_.reset(new SurfaceContextFactory(shell, mus_window_));
  // WindowTreeHost creates the compositor using the ContextFactory from
  // aura::Env. Install |context_factory_| there so that |context_factory_| is
  // picked up.
  ui::ContextFactory* default_context_factory =
      aura::Env::GetInstance()->context_factory();
  aura::Env::GetInstance()->set_context_factory(context_factory_.get());
  CreateCompositor();
  OnAcceleratedWidgetAvailable();
  aura::Env::GetInstance()->set_context_factory(default_context_factory);
  DCHECK_EQ(context_factory_.get(), compositor()->context_factory());

  input_method_.reset(new InputMethodMUS(this, mus_window_));
  SetSharedInputMethod(input_method_.get());
}

WindowTreeHostMojo::~WindowTreeHostMojo() {
  mus_window_->RemoveObserver(this);
  DestroyCompositor();
  DestroyDispatcher();
}

void WindowTreeHostMojo::SetShowState(mus::mojom::ShowState show_state) {
  show_state_ = show_state;
  WindowManagerConnection::Get()->window_manager()->SetShowState(
      mus_window_->id(), show_state_, base::Bind(&WindowManagerCallback));
}

////////////////////////////////////////////////////////////////////////////////
// WindowTreeHostMojo, aura::WindowTreeHost implementation:

ui::EventSource* WindowTreeHostMojo::GetEventSource() {
  return this;
}

gfx::AcceleratedWidget WindowTreeHostMojo::GetAcceleratedWidget() {
  return gfx::kNullAcceleratedWidget;
}

void WindowTreeHostMojo::ShowImpl() {
  mus_window_->SetVisible(true);
  window()->Show();
}

void WindowTreeHostMojo::HideImpl() {
  mus_window_->SetVisible(false);
  window()->Hide();
}

gfx::Rect WindowTreeHostMojo::GetBounds() const {
  return mus_window_->bounds();
}

void WindowTreeHostMojo::SetBounds(const gfx::Rect& bounds) {
  window()->SetBounds(gfx::Rect(bounds.size()));
}

gfx::Point WindowTreeHostMojo::GetLocationOnNativeScreen() const {
  return gfx::Point(0, 0);
}

void WindowTreeHostMojo::SetCapture() {
  NOTIMPLEMENTED();
}

void WindowTreeHostMojo::ReleaseCapture() {
  NOTIMPLEMENTED();
}

void WindowTreeHostMojo::SetCursorNative(gfx::NativeCursor cursor) {
  NOTIMPLEMENTED();
}

void WindowTreeHostMojo::MoveCursorToNative(const gfx::Point& location) {
  NOTIMPLEMENTED();
}

void WindowTreeHostMojo::OnCursorVisibilityChangedNative(bool show) {
  NOTIMPLEMENTED();
}

////////////////////////////////////////////////////////////////////////////////
// WindowTreeHostMojo, WindowObserver implementation:

void WindowTreeHostMojo::OnWindowBoundsChanged(mus::Window* window,
                                               const gfx::Rect& old_bounds,
                                               const gfx::Rect& new_bounds) {
  if (old_bounds.origin() != new_bounds.origin())
    OnHostMoved(new_bounds.origin());
  if (old_bounds.size() != new_bounds.size())
    OnHostResized(new_bounds.size());
}

void WindowTreeHostMojo::OnWindowFocusChanged(mus::Window* gained_focus,
                                              mus::Window* lost_focus) {
  if (gained_focus == mus_window_)
    GetInputMethod()->OnFocus();
  else if (lost_focus == mus_window_)
    GetInputMethod()->OnBlur();
}

void WindowTreeHostMojo::OnWindowInputEvent(mus::Window* view,
                                            const mus::mojom::EventPtr& event) {
  scoped_ptr<ui::Event> ui_event(event.To<scoped_ptr<ui::Event>>());
  if (!ui_event)
    return;

  if (ui_event->IsKeyEvent()) {
    GetInputMethod()->DispatchKeyEvent(
        static_cast<ui::KeyEvent*>(ui_event.get()));
  } else {
    SendEventToProcessor(ui_event.get());
  }
}

void WindowTreeHostMojo::OnWindowSharedPropertyChanged(
    mus::Window* window,
    const std::string& name,
    const std::vector<uint8_t>* old_data,
    const std::vector<uint8_t>* new_data) {
  if (name == mus::mojom::WindowManager::kShowState_Property) {
    show_state_ = static_cast<mus::mojom::ShowState>(
        window->GetSharedProperty<int32_t>(
            mus::mojom::WindowManager::kShowState_Property));
  }
}

}  // namespace views
