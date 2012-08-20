// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/hwnd_message_handler.h"

#include <dwmapi.h>

#include "base/system_monitor/system_monitor.h"
#include "ui/gfx/path.h"
#include "ui/base/native_theme/native_theme_win.h"
#include "ui/views/ime/input_method_win.h"
#include "ui/views/widget/hwnd_message_handler_delegate.h"
#include "ui/views/widget/native_widget_win.h"

namespace views {

////////////////////////////////////////////////////////////////////////////////
// HWNDMessageHandler, public:

HWNDMessageHandler::HWNDMessageHandler(HWNDMessageHandlerDelegate* delegate)
    : delegate_(delegate),
      remove_standard_frame_(false) {
}

HWNDMessageHandler::~HWNDMessageHandler() {
}

void HWNDMessageHandler::OnActivate(UINT action, BOOL minimized, HWND window) {
  SetMsgHandled(FALSE);
}

void HWNDMessageHandler::OnActivateApp(BOOL active, DWORD thread_id) {
  if (delegate_->IsWidgetWindow() && !active &&
      thread_id != GetCurrentThreadId()) {
    delegate_->HandleAppDeactivated();
    // Also update the native frame if it is rendering the non-client area.
    if (!remove_standard_frame_ &&
        !delegate_->IsUsingCustomFrame() &&
        delegate_->AsNativeWidgetWin()) {
      delegate_->AsNativeWidgetWin()->DefWindowProcWithRedrawLock(
          WM_NCACTIVATE, FALSE, 0);
    }
  }
}

BOOL HWNDMessageHandler::OnAppCommand(HWND window,
                                      short command,
                                      WORD device,
                                      int keystate) {
  BOOL handled = !!delegate_->HandleAppCommand(command);
  SetMsgHandled(handled);
  // Make sure to return TRUE if the event was handled or in some cases the
  // system will execute the default handler which can cause bugs like going
  // forward or back two pages instead of one.
  return handled;
}

void HWNDMessageHandler::OnCancelMode() {
  SetMsgHandled(FALSE);
}

void HWNDMessageHandler::OnCaptureChanged(HWND window) {
  delegate_->HandleCaptureLost();
}

void HWNDMessageHandler::OnClose() {
  delegate_->HandleClose();
}

void HWNDMessageHandler::OnCommand(UINT notification_code,
                                   int command,
                                   HWND window) {
  // If the notification code is > 1 it means it is control specific and we
  // should ignore it.
  if (notification_code > 1 || delegate_->HandleAppCommand(command))
    SetMsgHandled(FALSE);
}

void HWNDMessageHandler::OnDestroy() {
  delegate_->HandleDestroy();
}

void HWNDMessageHandler::OnDisplayChange(UINT bits_per_pixel,
                                         const CSize& screen_size) {
  delegate_->HandleDisplayChange();
}

void HWNDMessageHandler::OnDwmCompositionChanged(UINT msg,
                                                 WPARAM w_param,
                                                 LPARAM l_param) {
  if (!delegate_->IsWidgetWindow()) {
    SetMsgHandled(FALSE);
    return;
  }
  delegate_->HandleGlassModeChange();
}

void HWNDMessageHandler::OnEndSession(BOOL ending, UINT logoff) {
  SetMsgHandled(FALSE);
}

void HWNDMessageHandler::OnEnterSizeMove() {
  delegate_->HandleBeginWMSizeMove();
  SetMsgHandled(FALSE);
}

LRESULT HWNDMessageHandler::OnEraseBkgnd(HDC dc) {
  // Needed to prevent resize flicker.
  return 1;
}

void HWNDMessageHandler::OnExitMenuLoop(BOOL is_track_popup_menu) {
  SetMsgHandled(FALSE);
}

void HWNDMessageHandler::OnExitSizeMove() {
  delegate_->HandleEndWMSizeMove();
  SetMsgHandled(FALSE);
}

LRESULT HWNDMessageHandler::OnImeMessages(UINT message,
                                          WPARAM w_param,
                                          LPARAM l_param) {
  InputMethod* input_method = delegate_->GetInputMethod();
  if (!input_method || input_method->IsMock()) {
    SetMsgHandled(FALSE);
    return 0;
  }

  InputMethodWin* ime_win = static_cast<InputMethodWin*>(input_method);
  BOOL handled = FALSE;
  LRESULT result = ime_win->OnImeMessages(message, w_param, l_param, &handled);
  SetMsgHandled(handled);
  return result;
}

void HWNDMessageHandler::OnInitMenu(HMENU menu) {
  bool is_fullscreen = delegate_->AsNativeWidgetWin()->IsFullscreen();
  bool is_minimized = delegate_->AsNativeWidgetWin()->IsMinimized();
  bool is_maximized = delegate_->AsNativeWidgetWin()->IsMaximized();
  bool is_restored = !is_fullscreen && !is_minimized && !is_maximized;

  EnableMenuItem(menu, SC_RESTORE, is_minimized || is_maximized);
  EnableMenuItem(menu, SC_MOVE, is_restored);
  EnableMenuItem(menu, SC_SIZE, delegate_->CanResize() && is_restored);
  EnableMenuItem(menu, SC_MAXIMIZE, delegate_->CanMaximize() &&
                 !is_fullscreen && !is_maximized);
  EnableMenuItem(menu, SC_MINIMIZE, delegate_->CanMaximize() && !is_minimized);
}

void HWNDMessageHandler::OnInitMenuPopup() {
  SetMsgHandled(FALSE);
}

void HWNDMessageHandler::OnInputLangChange(DWORD character_set,
                                           HKL input_language_id) {
  InputMethod* input_method = delegate_->GetInputMethod();
  if (input_method && !input_method->IsMock()) {
    static_cast<InputMethodWin*>(input_method)->OnInputLangChange(
        character_set, input_language_id);
  }
}

LRESULT HWNDMessageHandler::OnKeyEvent(UINT message,
                                       WPARAM w_param,
                                       LPARAM l_param) {
  MSG msg = { hwnd(), message, w_param, l_param };
  ui::KeyEvent key(msg, message == WM_CHAR);
  InputMethod* input_method = delegate_->GetInputMethod();
  if (input_method)
    input_method->DispatchKeyEvent(key);
  else
    delegate_->AsNativeWidgetWin()->DispatchKeyEventPostIME(key);
  return 0;
}

void HWNDMessageHandler::OnKillFocus(HWND focused_window) {
  delegate_->HandleNativeBlur(focused_window);

  InputMethod* input_method = delegate_->GetInputMethod();
  if (input_method)
    input_method->OnBlur();
  SetMsgHandled(FALSE);
}

void HWNDMessageHandler::OnMove(const CPoint& point) {
  delegate_->HandleMove();
  SetMsgHandled(FALSE);
}

void HWNDMessageHandler::OnMoving(UINT param, const RECT* new_bounds) {
  delegate_->HandleMove();
}

LRESULT HWNDMessageHandler::OnNCHitTest(const CPoint& point) {
  if (!delegate_->IsWidgetWindow()) {
    SetMsgHandled(FALSE);
    return 0;
  }

  // If the DWM is rendering the window controls, we need to give the DWM's
  // default window procedure first chance to handle hit testing.
  if (!remove_standard_frame_ && !delegate_->IsUsingCustomFrame()) {
    LRESULT result;
    if (DwmDefWindowProc(hwnd(), WM_NCHITTEST, 0,
                         MAKELPARAM(point.x, point.y), &result)) {
      return result;
    }
  }

  // First, give the NonClientView a chance to test the point to see if it
  // provides any of the non-client area.
  POINT temp = point;
  MapWindowPoints(HWND_DESKTOP, hwnd(), &temp, 1);
  int component = delegate_->GetNonClientComponent(gfx::Point(temp));
  if (component != HTNOWHERE)
    return component;

  // Otherwise, we let Windows do all the native frame non-client handling for
  // us.
  SetMsgHandled(FALSE);
  return 0;
}

LRESULT HWNDMessageHandler::OnNCUAHDrawCaption(UINT message,
                                               WPARAM w_param,
                                               LPARAM l_param) {
  // See comment in widget_win.h at the definition of WM_NCUAHDRAWCAPTION for
  // an explanation about why we need to handle this message.
  SetMsgHandled(delegate_->IsUsingCustomFrame());
  return 0;
}

LRESULT HWNDMessageHandler::OnNCUAHDrawFrame(UINT message,
                                             WPARAM w_param,
                                             LPARAM l_param) {
  // See comment in widget_win.h at the definition of WM_NCUAHDRAWCAPTION for
  // an explanation about why we need to handle this message.
  SetMsgHandled(delegate_->IsUsingCustomFrame());
  return 0;
}

LRESULT HWNDMessageHandler::OnPowerBroadcast(DWORD power_event, DWORD data) {
  base::SystemMonitor* monitor = base::SystemMonitor::Get();
  if (monitor)
    monitor->ProcessWmPowerBroadcastMessage(power_event);
  SetMsgHandled(FALSE);
  return 0;
}

LRESULT HWNDMessageHandler::OnSetCursor(UINT message,
                                        WPARAM w_param,
                                        LPARAM l_param) {
  // Using ScopedRedrawLock here frequently allows content behind this window to
  // paint in front of this window, causing glaring rendering artifacts.
  // If omitting ScopedRedrawLock here triggers caption rendering artifacts via
  // DefWindowProc message handling, we'll need to find a better solution.
  SetMsgHandled(FALSE);
  return 0;
}

void HWNDMessageHandler::OnSetFocus(HWND last_focused_window) {
  delegate_->HandleNativeFocus(last_focused_window);
  InputMethod* input_method = delegate_->GetInputMethod();
  if (input_method)
    input_method->OnFocus();
  SetMsgHandled(FALSE);
}

LRESULT HWNDMessageHandler::OnSetIcon(UINT size_type, HICON new_icon) {
  // Use a ScopedRedrawLock to avoid weird non-client painting.
  return DefWindowProcWithRedrawLock(WM_SETICON, size_type,
                                     reinterpret_cast<LPARAM>(new_icon));
}

LRESULT HWNDMessageHandler::OnSetText(const wchar_t* text) {
  // Use a ScopedRedrawLock to avoid weird non-client painting.
  return DefWindowProcWithRedrawLock(WM_SETTEXT, NULL,
                                     reinterpret_cast<LPARAM>(text));
}

void HWNDMessageHandler::OnThemeChanged() {
  ui::NativeThemeWin::instance()->CloseHandles();
}

void HWNDMessageHandler::OnVScroll(int scroll_type,
                                   short position,
                                   HWND scrollbar) {
  SetMsgHandled(FALSE);
}

////////////////////////////////////////////////////////////////////////////////
// HWNDMessageHandler, private:

HWND HWNDMessageHandler::hwnd() {
  return delegate_->AsNativeWidgetWin()->hwnd();
}

LRESULT HWNDMessageHandler::DefWindowProcWithRedrawLock(UINT message,
                                                        WPARAM w_param,
                                                        LPARAM l_param) {
  return delegate_->AsNativeWidgetWin()->DefWindowProcWithRedrawLock(message,
                                                                     w_param,
                                                                     l_param);
}

void HWNDMessageHandler::SetMsgHandled(BOOL handled) {
  delegate_->AsNativeWidgetWin()->SetMsgHandled(handled);
}

}  // namespace views

