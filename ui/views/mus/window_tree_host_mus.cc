// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/mus/window_tree_host_mus.h"

#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/events/event.h"
#include "ui/views/mus/input_method_mus.h"
#include "ui/views/mus/native_widget_mus.h"
#include "ui/views/mus/platform_window_mus.h"

namespace views {

////////////////////////////////////////////////////////////////////////////////
// WindowTreeHostMus, public:

WindowTreeHostMus::WindowTreeHostMus(mojo::shell::mojom::Shell* shell,
                                     NativeWidgetMus* native_widget,
                                     mus::Window* window)
    : native_widget_(native_widget),
      show_state_(ui::PLATFORM_WINDOW_STATE_UNKNOWN) {
  SetPlatformWindow(
      make_scoped_ptr(new PlatformWindowMus(this, shell, window)));
  // The location of events is already transformed, and there is no way to
  // correctly determine the reverse transform. So, don't attempt to transform
  // event locations, else the root location is wrong.
  // TODO(sky): we need to transform for device scale though.
  dispatcher()->set_transform_events(false);
  compositor()->SetHostHasTransparentBackground(true);

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

void WindowTreeHostMus::OnCloseRequest() {
  OnHostCloseRequested();
}

}  // namespace views
