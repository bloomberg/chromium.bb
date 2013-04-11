// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_KEYBOARD_KEYBOARD_CONTROLLER_H_
#define UI_KEYBOARD_KEYBOARD_CONTROLLER_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "ui/base/ime/input_method_base.h"
#include "ui/keyboard/keyboard_export.h"

namespace aura {
class Window;
}

namespace ui {
class TextInputClient;
}

namespace keyboard {

class KeyboardControllerProxy;

// Provides control of the virtual keyboard, including providing a container,
// managing object lifetimes and controlling visibility.
class KEYBOARD_EXPORT KeyboardController
    : public ui::InputMethodBase::Observer {
 public:
  // Takes ownership of |proxy|.
  explicit KeyboardController(KeyboardControllerProxy* proxy);
  virtual ~KeyboardController();

  // Returns the container for the keyboard, which is then owned by the caller.
  // It is the responsibility of the caller to Show() the returned window.
  aura::Window* GetContainerWindow();

  // InputMethod::Observer overrides
  virtual void OnTextInputStateChanged(
      const ui::TextInputClient* client) OVERRIDE;

 private:
  scoped_ptr<KeyboardControllerProxy> proxy_;
  aura::Window* container_;

  DISALLOW_COPY_AND_ASSIGN(KeyboardController);
};

}  // namespace keyboard

#endif  // UI_KEYBOARD_KEYBOARD_CONTROLLER_H_
