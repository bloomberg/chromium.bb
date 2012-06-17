// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/desktop/desktop_screen_win.h"

#include "base/logging.h"
#include "ui/aura/desktop/desktop_screen.h"
#include "ui/aura/root_window.h"
#include "ui/aura/root_window_host.h"
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

int DesktopScreenWin::GetNumDisplays() {
  return GetSystemMetrics(SM_CMONITORS);
}

gfx::Display DesktopScreenWin::GetDisplayNearestWindow(
    gfx::NativeView window) const {
  gfx::AcceleratedWidget accelerated_window =
      window->GetRootWindow()->GetAcceleratedWidget();
  MONITORINFO monitor_info;
  monitor_info.cbSize = sizeof(monitor_info);
  GetMonitorInfo(MonitorFromWindow(accelerated_window,
                                   MONITOR_DEFAULTTONEAREST),
                 &monitor_info);
  return GetDisplay(monitor_info);
}

gfx::Display DesktopScreenWin::GetDisplayNearestPoint(
    const gfx::Point& point) const {
  POINT initial_loc = { point.x(), point.y() };
  HMONITOR monitor = MonitorFromPoint(initial_loc, MONITOR_DEFAULTTONEAREST);
  MONITORINFO mi = {0};
  mi.cbSize = sizeof(mi);
  if (monitor && GetMonitorInfo(monitor, &mi))
    return GetDisplay(mi);
  return gfx::Display();
}

gfx::Display DesktopScreenWin::GetPrimaryDisplay() const {
  MONITORINFO mi = GetMonitorInfoForMonitor(
      MonitorFromWindow(NULL, MONITOR_DEFAULTTOPRIMARY));
  gfx::Display display = GetDisplay(mi);
  DCHECK_EQ(GetSystemMetrics(SM_CXSCREEN), display.size().width());
  DCHECK_EQ(GetSystemMetrics(SM_CYSCREEN), display.size().height());
  return display;
}

////////////////////////////////////////////////////////////////////////////////

gfx::ScreenImpl* CreateDesktopScreen() {
  return new DesktopScreenWin;
}

}  // namespace aura
