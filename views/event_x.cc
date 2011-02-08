// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/event.h"

#include <gdk/gdkx.h>
#if defined(HAVE_XINPUT2)
#include <X11/extensions/XInput2.h>
#endif

#include "ui/base/keycodes/keyboard_code_conversion_x.h"
#include "views/widget/root_view.h"
#include "views/widget/widget_gtk.h"

namespace views {

namespace {

// Scroll amount for each wheelscroll event. 53 is also the value used for GTK+.
static int kWheelScrollAmount = 53;

int GetEventFlagsFromXState(unsigned int state) {
  int flags = 0;
  if (state & ControlMask)
    flags |= Event::EF_CONTROL_DOWN;
  if (state & ShiftMask)
    flags |= Event::EF_SHIFT_DOWN;
  if (state & Mod1Mask)
    flags |= Event::EF_ALT_DOWN;
  if (state & LockMask)
    flags |= Event::EF_CAPS_LOCK_DOWN;
  if (state & Button1Mask)
    flags |= Event::EF_LEFT_BUTTON_DOWN;
  if (state & Button2Mask)
    flags |= Event::EF_MIDDLE_BUTTON_DOWN;
  if (state & Button3Mask)
    flags |= Event::EF_RIGHT_BUTTON_DOWN;

  return flags;
}

// Get the event flag for the button in XButtonEvent. During a ButtonPress
// event, |state| in XButtonEvent does not include the button that has just been
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

#if defined(HAVE_XINPUT2)
int GetButtonMaskForX2Event(XIDeviceEvent* xievent) {
  int buttonflags = 0;

  for (int i = 0; i < 8 * xievent->buttons.mask_len; i++) {
    if (XIMaskIsSet(xievent->buttons.mask, i)) {
      buttonflags |= GetEventFlagsForButton(i);
    }
  }

  return buttonflags;
}

Event::EventType GetTouchEventType(XEvent* xev) {
  XGenericEventCookie* cookie = &xev->xcookie;
  switch (cookie->evtype) {
    case XI_ButtonPress:
      return Event::ET_TOUCH_PRESSED;
    case XI_ButtonRelease:
      return Event::ET_TOUCH_RELEASED;
    case XI_Motion:
      return Event::ET_TOUCH_MOVED;

    // Note: We will not generate a _STATIONARY event here. It will be created,
    // when necessary, by a RWHVV.

    // TODO(sad): When do we trigger a _CANCELLED event? Maybe that will also be
    // done by a RWHVV, e.g. when it gets destroyed in the middle of a
    // touch-sequence?
  }

  return Event::ET_UNKNOWN;
}

gfx::Point GetTouchEventLocation(XEvent* xev) {
  XIDeviceEvent* xiev = static_cast<XIDeviceEvent*>(xev->xcookie.data);
  return gfx::Point(xiev->event_x, xiev->event_y);
}

int GetTouchEventFlags(XEvent* xev) {
  XIDeviceEvent* xiev = static_cast<XIDeviceEvent*>(xev->xcookie.data);
  return GetButtonMaskForX2Event(xiev) |
         GetEventFlagsFromXState(xiev->mods.effective);
}

int GetTouchIDFromXEvent(XEvent* xev) {
  // TODO(sad): How we determine the touch-id from the event is as yet
  // undecided.
  XIDeviceEvent* xiev = static_cast<XIDeviceEvent*>(xev->xcookie.data);
  return xiev->sourceid;
}

#endif  // HAVE_XINPUT2

int GetMouseWheelOffset(XEvent* xev) {
#if defined(HAVE_XINPUT2)
  if (xev->type == GenericEvent) {
    XIDeviceEvent* xiev = static_cast<XIDeviceEvent*>(xev->xcookie.data);
    return xiev->detail == 4 ? kWheelScrollAmount : -kWheelScrollAmount;
  }
#endif
  return xev->xbutton.button == 4 ? kWheelScrollAmount : -kWheelScrollAmount;
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
#if defined(HAVE_XINPUT2)
    case GenericEvent: {
      XIDeviceEvent* xievent =
          static_cast<XIDeviceEvent*>(xev->xcookie.data);
      switch (xievent->evtype) {
        case XI_ButtonPress:
          return Event::ET_MOUSE_PRESSED;
        case XI_ButtonRelease:
          return Event::ET_MOUSE_RELEASED;
        case XI_Motion:
          return GetButtonMaskForX2Event(xievent) ? Event::ET_MOUSE_DRAGGED :
              Event::ET_MOUSE_MOVED;
      }
    }
#endif
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

#if defined(HAVE_XINPUT2)
    case GenericEvent: {
      XIDeviceEvent* xievent =
          static_cast<XIDeviceEvent*>(xev->xcookie.data);
      return gfx::Point(static_cast<int>(xievent->event_x),
                        static_cast<int>(xievent->event_y));
    }
#endif
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

#if defined(HAVE_XINPUT2)
    case GenericEvent: {
      XIDeviceEvent* xievent = static_cast<XIDeviceEvent*>(xev->xcookie.data);
      switch (xievent->evtype) {
        case XI_ButtonPress:
        case XI_ButtonRelease:
          return GetButtonMaskForX2Event(xievent) |
                 GetEventFlagsFromXState(xievent->mods.effective) |
                 GetEventFlagsForButton(xievent->detail);

        case XI_Motion:
           return GetButtonMaskForX2Event(xievent) |
                  GetEventFlagsFromXState(xievent->mods.effective);
      }
    }
#endif
  }

  return 0;
}

}  // namespace

KeyEvent::KeyEvent(XEvent* xev)
    : Event(xev->type == KeyPress ?
            Event::ET_KEY_PRESSED : Event::ET_KEY_RELEASED,
            GetEventFlagsFromXState(xev->xkey.state)),
      key_code_(ui::KeyboardCodeFromXKeyEvent(xev)),
      repeat_count_(0),
      message_flags_(0),
      native_event_(NULL) {
}

MouseEvent::MouseEvent(XEvent* xev)
    : LocatedEvent(GetMouseEventType(xev),
                   GetMouseEventLocation(xev),
                   GetMouseEventFlags(xev)) {
}

MouseWheelEvent::MouseWheelEvent(XEvent* xev)
    : LocatedEvent(Event::ET_MOUSEWHEEL,
                   GetMouseEventLocation(xev),
                   GetEventFlagsFromXState(xev->xbutton.state)),
      offset_(GetMouseWheelOffset(xev)) {
}

#if defined(HAVE_XINPUT2)
TouchEvent::TouchEvent(XEvent* xev)
    : LocatedEvent(GetTouchEventType(xev),
                   GetTouchEventLocation(xev),
                   GetTouchEventFlags(xev)),
      touch_id_(GetTouchIDFromXEvent(xev)) {
}
#endif

}  // namespace views
