// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/mus/window_tree_host_mus.h"

#include "mojo/application/public/interfaces/shell.mojom.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/events/event.h"
#include "ui/views/mus/input_method_mus.h"
#include "ui/views/mus/native_widget_mus.h"
#include "ui/views/mus/platform_window_mus.h"
#include "ui/views/mus/surface_context_factory.h"

namespace views {

////////////////////////////////////////////////////////////////////////////////
// WindowTreeHostMus, public:

WindowTreeHostMus::WindowTreeHostMus(mojo::Shell* shell,
                                     NativeWidgetMus* native_widget,
                                     mus::Window* window,
                                     mus::mojom::SurfaceType surface_type)
    : native_widget_(native_widget),
      show_state_(ui::PLATFORM_WINDOW_STATE_UNKNOWN) {
  context_factory_.reset(
      new SurfaceContextFactory(shell, window, surface_type));
  // WindowTreeHost creates the compositor using the ContextFactory from
  // aura::Env. Install |context_factory_| there so that |context_factory_| is
  // picked up.
  ui::ContextFactory* default_context_factory =
      aura::Env::GetInstance()->context_factory();
  aura::Env::GetInstance()->set_context_factory(context_factory_.get());
  SetPlatformWindow(make_scoped_ptr(new PlatformWindowMus(this, window)));
  compositor()->SetHostHasTransparentBackground(true);
  aura::Env::GetInstance()->set_context_factory(default_context_factory);
  DCHECK_EQ(context_factory_.get(), compositor()->context_factory());

  input_method_.reset(new InputMethodMUS(this, window));
  SetSharedInputMethod(input_method_.get());
}

WindowTreeHostMus::~WindowTreeHostMus() {
  DestroyCompositor();
  DestroyDispatcher();
}

PlatformWindowMus* WindowTreeHostMus::platform_window() {
  return static_cast<PlatformWindowMus*>(
      WindowTreeHostPlatform::platform_window());
}

void WindowTreeHostMus::DispatchEvent(ui::Event* event) {
  if (event->IsKeyEvent() && GetInputMethod()) {
    GetInputMethod()->DispatchKeyEvent(static_cast<ui::KeyEvent*>(event));
    event->StopPropagation();
    return;
  }
  WindowTreeHostPlatform::DispatchEvent(event);
}

void WindowTreeHostMus::OnClosed() {
  if (native_widget_)
    native_widget_->OnPlatformWindowClosed();
}

void WindowTreeHostMus::OnWindowStateChanged(ui::PlatformWindowState state) {
  show_state_ = state;
}

void WindowTreeHostMus::OnActivationChanged(bool active) {
  if (active)
    GetInputMethod()->OnFocus();
  else
    GetInputMethod()->OnBlur();
  if (native_widget_)
    native_widget_->OnActivationChanged(active);
  WindowTreeHostPlatform::OnActivationChanged(active);
}

}  // namespace views
