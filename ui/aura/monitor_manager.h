// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_MONITOR_MANAGER_H_
#define UI_AURA_MONITOR_MANAGER_H_
#pragma once

#include "base/basictypes.h"
#include "base/observer_list.h"
#include "ui/aura/aura_export.h"

namespace gfx {
class Point;
class Size;
}

namespace aura {
class Monitor;
class RootWindow;
class Window;

// Observers for monitor configuration changes.
// TODO(oshima): multiple monitor support.
class MonitorObserver {
 public:
  virtual void OnMonitorBoundsChanged(const Monitor* monitor) = 0;
};

// MonitorManager creates, deletes and updates Monitor objects when
// monitor configuration changes, and notifies MonitorObservers about
// the change. This is owned by Env and its lifetime is longer than
// any windows.
class AURA_EXPORT MonitorManager {
 public:
  MonitorManager();
  virtual ~MonitorManager();

  // Adds/removes MonitorObservers.
  void AddObserver(MonitorObserver* observer);
  void RemoveObserver(MonitorObserver* observer);

  // Called when native window's monitor size has changed.
  // TODO(oshima): multiple monitor support.
  virtual void OnNativeMonitorResized(const gfx::Size& size) = 0;

  // Create a root window for primary monitor.
  virtual RootWindow* CreateRootWindowForMonitor(const Monitor* monitor) = 0;

  // Returns the monitor object nearest given |window|.
  virtual const Monitor* GetMonitorNearestWindow(
      const Window* window) const = 0;
  virtual Monitor* GetMonitorNearestWindow(const Window* window) = 0;

  // Returns the monitor object nearest given |pint|.
  virtual  const Monitor* GetMonitorNearestPoint(
      const gfx::Point& point) const = 0;

  // Returns the monitor that is consiered "primary".
  virtual const Monitor* GetPrimaryMonitor() const = 0;

  virtual size_t GetNumMonitors() const = 0;

  // A utility function to create a root window for primary monitor.
  RootWindow* CreateRootWindowForPrimaryMonitor();

 protected:
  // Calls observers' OnMonitorBoundsChanged methods.
  void NotifyBoundsChanged(const Monitor* monitor);

 private:
  ObserverList<MonitorObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(MonitorManager);
};

}  // namespace aura

#endif  // UI_AURA_MONITOR_MANAGER_H_
