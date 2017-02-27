// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/manager/chromeos/touchscreen_util.h"

#include <set>

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "ui/events/devices/input_device.h"

namespace display {

namespace {

using DisplayInfoList = std::vector<ManagedDisplayInfo*>;
using DeviceList = std::vector<const ui::TouchscreenDevice*>;

// Helper method to associate |display| and |device|.
void Associate(ManagedDisplayInfo* display,
               const ui::TouchscreenDevice* device) {
  display->AddInputDevice(device->id);
  display->set_touch_support(Display::TOUCH_SUPPORT_AVAILABLE);
}

// Returns true if |path| is likely a USB device.
bool IsDeviceConnectedViaUsb(const base::FilePath& path) {
  std::vector<base::FilePath::StringType> components;
  path.GetComponents(&components);

  for (const auto& component : components) {
    if (base::StartsWith(component, "usb",
                         base::CompareCase::INSENSITIVE_ASCII))
      return true;
  }

  return false;
}

// Returns the UDL association score between |display| and |device|. A score <=
// 0 means that there is no association.
int GetUdlAssociationScore(const ManagedDisplayInfo* display,
                           const ui::TouchscreenDevice* device) {
  // If the devices are not both connected via USB, then there cannot be a UDL
  // association score.
  if (!IsDeviceConnectedViaUsb(display->sys_path()) ||
      !IsDeviceConnectedViaUsb(device->sys_path))
    return 0;

  // The association score is simply the number of prefix path components that
  // sysfs paths have in common.
  std::vector<base::FilePath::StringType> display_components;
  std::vector<base::FilePath::StringType> device_components;
  display->sys_path().GetComponents(&display_components);
  device->sys_path.GetComponents(&device_components);

  std::size_t largest_idx = 0;
  while (largest_idx < display_components.size() &&
         largest_idx < device_components.size() &&
         display_components[largest_idx] == device_components[largest_idx]) {
    ++largest_idx;
  }
  return largest_idx;
}

// Tries to find a UDL device that best matches |display|. Returns nullptr
// if one is not found.
const ui::TouchscreenDevice* GuessBestUdlDevice(
    const ManagedDisplayInfo* display,
    const DeviceList& devices) {
  int best_score = 0;
  const ui::TouchscreenDevice* best_device = nullptr;

  for (const ui::TouchscreenDevice* device : devices) {
    int score = GetUdlAssociationScore(display, device);
    if (score > best_score) {
      best_score = score;
      best_device = device;
    }
  }

  return best_device;
}

void AssociateUdlDevices(DisplayInfoList* displays, DeviceList* devices) {
  VLOG(2) << "Trying to match udl devices (" << displays->size()
          << " displays and " << devices->size() << " devices to match)";

  DisplayInfoList::iterator display_it = displays->begin();
  while (display_it != displays->end()) {
    ManagedDisplayInfo* display = *display_it;
    const ui::TouchscreenDevice* device = GuessBestUdlDevice(display, *devices);

    if (device) {
      VLOG(2) << "=> Matched device " << device->name << " to display "
              << display->name()
              << " (score=" << GetUdlAssociationScore(display, device) << ")";
      Associate(display, device);

      display_it = displays->erase(display_it);
      devices->erase(std::find(devices->begin(), devices->end(), device));

      continue;
    }

    ++display_it;
  }
}

// Returns true if |display| is internal.
bool IsInternalDisplay(const ManagedDisplayInfo* display) {
  return Display::IsInternalDisplayId(display->id());
}

// Returns true if |device| is internal.
bool IsInternalDevice(const ui::TouchscreenDevice* device) {
  return device->type == ui::InputDeviceType::INPUT_DEVICE_INTERNAL;
}

void AssociateInternalDevices(DisplayInfoList* displays, DeviceList* devices) {
  VLOG(2) << "Trying to match internal devices (" << displays->size()
          << " displays and " << devices->size() << " devices to match)";

  // Internal device assocation has a couple of gotchas:
  // - There can be internal devices but no internal display, or visa-versa.
  // - There can be multiple internal devices matching one internal display. We
  //   assume there is at most one internal display.
  // - All internal devices must be removed from |displays| and |devices| after
  //   this function has returned, since an internal device can never be
  //   associated with an external device.

  // Capture the internal display reference as we remove it from |displays|.
  ManagedDisplayInfo* internal_display = nullptr;
  DisplayInfoList::iterator display_it =
      std::find_if(displays->begin(), displays->end(), &IsInternalDisplay);
  if (display_it != displays->end()) {
    internal_display = *display_it;
    displays->erase(display_it);
  }

  bool matched = false;

  // Remove all internal devices from |devices|. If we have an internal display,
  // then associate the device with the display before removing it.
  DeviceList::iterator device_it = devices->begin();
  while (device_it != devices->end()) {
    const ui::TouchscreenDevice* internal_device = *device_it;

    // Not an internal device, skip it.
    if (!IsInternalDevice(internal_device)) {
      ++device_it;
      continue;
    }

    if (internal_display) {
      VLOG(2) << "=> Matched device " << internal_device->name << " to display "
              << internal_display->name();
      Associate(internal_display, internal_device);
      matched = true;
    } else {
      VLOG(2) << "=> Removing internal device " << internal_device->name;
    }

    device_it = devices->erase(device_it);
  }

  if (!matched && internal_display)
    VLOG(2) << "=> Removing internal display " << internal_display->name();
}

void AssociateSameSizeDevices(DisplayInfoList* displays, DeviceList* devices) {
  // Associate screens/displays with the same size.
  VLOG(2) << "Trying to match same-size devices (" << displays->size()
          << " displays and " << devices->size() << " devices to match)";

  DisplayInfoList::iterator display_it = displays->begin();
  while (display_it != displays->end()) {
    ManagedDisplayInfo* display = *display_it;
    const gfx::Size native_size = display->GetNativeModeSize();

    // Try to find an input device with roughly the same size as the display.
    DeviceList::iterator device_it = std::find_if(
        devices->begin(), devices->end(),
        [&native_size](const ui::TouchscreenDevice* device) {
          // Allow 1 pixel difference between screen and touchscreen
          // resolutions. Because in some cases for monitor resolution
          // 1024x768 touchscreen's resolution would be 1024x768, but for
          // some 1023x767. It really depends on touchscreen's firmware
          // configuration.
          return std::abs(native_size.width() - device->size.width()) <= 1 &&
                 std::abs(native_size.height() - device->size.height()) <= 1;
        });

    if (device_it != devices->end()) {
      const ui::TouchscreenDevice* device = *device_it;
      VLOG(2) << "=> Matched device " << device->name << " to display "
              << display->name() << " (display_size: " << native_size.ToString()
              << ", device_size: " << device->size.ToString() << ")";
      Associate(display, device);

      display_it = displays->erase(display_it);
      device_it = devices->erase(device_it);
      continue;
    }

    // Didn't find an input device. Skip this display.
    ++display_it;
  }
}

void AssociateToSingleDisplay(DisplayInfoList* displays, DeviceList* devices) {
  // If there is only one display left, then we should associate all input
  // devices with it.

  VLOG(2) << "Trying to match to single display (" << displays->size()
          << " displays and " << devices->size() << " devices to match)";

  // We only associate to one display.
  if (displays->size() != 1 || devices->empty())
    return;

  ManagedDisplayInfo* display = *displays->begin();
  for (const ui::TouchscreenDevice* device : *devices) {
    VLOG(2) << "=> Matched device " << device->name << " to display "
            << display->name();
    Associate(display, device);
  }

  displays->clear();
  devices->clear();
}

}  // namespace

void AssociateTouchscreens(
    std::vector<ManagedDisplayInfo>* all_displays,
    const std::vector<ui::TouchscreenDevice>& all_devices) {
  // |displays| and |devices| contain pointers directly to the values stored
  // inside of |all_displays| and |all_devices|. When a display or input device
  // has been associated, it is removed from the |displays| or |devices| list.

  // Construct our initial set of display/devices that we will process.
  DisplayInfoList displays;
  for (ManagedDisplayInfo& display : *all_displays) {
    display.ClearInputDevices();

    if (display.GetNativeModeSize().IsEmpty()) {
      VLOG(2) << "Will not match display " << display.id()
              << " since it doesn't have a native mode";
      continue;
    }
    displays.push_back(&display);
  }

  // Construct initial set of devices.
  DeviceList devices;
  for (const ui::TouchscreenDevice& device : all_devices)
    devices.push_back(&device);

  for (const ManagedDisplayInfo* display : displays) {
    VLOG(2) << "Received display " << display->name()
            << " (size: " << display->GetNativeModeSize().ToString()
            << ", sys_path: " << display->sys_path().LossyDisplayName() << ")";
  }
  for (const ui::TouchscreenDevice* device : devices) {
    VLOG(2) << "Received device " << device->name
            << " (size: " << device->size.ToString()
            << ", sys_path: " << device->sys_path.LossyDisplayName() << ")";
  }

  AssociateInternalDevices(&displays, &devices);
  AssociateUdlDevices(&displays, &devices);
  AssociateSameSizeDevices(&displays, &devices);
  AssociateToSingleDisplay(&displays, &devices);

  for (const ManagedDisplayInfo* display : displays)
    VLOG(2) << "Unmatched display " << display->name();
  for (const ui::TouchscreenDevice* device : devices)
    LOG(WARNING) << "Unmatched device " << device->name;
}

}  // namespace display
