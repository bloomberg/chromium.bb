// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/input_controller_evdev.h"

#include <algorithm>
#include <linux/input.h>

#include "base/thread_task_runner_handle.h"
#include "ui/events/ozone/evdev/input_device_factory_evdev_proxy.h"
#include "ui/events/ozone/evdev/keyboard_evdev.h"
#include "ui/events/ozone/evdev/mouse_button_map_evdev.h"

namespace ui {

InputControllerEvdev::InputControllerEvdev(KeyboardEvdev* keyboard,
                                           MouseButtonMapEvdev* button_map)
    : settings_update_pending_(false),
      input_device_factory_(nullptr),
      keyboard_(keyboard),
      button_map_(button_map),
      has_mouse_(false),
      has_touchpad_(false),
      caps_lock_led_state_(false),
      weak_ptr_factory_(this) {
}

InputControllerEvdev::~InputControllerEvdev() {
}

void InputControllerEvdev::SetInputDeviceFactory(
    InputDeviceFactoryEvdevProxy* input_device_factory) {
  input_device_factory_ = input_device_factory;

  UpdateDeviceSettings();
  UpdateCapsLockLed();
}

void InputControllerEvdev::set_has_mouse(bool has_mouse) {
  has_mouse_ = has_mouse;
}

void InputControllerEvdev::set_has_touchpad(bool has_touchpad) {
  has_touchpad_ = has_touchpad;
}

bool InputControllerEvdev::HasMouse() {
  return has_mouse_;
}

bool InputControllerEvdev::HasTouchpad() {
  return has_touchpad_;
}

bool InputControllerEvdev::IsCapsLockEnabled() {
  return keyboard_->IsCapsLockEnabled();
}

void InputControllerEvdev::SetCapsLockEnabled(bool enabled) {
  keyboard_->SetCapsLockEnabled(enabled);
  UpdateCapsLockLed();
}

void InputControllerEvdev::SetNumLockEnabled(bool enabled) {
  NOTIMPLEMENTED();
}

bool InputControllerEvdev::IsAutoRepeatEnabled() {
  return keyboard_->IsAutoRepeatEnabled();
}

void InputControllerEvdev::SetAutoRepeatEnabled(bool enabled) {
  keyboard_->SetAutoRepeatEnabled(enabled);
}

void InputControllerEvdev::SetAutoRepeatRate(const base::TimeDelta& delay,
                                             const base::TimeDelta& interval) {
  keyboard_->SetAutoRepeatRate(delay, interval);
}

void InputControllerEvdev::GetAutoRepeatRate(base::TimeDelta* delay,
                                             base::TimeDelta* interval) {
  keyboard_->GetAutoRepeatRate(delay, interval);
}

void InputControllerEvdev::DisableInternalTouchpad() {
  if (input_device_factory_)
    input_device_factory_->DisableInternalTouchpad();
}

void InputControllerEvdev::EnableInternalTouchpad() {
  if (input_device_factory_)
    input_device_factory_->EnableInternalTouchpad();
}

void InputControllerEvdev::DisableInternalKeyboardExceptKeys(
    scoped_ptr<std::set<DomCode>> excepted_keys) {
  if (input_device_factory_) {
    input_device_factory_->DisableInternalKeyboardExceptKeys(
        excepted_keys.Pass());

    // Release keys since we may block key up events.
    keyboard_->Reset();
  }
}

void InputControllerEvdev::EnableInternalKeyboard() {
  if (input_device_factory_)
    input_device_factory_->EnableInternalKeyboard();
}

void InputControllerEvdev::SetTouchpadSensitivity(int value) {
  input_device_settings_.touchpad_sensitivity = value;
  ScheduleUpdateDeviceSettings();
}

void InputControllerEvdev::SetTapToClick(bool enabled) {
  input_device_settings_.tap_to_click_enabled = enabled;
  ScheduleUpdateDeviceSettings();
}

void InputControllerEvdev::SetThreeFingerClick(bool enabled) {
  input_device_settings_.three_finger_click_enabled = enabled;
  ScheduleUpdateDeviceSettings();
}

void InputControllerEvdev::SetTapDragging(bool enabled) {
  input_device_settings_.tap_dragging_enabled = enabled;
  ScheduleUpdateDeviceSettings();
}

void InputControllerEvdev::SetNaturalScroll(bool enabled) {
  input_device_settings_.natural_scroll_enabled = enabled;
  ScheduleUpdateDeviceSettings();
}

void InputControllerEvdev::SetMouseSensitivity(int value) {
  input_device_settings_.mouse_sensitivity = value;
  ScheduleUpdateDeviceSettings();
}

void InputControllerEvdev::SetPrimaryButtonRight(bool right) {
  button_map_->UpdateButtonMap(BTN_LEFT, right ? BTN_RIGHT : BTN_LEFT);
  button_map_->UpdateButtonMap(BTN_RIGHT, right ? BTN_LEFT : BTN_RIGHT);
}

void InputControllerEvdev::SetTapToClickPaused(bool state) {
  input_device_settings_.tap_to_click_paused = state;
  ScheduleUpdateDeviceSettings();
}

void InputControllerEvdev::GetTouchDeviceStatus(
    const GetTouchDeviceStatusReply& reply) {
  if (input_device_factory_)
    input_device_factory_->GetTouchDeviceStatus(reply);
  else
    reply.Run(make_scoped_ptr(new std::string));
}

void InputControllerEvdev::GetTouchEventLog(
    const base::FilePath& out_dir,
    const GetTouchEventLogReply& reply) {
  if (input_device_factory_)
    input_device_factory_->GetTouchEventLog(out_dir, reply);
  else
    reply.Run(make_scoped_ptr(new std::vector<base::FilePath>));
}

void InputControllerEvdev::ScheduleUpdateDeviceSettings() {
  if (!input_device_factory_ || settings_update_pending_)
    return;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&InputControllerEvdev::UpdateDeviceSettings,
                            weak_ptr_factory_.GetWeakPtr()));
  settings_update_pending_ = true;
}

void InputControllerEvdev::UpdateDeviceSettings() {
  input_device_factory_->UpdateInputDeviceSettings(input_device_settings_);
  settings_update_pending_ = false;
}

void InputControllerEvdev::UpdateCapsLockLed() {
  if (!input_device_factory_)
    return;
  bool caps_lock_state = IsCapsLockEnabled();
  if (caps_lock_state != caps_lock_led_state_)
    input_device_factory_->SetCapsLockLed(caps_lock_state);
  caps_lock_led_state_ = caps_lock_state;
}

}  // namespace ui
