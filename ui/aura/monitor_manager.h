// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_MONITOR_MANAGER_H_
#define UI_AURA_MONITOR_MANAGER_H_
#pragma once

#include <vector>

#include "base/basictypes.h"
#include "ui/aura/aura_export.h"
#include "ui/gfx/insets.h"
namespace gfx {
class Point;
}

namespace aura {
class Monitor;
class RootWindow;
class Window;

class AURA_EXPORT MonitorManager {
 public:
  MonitorManager();
  virtual ~MonitorManager();

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

 protected:
  friend class RootWindow;

 private:
  DISALLOW_COPY_AND_ASSIGN(MonitorManager);
};

AURA_EXPORT MonitorManager* CreateSingleMonitorManager(RootWindow* root_window);

}  // namespace aura

#endif  // UI_AURA_MONITOR_MANAGER_H_
