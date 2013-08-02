// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/events/event_constants.h"

#include <string.h>
#include <X11/extensions/XInput.h>
#include <X11/extensions/XInput2.h>
#include <X11/Xlib.h>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/message_loop/message_pump_aurax11.h"
#include "ui/base/events/event_utils.h"
#include "ui/base/keycodes/keyboard_code_conversion_x.h"
#include "ui/base/touch/touch_factory_x11.h"
#include "ui/base/x/device_data_manager.h"
#include "ui/base/x/device_list_cache_x.h"
#include "ui/base/x/x11_atom_cache.h"
#include "ui/base/x/x11_util.h"
#include "ui/gfx/display.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/screen.h"

namespace {

// Scroll amount for each wheelscroll event. 53 is also the value used for GTK+.
const int kWheelScrollAmount = 53;

const int kMinWheelButton = 4;
const int kMaxWheelButton = 7;

// A workaround for some incorrect implemented input drivers:
// Ignore their mouse input valuators.
bool IgnoreMouseValuators() {
  static bool initialized = false;
  static bool ignore_valuators = true;
  if (initialized)
    return ignore_valuators;
  ignore_valuators =
      CommandLine::ForCurrentProcess()->HasSwitch("disable-mouse-valuators");
  initialized = true;
  return ignore_valuators;
}

// A class to track current modifier state on master device. Only track ctrl,
// alt, shift and caps lock keys currently. The tracked state can then be used
// by floating device.
class XModifierStateWatcher{
 public:
  static XModifierStateWatcher* GetInstance() {
    return Singleton<XModifierStateWatcher>::get();
  }

  void UpdateStateFromEvent(const base::NativeEvent& native_event) {
    // Floating device can't access the modifer state from master device.
    // We need to track the states of modifier keys in a singleton for
    // floating devices such as touch screen. Issue 106426 is one example
    // of why we need the modifier states for floating device.
    state_ = native_event->xkey.state;
    // master_state is the state before key press. We need to track the
    // state after key press for floating device. Currently only ctrl,
    // shift, alt and caps lock keys are tracked.
    ui::KeyboardCode keyboard_code = ui::KeyboardCodeFromNative(native_event);
    unsigned int mask = 0;

    switch (keyboard_code) {
      case ui::VKEY_CONTROL: {
        mask = ControlMask;
        break;
      }
      case ui::VKEY_SHIFT: {
        mask = ShiftMask;
        break;
      }
      case ui::VKEY_MENU: {
        mask = Mod1Mask;
        break;
      }
      case ui::VKEY_CAPITAL: {
        mask = LockMask;
        break;
      }
      default:
        break;
    }

    if (native_event->type == KeyPress)
      state_ |= mask;
    else
      state_ &= ~mask;
  }

  // Returns the current modifer state in master device. It only contains the
  // state of ctrl, shift, alt and caps lock keys.
  unsigned int state() { return state_; }

 private:
  friend struct DefaultSingletonTraits<XModifierStateWatcher>;

  XModifierStateWatcher() : state_(0) { }

  unsigned int state_;

  DISALLOW_COPY_AND_ASSIGN(XModifierStateWatcher);
};

#if defined(USE_XI2_MT)
// Detects if a touch event is a driver-generated 'special event'.
// A 'special event' is a touch event with maximum radius and pressure at
// location (0, 0).
// This needs to be done in a cleaner way: http://crbug.com/169256
bool TouchEventIsGeneratedHack(const base::NativeEvent& native_event) {
  XIDeviceEvent* event =
      static_cast<XIDeviceEvent*>(native_event->xcookie.data);
  CHECK(event->evtype == XI_TouchBegin ||
        event->evtype == XI_TouchUpdate ||
        event->evtype == XI_TouchEnd);

  // Force is normalized to [0, 1].
  if (ui::GetTouchForce(native_event) < 1.0f)
    return false;

  if (ui::EventLocationFromNative(native_event) != gfx::Point())
    return false;

  // Radius is in pixels, and the valuator is the diameter in pixels.
  double radius = ui::GetTouchRadiusX(native_event), min, max;
  unsigned int deviceid =
      static_cast<XIDeviceEvent*>(native_event->xcookie.data)->sourceid;
  if (!ui::DeviceDataManager::GetInstance()->GetDataRange(
      deviceid, ui::DeviceDataManager::DT_TOUCH_MAJOR, &min, &max)) {
    return false;
  }

  return radius * 2 == max;
}
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
  if (state & Mod5Mask)
    flags |= ui::EF_ALTGR_DOWN;
  if (state & Button1Mask)
    flags |= ui::EF_LEFT_MOUSE_BUTTON;
  if (state & Button2Mask)
    flags |= ui::EF_MIDDLE_MOUSE_BUTTON;
  if (state & Button3Mask)
    flags |= ui::EF_RIGHT_MOUSE_BUTTON;
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
      return ui::EF_LEFT_MOUSE_BUTTON;
    case 2:
      return ui::EF_MIDDLE_MOUSE_BUTTON;
    case 3:
      return ui::EF_RIGHT_MOUSE_BUTTON;
    default:
      return 0;
  }
}

