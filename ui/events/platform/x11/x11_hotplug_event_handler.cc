// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/platform/x11/x11_hotplug_event_handler.h"

#include <X11/extensions/XInput.h>
#include <X11/extensions/XInput2.h>

#include <algorithm>
#include <cmath>
#include <set>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/process/launch.h"
#include "base/strings/string_util.h"
#include "base/sys_info.h"
#include "ui/events/devices/device_hotplug_event_observer.h"
#include "ui/events/devices/device_util_linux.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/devices/keyboard_device.h"
#include "ui/events/devices/touchscreen_device.h"
#include "ui/gfx/x/x11_types.h"

namespace ui {

namespace {

// The name of the xinput device corresponding to the AT internal keyboard.
const char kATKeyboardName[] = "AT Translated Set 2 keyboard";

// The prefix of xinput devices corresponding to CrOS EC internal keyboards.
const char kCrosEcKeyboardPrefix[] = "cros-ec";

// Returns true if |name| is the name of a known keyboard device. Note, this may
// return false negatives.
bool IsKnownKeyboard(const std::string& name) {
  std::string lower = base::StringToLowerASCII(name);
  return lower.find("keyboard") != std::string::npos;
}

// Returns true if |name| is the name of a known internal keyboard device. Note,
// this may return false negatives.
bool IsInternalKeyboard(const std::string& name) {
  // TODO(rsadam@): Come up with a more generic way of identifying internal
  // keyboards. See crbug.com/420728.
  if (name == kATKeyboardName)
    return true;
  return name.compare(
             0u, strlen(kCrosEcKeyboardPrefix), kCrosEcKeyboardPrefix) == 0;
}

// Returns true if |name| is the name of a known XTEST device. Note, this may
// return false negatives.
bool IsTestKeyboard(const std::string& name) {
  return name.find("XTEST") != std::string::npos;
}

// We consider the touchscreen to be internal if it is an I2c device.
// With the device id, we can query X to get the device's dev input
// node eventXXX. Then we search all the dev input nodes registered
// by I2C devices to see if we can find eventXXX.
bool IsTouchscreenInternal(XDisplay* dpy, int device_id) {
#if !defined(CHROMEOS)
  return false;
#else
  if (!base::SysInfo::IsRunningOnChromeOS())
    return false;
#endif

  // Input device has a property "Device Node" pointing to its dev input node,
  // e.g.   Device Node (250): "/dev/input/event8"
  Atom device_node = XInternAtom(dpy, "Device Node", False);
  if (device_node == None)
    return false;

  Atom actual_type;
  int actual_format;
  unsigned long nitems, bytes_after;
  unsigned char* data;
  XDevice* dev = XOpenDevice(dpy, device_id);
  if (!dev)
    return false;

  if (XGetDeviceProperty(dpy,
                         dev,
                         device_node,
                         0,
                         1000,
                         False,
                         AnyPropertyType,
                         &actual_type,
                         &actual_format,
                         &nitems,
                         &bytes_after,
                         &data) != Success) {
    XCloseDevice(dpy, dev);
    return false;
  }
  base::FilePath dev_node_path(reinterpret_cast<char*>(data));
  XFree(data);
  XCloseDevice(dpy, dev);

  return ui::IsTouchscreenInternal(dev_node_path);
}

}  // namespace

X11HotplugEventHandler::X11HotplugEventHandler(
    DeviceHotplugEventObserver* delegate)
    : delegate_(delegate) {
}

X11HotplugEventHandler::~X11HotplugEventHandler() {
}

void X11HotplugEventHandler::OnHotplugEvent() {
  const XIDeviceList& device_list =
      DeviceListCacheX11::GetInstance()->GetXI2DeviceList(gfx::GetXDisplay());
  HandleTouchscreenDevices(device_list);
  HandleKeyboardDevices(device_list);
}

void X11HotplugEventHandler::HandleKeyboardDevices(
    const XIDeviceList& x11_devices) {
  std::vector<KeyboardDevice> devices;

  for (int i = 0; i < x11_devices.count; i++) {
    if (!x11_devices[i].enabled || x11_devices[i].use != XISlaveKeyboard)
      continue;  // Assume all keyboards are keyboard slaves
    std::string device_name(x11_devices[i].name);
    base::TrimWhitespaceASCII(device_name, base::TRIM_TRAILING, &device_name);
    if (IsTestKeyboard(device_name))
      continue;  // Skip test devices.
    InputDeviceType type;
    if (IsInternalKeyboard(device_name)) {
      type = InputDeviceType::INPUT_DEVICE_INTERNAL;
    } else if (IsKnownKeyboard(device_name)) {
      type = InputDeviceType::INPUT_DEVICE_EXTERNAL;
    } else {
      type = InputDeviceType::INPUT_DEVICE_UNKNOWN;
    }
    devices.push_back(
        KeyboardDevice(x11_devices[i].deviceid, type, device_name));
  }
  delegate_->OnKeyboardDevicesUpdated(devices);
}

void X11HotplugEventHandler::HandleTouchscreenDevices(
    const XIDeviceList& x11_devices) {
  std::vector<TouchscreenDevice> devices;
  Display* display = gfx::GetXDisplay();
  Atom valuator_x = XInternAtom(display, "Abs MT Position X", False);
  Atom valuator_y = XInternAtom(display, "Abs MT Position Y", False);
  if (valuator_x == None || valuator_y == None)
    return;

  std::set<int> no_match_touchscreen;
  for (int i = 0; i < x11_devices.count; i++) {
    if (!x11_devices[i].enabled || x11_devices[i].use != XIFloatingSlave)
      continue;  // Assume all touchscreens are floating slaves

    double max_x = -1.0;
    double max_y = -1.0;
    bool is_direct_touch = false;

    for (int j = 0; j < x11_devices[i].num_classes; j++) {
      XIAnyClassInfo* class_info = x11_devices[i].classes[j];

      if (class_info->type == XIValuatorClass) {
        XIValuatorClassInfo* valuator_info =
            reinterpret_cast<XIValuatorClassInfo*>(class_info);

        if (valuator_x == valuator_info->label) {
          // Ignore X axis valuator with unexpected properties
          if (valuator_info->number == 0 && valuator_info->mode == Absolute &&
              valuator_info->min == 0.0) {
            max_x = valuator_info->max;
          }
        } else if (valuator_y == valuator_info->label) {
          // Ignore Y axis valuator with unexpected properties
          if (valuator_info->number == 1 && valuator_info->mode == Absolute &&
              valuator_info->min == 0.0) {
            max_y = valuator_info->max;
          }
        }
      }
#if defined(USE_XI2_MT)
      if (class_info->type == XITouchClass) {
        XITouchClassInfo* touch_info =
            reinterpret_cast<XITouchClassInfo*>(class_info);
        is_direct_touch = touch_info->mode == XIDirectTouch;
      }
#endif
    }

    // Touchscreens should have absolute X and Y axes, and be direct touch
    // devices.
    if (max_x > 0.0 && max_y > 0.0 && is_direct_touch) {
      InputDeviceType type =
          IsTouchscreenInternal(display, x11_devices[i].deviceid)
              ? InputDeviceType::INPUT_DEVICE_INTERNAL
              : InputDeviceType::INPUT_DEVICE_EXTERNAL;
      std::string name(x11_devices[i].name);
      // |max_x| and |max_y| are inclusive values, so we need to add 1 to get
      // the size.
      devices.push_back(TouchscreenDevice(
          x11_devices[i].deviceid,
          type,
          name,
          gfx::Size(max_x + 1, max_y + 1)));
    }
  }

  delegate_->OnTouchscreenDevicesUpdated(devices);
}

}  // namespace ui
