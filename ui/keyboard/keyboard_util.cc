// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/keyboard/keyboard_util.h"

#include <string>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/string16.h"
#include "grit/keyboard_resources.h"
#include "grit/keyboard_resources_map.h"
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

// TODO(varunjain): It would be cleaner to have something in the
// ui::TextInputClient interface, say MoveCaretInDirection(). The code in
// here would get the ui::InputMethod from the root_window, and the
// ui::TextInputClient from that (see above in InsertText()).
bool MoveCursor(int swipe_direction,
                int modifier_flags,
                aura::RootWindow* root_window) {
  if (!root_window)
    return false;
  ui::KeyboardCode codex = ui::VKEY_UNKNOWN;
  ui::KeyboardCode codey = ui::VKEY_UNKNOWN;
  if (swipe_direction & kCursorMoveRight)
    codex = ui::VKEY_RIGHT;
  else if (swipe_direction & kCursorMoveLeft)
    codex = ui::VKEY_LEFT;

  if (swipe_direction & kCursorMoveUp)
    codey = ui::VKEY_UP;
  else if (swipe_direction & kCursorMoveDown)
    codey = ui::VKEY_DOWN;

  // First deal with the x movement.
  if (codex != ui::VKEY_UNKNOWN) {
    ui::KeyEvent press_event(ui::ET_KEY_PRESSED, codex, modifier_flags, 0);
    root_window->AsRootWindowHostDelegate()->OnHostKeyEvent(&press_event);
    ui::KeyEvent release_event(ui::ET_KEY_RELEASED, codex, modifier_flags, 0);
    root_window->AsRootWindowHostDelegate()->OnHostKeyEvent(&release_event);
  }

  // Then deal with the y movement.
  if (codey != ui::VKEY_UNKNOWN) {
    ui::KeyEvent press_event(ui::ET_KEY_PRESSED, codey, modifier_flags, 0);
    root_window->AsRootWindowHostDelegate()->OnHostKeyEvent(&press_event);
    ui::KeyEvent release_event(ui::ET_KEY_RELEASED, codey, modifier_flags, 0);
    root_window->AsRootWindowHostDelegate()->OnHostKeyEvent(&release_event);
  }
  return true;
}

const GritResourceMap* GetKeyboardExtensionResources(size_t* size) {
  // This looks a lot like the contents of a resource map; however it is
  // necessary to have a custom path for the extension path, so the resource
  // map cannot be used directly.
  static const GritResourceMap kKeyboardResources[] = {
    {"keyboard/api_adapter.js", IDR_KEYBOARD_API_ADAPTER_JS},
    {"keyboard/constants.js", IDR_KEYBOARD_CONSTANTS_JS},
    {"keyboard/elements/kb-altkey.html", IDR_KEYBOARD_ELEMENTS_ALTKEY},
    {"keyboard/elements/kb-altkey-container.html",
        IDR_KEYBOARD_ELEMENTS_ALTKEY_CONTAINER},
    {"keyboard/elements/kb-altkey-data.html",
        IDR_KEYBOARD_ELEMENTS_ALTKEY_DATA},
    {"keyboard/elements/kb-altkey-set.html", IDR_KEYBOARD_ELEMENTS_ALTKEY_SET},
    {"keyboard/elements/kb-key.html", IDR_KEYBOARD_ELEMENTS_KEY},
    {"keyboard/elements/kb-key-base.html", IDR_KEYBOARD_ELEMENTS_KEY_BASE},
    {"keyboard/elements/kb-key-import.html",
        IDR_KEYBOARD_ELEMENTS_KEY_IMPORT},
    {"keyboard/elements/kb-key-sequence.html",
        IDR_KEYBOARD_ELEMENTS_KEY_SEQUENCE},
    {"keyboard/elements/kb-keyboard.html", IDR_KEYBOARD_ELEMENTS_KEYBOARD},
    {"keyboard/elements/kb-keyset.html", IDR_KEYBOARD_ELEMENTS_KEYSET},
    {"keyboard/elements/kb-row.html", IDR_KEYBOARD_ELEMENTS_ROW},
    {"keyboard/images/keyboard.svg", IDR_KEYBOARD_IMAGES_KEYBOARD},
    {"keyboard/images/microphone.svg", IDR_KEYBOARD_IMAGES_MICROPHONE},
    {"keyboard/images/microphone-green.svg",
        IDR_KEYBOARD_IMAGES_MICROPHONE_GREEN},
    {"keyboard/index.html", IDR_KEYBOARD_INDEX},
    {"keyboard/layouts/dvorak.html", IDR_KEYBOARD_LAYOUTS_DVORAK},
    {"keyboard/layouts/latin-accents.js", IDR_KEYBOARD_LAYOUTS_LATIN_ACCENTS},
    {"keyboard/layouts/qwerty.html", IDR_KEYBOARD_LAYOUTS_QWERTY},
    {"keyboard/layouts/symbol-altkeys.js",
        IDR_KEYBOARD_LAYOUTS_SYMBOL_ALTKEYS},
    {"keyboard/layouts/spacebar-row.html", IDR_KEYBOARD_SPACEBAR_ROW},
    {"keyboard/main.js", IDR_KEYBOARD_MAIN_JS},
    {"keyboard/manifest.json", IDR_KEYBOARD_MANIFEST},
    {"keyboard/main.css", IDR_KEYBOARD_MAIN_CSS},
    {"keyboard/polymer.min.js", IDR_KEYBOARD_POLYMER},
    {"keyboard/voice_input.js", IDR_KEYBOARD_VOICE_INPUT_JS},
  };
  static const size_t kKeyboardResourcesSize = arraysize(kKeyboardResources);
  *size = kKeyboardResourcesSize;
  return kKeyboardResources;
}

}  // namespace keyboard