int GetButtonMaskForX2Event(XIDeviceEvent* xievent) {
  int buttonflags = 0;
  for (int i = 0; i < 8 * xievent->buttons.mask_len; i++) {
    if (XIMaskIsSet(xievent->buttons.mask, i)) {
      int button = (xievent->sourceid == xievent->deviceid) ?
                   ui::GetMappedButton(i) : i;
      buttonflags |= GetEventFlagsForButton(button);
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
      return TouchEventIsGeneratedHack(native_event) ? ui::ET_UNKNOWN :
                                                       ui::ET_TOUCH_PRESSED;
    case XI_TouchUpdate:
      return TouchEventIsGeneratedHack(native_event) ? ui::ET_UNKNOWN :
                                                       ui::ET_TOUCH_MOVED;
    case XI_TouchEnd:
      return TouchEventIsGeneratedHack(native_event) ? ui::ET_TOUCH_CANCELLED :
                                                       ui::ET_TOUCH_RELEASED;
  }

  return ui::ET_UNKNOWN;
#else
  ui::TouchFactory* factory = ui::TouchFactory::GetInstance();

  // If this device doesn't support multi-touch, then just use the normal
  // pressed/release events to indicate touch start/end.  With multi-touch,
  // these events are sent only for the first (pressed) or last (released)
  // touch point, and so we must infer start/end from motion events.
  if (!factory->IsMultiTouchDevice(event->sourceid)) {
    switch (event->evtype) {
      case XI_ButtonPress:
        return ui::ET_TOUCH_PRESSED;
      case XI_ButtonRelease:
        return ui::ET_TOUCH_RELEASED;
      case XI_Motion:
        if (GetButtonMaskForX2Event(event))
          return ui::ET_TOUCH_MOVED;
        return ui::ET_UNKNOWN;
      default:
        NOTREACHED();
    }
  }

  DCHECK_EQ(event->evtype, XI_Motion);

  // Note: We will not generate a _STATIONARY event here. It will be created,
  // when necessary, by a RWHVV.
  // TODO(sad): When should _CANCELLED be generated?

  ui::DeviceDataManager* manager = ui::DeviceDataManager::GetInstance();

  double slot;
  if (!manager->GetEventData(
      *native_event, ui::DeviceDataManager::DT_TOUCH_SLOT_ID, &slot))
    return ui::ET_UNKNOWN;

  if (!factory->IsSlotUsed(slot)) {
    // This is a new touch point.
    return ui::ET_TOUCH_PRESSED;
  }

  double tracking;
  if (!manager->GetEventData(
      *native_event, ui::DeviceDataManager::DT_TOUCH_TRACKING_ID, &tracking))
    return ui::ET_UNKNOWN;

  if (tracking == 0l) {
    // The touch point has been released.
    return ui::ET_TOUCH_RELEASED;
  }

  return ui::ET_TOUCH_MOVED;
#endif  // defined(USE_XI2_MT)
}

double GetTouchParamFromXEvent(XEvent* xev,
                              ui::DeviceDataManager::DataType val,
                              double default_value) {
  ui::DeviceDataManager::GetInstance()->GetEventData(
      *xev, val, &default_value);
  return default_value;
}

Atom GetNoopEventAtom() {
  return XInternAtom(
      base::MessagePumpAuraX11::GetDefaultXDisplay(),
      "noop", False);
}

}  // namespace

