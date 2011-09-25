// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/events/event.h"

#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <X11/extensions/XInput2.h>
#include <X11/Xlib.h>

#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "ui/base/keycodes/keyboard_code_conversion_x.h"
#include "views/touchui/touch_factory.h"
#include "views/widget/root_view.h"

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
#if defined(USE_XI2_MT)
  XIEvent* event = static_cast<XIEvent*>(xev->xcookie.data);
  switch(event->evtype) {
    case XI_TouchBegin:
      return ui::ET_TOUCH_PRESSED;
    case XI_TouchUpdate:
      return ui::ET_TOUCH_MOVED;
    case XI_TouchEnd:
      return ui::ET_TOUCH_RELEASED;
  }

  return ui::ET_UNKNOWN;
#else
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
#endif  // defined(USE_XI2_MT)
}

int GetTouchIDFromXEvent(XEvent* xev) {
  float id = 0;
#if defined(USE_XI2_MT)
  // TODO(ningxin.hu@gmail.com): Make the id always start from 0 for a new
  // touch-sequence when TRACKING_ID is used to extract the touch id.
  TouchFactory::TouchParam tp = TouchFactory::TP_TRACKING_ID;
#else
  TouchFactory::TouchParam tp = TouchFactory::TP_SLOT_ID;
#endif
  if (!TouchFactory::GetInstance()->ExtractTouchParam(
          *xev, tp, &id))
    LOG(ERROR) << "Could not get the touch ID for the event. Using 0.";
  return id;
}

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
    default:
      NOTREACHED();
      break;
  }
  return ui::ET_UNKNOWN;
}

int GetMouseWheelOffset(XEvent* xev) {
  if (xev->type == GenericEvent) {
    XIDeviceEvent* xiev = static_cast<XIDeviceEvent*>(xev->xcookie.data);
    return xiev->detail == 4 ? kWheelScrollAmount : -kWheelScrollAmount;
  }
  return xev->xbutton.button == 4 ? kWheelScrollAmount : -kWheelScrollAmount;
}

