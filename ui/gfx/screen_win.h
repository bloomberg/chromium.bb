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
  virtual gfx::Point GetCursorScreenPoint() override;
  virtual gfx::NativeWindow GetWindowUnderCursor() override;
  virtual gfx::NativeWindow GetWindowAtScreenPoint(const gfx::Point& point)
      override;
  virtual int GetNumDisplays() const override;
  virtual std::vector<gfx::Display> GetAllDisplays() const override;
  virtual gfx::Display GetDisplayNearestWindow(
      gfx::NativeView window) const override;
  virtual gfx::Display GetDisplayNearestPoint(
      const gfx::Point& point) const override;
  virtual gfx::Display GetDisplayMatching(
      const gfx::Rect& match_rect) const override;
  virtual gfx::Display GetPrimaryDisplay() const override;
  virtual void AddObserver(DisplayObserver* observer) override;
  virtual void RemoveObserver(DisplayObserver* observer) override;

  // Overriden from gfx::SingletonHwnd::Observer.
  virtual void OnWndProc(HWND hwnd,
                         UINT message,
                         WPARAM wparam,
                         LPARAM lparam) override;

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
