// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/screen.h"

#include <windows.h>

#include "base/logging.h"
#include "ui/gfx/display.h"

namespace {

MONITORINFO GetMonitorInfoForMonitor(HMONITOR monitor) {
  MONITORINFO monitor_info = { 0 };
  monitor_info.cbSize = sizeof(monitor_info);
  GetMonitorInfo(monitor, &monitor_info);
  return monitor_info;
}

gfx::Display GetDisplay(MONITORINFO& monitor_info) {
  // TODO(oshima): Implement ID and Observer.
  gfx::Display display(0, gfx::Rect(monitor_info.rcMonitor));
  display.set_work_area(gfx::Rect(monitor_info.rcWork));
  return display;
}

}  // namespace

namespace gfx {

// static
bool Screen::IsDIPEnabled() {
  return false;
}

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
int Screen::GetNumDisplays() {
  return GetSystemMetrics(SM_CMONITORS);
}

// static
gfx::Display Screen::GetDisplayNearestWindow(gfx::NativeView window) {
  MONITORINFO monitor_info;
  monitor_info.cbSize = sizeof(monitor_info);
  GetMonitorInfo(MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST),
                 &monitor_info);
  return GetDisplay(monitor_info);
}

// static
gfx::Display Screen::GetDisplayNearestPoint(const gfx::Point& point) {
  POINT initial_loc = { point.x(), point.y() };
  HMONITOR monitor = MonitorFromPoint(initial_loc, MONITOR_DEFAULTTONEAREST);
  MONITORINFO mi = {0};
  mi.cbSize = sizeof(mi);
  if (monitor && GetMonitorInfo(monitor, &mi))
    return GetDisplay(mi);
  return gfx::Display();
}

// static
gfx::Display Screen::GetPrimaryDisplay() {
  MONITORINFO mi = GetMonitorInfoForMonitor(
      MonitorFromWindow(NULL, MONITOR_DEFAULTTOPRIMARY));
  gfx::Display display = GetDisplay(mi);
  DCHECK_EQ(GetSystemMetrics(SM_CXSCREEN), display.size().width());
  DCHECK_EQ(GetSystemMetrics(SM_CYSCREEN), display.size().height());
  return display;
}

// static
gfx::Display Screen::GetDisplayMatching(const gfx::Rect& match_rect) {
  RECT other_bounds_rect = match_rect.ToRECT();
  MONITORINFO monitor_info = GetMonitorInfoForMonitor(MonitorFromRect(
      &other_bounds_rect, MONITOR_DEFAULTTONEAREST));
  return GetDisplay(monitor_info);
}

}  // namespace gfx
