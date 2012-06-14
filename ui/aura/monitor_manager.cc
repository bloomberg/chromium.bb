// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/monitor_manager.h"

#include <stdio.h>

#include "base/logging.h"
#include "ui/aura/display_observer.h"
#include "ui/aura/env.h"
#include "ui/aura/root_window.h"
#include "ui/aura/root_window_host.h"
#include "ui/gfx/display.h"
#include "ui/gfx/rect.h"

namespace aura {
namespace {
// Default bounds for a monitor.
const int kDefaultHostWindowX = 200;
const int kDefaultHostWindowY = 200;
const int kDefaultHostWindowWidth = 1280;
const int kDefaultHostWindowHeight = 1024;
}  // namespace

// static
bool MonitorManager::use_fullscreen_host_window_ = false;

// static
gfx::Display MonitorManager::CreateMonitorFromSpec(const std::string& spec) {
  static int synthesized_monitor_id = 1000;
  gfx::Rect bounds(kDefaultHostWindowX, kDefaultHostWindowY,
                   kDefaultHostWindowWidth, kDefaultHostWindowHeight);
  int x = 0, y = 0, width, height;
  float scale = 1.0f;
  if (sscanf(spec.c_str(), "%dx%d*%f", &width, &height, &scale) >= 2) {
    bounds.set_size(gfx::Size(width, height));
  } else if (sscanf(spec.c_str(), "%d+%d-%dx%d*%f", &x, &y, &width, &height,
                    &scale) >= 4 ) {
    bounds = gfx::Rect(x, y, width, height);
  } else if (use_fullscreen_host_window_) {
    bounds = gfx::Rect(aura::RootWindowHost::GetNativeScreenSize());
  }
  gfx::Display display(synthesized_monitor_id++);
  display.SetScaleAndBounds(scale, bounds);
  DVLOG(1) << "Display bounds=" << bounds.ToString() << ", scale=" << scale;
  return display;
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

void MonitorManager::AddObserver(DisplayObserver* observer) {
  observers_.AddObserver(observer);
}

void MonitorManager::RemoveObserver(DisplayObserver* observer) {
  observers_.RemoveObserver(observer);
}

void MonitorManager::NotifyBoundsChanged(const gfx::Display& display) {
  FOR_EACH_OBSERVER(DisplayObserver, observers_,
                    OnDisplayBoundsChanged(display));
}

void MonitorManager::NotifyDisplayAdded(const gfx::Display& display) {
  FOR_EACH_OBSERVER(DisplayObserver, observers_, OnDisplayAdded(display));
}

void MonitorManager::NotifyDisplayRemoved(const gfx::Display& display) {
  FOR_EACH_OBSERVER(DisplayObserver, observers_, OnDisplayRemoved(display));
}

}  // namespace aura