namespace ui {

void UpdateDeviceList() {
  Display* display = GetXDisplay();
  DeviceListCacheX::GetInstance()->UpdateDeviceList(display);
  TouchFactory::GetInstance()->UpdateDeviceList(display);
  DeviceDataManager::GetInstance()->UpdateDeviceList(display);
}

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
      // Drop wheel events; we should've already scrolled on the press.
      if (static_cast<int>(native_event->xbutton.button) >= kMinWheelButton &&
          static_cast<int>(native_event->xbutton.button) <= kMaxWheelButton)
        return ET_UNKNOWN;
      return ET_MOUSE_RELEASED;
    case MotionNotify:
      if (native_event->xmotion.state &
          (Button1Mask | Button2Mask | Button3Mask))
        return ET_MOUSE_DRAGGED;
      return ET_MOUSE_MOVED;
    case EnterNotify:
      // The standard on Windows is to send a MouseMove event when the mouse
      // first enters a window instead of sending a special mouse enter event.
      // To be consistent we follow the same style.
      return ET_MOUSE_MOVED;
    case LeaveNotify:
      return ET_MOUSE_EXITED;
    case GenericEvent: {
      TouchFactory* factory = TouchFactory::GetInstance();
      if (!factory->ShouldProcessXI2Event(native_event))
        return ET_UNKNOWN;

      XIDeviceEvent* xievent =
          static_cast<XIDeviceEvent*>(native_event->xcookie.data);

      if (factory->IsTouchDevice(xievent->sourceid))
        return GetTouchEventType(native_event);

      switch (xievent->evtype) {
        case XI_ButtonPress: {
          int button = EventButtonFromNative(native_event);
          if (button >= kMinWheelButton && button <= kMaxWheelButton)
            return ET_MOUSEWHEEL;
          return ET_MOUSE_PRESSED;
        }
        case XI_ButtonRelease: {
          int button = EventButtonFromNative(native_event);
          // Drop wheel events; we should've already scrolled on the press.
          if (button >= kMinWheelButton && button <= kMaxWheelButton)
            return ET_UNKNOWN;
          return ET_MOUSE_RELEASED;
        }
        case XI_Motion: {
          bool is_cancel;
          if (GetFlingData(native_event, NULL, NULL, NULL, NULL, &is_cancel)) {
            return is_cancel ? ET_SCROLL_FLING_CANCEL : ET_SCROLL_FLING_START;
          } else if (DeviceDataManager::GetInstance()->IsScrollEvent(
              native_event)) {
            return IsTouchpadEvent(native_event) ? ET_SCROLL : ET_MOUSEWHEEL;
          } else if (DeviceDataManager::GetInstance()->IsCMTMetricsEvent(
              native_event)) {
            return ET_UMA_DATA;
          } else if (GetButtonMaskForX2Event(xievent)) {
            return ET_MOUSE_DRAGGED;
          } else {
            return ET_MOUSE_MOVED;
          }
        }
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
    case KeyRelease: {
      XModifierStateWatcher::GetInstance()->UpdateStateFromEvent(native_event);
      return GetEventFlagsFromXState(native_event->xkey.state);
    }
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

      switch (xievent->evtype) {
#if defined(USE_XI2_MT)
        case XI_TouchBegin:
        case XI_TouchUpdate:
        case XI_TouchEnd:
          return GetButtonMaskForX2Event(xievent) |
                 GetEventFlagsFromXState(xievent->mods.effective) |
                 GetEventFlagsFromXState(
                     XModifierStateWatcher::GetInstance()->state());
          break;
#endif
        case XI_ButtonPress:
        case XI_ButtonRelease: {
          const bool touch =
              TouchFactory::GetInstance()->IsTouchDevice(xievent->sourceid);
          int flags = GetButtonMaskForX2Event(xievent) |
              GetEventFlagsFromXState(xievent->mods.effective);
          if (touch) {
            flags |= GetEventFlagsFromXState(
                XModifierStateWatcher::GetInstance()->state());
          }

          const EventType type = EventTypeFromNative(native_event);
          int button = EventButtonFromNative(native_event);
          if ((type == ET_MOUSE_PRESSED || type == ET_MOUSE_RELEASED) && !touch)
            flags |= GetEventFlagsForButton(button);
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

base::TimeDelta EventTimeFromNative(const base::NativeEvent& native_event) {
  switch(native_event->type) {
    case KeyPress:
    case KeyRelease:
      return base::TimeDelta::FromMilliseconds(native_event->xkey.time);
    case ButtonPress:
    case ButtonRelease:
      return base::TimeDelta::FromMilliseconds(native_event->xbutton.time);
      break;
    case MotionNotify:
      return base::TimeDelta::FromMilliseconds(native_event->xmotion.time);
      break;
    case EnterNotify:
    case LeaveNotify:
      return base::TimeDelta::FromMilliseconds(native_event->xcrossing.time);
      break;
    case GenericEvent: {
      double start, end;
      double touch_timestamp;
      if (GetGestureTimes(native_event, &start, &end)) {
        // If the driver supports gesture times, use them.
        return base::TimeDelta::FromMicroseconds(end * 1000000);
      } else if (DeviceDataManager::GetInstance()->GetEventData(*native_event,
                 DeviceDataManager::DT_TOUCH_RAW_TIMESTAMP, &touch_timestamp)) {
        return base::TimeDelta::FromMicroseconds(touch_timestamp * 1000000);
      } else {
        XIDeviceEvent* xide =
            static_cast<XIDeviceEvent*>(native_event->xcookie.data);
        return base::TimeDelta::FromMilliseconds(xide->time);
      }
      break;
    }
  }
  NOTREACHED();
  return base::TimeDelta();
}

gfx::Point EventLocationFromNative(const base::NativeEvent& native_event) {
  switch (native_event->type) {
    case EnterNotify:
    case LeaveNotify:
      return gfx::Point(native_event->xcrossing.x, native_event->xcrossing.y);
    case ButtonPress:
    case ButtonRelease:
      return gfx::Point(native_event->xbutton.x, native_event->xbutton.y);
    case MotionNotify:
      return gfx::Point(native_event->xmotion.x, native_event->xmotion.y);
    case GenericEvent: {
      XIDeviceEvent* xievent =
          static_cast<XIDeviceEvent*>(native_event->xcookie.data);

#if defined(USE_XI2_MT)
      // Touch event valuators aren't coordinates.
      // Return the |event_x|/|event_y| directly as event's position.
      if (xievent->evtype == XI_TouchBegin ||
          xievent->evtype == XI_TouchUpdate ||
          xievent->evtype == XI_TouchEnd)
        // Note: Touch events are always touch screen events.
        return gfx::Point(static_cast<int>(xievent->event_x),
                          static_cast<int>(xievent->event_y));
#endif
      if (IgnoreMouseValuators()) {
        return gfx::Point(static_cast<int>(xievent->event_x),
                          static_cast<int>(xievent->event_y));
      }
      // Read the position from the valuators, because the location reported in
      // event_x/event_y seems to be different (and doesn't match for events
      // coming from slave device and master device) from the values in the
      // valuators. See more on crbug.com/103981. The position in the valuators
      // is in the global screen coordinates. But it is necessary to convert it
      // into the window's coordinates. If the valuator is not set, that means
      // the value hasn't changed, and so we can use the value from
      // event_x/event_y (which are in the window's coordinates).
      double* valuators = xievent->valuators.values;

      double x = xievent->event_x;
      if (XIMaskIsSet(xievent->valuators.mask, 0))
        x = *valuators++ - (xievent->root_x - xievent->event_x);

      double y = xievent->event_y;
      if (XIMaskIsSet(xievent->valuators.mask, 1))
        y = *valuators++ - (xievent->root_y - xievent->event_y);

      return gfx::Point(static_cast<int>(x), static_cast<int>(y));
    }
  }
  return gfx::Point();
}

gfx::Point EventSystemLocationFromNative(
    const base::NativeEvent& native_event) {
  switch (native_event->type) {
    case EnterNotify:
    case LeaveNotify: {
      return gfx::Point(native_event->xcrossing.x_root,
                        native_event->xcrossing.y_root);
    }
    case ButtonPress:
    case ButtonRelease: {
      return gfx::Point(native_event->xbutton.x_root,
                        native_event->xbutton.y_root);
    }
    case MotionNotify: {
      return gfx::Point(native_event->xmotion.x_root,
                        native_event->xmotion.y_root);
    }
    case GenericEvent: {
      XIDeviceEvent* xievent =
          static_cast<XIDeviceEvent*>(native_event->xcookie.data);
      return gfx::Point(xievent->root_x, xievent->root_y);
    }
  }

  return gfx::Point();
}

int EventButtonFromNative(const base::NativeEvent& native_event) {
  CHECK_EQ(GenericEvent, native_event->type);
  XIDeviceEvent* xievent =
      static_cast<XIDeviceEvent*>(native_event->xcookie.data);
  int button = xievent->detail;

  return (xievent->sourceid == xievent->deviceid) ?
         ui::GetMappedButton(button) : button;
}

KeyboardCode KeyboardCodeFromNative(const base::NativeEvent& native_event) {
  return KeyboardCodeFromXKeyEvent(native_event);
}

bool IsMouseEvent(const base::NativeEvent& native_event) {
  if (native_event->type == EnterNotify ||
      native_event->type == LeaveNotify ||
      native_event->type == ButtonPress ||
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

int GetChangedMouseButtonFlagsFromNative(
    const base::NativeEvent& native_event) {
  switch (native_event->type) {
    case ButtonPress:
    case ButtonRelease:
      return GetEventFlagsFromXState(native_event->xbutton.state);
    case GenericEvent: {
      XIDeviceEvent* xievent =
          static_cast<XIDeviceEvent*>(native_event->xcookie.data);
      switch (xievent->evtype) {
        case XI_ButtonPress:
        case XI_ButtonRelease:
          return GetEventFlagsForButton(EventButtonFromNative(native_event));
        default:
          break;
      }
    }
    default:
      break;
  }
  return 0;
}

gfx::Vector2d GetMouseWheelOffset(const base::NativeEvent& native_event) {
  float x_offset, y_offset;
  if (GetScrollOffsets(
      native_event, &x_offset, &y_offset, NULL, NULL, NULL)) {
    return gfx::Vector2d(static_cast<int>(x_offset),
                         static_cast<int>(y_offset));
  }

  int button = native_event->type == GenericEvent ?
      EventButtonFromNative(native_event) : native_event->xbutton.button;

  switch (button) {
    case 4:
      return gfx::Vector2d(0, kWheelScrollAmount);
    case 5:
      return gfx::Vector2d(0, -kWheelScrollAmount);
    default:
      // TODO(derat): Do something for horizontal scrolls (buttons 6 and 7)?
      return gfx::Vector2d();
  }
}

void ClearTouchIdIfReleased(const base::NativeEvent& xev) {
#if defined(USE_XI2_MT)
  ui::EventType type = ui::EventTypeFromNative(xev);
  if (type == ui::ET_TOUCH_CANCELLED ||
      type == ui::ET_TOUCH_RELEASED) {
    ui::TouchFactory* factory = ui::TouchFactory::GetInstance();
    ui::DeviceDataManager* manager = ui::DeviceDataManager::GetInstance();
    double tracking_id;
    if (manager->GetEventData(
        *xev, ui::DeviceDataManager::DT_TOUCH_TRACKING_ID, &tracking_id)) {
      factory->ReleaseSlotForTrackingID(tracking_id);
    }
  }
#endif
}

int GetTouchId(const base::NativeEvent& xev) {
  double slot = 0;
  ui::TouchFactory* factory = ui::TouchFactory::GetInstance();
  XIDeviceEvent* xievent = static_cast<XIDeviceEvent*>(xev->xcookie.data);
  if (!factory->IsMultiTouchDevice(xievent->sourceid)) {
    // TODO(sad): Come up with a way to generate touch-ids for multi-touch
    // events when touch-events are generated from a single-touch device.
    return slot;
  }

  ui::DeviceDataManager* manager = ui::DeviceDataManager::GetInstance();

#if defined(USE_XI2_MT)
  double tracking_id;
  if (!manager->GetEventData(
      *xev, ui::DeviceDataManager::DT_TOUCH_TRACKING_ID, &tracking_id)) {
    LOG(ERROR) << "Could not get the tracking ID for the event. Using 0.";
  } else {
    slot = factory->GetSlotForTrackingID(tracking_id);
  }
#else
  if (!manager->GetEventData(
      *xev, ui::DeviceDataManager::DT_TOUCH_SLOT_ID, &slot))
    LOG(ERROR) << "Could not get the slot ID for the event. Using 0.";
#endif
  return slot;
}

float GetTouchRadiusX(const base::NativeEvent& native_event) {
  return GetTouchParamFromXEvent(native_event,
      ui::DeviceDataManager::DT_TOUCH_MAJOR, 0.0) / 2.0;
}

float GetTouchRadiusY(const base::NativeEvent& native_event) {
  return GetTouchParamFromXEvent(native_event,
      ui::DeviceDataManager::DT_TOUCH_MINOR, 0.0) / 2.0;
}

float GetTouchAngle(const base::NativeEvent& native_event) {
  return GetTouchParamFromXEvent(native_event,
      ui::DeviceDataManager::DT_TOUCH_ORIENTATION, 0.0) / 2.0;
}

float GetTouchForce(const base::NativeEvent& native_event) {
  double force = 0.0;
  force = GetTouchParamFromXEvent(native_event,
      ui::DeviceDataManager::DT_TOUCH_PRESSURE, 0.0);
  unsigned int deviceid =
      static_cast<XIDeviceEvent*>(native_event->xcookie.data)->sourceid;
  // Force is normalized to fall into [0, 1]
  if (!ui::DeviceDataManager::GetInstance()->NormalizeData(
      deviceid, ui::DeviceDataManager::DT_TOUCH_PRESSURE, &force))
    force = 0.0;
  return force;
}

bool GetScrollOffsets(const base::NativeEvent& native_event,
                      float* x_offset,
                      float* y_offset,
                      float* x_offset_ordinal,
                      float* y_offset_ordinal,
                      int* finger_count) {
  if (!DeviceDataManager::GetInstance()->IsScrollEvent(native_event))
    return false;

  // Temp values to prevent passing NULLs to DeviceDataManager.
  float x_offset_, y_offset_;
  float x_offset_ordinal_, y_offset_ordinal_;
  int finger_count_;
  if (!x_offset)
    x_offset = &x_offset_;
  if (!y_offset)
    y_offset = &y_offset_;
  if (!x_offset_ordinal)
    x_offset_ordinal = &x_offset_ordinal_;
  if (!y_offset_ordinal)
    y_offset_ordinal = &y_offset_ordinal_;
  if (!finger_count)
    finger_count = &finger_count_;

  DeviceDataManager::GetInstance()->GetScrollOffsets(
      native_event,
      x_offset, y_offset,
      x_offset_ordinal, y_offset_ordinal,
      finger_count);
  return true;
}

bool GetFlingData(const base::NativeEvent& native_event,
                  float* vx,
                  float* vy,
                  float* vx_ordinal,
                  float* vy_ordinal,
                  bool* is_cancel) {
  if (!DeviceDataManager::GetInstance()->IsFlingEvent(native_event))
    return false;

  float vx_, vy_;
  float vx_ordinal_, vy_ordinal_;
  bool is_cancel_;
  if (!vx)
    vx = &vx_;
  if (!vy)
    vy = &vy_;
  if (!vx_ordinal)
    vx_ordinal = &vx_ordinal_;
  if (!vy_ordinal)
    vy_ordinal = &vy_ordinal_;
  if (!is_cancel)
    is_cancel = &is_cancel_;

  DeviceDataManager::GetInstance()->GetFlingData(
      native_event, vx, vy, vx_ordinal, vy_ordinal, is_cancel);
  return true;
}

bool GetGestureTimes(const base::NativeEvent& native_event,
                     double* start_time,
                     double* end_time) {
  if (!DeviceDataManager::GetInstance()->HasGestureTimes(native_event))
    return false;

  double start_time_, end_time_;
  if (!start_time)
    start_time = &start_time_;
  if (!end_time)
    end_time = &end_time_;

  DeviceDataManager::GetInstance()->GetGestureTimes(
      native_event, start_time, end_time);
  return true;
}

void SetNaturalScroll(bool enabled) {
  DeviceDataManager::GetInstance()->set_natural_scroll_enabled(enabled);
}

bool IsNaturalScrollEnabled() {
  return DeviceDataManager::GetInstance()->natural_scroll_enabled();
}

bool IsTouchpadEvent(const base::NativeEvent& event) {
  return DeviceDataManager::GetInstance()->IsTouchpadXInputEvent(event);
}

bool IsNoopEvent(const base::NativeEvent& event) {
  return (event->type == ClientMessage &&
      event->xclient.message_type == GetNoopEventAtom());
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
  // Make sure we use atom from current xdisplay, which may
  // change during the test.
  noop->xclient.message_type = GetNoopEventAtom();
  return noop;
}

}  // namespace ui
