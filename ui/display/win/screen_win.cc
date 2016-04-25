// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/win/screen_win.h"

#include <windows.h>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "ui/display/display.h"
#include "ui/display/win/display_info.h"
#include "ui/display/win/dpi.h"
#include "ui/display/win/screen_win_display.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size_conversions.h"

namespace display {
namespace win {

namespace {

std::vector<ScreenWinDisplay> DisplayInfosToScreenWinDisplays(
    const std::vector<DisplayInfo>& display_infos) {
  std::vector<ScreenWinDisplay> screen_win_displays;
  for (const auto& display_info : display_infos)
    screen_win_displays.push_back(ScreenWinDisplay(display_info));

  return screen_win_displays;
}

std::vector<display::Display> ScreenWinDisplaysToDisplays(
    const std::vector<ScreenWinDisplay>& screen_win_displays) {
  std::vector<display::Display> displays;
  for (const auto& screen_win_display : screen_win_displays)
    displays.push_back(screen_win_display.display());

  return displays;
}

MONITORINFOEX MonitorInfoFromHMONITOR(HMONITOR monitor) {
  MONITORINFOEX monitor_info;
  ::ZeroMemory(&monitor_info, sizeof(monitor_info));
  monitor_info.cbSize = sizeof(monitor_info);
  ::GetMonitorInfo(monitor, &monitor_info);
  return monitor_info;
}

BOOL CALLBACK EnumMonitorCallback(HMONITOR monitor,
                                  HDC hdc,
                                  LPRECT rect,
                                  LPARAM data) {
  std::vector<DisplayInfo>* display_infos =
      reinterpret_cast<std::vector<DisplayInfo>*>(data);
  DCHECK(display_infos);
  display_infos->push_back(DisplayInfo(MonitorInfoFromHMONITOR(monitor),
                                       GetDPIScale()));
  return TRUE;
}

std::vector<DisplayInfo> GetDisplayInfosFromSystem() {
  std::vector<DisplayInfo> display_infos;
  EnumDisplayMonitors(nullptr, nullptr, EnumMonitorCallback,
                      reinterpret_cast<LPARAM>(&display_infos));
  DCHECK_EQ(static_cast<size_t>(::GetSystemMetrics(SM_CMONITORS)),
            display_infos.size());
  return display_infos;
}

}  // namespace

ScreenWin::ScreenWin() {
  Initialize();
}

ScreenWin::~ScreenWin() = default;

// static
gfx::Point ScreenWin::ScreenToDIPPoint(const gfx::Point& pixel_point) {
  return ScaleToFlooredPoint(pixel_point, 1.0f / GetDPIScale());
}

// static
gfx::Point ScreenWin::DIPToScreenPoint(const gfx::Point& dip_point) {
  return ScaleToFlooredPoint(dip_point, GetDPIScale());
}

// static
gfx::Point ScreenWin::ClientToDIPPoint(HWND hwnd,
                                       const gfx::Point& client_point) {
  // TODO(robliao): Get the scale factor from |hwnd|.
  return ScreenToDIPPoint(client_point);
}

// static
gfx::Point ScreenWin::DIPToClientPoint(HWND hwnd, const gfx::Point& dip_point) {
  // TODO(robliao): Get the scale factor from |hwnd|.
  return DIPToScreenPoint(dip_point);
}

// static
gfx::Rect ScreenWin::ScreenToDIPRect(HWND hwnd, const gfx::Rect& pixel_bounds) {
  // It's important we scale the origin and size separately. If we instead
  // calculated the size from the floored origin and ceiled right the size could
  // vary depending upon where the two points land. That would cause problems
  // for the places this code is used (in particular mapping from native window
  // bounds to DIPs).
  return gfx::Rect(ScreenToDIPPoint(pixel_bounds.origin()),
                   ScreenToDIPSize(hwnd, pixel_bounds.size()));
}

// static
gfx::Rect ScreenWin::DIPToScreenRect(HWND hwnd, const gfx::Rect& dip_bounds) {
  // See comment in ScreenToDIPRect for why we calculate size like this.
  return gfx::Rect(DIPToScreenPoint(dip_bounds.origin()),
                   DIPToScreenSize(hwnd, dip_bounds.size()));
}

// static
gfx::Rect ScreenWin::ClientToDIPRect(HWND hwnd, const gfx::Rect& pixel_bounds) {
  return ScreenToDIPRect(hwnd, pixel_bounds);
}

// static
gfx::Rect ScreenWin::DIPToClientRect(HWND hwnd, const gfx::Rect& dip_bounds) {
  return DIPToScreenRect(hwnd, dip_bounds);
}

// static
gfx::Size ScreenWin::ScreenToDIPSize(HWND hwnd,
                                     const gfx::Size& size_in_pixels) {
  // Always ceil sizes. Otherwise we may be leaving off part of the bounds.
  // TODO(robliao): Get the scale factor from |hwnd|.
  return ScaleToCeiledSize(size_in_pixels, 1.0f / GetDPIScale());
}

// static
gfx::Size ScreenWin::DIPToScreenSize(HWND hwnd, const gfx::Size& dip_size) {
  // Always ceil sizes. Otherwise we may be leaving off part of the bounds.
  // TODO(robliao): Get the scale factor from |hwnd|.
  return ScaleToCeiledSize(dip_size, GetDPIScale());
}

HWND ScreenWin::GetHWNDFromNativeView(gfx::NativeView window) const {
  NOTREACHED();
  return nullptr;
}

gfx::NativeWindow ScreenWin::GetNativeWindowFromHWND(HWND hwnd) const {
  NOTREACHED();
  return nullptr;
}

gfx::Point ScreenWin::GetCursorScreenPoint() {
  POINT pt;
  ::GetCursorPos(&pt);
  gfx::Point cursor_pos_pixels(pt);
  return ScreenToDIPPoint(cursor_pos_pixels);
}

gfx::NativeWindow ScreenWin::GetWindowUnderCursor() {
  POINT cursor_loc;
  HWND hwnd =
      ::GetCursorPos(&cursor_loc) ? ::WindowFromPoint(cursor_loc) : nullptr;
  return GetNativeWindowFromHWND(hwnd);
}

gfx::NativeWindow ScreenWin::GetWindowAtScreenPoint(const gfx::Point& point) {
  gfx::Point point_in_pixels = DIPToScreenPoint(point);
  return GetNativeWindowFromHWND(WindowFromPoint(point_in_pixels.ToPOINT()));
}

int ScreenWin::GetNumDisplays() const {
  return static_cast<int>(screen_win_displays_.size());
}

std::vector<display::Display> ScreenWin::GetAllDisplays() const {
  return ScreenWinDisplaysToDisplays(screen_win_displays_);
}

display::Display ScreenWin::GetDisplayNearestWindow(
    gfx::NativeView window) const {
  HWND window_hwnd = GetHWNDFromNativeView(window);
  if (!window_hwnd) {
    // When |window| isn't rooted to a display, we should just return the
    // default display so we get some correct display information like the
    // scaling factor.
    return GetPrimaryDisplay();
  }
  ScreenWinDisplay screen_win_display =
      GetScreenWinDisplayNearestHWND(window_hwnd);
  return screen_win_display.display();
}

Display ScreenWin::GetDisplayNearestPoint(const gfx::Point& point) const {
  gfx::Point screen_point(DIPToScreenPoint(point));
  ScreenWinDisplay screen_win_display =
      GetScreenWinDisplayNearestScreenPoint(screen_point);
  return screen_win_display.display();
}

display::Display ScreenWin::GetDisplayMatching(
    const gfx::Rect& match_rect) const {
  ScreenWinDisplay screen_win_display =
      GetScreenWinDisplayNearestScreenRect(match_rect);
  return screen_win_display.display();
}

display::Display ScreenWin::GetPrimaryDisplay() const {
  return GetPrimaryScreenWinDisplay().display();
}

void ScreenWin::AddObserver(display::DisplayObserver* observer) {
  change_notifier_.AddObserver(observer);
}

void ScreenWin::RemoveObserver(display::DisplayObserver* observer) {
  change_notifier_.RemoveObserver(observer);
}

void ScreenWin::UpdateFromDisplayInfos(
    const std::vector<DisplayInfo>& display_infos) {
  screen_win_displays_ = DisplayInfosToScreenWinDisplays(display_infos);
}

void ScreenWin::Initialize() {
  singleton_hwnd_observer_.reset(
      new gfx::SingletonHwndObserver(
          base::Bind(&ScreenWin::OnWndProc, base::Unretained(this))));
  UpdateFromDisplayInfos(GetDisplayInfosFromSystem());
}

MONITORINFOEX ScreenWin::MonitorInfoFromScreenPoint(
    const gfx::Point& screen_point) const {
  POINT initial_loc = { screen_point.x(), screen_point.y() };
  return MonitorInfoFromHMONITOR(::MonitorFromPoint(initial_loc,
                                                    MONITOR_DEFAULTTONEAREST));
}

MONITORINFOEX ScreenWin::MonitorInfoFromScreenRect(const gfx::Rect& screen_rect)
    const {
  RECT win_rect = screen_rect.ToRECT();
  return MonitorInfoFromHMONITOR(::MonitorFromRect(&win_rect,
                                                   MONITOR_DEFAULTTONEAREST));
}

MONITORINFOEX ScreenWin::MonitorInfoFromWindow(HWND hwnd,
                                               DWORD default_options) const {
  return MonitorInfoFromHMONITOR(::MonitorFromWindow(hwnd, default_options));
}

HWND ScreenWin::GetRootWindow(HWND hwnd) const {
  return ::GetAncestor(hwnd, GA_ROOT);
}

void ScreenWin::OnWndProc(HWND hwnd,
                          UINT message,
                          WPARAM wparam,
                          LPARAM lparam) {
  if (message != WM_DISPLAYCHANGE)
    return;

  std::vector<display::Display> old_displays = GetAllDisplays();
  UpdateFromDisplayInfos(GetDisplayInfosFromSystem());
  change_notifier_.NotifyDisplaysChanged(old_displays, GetAllDisplays());
}

ScreenWinDisplay ScreenWin::GetScreenWinDisplayNearestHWND(HWND hwnd)
    const {
  return GetScreenWinDisplay(MonitorInfoFromWindow(hwnd,
                                                   MONITOR_DEFAULTTONEAREST));
}

ScreenWinDisplay ScreenWin::GetScreenWinDisplayNearestScreenRect(
    const gfx::Rect& screen_rect) const {
  return GetScreenWinDisplay(MonitorInfoFromScreenRect(screen_rect));
}

ScreenWinDisplay ScreenWin::GetScreenWinDisplayNearestScreenPoint(
    const gfx::Point& screen_point) const {
  return GetScreenWinDisplay(MonitorInfoFromScreenPoint(screen_point));
}

ScreenWinDisplay ScreenWin::GetPrimaryScreenWinDisplay() const {
  MONITORINFOEX monitor_info = MonitorInfoFromWindow(nullptr,
                                                     MONITOR_DEFAULTTOPRIMARY);
  ScreenWinDisplay screen_win_display = GetScreenWinDisplay(monitor_info);
  display::Display display = screen_win_display.display();
  // The Windows primary monitor is defined to have an origin of (0, 0).
  DCHECK_EQ(0, display.bounds().origin().x());
  DCHECK_EQ(0, display.bounds().origin().y());
  return screen_win_display;
}

ScreenWinDisplay ScreenWin::GetScreenWinDisplay(
    const MONITORINFOEX& monitor_info) const {
  int64_t id = DisplayInfo::DeviceIdFromDeviceName(monitor_info.szDevice);
  for (const auto& screen_win_display : screen_win_displays_) {
    if (screen_win_display.display().id() == id)
      return screen_win_display;
  }
  // There is 1:1 correspondence between MONITORINFOEX and ScreenWinDisplay.
  // If we make it here, it means we have no displays and we should hand out the
  // default display.
  DCHECK_EQ(screen_win_displays_.size(), 0u);
  return ScreenWinDisplay();
}

}  // namespace win
}  // namespace display
