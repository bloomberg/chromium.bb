// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_KEYBOARD_TEST_TEST_KEYBOARD_UI_FACTORY_H_
#define UI_KEYBOARD_TEST_TEST_KEYBOARD_UI_FACTORY_H_

#include <memory>

#include "ui/aura/test/test_window_delegate.h"
#include "ui/keyboard/keyboard_ui.h"
#include "ui/keyboard/keyboard_ui_factory.h"

namespace aura {
class Window;
}

namespace ui {
class InputMethod;
}

namespace keyboard {

class TestKeyboardUIFactory : public KeyboardUIFactory {
 public:
  class TestKeyboardUI : public KeyboardUI {
   public:
    explicit TestKeyboardUI(ui::InputMethod* input_method);
    ~TestKeyboardUI() override;

    // Overridden from KeyboardUI:
    aura::Window* LoadKeyboardWindow(LoadCallback callback) override;
    aura::Window* GetKeyboardWindow() const override;
    ui::InputMethod* GetInputMethod() override;
    void ReloadKeyboardIfNeeded() override {}

   private:
    std::unique_ptr<aura::Window> window_;
    aura::test::TestWindowDelegate delegate_;
    ui::InputMethod* input_method_;
  };

  explicit TestKeyboardUIFactory(ui::InputMethod* input_method);
  ~TestKeyboardUIFactory() override;

  // Overridden from KeyboardUIFactory:
  std::unique_ptr<KeyboardUI> CreateKeyboardUI() override;

 private:
  ui::InputMethod* input_method_;

  DISALLOW_COPY_AND_ASSIGN(TestKeyboardUIFactory);
};

}  // namespace keyboard

#endif  // UI_KEYBOARD_TEST_TEST_KEYBOARD_UI_FACTORY_H_
