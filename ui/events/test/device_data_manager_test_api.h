// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_TEST_DEVICE_DATA_MANAGER_TEST_API_H_
#define UI_EVENTS_TEST_DEVICE_DATA_MANAGER_TEST_API_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "ui/events/devices/events_devices_export.h"

namespace gfx {
class Transform;
}

namespace ui {

class DeviceDataManager;
enum class StylusState;
struct InputDevice;
struct TouchscreenDevice;

namespace test {

// Test API class to access internals of the DeviceDataManager class.
class DeviceDataManagerTestAPI {
 public:
  // Constructs a test api that provides access to the global DeviceDataManager
  // instance that is accessible by DeviceDataManager::GetInstance().
  DeviceDataManagerTestAPI();
  ~DeviceDataManagerTestAPI();

  // Wrapper functions to DeviceDataManager.
  void NotifyObserversTouchscreenDeviceConfigurationChanged();
  void NotifyObserversKeyboardDeviceConfigurationChanged();
  void NotifyObserversMouseDeviceConfigurationChanged();
  void NotifyObserversTouchpadDeviceConfigurationChanged();
  void NotifyObserversDeviceListsComplete();
  void NotifyObserversStylusStateChanged(StylusState stylus_state);
  void OnDeviceListsComplete();

  // Methods for updating DeviceDataManager's device lists. Notify* methods must
  // be invoked separately to notify observers after making changes.
  void SetTouchscreenDevices(const std::vector<TouchscreenDevice>& devices);
  void SetKeyboardDevices(const std::vector<InputDevice>& devices);
  void SetMouseDevices(const std::vector<InputDevice>& devices);
  void SetTouchpadDevices(const std::vector<InputDevice>& devices);

  void UpdateTouchInfoForDisplay(int64_t target_display_id,
                                 int touch_device_id,
                                 const gfx::Transform& touch_transformer);

 private:
  DISALLOW_COPY_AND_ASSIGN(DeviceDataManagerTestAPI);
};

}  // namespace test
}  // namespace ui

#endif  // UI_EVENTS_TEST_DEVICE_DATA_MANAGER_TEST_API_H_
