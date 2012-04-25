// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/screen.h"

#include <windows.h>

#include "base/logging.h"
#include "ui/gfx/monitor.h"

namespace {

MONITORINFO GetMonitorInfoForMonitor(HMONITOR monitor) {
  MONITORINFO monitor_info = { 0 };
  monitor_info.cbSize = sizeof(monitor_info);
  GetMonitorInfo(monitor, &monitor_info);
  return monitor_info;
}

gfx::Monitor GetMonitor(MONITORINFO& monitor_info) {
  // TODO(oshima): Implement ID and Observer.
  gfx::Monitor monitor(0, gfx::Rect(monitor_info.rcMonitor));
  monitor.set_work_area(gfx::Rect(monitor_info.rcWork));
  return monitor;
}

}  // namespace

namespace gfx {

// static
gfx::Point Screen::GetCursorScreenPoint() {
  POINT pt;
  GetCursorPos(&pt);
  return gfx::Point(pt);
}

// static
gfx::NativeWindow Screen::GetWindowAtCursorScreenPoint() {
  POINT location;
  return GetCursorPos(&location) ? WindowFromPoint(location) : NULL;
}

// static
int Screen::GetNumMonitors() {
  return GetSystemMetrics(SM_CMONITORS);
}

// static
gfx::Monitor Screen::GetMonitorNearestWindow(gfx::NativeWindow window) {
  MONITORINFO monitor_info;
  monitor_info.cbSize = sizeof(monitor_info);
  GetMonitorInfo(MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST),
                 &monitor_info);
  return GetMonitor(monitor_info);
}

// static
gfx::Monitor Screen::GetMonitorNearestPoint(const gfx::Point& point) {
  POINT initial_loc = { point.x(), point.y() };
  HMONITOR monitor = MonitorFromPoint(initial_loc, MONITOR_DEFAULTTONEAREST);
  MONITORINFO mi = {0};
  mi.cbSize = sizeof(mi);
  if (monitor && GetMonitorInfo(monitor, &mi))
    return GetMonitor(mi);
  return gfx::Monitor();
}

// static
gfx::Monitor Screen::GetPrimaryMonitor() {
  MONITORINFO mi = GetMonitorInfoForMonitor(
      MonitorFromWindow(NULL, MONITOR_DEFAULTTOPRIMARY));
  gfx::Monitor monitor = GetMonitor(mi);
  DCHECK_EQ(GetSystemMetrics(SM_CXSCREEN), monitor.size().width());
  DCHECK_EQ(GetSystemMetrics(SM_CYSCREEN), monitor.size().height());
  return monitor;
}

// static
gfx::Monitor Screen::GetMonitorMatching(const gfx::Rect& match_rect) {
  RECT other_bounds_rect = match_rect.ToRECT();
  MONITORINFO monitor_info = GetMonitorInfoForMonitor(MonitorFromRect(
      &other_bounds_rect, MONITOR_DEFAULTTONEAREST));
  return GetMonitor(monitor_info);
}

}  // namespace gfx
