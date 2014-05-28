// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/chromeos/x11/touchscreen_device_manager_x11.h"

#include <X11/extensions/XInput.h>
#include <X11/extensions/XInput2.h>

#include <cmath>
#include <set>

#include "ui/gfx/x/x11_types.h"

namespace ui {

TouchscreenDeviceManagerX11::TouchscreenDeviceManagerX11()
    : display_(gfx::GetXDisplay()) {}

TouchscreenDeviceManagerX11::~TouchscreenDeviceManagerX11() {}

std::vector<TouchscreenDevice> TouchscreenDeviceManagerX11::GetDevices() {
  std::vector<TouchscreenDevice> devices;
  int num_devices = 0;
  Atom valuator_x = XInternAtom(display_, "Abs MT Position X", False);
  Atom valuator_y = XInternAtom(display_, "Abs MT Position Y", False);
  if (valuator_x == None || valuator_y == None)
    return devices;

  std::set<int> no_match_touchscreen;
  XIDeviceInfo* info = XIQueryDevice(display_, XIAllDevices, &num_devices);
  for (int i = 0; i < num_devices; i++) {
    if (!info[i].enabled || info[i].use != XIFloatingSlave)
      continue;  // Assume all touchscreens are floating slaves

    double width = -1.0;
    double height = -1.0;
    bool is_direct_touch = false;

    for (int j = 0; j < info[i].num_classes; j++) {
      XIAnyClassInfo* class_info = info[i].classes[j];

      if (class_info->type == XIValuatorClass) {
        XIValuatorClassInfo* valuator_info =
            reinterpret_cast<XIValuatorClassInfo*>(class_info);

        if (valuator_x == valuator_info->label) {
          // Ignore X axis valuator with unexpected properties
          if (valuator_info->number == 0 && valuator_info->mode == Absolute &&
              valuator_info->min == 0.0) {
            width = valuator_info->max;
          }
        } else if (valuator_y == valuator_info->label) {
          // Ignore Y axis valuator with unexpected properties
          if (valuator_info->number == 1 && valuator_info->mode == Absolute &&
              valuator_info->min == 0.0) {
            height = valuator_info->max;
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
    if (width > 0.0 && height > 0.0 && is_direct_touch) {
      devices.push_back(TouchscreenDevice(info[i].deviceid,
                                          gfx::Size(width, height)));
    }
  }

  XIFreeDeviceInfo(info);
  return devices;
}

}  // namespace ui
