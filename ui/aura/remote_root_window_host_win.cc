// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/remote_root_window_host_win.h"

#include <windows.h>

#include <algorithm>

#include "base/message_loop.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_sender.h"
#include "ui/aura/client/capture_client.h"
#include "ui/aura/root_window.h"
#include "ui/base/cursor/cursor_loader_win.h"
#include "ui/base/events/event.h"
#include "ui/base/events/event_utils.h"
#include "ui/base/keycodes/keyboard_code_conversion_win.h"
#include "ui/base/view_prop.h"
#include "ui/metro_viewer/metro_viewer_messages.h"

namespace aura {

namespace {

const char* kRootWindowHostWinKey = "__AURA_REMOTE_ROOT_WINDOW_HOST_WIN__";

// The touch id to be used for touch events coming in from Windows Ash.
const int kRemoteWindowTouchId = 10;

}  // namespace

RemoteRootWindowHostWin* g_instance = NULL;

RemoteRootWindowHostWin* RemoteRootWindowHostWin::Instance() {
  return g_instance;
}

RemoteRootWindowHostWin* RemoteRootWindowHostWin::Create(
    const gfx::Rect& bounds) {
  g_instance = new RemoteRootWindowHostWin(bounds);
  return g_instance;
}

RemoteRootWindowHostWin::RemoteRootWindowHostWin(const gfx::Rect& bounds)
    : delegate_(NULL), host_(NULL) {
  prop_.reset(new ui::ViewProp(NULL, kRootWindowHostWinKey, this));
}

RemoteRootWindowHostWin::~RemoteRootWindowHostWin() {
}

void RemoteRootWindowHostWin::Connected(IPC::Sender* host) {
  CHECK(host_ == NULL);
  host_ = host;
}

void RemoteRootWindowHostWin::Disconnected() {
  CHECK(host_ != NULL);
  host_ = NULL;
}

bool RemoteRootWindowHostWin::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(RemoteRootWindowHostWin, message)
    IPC_MESSAGE_HANDLER(MetroViewerHostMsg_MouseMoved, OnMouseMoved)
    IPC_MESSAGE_HANDLER(MetroViewerHostMsg_MouseButton, OnMouseButton)
    IPC_MESSAGE_HANDLER(MetroViewerHostMsg_KeyDown, OnKeyDown)
    IPC_MESSAGE_HANDLER(MetroViewerHostMsg_KeyUp, OnKeyUp)
    IPC_MESSAGE_HANDLER(MetroViewerHostMsg_Character, OnChar)
    IPC_MESSAGE_HANDLER(MetroViewerHostMsg_VisibilityChanged,
                        OnVisibilityChanged)
    IPC_MESSAGE_HANDLER(MetroViewerHostMsg_TouchDown,
                        OnTouchDown)
    IPC_MESSAGE_HANDLER(MetroViewerHostMsg_TouchUp,
                        OnTouchUp)
    IPC_MESSAGE_HANDLER(MetroViewerHostMsg_TouchMoved,
                        OnTouchMoved)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void RemoteRootWindowHostWin::SetDelegate(RootWindowHostDelegate* delegate) {
  delegate_ = delegate;
}

RootWindow* RemoteRootWindowHostWin::GetRootWindow() {
  return delegate_->AsRootWindow();
}

gfx::AcceleratedWidget RemoteRootWindowHostWin::GetAcceleratedWidget() {
  // TODO(cpu): This is bad. Chrome's compositor needs a valid window
  // initially and then later on we swap it. Since the compositor never
  // uses this initial window we tell ourselves this hack is ok to get
  // thing off the ground.
  return ::GetDesktopWindow();
}

void RemoteRootWindowHostWin::Show() {
}

void RemoteRootWindowHostWin::Hide() {
  NOTIMPLEMENTED();
}

void RemoteRootWindowHostWin::ToggleFullScreen() {
}

gfx::Rect RemoteRootWindowHostWin::GetBounds() const {
  gfx::Rect r(gfx::Point(0, 0), aura::RootWindowHost::GetNativeScreenSize());
  return r;
}

void RemoteRootWindowHostWin::SetBounds(const gfx::Rect& bounds) {
  delegate_->OnHostResized(bounds.size());
}

gfx::Point RemoteRootWindowHostWin::GetLocationOnNativeScreen() const {
  return gfx::Point(0, 0);
}

void RemoteRootWindowHostWin::SetCursor(gfx::NativeCursor native_cursor) {
  if (!host_)
    return;
  host_->Send(
      new MetroViewerHostMsg_SetCursor(uint64(native_cursor.platform())));
}

void RemoteRootWindowHostWin::SetCapture() {
}

void RemoteRootWindowHostWin::ReleaseCapture() {
}

bool RemoteRootWindowHostWin::QueryMouseLocation(gfx::Point* location_return) {
  POINT pt;
  GetCursorPos(&pt);
  *location_return =
      gfx::Point(static_cast<int>(pt.x), static_cast<int>(pt.y));
  return true;
}

bool RemoteRootWindowHostWin::ConfineCursorToRootWindow() {
  return true;
}

bool RemoteRootWindowHostWin::CopyAreaToSkCanvas(const gfx::Rect& source_bounds,
                                                 const gfx::Point& dest_offset,
                                                 SkCanvas* canvas) {
  NOTIMPLEMENTED();
  return false;
}

bool RemoteRootWindowHostWin::GrabSnapshot(
    const gfx::Rect& snapshot_bounds,
    std::vector<unsigned char>* png_representation) {
  NOTIMPLEMENTED();
  return false;
}

void RemoteRootWindowHostWin::UnConfineCursor() {
}

void RemoteRootWindowHostWin::OnCursorVisibilityChanged(bool show) {
  NOTIMPLEMENTED();
}

void RemoteRootWindowHostWin::MoveCursorTo(const gfx::Point& location) {
}

void RemoteRootWindowHostWin::SetFocusWhenShown(bool focus_when_shown) {
  NOTIMPLEMENTED();
}

void RemoteRootWindowHostWin::PostNativeEvent(
    const base::NativeEvent& native_event) {
}

void RemoteRootWindowHostWin::OnDeviceScaleFactorChanged(
    float device_scale_factor) {
  NOTIMPLEMENTED();
}

void RemoteRootWindowHostWin::PrepareForShutdown() {
}

void RemoteRootWindowHostWin::OnMouseMoved(int32 x, int32 y, int32 flags) {
  gfx::Point location(x, y);
  ui::MouseEvent event(ui::ET_MOUSE_MOVED, location, location, flags);
  delegate_->OnHostMouseEvent(&event);
}

void RemoteRootWindowHostWin::OnMouseButton(
    int32 x, int32 y, int32 extra, ui::EventType type, ui::EventFlags flags) {
  gfx::Point location(x, y);
  ui::MouseEvent mouse_event(type, location, location, 0);
  mouse_event.set_flags(flags);

  if (type == ui::ET_MOUSEWHEEL) {
    ui::MouseWheelEvent wheel_event(mouse_event, extra);
    delegate_->OnHostMouseEvent(&wheel_event);
  } else {
    mouse_event.SetClickCount(1);
    delegate_->OnHostMouseEvent(&mouse_event);
  }
}

void RemoteRootWindowHostWin::OnKeyDown(uint32 vkey,
                                        uint32 repeat_count,
                                        uint32 scan_code,
                                        uint32 flags) {
  ui::KeyEvent event(ui::ET_KEY_PRESSED,
                     ui::KeyboardCodeForWindowsKeyCode(vkey),
                     flags,
                     false);
  delegate_->OnHostKeyEvent(&event);
}

void RemoteRootWindowHostWin::OnKeyUp(uint32 vkey,
                                      uint32 repeat_count,
                                      uint32 scan_code,
                                      uint32 flags) {
  ui::KeyEvent event(ui::ET_KEY_RELEASED,
                     ui::KeyboardCodeForWindowsKeyCode(vkey),
                     flags,
                     false);
  delegate_->OnHostKeyEvent(&event);
}

void RemoteRootWindowHostWin::OnChar(uint32 key_code,
                                     uint32 repeat_count,
                                     uint32 scan_code,
                                     uint32 flags) {
  ui::KeyEvent event(ui::ET_KEY_PRESSED,
                     ui::KeyboardCodeForWindowsKeyCode(key_code),
                     flags,
                     true);
  delegate_->OnHostKeyEvent(&event);
}

void RemoteRootWindowHostWin::OnVisibilityChanged(bool visible) {
  if (visible)
    delegate_->OnHostActivated();
}

void RemoteRootWindowHostWin::OnTouchDown(int32 x, int32 y, uint64 timestamp) {
  ui::TouchEvent event(ui::ET_TOUCH_PRESSED,
                       gfx::Point(x, y),
                       kRemoteWindowTouchId,
                       base::TimeDelta::FromMicroseconds(timestamp));
  delegate_->OnHostTouchEvent(&event);
}

void RemoteRootWindowHostWin::OnTouchUp(int32 x, int32 y, uint64 timestamp) {
  ui::TouchEvent event(ui::ET_TOUCH_RELEASED,
                       gfx::Point(x, y),
                       kRemoteWindowTouchId,
                       base::TimeDelta::FromMicroseconds(timestamp));
  delegate_->OnHostTouchEvent(&event);
}

void RemoteRootWindowHostWin::OnTouchMoved(int32 x,
                                           int32 y,
                                           uint64 timestamp) {
  ui::TouchEvent event(ui::ET_TOUCH_MOVED,
                       gfx::Point(x, y),
                       kRemoteWindowTouchId,
                       base::TimeDelta::FromMicroseconds(timestamp));
  delegate_->OnHostTouchEvent(&event);
}

}  // namespace aura
