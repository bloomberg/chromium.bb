// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/mus/window_tree_host_mus.h"

#include "base/memory/ptr_util.h"
#include "ui/aura/env.h"
#include "ui/aura/mus/input_method_mus.h"
#include "ui/aura/mus/window_port_mus.h"
#include "ui/aura/mus/window_tree_host_mus_delegate.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_observer.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
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

 private:
  WindowTreeHostMus* window_tree_host_mus_;
  Window* window_;

  DISALLOW_COPY_AND_ASSIGN(ContentWindowObserver);
};

////////////////////////////////////////////////////////////////////////////////
// WindowTreeHostMus, public:

WindowTreeHostMus::WindowTreeHostMus(std::unique_ptr<WindowPortMus> window_port,
                                     WindowTreeHostMusDelegate* delegate,
                                     RootWindowType root_window_type,
                                     int64_t display_id,
                                     Window* content_window)
    : WindowTreeHostPlatform(std::move(window_port)),
      display_id_(display_id),
      root_window_type_(root_window_type),
      delegate_(delegate),
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

  // TODO: hook up to compositor correctly.
  // compositor()->SetWindow(window);

  compositor()->SetHostHasTransparentBackground(true);

  // Mus windows are assumed hidden.
  compositor()->SetVisible(false);
}

WindowTreeHostMus::~WindowTreeHostMus() {
  DestroyCompositor();
  DestroyDispatcher();
}

void WindowTreeHostMus::SetBoundsFromServer(const gfx::Rect& bounds) {
  base::AutoReset<bool> resetter(&in_set_bounds_from_server_, true);
  SetBounds(bounds);
}

display::Display WindowTreeHostMus::GetDisplay() const {
  for (const display::Display& display :
       display::Screen::GetScreen()->GetAllDisplays()) {
    if (display.id() == display_id_)
      return display;
  }
  return display::Display();
}

Window* WindowTreeHostMus::GetWindowWithServerWindow() {
  return content_window_ ? content_window_ : window();
}

void WindowTreeHostMus::ContentWindowDestroyed() {
  delete this;
}

void WindowTreeHostMus::ShowImpl() {
  WindowTreeHostPlatform::ShowImpl();
  window()->Show();
  if (content_window_)
    content_window_->Show();
}

void WindowTreeHostMus::HideImpl() {
  WindowTreeHostPlatform::HideImpl();
  window()->Hide();
  if (content_window_)
    content_window_->Hide();
}

void WindowTreeHostMus::SetBounds(const gfx::Rect& bounds) {
  gfx::Rect adjusted_bounds(bounds);
  if (!in_set_bounds_from_server_) {
    delegate_->SetRootWindowBounds(GetWindowWithServerWindow(),
                                   &adjusted_bounds);
  }
  WindowTreeHostPlatform::SetBounds(adjusted_bounds);
  if (content_window_)
    content_window_->SetBounds(adjusted_bounds);
}

gfx::Rect WindowTreeHostMus::GetBounds() const {
  // TODO(sky): this is wrong, root windows need to be told their location
  // relative to the display.
  const display::Display display(GetDisplay());
  const gfx::Vector2d origin(origin_offset_ +
                             display.bounds().OffsetFromOrigin());
  return gfx::Rect(gfx::Point(origin.x(), origin.y()),
                   WindowTreeHostPlatform::GetBounds().size());
}

gfx::Point WindowTreeHostMus::GetLocationOnNativeScreen() const {
  // TODO(sky): this is wrong, root windows need to be told their location
  // relative to the display.
  return gfx::Point(origin_offset_.x(), origin_offset_.y());
}

void WindowTreeHostMus::DispatchEvent(ui::Event* event) {
  DCHECK(!event->IsKeyEvent());
  WindowTreeHostPlatform::DispatchEvent(event);
}

void WindowTreeHostMus::OnClosed() {
}

void WindowTreeHostMus::OnActivationChanged(bool active) {
  if (active)
    GetInputMethod()->OnFocus();
  else
    GetInputMethod()->OnBlur();
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
