// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_MONITOR_MANAGER_H_
#define UI_AURA_MONITOR_MANAGER_H_
#pragma once

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/observer_list.h"
#include "ui/aura/aura_export.h"

namespace gfx {
class Display;
class Point;
class Size;
}

namespace aura {
class DisplayObserver;
class RootWindow;
class Window;

// MonitorManager creates, deletes and updates Monitor objects when
// monitor configuration changes, and notifies DisplayObservers about
// the change. This is owned by Env and its lifetime is longer than
// any windows.
class AURA_EXPORT MonitorManager {
 public:
  static void set_use_fullscreen_host_window(bool use_fullscreen) {
    use_fullscreen_host_window_ = use_fullscreen;
  }
  static bool use_fullscreen_host_window() {
    return use_fullscreen_host_window_;
  }

  // Creates a monitor from string spec. 100+200-1440x800 creates monitor
  // whose size is 1440x800 at the location (100, 200) in screen's coordinates.
  // The location can be omitted and be just "1440x800", which creates
  // monitor at the origin of the screen. An empty string creates
  // the monitor with default size.
  //  The device scale factor can be specified by "*", like "1280x780*2",
  // or |gfx::Display::GetDefaultDeviceScaleFactor()| will be used if omitted.
  static gfx::Display CreateMonitorFromSpec(const std::string& spec);

  // A utility function to create a root window for primary monitor.
  static RootWindow* CreateRootWindowForPrimaryMonitor();

  MonitorManager();
  virtual ~MonitorManager();

  // Adds/removes DisplayObservers.
  void AddObserver(DisplayObserver* observer);
  void RemoveObserver(DisplayObserver* observer);

  // Called when monitor configuration has changed. The new monitor
  // configurations is passed as a vector of Monitor object, which
  // contains each monitor's new infomration.
  virtual void OnNativeMonitorsChanged(
      const std::vector<gfx::Display>& display) = 0;

  // Create a root window for given |monitor|.
  virtual RootWindow* CreateRootWindowForMonitor(
      const gfx::Display& display) = 0;

  // Returns the display at |index|. The display at 0 is considered "primary".
  virtual const gfx::Display& GetMonitorAt(size_t index) = 0;

  virtual size_t GetNumMonitors() const = 0;

  // Returns the display object nearest given |window|.
  virtual const gfx::Display& GetMonitorNearestWindow(
      const Window* window) const = 0;

  // Returns the monitor object nearest given |pint|.
  virtual const gfx::Display& GetMonitorNearestPoint(
      const gfx::Point& point) const = 0;

 protected:
  // Calls observers' OnDisplayBoundsChanged methods.
  void NotifyBoundsChanged(const gfx::Display& display);
  void NotifyDisplayAdded(const gfx::Display& display);
  void NotifyDisplayRemoved(const gfx::Display& display);

 private:
  // If set before the RootWindow is created, the host window will cover the
  // entire monitor.  Note that this can still be overridden via the
  // switches::kAuraHostWindowSize flag.
  static bool use_fullscreen_host_window_;

  ObserverList<DisplayObserver> observers_;
  DISALLOW_COPY_AND_ASSIGN(MonitorManager);
};

}  // namespace aura

#endif  // UI_AURA_MONITOR_MANAGER_H_
