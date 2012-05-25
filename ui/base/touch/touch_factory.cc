// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/touch/touch_factory.h"

#include <X11/cursorfont.h>
#include <X11/extensions/XInput.h>
#include <X11/extensions/XInput2.h>
#include <X11/extensions/XIproto.h>

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "ui/base/ui_base_switches.h"
#include "ui/base/x/x11_util.h"

namespace {

// The X cursor is hidden if it is idle for kCursorIdleSeconds seconds.
int kCursorIdleSeconds = 5;

// Given the TouchParam, return the correspoding XIValuatorClassInfo using
// the X device information through Atom name matching.
XIValuatorClassInfo* FindTPValuator(Display* display,
                                    XIDeviceInfo* info,
                                    ui::TouchFactory::TouchParam tp) {
  // Lookup table for mapping TouchParam to Atom string used in X.
  // A full set of Atom strings can be found at xserver-properties.h.
  static struct {
    ui::TouchFactory::TouchParam tp;
    const char* atom;
  } kTouchParamAtom[] = {
    { ui::TouchFactory::TP_TOUCH_MAJOR, "Abs MT Touch Major" },
    { ui::TouchFactory::TP_TOUCH_MINOR, "Abs MT Touch Minor" },
    { ui::TouchFactory::TP_ORIENTATION, "Abs MT Orientation" },
    { ui::TouchFactory::TP_PRESSURE,    "Abs MT Pressure" },
#if !defined(USE_XI2_MT)
    // For Slot ID, See this chromeos revision: http://git.chromium.org/gitweb/?
    //        p=chromiumos/overlays/chromiumos-overlay.git;
    //        a=commit;h=9164d0a75e48c4867e4ef4ab51f743ae231c059a
    { ui::TouchFactory::TP_SLOT_ID,     "Abs MT Slot ID" },
#endif
    { ui::TouchFactory::TP_TRACKING_ID, "Abs MT Tracking ID" },
    { ui::TouchFactory::TP_LAST_ENTRY, NULL },
  };

  const char* atom_tp = NULL;

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kTouchParamAtom); i++) {
    if (tp == kTouchParamAtom[i].tp) {
      atom_tp = kTouchParamAtom[i].atom;
      break;
    }
  }

  if (!atom_tp)
    return NULL;

  for (int i = 0; i < info->num_classes; i++) {
    if (info->classes[i]->type != XIValuatorClass)
      continue;
    XIValuatorClassInfo* v =
        reinterpret_cast<XIValuatorClassInfo*>(info->classes[i]);

    if (v->label) {
      ui::XScopedString atom(XGetAtomName(display, v->label));
      if (atom.string() && strcmp(atom.string(), atom_tp) == 0)
        return v;
    }
  }

  return NULL;
}

}  // namespace

