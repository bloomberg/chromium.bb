// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/events.h"

#include <X11/Xlib.h>
#include <X11/extensions/XInput.h>
#include <X11/extensions/XInput2.h>
#include <string.h>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/message_pump_x.h"
#include "ui/base/keycodes/keyboard_code_conversion_x.h"
#include "ui/base/ui_base_switches.h"
#include "ui/base/touch/touch_factory.h"
#include "ui/base/x/x11_util.h"
#include "ui/gfx/point.h"
#include "ui/gfx/monitor.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/screen.h"

// Copied from xserver-properties.h
#define AXIS_LABEL_PROP_REL_HWHEEL "Rel Horiz Wheel"
#define AXIS_LABEL_PROP_REL_WHEEL "Rel Vert Wheel"

// CMT specific timings
#define AXIS_LABEL_PROP_ABS_START_TIME "Abs Start Timestamp"
#define AXIS_LABEL_PROP_ABS_END_TIME "Abs End Timestamp"

// Fling properties
#define AXIS_LABEL_PROP_ABS_FLING_X       "Abs Fling X Velocity"
#define AXIS_LABEL_PROP_ABS_FLING_Y       "Abs Fling Y Velocity"
#define AXIS_LABEL_PROP_ABS_FLING_STATE   "Abs Fling State"

namespace {

// Scroll amount for each wheelscroll event. 53 is also the value used for GTK+.
const int kWheelScrollAmount = 53;

const int kMinWheelButton = 4;
const int kMaxWheelButton = 7;

// A class to support the detection of scroll events, using X11 valuators.
class UI_EXPORT CMTEventData {
 public:
  // Returns the ScrollEventData singleton.
  static CMTEventData* GetInstance() {
    return Singleton<CMTEventData>::get();
  }

  // Updates the list of devices.
  void UpdateDeviceList(Display* display) {
    cmt_devices_.reset();
    touchpads_.reset();
    device_to_valuators_.clear();

    int count = 0;

    // Find all the touchpad devices.
    XDeviceInfo* dev_list = XListInputDevices(display, &count);
    Atom xi_touchpad = XInternAtom(display, XI_TOUCHPAD, false);
    for (int i = 0; i < count; ++i) {
      XDeviceInfo* dev = dev_list + i;
      if (dev->type == xi_touchpad)
        touchpads_[dev_list[i].id] = true;
    }
    if (dev_list)
      XFreeDeviceList(dev_list);

    XIDeviceInfo* info_list = XIQueryDevice(display, XIAllDevices, &count);
    Atom x_axis = XInternAtom(display, AXIS_LABEL_PROP_REL_HWHEEL, false);
    Atom y_axis = XInternAtom(display, AXIS_LABEL_PROP_REL_WHEEL, false);
    Atom start_time =
        XInternAtom(display, AXIS_LABEL_PROP_ABS_START_TIME, false);
    Atom end_time = XInternAtom(display, AXIS_LABEL_PROP_ABS_END_TIME, false);
    Atom fling_vx = XInternAtom(display, AXIS_LABEL_PROP_ABS_FLING_X, false);
    Atom fling_vy = XInternAtom(display, AXIS_LABEL_PROP_ABS_FLING_Y, false);
    Atom fling_state =
        XInternAtom(display, AXIS_LABEL_PROP_ABS_FLING_STATE, false);

    for (int i = 0; i < count; ++i) {
      XIDeviceInfo* info = info_list + i;

      if (info->use != XISlavePointer && info->use != XIFloatingSlave)
        continue;

      Valuators valuators;
      bool is_cmt = false;
      for (int j = 0; j < info->num_classes; ++j) {
        if (info->classes[j]->type != XIValuatorClass)
          continue;

        XIValuatorClassInfo* v =
            reinterpret_cast<XIValuatorClassInfo*>(info->classes[j]);
        int number = v->number;
        if (number > valuators.max)
          valuators.max = number;
        if (v->label == x_axis) {
          valuators.scroll_x = number;
          is_cmt = true;
        } else if (v->label == y_axis) {
          valuators.scroll_y = number;
          is_cmt = true;
        } else if (v->label == start_time) {
          valuators.start_time = number;
          is_cmt = true;
        } else if (v->label == end_time) {
          valuators.end_time = number;
          is_cmt = true;
        } else if (v->label == fling_vx) {
          valuators.fling_vx = number;
          is_cmt = true;
        } else if (v->label == fling_vy) {
          valuators.fling_vy = number;
          is_cmt = true;
        } else if (v->label == fling_state) {
          valuators.fling_state = number;
          is_cmt = true;
        }
      }
      if (is_cmt) {
        device_to_valuators_[info->deviceid] = valuators;
        cmt_devices_[info->deviceid] = true;
      }
    }
    if (info_list)
      XIFreeDeviceInfo(info_list);
  }

