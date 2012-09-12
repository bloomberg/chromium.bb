// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_DESKTOP_ROOT_WINDOW_HOST_WIN_H_
#define UI_VIEWS_WIDGET_DESKTOP_ROOT_WINDOW_HOST_WIN_H_

#include "ui/aura/root_window_host.h"
#include "ui/views/widget/desktop_root_window_host.h"
#include "ui/views/win/hwnd_message_handler_delegate.h"

namespace aura {
class DesktopActivationClient;
class DesktopDispatcherClient;
class FocusManager;
}

namespace views {
class DesktopCaptureClient;
class HWNDMessageHandler;

class DesktopRootWindowHostWin : public DesktopRootWindowHost,
                                 public aura::RootWindowHost,
                                 public HWNDMessageHandlerDelegate {
 public:
  DesktopRootWindowHostWin(
      internal::NativeWidgetDelegate* native_widget_delegate,
      const gfx::Rect& initial_bounds);
  virtual ~DesktopRootWindowHostWin();

 private:
  // Overridden from DesktopRootWindowHost:
  virtual void Init(aura::Window* content_window,
                    const Widget::InitParams& params) OVERRIDE;
  virtual void Close() OVERRIDE;
  virtual void CloseNow() OVERRIDE;
  virtual aura::RootWindowHost* AsRootWindowHost() OVERRIDE;
  virtual void ShowWindowWithState(ui::WindowShowState show_state) OVERRIDE;
  virtual bool IsVisible() const OVERRIDE;
  virtual gfx::Rect GetClientAreaBoundsInScreen() const OVERRIDE;

  // Overridden from aura::RootWindowHost:
  virtual aura::RootWindow* GetRootWindow() OVERRIDE;
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

  // Overridden from HWNDMessageHandlerDelegate:
  virtual bool IsWidgetWindow() const OVERRIDE;
  virtual bool IsUsingCustomFrame() const OVERRIDE;
  virtual void SchedulePaint() OVERRIDE;
  virtual void EnableInactiveRendering() OVERRIDE;
  virtual bool IsInactiveRenderingDisabled() OVERRIDE;
  virtual bool CanResize() const OVERRIDE;
  virtual bool CanMaximize() const OVERRIDE;
  virtual bool CanActivate() const OVERRIDE;
  virtual bool WidgetSizeIsClientSize() const OVERRIDE;
  virtual bool CanSaveFocus() const OVERRIDE;
  virtual void SaveFocusOnDeactivate() OVERRIDE;
  virtual void RestoreFocusOnActivate() OVERRIDE;
  virtual void RestoreFocusOnEnable() OVERRIDE;
  virtual bool IsModal() const OVERRIDE;
  virtual int GetInitialShowState() const OVERRIDE;
  virtual bool WillProcessWorkAreaChange() const OVERRIDE;
  virtual int GetNonClientComponent(const gfx::Point& point) const OVERRIDE;
  virtual void GetWindowMask(const gfx::Size& size, gfx::Path* path) OVERRIDE;
  virtual bool GetClientAreaInsets(gfx::Insets* insets) const OVERRIDE;
  virtual void GetMinMaxSize(gfx::Size* min_size,
                             gfx::Size* max_size) const OVERRIDE;
  virtual gfx::Size GetRootViewSize() const OVERRIDE;
  virtual void ResetWindowControls() OVERRIDE;
  virtual void PaintLayeredWindow(gfx::Canvas* canvas) OVERRIDE;
  virtual gfx::NativeViewAccessible GetNativeViewAccessible() OVERRIDE;
  virtual InputMethod* GetInputMethod() OVERRIDE;
  virtual void HandleAppDeactivated() OVERRIDE;
  virtual void HandleActivationChanged(bool active) OVERRIDE;
  virtual bool HandleAppCommand(short command) OVERRIDE;
  virtual void HandleCaptureLost() OVERRIDE;
  virtual void HandleClose() OVERRIDE;
  virtual bool HandleCommand(int command) OVERRIDE;
  virtual void HandleAccelerator(const ui::Accelerator& accelerator) OVERRIDE;
  virtual void HandleCreate() OVERRIDE;
  virtual void HandleDestroying() OVERRIDE;
  virtual void HandleDestroyed() OVERRIDE;
  virtual bool HandleInitialFocus() OVERRIDE;
  virtual void HandleDisplayChange() OVERRIDE;
  virtual void HandleBeginWMSizeMove() OVERRIDE;
  virtual void HandleEndWMSizeMove() OVERRIDE;
  virtual void HandleMove() OVERRIDE;
  virtual void HandleWorkAreaChanged() OVERRIDE;
  virtual void HandleVisibilityChanged(bool visible) OVERRIDE;
  virtual void HandleClientSizeChanged(const gfx::Size& new_size) OVERRIDE;
  virtual void HandleFrameChanged() OVERRIDE;
  virtual void HandleNativeFocus(HWND last_focused_window) OVERRIDE;
  virtual void HandleNativeBlur(HWND focused_window) OVERRIDE;
  virtual bool HandleMouseEvent(const ui::MouseEvent& event) OVERRIDE;
  virtual bool HandleKeyEvent(const ui::KeyEvent& event) OVERRIDE;
  virtual bool HandleUntranslatedKeyEvent(const ui::KeyEvent& event) OVERRIDE;
  virtual bool HandleIMEMessage(UINT message,
                                WPARAM w_param,
                                LPARAM l_param,
                                LRESULT* result) OVERRIDE;
  virtual void HandleInputLanguageChange(DWORD character_set,
                                         HKL input_language_id) OVERRIDE;
  virtual bool HandlePaintAccelerated(const gfx::Rect& invalid_rect) OVERRIDE;
  virtual void HandlePaint(gfx::Canvas* canvas) OVERRIDE;
  virtual void HandleScreenReaderDetected() OVERRIDE;
  virtual bool HandleTooltipNotify(int w_param,
                                   NMHDR* l_param,
                                   LRESULT* l_result) OVERRIDE;
  virtual void HandleTooltipMouseMove(UINT message,
                                      WPARAM w_param,
                                      LPARAM l_param) OVERRIDE;
  virtual bool PreHandleMSG(UINT message,
                            WPARAM w_param,
                            LPARAM l_param,
                            LRESULT* result) OVERRIDE;
  virtual void PostHandleMSG(UINT message,
                             WPARAM w_param,
                             LPARAM l_param) OVERRIDE;

  scoped_ptr<aura::RootWindow> root_window_;
  scoped_ptr<HWNDMessageHandler> message_handler_;
  scoped_ptr<DesktopCaptureClient> capture_client_;
  scoped_ptr<aura::DesktopActivationClient> activation_client_;
  scoped_ptr<aura::DesktopDispatcherClient> dispatcher_client_;
  scoped_ptr<aura::FocusManager> focus_manager_;

  // TODO(beng): Consider providing an interface to DesktopNativeWidgetAura
  //             instead of providing this route back to Widget.
  internal::NativeWidgetDelegate* native_widget_delegate_;

  aura::RootWindowHostDelegate* root_window_host_delegate_;
  aura::Window* content_window_;

  DISALLOW_COPY_AND_ASSIGN(DesktopRootWindowHostWin);
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_DESKTOP_ROOT_WINDOW_HOST_WIN_H_
