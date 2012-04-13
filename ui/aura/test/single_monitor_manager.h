// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_TEST_SINGLE_MONITOR_MANAGER_H_
#define UI_AURA_TEST_SINGLE_MONITOR_MANAGER_H_
#pragma once

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "ui/aura/monitor_manager.h"
#include "ui/aura/window_observer.h"

namespace gfx {
class Rect;
}

namespace aura {
namespace test {

// A monitor manager assuming there is one monitor.
class SingleMonitorManager : public MonitorManager,
                             public WindowObserver {
 public:
  SingleMonitorManager();
  virtual ~SingleMonitorManager();

  // MonitorManager overrides:
  virtual void OnNativeMonitorsChanged(
      const std::vector<const Monitor*>& monitors) OVERRIDE;
  virtual RootWindow* CreateRootWindowForMonitor(
      Monitor* monitor) OVERRIDE;
  virtual const Monitor* GetMonitorNearestWindow(
      const Window* window) const OVERRIDE;
  virtual const Monitor* GetMonitorNearestPoint(
      const gfx::Point& point) const OVERRIDE;
  virtual Monitor* GetMonitorAt(size_t index) OVERRIDE;
  virtual size_t GetNumMonitors() const OVERRIDE;
  virtual Monitor* GetMonitorNearestWindow(const Window* window) OVERRIDE;

  // WindowObserver overrides:
  virtual void OnWindowBoundsChanged(Window* window,
                                     const gfx::Rect& bounds) OVERRIDE;
  virtual void OnWindowDestroying(Window* window) OVERRIDE;

 private:
  void Init();
  void Update(const gfx::Size size);

  RootWindow* root_window_;
  scoped_ptr<Monitor> monitor_;

  DISALLOW_COPY_AND_ASSIGN(SingleMonitorManager);
};

}  // namespace test
}  // namespace aura

#endif  //  UI_AURA_TEST_SINGLE_MONITOR_MANAGER_H_
