// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/test/device_data_manager_test_api.h"

#include "base/logging.h"

namespace ui {
namespace test {

DeviceDataManagerTestAPI::DeviceDataManagerTestAPI() {
}

DeviceDataManagerTestAPI::~DeviceDataManagerTestAPI() {
}

void DeviceDataManagerTestAPI::
    NotifyObserversTouchscreenDeviceConfigurationChanged() {
  NOTREACHED();
}

void DeviceDataManagerTestAPI::
    NotifyObserversKeyboardDeviceConfigurationChanged() {
  NOTREACHED();
}

void DeviceDataManagerTestAPI::
    NotifyObserversMouseDeviceConfigurationChanged() {
  NOTREACHED();
}

void DeviceDataManagerTestAPI::
    NotifyObserversTouchpadDeviceConfigurationChanged() {
  NOTREACHED();
}

void DeviceDataManagerTestAPI::NotifyObserversDeviceListsComplete() {
  NOTREACHED();
}

void DeviceDataManagerTestAPI::NotifyObserversStylusStateChanged(
    StylusState stylus_state) {
  NOTREACHED();
}

void DeviceDataManagerTestAPI::OnDeviceListsComplete() {
  NOTREACHED();
}

void DeviceDataManagerTestAPI::SetTouchscreenDevices(
    const std::vector<TouchscreenDevice>& devices) {
  NOTREACHED();
}

void DeviceDataManagerTestAPI::SetKeyboardDevices(
    const std::vector<InputDevice>& devices) {
  NOTREACHED();
}

void DeviceDataManagerTestAPI::SetMouseDevices(
    const std::vector<InputDevice>& devices) {
  NOTREACHED();
}

void DeviceDataManagerTestAPI::SetTouchpadDevices(
    const std::vector<InputDevice>& devices) {
  NOTREACHED();
}

}  // namespace test
}  // namespace ui
