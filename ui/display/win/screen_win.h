// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_WIN_SCREEN_WIN_H_
#define UI_DISPLAY_WIN_SCREEN_WIN_H_

#include <windows.h>

#include <memory>
#include <vector>

#include "base/macros.h"
#include "ui/display/display_change_notifier.h"
#include "ui/display/display_export.h"
#include "ui/display/screen.h"
#include "ui/display/win/color_profile_reader.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/win/singleton_hwnd_observer.h"

namespace gfx {
class Display;
class Point;
class Rect;
class Size;
}   // namespace gfx

namespace display {
namespace win {

class DisplayInfo;
class ScreenWinDisplay;

class DISPLAY_EXPORT ScreenWin : public Screen,
                                 public ColorProfileReader::Client {
 public:
  ScreenWin();
  ~ScreenWin() override;

  // Converts a screen physical point to a screen DIP point.
  // The DPI scale is performed relative to the display containing the physical
  // point.
  static gfx::Point ScreenToDIPPoint(const gfx::Point& pixel_point);

  // Converts a screen DIP point to a screen physical point.
  // The DPI scale is performed relative to the display containing the DIP
  // point.
  static gfx::Point DIPToScreenPoint(const gfx::Point& dip_point);

  // Converts a client physical point relative to |hwnd| to a client DIP point.
  // The DPI scale is performed relative to |hwnd| using an origin of (0, 0).
  static gfx::Point ClientToDIPPoint(HWND hwnd, const gfx::Point& client_point);

  // Converts a client DIP point relative to |hwnd| to a client physical point.
  // The DPI scale is performed relative to |hwnd| using an origin of (0, 0).
  static gfx::Point DIPToClientPoint(HWND hwnd, const gfx::Point& dip_point);

  // WARNING: There is no right way to scale sizes and rects.
  // Sometimes you may need the enclosing rect (which favors transformations
  // that stretch the bounds towards integral values) or the enclosed rect
  // (transformations that shrink the bounds towards integral values).
  // This implementation favors the enclosing rect.
  //
  // Understand which you need before blindly assuming this is the right way.

  // Converts a screen physical rect to a screen DIP rect.
  // The DPI scale is performed relative to the display nearest to |hwnd|.
  // If |hwnd| is null, scaling will be performed to the display nearest to
  // |pixel_bounds|.
  static gfx::Rect ScreenToDIPRect(HWND hwnd, const gfx::Rect& pixel_bounds);

  // Converts a screen DIP rect to a screen physical rect.
  // The DPI scale is performed relative to the display nearest to |hwnd|.
  // If |hwnd| is null, scaling will be performed to the display nearest to
  // |dip_bounds|.
  static gfx::Rect DIPToScreenRect(HWND hwnd, const gfx::Rect& dip_bounds);

  // Converts a client physical rect to a client DIP rect.
  // The DPI scale is performed relative to |hwnd| using an origin of (0, 0).
  static gfx::Rect ClientToDIPRect(HWND hwnd, const gfx::Rect& pixel_bounds);

  // Converts a client DIP rect to a client physical rect.
  // The DPI scale is performed relative to |hwnd| using an origin of (0, 0).
  static gfx::Rect DIPToClientRect(HWND hwnd, const gfx::Rect& dip_bounds);

  // Converts a physical size to a DIP size.
  // The DPI scale is performed relative to the display nearest to |hwnd|.
  static gfx::Size ScreenToDIPSize(HWND hwnd, const gfx::Size& size_in_pixels);

  // Converts a DIP size to a physical size.
  // The DPI scale is performed relative to the display nearest to |hwnd|.
  static gfx::Size DIPToScreenSize(HWND hwnd, const gfx::Size& dip_size);

  // Returns the result of GetSystemMetrics for |metric| scaled to |hwnd|'s DPI.
  // Use this function if you're already working with screen pixels, as this
  // helps reduce any cascading rounding errors from DIP to the |hwnd|'s DPI.
  static int GetSystemMetricsForHwnd(HWND hwnd, int metric);

  // Returns the result of GetSystemMetrics for |metric| in DIP.
  // Use this function if you need to work in DIP and can tolerate cascading
  // rounding errors towards screen pixels.
  static int GetSystemMetricsInDIP(int metric);

  // Returns |hwnd|'s scale factor.
  static float GetScaleFactorForHWND(HWND hwnd);

  // Returns the system's global scale factor, ignoring the value of
  // --force-device-scale-factor. Only use this if you are working with Windows
  // metrics global to the system. Otherwise you should call
  // GetScaleFactorForHWND() to get the correct scale factor for the monitor
  // you are targeting.
  static float GetSystemScaleFactor();

  // Returns the HWND associated with the NativeView.
  virtual HWND GetHWNDFromNativeView(gfx::NativeView view) const;

  // Returns the NativeView associated with the HWND.
  virtual gfx::NativeWindow GetNativeWindowFromHWND(HWND hwnd) const;

 protected:
  ScreenWin(bool initialize);

  // Screen:
  gfx::Point GetCursorScreenPoint() override;
  bool IsWindowUnderCursor(gfx::NativeWindow window) override;
  gfx::NativeWindow GetWindowAtScreenPoint(const gfx::Point& point) override;
  int GetNumDisplays() const override;
  const std::vector<Display>& GetAllDisplays() const override;
  Display GetDisplayNearestWindow(gfx::NativeWindow window) const override;
  Display GetDisplayNearestPoint(const gfx::Point& point) const override;
  Display GetDisplayMatching(const gfx::Rect& match_rect) const override;
  Display GetPrimaryDisplay() const override;
  void AddObserver(DisplayObserver* observer) override;
  void RemoveObserver(DisplayObserver* observer) override;
  gfx::Rect ScreenToDIPRectInWindow(
      gfx::NativeView view, const gfx::Rect& screen_rect) const override;
  gfx::Rect DIPToScreenRectInWindow(
      gfx::NativeView view, const gfx::Rect& dip_rect) const override;

  // ColorProfileReader::Client:
  void OnColorProfilesChanged() override;

  void UpdateFromDisplayInfos(const std::vector<DisplayInfo>& display_infos);

  // Virtual to support mocking by unit tests.
  virtual MONITORINFOEX MonitorInfoFromScreenPoint(
      const gfx::Point& screen_point) const;
  virtual MONITORINFOEX MonitorInfoFromScreenRect(const gfx::Rect& screen_rect)
      const;
  virtual MONITORINFOEX MonitorInfoFromWindow(HWND hwnd, DWORD default_options)
      const;
  virtual HWND GetRootWindow(HWND hwnd) const;
  virtual int GetSystemMetrics(int metric) const;

 private:
  void Initialize();
  void OnWndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam);

