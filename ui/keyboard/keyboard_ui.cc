// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/keyboard/keyboard_ui.h"

#include "base/command_line.h"
#include "ui/aura/window.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/ui_base_switches.h"
#include "ui/keyboard/keyboard_controller.h"

namespace keyboard {

KeyboardUI::KeyboardUI() : keyboard_controller_(nullptr) {}
KeyboardUI::~KeyboardUI() {}

void KeyboardUI::ShowKeyboardContainer(aura::Window* container) {
  if (HasKeyboardWindow()) {
    GetKeyboardWindow()->Show();
    container->Show();
  }
}

void KeyboardUI::HideKeyboardContainer(aura::Window* container) {
  if (HasKeyboardWindow()) {
    container->Hide();
    GetKeyboardWindow()->Hide();
  }
}

void KeyboardUI::EnsureCaretInWorkArea() {
  if (!GetInputMethod())
    return;

  const aura::Window* keyboard_window = GetKeyboardWindow();
  const gfx::Rect keyboard_bounds_in_screen =
      keyboard_window->IsVisible() ? keyboard_window->GetBoundsInScreen()
                                   : gfx::Rect();

  // Use new virtual keyboard behavior only if the flag enabled and in
  // non-sticky mode.
  const bool new_vk_behavior =
      (!base::CommandLine::ForCurrentProcess()->HasSwitch(
           ::switches::kDisableNewVirtualKeyboardBehavior) &&
       !keyboard_controller_->keyboard_locked());

  if (new_vk_behavior) {
    GetInputMethod()->SetOnScreenKeyboardBounds(keyboard_bounds_in_screen);
  } else if (GetInputMethod()->GetTextInputClient()) {
    GetInputMethod()->GetTextInputClient()->EnsureCaretNotInRect(
        keyboard_bounds_in_screen);
  }
}

void KeyboardUI::SetController(KeyboardController* controller) {
  keyboard_controller_ = controller;
}

}  // namespace keyboard
