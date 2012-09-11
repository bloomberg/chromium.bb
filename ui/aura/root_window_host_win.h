// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_ROOT_WINDOW_HOST_WIN_H_
#define UI_AURA_ROOT_WINDOW_HOST_WIN_H_

#include "base/compiler_specific.h"
#include "ui/aura/root_window_host.h"
#include "ui/base/win/window_impl.h"

namespace ui {
class ViewProp;
}

namespace aura {

class RootWindowHostWin : public RootWindowHost, public ui::WindowImpl {
 public:
  RootWindowHostWin(RootWindowHostDelegate* delegate,
                    const gfx::Rect& bounds);
  virtual ~RootWindowHostWin();

  // RootWindowHost:
  virtual RootWindow* GetRootWindow() OVERRIDE;
  virtual gfx::AcceleratedWidget GetAcceleratedWidget() OVERRIDE;
  virtual void Show() OVERRIDE;
  virtual void Hide() OVERRIDE;
  virtual void ToggleFullScreen() OVERRIDE;
  virtual gfx::Rect GetBounds() const OVERRIDE;
  virtual void SetBounds(const gfx::Rect& bounds) OVERRIDE;
  virtual gfx::Point GetLocationOnNativeScreen() const OVERRIDE;
  virtual void SetCapture() OVERRIDE;
  virtual void ReleaseCapture() OVERRIDE;
  virtual void SetCursor(gfx::NativeCursor cursor) OVERRIDE;
  virtual void ShowCursor(bool show) OVERRIDE;
  virtual bool QueryMouseLocation(gfx::Point* location_return) OVERRIDE;
  virtual bool ConfineCursorToRootWindow() OVERRIDE;
  virtual void UnConfineCursor() OVERRIDE;
  virtual void MoveCursorTo(const gfx::Point& location) OVERRIDE;
  virtual void SetFocusWhenShown(bool focus_when_shown) OVERRIDE;
  virtual bool GrabSnapshot(
      const gfx::Rect& snapshot_bounds,
      std::vector<unsigned char>* png_representation) OVERRIDE;
  virtual void PostNativeEvent(const base::NativeEvent& native_event) OVERRIDE;
  virtual void OnDeviceScaleFactorChanged(float device_scale_factor) OVERRIDE;
  virtual void PrepareForShutdown() OVERRIDE;

 private:
  BEGIN_MSG_MAP_EX(RootWindowHostWin)
    // Range handlers must go first!
    MESSAGE_RANGE_HANDLER_EX(WM_MOUSEFIRST, WM_MOUSELAST, OnMouseRange)
    MESSAGE_RANGE_HANDLER_EX(WM_NCMOUSEMOVE, WM_NCXBUTTONDBLCLK, OnMouseRange)

    // Mouse capture events.
    MESSAGE_HANDLER_EX(WM_CAPTURECHANGED, OnCaptureChanged)

    // Key events.
    MESSAGE_HANDLER_EX(WM_KEYDOWN, OnKeyEvent)
    MESSAGE_HANDLER_EX(WM_KEYUP, OnKeyEvent)
    MESSAGE_HANDLER_EX(WM_SYSKEYDOWN, OnKeyEvent)
    MESSAGE_HANDLER_EX(WM_SYSKEYUP, OnKeyEvent)
    MESSAGE_HANDLER_EX(WM_CHAR, OnKeyEvent)
    MESSAGE_HANDLER_EX(WM_SYSCHAR, OnKeyEvent)
    MESSAGE_HANDLER_EX(WM_IME_CHAR, OnKeyEvent)

    MSG_WM_CLOSE(OnClose)
    MSG_WM_PAINT(OnPaint)
    MSG_WM_SIZE(OnSize)
  END_MSG_MAP()

  void OnClose();
  LRESULT OnKeyEvent(UINT message, WPARAM w_param, LPARAM l_param);
  LRESULT OnMouseRange(UINT message, WPARAM w_param, LPARAM l_param);
  LRESULT OnCaptureChanged(UINT message, WPARAM w_param, LPARAM l_param);
  void OnPaint(HDC dc);
  void OnSize(UINT param, const CSize& size);

  RootWindowHostDelegate* delegate_;

  bool fullscreen_;
  bool has_capture_;
  RECT saved_window_rect_;
  DWORD saved_window_style_;
  DWORD saved_window_ex_style_;

  scoped_ptr<ui::ViewProp> prop_;

  DISALLOW_COPY_AND_ASSIGN(RootWindowHostWin);
};

}  // namespace aura

#endif  // UI_AURA_ROOT_WINDOW_HOST_WIN_H_
