// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/tablet_mode/internal_input_devices_event_blocker.h"

#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/touch/touch_devices_controller.h"
#include "services/ws/public/cpp/input_devices/input_device_controller_client.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/devices/input_device_manager.h"
#include "ui/events/keycodes/dom/dom_code.h"

namespace ash {

namespace {

ws::InputDeviceControllerClient* GetInputDeviceControllerClient() {
  return Shell::Get()->shell_delegate()->GetInputDeviceControllerClient();
}

}  // namespace

InternalInputDevicesEventBlocker::InternalInputDevicesEventBlocker() {
  ui::InputDeviceManager::GetInstance()->AddObserver(this);
}

InternalInputDevicesEventBlocker::~InternalInputDevicesEventBlocker() {
  ui::InputDeviceManager::GetInstance()->RemoveObserver(this);
  if (is_blocked_)
    UpdateInternalInputDevices(/*should_block=*/false);
}

void InternalInputDevicesEventBlocker::OnKeyboardDeviceConfigurationChanged() {
  UpdateInternalInputDevices(is_blocked_);
}

void InternalInputDevicesEventBlocker::OnTouchpadDeviceConfigurationChanged() {
  UpdateInternalInputDevices(is_blocked_);
}

void InternalInputDevicesEventBlocker::UpdateInternalInputDevices(
    bool should_block) {
  is_blocked_ = should_block;

  // Block or unblock the internal touchpad.
  if (HasInternalTouchpad()) {
    Shell::Get()->touch_devices_controller()->SetTouchpadEnabled(
        !should_block, TouchDeviceEnabledSource::GLOBAL);
  }

  // Block or unblock the internal keyboard. Note InputDeviceControllerClient
  // may be null in tests.
  if (HasInternalKeyboard() && GetInputDeviceControllerClient()) {
    std::vector<ui::DomCode> allowed_keys;
    if (should_block) {
      // Only allow the acccessible keys present on the side of some devices to
      // continue working if the internal keyboard events should be blocked.
      allowed_keys.push_back(ui::DomCode::VOLUME_DOWN);
      allowed_keys.push_back(ui::DomCode::VOLUME_UP);
      allowed_keys.push_back(ui::DomCode::POWER);
    }
    GetInputDeviceControllerClient()->SetInternalKeyboardFilter(should_block,
                                                                allowed_keys);
  }
}

bool InternalInputDevicesEventBlocker::HasInternalTouchpad() {
  for (const ui::InputDevice& touchpad :
       ui::InputDeviceManager::GetInstance()->GetTouchpadDevices()) {
    if (touchpad.type == ui::INPUT_DEVICE_INTERNAL)
      return true;
  }
  return false;
}

bool InternalInputDevicesEventBlocker::HasInternalKeyboard() {
  for (const ui::InputDevice& keyboard :
       ui::InputDeviceManager::GetInstance()->GetKeyboardDevices()) {
    if (keyboard.type == ui::INPUT_DEVICE_INTERNAL)
      return true;
  }
  return false;
}

}  // namespace ash
