// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_KEYBOARD_BRIGHTNESS_CONTROL_DELEGATE_H_
#define ASH_SYSTEM_KEYBOARD_BRIGHTNESS_CONTROL_DELEGATE_H_

namespace ui {
class Accelerator;
}  // namespace ui

namespace ash {

// Delegate for controlling the keyboard brightness.
class KeyboardBrightnessControlDelegate {
 public:
  virtual ~KeyboardBrightnessControlDelegate() {}

  // Handles an accelerator-driven request to decrease or increase the keyboard
  // brightness.
  virtual void HandleKeyboardBrightnessDown(
      const ui::Accelerator& accelerator) = 0;
  virtual void HandleKeyboardBrightnessUp(
      const ui::Accelerator& accelerator) = 0;
};

}  // namespace ash

#endif  // ASH_SYSTEM_KEYBOARD_BRIGHTNESS_CONTROL_DELEGATE_H_
