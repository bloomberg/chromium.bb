// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/event.h"

#include <gdk/gdkx.h>

#include "app/keyboard_code_conversion_x.h"
#include "views/widget/root_view.h"
#include "views/widget/widget_gtk.h"

namespace views {

namespace {

int GetEventFlagsFromXState(unsigned int state) {
  int flags = 0;
  if (state & ControlMask)
    flags |= Event::EF_CONTROL_DOWN;
  if (state & ShiftMask)
    flags |= Event::EF_SHIFT_DOWN;
  if (state & Mod1Mask)
    flags |= Event::EF_ALT_DOWN;
  if (state & Button1Mask)
    flags |= Event::EF_LEFT_BUTTON_DOWN;
  if (state & Button2Mask)
    flags |= Event::EF_MIDDLE_BUTTON_DOWN;
  if (state & Button3Mask)
    flags |= Event::EF_RIGHT_BUTTON_DOWN;

  return flags;
}

// Get the event flag for the button in XButtonEvent. During a KeyPress event,
// |state| in XButtonEvent does not include the button that has just been
// pressed. Instead |state| contains flags for the buttons (if any) that had
// already been pressed before the current button, and |button| stores the most
// current pressed button. So, if you press down left mouse button, and while
// pressing it down, press down the right mouse button, then for the latter
// event, |state| would have Button1Mask set but not Button3Mask, and |button|
// would be 3.
int GetEventFlagsForButton(int button) {
  switch (button) {
    case 1:
      return Event::EF_LEFT_BUTTON_DOWN;
    case 2:
      return Event::EF_MIDDLE_BUTTON_DOWN;
    case 3:
      return Event::EF_RIGHT_BUTTON_DOWN;
  }

  DLOG(WARNING) << "Unexpected button (" << button << ") received.";
  return 0;
}

Event::EventType GetMouseEventType(XEvent* xev) {
  switch (xev->type) {
    case ButtonPress:
      return Event::ET_MOUSE_PRESSED;
    case ButtonRelease:
      return Event::ET_MOUSE_RELEASED;
    case MotionNotify:
      if (xev->xmotion.state & (Button1Mask | Button2Mask | Button3Mask)) {
        return Event::ET_MOUSE_DRAGGED;
      }
      return Event::ET_MOUSE_MOVED;
  }

  return Event::ET_UNKNOWN;
}

gfx::Point GetMouseEventLocation(XEvent* xev) {
  switch (xev->type) {
    case ButtonPress:
    case ButtonRelease:
      return gfx::Point(xev->xbutton.x, xev->xbutton.y);

    case MotionNotify:
      return gfx::Point(xev->xmotion.x, xev->xmotion.y);
  }

  return gfx::Point();
}

int GetMouseEventFlags(XEvent* xev) {
  switch (xev->type) {
    case ButtonPress:
    case ButtonRelease:
      return GetEventFlagsFromXState(xev->xbutton.state) |
             GetEventFlagsForButton(xev->xbutton.button);

    case MotionNotify:
      return GetEventFlagsFromXState(xev->xmotion.state);
  }

  return 0;
}

}  // namespace

KeyEvent::KeyEvent(XEvent* xev)
    : Event(xev->type == KeyPress ?
            Event::ET_KEY_PRESSED : Event::ET_KEY_RELEASED,
            GetEventFlagsFromXState(xev->xkey.state)),
      key_code_(app::KeyboardCodeFromXKeyEvent(xev)),
      repeat_count_(0),
      message_flags_(0) {
}

MouseEvent::MouseEvent(XEvent* xev)
    : LocatedEvent(GetMouseEventType(xev),
                   GetMouseEventLocation(xev),
                   GetMouseEventFlags(xev)) {
}

}  // namespace views
