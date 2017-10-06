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
  if (HasContentsWindow()) {
    {
      TRACE_EVENT0("vk", "ShowKeyboardContainerWindow");
      GetContentsWindow()->Show();
    }
    {
      TRACE_EVENT0("vk", "ShowKeyboardContainer");
      container->Show();
    }
  }
}

void KeyboardUI::HideKeyboardContainer(aura::Window* container) {
  if (HasContentsWindow()) {
    container->Hide();
    GetContentsWindow()->Hide();
  }
}

void KeyboardUI::EnsureCaretInWorkArea() {
  if (!GetInputMethod())
    return;

  TRACE_EVENT0("vk", "EnsureCaretInWorkArea");

  const aura::Window* contents_window = GetContentsWindow();
  const gfx::Rect keyboard_bounds_in_screen =
      contents_window->IsVisible() ? contents_window->GetBoundsInScreen()
                                   : gfx::Rect();

  // Use new virtual keyboard behavior only if in non-sticky mode.
  const bool new_vk_behavior = !keyboard_controller_->keyboard_locked();

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
