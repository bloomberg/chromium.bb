// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/events/event.h"

#include <windowsx.h>

#include "base/logging.h"
#include "ui/base/keycodes/keyboard_code_conversion_win.h"

namespace ui {

namespace {

// Returns a mask corresponding to the set of modifier keys that are currently
// pressed. Windows key messages don't come with control key state as parameters
// as with mouse messages, so we need to explicitly ask for these states.
int GetKeyStateFlags() {
  int flags = 0;
  flags |= (GetKeyState(VK_MENU) & 0x80)? ui::EF_ALT_DOWN : 0;
  flags |= (GetKeyState(VK_SHIFT) & 0x80)? ui::EF_SHIFT_DOWN : 0;
  flags |= (GetKeyState(VK_CONTROL) & 0x80)? ui::EF_CONTROL_DOWN : 0;
  return flags;
}

// Convert windows message identifiers to Event types.
ui::Event::EventType EventTypeFromNative(NativeEvent native_event) {
  switch (native_event.message) {
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
    case WM_CHAR:
      return ui::Event::ET_KEY_PRESSED;
    case WM_KEYUP:
    case WM_SYSKEYUP:
      return ui::Event::ET_KEY_RELEASED;
    case WM_LBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_NCLBUTTONDOWN:
    case WM_NCMBUTTONDOWN:
    case WM_NCRBUTTONDOWN:
    case WM_RBUTTONDOWN:
      return ui::Event::ET_MOUSE_PRESSED;
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
      return ui::Event::ET_MOUSE_RELEASED;
    case WM_MOUSEMOVE:
    case WM_NCMOUSEMOVE:
      return ui::Event::ET_MOUSE_MOVED;
    case WM_MOUSEWHEEL:
      return ui::Event::ET_MOUSEWHEEL;
    case WM_MOUSELEAVE:
    case WM_NCMOUSELEAVE:
      return ui::Event::ET_MOUSE_EXITED;
    default:
      NOTREACHED();
  }
  return ui::Event::ET_UNKNOWN;
}

bool IsClientMouseEvent(NativeEvent native_event) {
  return native_event.message == WM_MOUSELEAVE ||
         native_event.message == WM_MOUSEHOVER ||
        (native_event.message >= WM_MOUSEFIRST &&
         native_event.message <= WM_MOUSELAST);
}

bool IsNonClientMouseEvent(NativeEvent native_event) {
  return native_event.message == WM_NCMOUSELEAVE ||
         native_event.message == WM_NCMOUSEHOVER ||
        (native_event.message >= WM_NCMOUSEMOVE &&
         native_event.message <= WM_NCXBUTTONDBLCLK);
}

gfx::Point MousePositionFromNative(NativeEvent native_event) {
  // Client message. The position is contained in the LPARAM.
  if (IsClientMouseEvent(native_event))
    return gfx::Point(native_event.lParam);
  DCHECK(IsNonClientMouseEvent(native_event));
  // Non-client message. The position is contained in a POINTS structure in
  // LPARAM, and is in screen coordinates so we have to convert to client.
  POINT native_point = { GET_X_LPARAM(native_event.lParam),
                         GET_Y_LPARAM(native_event.lParam) };
  ScreenToClient(native_event.hwnd, &native_point);
  return gfx::Point(native_point);
}

int MouseEventFlagsFromNative(NativeEvent native_event) {
  int flags = 0;

  // Check if the event occurred in the non-client area.
  if (IsNonClientMouseEvent(native_event))
    flags |= ui::MouseEvent::EF_IS_NON_CLIENT;

  // Check for double click events.
  switch (native_event.message) {
    case WM_NCLBUTTONDBLCLK:
    case WM_NCMBUTTONDBLCLK:
    case WM_NCRBUTTONDBLCLK:
    case WM_LBUTTONDBLCLK:
    case WM_MBUTTONDBLCLK:
    case WM_RBUTTONDBLCLK:
      flags |= ui::MouseEvent::EF_IS_DOUBLE_CLICK;
      break;
  }

  // Check for pressed buttons.
  if (IsClientMouseEvent(native_event)) {
    if (native_event.wParam & MK_LBUTTON)
      flags |= ui::MouseEvent::EF_LEFT_BUTTON_DOWN;
    if (native_event.wParam & MK_MBUTTON)
      flags |= ui::MouseEvent::EF_MIDDLE_BUTTON_DOWN;
    if (native_event.wParam & MK_RBUTTON)
      flags |= ui::MouseEvent::EF_RIGHT_BUTTON_DOWN;
  } else if (IsNonClientMouseEvent(native_event)) {
    switch (native_event.message) {
      case WM_NCLBUTTONDOWN:
        flags |= ui::MouseEvent::EF_LEFT_BUTTON_DOWN;
        break;
      case WM_NCMBUTTONDOWN:
        flags |= ui::MouseEvent::EF_MIDDLE_BUTTON_DOWN;
        break;
      case WM_NCRBUTTONDOWN:
        flags |= ui::MouseEvent::EF_RIGHT_BUTTON_DOWN;
        break;
    }
  }

  // Finally make sure the key state flags are included.
  return flags | GetKeyStateFlags();
}

int MouseWheelEventFlagsFromNative(NativeEvent native_event) {
  int win_flags = GET_KEYSTATE_WPARAM(native_event.wParam);
  int flags = 0;
  flags |= (win_flags & MK_CONTROL) ? ui::MouseEvent::EF_CONTROL_DOWN : 0;
  flags |= (win_flags & MK_SHIFT) ? ui::MouseEvent::EF_SHIFT_DOWN : 0;
  flags |= (GetKeyState(VK_MENU) < 0) ? ui::MouseEvent::EF_ALT_DOWN : 0;
  flags |= (win_flags & MK_LBUTTON) ? ui::MouseEvent::EF_LEFT_BUTTON_DOWN : 0;
  flags |= (win_flags & MK_MBUTTON) ? ui::MouseEvent::EF_MIDDLE_BUTTON_DOWN : 0;
  flags |= (win_flags & MK_RBUTTON) ? ui::MouseEvent::EF_RIGHT_BUTTON_DOWN : 0;
  return flags;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// KeyEvent, public:

KeyEvent::KeyEvent(NativeEvent native_event)
    : Event(EventTypeFromNative(native_event), GetKeyStateFlags()),
      key_code_(KeyboardCodeForWindowsKeyCode(native_event.wParam)),
      repeat_count_(native_event.lParam & 0xFFFF),
      message_flags_((native_event.lParam & 0xFFFF0000) >> 16) {
}

////////////////////////////////////////////////////////////////////////////////
// MouseEvent, public:

MouseEvent::MouseEvent(NativeEvent native_event)
    : LocatedEvent(EventTypeFromNative(native_event),
                   MousePositionFromNative(native_event),
                   MouseEventFlagsFromNative(native_event)) {
}

////////////////////////////////////////////////////////////////////////////////
// MouseWheelEvent, public:

MouseWheelEvent::MouseWheelEvent(NativeEvent native_event)
    : LocatedEvent(ET_MOUSEWHEEL,
                   MousePositionFromNative(native_event),
                   MouseWheelEventFlagsFromNative(native_event)),
      offset_(GET_WHEEL_DELTA_WPARAM(native_event.wParam)) {
}

}  // namespace ui

