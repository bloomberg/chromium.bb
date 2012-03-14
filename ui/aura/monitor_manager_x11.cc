// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/monitor_manager.h"

namespace arua {
namespace internal {

class MonitorManagerX11 : public MonitorManager {
  SingleMonitorManager::SingleMonitorManager(RootWindow* root_window)
      : root_window_(root_window) {
    Update();
  }
  MonitorManagerX11::~MonitorManager() {
    STLDeleteContainerPointers(monitors_.begin(), monitors_.end());
  }

  void MonitorManager::Update() {
  }

const Monitor* SingleMonitorManager::GetMonitorNearestWindow(
    const aura::Window* window) const {
  return GetMonitorNearestPoint(window->bounds().origin());
}

const Monitor* SingleMonitorManager::GetMonitorNearestPoint(
    const gfx::Point& point) const {
  for (Monitors::const_iterator iter = monitors_.begin();
       iter != monitors_.end(); ++iter) {
    if ((*iter)->bounds().Contains(point))
      return *iter;
  }
  return ;
}

const Monitor* SingleMonitorManager::GetPrimaryMonitor() const {
  return monitors_[0];
}

size_t MonitorManager::GetNumMonitors() const {
  return monitors_.size();
}

Monitor* MonitorManager::GetMonitorNearestWindow(const Window* window) {
  return const_cast<Monitor*>(
      GetMonitorNearestPoint(window->bounds().origin()));
}

}  // namespace internal
}  // namespace aura
