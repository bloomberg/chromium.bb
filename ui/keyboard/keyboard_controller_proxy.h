// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_KEYBOARD_KEYBOARD_CONTROLLER_PROXY_H_
#define UI_KEYBOARD_KEYBOARD_CONTROLLER_PROXY_H_

#include "ui/keyboard/keyboard_export.h"

namespace aura {
class Window;
}

namespace ui {
class InputMethod;
}

namespace keyboard {

// A proxy used by the KeyboardController to get access to the virtual
// keyboard window.
class KEYBOARD_EXPORT KeyboardControllerProxy {
 public:
  virtual ~KeyboardControllerProxy() {}

  // Get the virtual keyboard window.  Ownership of the returned Window remains
  // with the proxy.
  virtual aura::Window* GetKeyboardWindow() = 0;

  // Get the InputMethod that will provide notifications about changes in the
  // text input context.
  virtual ui::InputMethod* GetInputMethod() = 0;
};

}  // namespace keyboard

#endif  // UI_KEYBOARD_KEYBOARD_CONTROLLER_PROXY_H_
