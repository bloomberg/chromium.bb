// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_WINDOW_NATIVE_WINDOW_WIN_H_
#define VIEWS_WINDOW_NATIVE_WINDOW_WIN_H_
#pragma once

#include "views/widget/native_widget_win.h"
#include "views/window/native_window.h"
#include "views/window/window.h"

namespace gfx {
class Font;
class Point;
class Size;
};

namespace views {
namespace internal {
class NativeWindowDelegate;

// This is exposed only for testing
// Adjusts the value of |child_rect| if necessary to ensure that it is
// completely visible within |parent_rect|.
void EnsureRectIsVisibleInRect(const gfx::Rect& parent_rect,
                               gfx::Rect* child_rect,
                               int padding);

}  // namespace internal

class Client;
class WindowDelegate;

////////////////////////////////////////////////////////////////////////////////
//
// NativeWindowWin
//
//  A NativeWindowWin is a NativeWidgetWin that encapsulates a window with a
//  frame. The frame may or may not be rendered by the operating system. The
//  window may or may not be top level.
//
////////////////////////////////////////////////////////////////////////////////
class NativeWindowWin : public NativeWidgetWin,
                        public NativeWindow {
 public:
  explicit NativeWindowWin(internal::NativeWindowDelegate* delegate);
  virtual ~NativeWindowWin();

  // Returns the system set window title font.
  static gfx::Font GetWindowTitleFont();

  // Overridden from NativeWindow:
  virtual Window* GetWindow() OVERRIDE;
  virtual const Window* GetWindow() const OVERRIDE;

 protected:
  friend Window;

  // Overridden from NativeWidgetWin:
  virtual void InitNativeWidget(const Widget::InitParams& params) OVERRIDE;
  virtual void OnDestroy() OVERRIDE;
  virtual LRESULT OnMouseRange(UINT message,
                               WPARAM w_param,
                               LPARAM l_param) OVERRIDE;
  LRESULT OnNCCalcSize(BOOL mode, LPARAM l_param);  // Don't override.
  virtual void OnNCPaint(HRGN rgn) OVERRIDE;
  virtual void OnWindowPosChanging(WINDOWPOS* window_pos) OVERRIDE;

  // Overridden from NativeWindow:
  virtual NativeWidget* AsNativeWidget() OVERRIDE;
  virtual const NativeWidget* AsNativeWidget() const OVERRIDE;
  virtual void BecomeModal() OVERRIDE;

 private:
  // If necessary, enables all ancestors.
  void RestoreEnabledIfNecessary();

  // Calculate the appropriate window styles for this window.
  DWORD CalculateWindowStyle();
  DWORD CalculateWindowExStyle();

  // Stops ignoring SetWindowPos() requests (see below).
  void StopIgnoringPosChanges() { ignore_window_pos_changes_ = false; }

  //  Update accessibility information via our WindowDelegate.
  void UpdateAccessibleName(std::wstring& accessible_name);
  void UpdateAccessibleRole();
  void UpdateAccessibleState();

  // A delegate implementation that handles events received here.
  internal::NativeWindowDelegate* delegate_;

  // Whether all ancestors have been enabled. This is only used if is_modal_ is
  // true.
  bool restored_enabled_;

  // When true, this flag makes us discard incoming SetWindowPos() requests that
  // only change our position/size.  (We still allow changes to Z-order,
  // activation, etc.)
  bool ignore_window_pos_changes_;

  // The following factory is used to ignore SetWindowPos() calls for short time
  // periods.
  ScopedRunnableMethodFactory<NativeWindowWin> ignore_pos_changes_factory_;

  // Set to true when the user presses the right mouse button on the caption
  // area. We need this so we can correctly show the context menu on mouse-up.
  bool is_right_mouse_pressed_on_caption_;

  // The last-seen monitor containing us, and its rect and work area.  These are
  // used to catch updates to the rect and work area and react accordingly.
  HMONITOR last_monitor_;
  gfx::Rect last_monitor_rect_, last_work_area_;

  DISALLOW_COPY_AND_ASSIGN(NativeWindowWin);
};

}  // namespace views

#endif  // VIEWS_WINDOW_NATIVE_WINDOW_WIN_H_
