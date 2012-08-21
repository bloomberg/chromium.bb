// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIN_HWND_MESSAGE_HANDLER_DELEGATE_H_
#define UI_VIEWS_WIN_HWND_MESSAGE_HANDLER_DELEGATE_H_

#include "ui/views/views_export.h"

namespace gfx {
class Path;
class Point;
class Size;
}

namespace views {

class InputMethod;
class NativeWidgetWin;

// Implemented by the object that uses the HWNDMessageHandler to handle
// notifications from the underlying HWND and service requests for data.
class VIEWS_EXPORT HWNDMessageHandlerDelegate {
 public:
  virtual bool IsWidgetWindow() const = 0;

  // TODO(beng): resolve this more satisfactorily vis-a-vis ShouldUseNativeFrame
  //             to avoid confusion.
  virtual bool IsUsingCustomFrame() const = 0;

  virtual void SchedulePaint() = 0;
  virtual void EnableInactiveRendering() = 0;
  virtual bool IsInactiveRenderingDisabled() = 0;

  virtual bool CanResize() const = 0;
  virtual bool CanMaximize() const = 0;
  virtual bool CanActivate() const = 0;

  virtual bool WillProcessWorkAreaChange() const = 0;

  virtual int GetNonClientComponent(const gfx::Point& point) const = 0;
  virtual void GetWindowMask(const gfx::Size& size, gfx::Path* mask) = 0;

  // Returns the minimum and maximum size the window can be resized to by the
  // user.
  virtual void GetMinMaxSize(gfx::Size* min_size,
                             gfx::Size* max_size) const = 0;

  virtual InputMethod* GetInputMethod() = 0;

  virtual gfx::NativeViewAccessible GetNativeViewAccessible() = 0;

  // TODO(beng): Investigate migrating these methods to On* prefixes once
  // HWNDMessageHandler is the WindowImpl.

  // Called when another app was activated.
  virtual void HandleAppDeactivated() = 0;

  // Called when the window was activated or deactivated. |active| reflects the
  // new state.
  virtual void HandleActivationChanged(bool active) = 0;

  // Called when a well known "app command" from the system was performed.
  // Returns true if the command was handled.
  virtual bool HandleAppCommand(short command) = 0;

  // Called when the window has lost mouse capture.
  virtual void HandleCaptureLost() = 0;

  // Called when the user tried to close the window.
  virtual void HandleClose() = 0;

  // Called when a command defined by the application was performed. Returns
  // true if the command was handled.
  virtual bool HandleCommand(int command) = 0;

  // Called when the HWND is created.
  virtual void HandleCreate() = 0;

  // Called when the HWND is destroyed.
  virtual void HandleDestroy() = 0;

  // Called when display settings are adjusted on the system.
  virtual void HandleDisplayChange() = 0;

  // Called when the system changes from glass to non-glass or vice versa.
  virtual void HandleGlassModeChange() = 0;

  // Called when the user begins or ends a size/move operation using the window
  // manager.
  virtual void HandleBeginWMSizeMove() = 0;
  virtual void HandleEndWMSizeMove() = 0;

  // Called when the window's position changed.
  virtual void HandleMove() = 0;

  // Called when the system's work area has changed.
  virtual void HandleWorkAreaChanged() = 0;

  // Called when focus shifted to this HWND from |last_focused_window|.
  virtual void HandleNativeFocus(HWND last_focused_window) = 0;

  // Called when focus shifted from the HWND to a different window.
  virtual void HandleNativeBlur(HWND focused_window) = 0;

  // Called when we have detected a screen reader.
  virtual void HandleScreenReaderDetected() = 0;

  // Called to forward a WM_NOTIFY message to the tooltip manager.
  virtual bool HandleTooltipNotify(int w_param,
                                   NMHDR* l_param,
                                   LRESULT* l_result) = 0;

  // This is provided for methods that need to call private methods on NWW.
  // TODO(beng): should be removed once HWNDMessageHandler is the WindowImpl.
  virtual NativeWidgetWin* AsNativeWidgetWin() = 0;

 protected:
  virtual ~HWNDMessageHandlerDelegate() {}
};

}  // namespace views

#endif  // UI_VIEWS_WIN_HWND_MESSAGE_HANDLER_DELEGATE_H_
