// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/desktop/desktop_screen_win.h"

#include "base/logging.h"
#include "ui/aura/desktop/desktop_screen.h"
#include "ui/aura/root_window.h"
#include "ui/aura/root_window_host.h"
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

namespace aura {

////////////////////////////////////////////////////////////////////////////////
// DesktopScreenWin, public:

DesktopScreenWin::DesktopScreenWin() {
}

DesktopScreenWin::~DesktopScreenWin() {
}

////////////////////////////////////////////////////////////////////////////////
// DesktopScreenWin, gfx::ScreenImpl implementation:

gfx::Point DesktopScreenWin::GetCursorScreenPoint() {
  POINT pt;
  GetCursorPos(&pt);
  return gfx::Point(pt);
}

gfx::NativeWindow DesktopScreenWin::GetWindowAtCursorScreenPoint() {
  POINT location;
  gfx::AcceleratedWidget accelerated_widget =
      GetCursorPos(&location) ? WindowFromPoint(location) : NULL;
  RootWindowHost* host = NULL;
  if (::IsWindow(accelerated_widget))
    host = RootWindowHost::GetForAcceleratedWidget(accelerated_widget);
  return host ? host->GetRootWindow() : NULL;
}

int DesktopScreenWin::GetNumMonitors() {
  return GetSystemMetrics(SM_CMONITORS);
}

gfx::Monitor DesktopScreenWin::GetMonitorNearestWindow(
    gfx::NativeView window) const {
  gfx::AcceleratedWidget accelerated_window =
      window->GetRootWindow()->GetAcceleratedWidget();
  MONITORINFO monitor_info;
  monitor_info.cbSize = sizeof(monitor_info);
  GetMonitorInfo(MonitorFromWindow(accelerated_window,
                                   MONITOR_DEFAULTTONEAREST),
                 &monitor_info);
  return GetMonitor(monitor_info);
}

gfx::Monitor DesktopScreenWin::GetMonitorNearestPoint(
    const gfx::Point& point) const {
  POINT initial_loc = { point.x(), point.y() };
  HMONITOR monitor = MonitorFromPoint(initial_loc, MONITOR_DEFAULTTONEAREST);
  MONITORINFO mi = {0};
  mi.cbSize = sizeof(mi);
  if (monitor && GetMonitorInfo(monitor, &mi))
    return GetMonitor(mi);
  return gfx::Monitor();
}

gfx::Monitor DesktopScreenWin::GetPrimaryMonitor() const {
  MONITORINFO mi = GetMonitorInfoForMonitor(
      MonitorFromWindow(NULL, MONITOR_DEFAULTTOPRIMARY));
  gfx::Monitor monitor = GetMonitor(mi);
  DCHECK_EQ(GetSystemMetrics(SM_CXSCREEN), monitor.size().width());
  DCHECK_EQ(GetSystemMetrics(SM_CYSCREEN), monitor.size().height());
  return monitor;
}

////////////////////////////////////////////////////////////////////////////////

gfx::ScreenImpl* CreateDesktopScreen() {
  return new DesktopScreenWin;
}

}  // namespace aura
