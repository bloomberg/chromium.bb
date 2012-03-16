// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/single_monitor_manager.h"

#include <string>

#include "base/command_line.h"
#include "ui/aura/aura_switches.h"
#include "ui/aura/monitor.h"
#include "ui/aura/root_window.h"
#include "ui/aura/root_window_host.h"
#include "ui/gfx/rect.h"

namespace aura {
namespace internal {

using std::string;

namespace {
// Default bounds for the primary monitor.
static const int kDefaultHostWindowX = 200;
static const int kDefaultHostWindowY = 200;
static const int kDefaultHostWindowWidth = 1280;
static const int kDefaultHostWindowHeight = 1024;
}

SingleMonitorManager::SingleMonitorManager()
    : root_window_(NULL),
      monitor_(new Monitor()) {
  Init();
}

SingleMonitorManager::~SingleMonitorManager() {
  if (root_window_)
    root_window_->RemoveObserver(this);
}

void SingleMonitorManager::OnNativeMonitorResized(const gfx::Size& size) {
  if (RootWindow::use_fullscreen_host_window()) {
    monitor_->set_size(size);
    NotifyBoundsChanged(monitor_.get());
  }
}

RootWindow* SingleMonitorManager::CreateRootWindowForMonitor(
    const Monitor* monitor) {
  DCHECK(!root_window_);
  DCHECK_EQ(monitor_.get(), monitor);
  root_window_ = new RootWindow();
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

const Monitor* SingleMonitorManager::GetPrimaryMonitor() const {
  return monitor_.get();
}

size_t SingleMonitorManager::GetNumMonitors() const {
  return 1;
}

Monitor* SingleMonitorManager::GetMonitorNearestWindow(const Window* window) {
  return monitor_.get();
}

void SingleMonitorManager::OnWindowBoundsChanged(
    Window* window, const gfx::Rect& bounds) {
  if (!RootWindow::use_fullscreen_host_window()) {
    Update(bounds.size());
    NotifyBoundsChanged(monitor_.get());
  }
}

void SingleMonitorManager::OnWindowDestroying(Window* window) {
  if (root_window_ == window)
    root_window_ = NULL;
}

void SingleMonitorManager::Init() {
  gfx::Rect bounds(kDefaultHostWindowX, kDefaultHostWindowY,
                   kDefaultHostWindowWidth, kDefaultHostWindowHeight);
  const string size_str = CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
      switches::kAuraHostWindowSize);
  int x = 0, y = 0, width, height;
  if (sscanf(size_str.c_str(), "%dx%d", &width, &height) == 2) {
    bounds.set_size(gfx::Size(width, height));
  } else if (sscanf(size_str.c_str(), "%d+%d-%dx%d", &x, &y, &width, &height)
             == 4) {
    bounds = gfx::Rect(x, y, width, height);
  } else if (RootWindow::use_fullscreen_host_window()) {
    bounds = gfx::Rect(RootWindowHost::GetNativeScreenSize());
  }
  monitor_->set_bounds(bounds);
}

void SingleMonitorManager::Update(const gfx::Size size) {
  monitor_->set_size(size);
}

}  // namespace inernal
}  // namespace aura
