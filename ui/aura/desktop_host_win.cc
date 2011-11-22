// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/desktop_host_win.h"

#include <windows.h>

#include <algorithm>

#include "base/message_loop.h"
#include "ui/aura/desktop.h"
#include "ui/aura/event.h"

using std::max;
using std::min;

namespace aura {

namespace {

const wchar_t* GetCursorId(gfx::NativeCursor native_cursor) {
  switch (native_cursor) {
    case kCursorNull:
      return IDC_ARROW;
    case kCursorPointer:
      return IDC_ARROW;
    case kCursorCross:
      return IDC_CROSS;
    case kCursorHand:
      return IDC_HAND;
    case kCursorIBeam:
      return IDC_IBEAM;
    case kCursorWait:
      return IDC_WAIT;
    case kCursorHelp:
      return IDC_HELP;
    case kCursorEastResize:
      return IDC_SIZEWE;
    case kCursorNorthResize:
      return IDC_SIZENS;
    case kCursorNorthEastResize:
      return IDC_SIZENESW;
    case kCursorNorthWestResize:
      return IDC_SIZENWSE;
    case kCursorSouthResize:
      return IDC_SIZENS;
    case kCursorSouthEastResize:
      return IDC_SIZENWSE;
    case kCursorSouthWestResize:
      return IDC_SIZENESW;
    case kCursorWestResize:
      return IDC_SIZEWE;
    case kCursorNorthSouthResize:
      return IDC_SIZENS;
    case kCursorEastWestResize:
      return IDC_SIZEWE;
    case kCursorNorthEastSouthWestResize:
      return IDC_SIZENESW;
    case kCursorNorthWestSouthEastResize:
      return IDC_SIZENWSE;
    case kCursorMove:
      return IDC_SIZEALL;
    case kCursorProgress:
      return IDC_APPSTARTING;
    case kCursorNoDrop:
      return IDC_NO;
    case kCursorNotAllowed:
      return IDC_NO;
    case kCursorColumnResize:
    case kCursorRowResize:
    case kCursorMiddlePanning:
    case kCursorEastPanning:
    case kCursorNorthPanning:
    case kCursorNorthEastPanning:
    case kCursorNorthWestPanning:
    case kCursorSouthPanning:
    case kCursorSouthEastPanning:
    case kCursorSouthWestPanning:
    case kCursorWestPanning:
    case kCursorVerticalText:
    case kCursorCell:
    case kCursorContextMenu:
    case kCursorAlias:
    case kCursorCopy:
    case kCursorNone:
    case kCursorZoomIn:
    case kCursorZoomOut:
    case kCursorGrab:
    case kCursorGrabbing:
    case kCursorCustom:
      // TODO(jamescook): Should we use WebKit glue resources for these?
      // Or migrate those resources to someplace ui/aura can share?
      NOTIMPLEMENTED();
      return IDC_ARROW;
    default:
      NOTREACHED();
      return IDC_ARROW;
  }
}

}  // namespace

// static
DesktopHost* DesktopHost::Create(const gfx::Rect& bounds) {
  return new DesktopHostWin(bounds);
}

// static
gfx::Size DesktopHost::GetNativeDisplaySize() {
  return gfx::Size(GetSystemMetrics(SM_CXSCREEN),
                   GetSystemMetrics(SM_CYSCREEN));
}

DesktopHostWin::DesktopHostWin(const gfx::Rect& bounds)
    : desktop_(NULL),
      fullscreen_(false),
      saved_window_style_(0),
      saved_window_ex_style_(0) {
  Init(NULL, bounds);
  SetWindowText(hwnd(), L"aura::Desktop!");
}

DesktopHostWin::~DesktopHostWin() {
  DestroyWindow(hwnd());
}

bool DesktopHostWin::Dispatch(const MSG& msg) {
  TranslateMessage(&msg);
  DispatchMessage(&msg);
  return true;
}

void DesktopHostWin::SetDesktop(Desktop* desktop) {
  desktop_ = desktop;
}

gfx::AcceleratedWidget DesktopHostWin::GetAcceleratedWidget() {
  return hwnd();
}

void DesktopHostWin::Show() {
  ShowWindow(hwnd(), SW_SHOWNORMAL);
}

void DesktopHostWin::ToggleFullScreen() {
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
    target_rect = mi.rcMonitor;
  } else {
    fullscreen_ = false;
    SetWindowLong(hwnd(), GWL_STYLE, saved_window_style_);
    SetWindowLong(hwnd(), GWL_EXSTYLE, saved_window_ex_style_);
    target_rect = saved_window_rect_;
  }
  SetWindowPos(hwnd(),
               NULL,
               target_rect.x(),
               target_rect.y(),
               target_rect.width(),
               target_rect.height(),
               SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
}

gfx::Size DesktopHostWin::GetSize() const {
  RECT r;
  GetClientRect(hwnd(), &r);
  return gfx::Rect(r).size();
}

void DesktopHostWin::SetSize(const gfx::Size& size) {
  if (fullscreen_) {
    saved_window_rect_.right = saved_window_rect_.left + size.width();
    saved_window_rect_.bottom = saved_window_rect_.top + size.height();
    return;
  }
  RECT window_rect;
  window_rect.left = 0;
  window_rect.top = 0;
  window_rect.right = size.width();
  window_rect.bottom = size.height();
  AdjustWindowRectEx(&window_rect,
                     GetWindowLong(hwnd(), GWL_STYLE),
                     FALSE,
                     GetWindowLong(hwnd(), GWL_EXSTYLE));
  SetWindowPos(
      hwnd(),
      NULL,
      0,
      0,
      window_rect.right - window_rect.left,
      window_rect.bottom - window_rect.top,
      SWP_NOMOVE | SWP_NOOWNERZORDER | SWP_NOREDRAW | SWP_NOREPOSITION);
}

gfx::Point DesktopHostWin::GetLocationOnNativeScreen() const {
  RECT r;
  GetClientRect(hwnd(), &r);
  return gfx::Point(r.left, r.top);
}


void DesktopHostWin::SetCursor(gfx::NativeCursor native_cursor) {
  // Custom web cursors are handled directly.
  if (native_cursor == kCursorCustom)
    return;
  const wchar_t* cursor_id = GetCursorId(native_cursor);
  // TODO(jamescook): Support for non-system cursors will require finding
  // the appropriate module to pass to LoadCursor().
  ::SetCursor(LoadCursor(NULL, cursor_id));
}

gfx::Point DesktopHostWin::QueryMouseLocation() {
  POINT pt;
  GetCursorPos(&pt);
  ScreenToClient(hwnd(), &pt);
  const gfx::Size size = GetSize();
  return gfx::Point(max(0, min(size.width(), static_cast<int>(pt.x))),
                    max(0, min(size.height(), static_cast<int>(pt.y))));
}

void DesktopHostWin::PostNativeEvent(const base::NativeEvent& native_event) {
  ::PostMessage(
      hwnd(), native_event.message, native_event.wParam, native_event.lParam);
}

void DesktopHostWin::OnClose() {
  // TODO: this obviously shouldn't be here.
  MessageLoopForUI::current()->Quit();
}

LRESULT DesktopHostWin::OnKeyEvent(UINT message,
                                   WPARAM w_param,
                                   LPARAM l_param) {
  MSG msg = { hwnd(), message, w_param, l_param };
  KeyEvent keyev(msg, message == WM_CHAR);
  SetMsgHandled(desktop_->DispatchKeyEvent(&keyev));
  return 0;
}

LRESULT DesktopHostWin::OnMouseRange(UINT message,
                                     WPARAM w_param,
                                     LPARAM l_param) {
  MSG msg = { hwnd(), message, w_param, l_param, 0,
              { GET_X_LPARAM(l_param), GET_Y_LPARAM(l_param) } };
  MouseEvent event(msg);
  bool handled = false;
  if (!(event.flags() & ui::EF_IS_NON_CLIENT))
    handled = desktop_->DispatchMouseEvent(&event);
  SetMsgHandled(handled);
  return 0;
}

void DesktopHostWin::OnPaint(HDC dc) {
  desktop_->Draw();
  ValidateRect(hwnd(), NULL);
}

void DesktopHostWin::OnSize(UINT param, const CSize& size) {
  // Minimizing resizes the window to 0x0 which causes our layout to go all
  // screwy, so we just ignore it.
  if (param != SIZE_MINIMIZED)
    desktop_->OnHostResized(gfx::Size(size.cx, size.cy));
}

}  // namespace aura
