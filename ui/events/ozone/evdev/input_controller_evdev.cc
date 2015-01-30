// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/input_controller_evdev.h"

#include <algorithm>
#include <linux/input.h>

#include "ui/events/ozone/evdev/input_device_factory_evdev.h"
#include "ui/events/ozone/evdev/keyboard_evdev.h"
#include "ui/events/ozone/evdev/mouse_button_map_evdev.h"

namespace ui {

InputControllerEvdev::InputControllerEvdev(KeyboardEvdev* keyboard,
                                           MouseButtonMapEvdev* button_map)
    : input_device_factory_(nullptr),
      keyboard_(keyboard),
      button_map_(button_map) {
}

InputControllerEvdev::~InputControllerEvdev() {
}

void InputControllerEvdev::SetInputDeviceFactory(
    InputDeviceFactoryEvdev* input_device_factory) {
  input_device_factory_ = input_device_factory;
}

bool InputControllerEvdev::HasMouse() {
  if (!input_device_factory_)
    return false;
  return input_device_factory_->HasMouse();
}

bool InputControllerEvdev::HasTouchpad() {
  if (!input_device_factory_)
    return false;
  return input_device_factory_->HasTouchpad();
}

bool InputControllerEvdev::IsCapsLockEnabled() {
  return keyboard_->IsCapsLockEnabled();
}

void InputControllerEvdev::SetCapsLockEnabled(bool enabled) {
  keyboard_->SetCapsLockEnabled(enabled);
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
  }
}

void InputControllerEvdev::EnableInternalKeyboard() {
  if (input_device_factory_)
    input_device_factory_->EnableInternalKeyboard();
}

void InputControllerEvdev::SetTouchpadSensitivity(int value) {
  if (input_device_factory_)
    input_device_factory_->SetTouchpadSensitivity(value);
}

void InputControllerEvdev::SetTapToClick(bool enabled) {
  if (input_device_factory_)
    input_device_factory_->SetTapToClick(enabled);
}

void InputControllerEvdev::SetThreeFingerClick(bool enabled) {
  if (input_device_factory_)
    input_device_factory_->SetThreeFingerClick(enabled);
}

void InputControllerEvdev::SetTapDragging(bool enabled) {
  if (input_device_factory_)
    input_device_factory_->SetTapDragging(enabled);
}

void InputControllerEvdev::SetNaturalScroll(bool enabled) {
  if (input_device_factory_)
    input_device_factory_->SetNaturalScroll(enabled);
}

void InputControllerEvdev::SetMouseSensitivity(int value) {
  if (input_device_factory_)
    input_device_factory_->SetMouseSensitivity(value);
}

void InputControllerEvdev::SetPrimaryButtonRight(bool right) {
  button_map_->UpdateButtonMap(BTN_LEFT, right ? BTN_RIGHT : BTN_LEFT);
  button_map_->UpdateButtonMap(BTN_RIGHT, right ? BTN_LEFT : BTN_RIGHT);
}

void InputControllerEvdev::SetTapToClickPaused(bool state) {
  if (input_device_factory_)
    input_device_factory_->SetTapToClickPaused(state);
}

void InputControllerEvdev::GetTouchDeviceStatus(
    const GetTouchDeviceStatusReply& reply) {
  if (input_device_factory_)
    input_device_factory_->GetTouchDeviceStatus(reply);
  else
    reply.Run(make_scoped_ptr(new std::string));
}

}  // namespace ui
