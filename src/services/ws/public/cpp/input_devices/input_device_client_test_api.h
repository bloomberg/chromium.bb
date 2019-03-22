// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WS_PUBLIC_CPP_INPUT_DEVICES_INPUT_DEVICE_CLIENT_TEST_API_H_
#define SERVICES_WS_PUBLIC_CPP_INPUT_DEVICES_INPUT_DEVICE_CLIENT_TEST_API_H_

#include <vector>

#include "base/macros.h"

namespace ui {
struct InputDevice;
struct TouchscreenDevice;

enum class StylusState;
}  // namespace ui

namespace ws {

class InputDeviceClient;

// Test interfaces for calling private functions of InputDeviceClient. Until
// Chrome OS has been converted to InputDeviceClient this uses DeviceDataManager
// if it exists.
//
// Usage depends upon exactly what you want to do, but often times you will
// configure the set of devices (keyboards and/or touchscreens) and then call
// OnDeviceListsComplete().
class InputDeviceClientTestApi {
 public:
  InputDeviceClientTestApi();
  ~InputDeviceClientTestApi();

  void NotifyObserversDeviceListsComplete();
  void NotifyObserversKeyboardDeviceConfigurationChanged();
  void NotifyObserversStylusStateChanged(ui::StylusState stylus_state);
  void NotifyObserversTouchscreenDeviceConfigurationChanged();
  void NotifyObserversTouchpadDeviceConfigurationChanged();
  void OnDeviceListsComplete();

  void SetKeyboardDevices(const std::vector<ui::InputDevice>& devices);
  void SetMouseDevices(const std::vector<ui::InputDevice>& devices);
  void SetTouchpadDevices(const std::vector<ui::InputDevice>& devices);

  // |are_touchscreen_target_displays_valid| is only applicable to
  // InputDeviceClient. See
  // InputDeviceClient::OnTouchscreenDeviceConfigurationChanged() for details.
  void SetTouchscreenDevices(
      const std::vector<ui::TouchscreenDevice>& devices,
      bool are_touchscreen_target_displays_valid = false);

  InputDeviceClient* GetInputDeviceClient();

 private:
  DISALLOW_COPY_AND_ASSIGN(InputDeviceClientTestApi);
};

}  // namespace ws

#endif  // SERVICES_WS_PUBLIC_CPP_INPUT_DEVICES_INPUT_DEVICE_CLIENT_TEST_API_H_
