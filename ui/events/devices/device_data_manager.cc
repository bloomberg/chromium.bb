// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/devices/device_data_manager.h"

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/logging.h"
#include "ui/events/devices/input_device_event_observer.h"
#include "ui/gfx/display.h"
#include "ui/gfx/geometry/point3_f.h"

namespace ui {

namespace {

bool InputDeviceEquals(const ui::InputDevice& a, const ui::InputDevice& b) {
  return a.id == b.id;
}

}  // namespace

// static
DeviceDataManager* DeviceDataManager::instance_ = NULL;

DeviceDataManager::DeviceDataManager() {
  CHECK(!instance_) << "Can not create multiple instances of DeviceDataManager";
  instance_ = this;

  base::AtExitManager::RegisterTask(
      base::Bind(&base::DeletePointer<DeviceDataManager>, this));

  ClearTouchDeviceAssociations();
}

DeviceDataManager::~DeviceDataManager() {
  CHECK_EQ(this, instance_);
  instance_ = NULL;
}

// static
DeviceDataManager* DeviceDataManager::instance() { return instance_; }

// static
void DeviceDataManager::CreateInstance() {
  if (instance())
    return;

  new DeviceDataManager();
}

// static
DeviceDataManager* DeviceDataManager::GetInstance() {
  CHECK(instance_) << "DeviceDataManager was not created.";
  return instance_;
}

// static
bool DeviceDataManager::HasInstance() {
  return instance_ != NULL;
}

void DeviceDataManager::ClearTouchDeviceAssociations() {
  for (int i = 0; i < kMaxDeviceNum; i++) {
    touch_device_transformer_map_[i] = gfx::Transform();
    touch_device_to_target_display_map_[i] = gfx::Display::kInvalidDisplayID;
    touch_radius_scale_map_[i] = 1.0;
  }
}

bool DeviceDataManager::IsTouchDeviceIdValid(
    unsigned int touch_device_id) const {
  return (touch_device_id > 0 && touch_device_id < kMaxDeviceNum);
}

void DeviceDataManager::UpdateTouchInfoForDisplay(
    int64_t target_display_id,
    unsigned int touch_device_id,
    const gfx::Transform& touch_transformer) {
  if (IsTouchDeviceIdValid(touch_device_id)) {
    touch_device_to_target_display_map_[touch_device_id] = target_display_id;
    touch_device_transformer_map_[touch_device_id] = touch_transformer;
  }
}

void DeviceDataManager::UpdateTouchRadiusScale(unsigned int touch_device_id,
                                               double scale) {
  if (IsTouchDeviceIdValid(touch_device_id))
    touch_radius_scale_map_[touch_device_id] = scale;
}

void DeviceDataManager::ApplyTouchRadiusScale(unsigned int touch_device_id,
                                              double* radius) {
  if (IsTouchDeviceIdValid(touch_device_id))
    *radius = (*radius) * touch_radius_scale_map_[touch_device_id];
}

void DeviceDataManager::ApplyTouchTransformer(unsigned int touch_device_id,
                                              float* x,
                                              float* y) {
  if (IsTouchDeviceIdValid(touch_device_id)) {
    gfx::Point3F point(*x, *y, 0.0);
    const gfx::Transform& trans =
        touch_device_transformer_map_[touch_device_id];
    trans.TransformPoint(&point);
    *x = point.x();
    *y = point.y();
  }
}

int64_t DeviceDataManager::GetTargetDisplayForTouchDevice(
    unsigned int touch_device_id) const {
  if (IsTouchDeviceIdValid(touch_device_id))
    return touch_device_to_target_display_map_[touch_device_id];
  return gfx::Display::kInvalidDisplayID;
}

void DeviceDataManager::OnTouchscreenDevicesUpdated(
    const std::vector<TouchscreenDevice>& devices) {
  if (devices.size() == touchscreen_devices_.size() &&
      std::equal(devices.begin(),
                 devices.end(),
                 touchscreen_devices_.begin(),
                 InputDeviceEquals)) {
    return;
  }
  touchscreen_devices_ = devices;
  FOR_EACH_OBSERVER(InputDeviceEventObserver,
                    observers_,
                    OnTouchscreenDeviceConfigurationChanged());
}

void DeviceDataManager::OnKeyboardDevicesUpdated(
    const std::vector<KeyboardDevice>& devices) {
  if (devices.size() == keyboard_devices_.size() &&
      std::equal(devices.begin(),
                 devices.end(),
                 keyboard_devices_.begin(),
                 InputDeviceEquals)) {
    return;
  }
  keyboard_devices_ = devices;
  FOR_EACH_OBSERVER(InputDeviceEventObserver,
                    observers_,
                    OnKeyboardDeviceConfigurationChanged());
}

void DeviceDataManager::OnMouseDevicesUpdated(
    const std::vector<InputDevice>& devices) {
  if (devices.size() == mouse_devices_.size() &&
      std::equal(devices.begin(),
                 devices.end(),
                 mouse_devices_.begin(),
                 InputDeviceEquals)) {
    return;
  }
  mouse_devices_ = devices;
  FOR_EACH_OBSERVER(InputDeviceEventObserver,
                    observers_,
                    OnMouseDeviceConfigurationChanged());
}

void DeviceDataManager::OnTouchpadDevicesUpdated(
    const std::vector<InputDevice>& devices) {
  if (devices.size() == touchpad_devices_.size() &&
      std::equal(devices.begin(),
                 devices.end(),
                 touchpad_devices_.begin(),
                 InputDeviceEquals)) {
    return;
  }
  touchpad_devices_ = devices;
  FOR_EACH_OBSERVER(InputDeviceEventObserver,
                    observers_,
                    OnTouchpadDeviceConfigurationChanged());
}

void DeviceDataManager::AddObserver(InputDeviceEventObserver* observer) {
  observers_.AddObserver(observer);
}

void DeviceDataManager::RemoveObserver(InputDeviceEventObserver* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace ui
