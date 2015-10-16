// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/mus/window_tree_host_mus.h"

#include "components/mus/public/cpp/window_tree_connection.h"
#include "mojo/application/public/interfaces/shell.mojom.h"
#include "mojo/converters/geometry/geometry_type_converters.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/views/mus/input_method_mus.h"
#include "ui/views/mus/surface_context_factory.h"

namespace views {

////////////////////////////////////////////////////////////////////////////////
// WindowTreeHostMojo, public:

WindowTreeHostMojo::WindowTreeHostMojo(mojo::Shell* shell, mus::Window* window)
    : window_(window), bounds_(window->bounds().To<gfx::Rect>()) {
  window_->AddObserver(this);

  context_factory_.reset(new SurfaceContextFactory(shell, window_));
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

  input_method_.reset(new InputMethodMUS(this, window_));
  SetSharedInputMethod(input_method_.get());
}

WindowTreeHostMojo::~WindowTreeHostMojo() {
  window_->RemoveObserver(this);
  DestroyCompositor();
  DestroyDispatcher();
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
  window()->Show();
}

void WindowTreeHostMojo::HideImpl() {}

gfx::Rect WindowTreeHostMojo::GetBounds() const {
  return bounds_;
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
// WindowTreeHostMojo, ViewObserver implementation:

void WindowTreeHostMojo::OnWindowBoundsChanged(mus::Window* window,
                                               const mojo::Rect& old_bounds,
                                               const mojo::Rect& new_bounds) {
  gfx::Rect old_bounds2 = old_bounds.To<gfx::Rect>();
  gfx::Rect new_bounds2 = new_bounds.To<gfx::Rect>();
  bounds_ = new_bounds2;
  if (old_bounds2.origin() != new_bounds2.origin())
    OnHostMoved(bounds_.origin());
  if (old_bounds2.size() != new_bounds2.size())
    OnHostResized(bounds_.size());
}

}  // namespace views
