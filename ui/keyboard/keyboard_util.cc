// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/keyboard/keyboard_util.h"

#include <string>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/metrics/histogram.h"
#include "base/strings/string16.h"
#include "grit/keyboard_resources.h"
#include "grit/keyboard_resources_map.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/root_window.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/keyboard/keyboard_switches.h"

namespace {

const char kKeyDown[] ="keydown";
const char kKeyUp[] = "keyup";

void SendProcessKeyEvent(ui::EventType type, aura::RootWindow* root_window) {
  ui::TranslatedKeyEvent event(type == ui::ET_KEY_PRESSED,
                               ui::VKEY_PROCESSKEY,
                               ui::EF_NONE);
  root_window->AsRootWindowHostDelegate()->OnHostKeyEvent(&event);
}

}  // namespace

namespace keyboard {

bool IsKeyboardEnabled() {
  return CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableVirtualKeyboard);
}

bool InsertText(const base::string16& text, aura::RootWindow* root_window) {
  if (!root_window)
    return false;

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

bool SendKeyEvent(const std::string type,
                  int key_value,
                  int key_code,
                  bool shift_modifier,
                  aura::RootWindow* root_window) {
  ui::EventType event_type = ui::ET_UNKNOWN;
  if (type == kKeyDown)
    event_type = ui::ET_KEY_PRESSED;
  else if (type == kKeyUp)
    event_type = ui::ET_KEY_RELEASED;
  if (event_type == ui::ET_UNKNOWN)
    return false;

  int flags = ui::EF_NONE;
  if (shift_modifier)
    flags = ui::EF_SHIFT_DOWN;

  ui::KeyboardCode code = static_cast<ui::KeyboardCode>(key_code);

  if (code == ui::VKEY_UNKNOWN) {
    // Handling of special printable characters (e.g. accented characters) for
    // which there is no key code.
    if (event_type == ui::ET_KEY_RELEASED) {
      ui::InputMethod* input_method = root_window->GetProperty(
          aura::client::kRootWindowInputMethodKey);
      if (!input_method)
        return false;

      ui::TextInputClient* tic = input_method->GetTextInputClient();

      SendProcessKeyEvent(ui::ET_KEY_PRESSED, root_window);
      tic->InsertChar(static_cast<uint16>(key_value), ui::EF_NONE);
      SendProcessKeyEvent(ui::ET_KEY_RELEASED, root_window);
    }
  } else {
    if (event_type == ui::ET_KEY_RELEASED) {
      // The number of key press events seen since the last backspace.
      static int keys_seen = 0;
      if (code == ui::VKEY_BACK) {
        // Log the rough lengths of characters typed between backspaces. This
        // metric will be used to determine the error rate for the keyboard.
        UMA_HISTOGRAM_CUSTOM_COUNTS(
            "VirtualKeyboard.KeystrokesBetweenBackspaces",
            keys_seen, 1, 1000, 50);
        keys_seen = 0;
      } else {
        ++keys_seen;
      }
    }

    ui::KeyEvent event(event_type, code, flags, false);
    root_window->AsRootWindowHostDelegate()->OnHostKeyEvent(&event);
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
    {"keyboard/elements/kb-key-codes.html", IDR_KEYBOARD_ELEMENTS_KEY_CODES},
    {"keyboard/elements/kb-key-import.html",
        IDR_KEYBOARD_ELEMENTS_KEY_IMPORT},
    {"keyboard/elements/kb-key-sequence.html",
        IDR_KEYBOARD_ELEMENTS_KEY_SEQUENCE},
    {"keyboard/elements/kb-keyboard.html", IDR_KEYBOARD_ELEMENTS_KEYBOARD},
    {"keyboard/elements/kb-keyset.html", IDR_KEYBOARD_ELEMENTS_KEYSET},
    {"keyboard/elements/kb-row.html", IDR_KEYBOARD_ELEMENTS_ROW},
    {"keyboard/elements/kb-shift-key.html", IDR_KEYBOARD_ELEMENTS_SHIFT_KEY},
    {"keyboard/layouts/function-key-row.html", IDR_KEYBOARD_FUNCTION_KEY_ROW},
    {"keyboard/images/back.svg", IDR_KEYBOARD_IMAGES_BACK},
    {"keyboard/images/brightness-down.svg",
        IDR_KEYBOARD_IMAGES_BRIGHTNESS_DOWN},
    {"keyboard/images/brightness-up.svg", IDR_KEYBOARD_IMAGES_BRIGHTNESS_UP},
    {"keyboard/images/change-window.svg", IDR_KEYBOARD_IMAGES_CHANGE_WINDOW},
    {"keyboard/images/down.svg", IDR_KEYBOARD_IMAGES_DOWN},
    {"keyboard/images/forward.svg", IDR_KEYBOARD_IMAGES_FORWARD},
    {"keyboard/images/fullscreen.svg", IDR_KEYBOARD_IMAGES_FULLSCREEN},
    {"keyboard/images/keyboard.svg", IDR_KEYBOARD_IMAGES_KEYBOARD},
    {"keyboard/images/left.svg", IDR_KEYBOARD_IMAGES_LEFT},
    {"keyboard/images/microphone.svg", IDR_KEYBOARD_IMAGES_MICROPHONE},
    {"keyboard/images/microphone-green.svg",
        IDR_KEYBOARD_IMAGES_MICROPHONE_GREEN},
    {"keyboard/images/mute.svg", IDR_KEYBOARD_IMAGES_MUTE},
    {"keyboard/images/reload.svg", IDR_KEYBOARD_IMAGES_RELOAD},
    {"keyboard/images/right.svg", IDR_KEYBOARD_IMAGES_RIGHT},
    {"keyboard/images/shutdown.svg", IDR_KEYBOARD_IMAGES_SHUTDOWN},
    {"keyboard/images/up.svg", IDR_KEYBOARD_IMAGES_UP},
    {"keyboard/images/volume-down.svg", IDR_KEYBOARD_IMAGES_VOLUME_DOWN},
    {"keyboard/images/volume-up.svg", IDR_KEYBOARD_IMAGES_VOLUME_UP},
    {"keyboard/index.html", IDR_KEYBOARD_INDEX},
    {"keyboard/layouts/dvorak.html", IDR_KEYBOARD_LAYOUTS_DVORAK},
    {"keyboard/layouts/latin-accents.js", IDR_KEYBOARD_LAYOUTS_LATIN_ACCENTS},
    {"keyboard/layouts/numeric.html", IDR_KEYBOARD_LAYOUTS_NUMERIC},
    {"keyboard/layouts/qwerty.html", IDR_KEYBOARD_LAYOUTS_QWERTY},
    {"keyboard/layouts/symbol-altkeys.js",
        IDR_KEYBOARD_LAYOUTS_SYMBOL_ALTKEYS},
    {"keyboard/layouts/system-qwerty.html", IDR_KEYBOARD_LAYOUTS_SYSTEM_QWERTY},
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

void LogKeyboardControlEvent(KeyboardControlEvent event) {
  UMA_HISTOGRAM_ENUMERATION(
      "VirtualKeyboard.KeyboardControlEvent",
      event,
      keyboard::KEYBOARD_CONTROL_MAX);
}

}  // namespace keyboard
