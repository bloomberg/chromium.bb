// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/desktop_host_win.h"

#include <windows.h>

#include "base/message_loop.h"
#include "ui/aura/desktop.h"
#include "ui/aura/event.h"

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

DesktopHostWin::DesktopHostWin(const gfx::Rect& bounds) : desktop_(NULL) {
  Init(NULL, bounds);
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

gfx::Size DesktopHostWin::GetSize() const {
  RECT r;
  GetClientRect(hwnd(), &r);
  return gfx::Rect(r).size();
}

void DesktopHostWin::SetSize(const gfx::Size& size) {
  SetWindowPos(
      hwnd(),
      NULL,
      0,
      0,
      size.width(),
      size.height(),
      SWP_NOMOVE | SWP_NOOWNERZORDER | SWP_NOREDRAW | SWP_NOREPOSITION);
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
  return gfx::Point(pt);
}

void DesktopHostWin::OnClose() {
  // TODO: this obviously shouldn't be here.
  MessageLoopForUI::current()->Quit();
}

LRESULT DesktopHostWin::OnKeyEvent(UINT message,
                                   WPARAM w_param,
                                   LPARAM l_param) {
  MSG msg = { hwnd(), message, w_param, l_param };
  SetMsgHandled(desktop_->OnKeyEvent(KeyEvent(msg)));
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
    handled = desktop_->OnMouseEvent(event);
  SetMsgHandled(handled);
  return 0;
}

void DesktopHostWin::OnPaint(HDC dc) {
  desktop_->Draw();
  ValidateRect(hwnd(), NULL);
}

void DesktopHostWin::OnSize(UINT param, const CSize& size) {
  desktop_->OnHostResized(gfx::Size(size.cx, size.cy));
}

}  // namespace aura
