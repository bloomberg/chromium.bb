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

// TODO(kpschoedel): move this to a shared location.
// Map DOM Level 3 .key values to legacy Windows-based VKEY values.
// This applies to non-printable keys.
KeyboardCode DomKeyToKeyboardCode(DomKey dom_key) {
  switch (dom_key) {
    // No value.
    case DomKey::NONE:
      return VKEY_UNKNOWN;
    // Special Key Values
    // http://www.w3.org/TR/DOM-Level-3-Events-key/#keys-special
    case DomKey::UNIDENTIFIED:
      return VKEY_UNKNOWN;
    // Modifier Keys
    // http://www.w3.org/TR/DOM-Level-3-Events-key/#keys-modifier
    case DomKey::ALT:
      return VKEY_MENU;
    case DomKey::ALT_GRAPH:
      return VKEY_ALTGR;
    case DomKey::CAPS_LOCK:
      return VKEY_CAPITAL;
    case DomKey::CONTROL:
      return VKEY_CONTROL;
    case DomKey::NUM_LOCK:
      return VKEY_NUMLOCK;
    case DomKey::OS:
      return VKEY_LWIN;
    case DomKey::SCROLL_LOCK:
      return VKEY_SCROLL;
    case DomKey::SHIFT:
      return VKEY_SHIFT;
    // Whitespace Keys
    // http://www.w3.org/TR/DOM-Level-3-Events-key/#keys-whitespace
    case DomKey::ENTER:
      return VKEY_RETURN;
    case DomKey::SEPARATOR:
      return VKEY_SEPARATOR;
    case DomKey::TAB:
      return VKEY_TAB;
    // Navigation Keys
    // http://www.w3.org/TR/DOM-Level-3-Events-key/#keys-navigation
    case DomKey::ARROW_DOWN:
      return VKEY_DOWN;
    case DomKey::ARROW_LEFT:
      return VKEY_LEFT;
    case DomKey::ARROW_RIGHT:
      return VKEY_RIGHT;
    case DomKey::ARROW_UP:
      return VKEY_UP;
    case DomKey::END:
      return VKEY_END;
    case DomKey::HOME:
      return VKEY_HOME;
    case DomKey::PAGE_DOWN:
      return VKEY_NEXT;
    case DomKey::PAGE_UP:
      return VKEY_PRIOR;
    // Editing Keys
    // http://www.w3.org/TR/DOM-Level-3-Events-key/#keys-editing
    case DomKey::BACKSPACE:
      return VKEY_BACK;
    case DomKey::CLEAR:
      return VKEY_CLEAR;
    case DomKey::CR_SEL:
      return VKEY_CRSEL;
    case DomKey::DEL:
      return VKEY_DELETE;
    case DomKey::ERASE_EOF:
      return VKEY_EREOF;
    case DomKey::EX_SEL:
      return VKEY_EXSEL;
    case DomKey::INSERT:
      return VKEY_INSERT;
    // UI Keys
    // http://www.w3.org/TR/DOM-Level-3-Events-key/#keys-ui
    case DomKey::ACCEPT:
      return VKEY_ACCEPT;
    case DomKey::ATTN:
      return VKEY_ATTN;
    case DomKey::CONTEXT_MENU:
      return VKEY_APPS;
    case DomKey::ESCAPE:
      return VKEY_ESCAPE;
    case DomKey::EXECUTE:
      return VKEY_EXECUTE;
    case DomKey::HELP:
      return VKEY_HELP;
    case DomKey::PAUSE:
      return VKEY_PAUSE;
    case DomKey::PLAY:
      return VKEY_PLAY;
    case DomKey::SELECT:
      return VKEY_SELECT;
    // Device Keys
    // http://www.w3.org/TR/DOM-Level-3-Events-key/#keys-device
    case DomKey::BRIGHTNESS_DOWN:
      return VKEY_BRIGHTNESS_DOWN;
    case DomKey::BRIGHTNESS_UP:
      return VKEY_BRIGHTNESS_UP;
    case DomKey::POWER:
      return VKEY_POWER;
    case DomKey::PRINT_SCREEN:
      return VKEY_SNAPSHOT;
    // IME and Composition Keys
    // http://www.w3.org/TR/DOM-Level-3-Events-key/#keys-composition
#if 0 // TODO(kpschoedel)
    case DomKey::COMPOSE:
      return VKEY_COMPOSE;
#endif
    case DomKey::CONVERT:
      return VKEY_CONVERT;
    case DomKey::FINAL_MODE:
      return VKEY_FINAL;
    case DomKey::MODE_CHANGE:
      return VKEY_MODECHANGE;
    case DomKey::NON_CONVERT:
      return VKEY_NONCONVERT;
    case DomKey::PROCESS:
      return VKEY_PROCESSKEY;
    // Keys specific to Korean keyboards
    case DomKey::HANGUL_MODE:
      return VKEY_HANGUL;
    case DomKey::HANJA_MODE:
      return VKEY_HANJA;
    case DomKey::JUNJA_MODE:
      return VKEY_JUNJA;
    // Keys specific to Japanese keyboards
    case DomKey::HANKAKU:
      return VKEY_DBE_SBCSCHAR;
    case DomKey::KANA_MODE:
      return VKEY_KANA;
    case DomKey::KANJI_MODE:
      return VKEY_KANJI;
    case DomKey::ZENKAKU:
      return VKEY_DBE_DBCSCHAR;
    case DomKey::ZENKAKU_HANKAKU:
      return VKEY_DBE_DBCSCHAR;
    // General-Purpose Function Keys
    // http://www.w3.org/TR/DOM-Level-3-Events-key/#keys-function
    case DomKey::F1:
      return VKEY_F1;
    case DomKey::F2:
      return VKEY_F2;
    case DomKey::F3:
      return VKEY_F3;
    case DomKey::F4:
      return VKEY_F4;
    case DomKey::F5:
      return VKEY_F5;
    case DomKey::F6:
      return VKEY_F6;
    case DomKey::F7:
      return VKEY_F7;
    case DomKey::F8:
      return VKEY_F8;
    case DomKey::F9:
      return VKEY_F9;
    case DomKey::F10:
      return VKEY_F10;
    case DomKey::F11:
      return VKEY_F11;
    case DomKey::F12:
      return VKEY_F12;
    case DomKey::F13:
      return VKEY_F13;
    case DomKey::F14:
      return VKEY_F14;
    case DomKey::F15:
      return VKEY_F15;
    case DomKey::F16:
      return VKEY_F16;
    case DomKey::F17:
      return VKEY_F17;
    case DomKey::F18:
      return VKEY_F18;
    case DomKey::F19:
      return VKEY_F19;
    case DomKey::F20:
      return VKEY_F20;
    case DomKey::F21:
      return VKEY_F21;
    case DomKey::F22:
      return VKEY_F22;
    case DomKey::F23:
      return VKEY_F23;
    case DomKey::F24:
      return VKEY_F24;
    // Multimedia Keys
    // http://www.w3.org/TR/DOM-Level-3-Events-key/#keys-multimedia
    case DomKey::MEDIA_PLAY_PAUSE:
      return VKEY_MEDIA_PLAY_PAUSE;
    case DomKey::MEDIA_SELECT:
      return VKEY_MEDIA_LAUNCH_MEDIA_SELECT;
    case DomKey::MEDIA_STOP:
      return VKEY_MEDIA_STOP;
    case DomKey::MEDIA_TRACK_NEXT:
      return VKEY_MEDIA_NEXT_TRACK;
    case DomKey::MEDIA_TRACK_PREVIOUS:
      return VKEY_MEDIA_PREV_TRACK;
    case DomKey::PRINT:
      return VKEY_PRINT;
    case DomKey::VOLUME_DOWN:
      return VKEY_VOLUME_DOWN;
    case DomKey::VOLUME_MUTE:
      return VKEY_VOLUME_MUTE;
    case DomKey::VOLUME_UP:
      return VKEY_VOLUME_UP;
    // Application Keys
    // http://www.w3.org/TR/DOM-Level-3-Events-key/#keys-apps
    case DomKey::LAUNCH_CALCULATOR:
      return VKEY_MEDIA_LAUNCH_APP2;
    case DomKey::LAUNCH_MAIL:
      return VKEY_MEDIA_LAUNCH_MAIL;
    case DomKey::LAUNCH_MY_COMPUTER:
      return VKEY_MEDIA_LAUNCH_APP1;
    // Browser Keys
    // http://www.w3.org/TR/DOM-Level-3-Events-key/#keys-browser
    case DomKey::BROWSER_BACK:
      return VKEY_BACK;
    case DomKey::BROWSER_FAVORITES:
      return VKEY_BROWSER_FAVORITES;
    case DomKey::BROWSER_FORWARD:
      return VKEY_BROWSER_FORWARD;
    case DomKey::BROWSER_HOME:
      return VKEY_BROWSER_HOME;
    case DomKey::BROWSER_REFRESH:
      return VKEY_BROWSER_REFRESH;
    case DomKey::BROWSER_SEARCH:
      return VKEY_BROWSER_SEARCH;
    case DomKey::BROWSER_STOP:
      return VKEY_BROWSER_STOP;
    // Media Controller Keys
    // http://www.w3.org/TR/DOM-Level-3-Events-key/#keys-media-controller
    case DomKey::ZOOM_TOGGLE:
      return VKEY_ZOOM;
    // No value.
    default:
      return VKEY_UNKNOWN;
  }
}