namespace ui {

TouchFactory::TouchFactory()
    : is_cursor_visible_(true),
      touch_events_allowed_(false),
      cursor_timer_(),
      pointer_device_lookup_(),
      touch_device_available_(false),
      touch_device_list_(),
#if defined(USE_XI2_MT)
      min_available_slot_(0),
#endif
      slots_used_() {
#if defined(USE_AURA)
  if (!base::MessagePumpForUI::HasXInput2())
    return;
#endif

  char nodata[] = { 0, 0, 0, 0, 0, 0, 0, 0 };
  XColor black;
  black.red = black.green = black.blue = 0;
  Display* display = ui::GetXDisplay();
  Pixmap blank = XCreateBitmapFromData(display, ui::GetX11RootWindow(),
                                       nodata, 8, 8);
  invisible_cursor_ = XCreatePixmapCursor(display, blank, blank,
                                          &black, &black, 0, 0);
  arrow_cursor_ = XCreateFontCursor(display, XC_arrow);

  SetCursorVisible(false, false);
  UpdateDeviceList(display);

  // Make sure the list of devices is kept up-to-date by listening for
  // XI_HierarchyChanged event on the root window.
  unsigned char mask[XIMaskLen(XI_LASTEVENT)];
  memset(mask, 0, sizeof(mask));

  XISetMask(mask, XI_HierarchyChanged);

  XIEventMask evmask;
  evmask.deviceid = XIAllDevices;
  evmask.mask_len = sizeof(mask);
  evmask.mask = mask;
  XISelectEvents(display, ui::GetX11RootWindow(), &evmask, 1);

  CommandLine* cmdline = CommandLine::ForCurrentProcess();
  if (cmdline->HasSwitch(switches::kTouchOptimizedUI) ||
      cmdline->HasSwitch(switches::kEnableTouchEvents)) {
    touch_events_allowed_ = true;
  }
}

TouchFactory::~TouchFactory() {
#if defined(USE_AURA)
  if (!base::MessagePumpForUI::HasXInput2())
    return;
#endif

  // The XDisplay may be lost by the time we get destroyed.
  if (ui::XDisplayExists()) {
    SetCursorVisible(true, false);
    Display* display = ui::GetXDisplay();
    XFreeCursor(display, invisible_cursor_);
    XFreeCursor(display, arrow_cursor_);
  }
}

// static
TouchFactory* TouchFactory::GetInstance() {
  return Singleton<TouchFactory>::get();
}

void TouchFactory::UpdateDeviceList(Display* display) {
  // Detect touch devices.
  // NOTE: The new API for retrieving the list of devices (XIQueryDevice) does
  // not provide enough information to detect a touch device. As a result, the
  // old version of query function (XListInputDevices) is used instead.
  // If XInput2 is not supported, this will return null (with count of -1) so
  // we assume there cannot be any touch devices.
  int count = 0;
  touch_device_available_ = false;
  touch_device_lookup_.reset();
  touch_device_list_.clear();
#if !defined(USE_XI2_MT)
  XDeviceInfo* devlist = XListInputDevices(display, &count);
  for (int i = 0; i < count; i++) {
    if (devlist[i].type) {
      XScopedString devtype(XGetAtomName(display, devlist[i].type));
      if (devtype.string() && !strcmp(devtype.string(), XI_TOUCHSCREEN)) {
        touch_device_lookup_[devlist[i].id] = true;
        touch_device_list_[devlist[i].id] = true;
        touch_device_available_ = true;
      }
    }
  }
  if (devlist)
    XFreeDeviceList(devlist);
#endif

  // Instead of asking X for the list of devices all the time, let's maintain a
  // list of pointer devices we care about.
  // It should not be necessary to select for slave devices. XInput2 provides
  // enough information to the event callback to decide which slave device
  // triggered the event, thus decide whether the 'pointer event' is a
  // 'mouse event' or a 'touch event'.
  // However, on some desktops, some events from a master pointer are
  // not delivered to the client. So we select for slave devices instead.
  // If the touch device has 'GrabDevice' set and 'SendCoreEvents' unset (which
  // is possible), then the device is detected as a floating device, and a
  // floating device is not connected to a master device. So it is necessary to
  // also select on the floating devices.
  pointer_device_lookup_.reset();
  XIDeviceInfo* devices = XIQueryDevice(display, XIAllDevices, &count);
  for (int i = 0; i < count; i++) {
    XIDeviceInfo* devinfo = devices + i;
#if defined(USE_XI2_MT)
    for (int k = 0; k < devinfo->num_classes; ++k) {
      XIAnyClassInfo* xiclassinfo = devinfo->classes[k];
      if (xiclassinfo->type == XITouchClass) {
        XITouchClassInfo* tci =
            reinterpret_cast<XITouchClassInfo *>(xiclassinfo);
        // Only care direct touch device (such as touch screen) right now
        if (tci->mode == XIDirectTouch) {
          touch_device_lookup_[devinfo->deviceid] = true;
          touch_device_list_[devinfo->deviceid] = true;
          touch_device_available_ = true;
        }
      }
    }
#endif
    if (devinfo->use == XIFloatingSlave || devinfo->use == XISlavePointer)
      pointer_device_lookup_[devinfo->deviceid] = true;
  }
  if (devices)
    XIFreeDeviceInfo(devices);

  SetupValuator();
}

bool TouchFactory::ShouldProcessXI2Event(XEvent* xev) {
  DCHECK_EQ(GenericEvent, xev->type);
  XIEvent* event = static_cast<XIEvent*>(xev->xcookie.data);
  XIDeviceEvent* xiev = reinterpret_cast<XIDeviceEvent*>(event);

#if defined(USE_XI2_MT)
  if (event->evtype == XI_TouchBegin ||
      event->evtype == XI_TouchUpdate ||
      event->evtype == XI_TouchEnd) {
    return touch_events_allowed_ && IsTouchDevice(xiev->deviceid);
  }
#endif
  if (event->evtype != XI_ButtonPress &&
      event->evtype != XI_ButtonRelease &&
      event->evtype != XI_Motion)
    return true;

  if (!pointer_device_lookup_[xiev->deviceid])
    return false;

  return IsTouchDevice(xiev->deviceid) ? touch_events_allowed_ : true;
}

void TouchFactory::SetupXI2ForXWindow(Window window) {
  // Setup mask for mouse events. It is possible that a device is loaded/plugged
  // in after we have setup XInput2 on a window. In such cases, we need to
  // either resetup XInput2 for the window, so that we get events from the new
  // device, or we need to listen to events from all devices, and then filter
  // the events from uninteresting devices. We do the latter because that's
  // simpler.

  Display* display = ui::GetXDisplay();

  unsigned char mask[XIMaskLen(XI_LASTEVENT)];
  memset(mask, 0, sizeof(mask));

#if defined(USE_XI2_MT)
  XISetMask(mask, XI_TouchBegin);
  XISetMask(mask, XI_TouchUpdate);
  XISetMask(mask, XI_TouchEnd);
#endif
  XISetMask(mask, XI_ButtonPress);
  XISetMask(mask, XI_ButtonRelease);
  XISetMask(mask, XI_Motion);

  XIEventMask evmask;
  evmask.deviceid = XIAllDevices;
  evmask.mask_len = sizeof(mask);
  evmask.mask = mask;
  XISelectEvents(display, window, &evmask, 1);
  XFlush(display);
}

void TouchFactory::SetTouchDeviceList(
    const std::vector<unsigned int>& devices) {
  touch_device_lookup_.reset();
  touch_device_list_.clear();
  for (std::vector<unsigned int>::const_iterator iter = devices.begin();
       iter != devices.end(); ++iter) {
    DCHECK(*iter < touch_device_lookup_.size());
    touch_device_lookup_[*iter] = true;
    touch_device_list_[*iter] = false;
  }

  SetupValuator();
}

bool TouchFactory::IsTouchDevice(unsigned deviceid) const {
  return deviceid < touch_device_lookup_.size() ?
      touch_device_lookup_[deviceid] : false;
}

bool TouchFactory::IsMultiTouchDevice(unsigned int deviceid) const {
  return (deviceid < touch_device_lookup_.size() &&
          touch_device_lookup_[deviceid]) ?
          touch_device_list_.find(deviceid)->second :
          false;
}

#if defined(USE_XI2_MT)
int TouchFactory::GetSlotForTrackingID(uint32 tracking_id) {
  TrackingIdMap::iterator itr = tracking_id_map_.find(tracking_id);
  if (itr != tracking_id_map_.end())
    return itr->second;

  int slot = min_available_slot_;
  if (slot == kMaxTouchPoints) {
    LOG(ERROR) << "Could not find available slot for touch point";
    return 0;
  }
  SetSlotUsed(slot, true);
  tracking_id_map_.insert(std::make_pair(tracking_id, slot));

  // Updates the minium available slot ID
  while (++min_available_slot_ < kMaxTouchPoints &&
         IsSlotUsed(min_available_slot_))
    continue;

  return slot;
}

void TouchFactory::ReleaseSlotForTrackingID(uint32 tracking_id) {
  TrackingIdMap::iterator itr = tracking_id_map_.find(tracking_id);
  if (itr != tracking_id_map_.end()) {
    int slot = itr->second;
    SetSlotUsed(slot, false);
    tracking_id_map_.erase(itr);
    if (slot < min_available_slot_)
      min_available_slot_ = slot;
  } else {
    NOTREACHED() << "Cannot find slot mapping to tracking ID " << tracking_id;
  }
}
#endif

bool TouchFactory::IsSlotUsed(int slot) const {
  CHECK_LT(slot, kMaxTouchPoints);
  return slots_used_[slot];
}

void TouchFactory::SetSlotUsed(int slot, bool used) {
  CHECK_LT(slot, kMaxTouchPoints);
  slots_used_[slot] = used;
}

bool TouchFactory::GrabTouchDevices(Display* display, ::Window window) {
#if defined(USE_AURA)
  if (!base::MessagePumpForUI::HasXInput2() ||
      touch_device_list_.empty())
    return true;
#endif

  unsigned char mask[XIMaskLen(XI_LASTEVENT)];
  bool success = true;

  memset(mask, 0, sizeof(mask));
#if defined(USE_XI2_MT)
  XISetMask(mask, XI_TouchBegin);
  XISetMask(mask, XI_TouchUpdate);
  XISetMask(mask, XI_TouchEnd);
#endif
  XISetMask(mask, XI_ButtonPress);
  XISetMask(mask, XI_ButtonRelease);
  XISetMask(mask, XI_Motion);

  XIEventMask evmask;
  evmask.mask_len = sizeof(mask);
  evmask.mask = mask;
  for (std::map<int, bool>::const_iterator iter =
       touch_device_list_.begin();
       iter != touch_device_list_.end(); ++iter) {
    evmask.deviceid = iter->first;
    Status status = XIGrabDevice(display, iter->first, window, CurrentTime,
        None, GrabModeAsync, GrabModeAsync, False, &evmask);
    success = success && status == GrabSuccess;
  }

  return success;
}

bool TouchFactory::UngrabTouchDevices(Display* display) {
#if defined(USE_AURA)
  if (!base::MessagePumpForUI::HasXInput2())
    return true;
#endif

  bool success = true;
  for (std::map<int, bool>::const_iterator iter =
       touch_device_list_.begin();
       iter != touch_device_list_.end(); ++iter) {
    Status status = XIUngrabDevice(display, iter->first, CurrentTime);
    success = success && status == GrabSuccess;
  }
  return success;
}

void TouchFactory::SetCursorVisible(bool show, bool start_timer) {
  // This function may get called after the display is terminated.
  if (!ui::XDisplayExists())
    return;

#if defined(USE_AURA)
  if (!base::MessagePumpForUI::HasXInput2())
    return;
#endif

  // The cursor is going to be shown. Reset the timer for hiding it.
  if (show && start_timer) {
    cursor_timer_.Stop();
    cursor_timer_.Start(
        FROM_HERE, base::TimeDelta::FromSeconds(kCursorIdleSeconds),
        this, &TouchFactory::HideCursorForInactivity);
  } else {
    cursor_timer_.Stop();
  }

  if (show == is_cursor_visible_)
    return;

  is_cursor_visible_ = show;

  Display* display = ui::GetXDisplay();
  Window window = DefaultRootWindow(display);

  // Hide the cursor only if there's a chance that the user will be using touch
  // (i.e. if a touch device is available).
  if (is_cursor_visible_)
    XDefineCursor(display, window, arrow_cursor_);
  else if (touch_device_available_)
    XDefineCursor(display, window, invisible_cursor_);
}

bool TouchFactory::ExtractTouchParam(const XEvent& xev,
                                     TouchParam tp,
                                     float* value) {
  XIDeviceEvent* xiev = static_cast<XIDeviceEvent*>(xev.xcookie.data);
  if (xiev->sourceid >= kMaxDeviceNum)
    return false;
  int v = valuator_lookup_[xiev->sourceid][tp];
  if (v >= 0 && XIMaskIsSet(xiev->valuators.mask, v)) {
    *value = xiev->valuators.values[v];
    return true;
  }

#if defined(USE_XI2_MT)
  // With XInput2 MT, Tracking ID is provided in the detail field.
  if (tp == TP_TRACKING_ID) {
    *value = xiev->detail;
    return true;
  }
#endif

  return false;
}

bool TouchFactory::NormalizeTouchParam(unsigned int deviceid,
                                       TouchParam tp,
                                       float* value) {
  float max_value;
  float min_value;
  if (GetTouchParamRange(deviceid, tp, &min_value, &max_value)) {
    *value = (*value - min_value) / (max_value - min_value);
    DCHECK(*value >= 0.0 && *value <= 1.0);
    return true;
  }
  return false;
}

bool TouchFactory::GetTouchParamRange(unsigned int deviceid,
                                      TouchParam tp,
                                      float* min,
                                      float* max) {
  if (valuator_lookup_[deviceid][tp] >= 0) {
    *min = touch_param_min_[deviceid][tp];
    *max = touch_param_max_[deviceid][tp];
    return true;
  }
  return false;
}

bool TouchFactory::IsTouchDevicePresent() {
  return (touch_device_available_ && touch_events_allowed_);
}

void TouchFactory::SetupValuator() {
  memset(valuator_lookup_, -1, sizeof(valuator_lookup_));
  memset(touch_param_min_, 0, sizeof(touch_param_min_));
  memset(touch_param_max_, 0, sizeof(touch_param_max_));

  Display* display = ui::GetXDisplay();
  int ndevice;
  XIDeviceInfo* info_list = XIQueryDevice(display, XIAllDevices, &ndevice);

  for (int i = 0; i < ndevice; i++) {
    XIDeviceInfo* info = info_list + i;

    if (!IsTouchDevice(info->deviceid))
      continue;

    for (int j = 0; j < TP_LAST_ENTRY; j++) {
      TouchParam tp = static_cast<TouchParam>(j);
      XIValuatorClassInfo* valuator = FindTPValuator(display, info, tp);
      if (valuator) {
        valuator_lookup_[info->deviceid][j] = valuator->number;
        touch_param_min_[info->deviceid][j] = valuator->min;
        touch_param_max_[info->deviceid][j] = valuator->max;
      }
    }

#if !defined(USE_XI2_MT)
    // In order to support multi-touch with XI2.0, we need both a slot_id and
    // tracking_id valuator.  Without these we'll treat the device as a
    // single-touch device (like a mouse).
    // TODO(rbyers): Multi-touch is disabled: http://crbug.com/112329
    //if (valuator_lookup_[info->deviceid][TP_SLOT_ID] == -1 ||
    //    valuator_lookup_[info->deviceid][TP_TRACKING_ID] == -1) {
      DVLOG(1) << "Touch device " << info->deviceid <<
        " does not provide enough information for multi-touch, treating as "
        "a single-touch device.";
      touch_device_list_[info->deviceid] = false;
    //}
#endif
  }

  if (info_list)
    XIFreeDeviceInfo(info_list);
}

}  // namespace ui
