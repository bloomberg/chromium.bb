// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/events/event.h"

#include <windows.h>

#include "base/logging.h"
#include "ui/base/keycodes/keyboard_code_conversion_win.h"

namespace views {

namespace {

// Returns a mask corresponding to the set of modifier keys that are currently
// pressed. Windows key messages don't come with control key state as parameters
// as with mouse messages, so we need to explicitly ask for these states.
int GetKeyStateFlags() {
  int flags = 0;
  if (GetKeyState(VK_MENU) & 0x80)
    flags |= ui::EF_ALT_DOWN;
  if (GetKeyState(VK_SHIFT) & 0x80)
    flags |= ui::EF_SHIFT_DOWN;
  if (GetKeyState(VK_CONTROL) & 0x80)
    flags |= ui::EF_CONTROL_DOWN;
  return flags;
}

// Convert windows message identifiers to Event types.
ui::EventType EventTypeFromNative(NativeEvent native_event) {
  switch (native_event.message) {
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
      return ui::ET_KEY_PRESSED;
    case WM_KEYUP:
    case WM_SYSKEYUP:
      return ui::ET_KEY_RELEASED;
    case WM_LBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_NCLBUTTONDOWN:
    case WM_NCMBUTTONDOWN:
    case WM_NCRBUTTONDOWN:
    case WM_RBUTTONDOWN:
      return ui::ET_MOUSE_PRESSED;
    case WM_LBUTTONDBLCLK:
    case WM_LBUTTONUP:
    case WM_MBUTTONDBLCLK:
    case WM_MBUTTONUP:
    case WM_NCLBUTTONDBLCLK:
    case WM_NCLBUTTONUP:
    case WM_NCMBUTTONDBLCLK:
    case WM_NCMBUTTONUP:
    case WM_NCRBUTTONDBLCLK:
    case WM_NCRBUTTONUP:
    case WM_RBUTTONDBLCLK:
    case WM_RBUTTONUP:
      return ui::ET_MOUSE_RELEASED;
    case WM_MOUSEMOVE:
    case WM_NCMOUSEMOVE:
      return ui::ET_MOUSE_MOVED;
    case WM_MOUSEWHEEL:
      return ui::ET_MOUSEWHEEL;
    case WM_MOUSELEAVE:
    case WM_NCMOUSELEAVE:
      return ui::ET_MOUSE_EXITED;
    default:
      NOTREACHED();
  }
  return ui::ET_UNKNOWN;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// Event, public:

int Event::GetWindowsFlags() const {
  // TODO: need support for x1/x2.
  int result = 0;
  result |= (flags_ & ui::EF_SHIFT_DOWN) ? MK_SHIFT : 0;
  result |= (flags_ & ui::EF_CONTROL_DOWN) ? MK_CONTROL : 0;
  result |= (flags_ & ui::EF_LEFT_BUTTON_DOWN) ? MK_LBUTTON : 0;
  result |= (flags_ & ui::EF_MIDDLE_BUTTON_DOWN) ? MK_MBUTTON : 0;
  result |= (flags_ & ui::EF_RIGHT_BUTTON_DOWN) ? MK_RBUTTON : 0;
  return result;
}

//static
int Event::ConvertWindowsFlags(UINT win_flags) {
  int r = 0;
  if (win_flags & MK_CONTROL)
    r |= ui::EF_CONTROL_DOWN;
  if (win_flags & MK_SHIFT)
    r |= ui::EF_SHIFT_DOWN;
  if (GetKeyState(VK_MENU) < 0)
    r |= ui::EF_ALT_DOWN;
  if (win_flags & MK_LBUTTON)
    r |= ui::EF_LEFT_BUTTON_DOWN;
  if (win_flags & MK_MBUTTON)
    r |= ui::EF_MIDDLE_BUTTON_DOWN;
  if (win_flags & MK_RBUTTON)
    r |= ui::EF_RIGHT_BUTTON_DOWN;
  return r;
}

////////////////////////////////////////////////////////////////////////////////
// Event, private:

void Event::Init() {
  ZeroMemory(&native_event_, sizeof(native_event_));
  native_event_2_ = NULL;
}

void Event::InitWithNativeEvent(NativeEvent native_event) {
  native_event_ = native_event;
  // TODO(beng): remove once we rid views of Gtk/Gdk.
  native_event_2_ = NULL;
}

void Event::InitWithNativeEvent2(NativeEvent2 native_event_2,
                                 FromNativeEvent2) {
  // No one should ever call this on Windows.
  // TODO(beng): remove once we rid views of Gtk/Gdk.
  NOTREACHED();
  native_event_2_ = NULL;
}

////////////////////////////////////////////////////////////////////////////////
// KeyEvent, public:

KeyEvent::KeyEvent(NativeEvent native_event)
    : Event(native_event,
            EventTypeFromNative(native_event),
            GetKeyStateFlags()),
      key_code_(ui::KeyboardCodeForWindowsKeyCode(native_event.wParam)) {
}

KeyEvent::KeyEvent(NativeEvent2 native_event_2, FromNativeEvent2 from_native)
    : Event(native_event_2, ui::ET_UNKNOWN, 0, from_native) {
  // No one should ever call this on Windows.
  // TODO(beng): remove once we rid views of Gtk/Gdk.
  NOTREACHED();
}

}  // namespace views
