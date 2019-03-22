// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/exo/wm_helper_cast_shell.h"

#include "base/memory/singleton.h"
#include "chromecast/browser/cast_browser_process.h"
#include "chromecast/graphics/cast_screen.h"
#include "chromecast/graphics/cast_window_manager_aura.h"
#include "ui/aura/client/focus_client.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/display/display.h"
#include "ui/display/manager/display_configurator.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/screen.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/events/devices/input_device_manager.h"
#include "ui/wm/public/activation_client.h"

namespace exo {

WMHelperCastShell::WMHelperCastShell(
    aura::Env* env,
    chromecast::CastWindowManagerAura* cast_window_manager_aura,
    CastScreen* cast_screen)
    : cast_window_manager_aura_(cast_window_manager_aura),
      env_(env),
      cast_screen_(cast_screen) {
  cast_screen_->AddObserver(&display_observer_);
}

WMHelperCastShell::~WMHelperCastShell() {
  cast_screen_->RemoveObserver(&display_observer_);
}

aura::Env* WMHelperCastShell::env() {
  return env_;
}

void WMHelperCastShell::AddActivationObserver(
    wm::ActivationChangeObserver* observer) {
  NOTIMPLEMENTED();
}

void WMHelperCastShell::RemoveActivationObserver(
    wm::ActivationChangeObserver* observer) {
  NOTIMPLEMENTED();
}

void WMHelperCastShell::AddFocusObserver(
    aura::client::FocusChangeObserver* observer) {
  NOTIMPLEMENTED();
}

void WMHelperCastShell::RemoveFocusObserver(
    aura::client::FocusChangeObserver* observer) {
  NOTIMPLEMENTED();
}

void WMHelperCastShell::AddDragDropObserver(DragDropObserver* observer) {
  drag_drop_observers_.AddObserver(observer);
}

void WMHelperCastShell::RemoveDragDropObserver(DragDropObserver* observer) {
  drag_drop_observers_.RemoveObserver(observer);
}

void WMHelperCastShell::SetDragDropDelegate(aura::Window* window) {
  aura::client::SetDragDropDelegate(window, this);
}

void WMHelperCastShell::ResetDragDropDelegate(aura::Window* window) {
  aura::client::SetDragDropDelegate(window, nullptr);
}

void WMHelperCastShell::AddVSyncObserver(
    ui::CompositorVSyncManager::Observer* observer) {
  NOTIMPLEMENTED();
}

void WMHelperCastShell::RemoveVSyncObserver(
    ui::CompositorVSyncManager::Observer* observer) {
  NOTIMPLEMENTED();
}

void WMHelperCastShell::OnDragEntered(const ui::DropTargetEvent& event) {}

int WMHelperCastShell::OnDragUpdated(const ui::DropTargetEvent& event) {}

void WMHelperCastShell::OnDragExited() {}

int WMHelperCastShell::OnPerformDrop(const ui::DropTargetEvent& event) {
  NOTIMPLEMENTED();
  return ui::DragDropTypes::DRAG_MOVE;
}

const display::ManagedDisplayInfo& WMHelperCastShell::GetDisplayInfo(
    int64_t display_id) const {
  return display_observer_.GetDisplayInfo(display_id);
}

const std::vector<uint8_t>& WMHelperCastShell::GetDisplayIdentificationData(
    int64_t display_id) const {
  NOTIMPLEMENTED();
  static std::vector<uint8_t> no_data;
  return no_data;
}

aura::Window* WMHelperCastShell::GetPrimaryDisplayContainer(int container_id) {
  return cast_window_manager_aura_->GetRootWindow();
}

aura::Window* WMHelperCastShell::GetActiveWindow() const {
  NOTIMPLEMENTED();
  return nullptr;
}

aura::Window* WMHelperCastShell::GetFocusedWindow() const {
  NOTIMPLEMENTED();
  return nullptr;
}

aura::Window* WMHelperCastShell::GetRootWindowForNewWindows() const {
  return cast_window_manager_aura_->GetRootWindow();
}

aura::client::CursorClient* WMHelperCastShell::GetCursorClient() {
  NOTIMPLEMENTED();
  return nullptr;
}

void WMHelperCastShell::AddPreTargetHandler(ui::EventHandler* handler) {
  cast_window_manager_aura_->GetRootWindow()->AddPreTargetHandler(handler);
}

void WMHelperCastShell::PrependPreTargetHandler(ui::EventHandler* handler) {
  NOTIMPLEMENTED();
}

void WMHelperCastShell::RemovePreTargetHandler(ui::EventHandler* handler) {
  cast_window_manager_aura_->GetRootWindow()->RemovePreTargetHandler(handler);
}

void WMHelperCastShell::AddPostTargetHandler(ui::EventHandler* handler) {
  cast_window_manager_aura_->GetRootWindow()->AddPostTargetHandler(handler);
}

void WMHelperCastShell::RemovePostTargetHandler(ui::EventHandler* handler) {
  cast_window_manager_aura_->GetRootWindow()->RemovePostTargetHandler(handler);
}

bool WMHelperCastShell::IsTabletModeWindowManagerEnabled() const {
  NOTIMPLEMENTED();
  return false;
}

double WMHelperCastShell::GetDefaultDeviceScaleFactor() const {
  NOTIMPLEMENTED();
  return 1.0;
}

WMHelperCastShell::CastDisplayObserver::CastDisplayObserver() {}

WMHelperCastShell::CastDisplayObserver::~CastDisplayObserver() {}

void WMHelperCastShell::CastDisplayObserver::OnWillProcessDisplayChanges() {}

void WMHelperCastShell::CastDisplayObserver::OnDidProcessDisplayChanges() {}

void WMHelperCastShell::CastDisplayObserver::OnDisplayAdded(
    const display::Display& new_display) {
  display::ManagedDisplayInfo md(new_display.id(), "CastDisplayInfo", true);
  md.SetBounds(new_display.bounds());
  display_info_.emplace(new_display.id(), md);
}

void WMHelperCastShell::CastDisplayObserver::OnDisplayRemoved(
    const display::Display& old_display) {
  display_info_.erase(old_display.id());
}

void WMHelperCastShell::CastDisplayObserver::OnDisplayMetricsChanged(
    const display::Display& display,
    uint32_t changed_metrics) {
  if (display_info_.find(display.id()) == display_info_.end()) {
    display::ManagedDisplayInfo md(display.id(), "CastDisplayInfo", true);
    md.SetBounds(display.bounds());
    display_info_.emplace(display.id(), md);
  }

  // Currently only updates bounds
  if ((DISPLAY_METRIC_BOUNDS & changed_metrics) == DISPLAY_METRIC_BOUNDS)
    display_info_[display.id()].SetBounds(display.bounds());
}

const display::ManagedDisplayInfo&
WMHelperCastShell::CastDisplayObserver::GetDisplayInfo(
    int64_t display_id) const {
  auto iter = display_info_.find(display_id);
  CHECK(iter != display_info_.end()) << display_id;
  return iter->second;
}

}  // namespace exo
