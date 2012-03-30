// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/test/single_monitor_manager.h"

#include <string>

#include "base/command_line.h"
#include "ui/aura/aura_switches.h"
#include "ui/aura/monitor.h"
#include "ui/aura/root_window.h"
#include "ui/aura/root_window_host.h"
#include "ui/gfx/rect.h"

namespace aura {
namespace test {

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
    const std::vector<const Monitor*>& monitors) {
  DCHECK(monitors.size() > 0);
  if (use_fullscreen_host_window()) {
    monitor_->set_size(monitors[0]->bounds().size());
    NotifyBoundsChanged(monitor_.get());
  }
}

RootWindow* SingleMonitorManager::CreateRootWindowForMonitor(
    Monitor* monitor) {
  DCHECK(!root_window_);
  DCHECK_EQ(monitor_.get(), monitor);
  root_window_ = new RootWindow(monitor->bounds());
  root_window_->AddObserver(this);
  return root_window_;
}

const Monitor* SingleMonitorManager::GetMonitorNearestWindow(
    const Window* window) const {
  return monitor_.get();
}

const Monitor* SingleMonitorManager::GetMonitorNearestPoint(
    const gfx::Point& point) const {
  return monitor_.get();
}

Monitor* SingleMonitorManager::GetMonitorAt(size_t index) {
  return !index ? monitor_.get() : NULL;
}

size_t SingleMonitorManager::GetNumMonitors() const {
  return 1;
}

Monitor* SingleMonitorManager::GetMonitorNearestWindow(const Window* window) {
  return monitor_.get();
}

void SingleMonitorManager::OnWindowBoundsChanged(
    Window* window, const gfx::Rect& bounds) {
  if (!use_fullscreen_host_window()) {
    Update(bounds.size());
    NotifyBoundsChanged(monitor_.get());
  }
}

void SingleMonitorManager::OnWindowDestroying(Window* window) {
  if (root_window_ == window)
    root_window_ = NULL;
}

void SingleMonitorManager::Init() {
  const string size_str = CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
      switches::kAuraHostWindowSize);
  monitor_.reset(CreateMonitorFromSpec(size_str));
}

void SingleMonitorManager::Update(const gfx::Size size) {
  monitor_->set_size(size);
}

}  // namespace test
}  // namespace aura
