// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/screen_win.h"

#include <windows.h>

#include "base/hash.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/win_util.h"
#include "ui/gfx/display.h"
#include "ui/gfx/win/dpi.h"

namespace {

MONITORINFOEX GetMonitorInfoForMonitor(HMONITOR monitor) {
  MONITORINFOEX monitor_info;
  ZeroMemory(&monitor_info, sizeof(MONITORINFOEX));
  monitor_info.cbSize = sizeof(monitor_info);
  GetMonitorInfo(monitor, &monitor_info);
  return monitor_info;
}

gfx::Display GetDisplay(MONITORINFOEX& monitor_info) {
  int64 id = static_cast<int64>(
      base::Hash(base::WideToUTF8(monitor_info.szDevice)));
  gfx::Rect bounds = gfx::Rect(monitor_info.rcMonitor);
  gfx::Display display(id, bounds);
  display.set_work_area(gfx::Rect(monitor_info.rcWork));
  display.SetScaleAndBounds(gfx::win::GetDeviceScaleFactor(), bounds);
  return display;
}

BOOL CALLBACK EnumMonitorCallback(HMONITOR monitor,
                                  HDC hdc,
                                  LPRECT rect,
                                  LPARAM data) {
  std::vector<gfx::Display>* all_displays =
      reinterpret_cast<std::vector<gfx::Display>*>(data);
  DCHECK(all_displays);

  MONITORINFOEX monitor_info = GetMonitorInfoForMonitor(monitor);
  gfx::Display display = GetDisplay(monitor_info);
  all_displays->push_back(display);
  return TRUE;
}

std::vector<gfx::Display> GetDisplays() {
  std::vector<gfx::Display> displays;
  EnumDisplayMonitors(NULL, NULL, EnumMonitorCallback,
                      reinterpret_cast<LPARAM>(&displays));
  return displays;
}

}  // namespace

namespace gfx {

ScreenWin::ScreenWin()
    : displays_(GetDisplays()) {
  SingletonHwnd::GetInstance()->AddObserver(this);
}

ScreenWin::~ScreenWin() {
  SingletonHwnd::GetInstance()->RemoveObserver(this);
}

bool ScreenWin::IsDIPEnabled() {
  return IsInHighDPIMode();
}

gfx::Point ScreenWin::GetCursorScreenPoint() {
  POINT pt;
  GetCursorPos(&pt);
  gfx::Point cursor_pos_pixels(pt);
  return gfx::win::ScreenToDIPPoint(cursor_pos_pixels);
}

gfx::NativeWindow ScreenWin::GetWindowUnderCursor() {
  POINT cursor_loc;
  HWND hwnd = GetCursorPos(&cursor_loc) ? WindowFromPoint(cursor_loc) : NULL;
  return GetNativeWindowFromHWND(hwnd);
}

gfx::NativeWindow ScreenWin::GetWindowAtScreenPoint(const gfx::Point& point) {
  gfx::Point point_in_pixels = gfx::win::DIPToScreenPoint(point);
  return GetNativeWindowFromHWND(WindowFromPoint(point_in_pixels.ToPOINT()));
}

int ScreenWin::GetNumDisplays() const {
  return GetSystemMetrics(SM_CMONITORS);
}

std::vector<gfx::Display> ScreenWin::GetAllDisplays() const {
  return displays_;
}

gfx::Display ScreenWin::GetDisplayNearestWindow(gfx::NativeView window) const {
  HWND window_hwnd = GetHWNDFromNativeView(window);
  if (!window_hwnd) {
    // When |window| isn't rooted to a display, we should just return the
    // default display so we get some correct display information like the
    // scaling factor.
    return GetPrimaryDisplay();
  }

  MONITORINFOEX monitor_info;
  monitor_info.cbSize = sizeof(monitor_info);
  GetMonitorInfo(MonitorFromWindow(window_hwnd, MONITOR_DEFAULTTONEAREST),
                 &monitor_info);
  return GetDisplay(monitor_info);
}

gfx::Display ScreenWin::GetDisplayNearestPoint(const gfx::Point& point) const {
  POINT initial_loc = { point.x(), point.y() };
  HMONITOR monitor = MonitorFromPoint(initial_loc, MONITOR_DEFAULTTONEAREST);
  MONITORINFOEX mi;
  ZeroMemory(&mi, sizeof(MONITORINFOEX));
  mi.cbSize = sizeof(mi);
  if (monitor && GetMonitorInfo(monitor, &mi)) {
    return GetDisplay(mi);
  }
  return gfx::Display();
}

gfx::Display ScreenWin::GetDisplayMatching(const gfx::Rect& match_rect) const {
  RECT other_bounds_rect = match_rect.ToRECT();
  MONITORINFOEX monitor_info = GetMonitorInfoForMonitor(MonitorFromRect(
      &other_bounds_rect, MONITOR_DEFAULTTONEAREST));
  return GetDisplay(monitor_info);
}

gfx::Display ScreenWin::GetPrimaryDisplay() const {
  MONITORINFOEX mi = GetMonitorInfoForMonitor(
      MonitorFromWindow(NULL, MONITOR_DEFAULTTOPRIMARY));
  gfx::Display display = GetDisplay(mi);
  // TODO(kevers|girard): Test if these checks can be reintroduced for high-DIP
  // once more of the app is DIP-aware.
  if (!(IsInHighDPIMode() || IsHighDPIEnabled())) {
    DCHECK_EQ(GetSystemMetrics(SM_CXSCREEN), display.size().width());
    DCHECK_EQ(GetSystemMetrics(SM_CYSCREEN), display.size().height());
  }
  return display;
}

void ScreenWin::AddObserver(DisplayObserver* observer) {
  change_notifier_.AddObserver(observer);
}

void ScreenWin::RemoveObserver(DisplayObserver* observer) {
  change_notifier_.RemoveObserver(observer);
}

void ScreenWin::OnWndProc(HWND hwnd,
                          UINT message,
                          WPARAM wparam,
                          LPARAM lparam) {
  if (message != WM_DISPLAYCHANGE)
    return;

  std::vector<gfx::Display> old_displays = displays_;
  displays_ = GetDisplays();

  change_notifier_.NotifyDisplaysChanged(old_displays, displays_);
}

HWND ScreenWin::GetHWNDFromNativeView(NativeView window) const {
  NOTREACHED();
  return NULL;
}

NativeWindow ScreenWin::GetNativeWindowFromHWND(HWND hwnd) const {
  NOTREACHED();
  return NULL;
}

}  // namespace gfx
