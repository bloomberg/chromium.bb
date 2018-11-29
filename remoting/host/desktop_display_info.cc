// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/desktop_display_info.h"

#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

namespace remoting {

DesktopDisplayInfo::DesktopDisplayInfo() = default;

DesktopDisplayInfo::~DesktopDisplayInfo() = default;

bool DesktopDisplayInfo::operator==(const DesktopDisplayInfo& other) {
  if (other.displays_.size() == displays_.size()) {
    for (size_t display = 0; display < displays_.size(); display++) {
      DisplayGeometry this_display = displays_[display];
      DisplayGeometry other_display = other.displays_[display];
      if (this_display.x != other_display.x ||
          this_display.y != other_display.y ||
          this_display.width != other_display.width ||
          this_display.height != other_display.height ||
          this_display.dpi != other_display.dpi ||
          this_display.bpp != other_display.bpp ||
          this_display.is_default != other_display.is_default) {
        return false;
      }
    }
    return true;
  }
  return false;
}

bool DesktopDisplayInfo::operator!=(const DesktopDisplayInfo& other) {
  return !(*this == other);
}

void DesktopDisplayInfo::LoadCurrentDisplayInfo() {
  displays_.clear();

#if defined(OS_WIN)
  BOOL enum_result = TRUE;
  for (int device_index = 0;; ++device_index) {
    DisplayGeometry info;

    DISPLAY_DEVICE device = {};
    device.cb = sizeof(device);
    enum_result = EnumDisplayDevices(NULL, device_index, &device, 0);

    // |enum_result| is 0 if we have enumerated all devices.
    if (!enum_result)
      break;

    // We only care about active displays.
    if (!(device.StateFlags & DISPLAY_DEVICE_ACTIVE))
      continue;

    info.is_default = false;
    if (device.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE)
      info.is_default = true;

    // Get additional info about device.
    DEVMODE devmode;
    devmode.dmSize = sizeof(devmode);
    EnumDisplaySettingsEx(device.DeviceName, ENUM_CURRENT_SETTINGS, &devmode,
                          0);

    info.x = devmode.dmPosition.x;
    info.y = devmode.dmPosition.y;
    info.width = devmode.dmPelsWidth;
    info.height = devmode.dmPelsHeight;
    info.dpi = devmode.dmLogPixels;
    info.bpp = devmode.dmBitsPerPel;
    displays_.push_back(info);
  }
#endif
}

}  // namespace remoting
