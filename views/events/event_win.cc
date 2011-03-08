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
  flags |= (GetKeyState(VK_MENU) & 0x80)? ui::EF_ALT_DOWN : 0;
  flags |= (GetKeyState(VK_SHIFT) & 0x80)? ui::EF_SHIFT_DOWN : 0;
  flags |= (GetKeyState(VK_CONTROL) & 0x80)? ui::EF_CONTROL_DOWN : 0;
  return flags;
}

// Convert windows message identifiers to Event types.
ui::EventType EventTypeFromNative(NativeEvent native_event) {
  switch (native_event.message) {
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
    case WM_CHAR:
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

// Get views::Event flags from a native Windows message
int EventFlagsFromNative(NativeEvent native_event) {
  int flags = 0;

  // TODO(msw): ORing the pressed/released button into the flags is _wrong_.
  // It makes it impossible to tell which button was modified when multiple
  // buttons are/were held down. We need to instead put the modified button into
  // a separate member on the MouseEvent, then audit all consumers of
  // MouseEvents to fix them to use the resulting values correctly.
  switch (native_event.message) {
    case WM_LBUTTONDBLCLK:
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_NCLBUTTONDBLCLK:
    case WM_NCLBUTTONDOWN:
    case WM_NCLBUTTONUP:
      native_event.wParam |= MK_LBUTTON;
      break;
    case WM_MBUTTONDBLCLK:
    case WM_MBUTTONDOWN:
    case WM_MBUTTONUP:
    case WM_NCMBUTTONDBLCLK:
    case WM_NCMBUTTONDOWN:
    case WM_NCMBUTTONUP:
      native_event.wParam |= MK_MBUTTON;
      break;
    case WM_RBUTTONDBLCLK:
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
    case WM_NCRBUTTONDBLCLK:
    case WM_NCRBUTTONDOWN:
    case WM_NCRBUTTONUP:
      native_event.wParam |= MK_RBUTTON;
      break;
  }

  // Check if the event occurred in the non-client area.
  if (IsNonClientMouseEvent(native_event))
    flags |= ui::EF_IS_NON_CLIENT;

    // Check for double click events.
  switch (native_event.message) {
    case WM_NCLBUTTONDBLCLK:
    case WM_NCMBUTTONDBLCLK:
    case WM_NCRBUTTONDBLCLK:
    case WM_LBUTTONDBLCLK:
    case WM_MBUTTONDBLCLK:
    case WM_RBUTTONDBLCLK:
      flags |= ui::EF_IS_DOUBLE_CLICK;
      break;
  }

  UINT win_flags = GET_KEYSTATE_WPARAM(native_event.wParam);
  flags |= (win_flags & MK_CONTROL) ? ui::EF_CONTROL_DOWN : 0;
  flags |= (win_flags & MK_SHIFT) ? ui::EF_SHIFT_DOWN : 0;
  flags |= (GetKeyState(VK_MENU) < 0) ? ui::EF_ALT_DOWN : 0;
  flags |= (win_flags & MK_LBUTTON) ? ui::EF_LEFT_BUTTON_DOWN : 0;
  flags |= (win_flags & MK_MBUTTON) ? ui::EF_MIDDLE_BUTTON_DOWN : 0;
  flags |= (win_flags & MK_RBUTTON) ? ui::EF_RIGHT_BUTTON_DOWN : 0;

  return flags;
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
// LocatedEvent, protected:

LocatedEvent::LocatedEvent(NativeEvent native_event)
    : Event(native_event, EventTypeFromNative(native_event),
            EventFlagsFromNative(native_event)),
      location_(native_event.pt.x, native_event.pt.y) {
}

LocatedEvent::LocatedEvent(NativeEvent2 native_event_2,
                           FromNativeEvent2 from_native)
    : Event(native_event_2, ui::ET_UNKNOWN, 0, from_native) {
  // No one should ever call this on Windows.
  // TODO(msw): remove once we rid views of Gtk/Gdk.
  NOTREACHED();
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

////////////////////////////////////////////////////////////////////////////////
// MouseEvent, public:

MouseEvent::MouseEvent(NativeEvent native_event)
    : LocatedEvent(native_event) {
  if (IsNonClientMouseEvent(native_event)) {
    // Non-client message. The position is contained in a POINTS structure in
    // LPARAM, and is in screen coordinates so we have to convert to client.
    POINT native_point = location_.ToPOINT();
    ScreenToClient(native_event.hwnd, &native_point);
    location_ = gfx::Point(native_point);
  }
}

MouseEvent::MouseEvent(NativeEvent2 native_event_2,
                       FromNativeEvent2 from_native)
    : LocatedEvent(native_event_2, from_native) {
  // No one should ever call this on Windows.
  // TODO(msw): remove once we rid views of Gtk/Gdk.
  NOTREACHED();
}

////////////////////////////////////////////////////////////////////////////////
// MouseWheelEvent, public:

MouseWheelEvent::MouseWheelEvent(NativeEvent native_event)
    : MouseEvent(native_event),
      offset_(GET_WHEEL_DELTA_WPARAM(native_event.wParam)) {
}

MouseWheelEvent::MouseWheelEvent(NativeEvent2 native_event_2,
                                 FromNativeEvent2 from_native)
    : MouseEvent(native_event_2, from_native) {
  // No one should ever call this on Windows.
  // TODO(msw): remove once we rid views of Gtk/Gdk.
  NOTREACHED();
}

}  // namespace views
