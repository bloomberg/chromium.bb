// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/screen.h"

#include <windows.h>

namespace views {

// static
gfx::Point Screen::GetCursorScreenPoint() {
  POINT pt;
  GetCursorPos(&pt);
  return gfx::Point(pt);
}

// static
gfx::Rect Screen::GetMonitorWorkAreaNearestWindow(gfx::NativeWindow window) {
  MONITORINFO monitor_info;
  monitor_info.cbSize = sizeof(monitor_info);
  GetMonitorInfo(MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST),
                 &monitor_info);
  return gfx::Rect(monitor_info.rcWork);
}

// static
gfx::Rect Screen::GetMonitorAreaNearestWindow(gfx::NativeWindow window) {
  MONITORINFO monitor_info;
  monitor_info.cbSize = sizeof(monitor_info);
  GetMonitorInfo(MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST),
                 &monitor_info);
  return gfx::Rect(monitor_info.rcMonitor);
}

// static
gfx::Rect Screen::GetMonitorAreaNearestPoint(const gfx::Point& point) {
  POINT initial_loc = { point.x(), point.y() };
  HMONITOR monitor = MonitorFromPoint(initial_loc, MONITOR_DEFAULTTONEAREST);
  if (!monitor)
    return gfx::Rect();

  MONITORINFO mi = {0};
  mi.cbSize = sizeof(mi);
  GetMonitorInfo(monitor, &mi);
  return gfx::Rect(mi.rcMonitor);
}

}  // namespace

