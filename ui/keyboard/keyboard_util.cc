// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/keyboard/keyboard_util.h"

#include <string>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/string16.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/root_window.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/keyboard/keyboard_switches.h"

namespace keyboard {

bool IsKeyboardEnabled() {
  return CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableVirtualKeyboard);
}

bool InsertText(const base::string16& text, aura::RootWindow* root_window) {
  if (!root_window)
    return false;

  // Handle Backspace and Enter specially: using TextInputClient::InsertText is
  // very unreliable for these characters.
  // TODO(bryeung): remove this code once virtual keyboards are able to send
  // these events directly via the Input Injection API.
  if (text.length() == 1) {
    ui::KeyboardCode code = ui::VKEY_UNKNOWN;
    if (text[0] == L'\n')
      code = ui::VKEY_RETURN;
    else if (text[0] == L'\b')
      code = ui::VKEY_BACK;

    if (code != ui::VKEY_UNKNOWN) {
      ui::KeyEvent press_event(ui::ET_KEY_PRESSED, code, 0, 0);
      root_window->AsRootWindowHostDelegate()->OnHostKeyEvent(&press_event);

      ui::KeyEvent release_event(ui::ET_KEY_RELEASED, code, 0, 0);
      root_window->AsRootWindowHostDelegate()->OnHostKeyEvent(&release_event);

      return true;
    }
  }

  ui::InputMethod* input_method = root_window->GetProperty(
      aura::client::kRootWindowInputMethodKey);
  if (!input_method)
    return false;

  ui::TextInputClient* tic = input_method->GetTextInputClient();
  if (!tic || tic->GetTextInputType() == ui::TEXT_INPUT_TYPE_NONE)
    return false;

  tic->InsertText(text);

  return true;
}

}  // namespace keyboard
