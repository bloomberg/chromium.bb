// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/events.h"

#include <X11/Xlib.h>
#include <X11/extensions/XInput.h>
#include <X11/extensions/XInput2.h>
#include <string.h>

#include "base/logging.h"
#include "ui/base/keycodes/keyboard_code_conversion_x.h"
#include "ui/base/touch/touch_factory.h"
#include "ui/base/x/x11_util.h"
#include "ui/gfx/point.h"

#if !defined(TOOLKIT_USES_GTK)
#include "base/message_pump_x.h"
#endif

// Copied from xserver-properties.h
#define AXIS_LABEL_PROP_REL_HWHEEL "Rel Horiz Wheel"
#define AXIS_LABEL_PROP_REL_WHEEL "Rel Vert Wheel"

namespace {

// Scroll amount for each wheelscroll event. 53 is also the value used for GTK+.
static const int kWheelScrollAmount = 53;

static const int kMinWheelButton = 4;
#if defined(OS_CHROMEOS)
// TODO(davemoore) For now use the button to decide how much to scroll by.
// When we go to XI2 scroll events this won't be necessary. If this doesn't
// happen for some reason we can better detect which devices are touchpads.
static const int kTouchpadScrollAmount = 3;
// Chrome OS also uses buttons 8 and 9 for scrolling.
static const int kMaxWheelButton = 9;
#else
static const int kMaxWheelButton = 7;
#endif

// A class to support the detection of scroll events, using X11 valuators.
class UI_EXPORT ScrollEventData {
 public:
  // Returns the ScrollEventData singleton.
  static ScrollEventData* GetInstance() {
    return Singleton<ScrollEventData>::get();
  }

  // Updates the list of devices.
  void UpdateDeviceList(Display* display) {
    scroll_devices_.reset();
    device_to_valuators_.clear();

    int count = 0;
    XDeviceInfo* dev_list = XListInputDevices(display, &count);
    Atom xi_touchpad = XInternAtom(display, XI_TOUCHPAD, false);
    for (int i = 0; i < count; ++i) {
      XDeviceInfo* dev = dev_list + i;
      if (dev->type == xi_touchpad)
        scroll_devices_[dev_list[i].id] = true;
    }
    if (dev_list)
      XFreeDeviceList(dev_list);

    XIDeviceInfo* info_list = XIQueryDevice(display, XIAllDevices, &count);
    Atom x_axis = XInternAtom(display, AXIS_LABEL_PROP_REL_HWHEEL, false);
    Atom y_axis = XInternAtom(display, AXIS_LABEL_PROP_REL_WHEEL, false);
    for (int i = 0; i < count; ++i) {
      XIDeviceInfo* info = info_list + i;

      if (!scroll_devices_[info->deviceid])
        continue;

      if (info->use != XISlavePointer && info->use != XIFloatingSlave) {
        scroll_devices_[info->deviceid] = false;
        continue;
      }

      Valuators valuators = {-1, -1};
      for (int j = 0; j < info->num_classes; ++j) {
        if (info->classes[j]->type != XIValuatorClass)
          continue;

        XIValuatorClassInfo* v =
            reinterpret_cast<XIValuatorClassInfo*>(info->classes[j]);
        if (v->label == x_axis)
          valuators.x_scroll = v->number;
        else if (v->label == y_axis)
          valuators.y_scroll = v->number;
      }
      if (valuators.x_scroll >= 0 && valuators.y_scroll >= 0)
        device_to_valuators_[info->deviceid] = valuators;
      else
        scroll_devices_[info->deviceid] = false;
    }
  }

  // Returns true if this is a scroll event (a motion event with the necessary
  // valuators. Also returns the offsets. |x_offset| and |y_offset| can be
  // NULL.
  bool GetScrollOffsets(const XEvent& xev, float* x_offset, float* y_offset) {
    XIDeviceEvent* xiev = static_cast<XIDeviceEvent*>(xev.xcookie.data);

    if (x_offset)
      *x_offset = 0;
    if (y_offset)
      *y_offset = 0;

    if (!scroll_devices_[xiev->deviceid])
      return false;

    int x_scroll = device_to_valuators_[xiev->deviceid].x_scroll;
    int y_scroll = device_to_valuators_[xiev->deviceid].y_scroll;

    bool has_x_offset = XIMaskIsSet(xiev->valuators.mask, x_scroll);
    bool has_y_offset = XIMaskIsSet(xiev->valuators.mask, y_scroll);
    bool is_scroll = has_x_offset || has_y_offset;

    if (!x_offset && !y_offset)
      return is_scroll;

    double* valuators = xiev->valuators.values;
    for (int i = 0; i < xiev->valuators.mask_len * 8; ++i) {
      if (XIMaskIsSet(xiev->valuators.mask, i)) {
        if (x_offset && x_scroll == i)
          *x_offset = -(*valuators);
        else if (y_offset && y_scroll == i)
          *y_offset = -(*valuators);
        valuators++;
      }
    }

    return is_scroll;
  }

