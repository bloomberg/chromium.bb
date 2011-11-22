// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/events.h"

#include <X11/Xlib.h>
#include <X11/extensions/XInput2.h>
#include <string.h>

#include "base/logging.h"
#include "ui/base/keycodes/keyboard_code_conversion_x.h"
#include "ui/base/touch/touch_factory.h"
#include "ui/gfx/point.h"

#if !defined(TOOLKIT_USES_GTK)
#include "base/message_pump_x.h"
#endif

namespace {

// Scroll amount for each wheelscroll event. 53 is also the value used for GTK+.
static const int kWheelScrollAmount = 53;

static const int kMinWheelButton = 4;
#if defined(OS_CHROMEOS)
// Chrome OS also uses buttons 8 and 9 for scrolling.
static const int kMaxWheelButton = 9;
#else
static const int kMaxWheelButton = 7;
#endif

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
    default:
      return 0;
  }
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

ui::EventType GetTouchEventType(const base::NativeEvent& native_event) {
  XIDeviceEvent* event =
      static_cast<XIDeviceEvent*>(native_event->xcookie.data);
#if defined(USE_XI2_MT)
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
  ui::TouchFactory* factory = ui::TouchFactory::GetInstance();

  // Check to see if we are treating a mouse-event as a touch-device.
  if (!factory->IsRealTouchDevice(event->sourceid)) {
    switch (event->evtype) {
      case XI_ButtonPress:
        return ui::ET_TOUCH_PRESSED;
      case XI_ButtonRelease:
        return ui::ET_TOUCH_RELEASED;
      case XI_Motion:
        return ui::ET_TOUCH_MOVED;
      default:
        NOTREACHED();
    }
  }

  DCHECK_EQ(event->evtype, XI_Motion);

  // Note: We will not generate a _STATIONARY event here. It will be created,
  // when necessary, by a RWHVV.
  // TODO(sad): When should _CANCELLED be generated?

  float slot;
  if (!factory->ExtractTouchParam(*native_event, ui::TouchFactory::TP_SLOT_ID,
                                  &slot))
    return ui::ET_UNKNOWN;

  if (!factory->IsSlotUsed(slot)) {
    // This is a new touch point.
    return ui::ET_TOUCH_PRESSED;
  }

  float tracking;
  if (!factory->ExtractTouchParam(*native_event,
                                  ui::TouchFactory::TP_TRACKING_ID, &tracking))
    return ui::ET_UNKNOWN;

  if (tracking == 0l) {
    // The touch point has been released.
    return ui::ET_TOUCH_RELEASED;
  }

  return ui::ET_TOUCH_MOVED;
#endif  // defined(USE_XI2_MT)
}

float GetTouchParamFromXEvent(XEvent* xev,
                              ui::TouchFactory::TouchParam tp,
                              float default_value) {
  ui::TouchFactory::GetInstance()->ExtractTouchParam(*xev, tp, &default_value);
  return default_value;
}

}  // namespace