  bool natural_scroll_enabled() const { return natural_scroll_enabled_; }
  void set_natural_scroll_enabled(bool enabled) {
    natural_scroll_enabled_ = enabled;
  }

  bool IsTouchpadXInputEvent(const base::NativeEvent& native_event) {
    if (native_event->type != GenericEvent)
      return false;

    XIDeviceEvent* xievent =
        static_cast<XIDeviceEvent*>(native_event->xcookie.data);
    return touchpads_[xievent->sourceid];
  }

  float GetNaturalScrollFactor(int deviceid) {
    // Natural scroll is touchpad-only.
    if (!touchpads_[deviceid])
      return -1.0f;

    return natural_scroll_enabled_ ? 1.0f : -1.0f;
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

    const int deviceid = xiev->deviceid;
    if (!cmt_devices_[deviceid])
      return false;

    const float natural_scroll_factor = GetNaturalScrollFactor(deviceid);
    const Valuators v = device_to_valuators_[deviceid];
    const bool has_x_offset = XIMaskIsSet(xiev->valuators.mask, v.scroll_x);
    const bool has_y_offset = XIMaskIsSet(xiev->valuators.mask, v.scroll_y);
    const bool is_scroll = has_x_offset || has_y_offset;

    if (!is_scroll || (!x_offset && !y_offset))
      return is_scroll;

    double* valuators = xiev->valuators.values;
    for (int i = 0; i <= v.max; ++i) {
      if (XIMaskIsSet(xiev->valuators.mask, i)) {
        if (x_offset && v.scroll_x == i)
          *x_offset = *valuators * natural_scroll_factor;
        else if (y_offset && v.scroll_y == i)
          *y_offset = *valuators * natural_scroll_factor;
        valuators++;
      }
    }

    return true;
  }

  bool GetFlingData(const XEvent& xev,
                    float* vx, float* vy,
                    bool* is_cancel) {
    XIDeviceEvent* xiev = static_cast<XIDeviceEvent*>(xev.xcookie.data);

    *vx = 0;
    *vy = 0;
    *is_cancel = false;

    const int deviceid = xiev->deviceid;
    if (!cmt_devices_[deviceid])
      return false;

    const float natural_scroll_factor = GetNaturalScrollFactor(deviceid);
    const Valuators v = device_to_valuators_[deviceid];
    if (!XIMaskIsSet(xiev->valuators.mask, v.fling_vx) ||
        !XIMaskIsSet(xiev->valuators.mask, v.fling_vy) ||
        !XIMaskIsSet(xiev->valuators.mask, v.fling_state))
      return false;

    double* valuators = xiev->valuators.values;
    for (int i = 0; i <= v.max; ++i) {
      if (XIMaskIsSet(xiev->valuators.mask, i)) {
        // Convert values to unsigned ints represending ms before storing them,
        // as that is how they were encoded before conversion to doubles.
        if (v.fling_vx == i)
          *vx = natural_scroll_factor *
                (static_cast<int>(*valuators)) / 1000.0f;
        else if (v.fling_vy == i)
          *vy = natural_scroll_factor *
                (static_cast<int>(*valuators)) / 1000.0f;
        else if (v.fling_state == i)
          *is_cancel = !!static_cast<unsigned int>(*valuators);
        valuators++;
      }
    }

    return true;
  }