// TODO(kpschoedel): move this to a shared location.
// This table maps DOM Level 3 .code values to legacy Windows-based VKEY
// values, where the VKEYs are interpreted positionally.
const struct DomCodeToKeyboardCodeEntry {
  DomCode dom_code;
  KeyboardCode key_code;
} dom_code_to_keyboard_code[] = {
    // Entries are ordered by numeric value of the DomCode enum,
    // which is the USB physical key code.
    // DomCode::HYPER                              0x000010 Hyper
    // DomCode::SUPER                              0x000011 Super
    // DomCode::FN                                 0x000012 Fn
    // DomCode::FN_LOCK                            0x000013 FLock
    // DomCode::SUSPEND                            0x000014 Suspend
    // DomCode::RESUME                             0x000015 Resume
    // DomCode::TURBO                              0x000016 Turbo
    {DomCode::SLEEP, VKEY_SLEEP},  // 0x010082 Sleep
    // DomCode::WAKE_UP                            0x010083 WakeUp
    {DomCode::KEY_A, VKEY_A},                   // 0x070004 KeyA
    {DomCode::KEY_B, VKEY_B},                   // 0x070005 KeyB
    {DomCode::KEY_C, VKEY_C},                   // 0x070006 KeyC
    {DomCode::KEY_D, VKEY_D},                   // 0x070007 KeyD
    {DomCode::KEY_E, VKEY_E},                   // 0x070008 KeyE
    {DomCode::KEY_F, VKEY_F},                   // 0x070009 KeyF
    {DomCode::KEY_G, VKEY_G},                   // 0x07000A KeyG
    {DomCode::KEY_H, VKEY_H},                   // 0x07000B KeyH
    {DomCode::KEY_I, VKEY_I},                   // 0x07000C KeyI
    {DomCode::KEY_J, VKEY_J},                   // 0x07000D KeyJ
    {DomCode::KEY_K, VKEY_K},                   // 0x07000E KeyK
    {DomCode::KEY_L, VKEY_L},                   // 0x07000F KeyL
    {DomCode::KEY_M, VKEY_M},                   // 0x070010 KeyM
    {DomCode::KEY_N, VKEY_N},                   // 0x070011 KeyN
    {DomCode::KEY_O, VKEY_O},                   // 0x070012 KeyO
    {DomCode::KEY_P, VKEY_P},                   // 0x070013 KeyP
    {DomCode::KEY_Q, VKEY_Q},                   // 0x070014 KeyQ
    {DomCode::KEY_R, VKEY_R},                   // 0x070015 KeyR
    {DomCode::KEY_S, VKEY_S},                   // 0x070016 KeyS
    {DomCode::KEY_T, VKEY_T},                   // 0x070017 KeyT
    {DomCode::KEY_U, VKEY_U},                   // 0x070018 KeyU
    {DomCode::KEY_V, VKEY_V},                   // 0x070019 KeyV
    {DomCode::KEY_W, VKEY_W},                   // 0x07001A KeyW
    {DomCode::KEY_X, VKEY_X},                   // 0x07001B KeyX
    {DomCode::KEY_Y, VKEY_Y},                   // 0x07001C KeyY
    {DomCode::KEY_Z, VKEY_Z},                   // 0x07001D KeyZ
    {DomCode::DIGIT1, VKEY_1},                  // 0x07001E Digit1
    {DomCode::DIGIT2, VKEY_2},                  // 0x07001F Digit2
    {DomCode::DIGIT3, VKEY_3},                  // 0x070020 Digit3
    {DomCode::DIGIT4, VKEY_4},                  // 0x070021 Digit4
    {DomCode::DIGIT5, VKEY_5},                  // 0x070022 Digit5
    {DomCode::DIGIT6, VKEY_6},                  // 0x070023 Digit6
    {DomCode::DIGIT7, VKEY_7},                  // 0x070024 Digit7
    {DomCode::DIGIT8, VKEY_8},                  // 0x070025 Digit8
    {DomCode::DIGIT9, VKEY_9},                  // 0x070026 Digit9
    {DomCode::DIGIT0, VKEY_0},                  // 0x070027 Digit0
    {DomCode::ENTER, VKEY_RETURN},              // 0x070028 Enter
    {DomCode::ESCAPE, VKEY_ESCAPE},             // 0x070029 Escape
    {DomCode::BACKSPACE, VKEY_BACK},            // 0x07002A Backspace
    {DomCode::TAB, VKEY_TAB},                   // 0x07002B Tab
    {DomCode::SPACE, VKEY_SPACE},               // 0x07002C Space
    {DomCode::MINUS, VKEY_OEM_MINUS},           // 0x07002D Minus
    {DomCode::EQUAL, VKEY_OEM_PLUS},            // 0x07002E Equal
    {DomCode::BRACKET_LEFT, VKEY_OEM_4},        // 0x07002F BracketLeft
    {DomCode::BRACKET_RIGHT, VKEY_OEM_6},       // 0x070030 BracketRight
    {DomCode::BACKSLASH, VKEY_OEM_5},           // 0x070031 Backslash
    // DomCode::INTL_HASH, VKEY_OEM_5           // 0x070032 IntlHash
    {DomCode::SEMICOLON, VKEY_OEM_1},           // 0x070033 Semicolon
    {DomCode::QUOTE, VKEY_OEM_7},               // 0x070034 Quote
    {DomCode::BACKQUOTE, VKEY_OEM_3},           // 0x070035 Backquote
    {DomCode::COMMA, VKEY_OEM_COMMA},           // 0x070036 Comma
    {DomCode::PERIOD, VKEY_OEM_PERIOD},         // 0x070037 Period
    {DomCode::SLASH, VKEY_OEM_2},               // 0x070038 Slash
    {DomCode::CAPS_LOCK, VKEY_CAPITAL},         // 0x070039 CapsLock
    {DomCode::F1, VKEY_F1},                     // 0x07003A F1
    {DomCode::F2, VKEY_F2},                     // 0x07003B F2
    {DomCode::F3, VKEY_F3},                     // 0x07003C F3
    {DomCode::F4, VKEY_F4},                     // 0x07003D F4
    {DomCode::F5, VKEY_F5},                     // 0x07003E F5
    {DomCode::F6, VKEY_F6},                     // 0x07003F F6
    {DomCode::F7, VKEY_F7},                     // 0x070040 F7
    {DomCode::F8, VKEY_F8},                     // 0x070041 F8
    {DomCode::F9, VKEY_F9},                     // 0x070042 F9
    {DomCode::F10, VKEY_F10},                   // 0x070043 F10
    {DomCode::F11, VKEY_F11},                   // 0x070044 F11
    {DomCode::F12, VKEY_F12},                   // 0x070045 F12
    {DomCode::PRINT_SCREEN, VKEY_SNAPSHOT},     // 0x070046 PrintScreen
    {DomCode::SCROLL_LOCK, VKEY_SCROLL},        // 0x070047 ScrollLock
    {DomCode::PAUSE, VKEY_PAUSE},               // 0x070048 Pause
    {DomCode::INSERT, VKEY_INSERT},             // 0x070049 Insert
    {DomCode::HOME, VKEY_HOME},                 // 0x07004A Home
    {DomCode::PAGE_UP, VKEY_PRIOR},             // 0x07004B PageUp
    {DomCode::DEL, VKEY_DELETE},                // 0x07004C Delete
    {DomCode::END, VKEY_END},                   // 0x07004D End
    {DomCode::PAGE_DOWN, VKEY_NEXT},            // 0x07004E PageDown
    {DomCode::ARROW_RIGHT, VKEY_RIGHT},         // 0x07004F ArrowRight
    {DomCode::ARROW_LEFT, VKEY_LEFT},           // 0x070050 ArrowLeft
    {DomCode::ARROW_DOWN, VKEY_DOWN},           // 0x070051 ArrowDown
    {DomCode::ARROW_UP, VKEY_UP},               // 0x070052 ArrowUp
    {DomCode::NUM_LOCK, VKEY_NUMLOCK},          // 0x070053 NumLock
    {DomCode::NUMPAD_DIVIDE, VKEY_DIVIDE},      // 0x070054 NumpadDivide
    {DomCode::NUMPAD_MULTIPLY, VKEY_MULTIPLY},  // 0x070055 NumpadMultiply
    {DomCode::NUMPAD_SUBTRACT, VKEY_SUBTRACT},  // 0x070056 NumpadSubtract
    {DomCode::NUMPAD_ADD, VKEY_ADD},            // 0x070057 NumpadAdd
    {DomCode::NUMPAD_ENTER, VKEY_RETURN},       // 0x070058 NumpadEnter
    {DomCode::NUMPAD1, VKEY_NUMPAD1},           // 0x070059 Numpad1
    {DomCode::NUMPAD2, VKEY_NUMPAD2},           // 0x07005A Numpad2
    {DomCode::NUMPAD3, VKEY_NUMPAD3},           // 0x07005B Numpad3
    {DomCode::NUMPAD4, VKEY_NUMPAD4},           // 0x07005C Numpad4
    {DomCode::NUMPAD5, VKEY_NUMPAD5},           // 0x07005D Numpad5
    {DomCode::NUMPAD6, VKEY_NUMPAD6},           // 0x07005E Numpad6
    {DomCode::NUMPAD7, VKEY_NUMPAD7},           // 0x07005F Numpad7
    {DomCode::NUMPAD8, VKEY_NUMPAD8},           // 0x070060 Numpad8
    {DomCode::NUMPAD9, VKEY_NUMPAD9},           // 0x070061 Numpad9
    {DomCode::NUMPAD0, VKEY_NUMPAD0},           // 0x070062 Numpad0
    {DomCode::NUMPAD_DECIMAL, VKEY_DECIMAL},    // 0x070063 NumpadDecimal
    {DomCode::INTL_BACKSLASH, VKEY_OEM_102},    // 0x070064 IntlBackslash
    {DomCode::CONTEXT_MENU, VKEY_APPS},         // 0x070065 ContextMenu
    {DomCode::POWER, VKEY_POWER},               // 0x070066 Power
    // DomCode::NUMPAD_EQUAL                       0x070067 NumpadEqual
    // DomCode::OPEN                               0x070074 Open
    {DomCode::HELP, VKEY_HELP},      // 0x070075 Help
    {DomCode::SELECT, VKEY_SELECT},  // 0x070077 Select
    // DomCode::AGAIN                             0x070079 Again
    // DomCode::UNDO                              0x07007A Undo
    // DomCode::CUT                               0x07007B Cut
    // DomCode::COPY                              0x07007C Copy
    // DomCode::PASTE                             0x07007D Paste
    // DomCode::FIND                              0x07007E Find
    {DomCode::VOLUME_MUTE, VKEY_VOLUME_MUTE},  // 0x07007F VolumeMute
    {DomCode::VOLUME_UP, VKEY_VOLUME_UP},      // 0x070080 VolumeUp
    {DomCode::VOLUME_DOWN, VKEY_VOLUME_DOWN},  // 0x070081 VolumeDown
    {DomCode::NUMPAD_COMMA, VKEY_OEM_COMMA},   // 0x070085 NumpadComma
    {DomCode::INTL_RO, VKEY_OEM_102},          // 0x070087 IntlRo
    {DomCode::KANA_MODE, VKEY_KANA},           // 0x070088 KanaMode
    {DomCode::INTL_YEN, VKEY_OEM_5},           // 0x070089 IntlYen
    {DomCode::CONVERT, VKEY_CONVERT},          // 0x07008A Convert
    {DomCode::NON_CONVERT, VKEY_NONCONVERT},   // 0x07008B NonConvert
    {DomCode::LANG1, VKEY_KANA},               // 0x070090 Lang1
    {DomCode::LANG2, VKEY_KANJI},              // 0x070091 Lang2
    // DomCode::LANG3                             0x070092 Lang3
    // DomCode::LANG4                             0x070093 Lang4
    // DomCode::LANG5                             0x070094 Lang5
    // DomCode::ABORT                             0x07009B Abort
    // DomCode::PROPS                             0x0700A3 Props
    // DomCode::NUMPAD_PAREN_LEFT                 0x0700B6 NumpadParenLeft
    // DomCode::NUMPAD_PAREN_RIGHT                0x0700B7 NumpadParenRight
    {DomCode::NUMPAD_BACKSPACE, VKEY_BACK},  // 0x0700BB NumpadBackspace
    // DomCode::NUMPAD_MEMORY_STORE                0x0700D0 NumpadMemoryStore
    // DomCode::NUMPAD_MEMORY_RECALL               0x0700D1 NumpadMemoryRecall
    // DomCode::NUMPAD_MEMORY_CLEAR                0x0700D2 NumpadMemoryClear
    // DomCode::NUMPAD_MEMORY_ADD                  0x0700D3 NumpadMemoryAdd
    // DomCode::NUMPAD_MEMORY_SUBTRACT             0x0700D4
    // NumpadMemorySubtract
    {DomCode::NUMPAD_CLEAR, VKEY_CLEAR},        // 0x0700D8 NumpadClear
    {DomCode::NUMPAD_CLEAR_ENTRY, VKEY_CLEAR},  // 0x0700D9 NumpadClearEntry
    {DomCode::CONTROL_LEFT, VKEY_LCONTROL},     // 0x0700E0 ControlLeft
    {DomCode::SHIFT_LEFT, VKEY_LSHIFT},         // 0x0700E1 ShiftLeft
    {DomCode::ALT_LEFT, VKEY_LMENU},            // 0x0700E2 AltLeft
    {DomCode::OS_LEFT, VKEY_LWIN},              // 0x0700E3 OSLeft
    {DomCode::CONTROL_RIGHT, VKEY_RCONTROL},    // 0x0700E4 ControlRight
    {DomCode::SHIFT_RIGHT, VKEY_RSHIFT},        // 0x0700E5 ShiftRight
    {DomCode::ALT_RIGHT, VKEY_RMENU},           // 0x0700E6 AltRight
    {DomCode::OS_RIGHT, VKEY_RWIN},             // 0x0700E7 OSRight
    {DomCode::MEDIA_TRACK_NEXT,
     VKEY_MEDIA_NEXT_TRACK},  // 0x0C00B5 MediaTrackNext
    {DomCode::MEDIA_TRACK_PREVIOUS,
     VKEY_MEDIA_PREV_TRACK},                 // 0x0C00B6 MediaTrackPrevious
    {DomCode::MEDIA_STOP, VKEY_MEDIA_STOP},  // 0x0C00B7 MediaStop
    // DomCode::EJECT                           0x0C00B8 Eject
    {DomCode::MEDIA_PLAY_PAUSE,
     VKEY_MEDIA_PLAY_PAUSE},  // 0x0C00CD MediaPlayPause
    {DomCode::MEDIA_SELECT,
     VKEY_MEDIA_LAUNCH_MEDIA_SELECT},                // 0x0C0183 MediaSelect
    {DomCode::LAUNCH_MAIL, VKEY_MEDIA_LAUNCH_MAIL},  // 0x0C018A LaunchMail
    {DomCode::LAUNCH_APP2, VKEY_MEDIA_LAUNCH_APP2},  // 0x0C0192 LaunchApp2
    {DomCode::LAUNCH_APP1, VKEY_MEDIA_LAUNCH_APP1},  // 0x0C0194 LaunchApp1
    {DomCode::BROWSER_SEARCH, VKEY_BROWSER_SEARCH},  // 0x0C0221 BrowserSearch
    {DomCode::BROWSER_HOME, VKEY_BROWSER_HOME},      // 0x0C0223 BrowserHome
    {DomCode::BROWSER_BACK, VKEY_BROWSER_BACK},      // 0x0C0224 BrowserBack
    {DomCode::BROWSER_FORWARD,
     VKEY_BROWSER_FORWARD},                      // 0x0C0225 BrowserForward
    {DomCode::BROWSER_STOP, VKEY_BROWSER_STOP},  // 0x0C0226 BrowserStop
    {DomCode::BROWSER_REFRESH,
     VKEY_BROWSER_REFRESH},  // 0x0C0227 BrowserRefresh
    {DomCode::BROWSER_FAVORITES,
     VKEY_BROWSER_FAVORITES},  // 0x0C022A BrowserFavorites
};

