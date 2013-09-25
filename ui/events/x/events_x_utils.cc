// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/x/events_x_utils.h"

#include <X11/keysym.h>
#include <X11/X.h>
#include <X11/Xlib.h>

#include "base/logging.h"
#include "ui/events/keycodes/keyboard_code_conversion_x.h"

namespace {

// Converts ui::EventType to XKeyEvent state.
unsigned int XKeyEventState(int flags) {
  return
      ((flags & ui::EF_SHIFT_DOWN) ? ShiftMask : 0) |
      ((flags & ui::EF_CONTROL_DOWN) ? ControlMask : 0) |
      ((flags & ui::EF_ALT_DOWN) ? Mod1Mask : 0) |
      ((flags & ui::EF_CAPS_LOCK_DOWN) ? LockMask : 0);
}

// Converts EventType to XKeyEvent type.
int XKeyEventType(ui::EventType type) {
  switch (type) {
    case ui::ET_KEY_PRESSED:
      return KeyPress;
    case ui::ET_KEY_RELEASED:
      return KeyRelease;
    default:
      return 0;
  }
}

// Converts KeyboardCode to XKeyEvent keycode.
unsigned int XKeyEventKeyCode(ui::KeyboardCode key_code,
                              int flags,
                              XDisplay* display) {
  const int keysym = XKeysymForWindowsKeyCode(key_code,
                                              flags & ui::EF_SHIFT_DOWN);
  // Tests assume the keycode for XK_less is equal to the one of XK_comma,
  // but XKeysymToKeycode returns 94 for XK_less while it returns 59 for
  // XK_comma. Here we convert the value for XK_less to the value for XK_comma.
  return (keysym == XK_less) ? 59 : XKeysymToKeycode(display, keysym);
}

}  // namespace ui

namespace ui {

void InitXKeyEventForTesting(EventType type,
                             KeyboardCode key_code,
                             int flags,
                             XEvent* event) {
  CHECK(event);
  XDisplay* display = gfx::GetXDisplay();
  XKeyEvent key_event;
  key_event.type = XKeyEventType(type);
  CHECK_NE(0, key_event.type);
  key_event.serial = 0;
  key_event.send_event = 0;
  key_event.display = display;
  key_event.time = 0;
  key_event.window = 0;
  key_event.root = 0;
  key_event.subwindow = 0;
  key_event.x = 0;
  key_event.y = 0;
  key_event.x_root = 0;
  key_event.y_root = 0;
  key_event.state = XKeyEventState(flags);
  key_event.keycode = XKeyEventKeyCode(key_code, flags, display);
  key_event.same_screen = 1;
  event->type = key_event.type;
  event->xkey = key_event;
}

}  // namespace ui