  // Returns the ScreenWinDisplay closest to or enclosing |hwnd|.
  ScreenWinDisplay GetScreenWinDisplayNearestHWND(HWND hwnd) const;

  // Returns the ScreenWinDisplay closest to or enclosing |screen_rect|.
  ScreenWinDisplay GetScreenWinDisplayNearestScreenRect(
      const gfx::Rect& screen_rect) const;

  // Returns the ScreenWinDisplay closest to or enclosing |screen_point|.
  ScreenWinDisplay GetScreenWinDisplayNearestScreenPoint(
      const gfx::Point& screen_point) const;

  // Returns the ScreenWinDisplay closest to or enclosing |dip_point|.
  ScreenWinDisplay GetScreenWinDisplayNearestDIPPoint(
      const gfx::Point& dip_point) const;

  // Returns the ScreenWinDisplay closest to or enclosing |dip_rect|.
  ScreenWinDisplay GetScreenWinDisplayNearestDIPRect(
      const gfx::Rect& dip_rect) const;

  // Returns the ScreenWinDisplay corresponding to the primary monitor.
  ScreenWinDisplay GetPrimaryScreenWinDisplay() const;

  ScreenWinDisplay GetScreenWinDisplay(const MONITORINFOEX& monitor_info) const;

  // Returns the result of calling |getter| with |value| on the global
  // ScreenWin if it exists, otherwise return the default ScreenWinDisplay.
  template <typename Getter, typename GetterType>
  static ScreenWinDisplay GetScreenWinDisplayVia(Getter getter,
                                                 GetterType value);

  void RecordDisplayScaleFactors() const;

  // Helper implementing the DisplayObserver handling.
  DisplayChangeNotifier change_notifier_;

  std::unique_ptr<gfx::SingletonHwndObserver> singleton_hwnd_observer_;

  // Current list of ScreenWinDisplays.
  std::vector<ScreenWinDisplay> screen_win_displays_;

  // The Displays corresponding to |screen_win_displays_| for GetAllDisplays().
  // This must be updated anytime |screen_win_displays_| is updated.
  std::vector<Display> displays_;

  // A helper to read color profiles from the filesystem.
  std::unique_ptr<ColorProfileReader> color_profile_reader_;

  // Whether or not HDR mode is enabled.
  // TODO(ccameron): Set this via the GPU process when the system "HDR and
  // advanced color" setting is enabled.
  bool hdr_enabled_ = false;

  DISALLOW_COPY_AND_ASSIGN(ScreenWin);
};

}  // namespace win
}  // namespace display

#endif  // UI_DISPLAY_WIN_SCREEN_WIN_H_
