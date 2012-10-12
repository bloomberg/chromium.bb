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

class ScreenWin : public gfx::Screen {
 public:
  ScreenWin() {}

  bool IsDIPEnabled() OVERRIDE {
    return false;
  }

  gfx::Point GetCursorScreenPoint() OVERRIDE {
    POINT pt;
    GetCursorPos(&pt);
    return gfx::Point(pt);
  }

  gfx::NativeWindow GetWindowAtCursorScreenPoint() OVERRIDE {
    POINT location;
    return GetCursorPos(&location) ? WindowFromPoint(location) : NULL;
  }

  int GetNumDisplays() OVERRIDE {
    return GetSystemMetrics(SM_CMONITORS);
  }

  gfx::Display GetDisplayNearestWindow(gfx::NativeView window) const OVERRIDE {
    MONITORINFO monitor_info;
    monitor_info.cbSize = sizeof(monitor_info);
    GetMonitorInfo(MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST),
                  &monitor_info);
    return GetDisplay(monitor_info);
  }

  gfx::Display GetDisplayNearestPoint(const gfx::Point& point) const OVERRIDE {
    POINT initial_loc = { point.x(), point.y() };
    HMONITOR monitor = MonitorFromPoint(initial_loc, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi = {0};
    mi.cbSize = sizeof(mi);
    if (monitor && GetMonitorInfo(monitor, &mi))
      return GetDisplay(mi);
    return gfx::Display();
  }

  gfx::Display GetDisplayMatching(const gfx::Rect& match_rect) const OVERRIDE {
    RECT other_bounds_rect = match_rect.ToRECT();
    MONITORINFO monitor_info = GetMonitorInfoForMonitor(MonitorFromRect(
        &other_bounds_rect, MONITOR_DEFAULTTONEAREST));
    return GetDisplay(monitor_info);
  }

  gfx::Display GetPrimaryDisplay() const OVERRIDE {
    MONITORINFO mi = GetMonitorInfoForMonitor(
        MonitorFromWindow(NULL, MONITOR_DEFAULTTOPRIMARY));
    gfx::Display display = GetDisplay(mi);
    DCHECK_EQ(GetSystemMetrics(SM_CXSCREEN), display.size().width());
    DCHECK_EQ(GetSystemMetrics(SM_CYSCREEN), display.size().height());
    return display;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ScreenWin);
};

}  // namespace

namespace gfx {

Screen* CreateNativeScreen() {
  return new ScreenWin;
}

}  // namespace gfx
