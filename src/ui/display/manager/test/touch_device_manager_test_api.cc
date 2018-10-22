// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/manager/test/touch_device_manager_test_api.h"

#include "ui/display/manager/managed_display_info.h"
#include "ui/events/devices/touchscreen_device.h"

namespace display {
namespace test {

TouchDeviceManagerTestApi::TouchDeviceManagerTestApi(
    TouchDeviceManager* touch_device_manager)
    : touch_device_manager_(touch_device_manager) {
  DCHECK(touch_device_manager_);
}

TouchDeviceManagerTestApi::~TouchDeviceManagerTestApi() {}

void TouchDeviceManagerTestApi::Associate(ManagedDisplayInfo* display_info,
                                          const ui::TouchscreenDevice& device) {
  touch_device_manager_->Associate(display_info, device);
}

void TouchDeviceManagerTestApi::Associate(int64_t display_id,
                                          const ui::TouchscreenDevice& device) {
  touch_device_manager_
      ->active_touch_associations_[TouchDeviceIdentifier::FromDevice(device)] =
      display_id;
}

std::size_t TouchDeviceManagerTestApi::GetTouchDeviceCount(
    const ManagedDisplayInfo& info) const {
  return touch_device_manager_->GetAssociatedTouchDevicesForDisplay(info.id())
      .size();
}

bool TouchDeviceManagerTestApi::AreAssociated(
    const ManagedDisplayInfo& info,
    const ui::TouchscreenDevice& device) const {
  return touch_device_manager_->DisplayHasTouchDevice(
      info.id(), TouchDeviceIdentifier::FromDevice(device));
}

void TouchDeviceManagerTestApi::ResetTouchDeviceManager() {
  touch_device_manager_->RegisterTouchAssociations(
      TouchDeviceManager::TouchAssociationMap(),
      TouchDeviceManager::PortAssociationMap());
}

}  // namespace test
}  // namespace display
