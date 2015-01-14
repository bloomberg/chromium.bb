// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/keycodes/keyboard_code_conversion.h"

#include "ui/events/event_constants.h"
#include "ui/events/keycodes/dom3/dom_code.h"
#include "ui/events/keycodes/dom3/dom_key.h"

namespace ui {

namespace {

// This table maps a subset of |KeyboardCode| (VKEYs) to DomKey and character.
// Only values not otherwise handled by GetMeaningFromKeyCode() are here.
const struct KeyboardCodeToMeaning {
  KeyboardCode key_code;
  DomKey key;
  base::char16 plain_character;
  base::char16 shift_character;
} kKeyboardCodeToMeaning[] = {
    {VKEY_BACK, DomKey::BACKSPACE, '\b', 0},
    {VKEY_TAB, DomKey::TAB, '\t', 0},
    {VKEY_RETURN, DomKey::ENTER, '\r', 0},
    {VKEY_ESCAPE, DomKey::ESCAPE, 0x1B, 0},
    {VKEY_SPACE, DomKey::CHARACTER, ' ', 0},
    {VKEY_MULTIPLY, DomKey::CHARACTER, '*', 0},
    {VKEY_ADD, DomKey::CHARACTER, '+', 0},
    {VKEY_SEPARATOR, DomKey::CHARACTER, ',', 0},
    {VKEY_SUBTRACT, DomKey::CHARACTER, '-', 0},
    {VKEY_DECIMAL, DomKey::CHARACTER, '.', 0},
    {VKEY_DIVIDE, DomKey::CHARACTER, '/', 0},
    {VKEY_OEM_1, DomKey::CHARACTER, ';', ':'},
    {VKEY_OEM_PLUS, DomKey::CHARACTER, '=', '+'},
    {VKEY_OEM_COMMA, DomKey::CHARACTER, ',', '<'},
    {VKEY_OEM_MINUS, DomKey::CHARACTER, '-', '_'},
    {VKEY_OEM_PERIOD, DomKey::CHARACTER, '.', '>'},
    {VKEY_OEM_2, DomKey::CHARACTER, '/', '?'},
    {VKEY_OEM_3, DomKey::CHARACTER, '`', '~'},
    {VKEY_OEM_4, DomKey::CHARACTER, '[', '{'},
    {VKEY_OEM_5, DomKey::CHARACTER, '\\', '|'},
    {VKEY_OEM_6, DomKey::CHARACTER, ']', '}'},
    {VKEY_OEM_7, DomKey::CHARACTER, '\'', '"'},
    {VKEY_OEM_102, DomKey::CHARACTER, '<', '>'},
    {VKEY_CLEAR, DomKey::CLEAR, 0, 0},
    {VKEY_SHIFT, DomKey::SHIFT, 0, 0},
    {VKEY_CONTROL, DomKey::CONTROL, 0, 0},
    {VKEY_MENU, DomKey::ALT, 0, 0},
    {VKEY_PAUSE, DomKey::PAUSE, 0, 0},
    {VKEY_CAPITAL, DomKey::CAPS_LOCK, 0, 0},
    // Windows conflates 'KanaMode' and 'HangulMode'.
    {VKEY_KANA, DomKey::KANA_MODE, 0, 0},
    {VKEY_JUNJA, DomKey::JUNJA_MODE, 0, 0},
    {VKEY_FINAL, DomKey::FINAL_MODE, 0, 0},
    // Windows conflates 'HanjaMode' and 'KanjiMode'.
    {VKEY_HANJA, DomKey::HANJA_MODE, 0, 0},
    {VKEY_CONVERT, DomKey::CONVERT, 0, 0},
    {VKEY_NONCONVERT, DomKey::NON_CONVERT, 0, 0},
    {VKEY_ACCEPT, DomKey::ACCEPT, 0, 0},
    {VKEY_MODECHANGE, DomKey::MODE_CHANGE, 0, 0},
    {VKEY_PRIOR, DomKey::PAGE_UP, 0, 0},
    {VKEY_NEXT, DomKey::PAGE_DOWN, 0, 0},
    {VKEY_END, DomKey::END, 0, 0},
    {VKEY_HOME, DomKey::HOME, 0, 0},
    {VKEY_LEFT, DomKey::ARROW_LEFT, 0, 0},
    {VKEY_UP, DomKey::ARROW_UP, 0, 0},
    {VKEY_RIGHT, DomKey::ARROW_RIGHT, 0, 0},
    {VKEY_DOWN, DomKey::ARROW_DOWN, 0, 0},
    {VKEY_SELECT, DomKey::SELECT, 0, 0},
    {VKEY_PRINT, DomKey::PRINT, 0, 0},
    {VKEY_EXECUTE, DomKey::EXECUTE, 0, 0},
    {VKEY_SNAPSHOT, DomKey::PRINT_SCREEN, 0, 0},
    {VKEY_INSERT, DomKey::INSERT, 0, 0},
    {VKEY_DELETE, DomKey::DEL, 0, 0},
    {VKEY_HELP, DomKey::HELP, 0, 0},
    {VKEY_LWIN, DomKey::OS, 0, 0},
    {VKEY_RWIN, DomKey::OS, 0, 0},
    {VKEY_APPS, DomKey::MEDIA_APPS, 0, 0},
    {VKEY_NUMLOCK, DomKey::NUM_LOCK, 0, 0},
    {VKEY_SCROLL, DomKey::SCROLL_LOCK, 0, 0},
    {VKEY_LSHIFT, DomKey::SHIFT, 0, 0},
    {VKEY_RSHIFT, DomKey::SHIFT, 0, 0},
    {VKEY_LCONTROL, DomKey::CONTROL, 0, 0},
    {VKEY_RCONTROL, DomKey::CONTROL, 0, 0},
    {VKEY_LMENU, DomKey::ALT, 0, 0},
    {VKEY_RMENU, DomKey::ALT, 0, 0},
    {VKEY_BROWSER_BACK, DomKey::BROWSER_BACK, 0, 0},
    {VKEY_BROWSER_FORWARD, DomKey::BROWSER_FORWARD, 0, 0},
    {VKEY_BROWSER_REFRESH, DomKey::BROWSER_REFRESH, 0, 0},
    {VKEY_BROWSER_STOP, DomKey::BROWSER_STOP, 0, 0},
    {VKEY_BROWSER_SEARCH, DomKey::BROWSER_SEARCH, 0, 0},
    {VKEY_BROWSER_FAVORITES, DomKey::BROWSER_FAVORITES, 0, 0},
    {VKEY_BROWSER_HOME, DomKey::BROWSER_HOME, 0, 0},
    {VKEY_VOLUME_MUTE, DomKey::VOLUME_MUTE, 0, 0},
    {VKEY_VOLUME_DOWN, DomKey::VOLUME_DOWN, 0, 0},
    {VKEY_VOLUME_UP, DomKey::VOLUME_UP, 0, 0},
    {VKEY_MEDIA_NEXT_TRACK, DomKey::MEDIA_TRACK_NEXT, 0, 0},
    {VKEY_MEDIA_PREV_TRACK, DomKey::MEDIA_TRACK_PREVIOUS, 0, 0},
    {VKEY_MEDIA_STOP, DomKey::MEDIA_STOP, 0, 0},
    {VKEY_MEDIA_PLAY_PAUSE, DomKey::MEDIA_PLAY_PAUSE, 0, 0},
    {VKEY_MEDIA_LAUNCH_MAIL, DomKey::LAUNCH_MAIL, 0, 0},
    {VKEY_MEDIA_LAUNCH_MEDIA_SELECT, DomKey::LAUNCH_MEDIA_PLAYER, 0, 0},
    {VKEY_MEDIA_LAUNCH_APP1, DomKey::LAUNCH_MY_COMPUTER, 0, 0},
    {VKEY_MEDIA_LAUNCH_APP2, DomKey::LAUNCH_CALCULATOR, 0, 0},
    {VKEY_OEM_8, DomKey::SUPER, 0, 0},  // ISO Level 5 Shift in ChromeOS
    {VKEY_PROCESSKEY, DomKey::PROCESS, 0, 0},
    {VKEY_DBE_SBCSCHAR, DomKey::HANKAKU, 0, 0},
    {VKEY_DBE_DBCSCHAR, DomKey::ZENKAKU, 0, 0},
    {VKEY_ATTN, DomKey::ATTN, 0, 0},
    {VKEY_CRSEL, DomKey::CR_SEL, 0, 0},
    {VKEY_EXSEL, DomKey::EX_SEL, 0, 0},
    {VKEY_EREOF, DomKey::ERASE_EOF, 0, 0},
    {VKEY_PLAY, DomKey::MEDIA_PLAY, 0, 0},
    {VKEY_ZOOM, DomKey::ZOOM_TOGGLE, 0, 0},
    {VKEY_OEM_CLEAR, DomKey::CLEAR, 0, 0},
    {VKEY_ALTGR, DomKey::ALT_GRAPH, 0, 0},
#if defined(OS_POSIX)
    {VKEY_POWER, DomKey::POWER, 0, 0},
    {VKEY_BRIGHTNESS_DOWN, DomKey::BRIGHTNESS_DOWN, 0, 0},
    {VKEY_BRIGHTNESS_UP, DomKey::BRIGHTNESS_UP, 0, 0},
    {VKEY_COMPOSE, DomKey::COMPOSE, 0, 0},
    {VKEY_OEM_103, DomKey::MEDIA_REWIND, 0, 0},
    {VKEY_OEM_104, DomKey::MEDIA_FAST_FORWARD, 0, 0},
#endif
};

bool IsRightSideDomCode(DomCode code) {
  return (code == DomCode::SHIFT_RIGHT) || (code == DomCode::CONTROL_RIGHT) ||
         (code == DomCode::ALT_RIGHT) || (code == DomCode::OS_RIGHT);
}

}  // anonymous namespace

base::char16 GetCharacterFromKeyCode(KeyboardCode key_code, int flags) {
  ui::DomKey dom_key;
  base::char16 character;
  if (GetMeaningFromKeyCode(key_code, flags, &dom_key, &character))
    return character;
  return 0;
}

bool GetMeaningFromKeyCode(KeyboardCode key_code,
                           int flags,
                           DomKey* dom_key,
                           base::char16* character) {
  const bool ctrl = (flags & EF_CONTROL_DOWN) != 0;
  const bool shift = (flags & EF_SHIFT_DOWN) != 0;
  const bool upper = shift ^ ((flags & EF_CAPS_LOCK_DOWN) != 0);

  // Control characters.
  if (ctrl) {
    // Following Windows behavior to map ctrl-a ~ ctrl-z to \x01 ~ \x1A.
    if (key_code >= VKEY_A && key_code <= VKEY_Z) {
      *character = static_cast<uint16>(key_code - VKEY_A + 1);
      switch (key_code) {
        case VKEY_H:
          *dom_key = DomKey::BACKSPACE;
          break;
        case VKEY_I:
          *dom_key = DomKey::TAB;
          break;
        case VKEY_J:
        case VKEY_M:
          *dom_key = DomKey::ENTER;
          break;
        default:
          *dom_key = DomKey::CHARACTER;
          break;
      }
      return true;
    }
    // Other control characters.
    if (shift) {
      // The following graphics characters require the shift key to input.
      switch (key_code) {
        // ctrl-@ maps to \x00 (Null byte)
        case VKEY_2:
          *dom_key = DomKey::CHARACTER;
          *character = 0;
          return true;
        // ctrl-^ maps to \x1E (Record separator, Information separator two)
        case VKEY_6:
          *dom_key = DomKey::CHARACTER;
          *character = 0x1E;
          return true;
        // ctrl-_ maps to \x1F (Unit separator, Information separator one)
        case VKEY_OEM_MINUS:
          *dom_key = DomKey::CHARACTER;
          *character = 0x1F;
          return true;
        // Returns 0 for all other keys to avoid inputting unexpected chars.
        default:
          *dom_key = DomKey::UNIDENTIFIED;
          *character = 0;
          return false;
      }
    } else {
      switch (key_code) {
        // ctrl-[ maps to \x1B (Escape)
        case VKEY_OEM_4:
          *dom_key = DomKey::ESCAPE;
          *character = 0x1B;
          return true;
        // ctrl-\ maps to \x1C (File separator, Information separator four)
        case VKEY_OEM_5:
          *dom_key = DomKey::CHARACTER;
          *character = 0x1C;
          return true;
        // ctrl-] maps to \x1D (Group separator, Information separator three)
        case VKEY_OEM_6:
          *dom_key = DomKey::CHARACTER;
          *character = 0x1D;
          return true;
        // ctrl-Enter maps to \x0A (Line feed)
        case VKEY_RETURN:
          *dom_key = DomKey::CHARACTER;
          *character = 0x0A;
          return true;
        // Returns 0 for all other keys to avoid inputting unexpected chars.
        default:
          *dom_key = DomKey::UNIDENTIFIED;
          *character = 0;
          return false;
      }
    }
  }

  // ASCII alphanumeric characters.
  if (key_code >= VKEY_A && key_code <= VKEY_Z) {
    *dom_key = DomKey::CHARACTER;
    *character = static_cast<uint16>(key_code - VKEY_A + (upper ? 'A' : 'a'));
    return true;
  }
  if (key_code >= VKEY_0 && key_code <= VKEY_9) {
    *dom_key = DomKey::CHARACTER;
    *character =
        shift ? ")!@#$%^&*("[key_code - VKEY_0] : static_cast<uint16>(key_code);
    return true;
  }
  if (key_code >= VKEY_NUMPAD0 && key_code <= VKEY_NUMPAD9) {
    *dom_key = DomKey::CHARACTER;
    *character = static_cast<uint16>(key_code - VKEY_NUMPAD0 + '0');
    return true;
  }

  // Function keys.
  if (key_code >= VKEY_F1 && key_code <= VKEY_F24) {
    *dom_key =
        static_cast<DomKey>(key_code - VKEY_F1 + static_cast<int>(DomKey::F1));
    *character = 0;
    return true;
  }

  // Other keys.
  for (size_t i = 0; i < arraysize(kKeyboardCodeToMeaning); ++i) {
    if (kKeyboardCodeToMeaning[i].key_code == key_code) {
      const KeyboardCodeToMeaning* p = &kKeyboardCodeToMeaning[i];
      *dom_key = p->key;
      *character = (shift && p->shift_character) ? p->shift_character
                                                 : p->plain_character;
      return true;
    }
  }
  *dom_key = DomKey::UNIDENTIFIED;
  *character = 0;
  return false;
}

// Determine the non-located VKEY corresponding to a located VKEY.
KeyboardCode LocatedToNonLocatedKeyboardCode(KeyboardCode key_code) {
  switch (key_code) {
    case VKEY_RWIN:
      return VKEY_LWIN;
    case VKEY_LSHIFT:
    case VKEY_RSHIFT:
      return VKEY_SHIFT;
    case VKEY_LCONTROL:
    case VKEY_RCONTROL:
      return VKEY_CONTROL;
    case VKEY_LMENU:
    case VKEY_RMENU:
      return VKEY_MENU;
    case VKEY_NUMPAD0:
      return VKEY_0;
    case VKEY_NUMPAD1:
      return VKEY_1;
    case VKEY_NUMPAD2:
      return VKEY_2;
    case VKEY_NUMPAD3:
      return VKEY_3;
    case VKEY_NUMPAD4:
      return VKEY_4;
    case VKEY_NUMPAD5:
      return VKEY_5;
    case VKEY_NUMPAD6:
      return VKEY_6;
    case VKEY_NUMPAD7:
      return VKEY_7;
    case VKEY_NUMPAD8:
      return VKEY_8;
    case VKEY_NUMPAD9:
      return VKEY_9;
    default:
      return key_code;
  }
}

// Determine the located VKEY corresponding to a non-located VKEY.
KeyboardCode NonLocatedToLocatedKeyboardCode(KeyboardCode key_code,
                                             DomCode dom_code) {
  switch (key_code) {
    case VKEY_SHIFT:
      return IsRightSideDomCode(dom_code) ? VKEY_RSHIFT : VKEY_LSHIFT;
    case VKEY_CONTROL:
      return IsRightSideDomCode(dom_code) ? VKEY_RCONTROL : VKEY_LCONTROL;
    case VKEY_MENU:
      return IsRightSideDomCode(dom_code) ? VKEY_RMENU : VKEY_LMENU;
    case VKEY_LWIN:
      return IsRightSideDomCode(dom_code) ? VKEY_RWIN : VKEY_LWIN;
    case VKEY_0:
      return (dom_code == DomCode::NUMPAD0) ? VKEY_NUMPAD0 : VKEY_0;
    case VKEY_1:
      return (dom_code == DomCode::NUMPAD1) ? VKEY_NUMPAD1 : VKEY_1;
    case VKEY_2:
      return (dom_code == DomCode::NUMPAD2) ? VKEY_NUMPAD2 : VKEY_2;
    case VKEY_3:
      return (dom_code == DomCode::NUMPAD3) ? VKEY_NUMPAD3 : VKEY_3;
    case VKEY_4:
      return (dom_code == DomCode::NUMPAD4) ? VKEY_NUMPAD4 : VKEY_4;
    case VKEY_5:
      return (dom_code == DomCode::NUMPAD5) ? VKEY_NUMPAD5 : VKEY_5;
    case VKEY_6:
      return (dom_code == DomCode::NUMPAD6) ? VKEY_NUMPAD6 : VKEY_6;
    case VKEY_7:
      return (dom_code == DomCode::NUMPAD7) ? VKEY_NUMPAD7 : VKEY_7;
    case VKEY_8:
      return (dom_code == DomCode::NUMPAD8) ? VKEY_NUMPAD8 : VKEY_8;
    case VKEY_9:
      return (dom_code == DomCode::NUMPAD9) ? VKEY_NUMPAD9 : VKEY_9;
    default:
      return key_code;
  }
}

}  // namespace ui
