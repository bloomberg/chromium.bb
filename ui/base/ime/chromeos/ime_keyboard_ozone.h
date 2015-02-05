// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_CHROMEOS_IME_KEYBOARD_OZONE_H_
#define UI_BASE_IME_CHROMEOS_IME_KEYBOARD_OZONE_H_

#include "ui/base/ime/chromeos/ime_keyboard.h"

#include <string>

#include "base/compiler_specific.h"
#include "ui/base/ime/ui_base_ime_export.h"

namespace ui {
class InputController;
}

namespace chromeos {
namespace input_method {

class UI_BASE_IME_EXPORT ImeKeyboardOzone : public ImeKeyboard {
 public:
  ImeKeyboardOzone(ui::InputController* controller);
  ~ImeKeyboardOzone() override;

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
  ui::InputController* input_controller_;

  DISALLOW_COPY_AND_ASSIGN(ImeKeyboardOzone);
};

}  // namespace input_method
}  // namespace chromeos

#endif  // UI_BASE_IME_CHROMEOS_IME_KEYBOARD_OZONE_H_
