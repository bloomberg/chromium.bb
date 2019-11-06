// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ws/public/cpp/input_devices/input_device_client_test_api.h"

#include "services/ws/public/cpp/input_devices/input_device_client.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/devices/touchscreen_device.h"

namespace ws {

InputDeviceClientTestApi::InputDeviceClientTestApi() = default;

InputDeviceClientTestApi::~InputDeviceClientTestApi() = default;

void InputDeviceClientTestApi::NotifyObserversDeviceListsComplete() {
  if (ui::DeviceDataManager::instance_)
    ui::DeviceDataManager::instance_->NotifyObserversDeviceListsComplete();
  else
    GetInputDeviceClient()->NotifyObserversDeviceListsComplete();
}

void InputDeviceClientTestApi::
    NotifyObserversKeyboardDeviceConfigurationChanged() {
  if (ui::DeviceDataManager::instance_)
    ui::DeviceDataManager::instance_
        ->NotifyObserversKeyboardDeviceConfigurationChanged();
  else
    GetInputDeviceClient()->NotifyObserversKeyboardDeviceConfigurationChanged();
}

void InputDeviceClientTestApi::NotifyObserversStylusStateChanged(
    ui::StylusState stylus_state) {
  if (ui::DeviceDataManager::instance_) {
    ui::DeviceDataManager::instance_->NotifyObserversStylusStateChanged(
        stylus_state);
  } else {
    GetInputDeviceClient()->OnStylusStateChanged(stylus_state);
  }
}

void InputDeviceClientTestApi::
    NotifyObserversTouchscreenDeviceConfigurationChanged() {
  if (ui::DeviceDataManager::instance_) {
    ui::DeviceDataManager::instance_
        ->NotifyObserversTouchscreenDeviceConfigurationChanged();
  } else {
    GetInputDeviceClient()
        ->NotifyObserversTouchscreenDeviceConfigurationChanged();
  }
}

void InputDeviceClientTestApi::
    NotifyObserversTouchpadDeviceConfigurationChanged() {
  if (ui::DeviceDataManager::instance_) {
    ui::DeviceDataManager::instance_
        ->NotifyObserversTouchpadDeviceConfigurationChanged();
  } else {
    GetInputDeviceClient()->NotifyObserversTouchpadDeviceConfigurationChanged();
  }
}

void InputDeviceClientTestApi::OnDeviceListsComplete() {
  if (ui::DeviceDataManager::instance_)
    ui::DeviceDataManager::instance_->OnDeviceListsComplete();
  else
    GetInputDeviceClient()->OnDeviceListsComplete({}, {}, {}, {}, {}, false);
}

void InputDeviceClientTestApi::SetKeyboardDevices(
    const std::vector<ui::InputDevice>& devices) {
  if (ui::DeviceDataManager::instance_) {
    ui::DeviceDataManager::instance_->OnKeyboardDevicesUpdated(devices);
  } else {
    GetInputDeviceClient()->OnKeyboardDeviceConfigurationChanged(devices);
  }
}

void InputDeviceClientTestApi::SetMouseDevices(
    const std::vector<ui::InputDevice>& devices) {
  if (ui::DeviceDataManager::instance_) {
    ui::DeviceDataManager::instance_->OnMouseDevicesUpdated(devices);
  } else {
    GetInputDeviceClient()->OnMouseDeviceConfigurationChanged(devices);
  }
}

void InputDeviceClientTestApi::SetTouchscreenDevices(
    const std::vector<ui::TouchscreenDevice>& devices,
    bool are_touchscreen_target_displays_valid) {
  if (ui::DeviceDataManager::instance_) {
    ui::DeviceDataManager::instance_->OnTouchscreenDevicesUpdated(devices);
  } else {
    GetInputDeviceClient()->OnTouchscreenDeviceConfigurationChanged(
        devices, are_touchscreen_target_displays_valid);
  }
}

void InputDeviceClientTestApi::SetTouchpadDevices(
    const std::vector<ui::InputDevice>& devices) {
  if (ui::DeviceDataManager::instance_) {
    ui::DeviceDataManager::instance_->OnTouchpadDevicesUpdated(devices);
  } else {
    GetInputDeviceClient()->OnTouchpadDeviceConfigurationChanged(devices);
  }
}

void InputDeviceClientTestApi::SetUncategorizedDevices(
    const std::vector<ui::InputDevice>& devices) {
  if (ui::DeviceDataManager::instance_)
    ui::DeviceDataManager::instance_->OnUncategorizedDevicesUpdated(devices);
  else
    GetInputDeviceClient()->OnUncategorizedDeviceConfigurationChanged(devices);
}

InputDeviceClient* InputDeviceClientTestApi::GetInputDeviceClient() {
  if (ui::DeviceDataManager::instance_ ||
      !ui::InputDeviceManager::HasInstance())
    return nullptr;
  return static_cast<InputDeviceClient*>(ui::InputDeviceManager::GetInstance());
}

}  // namespace ws