// TODO(kpschoedel): move this to a shared location.
// Map DOM Level 3 .code values to legacy Windows-based VKEY values,
// where the VKEYs are interpreted positionally.
KeyboardCode DomCodeToKeyboardCode(DomCode dom_code) {
  const DomCodeToKeyboardCodeEntry* end =
      dom_code_to_keyboard_code + arraysize(dom_code_to_keyboard_code);
  const DomCodeToKeyboardCodeEntry* found =
      std::lower_bound(dom_code_to_keyboard_code, end, dom_code,
                       [](const DomCodeToKeyboardCodeEntry& a, DomCode b) {
        return static_cast<int>(a.dom_code) < static_cast<int>(b);
      });
  if ((found != end) && (found->dom_code == dom_code))
    return found->key_code;
  return VKEY_UNKNOWN;
}

// TODO(kpschoedel): move this to a shared location.
bool ControlCharacter(DomCode dom_code,
                      DomKey* dom_key,
                      base::char16* character) {
  int code = static_cast<int>(dom_code);
  const int kKeyA = static_cast<int>(DomCode::KEY_A);
  // Control-A - Control-Z map to 0x01 - 0x1A.
  if (code >= kKeyA && code <= static_cast<int>(DomCode::KEY_Z)) {
    *character = static_cast<base::char16>(code - kKeyA + 1);
    switch (dom_code) {
      case DomCode::KEY_H:
        *dom_key = DomKey::BACKSPACE;
      case DomCode::KEY_I:
        *dom_key = DomKey::TAB;
      case DomCode::KEY_M:
        *dom_key = DomKey::ENTER;
      default:
        *dom_key = DomKey::CHARACTER;
    }
    return true;
  }
  switch (dom_code) {
    case DomCode::DIGIT2:
      // NUL
      *character = 0;
      *dom_key = DomKey::CHARACTER;
      return true;
    case DomCode::ENTER:
      // NL
      *character = 0x0A;
      *dom_key = DomKey::CHARACTER;
      return true;
    case DomCode::BRACKET_LEFT:
      // ESC
      *character = 0x1B;
      *dom_key = DomKey::ESCAPE;
      return true;
    case DomCode::BACKSLASH:
      // FS
      *character = 0x1C;
      *dom_key = DomKey::CHARACTER;
      return true;
    case DomCode::BRACKET_RIGHT:
      // GS
      *character = 0x1D;
      *dom_key = DomKey::CHARACTER;
      return true;
    case DomCode::DIGIT6:
      // RS
      *character = 0x1E;
      *dom_key = DomKey::CHARACTER;
      return true;
    case DomCode::MINUS:
      // US
      *character = 0x1F;
      *dom_key = DomKey::CHARACTER;
      return true;
    default:
      return false;
  }
}

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
                                      KeyboardCode* out_key_code) const {
  if ((flags & EF_CONTROL_DOWN) == EF_CONTROL_DOWN) {
    if (ControlCharacter(dom_code, out_dom_key, out_character)) {
      *out_key_code = DomCodeToKeyboardCode(dom_code);
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
        *out_key_code = DomCodeToKeyboardCode(dom_code);
        return true;
      }
    }
  }

  for (size_t i = 0; i < arraysize(non_printable_code_map); ++i) {
    const NonPrintableCodeEntry* e = &non_printable_code_map[i];
    if (e->dom_code == dom_code) {
      *out_dom_key = e->dom_key;
      *out_character = e->character;
      *out_key_code = DomKeyToKeyboardCode(e->dom_key);
      return true;
    }
  }

  return false;
}

}  // namespace ui
