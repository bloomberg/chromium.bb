// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_SCREEN_WIN_H_
#define UI_GFX_SCREEN_WIN_H_

#include "base/compiler_specific.h"
#include "ui/gfx/display_change_notifier.h"
#include "ui/gfx/gfx_export.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/win/singleton_hwnd.h"

namespace gfx {

class GFX_EXPORT ScreenWin : public Screen,
                             public SingletonHwnd::Observer {
 public:
  ScreenWin();
  virtual ~ScreenWin();

 protected:
  // Overridden from gfx::Screen:
  virtual bool IsDIPEnabled() OVERRIDE;
  virtual gfx::Point GetCursorScreenPoint() OVERRIDE;
  virtual gfx::NativeWindow GetWindowUnderCursor() OVERRIDE;
  virtual gfx::NativeWindow GetWindowAtScreenPoint(const gfx::Point& point)
      OVERRIDE;
  virtual int GetNumDisplays() const OVERRIDE;
  virtual std::vector<gfx::Display> GetAllDisplays() const OVERRIDE;
  virtual gfx::Display GetDisplayNearestWindow(
      gfx::NativeView window) const OVERRIDE;
  virtual gfx::Display GetDisplayNearestPoint(
      const gfx::Point& point) const OVERRIDE;
  virtual gfx::Display GetDisplayMatching(
      const gfx::Rect& match_rect) const OVERRIDE;
  virtual gfx::Display GetPrimaryDisplay() const OVERRIDE;
  virtual void AddObserver(DisplayObserver* observer) OVERRIDE;
  virtual void RemoveObserver(DisplayObserver* observer) OVERRIDE;

  // Overriden from gfx::SingletonHwnd::Observer.
  virtual void OnWndProc(HWND hwnd,
                         UINT message,
                         WPARAM wparam,
                         LPARAM lparam) OVERRIDE;

  // Returns the HWND associated with the NativeView.
  virtual HWND GetHWNDFromNativeView(NativeView window) const;

  // Returns the NativeView associated with the HWND.
  virtual NativeWindow GetNativeWindowFromHWND(HWND hwnd) const;

 private:
  // Helper implementing the DisplayObserver handling.
  gfx::DisplayChangeNotifier change_notifier_;

  // Current list of displays.
  std::vector<gfx::Display> displays_;

  DISALLOW_COPY_AND_ASSIGN(ScreenWin);
};

}  // namespace gfx

#endif  // UI_GFX_SCREEN_WIN_H_
