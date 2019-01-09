// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_KEYBOARD_TEST_TEST_KEYBOARD_LAYOUT_DELEGATE_H_
#define UI_KEYBOARD_TEST_TEST_KEYBOARD_LAYOUT_DELEGATE_H_

#include "ui/keyboard/keyboard_layout_delegate.h"

namespace keyboard {

class TestKeyboardLayoutDelegate : public KeyboardLayoutDelegate {
 public:
  TestKeyboardLayoutDelegate() = default;
  ~TestKeyboardLayoutDelegate() override = default;

  // Overridden from keyboard::KeyboardLayoutDelegate
  void MoveKeyboardToDisplay(const display::Display& display) override {}
  void MoveKeyboardToTouchableDisplay() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TestKeyboardLayoutDelegate);
};

}  // namespace keyboard

#endif  // UI_KEYBOARD_TEST_TEST_KEYBOARD_LAYOUT_DELEGATE_H_
