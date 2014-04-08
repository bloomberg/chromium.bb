// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/chromeos/x11/touchscreen_delegate_x11.h"

#include <X11/extensions/XInput.h>
#include <X11/extensions/XInput2.h>

#include <cmath>
#include <set>

#include "base/message_loop/message_pump_x11.h"
#include "ui/display/chromeos/display_mode.h"
#include "ui/display/chromeos/display_snapshot.h"

namespace ui {

TouchscreenDelegateX11::TouchscreenDelegateX11()
    : display_(base::MessagePumpX11::GetDefaultXDisplay()) {}

TouchscreenDelegateX11::~TouchscreenDelegateX11() {}

void TouchscreenDelegateX11::AssociateTouchscreens(
    DisplayConfigurator::DisplayStateList* outputs) {
  int ndevices = 0;
  Atom valuator_x = XInternAtom(display_, "Abs MT Position X", False);
  Atom valuator_y = XInternAtom(display_, "Abs MT Position Y", False);
  if (valuator_x == None || valuator_y == None)
    return;

  std::set<int> no_match_touchscreen;
  XIDeviceInfo* info = XIQueryDevice(display_, XIAllDevices, &ndevices);
  for (int i = 0; i < ndevices; i++) {
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

    // Touchscreens should have absolute X and Y axes,
    // and be direct touch devices.
    if (width > 0.0 && height > 0.0 && is_direct_touch) {
      size_t k = 0;
      for (; k < outputs->size(); k++) {
        DisplayConfigurator::DisplayState* output = &(*outputs)[k];
        if (output->touch_device_id != None)
          continue;

        const DisplayMode* mode_info = output->display->native_mode();
        if (!mode_info)
          continue;

        // Allow 1 pixel difference between screen and touchscreen
        // resolutions.  Because in some cases for monitor resolution
        // 1024x768 touchscreen's resolution would be 1024x768, but for
        // some 1023x767.  It really depends on touchscreen's firmware
        // configuration.
        if (std::abs(mode_info->size().width() - width) <= 1.0 &&
            std::abs(mode_info->size().height() - height) <= 1.0) {
          output->touch_device_id = info[i].deviceid;

          VLOG(2) << "Found touchscreen for output #" << k << " id "
                  << output->touch_device_id << " width " << width << " height "
                  << height;
          break;
        }
      }

      if (k == outputs->size()) {
        no_match_touchscreen.insert(info[i].deviceid);
        VLOG(2) << "No matching output for touchscreen"
                << " id " << info[i].deviceid << " width " << width
                << " height " << height;
      }
    }
  }

  // Sometimes we can't find a matching screen for the touchscreen, e.g.
  // due to the touchscreen's reporting range having no correlation with the
  // screen's resolution. In this case, we arbitrarily assign unmatched
  // touchscreens to unmatched screens.
  for (std::set<int>::iterator it = no_match_touchscreen.begin();
       it != no_match_touchscreen.end();
       it++) {
    for (size_t i = 0; i < outputs->size(); i++) {
      if ((*outputs)[i].display->type() != DISPLAY_CONNECTION_TYPE_INTERNAL &&
          (*outputs)[i].display->native_mode() != NULL &&
          (*outputs)[i].touch_device_id == None) {
        (*outputs)[i].touch_device_id = *it;
        VLOG(2) << "Arbitrarily matching touchscreen "
                << (*outputs)[i].touch_device_id << " to output #" << i;
        break;
      }
    }
  }

  XIFreeDeviceInfo(info);
}

void TouchscreenDelegateX11::ConfigureCTM(
    int touch_device_id,
    const DisplayConfigurator::CoordinateTransformation& ctm) {
  VLOG(1) << "ConfigureCTM: id=" << touch_device_id << " scale=" << ctm.x_scale
          << "x" << ctm.y_scale << " offset=(" << ctm.x_offset << ", "
          << ctm.y_offset << ")";
  int ndevices = 0;
  XIDeviceInfo* info = XIQueryDevice(display_, touch_device_id, &ndevices);
  Atom prop = XInternAtom(display_, "Coordinate Transformation Matrix", False);
  Atom float_atom = XInternAtom(display_, "FLOAT", False);
  if (ndevices == 1 && prop != None && float_atom != None) {
    Atom type;
    int format;
    unsigned long num_items;
    unsigned long bytes_after;
    unsigned char* data = NULL;
    // Verify that the property exists with correct format, type, etc.
    int status = XIGetProperty(display_,
                               info->deviceid,
                               prop,
                               0,
                               0,
                               False,
                               AnyPropertyType,
                               &type,
                               &format,
                               &num_items,
                               &bytes_after,
                               &data);
    if (data)
      XFree(data);
    if (status == Success && type == float_atom && format == 32) {
      float value[3][3] = {
        { ctm.x_scale,         0.0, ctm.x_offset },
        {         0.0, ctm.y_scale, ctm.y_offset },
        {         0.0,         0.0,          1.0 }
      };
      XIChangeProperty(display_,
                       info->deviceid,
                       prop,
                       type,
                       format,
                       PropModeReplace,
                       reinterpret_cast<unsigned char*>(value),
                       9);
    }
  }
  XIFreeDeviceInfo(info);
}

}  // namespace ui