 private:
  // Requirement for Singleton
  friend struct DefaultSingletonTraits<ScrollEventData>;

  struct Valuators {
    int x_scroll;
    int y_scroll;
  };

  ScrollEventData() {
    UpdateDeviceList(ui::GetXDisplay());
  }

  ~ScrollEventData() {}

  // A quick lookup table for determining if events from the pointer device
  // should be processed.
  static const int kMaxDeviceNum = 128;
  std::bitset<kMaxDeviceNum> scroll_devices_;
  std::map<int, Valuators> device_to_valuators_;

  DISALLOW_COPY_AND_ASSIGN(ScrollEventData);
};

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
      return ui::ET_TOUCH_PRESSED;
    case XI_TouchUpdate:
      return ui::ET_TOUCH_MOVED;
    case XI_TouchEnd:
      return ui::ET_TOUCH_RELEASED;
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
        case XI_ButtonRelease: {
          int button = EventButtonFromNative(native_event);
          if (button >= kMinWheelButton && button <= kMaxWheelButton)
            return ET_MOUSEWHEEL;
          return xievent->evtype == XI_ButtonPress ?
              ET_MOUSE_PRESSED : ET_MOUSE_RELEASED;
        }
        case XI_Motion:
          if (GetScrollOffsets(native_event, NULL, NULL))
            return ET_SCROLL;
          else if (GetButtonMaskForX2Event(xievent))
            return ET_MOUSE_DRAGGED;
          else
            return ET_MOUSE_MOVED;
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

#if defined(USE_XI2_MT)
      // Touch event valuators aren't coordinates.
      // Return the |event_x|/|event_y| directly as event's position.
      if (xievent->evtype == XI_TouchBegin ||
          xievent->evtype == XI_TouchUpdate ||
          xievent->evtype == XI_TouchEnd)
        return gfx::Point(static_cast<int>(xievent->event_x),
                          static_cast<int>(xievent->event_y));
#endif
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
  if (native_event->type == GenericEvent)
    button = EventButtonFromNative(native_event);
  else
    button = native_event->xbutton.button;
  switch (button) {
    case 4:
#if defined(OS_CHROMEOS)
      return kTouchpadScrollAmount;
    case 8:
#endif
      return kWheelScrollAmount;

    case 5:
#if defined(OS_CHROMEOS)
      return -kTouchpadScrollAmount;
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
  if (!factory->IsMultiTouchDevice(xievent->sourceid)) {
    // TODO(sad): Come up with a way to generate touch-ids for multi-touch
    // events when touch-events are generated from a single-touch device.
    return slot;
  }

#if defined(USE_XI2_MT)
  float tracking_id;
  if (!factory->ExtractTouchParam(
         *xev, ui::TouchFactory::TP_TRACKING_ID, &tracking_id)) {
    LOG(ERROR) << "Could not get the slot ID for the event. Using 0.";
  } else {
    slot = factory->GetSlotForTrackingID(tracking_id);
    ui::EventType type = ui::EventTypeFromNative(xev);
    if (type == ui::ET_TOUCH_CANCELLED ||
        type == ui::ET_TOUCH_RELEASED) {
      factory->ReleaseSlotForTrackingID(tracking_id);
    }
  }
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

bool GetScrollOffsets(const base::NativeEvent& native_event,
                      float* x_offset,
                      float* y_offset) {
  return ScrollEventData::GetInstance()->GetScrollOffsets(
      *native_event, x_offset, y_offset);
}

void UpdateDeviceList() {
  Display* display = GetXDisplay();
  ScrollEventData::GetInstance()->UpdateDeviceList(display);
  TouchFactory::GetInstance()->UpdateDeviceList(display);
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
