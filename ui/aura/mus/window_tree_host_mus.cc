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

bool IsUsingTestContext() {
  return aura::Env::GetInstance()->context_factory()->DoesCreateTestContexts();
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// WindowTreeHostMus, public:

WindowTreeHostMus::WindowTreeHostMus(
    std::unique_ptr<WindowPortMus> window_port,
    WindowTreeClient* window_tree_client,
    int64_t display_id,
    const std::map<std::string, std::vector<uint8_t>>* properties)
    : WindowTreeHostPlatform(std::move(window_port)),
      display_id_(display_id),
      delegate_(window_tree_client) {
  window()->SetProperty(kWindowTreeHostMusKey, this);
  // TODO(sky): find a cleaner way to set this! Better solution is to likely
  // have constructor take aura::Window.
  WindowPortMus::Get(window())->window_ = window();
  if (properties) {
    // Apply the properties before initializing the window, that way the
    // server seems them at the time the window is created.
    WindowMus* window_mus = WindowMus::Get(window());
    for (auto& pair : *properties)
      window_mus->SetPropertyFromServer(pair.first, &pair.second);
  }
  Id server_id = WindowMus::Get(window())->server_id();
  cc::FrameSinkId frame_sink_id(server_id, 0);
  DCHECK(frame_sink_id.is_valid());
  CreateCompositor(frame_sink_id);
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
  OnAcceleratedWidgetAvailable(accelerated_widget,
                               GetDisplay().device_scale_factor());

  delegate_->OnWindowTreeHostCreated(this);

  SetPlatformWindow(base::MakeUnique<ui::StubWindow>(
      this,
      false));  // Do not advertise accelerated widget; already set manually.

  input_method_ = base::MakeUnique<InputMethodMus>(this, window());
  input_method_->Init(window_tree_client->connector());
  SetSharedInputMethod(input_method_.get());

  compositor()->SetHostHasTransparentBackground(true);

  // Mus windows are assumed hidden.
  compositor()->SetVisible(false);
}

// Pass |properties| to CreateWindowPortForTopLevel() so that |properties|
// are passed to the server *and* pass |properties| to the WindowTreeHostMus
// constructor (above) which applies the properties to the Window. Some of the
// properties may be server specific and not applied to the Window.
WindowTreeHostMus::WindowTreeHostMus(
    WindowTreeClient* window_tree_client,
    const std::map<std::string, std::vector<uint8_t>>* properties)
    : WindowTreeHostMus(
          static_cast<WindowTreeHostMusDelegate*>(window_tree_client)
              ->CreateWindowPortForTopLevel(properties),
          window_tree_client,
          display::Screen::GetScreen()->GetPrimaryDisplay().id(),
          properties) {}

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

display::Display WindowTreeHostMus::GetDisplay() const {
  display::Display display;
  display::Screen::GetScreen()->GetDisplayWithDisplayId(display_id_, &display);
  return display;
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

gfx::ICCProfile WindowTreeHostMus::GetICCProfileForCurrentDisplay() {
  // TODO: This should read the profile from mus. crbug.com/647510
  return gfx::ICCProfile();
}

void WindowTreeHostMus::MoveCursorToScreenLocationInPixels(
    const gfx::Point& location_in_pixels) {
  // TODO: this needs to message the server http://crbug.com/693340. Setting
  // the location is really only appropriate in tests, outside of tests this
  // value is ignored.
  NOTIMPLEMENTED();
  Env::GetInstance()->set_last_mouse_location(location_in_pixels);
}

}  // namespace aura
