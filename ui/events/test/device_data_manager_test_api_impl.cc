// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/test/device_data_manager_test_api.h"

#include "ui/events/devices/device_data_manager.h"
#include "ui/gfx/transform.h"

namespace ui {
namespace test {

DeviceDataManagerTestAPI::DeviceDataManagerTestAPI() {
}

DeviceDataManagerTestAPI::~DeviceDataManagerTestAPI() {
}

void DeviceDataManagerTestAPI::
    NotifyObserversTouchscreenDeviceConfigurationChanged() {
  DeviceDataManager::GetInstance()
      ->NotifyObserversTouchscreenDeviceConfigurationChanged();
}

void DeviceDataManagerTestAPI::
    NotifyObserversKeyboardDeviceConfigurationChanged() {
  DeviceDataManager::GetInstance()
      ->NotifyObserversKeyboardDeviceConfigurationChanged();
}

void DeviceDataManagerTestAPI::
    NotifyObserversMouseDeviceConfigurationChanged() {
  DeviceDataManager::GetInstance()
      ->NotifyObserversMouseDeviceConfigurationChanged();
}

void DeviceDataManagerTestAPI::
    NotifyObserversTouchpadDeviceConfigurationChanged() {
  DeviceDataManager::GetInstance()
      ->NotifyObserversTouchpadDeviceConfigurationChanged();
}

void DeviceDataManagerTestAPI::NotifyObserversDeviceListsComplete() {
  DeviceDataManager::GetInstance()->NotifyObserversDeviceListsComplete();
}

void DeviceDataManagerTestAPI::NotifyObserversStylusStateChanged(
    StylusState stylus_state) {
  DeviceDataManager::GetInstance()->NotifyObserversStylusStateChanged(
      stylus_state);
}

void DeviceDataManagerTestAPI::OnDeviceListsComplete() {
  DeviceDataManager::GetInstance()->OnDeviceListsComplete();
}

void DeviceDataManagerTestAPI::SetTouchscreenDevices(
    const std::vector<TouchscreenDevice>& devices) {
  DeviceDataManager::GetInstance()->touchscreen_devices_ = devices;
}

void DeviceDataManagerTestAPI::SetKeyboardDevices(
    const std::vector<InputDevice>& devices) {
  DeviceDataManager::GetInstance()->keyboard_devices_ = devices;
}

void DeviceDataManagerTestAPI::SetMouseDevices(
    const std::vector<InputDevice>& devices) {
  DeviceDataManager::GetInstance()->mouse_devices_ = devices;
}

void DeviceDataManagerTestAPI::SetTouchpadDevices(
    const std::vector<InputDevice>& devices) {
  DeviceDataManager::GetInstance()->touchpad_devices_ = devices;
}

void DeviceDataManagerTestAPI::UpdateTouchInfoForDisplay(
    int64_t target_display_id,
    int touch_device_id,
    const gfx::Transform& touch_transformer) {
  DeviceDataManager::GetInstance()->UpdateTouchInfoForDisplay(
      target_display_id, touch_device_id, touch_transformer);
}
}  // namespace test
}  // namespace ui
