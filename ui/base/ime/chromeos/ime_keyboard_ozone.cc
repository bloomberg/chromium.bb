// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/chromeos/ime_keyboard_ozone.h"

#include "ui/ozone/public/input_controller.h"
#include "ui/ozone/public/ozone_platform.h"

namespace chromeos {
namespace input_method {

ImeKeyboardOzone::ImeKeyboardOzone(ui::InputController* input_controller)
    : input_controller_(input_controller) {
}


ImeKeyboardOzone::~ImeKeyboardOzone() {
}

bool ImeKeyboardOzone::SetCurrentKeyboardLayoutByName(
    const std::string& layout_name) {
  last_layout_ = layout_name;
  input_controller_->SetCurrentLayoutByName(layout_name);
  return true;
}

bool ImeKeyboardOzone::ReapplyCurrentKeyboardLayout() {
  return SetCurrentKeyboardLayoutByName(last_layout_);
}

void ImeKeyboardOzone::SetCapsLockEnabled(bool enable_caps_lock) {
  // Inform ImeKeyboard of caps lock state.
  ImeKeyboard::SetCapsLockEnabled(enable_caps_lock);
  // Inform Ozone InputController input of caps lock state.
  input_controller_->SetCapsLockEnabled(enable_caps_lock);
}

bool ImeKeyboardOzone::CapsLockIsEnabled() {
  return input_controller_->IsCapsLockEnabled();
}

void ImeKeyboardOzone::ReapplyCurrentModifierLockStatus() {
}

void ImeKeyboardOzone::DisableNumLock() {
  input_controller_->SetNumLockEnabled(false);
}

bool ImeKeyboardOzone::SetAutoRepeatRate(const AutoRepeatRate& rate) {
  input_controller_->SetAutoRepeatRate(
      base::TimeDelta::FromMilliseconds(rate.initial_delay_in_ms),
      base::TimeDelta::FromMilliseconds(rate.repeat_interval_in_ms));
  return true;
}

bool ImeKeyboardOzone::SetAutoRepeatEnabled(bool enabled) {
  input_controller_->SetAutoRepeatEnabled(enabled);
  return true;
}

bool ImeKeyboardOzone::GetAutoRepeatEnabled() {
  return input_controller_->IsAutoRepeatEnabled();
}

// static
ImeKeyboard* ImeKeyboard::Create() {
  return new ImeKeyboardOzone(
      ui::OzonePlatform::GetInstance()->GetInputController());
}

}  // namespace input_method
}  // namespace chromeos