namespace ui {

EventType EventTypeFromNative(const base::NativeEvent& native_event) {
  switch (native_event->type) {
    case KeyPress:
      return ET_KEY_PRESSED;
    case KeyRelease:
      return ET_KEY_RELEASED;
    case ButtonPress:
      if (static_cast<int>(native_event->xbutton.button) >= kMinWheelButton &&
          static_cast<int>(native_event->xbutton.button) <= kMaxWheelButton)
        return ET_MOUSEWHEEL;
      return ET_MOUSE_PRESSED;
    case ButtonRelease:
      if (static_cast<int>(native_event->xbutton.button) >= kMinWheelButton &&
          static_cast<int>(native_event->xbutton.button) <= kMaxWheelButton)
        return ET_MOUSEWHEEL;
      return ET_MOUSE_RELEASED;
    case MotionNotify:
      if (native_event->xmotion.state &
          (Button1Mask | Button2Mask | Button3Mask))
        return ET_MOUSE_DRAGGED;
      return ET_MOUSE_MOVED;
    case EnterNotify:
      return ET_MOUSE_ENTERED;
    case LeaveNotify:
      return ET_MOUSE_EXITED;
    case GenericEvent: {
      XIDeviceEvent* xievent =
          static_cast<XIDeviceEvent*>(native_event->xcookie.data);
      if (TouchFactory::GetInstance()->IsTouchDevice(xievent->sourceid))
        return GetTouchEventType(native_event);
      switch (xievent->evtype) {
        case XI_ButtonPress:
          if (xievent->detail >= kMinWheelButton &&
              xievent->detail <= kMaxWheelButton)
            return ET_MOUSEWHEEL;
          return ET_MOUSE_PRESSED;
        case XI_ButtonRelease:
          if (xievent->detail >= kMinWheelButton &&
              xievent->detail <= kMaxWheelButton)
            return ET_MOUSEWHEEL;
          return ET_MOUSE_RELEASED;
        case XI_Motion:
          return GetButtonMaskForX2Event(xievent) ?
              ET_MOUSE_DRAGGED : ET_MOUSE_MOVED;
      }
    }
    default:
      break;
  }
  return ET_UNKNOWN;
}

int EventFlagsFromNative(const base::NativeEvent& native_event) {
  switch (native_event->type) {
    case KeyPress:
    case KeyRelease:
      return GetEventFlagsFromXState(native_event->xbutton.state);
    case ButtonPress:
    case ButtonRelease: {
      int flags = GetEventFlagsFromXState(native_event->xbutton.state);
      const EventType type = EventTypeFromNative(native_event);
      if (type == ET_MOUSE_PRESSED || type == ET_MOUSE_RELEASED)
        flags |= GetEventFlagsForButton(native_event->xbutton.button);
      return flags;
    }
    case MotionNotify:
      return GetEventFlagsFromXState(native_event->xmotion.state);
    case GenericEvent: {
      XIDeviceEvent* xievent =
          static_cast<XIDeviceEvent*>(native_event->xcookie.data);
      const bool touch =
          TouchFactory::GetInstance()->IsTouchDevice(xievent->sourceid);
      switch (xievent->evtype) {
        case XI_ButtonPress:
        case XI_ButtonRelease: {
          int flags = GetButtonMaskForX2Event(xievent) |
              GetEventFlagsFromXState(xievent->mods.effective);
          const EventType type = EventTypeFromNative(native_event);
          if ((type == ET_MOUSE_PRESSED || type == ET_MOUSE_RELEASED) && !touch)
            flags |= GetEventFlagsForButton(xievent->detail);
          return flags;
        }
        case XI_Motion:
           return GetButtonMaskForX2Event(xievent) |
                  GetEventFlagsFromXState(xievent->mods.effective);
      }
    }
  }
  return 0;
}

gfx::Point EventLocationFromNative(const base::NativeEvent& native_event) {
  switch (native_event->type) {
    case ButtonPress:
    case ButtonRelease:
      return gfx::Point(native_event->xbutton.x, native_event->xbutton.y);
    case MotionNotify:
      return gfx::Point(native_event->xmotion.x, native_event->xmotion.y);
    case GenericEvent: {
      XIDeviceEvent* xievent =
          static_cast<XIDeviceEvent*>(native_event->xcookie.data);
      return gfx::Point(static_cast<int>(xievent->event_x),
                        static_cast<int>(xievent->event_y));
    }
  }
  return gfx::Point();
}

KeyboardCode KeyboardCodeFromNative(const base::NativeEvent& native_event) {
  return KeyboardCodeFromXKeyEvent(native_event);
}

bool IsMouseEvent(const base::NativeEvent& native_event) {
  if (native_event->type == ButtonPress ||
      native_event->type == ButtonRelease ||
      native_event->type == MotionNotify)
    return true;
  if (native_event->type == GenericEvent) {
    XIDeviceEvent* xievent =
        static_cast<XIDeviceEvent*>(native_event->xcookie.data);
    return xievent->evtype == XI_ButtonPress ||
           xievent->evtype == XI_ButtonRelease ||
           xievent->evtype == XI_Motion;
  }
  return false;
}

int GetMouseWheelOffset(const base::NativeEvent& native_event) {
  int button;
  if (native_event->type == GenericEvent) {
    XIDeviceEvent* xiev =
        static_cast<XIDeviceEvent*>(native_event->xcookie.data);
    button = xiev->detail;
  } else {
    button = native_event->xbutton.button;
  }
  switch (button) {
    case 4:
#if defined(OS_CHROMEOS)
    case 8:
#endif
      return kWheelScrollAmount;

    case 5:
#if defined(OS_CHROMEOS)
    case 9:
#endif
      return -kWheelScrollAmount;

    default:
      // TODO(derat): Do something for horizontal scrolls (buttons 6 and 7)?
      return 0;
  }
}

int GetTouchId(const base::NativeEvent& xev) {
  float slot = 0;
  ui::TouchFactory* factory = ui::TouchFactory::GetInstance();
  XIDeviceEvent* xievent = static_cast<XIDeviceEvent*>(xev->xcookie.data);
  if (!factory->IsRealTouchDevice(xievent->sourceid)) {
    // TODO(sad): Come up with a way to generate touch-ids for multi-touch
    // events when touch-events are generated from a mouse.
    return slot;
  }

#if defined(USE_XI2_MT)
  float tracking_id;
  if (!factory->ExtractTouchParam(
         *xev, ui::TouchFactory::TP_TRACKING_ID, &tracking_id))
    LOG(ERROR) << "Could not get the slot ID for the event. Using 0.";
  else
    slot = factory->GetSlotForTrackingID(tracking_id);
#else
  if (!factory->ExtractTouchParam(
         *xev, ui::TouchFactory::TP_SLOT_ID, &slot))
    LOG(ERROR) << "Could not get the slot ID for the event. Using 0.";
#endif
  return slot;
}

float GetTouchRadiusX(const base::NativeEvent& native_event) {
  return GetTouchParamFromXEvent(native_event,
      ui::TouchFactory::TP_TOUCH_MAJOR, 2.0) / 2.0;
}

float GetTouchRadiusY(const base::NativeEvent& native_event) {
  return GetTouchParamFromXEvent(native_event,
      ui::TouchFactory::TP_TOUCH_MINOR, 2.0) / 2.0;
}

float GetTouchAngle(const base::NativeEvent& native_event) {
  return GetTouchParamFromXEvent(native_event,
      ui::TouchFactory::TP_ORIENTATION, 0.0) / 2.0;
}

float GetTouchForce(const base::NativeEvent& native_event) {
  float force = 0.0;
  force = GetTouchParamFromXEvent(native_event, ui::TouchFactory::TP_PRESSURE,
                                  0.0);
  unsigned int deviceid =
      static_cast<XIDeviceEvent*>(native_event->xcookie.data)->sourceid;
  // Force is normalized to fall into [0, 1]
  if (!ui::TouchFactory::GetInstance()->NormalizeTouchParam(
      deviceid, ui::TouchFactory::TP_PRESSURE, &force))
    force = 0.0;
  return force;
}

base::NativeEvent CreateNoopEvent() {
  static XEvent* noop = NULL;
  if (!noop) {
    noop = new XEvent();
    memset(noop, 0, sizeof(XEvent));
    noop->xclient.type = ClientMessage;
    noop->xclient.window = None;
    noop->xclient.format = 8;
    DCHECK(!noop->xclient.display);
  }
  // TODO(oshima): Remove ifdef once gtk is removed from views.
#if defined(TOOLKIT_USES_GTK)
  NOTREACHED();
#else
  // Make sure we use atom from current xdisplay, which may
  // change during the test.
  noop->xclient.message_type = XInternAtom(
      base::MessagePumpX::GetDefaultXDisplay(),
      "noop", False);
#endif
  return noop;
}

}  // namespace ui
