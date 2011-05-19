// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/events/event.h"

#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#if defined(HAVE_XINPUT2)
#include <X11/extensions/XInput2.h>
#endif
#include <X11/Xlib.h>

#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "ui/base/keycodes/keyboard_code_conversion_x.h"
#include "views/widget/root_view.h"

#if defined(HAVE_XINPUT2)
#include "views/touchui/touch_factory.h"
#endif

namespace views {

namespace {

// Scroll amount for each wheelscroll event. 53 is also the value used for GTK+.
static int kWheelScrollAmount = 53;

int GetEventFlagsFromXState(unsigned int state) {
  int flags = 0;
  if (state & ControlMask)
    flags |= ui::EF_CONTROL_DOWN;
  if (state & ShiftMask)
    flags |= ui::EF_SHIFT_DOWN;
  if (state & Mod1Mask)
    flags |= ui::EF_ALT_DOWN;
  if (state & LockMask)
    flags |= ui::EF_CAPS_LOCK_DOWN;
  if (state & Button1Mask)
    flags |= ui::EF_LEFT_BUTTON_DOWN;
  if (state & Button2Mask)
    flags |= ui::EF_MIDDLE_BUTTON_DOWN;
  if (state & Button3Mask)
    flags |= ui::EF_RIGHT_BUTTON_DOWN;

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
      return ui::EF_LEFT_BUTTON_DOWN;
    case 2:
      return ui::EF_MIDDLE_BUTTON_DOWN;
    case 3:
      return ui::EF_RIGHT_BUTTON_DOWN;
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

ui::EventType GetTouchEventType(XEvent* xev) {
  XGenericEventCookie* cookie = &xev->xcookie;
  DCHECK_EQ(cookie->evtype, XI_Motion);

  // Note: We will not generate a _STATIONARY event here. It will be created,
  // when necessary, by a RWHVV.
  // TODO(sad): When should _CANCELLED be generated?

  TouchFactory* factory = TouchFactory::GetInstance();
  float slot;
  if (!factory->ExtractTouchParam(*xev, TouchFactory::TP_SLOT_ID, &slot))
    return ui::ET_UNKNOWN;

  if (!factory->IsSlotUsed(slot)) {
    // This is a new touch point.
    return ui::ET_TOUCH_PRESSED;
  }

  float tracking;
  if (!factory->ExtractTouchParam(*xev, TouchFactory::TP_TRACKING_ID,
        &tracking))
    return ui::ET_UNKNOWN;

  if (tracking == 0l) {
    // The touch point has been released.
    return ui::ET_TOUCH_RELEASED;
  }

  return ui::ET_TOUCH_MOVED;
}

int GetTouchIDFromXEvent(XEvent* xev) {
  float slot = 0;
  if (!TouchFactory::GetInstance()->ExtractTouchParam(
        *xev, TouchFactory::TP_SLOT_ID, &slot))
    LOG(ERROR) << "Could not get the slot ID for the event. Using 0.";
  return slot;
}

#endif  // HAVE_XINPUT2

ui::EventType EventTypeFromNative(NativeEvent2 native_event) {
  switch (native_event->type) {
    case KeyPress:
      return ui::ET_KEY_PRESSED;
    case KeyRelease:
      return ui::ET_KEY_RELEASED;
    case ButtonPress:
      if (native_event->xbutton.button == 4 ||
          native_event->xbutton.button == 5)
        return ui::ET_MOUSEWHEEL;
      return ui::ET_MOUSE_PRESSED;
    case ButtonRelease:
      if (native_event->xbutton.button == 4 ||
          native_event->xbutton.button == 5)
        return ui::ET_MOUSEWHEEL;
      return ui::ET_MOUSE_RELEASED;
    case MotionNotify:
      if (native_event->xmotion.state &
          (Button1Mask | Button2Mask | Button3Mask))
        return ui::ET_MOUSE_DRAGGED;
      return ui::ET_MOUSE_MOVED;
#if defined(HAVE_XINPUT2)
    case GenericEvent: {
      XIDeviceEvent* xievent =
          static_cast<XIDeviceEvent*>(native_event->xcookie.data);
      if (TouchFactory::GetInstance()->IsTouchDevice(xievent->sourceid))
        return GetTouchEventType(native_event);
      switch (xievent->evtype) {
        case XI_ButtonPress:
          return (xievent->detail == 4 || xievent->detail == 5) ?
              ui::ET_MOUSEWHEEL : ui::ET_MOUSE_PRESSED;
        case XI_ButtonRelease:
          return (xievent->detail == 4 || xievent->detail == 5) ?
              ui::ET_MOUSEWHEEL : ui::ET_MOUSE_RELEASED;
        case XI_Motion:
          return GetButtonMaskForX2Event(xievent) ? ui::ET_MOUSE_DRAGGED :
              ui::ET_MOUSE_MOVED;
      }
    }
#endif
    default:
      NOTREACHED();
      break;
  }
  return ui::ET_UNKNOWN;
}

int GetMouseWheelOffset(XEvent* xev) {
#if defined(HAVE_XINPUT2)
  if (xev->type == GenericEvent) {
    XIDeviceEvent* xiev = static_cast<XIDeviceEvent*>(xev->xcookie.data);
    return xiev->detail == 4 ? kWheelScrollAmount : -kWheelScrollAmount;
  }
#endif
  return xev->xbutton.button == 4 ? kWheelScrollAmount : -kWheelScrollAmount;
}

gfx::Point GetEventLocation(XEvent* xev) {
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

int GetLocatedEventFlags(XEvent* xev) {
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
      bool touch =
          TouchFactory::GetInstance()->IsTouchDevice(xievent->sourceid);
      switch (xievent->evtype) {
        case XI_ButtonPress:
        case XI_ButtonRelease:
          return GetButtonMaskForX2Event(xievent) |
                 GetEventFlagsFromXState(xievent->mods.effective) |
                 (touch ? 0 : GetEventFlagsForButton(xievent->detail));

        case XI_Motion:
           return GetButtonMaskForX2Event(xievent) |
                  GetEventFlagsFromXState(xievent->mods.effective);
      }
    }
#endif
  }

  return 0;
}

uint16 GetCharacterFromXKeyEvent(XKeyEvent* key) {
  char buf[6];
  int bytes_written = XLookupString(key, buf, 6, NULL, NULL);
  DCHECK_LE(bytes_written, 6);

  string16 result;
  return (bytes_written > 0 && UTF8ToUTF16(buf, bytes_written, &result) &&
          result.length() == 1) ? result[0] : 0;
}

float GetTouchRadiusFromXEvent(XEvent* xev) {
  float diameter = 0.0;

#if defined(HAVE_XINPUT2)
  TouchFactory* touch_factory = TouchFactory::GetInstance();
  touch_factory->ExtractTouchParam(*xev, TouchFactory::TP_TOUCH_MAJOR,
                                   &diameter);
#endif

  return diameter / 2.0;
}

float GetTouchAngleFromXEvent(XEvent* xev) {
  float angle = 0.0;

#if defined(HAVE_XINPUT2)
  TouchFactory* touch_factory = TouchFactory::GetInstance();
  touch_factory->ExtractTouchParam(*xev, TouchFactory::TP_ORIENTATION,
                                   &angle);
#endif

  return angle;
}


float GetTouchRatioFromXEvent(XEvent* xev) {
  float ratio = 1.0;

#if defined(HAVE_XINPUT2)
  TouchFactory* touch_factory = TouchFactory::GetInstance();
  float major_v = -1.0;
  float minor_v = -1.0;

  if (!touch_factory->ExtractTouchParam(*xev,
                                        TouchFactory::TP_TOUCH_MAJOR,
                                        &major_v) ||
      !touch_factory->ExtractTouchParam(*xev,
                                        TouchFactory::TP_TOUCH_MINOR,
                                        &minor_v))
    return ratio;

  // In case minor axis exists but is zero.
  if (minor_v > 0.0)
    ratio = major_v / minor_v;
#endif

  return ratio;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// Event, private:

void Event::InitWithNativeEvent2(NativeEvent2 native_event_2,
                                 FromNativeEvent2) {
  native_event_ = NULL;
  // TODO(beng): remove once we rid views of Gtk/Gdk.
  native_event_2_ = native_event_2;
}

////////////////////////////////////////////////////////////////////////////////
// LocatedEvent, protected:

LocatedEvent::LocatedEvent(NativeEvent2 native_event_2,
                           FromNativeEvent2 from_native)
    : Event(native_event_2,
            EventTypeFromNative(native_event_2),
            GetLocatedEventFlags(native_event_2),
            from_native),
      location_(GetEventLocation(native_event_2)) {
}

////////////////////////////////////////////////////////////////////////////////
// KeyEvent, public:

KeyEvent::KeyEvent(NativeEvent2 native_event_2, FromNativeEvent2 from_native)
    : Event(native_event_2,
            EventTypeFromNative(native_event_2),
            GetEventFlagsFromXState(native_event_2->xkey.state),
            from_native),
      key_code_(ui::KeyboardCodeFromXKeyEvent(native_event_2)) {
}

uint16 KeyEvent::GetCharacter() const {
  if (!native_event_2())
    return GetCharacterFromKeyCode(key_code_, flags());

  DCHECK(native_event_2()->type == KeyPress ||
         native_event_2()->type == KeyRelease);

  uint16 ch = GetCharacterFromXKeyEvent(&native_event_2()->xkey);
  return ch ? ch : GetCharacterFromKeyCode(key_code_, flags());
}

uint16 KeyEvent::GetUnmodifiedCharacter() const {
  if (!native_event_2())
    return GetCharacterFromKeyCode(key_code_, flags() & ui::EF_SHIFT_DOWN);

  DCHECK(native_event_2()->type == KeyPress ||
         native_event_2()->type == KeyRelease);

  XKeyEvent key = native_event_2()->xkey;

  static const unsigned int kIgnoredModifiers = ControlMask | LockMask |
      Mod1Mask | Mod2Mask | Mod3Mask | Mod4Mask | Mod5Mask;

  // We can't use things like (key.state & ShiftMask), as it may mask out bits
  // used by X11 internally.
  key.state &= ~kIgnoredModifiers;
  uint16 ch = GetCharacterFromXKeyEvent(&key);
  return ch ? ch :
      GetCharacterFromKeyCode(key_code_, flags() & ui::EF_SHIFT_DOWN);
}

////////////////////////////////////////////////////////////////////////////////
// MouseEvent, public:

MouseEvent::MouseEvent(NativeEvent2 native_event_2,
                       FromNativeEvent2 from_native)
    : LocatedEvent(native_event_2, from_native) {
}

MouseEvent::MouseEvent(const TouchEvent& touch,
                       FromNativeEvent2 from_native)
    : LocatedEvent(touch.native_event_2(), from_native) {
  // The location of the event is correctly extracted from the native event. But
  // it is necessary to update the event type.
  ui::EventType mtype = ui::ET_UNKNOWN;
  switch (touch.type()) {
    case ui::ET_TOUCH_RELEASED:
      mtype = ui::ET_MOUSE_RELEASED;
      break;
    case ui::ET_TOUCH_PRESSED:
      mtype = ui::ET_MOUSE_PRESSED;
      break;
    case ui::ET_TOUCH_MOVED:
      mtype = ui::ET_MOUSE_MOVED;
      break;
    default:
      NOTREACHED() << "Invalid mouse event.";
  }
  set_type(mtype);

  // It may not be possible to extract the button-information necessary for a
  // MouseEvent from the native event for a TouchEvent, so the flags are
  // explicitly updated as well. The button is approximated from the touchpoint
  // identity.
  int new_flags = flags() & ~(ui::EF_LEFT_BUTTON_DOWN |
                              ui::EF_RIGHT_BUTTON_DOWN |
                              ui::EF_MIDDLE_BUTTON_DOWN);
  int button = ui::EF_LEFT_BUTTON_DOWN;
  if (touch.identity() == 1)
    button = ui::EF_RIGHT_BUTTON_DOWN;
  else if (touch.identity() == 2)
    button = ui::EF_MIDDLE_BUTTON_DOWN;
  set_flags(new_flags | button);
}

////////////////////////////////////////////////////////////////////////////////
// MouseWheelEvent, public:

MouseWheelEvent::MouseWheelEvent(NativeEvent2 native_event_2,
                                 FromNativeEvent2 from_native)
    : MouseEvent(native_event_2, from_native),
      offset_(GetMouseWheelOffset(native_event_2)) {
}

////////////////////////////////////////////////////////////////////////////////
// TouchEvent, public:

#if defined(HAVE_XINPUT2)
TouchEvent::TouchEvent(NativeEvent2 native_event_2,
                       FromNativeEvent2 from_native)
    : LocatedEvent(native_event_2, from_native),
      touch_id_(GetTouchIDFromXEvent(native_event_2)),
      radius_(GetTouchRadiusFromXEvent(native_event_2)),
      angle_(GetTouchAngleFromXEvent(native_event_2)),
      ratio_(GetTouchRatioFromXEvent(native_event_2)) {
  if (type() == ui::ET_TOUCH_PRESSED || type() == ui::ET_TOUCH_RELEASED) {
    TouchFactory* factory = TouchFactory::GetInstance();
    float slot;
    if (factory->ExtractTouchParam(*native_event_2,
                                   TouchFactory::TP_SLOT_ID, &slot)) {
      factory->SetSlotUsed(slot, type() == ui::ET_TOUCH_PRESSED);
    }
  }
}
#endif

}  // namespace views