  bool GetGestureTimes(const XEvent& xev,
                       double* start_time,
                       double* end_time) {
    *start_time = 0;
    *end_time = 0;

    XIDeviceEvent* xiev = static_cast<XIDeviceEvent*>(xev.xcookie.data);
    if (!cmt_devices_[xiev->deviceid])
      return false;

    Valuators v = device_to_valuators_[xiev->deviceid];
    if (!XIMaskIsSet(xiev->valuators.mask, v.start_time) ||
        !XIMaskIsSet(xiev->valuators.mask, v.end_time))
      return false;

    double* valuators = xiev->valuators.values;
    for (int i = 0; i <= v.max; ++i) {
      if (XIMaskIsSet(xiev->valuators.mask, i)) {
        // Convert values to unsigned ints represending ms before storing them,
        // as that is how they were encoded before conversion to doubles.
        if (v.start_time == i)
          *start_time =
              static_cast<double>(static_cast<unsigned int>(*valuators)) / 1000;
        else if (v.end_time == i)
          *end_time =
              static_cast<double>(static_cast<unsigned int>(*valuators)) / 1000;
        valuators++;
      }
    }

    return true;
  }

 private:
  // Requirement for Singleton
  friend struct DefaultSingletonTraits<CMTEventData>;

  struct Valuators {
    int max;
    int scroll_x;
    int scroll_y;
    int start_time;
    int end_time;
    int fling_vx;
    int fling_vy;
    int fling_state;

    Valuators()
        : max(-1),
          scroll_x(-1),
          scroll_y(-1),
          start_time(-1),
          end_time(-1),
          fling_vx(-1),
          fling_vy(-1),
          fling_state(-1) {

    }

  };

  CMTEventData() : natural_scroll_enabled_(true) {
    UpdateDeviceList(ui::GetXDisplay());
  }

  ~CMTEventData() {}

  // A quick lookup table for determining if events from the pointer device
  // should be processed.
  static const int kMaxDeviceNum = 128;
  bool natural_scroll_enabled_;
  std::bitset<kMaxDeviceNum> cmt_devices_;
  std::bitset<kMaxDeviceNum> touchpads_;
  std::map<int, Valuators> device_to_valuators_;

  DISALLOW_COPY_AND_ASSIGN(CMTEventData);
};

// A class to track current modifier state on master device. Only track ctrl,
// alt, shift and caps lock keys currently. The tracked state can then be used
// by floating device.
class UI_EXPORT XModifierStateWatcher{
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

Atom GetNoopEventAtom() {
  return XInternAtom(
      base::MessagePumpX::GetDefaultXDisplay(),
      "noop", False);
}

#if defined(USE_XI2_MT)
gfx::Point CalibrateTouchCoordinates(
    const XIDeviceEvent* xievent) {
  int x = static_cast<int>(xievent->event_x);
  int y = static_cast<int>(xievent->event_y);
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableTouchCalibration))
    return gfx::Point(x, y);
  // TODO(skuhne): Find a new home for these hardware dependent touch
  // constants.
  // Note: These values have been found to be correct for the device I was
  // testing with. I have the feeling that the DPI resolution of the bezel is
  // less then the dpi resolution over the visible part - which would explain
  // why the small value (50) is so wide compared to the entire area.
  gfx::Rect bounds = gfx::Screen::GetPrimaryMonitor().bounds_in_pixel();
  const int kLeftBorder = 50;
  const int kRightBorder = 50;
  const int kBottomBorder = 50;
  const int kTopBorder = 0;
  const int resolution_x = bounds.width();
  const int resolution_y = bounds.height();
  // The "grace area" (10% in this case) is to make it easier for the user to
  // navigate to the corner.
  const double kGraceAreaFraction = 0.1;
  // Offset the x position to the real
  x -= kLeftBorder;
  // Check if we are in the grace area of the left side.
  // Note: We might not want to do this when the gesture is locked?
  if (x < 0 && x > -kLeftBorder * kGraceAreaFraction)
    x = 0;
  // Check if we are in the grace area of the right side.
  // Note: We might not want to do this when the gesture is locked?
  if (x > resolution_x - kLeftBorder &&
      x < resolution_x - kLeftBorder + kRightBorder * kGraceAreaFraction)
    x = resolution_x - kLeftBorder;
  // Scale the screen area back to the full resolution of the screen.
  x = (x * resolution_x) / (resolution_x - (kRightBorder + kLeftBorder));
  // Offset the y position to the real
  y -= kTopBorder;
  // Check if we are in the grace area of the left side.
  // Note: We might not want to do this when the gesture is locked?
  if (y < 0 && y > -kTopBorder * kGraceAreaFraction)
    y = 0;
  // Check if we are in the grace area of the right side.
  // Note: We might not want to do this when the gesture is locked?
  if (y > resolution_y - kTopBorder &&
      y < resolution_y - kTopBorder + kBottomBorder * kGraceAreaFraction)
    y = resolution_y - kTopBorder;
  // Scale the screen area back to the full resolution of the screen.
  y = (y * resolution_y) / (resolution_y - (kBottomBorder + kTopBorder));
  // Set the modified coordinate back to the event.
  return gfx::Point(x, y);
}
#endif // defined(USE_XI2_MT)

}  // namespace

