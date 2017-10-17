// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/mus/window_tree_host_mus.h"

#include "base/memory/ptr_util.h"
#include "ui/aura/env.h"
#include "ui/aura/mus/input_method_mus.h"
#include "ui/aura/mus/window_port_mus.h"
#include "ui/aura/mus/window_tree_client.h"
#include "ui/aura/mus/window_tree_host_mus_delegate.h"
#include "ui/aura/mus/window_tree_host_mus_init_params.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/class_property.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/platform_window/stub/stub_window.h"

DECLARE_UI_CLASS_PROPERTY_TYPE(aura::WindowTreeHostMus*);

namespace aura {

namespace {

DEFINE_UI_CLASS_PROPERTY_KEY(
    WindowTreeHostMus*, kWindowTreeHostMusKey, nullptr);

static uint32_t accelerated_widget_count = 1;

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// WindowTreeHostMus, public:

WindowTreeHostMus::WindowTreeHostMus(WindowTreeHostMusInitParams init_params)
    : WindowTreeHostPlatform(std::move(init_params.window_port)),
      display_id_(init_params.display_id),
      delegate_(init_params.window_tree_client) {
  gfx::Rect bounds_in_pixels;
  display_init_params_ = std::move(init_params.display_init_params);
  if (display_init_params_) {
    bounds_in_pixels = display_init_params_->viewport_metrics.bounds_in_pixels;
    if (display_init_params_->display)
      DCHECK_EQ(display_id_, display_init_params_->display->id());
  }
  window()->SetProperty(kWindowTreeHostMusKey, this);
  // TODO(sky): find a cleaner way to set this! Better solution is to likely
  // have constructor take aura::Window.
  WindowPortMus* window_mus = WindowPortMus::Get(window());
  window_mus->window_ = window();
  // Apply the properties before initializing the window, that way the server
  // seems them at the time the window is created.
  for (auto& pair : init_params.properties)
    window_mus->SetPropertyFromServer(pair.first, &pair.second);
  CreateCompositor(viz::FrameSinkId());
  gfx::AcceleratedWidget accelerated_widget;
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
  OnAcceleratedWidgetAvailable(accelerated_widget,
                               GetDisplay().device_scale_factor());

  delegate_->OnWindowTreeHostCreated(this);

  // Do not advertise accelerated widget; already set manually.
  const bool use_default_accelerated_widget = false;
  SetPlatformWindow(std::make_unique<ui::StubWindow>(
      this, use_default_accelerated_widget, bounds_in_pixels));

  if (!init_params.use_classic_ime) {
    input_method_ = std::make_unique<InputMethodMus>(this, window());
    input_method_->Init(init_params.window_tree_client->connector());
    SetSharedInputMethod(input_method_.get());
  }

  compositor()->SetBackgroundColor(SK_ColorTRANSPARENT);

  // Mus windows are assumed hidden.
  compositor()->SetVisible(false);

  if (window_mus->window_mus_type() ==
      WindowMusType::DISPLAY_MANUALLY_CREATED) {
    compositor()->SetLocalSurfaceId(
        window_mus->GetOrAllocateLocalSurfaceId(bounds_in_pixels.size()));
  }
}

WindowTreeHostMus::~WindowTreeHostMus() {
  DestroyCompositor();
  DestroyDispatcher();
}

// static
WindowTreeHostMus* WindowTreeHostMus::ForWindow(aura::Window* window) {
  if (!window)
    return nullptr;

  aura::Window* root = window->GetRootWindow();
  if (!root) {
    // During initial setup this function is called for the root, before the
    // WindowTreeHost has been registered so that GetRootWindow() returns null.
    // Fallback to checking window, in case it really is the root.
    return window->GetProperty(kWindowTreeHostMusKey);
  }

  return root->GetProperty(kWindowTreeHostMusKey);
}

void WindowTreeHostMus::SetBoundsFromServer(const gfx::Rect& bounds_in_pixels) {
  base::AutoReset<bool> resetter(&in_set_bounds_from_server_, true);
  SetBoundsInPixels(bounds_in_pixels);
}

void WindowTreeHostMus::SetClientArea(
    const gfx::Insets& insets,
    const std::vector<gfx::Rect>& additional_client_area) {
  delegate_->OnWindowTreeHostClientAreaWillChange(this, insets,
                                                  additional_client_area);
}

void WindowTreeHostMus::SetHitTestMask(const base::Optional<gfx::Rect>& rect) {
  delegate_->OnWindowTreeHostHitTestMaskWillChange(this, rect);
}

void WindowTreeHostMus::SetOpacity(float value) {
  delegate_->OnWindowTreeHostSetOpacity(this, value);
}

void WindowTreeHostMus::DeactivateWindow() {
  delegate_->OnWindowTreeHostDeactivateWindow(this);
}

void WindowTreeHostMus::StackAbove(Window* window) {
  delegate_->OnWindowTreeHostStackAbove(this, window);
}

void WindowTreeHostMus::StackAtTop() {
  delegate_->OnWindowTreeHostStackAtTop(this);
}

void WindowTreeHostMus::PerformWindowMove(
    ui::mojom::MoveLoopSource mus_source,
    const gfx::Point& cursor_location,
    const base::Callback<void(bool)>& callback) {
  delegate_->OnWindowTreeHostPerformWindowMove(
      this, mus_source, cursor_location, callback);
}

void WindowTreeHostMus::CancelWindowMove() {
  delegate_->OnWindowTreeHostCancelWindowMove(this);
}

void WindowTreeHostMus::ConfineCursorToBounds(
    const gfx::Rect& bounds_in_pixels) {
  delegate_->OnWindowTreeHostConfineCursorToBounds(bounds_in_pixels,
                                                   display_id_);
}

display::Display WindowTreeHostMus::GetDisplay() const {
  display::Display display;
  display::Screen::GetScreen()->GetDisplayWithDisplayId(display_id_, &display);
  return display;
}

std::unique_ptr<DisplayInitParams>
WindowTreeHostMus::ReleaseDisplayInitParams() {
  return std::move(display_init_params_);
}

void WindowTreeHostMus::HideImpl() {
  WindowTreeHostPlatform::HideImpl();
  window()->Hide();
}

void WindowTreeHostMus::SetBoundsInPixels(const gfx::Rect& bounds) {
  if (!in_set_bounds_from_server_)
    delegate_->OnWindowTreeHostBoundsWillChange(this, bounds);
  WindowTreeHostPlatform::SetBoundsInPixels(bounds);
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

void WindowTreeHostMus::MoveCursorToScreenLocationInPixels(
    const gfx::Point& location_in_pixels) {
  // |location_in_pixels| is relative to the display.
  delegate_->OnWindowTreeHostMoveCursorToDisplayLocation(location_in_pixels,
                                                         display_id_);
}

gfx::Transform WindowTreeHostMus::GetRootTransformForLocalEventCoordinates()
    const {
  if (WindowMus::Get(window())->window_mus_type() !=
      WindowMusType::DISPLAY_MANUALLY_CREATED) {
    return WindowTreeHost::GetRootTransformForLocalEventCoordinates();
  }
  // Local events already have the transform set on the window applied, so
  // don't apply it again.
  gfx::Transform transform;
  const float scale = window()->layer()->device_scale_factor();
  transform.Scale(scale, scale);
  return transform;
}

}  // namespace aura
