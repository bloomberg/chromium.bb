// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/layout/layout_util.h"

#include <algorithm>

#include "ui/events/event_constants.h"
#include "ui/events/keycodes/dom3/dom_code.h"
#include "ui/events/keycodes/dom3/dom_key.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"

namespace ui {

namespace {

// This table, used by DomKeyToKeyboardCode(), maps DOM Level 3 .code
// values to legacy Windows-based VKEY values, where the VKEYs are
// interpreted positionally.
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
    {DomCode::KEY_A, VKEY_A},              // 0x070004 KeyA
    {DomCode::KEY_B, VKEY_B},              // 0x070005 KeyB
    {DomCode::KEY_C, VKEY_C},              // 0x070006 KeyC
    {DomCode::KEY_D, VKEY_D},              // 0x070007 KeyD
    {DomCode::KEY_E, VKEY_E},              // 0x070008 KeyE
    {DomCode::KEY_F, VKEY_F},              // 0x070009 KeyF
    {DomCode::KEY_G, VKEY_G},              // 0x07000A KeyG
    {DomCode::KEY_H, VKEY_H},              // 0x07000B KeyH
    {DomCode::KEY_I, VKEY_I},              // 0x07000C KeyI
    {DomCode::KEY_J, VKEY_J},              // 0x07000D KeyJ
    {DomCode::KEY_K, VKEY_K},              // 0x07000E KeyK
    {DomCode::KEY_L, VKEY_L},              // 0x07000F KeyL
    {DomCode::KEY_M, VKEY_M},              // 0x070010 KeyM
    {DomCode::KEY_N, VKEY_N},              // 0x070011 KeyN
    {DomCode::KEY_O, VKEY_O},              // 0x070012 KeyO
    {DomCode::KEY_P, VKEY_P},              // 0x070013 KeyP
    {DomCode::KEY_Q, VKEY_Q},              // 0x070014 KeyQ
    {DomCode::KEY_R, VKEY_R},              // 0x070015 KeyR
    {DomCode::KEY_S, VKEY_S},              // 0x070016 KeyS
    {DomCode::KEY_T, VKEY_T},              // 0x070017 KeyT
    {DomCode::KEY_U, VKEY_U},              // 0x070018 KeyU
    {DomCode::KEY_V, VKEY_V},              // 0x070019 KeyV
    {DomCode::KEY_W, VKEY_W},              // 0x07001A KeyW
    {DomCode::KEY_X, VKEY_X},              // 0x07001B KeyX
    {DomCode::KEY_Y, VKEY_Y},              // 0x07001C KeyY
    {DomCode::KEY_Z, VKEY_Z},              // 0x07001D KeyZ
    {DomCode::DIGIT1, VKEY_1},             // 0x07001E Digit1
    {DomCode::DIGIT2, VKEY_2},             // 0x07001F Digit2
    {DomCode::DIGIT3, VKEY_3},             // 0x070020 Digit3
    {DomCode::DIGIT4, VKEY_4},             // 0x070021 Digit4
    {DomCode::DIGIT5, VKEY_5},             // 0x070022 Digit5
    {DomCode::DIGIT6, VKEY_6},             // 0x070023 Digit6
    {DomCode::DIGIT7, VKEY_7},             // 0x070024 Digit7
    {DomCode::DIGIT8, VKEY_8},             // 0x070025 Digit8
    {DomCode::DIGIT9, VKEY_9},             // 0x070026 Digit9
    {DomCode::DIGIT0, VKEY_0},             // 0x070027 Digit0
    {DomCode::ENTER, VKEY_RETURN},         // 0x070028 Enter
    {DomCode::ESCAPE, VKEY_ESCAPE},        // 0x070029 Escape
    {DomCode::BACKSPACE, VKEY_BACK},       // 0x07002A Backspace
    {DomCode::TAB, VKEY_TAB},              // 0x07002B Tab
    {DomCode::SPACE, VKEY_SPACE},          // 0x07002C Space
    {DomCode::MINUS, VKEY_OEM_MINUS},      // 0x07002D Minus
    {DomCode::EQUAL, VKEY_OEM_PLUS},       // 0x07002E Equal
    {DomCode::BRACKET_LEFT, VKEY_OEM_4},   // 0x07002F BracketLeft
    {DomCode::BRACKET_RIGHT, VKEY_OEM_6},  // 0x070030 BracketRight
    {DomCode::BACKSLASH, VKEY_OEM_5},      // 0x070031 Backslash
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

}  // anonymous namespace

// Returns a Windows-based VKEY for a non-printable DOM Level 3 |key|.
// The returned VKEY is non-positional (e.g. VKEY_SHIFT).
KeyboardCode NonPrintableDomKeyToKeyboardCode(DomKey dom_key) {
  switch (dom_key) {
    // No value.
    case DomKey::NONE:
      return VKEY_UNKNOWN;
    // Character values.
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
#if 0  // TODO(kpschoedel)
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
      return VKEY_BROWSER_BACK;
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

// Returns the Windows-based VKEY value corresponding to a DOM Level 3 |code|.
// The returned VKEY is located (e.g. VKEY_LSHIFT).
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

// Returns the Windows-based VKEY value corresponding to a DOM Level 3 |code|.
// The returned VKEY is non-located (e.g. VKEY_SHIFT).
KeyboardCode DomCodeToNonLocatedKeyboardCode(DomCode dom_code) {
  return LocatedToNonLocatedKeyboardCode(DomCodeToKeyboardCode(dom_code));
}

bool LookupControlCharacter(DomCode dom_code,
                            int flags,
                            DomKey* dom_key,
                            base::char16* character,
                            KeyboardCode* key_code) {
  if ((flags & EF_CONTROL_DOWN) == 0)
    return false;

  int code = static_cast<int>(dom_code);
  const int kKeyA = static_cast<int>(DomCode::KEY_A);
  // Control-A - Control-Z map to 0x01 - 0x1A.
  if (code >= kKeyA && code <= static_cast<int>(DomCode::KEY_Z)) {
    *character = static_cast<base::char16>(code - kKeyA + 1);
    *key_code = static_cast<KeyboardCode>(code - kKeyA + VKEY_A);
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
      *key_code = VKEY_2;
      return true;
    case DomCode::ENTER:
      // NL
      *character = 0x0A;
      *dom_key = DomKey::CHARACTER;
      *key_code = VKEY_RETURN;
      return true;
    case DomCode::BRACKET_LEFT:
      // ESC
      *character = 0x1B;
      *dom_key = DomKey::ESCAPE;
      *key_code = VKEY_OEM_4;
      return true;
    case DomCode::BACKSLASH:
      // FS
      *character = 0x1C;
      *dom_key = DomKey::CHARACTER;
      *key_code = VKEY_OEM_5;
      return true;
    case DomCode::BRACKET_RIGHT:
      // GS
      *character = 0x1D;
      *dom_key = DomKey::CHARACTER;
      *key_code = VKEY_OEM_6;
      return true;
    case DomCode::DIGIT6:
      // RS
      *character = 0x1E;
      *dom_key = DomKey::CHARACTER;
      *key_code = VKEY_6;
      return true;
    case DomCode::MINUS:
      // US
      *character = 0x1F;
      *dom_key = DomKey::CHARACTER;
      *key_code = VKEY_OEM_MINUS;
      return true;
    default:
      return false;
  }
}

int ModifierDomKeyToEventFlag(DomKey key) {
  switch (key) {
    case DomKey::ALT:
      return EF_ALT_DOWN;
    case DomKey::ALT_GRAPH:
      return EF_ALTGR_DOWN;
    // ChromeOS uses F16 to represent CapsLock before the rewriting stage,
    // based on the historical X11 implementation.
    // TODO post-X11: Switch to use CapsLock uniformly.
    case DomKey::F16:
    case DomKey::CAPS_LOCK:
      return EF_CAPS_LOCK_DOWN;
    case DomKey::CONTROL:
      return EF_CONTROL_DOWN;
    case DomKey::HYPER:
      return EF_MOD3_DOWN;
    case DomKey::META:
      return EF_ALT_DOWN;
    case DomKey::OS:
      return EF_COMMAND_DOWN;
    case DomKey::SHIFT:
      return EF_SHIFT_DOWN;
    case DomKey::SUPER:
      return EF_MOD3_DOWN;
    default:
      return EF_NONE;
  }
  // Not represented:
  //   DomKey::ACCEL
  //   DomKey::FN
  //   DomKey::FN_LOCK
  //   DomKey::NUM_LOCK
  //   DomKey::SCROLL_LOCK
  //   DomKey::SYMBOL
  //   DomKey::SYMBOL_LOCK
}

}  // namespace ui
