// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/demo/mus_demo.h"

#include "base/memory/ptr_util.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/service_context.h"
#include "services/ui/demo/window_tree_data.h"
#include "services/ui/public/cpp/gpu/gpu.h"
#include "ui/aura/client/default_capture_client.h"
#include "ui/aura/env.h"
#include "ui/aura/mus/property_converter.h"
#include "ui/aura/mus/window_tree_client.h"
#include "ui/aura/mus/window_tree_host_mus.h"
#include "ui/aura/window.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/wm/core/wm_state.h"

namespace ui {
namespace demo {

namespace {

// Size of square in pixels to draw.
const int kSquareSize = 300;

}

MusDemo::MusDemo() {}

MusDemo::~MusDemo() {
  display::Screen::SetScreenInstance(nullptr);
}

void MusDemo::OnStart() {
  screen_ = base::MakeUnique<display::ScreenBase>();
  display::Screen::SetScreenInstance(screen_.get());

  env_ = aura::Env::CreateInstance(aura::Env::Mode::MUS);
  capture_client_ = base::MakeUnique<aura::client::DefaultCaptureClient>();
  property_converter_ = base::MakeUnique<aura::PropertyConverter>();
  wm_state_ = base::MakeUnique<::wm::WMState>();

  window_tree_client_ = base::MakeUnique<aura::WindowTreeClient>(
      context()->connector(), this, this);
  window_tree_client_->ConnectAsWindowManager();

  window_tree_data_ = base::MakeUnique<WindowTreeData>(kSquareSize);

  env_->SetWindowTreeClient(window_tree_client_.get());
}

bool MusDemo::OnConnect(const service_manager::ServiceInfo& remote_info,
                        service_manager::InterfaceRegistry* registry) {
  return true;
}

void MusDemo::OnEmbed(
    std::unique_ptr<aura::WindowTreeHostMus> window_tree_host) {
  // Not called for the WindowManager.
  NOTREACHED();
}

void MusDemo::OnUnembed(aura::Window* root) {
  NOTREACHED();
}

void MusDemo::OnEmbedRootDestroyed(aura::WindowTreeHostMus* window_tree_host) {
  // Not called for the WindowManager.
  NOTREACHED();
}

void MusDemo::OnLostConnection(aura::WindowTreeClient* client) {
  window_tree_client_.reset();
  window_tree_data_.reset();
}

void MusDemo::OnPointerEventObserved(const PointerEvent& event,
                                     aura::Window* target) {}

aura::PropertyConverter* MusDemo::GetPropertyConverter() {
  return property_converter_.get();
}

void MusDemo::SetWindowManagerClient(aura::WindowManagerClient* client) {}

bool MusDemo::OnWmSetBounds(aura::Window* window, gfx::Rect* bounds) {
  return true;
}

bool MusDemo::OnWmSetProperty(aura::Window* window,
                              const std::string& name,
                              std::unique_ptr<std::vector<uint8_t>>* new_data) {
  return true;
}

void MusDemo::OnWmSetCanFocus(aura::Window* window, bool can_focus) {}

aura::Window* MusDemo::OnWmCreateTopLevelWindow(
    mojom::WindowType window_type,
    std::map<std::string, std::vector<uint8_t>>* properties) {
  NOTREACHED();
  return nullptr;
}

void MusDemo::OnWmClientJankinessChanged(
    const std::set<aura::Window*>& client_windows,
    bool janky) {
  // Don't care
}

void MusDemo::OnWmWillCreateDisplay(const display::Display& display) {
  screen_->display_list().AddDisplay(display,
                                     display::DisplayList::Type::PRIMARY);
}

void MusDemo::OnWmNewDisplay(
    std::unique_ptr<aura::WindowTreeHostMus> window_tree_host,
    const display::Display& display) {
  DCHECK(!window_tree_data_->IsInitialized());  // Only support one display.
  window_tree_data_->Init(std::move(window_tree_host));
}

void MusDemo::OnWmDisplayRemoved(aura::WindowTreeHostMus* window_tree_host) {
  window_tree_data_.reset();
}

void MusDemo::OnWmDisplayModified(const display::Display& display) {}

mojom::EventResult MusDemo::OnAccelerator(uint32_t id, const Event& event) {
  return mojom::EventResult::UNHANDLED;
}

void MusDemo::OnWmPerformMoveLoop(aura::Window* window,
                                  mojom::MoveLoopSource source,
                                  const gfx::Point& cursor_location,
                                  const base::Callback<void(bool)>& on_done) {
  // Don't care
}

void MusDemo::OnWmCancelMoveLoop(aura::Window* window) {}

void MusDemo::OnWmSetClientArea(
    aura::Window* window,
    const gfx::Insets& insets,
    const std::vector<gfx::Rect>& additional_client_areas) {}

bool MusDemo::IsWindowActive(aura::Window* window) { return false; }

void MusDemo::OnWmDeactivateWindow(aura::Window* window) {}

}  // namespace demo
}  // namespace aura
