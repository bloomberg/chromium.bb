// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/monitor_manager.h"

#include <stdio.h>

#include "base/logging.h"
#include "ui/aura/env.h"
#include "ui/aura/monitor_observer.h"
#include "ui/aura/root_window.h"
#include "ui/aura/root_window_host.h"
#include "ui/gfx/monitor.h"
#include "ui/gfx/rect.h"

namespace aura {
namespace {
// Default bounds for a monitor.
static const int kDefaultHostWindowX = 200;
static const int kDefaultHostWindowY = 200;
static const int kDefaultHostWindowWidth = 1280;
static const int kDefaultHostWindowHeight = 1024;
}  // namespace

// static
bool MonitorManager::use_fullscreen_host_window_ = false;

// static
gfx::Monitor MonitorManager::CreateMonitorFromSpec(const std::string& spec) {
  static int synthesized_monitor_id = 1000;
  gfx::Rect bounds(kDefaultHostWindowX, kDefaultHostWindowY,
                   kDefaultHostWindowWidth, kDefaultHostWindowHeight);
  int x = 0, y = 0, width, height;
  float scale = gfx::Monitor::GetDefaultDeviceScaleFactor();
  if (sscanf(spec.c_str(), "%dx%d*%f", &width, &height, &scale) >= 2) {
    bounds.set_size(gfx::Size(width, height));
  } else if (sscanf(spec.c_str(), "%d+%d-%dx%d*%f", &x, &y, &width, &height,
                    &scale) >= 4 ) {
    bounds = gfx::Rect(x, y, width, height);
  } else if (use_fullscreen_host_window_) {
    bounds = gfx::Rect(aura::RootWindowHost::GetNativeScreenSize());
  }
  gfx::Monitor monitor(synthesized_monitor_id++);
  monitor.SetScaleAndBounds(scale, bounds);
  DVLOG(1) << "Monitor bounds=" << bounds.ToString() << ", scale=" << scale;
  return monitor;
}

// static
RootWindow* MonitorManager::CreateRootWindowForPrimaryMonitor() {
  MonitorManager* manager = aura::Env::GetInstance()->monitor_manager();
  RootWindow* root =
      manager->CreateRootWindowForMonitor(manager->GetMonitorAt(0));
  if (use_fullscreen_host_window_)
    root->ConfineCursorToWindow();
  return root;
}

MonitorManager::MonitorManager() {
}

MonitorManager::~MonitorManager() {
}

void MonitorManager::AddObserver(MonitorObserver* observer) {
  observers_.AddObserver(observer);
}

void MonitorManager::RemoveObserver(MonitorObserver* observer) {
  observers_.RemoveObserver(observer);
}

void MonitorManager::NotifyBoundsChanged(const gfx::Monitor& monitor) {
  FOR_EACH_OBSERVER(MonitorObserver, observers_,
                    OnMonitorBoundsChanged(monitor));
}

void MonitorManager::NotifyMonitorAdded(const gfx::Monitor& monitor) {
  FOR_EACH_OBSERVER(MonitorObserver, observers_,
                    OnMonitorAdded(monitor));
}

void MonitorManager::NotifyMonitorRemoved(const gfx::Monitor& monitor) {
  FOR_EACH_OBSERVER(MonitorObserver, observers_,
                    OnMonitorRemoved(monitor));
}

}  // namespace aura
