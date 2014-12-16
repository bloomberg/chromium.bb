// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_CHROMEOS_IME_KEYBOARD_OZONE_H_
#define UI_BASE_IME_CHROMEOS_IME_KEYBOARD_OZONE_H_

#include "ui/base/ime/chromeos/ime_keyboard.h"

#include <string>

#include "base/compiler_specific.h"
#include "ui/base/ui_base_export.h"

namespace ui {
class InputController;
}

namespace chromeos {
namespace input_method {

class UI_BASE_EXPORT ImeKeyboardOzone : public ImeKeyboard {
 public:
  ImeKeyboardOzone(ui::InputController* controller);
  virtual ~ImeKeyboardOzone();

  virtual bool SetCurrentKeyboardLayoutByName(const std::string& layout_name)
      override;
  virtual bool SetAutoRepeatRate(const AutoRepeatRate& rate) override;
  virtual bool SetAutoRepeatEnabled(bool enabled) override;
  virtual bool GetAutoRepeatEnabled() override;
  virtual bool ReapplyCurrentKeyboardLayout() override;
  virtual void ReapplyCurrentModifierLockStatus() override;
  virtual void DisableNumLock() override;
  virtual void SetCapsLockEnabled(bool enable_caps_lock) override;
  virtual bool CapsLockIsEnabled() override;

 private:
  ui::InputController* input_controller_;

  DISALLOW_COPY_AND_ASSIGN(ImeKeyboardOzone);
};

}  // namespace input_method
}  // namespace chromeos

#endif  // UI_BASE_IME_CHROMEOS_IME_KEYBOARD_OZONE_H_
