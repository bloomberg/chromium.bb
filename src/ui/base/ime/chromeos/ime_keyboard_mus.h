// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_CHROMEOS_IME_KEYBOARD_MUS_H_
#define UI_BASE_IME_CHROMEOS_IME_KEYBOARD_MUS_H_

#include "base/component_export.h"
#include "base/macros.h"
#include "ui/base/ime/chromeos/ime_keyboard.h"

namespace ws {
class InputDeviceControllerClient;
}

namespace chromeos {
namespace input_method {

class COMPONENT_EXPORT(UI_BASE_IME_CHROMEOS) ImeKeyboardMus
    : public ImeKeyboard {
 public:
  explicit ImeKeyboardMus(
      ws::InputDeviceControllerClient* input_device_controller_client);
  ~ImeKeyboardMus() override;

  // ImeKeyboard:
  bool SetCurrentKeyboardLayoutByName(const std::string& layout_name) override;
  bool SetAutoRepeatRate(const AutoRepeatRate& rate) override;
  bool SetAutoRepeatEnabled(bool enabled) override;
  bool GetAutoRepeatEnabled() override;
  bool ReapplyCurrentKeyboardLayout() override;
  void ReapplyCurrentModifierLockStatus() override;
  void DisableNumLock() override;
  void SetCapsLockEnabled(bool enable_caps_lock) override;
  bool CapsLockIsEnabled() override;

 private:
  ws::InputDeviceControllerClient* input_device_controller_client_;

  DISALLOW_COPY_AND_ASSIGN(ImeKeyboardMus);
};

}  // namespace input_method
}  // namespace chromeos

#endif  // UI_BASE_IME_CHROMEOS_IME_KEYBOARD_MUS_H_
