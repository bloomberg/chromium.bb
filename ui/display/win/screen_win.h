// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_WIN_SCREEN_WIN_H_
#define UI_DISPLAY_WIN_SCREEN_WIN_H_

#include <windows.h>

#include <memory>
#include <vector>

#include "base/macros.h"
#include "ui/display/display_export.h"
#include "ui/gfx/display_change_notifier.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/win/singleton_hwnd_observer.h"

namespace gfx {
class Display;
class Point;
class Rect;
}   // namespace gfx

namespace display {
namespace win {

class DisplayInfo;
class ScreenWinDisplay;

class DISPLAY_EXPORT ScreenWin : public gfx::Screen {
 public:
  ScreenWin();
  ~ScreenWin() override;

  // Returns the HWND associated with the NativeView.
  virtual HWND GetHWNDFromNativeView(gfx::NativeView window) const;

  // Returns the NativeView associated with the HWND.
  virtual gfx::NativeWindow GetNativeWindowFromHWND(HWND hwnd) const;

 protected:
  // gfx::Screen:
  gfx::Point GetCursorScreenPoint() override;
  gfx::NativeWindow GetWindowUnderCursor() override;
  gfx::NativeWindow GetWindowAtScreenPoint(const gfx::Point& point) override;
  int GetNumDisplays() const override;
  std::vector<gfx::Display> GetAllDisplays() const override;
  gfx::Display GetDisplayNearestWindow(gfx::NativeView window) const override;
  gfx::Display GetDisplayNearestPoint(const gfx::Point& point) const override;
  gfx::Display GetDisplayMatching(const gfx::Rect& match_rect) const override;
  gfx::Display GetPrimaryDisplay() const override;
  void AddObserver(gfx::DisplayObserver* observer) override;
  void RemoveObserver(gfx::DisplayObserver* observer) override;

  void UpdateFromDisplayInfos(const std::vector<DisplayInfo>& display_infos);

  // Virtual to support mocking by unit tests.
  virtual void Initialize();
  virtual MONITORINFOEX MonitorInfoFromScreenPoint(
      const gfx::Point& screen_point) const;
  virtual MONITORINFOEX MonitorInfoFromScreenRect(const gfx::Rect& screen_rect)
      const;
  virtual MONITORINFOEX MonitorInfoFromWindow(HWND hwnd, DWORD default_options)
      const;
  virtual HWND GetRootWindow(HWND hwnd) const;

 private:
  void OnWndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);

  // Returns the ScreenWinDisplay closest to or enclosing |hwnd|.
  ScreenWinDisplay GetScreenWinDisplayNearestHWND(HWND hwnd) const;

  // Returns the ScreenWinDisplay closest to or enclosing |screen_rect|.
  ScreenWinDisplay GetScreenWinDisplayNearestScreenRect(
      const gfx::Rect& screen_rect) const;

  // Returns the ScreenWinDisplay closest to or enclosing |screen_point|.
  ScreenWinDisplay GetScreenWinDisplayNearestScreenPoint(
      const gfx::Point& screen_point) const;

  // Returns the ScreenWinDisplay corresponding to the primary monitor.
  ScreenWinDisplay GetPrimaryScreenWinDisplay() const;

  ScreenWinDisplay GetScreenWinDisplay(const MONITORINFOEX& monitor_info) const;

  // Helper implementing the DisplayObserver handling.
  gfx::DisplayChangeNotifier change_notifier_;

  std::unique_ptr<gfx::SingletonHwndObserver> singleton_hwnd_observer_;

  // Current list of ScreenWinDisplays.
  std::vector<ScreenWinDisplay> screen_win_displays_;

  DISALLOW_COPY_AND_ASSIGN(ScreenWin);
};

}  // namespace win
}  // namespace display

#endif  // UI_DISPLAY_WIN_SCREEN_WIN_H_
