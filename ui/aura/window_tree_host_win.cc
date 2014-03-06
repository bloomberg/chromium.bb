// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/window_tree_host_win.h"

#include <windows.h>

#include <algorithm>

#include "base/message_loop/message_loop.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/cursor/cursor_loader_win.h"
#include "ui/base/view_prop.h"
#include "ui/compositor/compositor.h"
#include "ui/events/event.h"
#include "ui/gfx/display.h"
#include "ui/gfx/insets.h"
#include "ui/gfx/screen.h"

using std::max;
using std::min;

namespace aura {
namespace {

bool use_popup_as_root_window_for_test = false;

}  // namespace

// static
WindowTreeHost* WindowTreeHost::Create(const gfx::Rect& bounds) {
  return new WindowTreeHostWin(bounds);
}

// static
gfx::Size WindowTreeHost::GetNativeScreenSize() {
  return gfx::Size(GetSystemMetrics(SM_CXSCREEN),
                   GetSystemMetrics(SM_CYSCREEN));
}

WindowTreeHostWin::WindowTreeHostWin(const gfx::Rect& bounds)
    : fullscreen_(false),
      has_capture_(false),
      saved_window_style_(0),
      saved_window_ex_style_(0) {
  if (use_popup_as_root_window_for_test)
    set_window_style(WS_POPUP);
  Init(NULL, bounds);
  SetWindowText(hwnd(), L"aura::RootWindow!");
  CreateCompositor(GetAcceleratedWidget());
}

WindowTreeHostWin::~WindowTreeHostWin() {
  DestroyCompositor();
  DestroyDispatcher();
  DestroyWindow(hwnd());
}

gfx::AcceleratedWidget WindowTreeHostWin::GetAcceleratedWidget() {
  return hwnd();
}

void WindowTreeHostWin::Show() {
  ShowWindow(hwnd(), SW_SHOWNORMAL);
}

void WindowTreeHostWin::Hide() {
  NOTIMPLEMENTED();
}

void WindowTreeHostWin::ToggleFullScreen() {
  gfx::Rect target_rect;
  if (!fullscreen_) {
    fullscreen_ = true;
    saved_window_style_ = GetWindowLong(hwnd(), GWL_STYLE);
    saved_window_ex_style_ = GetWindowLong(hwnd(), GWL_EXSTYLE);
    GetWindowRect(hwnd(), &saved_window_rect_);
    SetWindowLong(hwnd(), GWL_STYLE,
                  saved_window_style_ & ~(WS_CAPTION | WS_THICKFRAME));
    SetWindowLong(hwnd(), GWL_EXSTYLE,
                  saved_window_ex_style_ & ~(WS_EX_DLGMODALFRAME |
                      WS_EX_WINDOWEDGE | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE));

    MONITORINFO mi;
    mi.cbSize = sizeof(mi);
    GetMonitorInfo(MonitorFromWindow(hwnd(), MONITOR_DEFAULTTONEAREST), &mi);
    target_rect = gfx::Rect(mi.rcMonitor);
  } else {
    fullscreen_ = false;
    SetWindowLong(hwnd(), GWL_STYLE, saved_window_style_);
    SetWindowLong(hwnd(), GWL_EXSTYLE, saved_window_ex_style_);
    target_rect = gfx::Rect(saved_window_rect_);
  }
  SetWindowPos(hwnd(),
               NULL,
               target_rect.x(),
               target_rect.y(),
               target_rect.width(),
               target_rect.height(),
               SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
}

gfx::Rect WindowTreeHostWin::GetBounds() const {
  RECT r;
  GetClientRect(hwnd(), &r);
  return gfx::Rect(r);
}

void WindowTreeHostWin::SetBounds(const gfx::Rect& bounds) {
  if (fullscreen_) {
    saved_window_rect_.right = saved_window_rect_.left + bounds.width();
    saved_window_rect_.bottom = saved_window_rect_.top + bounds.height();
    return;
  }
  RECT window_rect;
  window_rect.left = bounds.x();
  window_rect.top = bounds.y();
  window_rect.right = bounds.right() ;
  window_rect.bottom = bounds.bottom();
  AdjustWindowRectEx(&window_rect,
                     GetWindowLong(hwnd(), GWL_STYLE),
                     FALSE,
                     GetWindowLong(hwnd(), GWL_EXSTYLE));
  SetWindowPos(
      hwnd(),
      NULL,
      window_rect.left,
      window_rect.top,
      window_rect.right - window_rect.left,
      window_rect.bottom - window_rect.top,
      SWP_NOMOVE | SWP_NOOWNERZORDER | SWP_NOREDRAW | SWP_NOREPOSITION);

  // Explicity call OnHostResized when the scale has changed because
  // the window size may not have changed.
  float current_scale = compositor()->device_scale_factor();
  float new_scale = gfx::Screen::GetScreenFor(window())->
      GetDisplayNearestWindow(window()).device_scale_factor();
  if (current_scale != new_scale)
    OnHostResized(bounds.size());
}

gfx::Insets WindowTreeHostWin::GetInsets() const {
  return gfx::Insets();
}

void WindowTreeHostWin::SetInsets(const gfx::Insets& insets) {
}

gfx::Point WindowTreeHostWin::GetLocationOnNativeScreen() const {
  RECT r;
  GetClientRect(hwnd(), &r);
  return gfx::Point(r.left, r.top);
}


void WindowTreeHostWin::SetCapture() {
  if (!has_capture_) {
    has_capture_ = true;
    ::SetCapture(hwnd());
  }
}

void WindowTreeHostWin::ReleaseCapture() {
  if (has_capture_) {
    has_capture_ = false;
    ::ReleaseCapture();
  }
}

bool WindowTreeHostWin::QueryMouseLocation(gfx::Point* location_return) {
  client::CursorClient* cursor_client = client::GetCursorClient(window());
  if (cursor_client && !cursor_client->IsMouseEventsEnabled()) {
    *location_return = gfx::Point(0, 0);
    return false;
  }

  POINT pt;
  GetCursorPos(&pt);
  ScreenToClient(hwnd(), &pt);
  const gfx::Size size = GetBounds().size();
  *location_return =
      gfx::Point(max(0, min(size.width(), static_cast<int>(pt.x))),
                 max(0, min(size.height(), static_cast<int>(pt.y))));
  return (pt.x >= 0 && static_cast<int>(pt.x) < size.width() &&
          pt.y >= 0 && static_cast<int>(pt.y) < size.height());
}

bool WindowTreeHostWin::ConfineCursorToRootWindow() {
  RECT window_rect;
  GetWindowRect(hwnd(), &window_rect);
  return ClipCursor(&window_rect) != 0;
}

void WindowTreeHostWin::UnConfineCursor() {
  ClipCursor(NULL);
}

void WindowTreeHostWin::SetCursorNative(gfx::NativeCursor native_cursor) {
  // Custom web cursors are handled directly.
  if (native_cursor == ui::kCursorCustom)
    return;

  ui::CursorLoaderWin cursor_loader;
  cursor_loader.SetPlatformCursor(&native_cursor);
  ::SetCursor(native_cursor.platform());
}

void WindowTreeHostWin::MoveCursorToNative(const gfx::Point& location) {
  // Deliberately not implemented.
}

void WindowTreeHostWin::OnCursorVisibilityChangedNative(bool show) {
  NOTIMPLEMENTED();
}

void WindowTreeHostWin::PostNativeEvent(const base::NativeEvent& native_event) {
  ::PostMessage(
      hwnd(), native_event.message, native_event.wParam, native_event.lParam);
}

void WindowTreeHostWin::OnDeviceScaleFactorChanged(
    float device_scale_factor) {
  NOTIMPLEMENTED();
}

void WindowTreeHostWin::PrepareForShutdown() {
  NOTIMPLEMENTED();
}

ui::EventProcessor* WindowTreeHostWin::GetEventProcessor() {
  return dispatcher();
}

void WindowTreeHostWin::OnClose() {
  // TODO: this obviously shouldn't be here.
  base::MessageLoopForUI::current()->Quit();
}

LRESULT WindowTreeHostWin::OnKeyEvent(UINT message,
                                      WPARAM w_param,
                                      LPARAM l_param) {
  MSG msg = { hwnd(), message, w_param, l_param };
  ui::KeyEvent keyev(msg, message == WM_CHAR);
  ui::EventDispatchDetails details = SendEventToProcessor(&keyev);
  SetMsgHandled(keyev.handled() || details.dispatcher_destroyed);
  return 0;
}

LRESULT WindowTreeHostWin::OnMouseRange(UINT message,
                                        WPARAM w_param,
                                        LPARAM l_param) {
  MSG msg = { hwnd(), message, w_param, l_param, 0,
              { CR_GET_X_LPARAM(l_param), CR_GET_Y_LPARAM(l_param) } };
  ui::MouseEvent event(msg);
  bool handled = false;
  if (!(event.flags() & ui::EF_IS_NON_CLIENT)) {
    ui::EventDispatchDetails details = SendEventToProcessor(&event);
    handled = event.handled() || details.dispatcher_destroyed;
  }
  SetMsgHandled(handled);
  return 0;
}

LRESULT WindowTreeHostWin::OnCaptureChanged(UINT message,
                                            WPARAM w_param,
                                            LPARAM l_param) {
  if (has_capture_) {
    has_capture_ = false;
    OnHostLostWindowCapture();
  }
  return 0;
}

LRESULT WindowTreeHostWin::OnNCActivate(UINT message,
                                        WPARAM w_param,
                                        LPARAM l_param) {
  if (!!w_param)
    OnHostActivated();
  return DefWindowProc(hwnd(), message, w_param, l_param);
}

void WindowTreeHostWin::OnMove(const gfx::Point& point) {
  OnHostMoved(point);
}

void WindowTreeHostWin::OnPaint(HDC dc) {
  gfx::Rect damage_rect;
  RECT update_rect = {0};
  if (GetUpdateRect(hwnd(), &update_rect, FALSE))
    damage_rect = gfx::Rect(update_rect);
  compositor()->ScheduleRedrawRect(damage_rect);
  ValidateRect(hwnd(), NULL);
}

void WindowTreeHostWin::OnSize(UINT param, const gfx::Size& size) {
  // Minimizing resizes the window to 0x0 which causes our layout to go all
  // screwy, so we just ignore it.
  if (param != SIZE_MINIMIZED)
    OnHostResized(size);
}

namespace test {

// static
void SetUsePopupAsRootWindowForTest(bool use) {
  use_popup_as_root_window_for_test = use;
}

}  // namespace test

}  // namespace aura
