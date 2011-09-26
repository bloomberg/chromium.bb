// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/events.h"

#include <X11/extensions/XInput2.h>

#include "base/logging.h"
#include "ui/base/keycodes/keyboard_code_conversion_x.h"
#include "ui/base/touch/touch_factory.h"
#include "ui/gfx/point.h"

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

ui::EventType GetTouchEventType(const ui::NativeEvent& native_event) {
#if defined(USE_XI2_MT)
  XIEvent* event = static_cast<XIEvent*>(native_event->xcookie.data);
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
  XGenericEventCookie* cookie = &native_event->xcookie;
  DCHECK_EQ(cookie->evtype, XI_Motion);

  // Note: We will not generate a _STATIONARY event here. It will be created,
  // when necessary, by a RWHVV.
  // TODO(sad): When should _CANCELLED be generated?

  ui::TouchFactory* factory = ui::TouchFactory::GetInstance();
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

}  // namespace

namespace ui {

EventType EventTypeFromNative(const NativeEvent& native_event) {
  switch (native_event->type) {
    case KeyPress:
      return ET_KEY_PRESSED;
    case KeyRelease:
      return ET_KEY_RELEASED;
    case ButtonPress:
      if (native_event->xbutton.button == 4 ||
          native_event->xbutton.button == 5)
        return ET_MOUSEWHEEL;
      return ET_MOUSE_PRESSED;
    case ButtonRelease:
      if (native_event->xbutton.button == 4 ||
          native_event->xbutton.button == 5)
        return ET_MOUSEWHEEL;
      return ET_MOUSE_RELEASED;
    case MotionNotify:
      if (native_event->xmotion.state &
          (Button1Mask | Button2Mask | Button3Mask))
        return ET_MOUSE_DRAGGED;
      return ET_MOUSE_MOVED;
    case GenericEvent: {
      XIDeviceEvent* xievent =
          static_cast<XIDeviceEvent*>(native_event->xcookie.data);
      if (TouchFactory::GetInstance()->IsTouchDevice(xievent->sourceid))
        return GetTouchEventType(native_event);
      switch (xievent->evtype) {
        case XI_ButtonPress:
          return (xievent->detail == 4 || xievent->detail == 5) ?
              ET_MOUSEWHEEL : ET_MOUSE_PRESSED;
        case XI_ButtonRelease:
          return (xievent->detail == 4 || xievent->detail == 5) ?
              ET_MOUSEWHEEL : ET_MOUSE_RELEASED;
        case XI_Motion:
          return GetButtonMaskForX2Event(xievent) ?
              ET_MOUSE_DRAGGED : ET_MOUSE_MOVED;
      }
    }
    default:
      NOTREACHED();
      break;
  }
  return ET_UNKNOWN;
}

int EventFlagsFromNative(const NativeEvent& native_event) {
  switch (native_event->type) {
    case KeyPress:
    case KeyRelease:
      return GetEventFlagsFromXState(native_event->xbutton.state);
    case ButtonPress:
    case ButtonRelease:
      return GetEventFlagsFromXState(native_event->xbutton.state) |
             GetEventFlagsForButton(native_event->xbutton.button);
    case MotionNotify:
      return GetEventFlagsFromXState(native_event->xmotion.state);
    case GenericEvent: {
      XIDeviceEvent* xievent =
          static_cast<XIDeviceEvent*>(native_event->xcookie.data);
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

gfx::Point EventLocationFromNative(const NativeEvent& native_event) {
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

KeyboardCode KeyboardCodeFromNative(const NativeEvent& native_event) {
  return KeyboardCodeFromXKeyEvent(native_event);
}

bool IsMouseEvent(const NativeEvent& native_event) {
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

int GetMouseWheelOffset(const NativeEvent& native_event) {
  if (native_event->type == GenericEvent) {
    XIDeviceEvent* xiev =
        static_cast<XIDeviceEvent*>(native_event->xcookie.data);
    return xiev->detail == 4 ? kWheelScrollAmount : -kWheelScrollAmount;
  }
  return native_event->xbutton.button == 4 ?
      kWheelScrollAmount : -kWheelScrollAmount;
}

}  // namespace ui
