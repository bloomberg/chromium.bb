// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/single_monitor_manager.h"

#include <string>

#include "base/command_line.h"
#include "ui/aura/aura_switches.h"
#include "ui/aura/root_window.h"
#include "ui/aura/root_window_host.h"
#include "ui/gfx/monitor.h"
#include "ui/gfx/rect.h"

namespace aura {

using std::string;

namespace {
// Default bounds for the primary monitor.
static const int kDefaultHostWindowX = 200;
static const int kDefaultHostWindowY = 200;
static const int kDefaultHostWindowWidth = 1280;
static const int kDefaultHostWindowHeight = 1024;
}

SingleMonitorManager::SingleMonitorManager()
    : root_window_(NULL) {
  Init();
}

SingleMonitorManager::~SingleMonitorManager() {
  // All monitors must have been deleted when monitor manager is deleted.
  CHECK(!root_window_);
}

void SingleMonitorManager::OnNativeMonitorsChanged(
    const std::vector<gfx::Monitor>& monitors) {
  DCHECK(monitors.size() > 0);
  if (use_fullscreen_host_window()) {
    monitor_.SetSize(monitors[0].bounds().size());
    NotifyBoundsChanged(monitor_);
  }
}

RootWindow* SingleMonitorManager::CreateRootWindowForMonitor(
    const gfx::Monitor& monitor) {
  DCHECK(!root_window_);
  DCHECK_EQ(monitor_.id(), monitor.id());
  root_window_ = new RootWindow(monitor.bounds());
  root_window_->AddObserver(this);
  root_window_->Init();
  return root_window_;
}

const gfx::Monitor& SingleMonitorManager::GetMonitorAt(size_t index) {
  return monitor_;
}

size_t SingleMonitorManager::GetNumMonitors() const {
  return 1;
}

const gfx::Monitor& SingleMonitorManager::GetMonitorNearestWindow(
    const Window* window) const {
  return monitor_;
}

const gfx::Monitor& SingleMonitorManager::GetMonitorNearestPoint(
    const gfx::Point& point) const {
  return monitor_;
}

void SingleMonitorManager::OnWindowBoundsChanged(
    Window* window, const gfx::Rect& old_bounds, const gfx::Rect& new_bounds) {
  if (!use_fullscreen_host_window()) {
    Update(new_bounds.size());
    NotifyBoundsChanged(monitor_);
  }
}

void SingleMonitorManager::OnWindowDestroying(Window* window) {
  if (root_window_ == window)
    root_window_ = NULL;
}

void SingleMonitorManager::Init() {
  const string size_str = CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
      switches::kAuraHostWindowSize);
  monitor_ = CreateMonitorFromSpec(size_str);
}

void SingleMonitorManager::Update(const gfx::Size size) {
  monitor_.SetSize(size);
}

}  // namespace aura
