// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/mus/window_tree_host_mus.h"

#include "base/memory/ptr_util.h"
#include "ui/aura/env.h"
#include "ui/aura/mus/input_method_mus.h"
#include "ui/aura/mus/window_port_mus.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_observer.h"
#include "ui/events/event.h"
#include "ui/platform_window/stub/stub_window.h"

namespace aura {

namespace {
static uint32_t accelerated_widget_count = 1;

bool IsUsingTestContext() {
  return aura::Env::GetInstance()->context_factory()->DoesCreateTestContexts();
}

}  // namespace

class WindowTreeHostMus::ContentWindowObserver : public WindowObserver {
 public:
  ContentWindowObserver(WindowTreeHostMus* window_tree_host_mus, Window* window)
      : window_tree_host_mus_(window_tree_host_mus), window_(window) {
    window_->AddObserver(this);
  }
  ~ContentWindowObserver() override { window_->RemoveObserver(this); }

  // WindowObserver:
  void OnWindowDestroyed(Window* window) override {
    window_tree_host_mus_->ContentWindowDestroyed();
  }
  void OnWindowBoundsChanged(Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds) override {
    if (old_bounds.size() != new_bounds.size())
      window_tree_host_mus_->ContentWindowResized();
  }
  void OnWindowVisibilityChanging(Window* window, bool visible) override {
    window_tree_host_mus_->ContentWindowVisibilityChanging(visible);
  }

 private:
  WindowTreeHostMus* window_tree_host_mus_;
  Window* window_;

  DISALLOW_COPY_AND_ASSIGN(ContentWindowObserver);
};

////////////////////////////////////////////////////////////////////////////////
// WindowTreeHostMus, public:

WindowTreeHostMus::WindowTreeHostMus(std::unique_ptr<WindowPortMus> window_port,
                                     Window* content_window)
    : WindowTreeHostPlatform(std::move(window_port)),
      content_window_(content_window) {
  gfx::AcceleratedWidget accelerated_widget;
  if (IsUsingTestContext()) {
    accelerated_widget = gfx::kNullAcceleratedWidget;
  } else {
// We need accelerated widget numbers to be different for each
// window and fit in the smallest sizeof(AcceleratedWidget) uint32_t
// has this property.
#if defined(OS_WIN) || defined(OS_ANDROID)
    accelerated_widget =
        reinterpret_cast<gfx::AcceleratedWidget>(accelerated_widget_count++);
#else
    accelerated_widget =
        static_cast<gfx::AcceleratedWidget>(accelerated_widget_count++);
#endif
  }
  // TODO(markdittmer): Use correct device-scale-factor from |window|.
  OnAcceleratedWidgetAvailable(accelerated_widget, 1.f);

  SetPlatformWindow(base::MakeUnique<ui::StubWindow>(
      this,
      false));  // Do not advertise accelerated widget; already set manually.

  if (content_window_) {
    window()->AddChild(content_window_);
    window()->SetBounds(gfx::Rect(content_window_->bounds().size()));
    content_window_observer_ =
        base::MakeUnique<ContentWindowObserver>(this, content_window_);
  } else {
    // Initialize the stub platform window bounds to those of the ui::Window.
    platform_window()->SetBounds(window()->bounds());
  }

  input_method_ = base::MakeUnique<InputMethodMus>(
      this, content_window_ ? content_window_ : window());

  // TODO: resolve
  // compositor()->SetWindow(window);

  compositor()->SetHostHasTransparentBackground(true);
}

WindowTreeHostMus::~WindowTreeHostMus() {
  DestroyCompositor();
  DestroyDispatcher();
}

void WindowTreeHostMus::ContentWindowDestroyed() {
  delete this;
}

void WindowTreeHostMus::ContentWindowResized() {
  window()->SetBounds(gfx::Rect(content_window_->bounds().size()));
}

void WindowTreeHostMus::ContentWindowVisibilityChanging(bool visible) {
  if (visible)
    window()->Show();
  else
    window()->Hide();
}

void WindowTreeHostMus::DispatchEvent(ui::Event* event) {
  DCHECK(!event->IsKeyEvent());
  WindowTreeHostPlatform::DispatchEvent(event);
}

void WindowTreeHostMus::OnClosed() {
  // TODO: figure out if needed.
  /*
  if (native_widget_)
    native_widget_->OnPlatformWindowClosed();
  */
}

void WindowTreeHostMus::OnActivationChanged(bool active) {
  if (active)
    GetInputMethod()->OnFocus();
  else
    GetInputMethod()->OnBlur();
  // TODO: figure out if needed.
  /*
  if (native_widget_)
    native_widget_->OnActivationChanged(active);
  */
  WindowTreeHostPlatform::OnActivationChanged(active);
}

void WindowTreeHostMus::OnCloseRequest() {
  OnHostCloseRequested();
}

gfx::ICCProfile WindowTreeHostMus::GetICCProfileForCurrentDisplay() {
  // TODO: This should read the profile from mus. crbug.com/647510
  return gfx::ICCProfile();
}

}  // namespace aura