gfx::Point GetEventLocation(XEvent* xev) {
  switch (xev->type) {
    case ButtonPress:
    case ButtonRelease:
      return gfx::Point(xev->xbutton.x, xev->xbutton.y);

    case MotionNotify:
      return gfx::Point(xev->xmotion.x, xev->xmotion.y);

    case GenericEvent: {
      XIDeviceEvent* xievent =
          static_cast<XIDeviceEvent*>(xev->xcookie.data);
      return gfx::Point(static_cast<int>(xievent->event_x),
                        static_cast<int>(xievent->event_y));
    }
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

float GetTouchParamFromXEvent(XEvent* xev,
                              TouchFactory::TouchParam tp,
                              float default_value) {
  TouchFactory::GetInstance()->ExtractTouchParam(*xev, tp, &default_value);
  return default_value;
}

float GetTouchForceFromXEvent(XEvent* xev) {
  float force = 0.0;
  force = GetTouchParamFromXEvent(xev, TouchFactory::TP_PRESSURE, 0.0);
  unsigned int deviceid =
      static_cast<XIDeviceEvent*>(xev->xcookie.data)->sourceid;
  // Force is normalized to fall into [0, 1]
  if (!TouchFactory::GetInstance()->NormalizeTouchParam(
      deviceid, TouchFactory::TP_PRESSURE, &force))
    force = 0.0;
  return force;
}

// The following two functions are copied from event_gtk.cc. These will be
// removed when GTK dependency is removed.
uint16 GetCharacterFromGdkKeyval(guint keyval) {
  guint32 ch = gdk_keyval_to_unicode(keyval);

  // We only support BMP characters.
  return ch < 0xFFFE ? static_cast<uint16>(ch) : 0;
}

GdkEventKey* GetGdkEventKeyFromNative(NativeEvent native_event) {
  DCHECK(native_event->type == GDK_KEY_PRESS ||
         native_event->type == GDK_KEY_RELEASE);
  return &native_event->key;
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
      key_code_(ui::KeyboardCodeFromXKeyEvent(native_event_2)),
      character_(0),
      unmodified_character_(0) {
}

uint16 KeyEvent::GetCharacter() const {
  if (character_)
    return character_;

  if (!native_event_2()) {
    // This event may have been created from a Gdk event.
    if (IsControlDown() || !native_event())
      return GetCharacterFromKeyCode(key_code_, flags());

    uint16 ch = GetCharacterFromGdkKeyval(
        GetGdkEventKeyFromNative(native_event())->keyval);
    return ch ? ch : GetCharacterFromKeyCode(key_code_, flags());
  }

  DCHECK(native_event_2()->type == KeyPress ||
         native_event_2()->type == KeyRelease);

  uint16 ch = GetCharacterFromXKeyEvent(&native_event_2()->xkey);
  return ch ? ch : GetCharacterFromKeyCode(key_code_, flags());
}

uint16 KeyEvent::GetUnmodifiedCharacter() const {
  if (unmodified_character_)
    return unmodified_character_;

  if (!native_event_2()) {
    // This event may have been created from a Gdk event.
    if (!native_event())
      return GetCharacterFromKeyCode(key_code_, flags() & ui::EF_SHIFT_DOWN);

    GdkEventKey* key = GetGdkEventKeyFromNative(native_event());

    static const guint kIgnoredModifiers =
      GDK_CONTROL_MASK | GDK_LOCK_MASK | GDK_MOD1_MASK | GDK_MOD2_MASK |
      GDK_MOD3_MASK | GDK_MOD4_MASK | GDK_MOD5_MASK | GDK_SUPER_MASK |
      GDK_HYPER_MASK | GDK_META_MASK;

    // We can't use things like (key->state & GDK_SHIFT_MASK), as it may mask
    // out bits used by X11 or Gtk internally.
    GdkModifierType modifiers =
      static_cast<GdkModifierType>(key->state & ~kIgnoredModifiers);
    guint keyval = 0;
    uint16 ch = 0;
    if (gdk_keymap_translate_keyboard_state(NULL, key->hardware_keycode,
          modifiers, key->group, &keyval,
          NULL, NULL, NULL)) {
      ch = GetCharacterFromGdkKeyval(keyval);
    }

    return ch ? ch :
      GetCharacterFromKeyCode(key_code_, flags() & ui::EF_SHIFT_DOWN);
  }

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

////////////////////////////////////////////////////////////////////////////////
// MouseWheelEvent, public:

MouseWheelEvent::MouseWheelEvent(NativeEvent2 native_event_2,
                                 FromNativeEvent2 from_native)
    : MouseEvent(native_event_2, from_native),
      offset_(GetMouseWheelOffset(native_event_2)) {
}

////////////////////////////////////////////////////////////////////////////////
// TouchEvent, public:

TouchEvent::TouchEvent(NativeEvent2 native_event_2,
                       FromNativeEvent2 from_native)
    : LocatedEvent(native_event_2, from_native),
      touch_id_(GetTouchIDFromXEvent(native_event_2)),
      radius_x_(GetTouchParamFromXEvent(native_event_2,
                                        TouchFactory::TP_TOUCH_MAJOR,
                                        2.0) / 2.0),
      radius_y_(GetTouchParamFromXEvent(native_event_2,
                                        TouchFactory::TP_TOUCH_MINOR,
                                        2.0) / 2.0),
      rotation_angle_(GetTouchParamFromXEvent(native_event_2,
                                              TouchFactory::TP_ORIENTATION,
                                              0.0)),
      force_(GetTouchForceFromXEvent(native_event_2)) {
#if !defined(USE_XI2_MT)
  if (type() == ui::ET_TOUCH_PRESSED || type() == ui::ET_TOUCH_RELEASED) {
    TouchFactory* factory = TouchFactory::GetInstance();
    float slot;
    if (factory->ExtractTouchParam(*native_event_2,
                                   TouchFactory::TP_SLOT_ID, &slot)) {
      factory->SetSlotUsed(slot, type() == ui::ET_TOUCH_PRESSED);
    }
  }
#endif
}

}  // namespace views
