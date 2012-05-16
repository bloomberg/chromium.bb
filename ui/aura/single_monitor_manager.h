// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_SINGLE_MONITOR_MANAGER_H_
#define UI_AURA_SINGLE_MONITOR_MANAGER_H_
#pragma once

#include "base/compiler_specific.h"
#include "ui/aura/aura_export.h"
#include "ui/aura/monitor_manager.h"
#include "ui/aura/window_observer.h"
#include "ui/gfx/monitor.h"

namespace gfx {
class Rect;
}

namespace aura {

// A monitor manager assuming there is one monitor.
class AURA_EXPORT SingleMonitorManager : public MonitorManager,
                                         public WindowObserver {
 public:
  SingleMonitorManager();
  virtual ~SingleMonitorManager();

  // MonitorManager overrides:
  virtual void OnNativeMonitorsChanged(
      const std::vector<gfx::Monitor>& monitors) OVERRIDE;
  virtual RootWindow* CreateRootWindowForMonitor(
      const gfx::Monitor& monitor) OVERRIDE;
  virtual const gfx::Monitor& GetMonitorAt(size_t index) OVERRIDE;

  virtual size_t GetNumMonitors() const OVERRIDE;

  virtual const gfx::Monitor& GetMonitorNearestWindow(
      const Window* window) const OVERRIDE;
  virtual const gfx::Monitor& GetMonitorNearestPoint(
      const gfx::Point& point) const OVERRIDE;

  // WindowObserver overrides:
  virtual void OnWindowBoundsChanged(Window* window,
                                     const gfx::Rect& old_bounds,
                                     const gfx::Rect& new_bounds) OVERRIDE;
  virtual void OnWindowDestroying(Window* window) OVERRIDE;

 private:
  void Init();
  void Update(const gfx::Size size);

  RootWindow* root_window_;
  gfx::Monitor monitor_;

  DISALLOW_COPY_AND_ASSIGN(SingleMonitorManager);
};

}  // namespace aura

#endif  // UI_AURA_SINGLE_MONITOR_MANAGER_H_
