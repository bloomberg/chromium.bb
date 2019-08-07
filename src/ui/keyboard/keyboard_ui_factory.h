// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_KEYBOARD_KEYBOARD_UI_FACTORY_H_
#define UI_KEYBOARD_KEYBOARD_UI_FACTORY_H_

#include <memory>

#include "base/macros.h"
#include "ui/keyboard/keyboard_export.h"

namespace keyboard {

class KeyboardUI;

// KeyboardUIFactory is the factory of platform-dependent KeyboardUI.
class KEYBOARD_EXPORT KeyboardUIFactory {
 public:
  KeyboardUIFactory();
  virtual ~KeyboardUIFactory();

  // Creates a new instance of KeyboardUI.
  virtual std::unique_ptr<KeyboardUI> CreateKeyboardUI() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(KeyboardUIFactory);
};

}  // namespace keyboard

#endif  // UI_KEYBOARD_KEYBOARD_UI_FACTORY_H_
