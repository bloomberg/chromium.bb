// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_DEVICES_DEVICE_DATA_MANAGER_H_
#define UI_EVENTS_DEVICES_DEVICE_DATA_MANAGER_H_

#include <stdint.h>

#include <vector>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "ui/events/devices/device_hotplug_event_observer.h"
#include "ui/events/devices/events_devices_export.h"
#include "ui/events/devices/keyboard_device.h"
#include "ui/events/devices/touchscreen_device.h"
#include "ui/gfx/transform.h"

namespace ui {

class InputDeviceEventObserver;

// Keeps track of device mappings and event transformations.
class EVENTS_DEVICES_EXPORT DeviceDataManager
    : public DeviceHotplugEventObserver {
 public:
  static const int kMaxDeviceNum = 128;
  ~DeviceDataManager() override;

  static void CreateInstance();
  static DeviceDataManager* GetInstance();
  static bool HasInstance();

  void ClearTouchDeviceAssociations();
  void UpdateTouchInfoForDisplay(int64_t target_display_id,
                                 unsigned int touch_device_id,
                                 const gfx::Transform& touch_transformer);
  void ApplyTouchTransformer(unsigned int touch_device_id, float* x, float* y);

  // Gets the display that touches from |touch_device_id| should be sent to.
  int64_t GetTargetDisplayForTouchDevice(unsigned int touch_device_id) const;

  void UpdateTouchRadiusScale(unsigned int touch_device_id, double scale);
  void ApplyTouchRadiusScale(unsigned int touch_device_id, double* radius);

  const std::vector<TouchscreenDevice>& touchscreen_devices() const {
    return touchscreen_devices_;
  }

  const std::vector<KeyboardDevice>& keyboard_devices() const {
    return keyboard_devices_;
  }

  void AddObserver(InputDeviceEventObserver* observer);
  void RemoveObserver(InputDeviceEventObserver* observer);

 protected:
  DeviceDataManager();

  static DeviceDataManager* instance();

  // DeviceHotplugEventObserver:
  void OnTouchscreenDevicesUpdated(
      const std::vector<TouchscreenDevice>& devices) override;
  void OnKeyboardDevicesUpdated(
      const std::vector<KeyboardDevice>& devices) override;
  void OnMouseDevicesUpdated(
      const std::vector<InputDevice>& devices) override;
  void OnTouchpadDevicesUpdated(
      const std::vector<InputDevice>& devices) override;

 private:
  static DeviceDataManager* instance_;

  bool IsTouchDeviceIdValid(unsigned int touch_device_id) const;

  double touch_radius_scale_map_[kMaxDeviceNum];

  // Index table to find the target display id for a touch device.
  int64_t touch_device_to_target_display_map_[kMaxDeviceNum];
  // Index table to find the TouchTransformer for a touch device.
  gfx::Transform touch_device_transformer_map_[kMaxDeviceNum];

  std::vector<TouchscreenDevice> touchscreen_devices_;
  std::vector<KeyboardDevice> keyboard_devices_;
  std::vector<InputDevice> mouse_devices_;
  std::vector<InputDevice> touchpad_devices_;

  ObserverList<InputDeviceEventObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(DeviceDataManager);
};

}  // namespace ui

#endif  // UI_EVENTS_DEVICES_DEVICE_DATA_MANAGER_H_
