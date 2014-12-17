// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/layout/stub/stub_keyboard_layout_engine.h"

#include <algorithm>

#include "base/strings/string16.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/dom3/dom_code.h"
#include "ui/events/keycodes/dom3/dom_key.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"
#include "ui/events/ozone/layout/layout_util.h"

namespace ui {

namespace {

// All of the characters have low ordinals, so we use bit 15 to flag dead keys.
#define DK  0x8000

const struct PrintableCodeEntry {
  DomCode dom_code;
  base::char16 character[4];
} printable_code_map[] = {
    // Stub table based on X US international.
    {DomCode::KEY_A,                    {'a', 'A', 0x00E1, 0x00C1}},
    {DomCode::KEY_B,                    {'b', 'B', 'b', 'B'}},
    {DomCode::KEY_C,                    {'c', 'C', 0x00A9, 0x00A2}},
    {DomCode::KEY_D,                    {'d', 'D', 0x00F0, 0x00D0}},
    {DomCode::KEY_E,                    {'e', 'E', 0x00E9, 0x00C9}},
    {DomCode::KEY_F,                    {'f', 'F', 'f', 'F'}},
    {DomCode::KEY_G,                    {'g', 'G', 'g', 'G'}},
    {DomCode::KEY_H,                    {'h', 'H', 'h', 'H'}},
    {DomCode::KEY_I,                    {'i', 'I', 0x00ED, 0x00CD}},
    {DomCode::KEY_J,                    {'j', 'J', 'j', 'J'}},
    {DomCode::KEY_K,                    {'k', 'K', 0x0153, 0x0152}},
    {DomCode::KEY_L,                    {'l', 'L', 0x00F8, 0x00D8}},
    {DomCode::KEY_M,                    {'m', 'M', 0x00B5, 0x00B5}},
    {DomCode::KEY_N,                    {'n', 'N', 0x00F1, 0x00D1}},
    {DomCode::KEY_O,                    {'o', 'O', 0x00F3, 0x00D3}},
    {DomCode::KEY_P,                    {'p', 'P', 0x00F6, 0x00D6}},
    {DomCode::KEY_Q,                    {'q', 'Q', 0x00E4, 0x00C4}},
    {DomCode::KEY_R,                    {'r', 'R', 0x00AE, 0x00AE}},
    {DomCode::KEY_S,                    {'s', 'S', 0x00DF, 0x00A7}},
    {DomCode::KEY_T,                    {'t', 'T', 0x00FE, 0x00DE}},
    {DomCode::KEY_U,                    {'u', 'U', 0x00FA, 0x00DA}},
    {DomCode::KEY_V,                    {'v', 'V', 'v', 'V'}},
    {DomCode::KEY_W,                    {'w', 'W', 0x00E5, 0x00C5}},
    {DomCode::KEY_X,                    {'x', 'X', 'x', 'X'}},
    {DomCode::KEY_Y,                    {'y', 'Y', 0x00FC, 0x00DC}},
    {DomCode::KEY_Z,                    {'z', 'Z', 0x00E6, 0x00C6}},
    {DomCode::DIGIT1,                   {'1', '!', 0x00A1, 0x00B9}},
    {DomCode::DIGIT2,                   {'2', '@', 0x00B2, DK|0x030B}},
    {DomCode::DIGIT3,                   {'3', '#', 0x00B3, DK|0x0304}},
    {DomCode::DIGIT4,                   {'4', '$', 0x00A4, 0x00A3}},
    {DomCode::DIGIT5,                   {'5', '%', 0x20AC, DK|0x0327}},
    {DomCode::DIGIT6,                   {'6', '^', DK|0x0302, 0x00BC}},
    {DomCode::DIGIT7,                   {'7', '&', 0x00BD, DK|0x031B}},
    {DomCode::DIGIT8,                   {'8', '*', 0x00BE, DK|0x0328}},
    {DomCode::DIGIT9,                   {'9', '(', 0x2018, DK|0x0306}},
    {DomCode::DIGIT0,                   {'0', ')', 0x2019, DK|0x030A}},
    {DomCode::SPACE,                    {' ', ' ', 0x00A0, 0x00A0}},
    {DomCode::MINUS,                    {'-', '_', 0x00A5, DK|0x0323}},
    {DomCode::EQUAL,                    {'=', '+', 0x00D7, 0x00F7}},
    {DomCode::BRACKET_LEFT,             {'[', '{', 0x00AB, 0x201C}},
    {DomCode::BRACKET_RIGHT,            {']', '}', 0x00BB, 0x201D}},
    {DomCode::BACKSLASH,                {'\\', '|', 0x00AC, 0x00A6}},
    {DomCode::SEMICOLON,                {';', ':', 0x00B6, 0x00B0}},
    {DomCode::QUOTE,                    {'\'', '"', DK|0x0301, DK|0x0308}},
    {DomCode::BACKQUOTE,                {'`', '~', DK|0x0300, DK|0x0303}},
    {DomCode::COMMA,                    {',', '<', 0x00E7, 0x00C7}},
    {DomCode::PERIOD,                   {'.', '>', DK|0x0307, DK|0x030C}},
    {DomCode::SLASH,                    {'/', '?', 0x00BF, DK|0x0309}},
    {DomCode::INTL_BACKSLASH,           {'\\', '|', '\\', '|'}},
    {DomCode::INTL_YEN,                 {0x00A5, '|', 0x00A5, '|'}},
    {DomCode::NUMPAD_DIVIDE,            {'/', '/', '/', '/'}},
    {DomCode::NUMPAD_MULTIPLY,          {'*', '*', '*', '*'}},
    {DomCode::NUMPAD_SUBTRACT,          {'-', '-', '-', '-'}},
    {DomCode::NUMPAD_ADD,               {'+', '+', '+', '+'}},
    {DomCode::NUMPAD1,                  {'1', '1', '1', '1'}},
    {DomCode::NUMPAD2,                  {'2', '2', '2', '2'}},
    {DomCode::NUMPAD3,                  {'3', '3', '3', '3'}},
    {DomCode::NUMPAD4,                  {'4', '4', '4', '4'}},
    {DomCode::NUMPAD5,                  {'5', '5', '5', '5'}},
    {DomCode::NUMPAD6,                  {'6', '6', '6', '6'}},
    {DomCode::NUMPAD7,                  {'7', '7', '7', '7'}},
    {DomCode::NUMPAD8,                  {'8', '8', '8', '8'}},
    {DomCode::NUMPAD9,                  {'9', '9', '9', '9'}},
    {DomCode::NUMPAD0,                  {'0', '0', '0', '0'}},
    {DomCode::NUMPAD_DECIMAL,           {'.', '.', '.', '.'}},
    {DomCode::NUMPAD_EQUAL,             {'=', '=', '=', '='}},
    {DomCode::NUMPAD_COMMA,             {',', ',', ',', ','}},
    {DomCode::NUMPAD_PAREN_LEFT,        {'(', '(', '(', '('}},
    {DomCode::NUMPAD_PAREN_RIGHT,       {')', ')', ')', ')'}},
    {DomCode::NUMPAD_SIGN_CHANGE,       {0x00B1, 0x00B1, 0x2213, 0x2213}},
};

const struct NonPrintableCodeEntry {
  DomCode dom_code;
  DomKey dom_key;
  base::char16 character;
} non_printable_code_map[] = {
    {DomCode::ABORT, DomKey::CANCEL},
    {DomCode::AGAIN, DomKey::AGAIN},
    {DomCode::ALT_LEFT, DomKey::ALT},
    {DomCode::ALT_RIGHT, DomKey::ALT},
    {DomCode::ARROW_DOWN, DomKey::ARROW_DOWN},
    {DomCode::ARROW_LEFT, DomKey::ARROW_LEFT},
    {DomCode::ARROW_RIGHT, DomKey::ARROW_RIGHT},
    {DomCode::ARROW_UP, DomKey::ARROW_UP},
    {DomCode::BACKSPACE, DomKey::BACKSPACE, 0x0008},
    {DomCode::BRIGHTNESS_DOWN, DomKey::BRIGHTNESS_DOWN},
    {DomCode::BRIGHTNESS_UP, DomKey::BRIGHTNESS_UP},
    // {DomCode::BRIGHTNESS_AUTO, DomKey::_}
    // {DomCode::BRIGHTNESS_MAXIMUM, DomKey::_}
    // {DomCode::BRIGHTNESS_MINIMIUM, DomKey::_}
    // {DomCode::BRIGHTNESS_TOGGLE, DomKey::_}
    {DomCode::BROWSER_BACK, DomKey::BROWSER_BACK},
    {DomCode::BROWSER_FAVORITES, DomKey::BROWSER_FAVORITES},
    {DomCode::BROWSER_FORWARD, DomKey::BROWSER_FORWARD},
    {DomCode::BROWSER_HOME, DomKey::BROWSER_HOME},
    {DomCode::BROWSER_REFRESH, DomKey::BROWSER_REFRESH},
    {DomCode::BROWSER_SEARCH, DomKey::BROWSER_SEARCH},
    {DomCode::BROWSER_STOP, DomKey::BROWSER_STOP},
    {DomCode::CAPS_LOCK, DomKey::CAPS_LOCK},
    {DomCode::CONTEXT_MENU, DomKey::CONTEXT_MENU},
    {DomCode::CONTROL_LEFT, DomKey::CONTROL},
    {DomCode::CONTROL_RIGHT, DomKey::CONTROL},
    {DomCode::CONVERT, DomKey::CONVERT},
    {DomCode::COPY, DomKey::COPY},
    {DomCode::CUT, DomKey::CUT},
    {DomCode::DEL, DomKey::DEL, 0x007F},
    {DomCode::EJECT, DomKey::EJECT},
    {DomCode::END, DomKey::END},
    {DomCode::ENTER, DomKey::ENTER, 0x000D},
    {DomCode::ESCAPE, DomKey::ESCAPE, 0x001B},
    {DomCode::F1, DomKey::F1},
    {DomCode::F2, DomKey::F2},
    {DomCode::F3, DomKey::F3},
    {DomCode::F4, DomKey::F4},
    {DomCode::F5, DomKey::F5},
    {DomCode::F6, DomKey::F6},
    {DomCode::F7, DomKey::F7},
    {DomCode::F8, DomKey::F8},
    {DomCode::F9, DomKey::F9},
    {DomCode::F10, DomKey::F10},
    {DomCode::F11, DomKey::F11},
    {DomCode::F12, DomKey::F12},
    {DomCode::F13, DomKey::F13},
    {DomCode::F14, DomKey::F14},
    {DomCode::F15, DomKey::F15},
    {DomCode::F16, DomKey::F16},
    {DomCode::F17, DomKey::F17},
    {DomCode::F18, DomKey::F18},
    {DomCode::F19, DomKey::F19},
    {DomCode::F20, DomKey::F20},
    {DomCode::F21, DomKey::F21},
    {DomCode::F22, DomKey::F22},
    {DomCode::F23, DomKey::F23},
    {DomCode::F24, DomKey::F24},
    {DomCode::FIND, DomKey::FIND},
    {DomCode::FN, DomKey::FN},
    {DomCode::FN_LOCK, DomKey::FN_LOCK},
    {DomCode::HELP, DomKey::HELP},
    {DomCode::HOME, DomKey::HOME},
    {DomCode::HYPER, DomKey::HYPER},
    {DomCode::INSERT, DomKey::INSERT},
    // {DomCode::INTL_RO, DomKey::_}
    {DomCode::KANA_MODE, DomKey::KANA_MODE},
    {DomCode::LANG1, DomKey::HANGUL_MODE},
    {DomCode::LANG2, DomKey::HANJA_MODE},
    {DomCode::LANG3, DomKey::KATAKANA},
    {DomCode::LANG4, DomKey::HIRAGANA},
    {DomCode::LANG5, DomKey::ZENKAKU_HANKAKU},
    {DomCode::LAUNCH_APP1, DomKey::LAUNCH_MY_COMPUTER},
    {DomCode::LAUNCH_APP2, DomKey::LAUNCH_CALCULATOR},
    {DomCode::LAUNCH_MAIL, DomKey::LAUNCH_MAIL},
    {DomCode::LAUNCH_SCREEN_SAVER, DomKey::LAUNCH_SCREEN_SAVER},
    // {DomCode::LAUNCH_DOCUMENTS, DomKey::_}
    // {DomCode::LAUNCH_FILE_BROWSER, DomKey::_}
    // {DomCode::LAUNCH_KEYBOARD_LAYOUT, DomKey::_}
    {DomCode::LOCK_SCREEN, DomKey::LAUNCH_SCREEN_SAVER},
    {DomCode::MAIL_FORWARD, DomKey::MAIL_FORWARD},
    {DomCode::MAIL_REPLY, DomKey::MAIL_REPLY},
    {DomCode::MAIL_SEND, DomKey::MAIL_SEND},
    {DomCode::MEDIA_PLAY_PAUSE, DomKey::MEDIA_PLAY_PAUSE},
    {DomCode::MEDIA_SELECT, DomKey::MEDIA_SELECT},
    {DomCode::MEDIA_STOP, DomKey::MEDIA_STOP},
    {DomCode::MEDIA_TRACK_NEXT, DomKey::MEDIA_TRACK_NEXT},
    {DomCode::MEDIA_TRACK_PREVIOUS, DomKey::MEDIA_TRACK_PREVIOUS},
    // {DomCode::MENU, DomKey::_}
    {DomCode::NON_CONVERT, DomKey::NON_CONVERT},
    {DomCode::NUM_LOCK, DomKey::NUM_LOCK},
    {DomCode::NUMPAD_BACKSPACE, DomKey::BACKSPACE, 0x0008},
    {DomCode::NUMPAD_CLEAR, DomKey::CLEAR},
    {DomCode::NUMPAD_ENTER, DomKey::ENTER, 0x000D},
    // {DomCode::NUMPAD_CLEAR_ENTRY, DomKey::_}
    // {DomCode::NUMPAD_MEMORY_ADD, DomKey::_}
    // {DomCode::NUMPAD_MEMORY_CLEAR, DomKey::_}
    // {DomCode::NUMPAD_MEMORY_RECALL, DomKey::_}
    // {DomCode::NUMPAD_MEMORY_STORE, DomKey::_}
    // {DomCode::NUMPAD_MEMORY_SUBTRACT, DomKey::_}
    {DomCode::OPEN, DomKey::OPEN},
    {DomCode::OS_LEFT, DomKey::OS},
    {DomCode::OS_RIGHT, DomKey::OS},
    {DomCode::PAGE_DOWN, DomKey::PAGE_DOWN},
    {DomCode::PAGE_UP, DomKey::PAGE_UP},
    {DomCode::PASTE, DomKey::PASTE},
    {DomCode::PAUSE, DomKey::PAUSE},
    {DomCode::POWER, DomKey::POWER},
    {DomCode::PRINT_SCREEN, DomKey::PRINT_SCREEN},
    {DomCode::PROPS, DomKey::PROPS},
    {DomCode::SCROLL_LOCK, DomKey::SCROLL_LOCK},
    {DomCode::SELECT, DomKey::SELECT},
    // {DomCode::SELECT_TASK, DomKey::_}
    {DomCode::SHIFT_LEFT, DomKey::SHIFT},
    {DomCode::SHIFT_RIGHT, DomKey::SHIFT},
    {DomCode::SUPER, DomKey::SUPER},
    {DomCode::TAB, DomKey::TAB, 0x0009},
    {DomCode::UNDO, DomKey::UNDO},
    // {DomCode::VOICE_COMMAND, DomKey::_}
    {DomCode::VOLUME_DOWN, DomKey::VOLUME_DOWN},
    {DomCode::VOLUME_MUTE, DomKey::VOLUME_MUTE},
    {DomCode::VOLUME_UP, DomKey::VOLUME_UP},
    {DomCode::WAKE_UP, DomKey::WAKE_UP},
    {DomCode::ZOOM_TOGGLE, DomKey::ZOOM_TOGGLE},
};

}  // anonymous namespace

StubKeyboardLayoutEngine::StubKeyboardLayoutEngine() {
}

StubKeyboardLayoutEngine::~StubKeyboardLayoutEngine() {
}

bool StubKeyboardLayoutEngine::CanSetCurrentLayout() const {
  return false;
}

bool StubKeyboardLayoutEngine::SetCurrentLayoutByName(
    const std::string& layout_name) {
  return false;
}

bool StubKeyboardLayoutEngine::UsesISOLevel5Shift() const {
  return false;
}

bool StubKeyboardLayoutEngine::UsesAltGr() const {
  return true;
}

bool StubKeyboardLayoutEngine::Lookup(DomCode dom_code,
                                      int flags,
                                      DomKey* out_dom_key,
                                      base::char16* out_character,
                                      KeyboardCode* out_key_code,
                                      uint32* platform_keycode) const {
  if ((flags & EF_CONTROL_DOWN) == EF_CONTROL_DOWN) {
    if (LookupControlCharacter(dom_code, flags, out_dom_key, out_character,
                               out_key_code)) {
      return true;
    }
  } else {
    for (size_t i = 0; i < arraysize(printable_code_map); ++i) {
      const PrintableCodeEntry* e = &printable_code_map[i];
      if (e->dom_code == dom_code) {
        int state = (((flags & EF_ALTGR_DOWN) == EF_ALTGR_DOWN) << 1) |
                    ((flags & EF_SHIFT_DOWN) == EF_SHIFT_DOWN);
        base::char16 ch = e->character[state];
        *out_dom_key = (ch & DK) ? DomKey::DEAD : DomKey::CHARACTER;
        *out_character = ch;
        if ((flags & EF_CAPS_LOCK_DOWN) == EF_CAPS_LOCK_DOWN) {
          ch = (ch & ~DK) | 0x20;
          if ((ch >= 'a') && (ch <= 'z'))
            *out_character = e->character[state ^ 1];
        }
        *out_key_code = DomCodeToNonLocatedKeyboardCode(dom_code);
        return true;
      }
    }
  }

  for (size_t i = 0; i < arraysize(non_printable_code_map); ++i) {
    const NonPrintableCodeEntry* e = &non_printable_code_map[i];
    if (e->dom_code == dom_code) {
      *out_dom_key = e->dom_key;
      *out_character = e->character;
      *out_key_code = NonPrintableDomKeyToKeyboardCode(e->dom_key);
      return true;
    }
  }

  return false;
}

}  // namespace ui