namespace ui {

void UpdateDeviceList() {
  Display* display = GetXDisplay();
  CMTEventData::GetInstance()->UpdateDeviceList(display);
  TouchFactory::GetInstance()->UpdateDeviceList(display);
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
      return ET_MOUSE_ENTERED;
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
          float vx, vy;
          bool is_cancel;
          if (GetFlingData(native_event, &vx, &vy, &is_cancel)) {
            return is_cancel ? ET_SCROLL_FLING_CANCEL : ET_SCROLL_FLING_START;
          } else if (GetScrollOffsets(native_event, NULL, NULL))
            return ET_SCROLL;
          else if (GetButtonMaskForX2Event(xievent)) {
            return ET_MOUSE_DRAGGED;
          } else
            return ET_MOUSE_MOVED;
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

      const bool touch =
          TouchFactory::GetInstance()->IsTouchDevice(xievent->sourceid);
      switch (xievent->evtype) {
        case XI_ButtonPress:
        case XI_ButtonRelease: {
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
    case GenericEvent: {
      double start, end;
      if (GetGestureTimes(native_event, &start, &end)) {
        // If the driver supports gesture times, use them.
        return base::TimeDelta::FromMicroseconds(start * 1000000);
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
        return CalibrateTouchCoordinates(xievent);
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
  int button = native_event->type == GenericEvent
    ? EventButtonFromNative(native_event) : native_event->xbutton.button;

  switch (button) {
    case 4:
      return kWheelScrollAmount;
    case 5:
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
  return CMTEventData::GetInstance()->GetScrollOffsets(
      *native_event, x_offset, y_offset);
}

bool GetFlingData(const base::NativeEvent& native_event,
                  float* vx,
                  float* vy,
                  bool* is_cancel) {
  return CMTEventData::GetInstance()->GetFlingData(
      *native_event, vx, vy, is_cancel);
}

bool GetGestureTimes(const base::NativeEvent& native_event,
                     double* start_time,
                     double* end_time) {
  return CMTEventData::GetInstance()->GetGestureTimes(
      *native_event, start_time, end_time);
}

void SetNaturalScroll(bool enabled) {
  CMTEventData::GetInstance()->set_natural_scroll_enabled(enabled);
}

bool IsNaturalScrollEnabled() {
  return CMTEventData::GetInstance()->natural_scroll_enabled();
}

bool IsTouchpadEvent(const base::NativeEvent& event) {
  return CMTEventData::GetInstance()->IsTouchpadXInputEvent(event);
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
