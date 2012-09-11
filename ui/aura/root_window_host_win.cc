// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/root_window_host_win.h"

#include <windows.h>

#include <algorithm>

#include "base/message_loop.h"
#include "ui/aura/client/capture_client.h"
#include "ui/aura/env.h"
#include "ui/aura/root_window.h"
#include "ui/base/events/event.h"
#include "ui/base/view_prop.h"

using std::max;
using std::min;

namespace aura {

namespace {

const char* kRootWindowHostWinKey = "__AURA_ROOT_WINDOW_HOST_WIN__";

// TODO(mazda): Move the cursor code to ui/base/cursor/cursor_loader_win.{cc,h}.
const wchar_t* GetCursorId(gfx::NativeCursor native_cursor) {
  switch (native_cursor.native_type()) {
    case ui::kCursorNull:
      return IDC_ARROW;
    case ui::kCursorPointer:
      return IDC_ARROW;
    case ui::kCursorCross:
      return IDC_CROSS;
    case ui::kCursorHand:
      return IDC_HAND;
    case ui::kCursorIBeam:
      return IDC_IBEAM;
    case ui::kCursorWait:
      return IDC_WAIT;
    case ui::kCursorHelp:
      return IDC_HELP;
    case ui::kCursorEastResize:
      return IDC_SIZEWE;
    case ui::kCursorNorthResize:
      return IDC_SIZENS;
    case ui::kCursorNorthEastResize:
      return IDC_SIZENESW;
    case ui::kCursorNorthWestResize:
      return IDC_SIZENWSE;
    case ui::kCursorSouthResize:
      return IDC_SIZENS;
    case ui::kCursorSouthEastResize:
      return IDC_SIZENWSE;
    case ui::kCursorSouthWestResize:
      return IDC_SIZENESW;
    case ui::kCursorWestResize:
      return IDC_SIZEWE;
    case ui::kCursorNorthSouthResize:
      return IDC_SIZENS;
    case ui::kCursorEastWestResize:
      return IDC_SIZEWE;
    case ui::kCursorNorthEastSouthWestResize:
      return IDC_SIZENESW;
    case ui::kCursorNorthWestSouthEastResize:
      return IDC_SIZENWSE;
    case ui::kCursorMove:
      return IDC_SIZEALL;
    case ui::kCursorProgress:
      return IDC_APPSTARTING;
    case ui::kCursorNoDrop:
      return IDC_NO;
    case ui::kCursorNotAllowed:
      return IDC_NO;
    case ui::kCursorColumnResize:
    case ui::kCursorRowResize:
    case ui::kCursorMiddlePanning:
    case ui::kCursorEastPanning:
    case ui::kCursorNorthPanning:
    case ui::kCursorNorthEastPanning:
    case ui::kCursorNorthWestPanning:
    case ui::kCursorSouthPanning:
    case ui::kCursorSouthEastPanning:
    case ui::kCursorSouthWestPanning:
    case ui::kCursorWestPanning:
    case ui::kCursorVerticalText:
    case ui::kCursorCell:
    case ui::kCursorContextMenu:
    case ui::kCursorAlias:
    case ui::kCursorCopy:
    case ui::kCursorNone:
    case ui::kCursorZoomIn:
    case ui::kCursorZoomOut:
    case ui::kCursorGrab:
    case ui::kCursorGrabbing:
    case ui::kCursorCustom:
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
RootWindowHost* RootWindowHost::Create(RootWindowHostDelegate* delegate,
                                       const gfx::Rect& bounds) {
  return new RootWindowHostWin(delegate, bounds);
}

// static
RootWindowHost* RootWindowHost::GetForAcceleratedWidget(
    gfx::AcceleratedWidget accelerated_widget) {
  return reinterpret_cast<RootWindowHost*>(
      ui::ViewProp::GetValue(accelerated_widget, kRootWindowHostWinKey));
}

// static
gfx::Size RootWindowHost::GetNativeScreenSize() {
  return gfx::Size(GetSystemMetrics(SM_CXSCREEN),
                   GetSystemMetrics(SM_CYSCREEN));
}

RootWindowHostWin::RootWindowHostWin(RootWindowHostDelegate* delegate,
                                     const gfx::Rect& bounds)
    : delegate_(delegate),
      fullscreen_(false),
      has_capture_(false),
      saved_window_style_(0),
      saved_window_ex_style_(0) {
  Init(NULL, bounds);
  SetWindowText(hwnd(), L"aura::RootWindow!");
  prop_.reset(new ui::ViewProp(hwnd(), kRootWindowHostWinKey, this));
}

RootWindowHostWin::~RootWindowHostWin() {
  DestroyWindow(hwnd());
}

RootWindow* RootWindowHostWin::GetRootWindow() {
  return delegate_->AsRootWindow();
}

gfx::AcceleratedWidget RootWindowHostWin::GetAcceleratedWidget() {
  return hwnd();
}

void RootWindowHostWin::Show() {
  ShowWindow(hwnd(), SW_SHOWNORMAL);
}

void RootWindowHostWin::Hide() {
  NOTIMPLEMENTED();
}

void RootWindowHostWin::ToggleFullScreen() {
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

gfx::Rect RootWindowHostWin::GetBounds() const {
  RECT r;
  GetClientRect(hwnd(), &r);
  return gfx::Rect(r);
}

void RootWindowHostWin::SetBounds(const gfx::Rect& bounds) {
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
      0,
      0,
      window_rect.right - window_rect.left,
      window_rect.bottom - window_rect.top,
      SWP_NOMOVE | SWP_NOOWNERZORDER | SWP_NOREDRAW | SWP_NOREPOSITION);
}

gfx::Point RootWindowHostWin::GetLocationOnNativeScreen() const {
  RECT r;
  GetClientRect(hwnd(), &r);
  return gfx::Point(r.left, r.top);
}


void RootWindowHostWin::SetCursor(gfx::NativeCursor native_cursor) {
  // Custom web cursors are handled directly.
  if (native_cursor == ui::kCursorCustom)
    return;
  const wchar_t* cursor_id = GetCursorId(native_cursor);
  // TODO(jamescook): Support for non-system cursors will require finding
  // the appropriate module to pass to LoadCursor().
  ::SetCursor(LoadCursor(NULL, cursor_id));
}

void RootWindowHostWin::SetCapture() {
  if (!has_capture_) {
    has_capture_ = true;
    ::SetCapture(hwnd());
  }
}

void RootWindowHostWin::ReleaseCapture() {
  if (has_capture_) {
    has_capture_ = false;
    ::ReleaseCapture();
  }
}

void RootWindowHostWin::ShowCursor(bool show) {
  // NOTIMPLEMENTED();
}

bool RootWindowHostWin::QueryMouseLocation(gfx::Point* location_return) {
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

bool RootWindowHostWin::ConfineCursorToRootWindow() {
  RECT window_rect;
  GetWindowRect(hwnd(), &window_rect);
  return ClipCursor(&window_rect) != 0;
}

bool RootWindowHostWin::GrabSnapshot(
    const gfx::Rect& snapshot_bounds,
    std::vector<unsigned char>* png_representation) {
  NOTIMPLEMENTED();
  return false;
}

void RootWindowHostWin::UnConfineCursor() {
  ClipCursor(NULL);
}

void RootWindowHostWin::MoveCursorTo(const gfx::Point& location) {
  POINT pt;
  ClientToScreen(hwnd(), &pt);
  SetCursorPos(pt.x, pt.y);
}

void RootWindowHostWin::SetFocusWhenShown(bool focus_when_shown) {
  NOTIMPLEMENTED();
}

void RootWindowHostWin::PostNativeEvent(const base::NativeEvent& native_event) {
  ::PostMessage(
      hwnd(), native_event.message, native_event.wParam, native_event.lParam);
}

void RootWindowHostWin::OnDeviceScaleFactorChanged(
    float device_scale_factor) {
  NOTIMPLEMENTED();
}

void RootWindowHostWin::PrepareForShutdown() {
  NOTIMPLEMENTED();
}

void RootWindowHostWin::OnClose() {
  // TODO: this obviously shouldn't be here.
  MessageLoopForUI::current()->Quit();
}

LRESULT RootWindowHostWin::OnKeyEvent(UINT message,
                                      WPARAM w_param,
                                      LPARAM l_param) {
  MSG msg = { hwnd(), message, w_param, l_param };
  ui::KeyEvent keyev(msg, message == WM_CHAR);
  SetMsgHandled(delegate_->OnHostKeyEvent(&keyev));
  return 0;
}

LRESULT RootWindowHostWin::OnMouseRange(UINT message,
                                        WPARAM w_param,
                                        LPARAM l_param) {
  MSG msg = { hwnd(), message, w_param, l_param, 0,
              { GET_X_LPARAM(l_param), GET_Y_LPARAM(l_param) } };
  ui::MouseEvent event(msg);
  bool handled = false;
  if (!(event.flags() & ui::EF_IS_NON_CLIENT))
    handled = delegate_->OnHostMouseEvent(&event);
  SetMsgHandled(handled);
  return 0;
}

LRESULT RootWindowHostWin::OnCaptureChanged(UINT message,
                                            WPARAM w_param,
                                            LPARAM l_param) {
  if (has_capture_) {
    has_capture_ = false;
    delegate_->OnHostLostCapture();
  }
  return 0;
}

void RootWindowHostWin::OnPaint(HDC dc) {
  delegate_->OnHostPaint();
  ValidateRect(hwnd(), NULL);
}

void RootWindowHostWin::OnSize(UINT param, const CSize& size) {
  // Minimizing resizes the window to 0x0 which causes our layout to go all
  // screwy, so we just ignore it.
  if (param != SIZE_MINIMIZED)
    delegate_->OnHostResized(gfx::Size(size.cx, size.cy));
}

}  // namespace aura
