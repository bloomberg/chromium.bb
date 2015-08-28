// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/keycodes/keyboard_code_conversion.h"

#include <algorithm>

#include "ui/events/event_constants.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/dom_us_layout_data.h"

namespace ui {

namespace {

// This table maps a subset of |KeyboardCode| (VKEYs) to DomKey and character.
// Only values not otherwise handled by GetDomKeyFromKeyCode() are here.
const struct KeyboardCodeToDomKey {
  KeyboardCode key_code;
  DomKey::Base plain;
  DomKey::Base shift;
} kKeyboardCodeToDomKey[] = {
    {VKEY_BACK, DomKey::BACKSPACE},
    {VKEY_TAB, DomKey::TAB},
    {VKEY_RETURN, DomKey::ENTER},
    {VKEY_ESCAPE, DomKey::ESCAPE},
    {VKEY_SPACE, DomKey::Constant<' '>::Character},
    {VKEY_MULTIPLY, DomKey::Constant<'*'>::Character},
    {VKEY_ADD, DomKey::Constant<'+'>::Character},
    {VKEY_SEPARATOR, DomKey::Constant<','>::Character},
    {VKEY_SUBTRACT, DomKey::Constant<'-'>::Character},
    {VKEY_DECIMAL, DomKey::Constant<'.'>::Character},
    {VKEY_DIVIDE, DomKey::Constant<'/'>::Character},
    {VKEY_OEM_1, DomKey::Constant<';'>::Character,
     DomKey::Constant<':'>::Character},
    {VKEY_OEM_PLUS, DomKey::Constant<'='>::Character,
     DomKey::Constant<'+'>::Character},
    {VKEY_OEM_COMMA, DomKey::Constant<','>::Character,
     DomKey::Constant<'<'>::Character},
    {VKEY_OEM_MINUS, DomKey::Constant<'-'>::Character,
     DomKey::Constant<'_'>::Character},
    {VKEY_OEM_PERIOD, DomKey::Constant<'.'>::Character,
     DomKey::Constant<'>'>::Character},
    {VKEY_OEM_2, DomKey::Constant<'/'>::Character,
     DomKey::Constant<'?'>::Character},
    {VKEY_OEM_3, DomKey::Constant<'`'>::Character,
     DomKey::Constant<'~'>::Character},
    {VKEY_OEM_4, DomKey::Constant<'['>::Character,
     DomKey::Constant<'{'>::Character},
    {VKEY_OEM_5, DomKey::Constant<'\\'>::Character,
     DomKey::Constant<'|'>::Character},
    {VKEY_OEM_6, DomKey::Constant<']'>::Character,
     DomKey::Constant<'}'>::Character},
    {VKEY_OEM_7, DomKey::Constant<'\''>::Character,
     DomKey::Constant<'"'>::Character},
    {VKEY_OEM_102, DomKey::Constant<'<'>::Character,
     DomKey::Constant<'>'>::Character},
    {VKEY_CLEAR, DomKey::CLEAR},
    {VKEY_SHIFT, DomKey::SHIFT},
    {VKEY_CONTROL, DomKey::CONTROL},
    {VKEY_MENU, DomKey::ALT},
    {VKEY_PAUSE, DomKey::PAUSE},
    {VKEY_CAPITAL, DomKey::CAPS_LOCK},
    // Windows conflates 'KanaMode' and 'HangulMode'.
    {VKEY_KANA, DomKey::KANA_MODE},
    {VKEY_JUNJA, DomKey::JUNJA_MODE},
    {VKEY_FINAL, DomKey::FINAL_MODE},
    // Windows conflates 'HanjaMode' and 'KanjiMode'.
    {VKEY_HANJA, DomKey::HANJA_MODE},
    {VKEY_CONVERT, DomKey::CONVERT},
    {VKEY_NONCONVERT, DomKey::NON_CONVERT},
    {VKEY_ACCEPT, DomKey::ACCEPT},
    {VKEY_MODECHANGE, DomKey::MODE_CHANGE},
    {VKEY_PRIOR, DomKey::PAGE_UP},
    {VKEY_NEXT, DomKey::PAGE_DOWN},
    {VKEY_END, DomKey::END},
    {VKEY_HOME, DomKey::HOME},
    {VKEY_LEFT, DomKey::ARROW_LEFT},
    {VKEY_UP, DomKey::ARROW_UP},
    {VKEY_RIGHT, DomKey::ARROW_RIGHT},
    {VKEY_DOWN, DomKey::ARROW_DOWN},
    {VKEY_SELECT, DomKey::SELECT},
    {VKEY_PRINT, DomKey::PRINT},
    {VKEY_EXECUTE, DomKey::EXECUTE},
    {VKEY_SNAPSHOT, DomKey::PRINT_SCREEN},
    {VKEY_INSERT, DomKey::INSERT},
    {VKEY_DELETE, DomKey::DEL},
    {VKEY_HELP, DomKey::HELP},
    {VKEY_LWIN, DomKey::OS},
    {VKEY_RWIN, DomKey::OS},
    {VKEY_APPS, DomKey::MEDIA_APPS},
    {VKEY_NUMLOCK, DomKey::NUM_LOCK},
    {VKEY_SCROLL, DomKey::SCROLL_LOCK},
    {VKEY_LSHIFT, DomKey::SHIFT},
    {VKEY_RSHIFT, DomKey::SHIFT},
    {VKEY_LCONTROL, DomKey::CONTROL},
    {VKEY_RCONTROL, DomKey::CONTROL},
    {VKEY_LMENU, DomKey::ALT},
    {VKEY_RMENU, DomKey::ALT},
    {VKEY_BROWSER_BACK, DomKey::BROWSER_BACK},
    {VKEY_BROWSER_FORWARD, DomKey::BROWSER_FORWARD},
    {VKEY_BROWSER_REFRESH, DomKey::BROWSER_REFRESH},
    {VKEY_BROWSER_STOP, DomKey::BROWSER_STOP},
    {VKEY_BROWSER_SEARCH, DomKey::BROWSER_SEARCH},
    {VKEY_BROWSER_FAVORITES, DomKey::BROWSER_FAVORITES},
    {VKEY_BROWSER_HOME, DomKey::BROWSER_HOME},
    {VKEY_VOLUME_MUTE, DomKey::VOLUME_MUTE},
    {VKEY_VOLUME_DOWN, DomKey::VOLUME_DOWN},
    {VKEY_VOLUME_UP, DomKey::VOLUME_UP},
    {VKEY_MEDIA_NEXT_TRACK, DomKey::MEDIA_TRACK_NEXT},
    {VKEY_MEDIA_PREV_TRACK, DomKey::MEDIA_TRACK_PREVIOUS},
    {VKEY_MEDIA_STOP, DomKey::MEDIA_STOP},
    {VKEY_MEDIA_PLAY_PAUSE, DomKey::MEDIA_PLAY_PAUSE},
    {VKEY_MEDIA_LAUNCH_MAIL, DomKey::LAUNCH_MAIL},
    {VKEY_MEDIA_LAUNCH_MEDIA_SELECT, DomKey::LAUNCH_MEDIA_PLAYER},
    {VKEY_MEDIA_LAUNCH_APP1, DomKey::LAUNCH_MY_COMPUTER},
    {VKEY_MEDIA_LAUNCH_APP2, DomKey::LAUNCH_CALCULATOR},
    {VKEY_OEM_8, DomKey::SUPER},  // ISO Level 5 Shift in ChromeOS
    {VKEY_PROCESSKEY, DomKey::PROCESS},
    {VKEY_DBE_SBCSCHAR, DomKey::HANKAKU},
    {VKEY_DBE_DBCSCHAR, DomKey::ZENKAKU},
    {VKEY_ATTN, DomKey::ATTN},
    {VKEY_CRSEL, DomKey::CR_SEL},
    {VKEY_EXSEL, DomKey::EX_SEL},
    {VKEY_EREOF, DomKey::ERASE_EOF},
    {VKEY_PLAY, DomKey::MEDIA_PLAY},
    {VKEY_ZOOM, DomKey::ZOOM_TOGGLE},
    {VKEY_OEM_CLEAR, DomKey::CLEAR},
    {VKEY_ALTGR, DomKey::ALT_GRAPH},
#if defined(OS_POSIX)
    {VKEY_POWER, DomKey::POWER},
    {VKEY_BRIGHTNESS_DOWN, DomKey::BRIGHTNESS_DOWN},
    {VKEY_BRIGHTNESS_UP, DomKey::BRIGHTNESS_UP},
    {VKEY_COMPOSE, DomKey::COMPOSE},
    {VKEY_OEM_103, DomKey::MEDIA_REWIND},
    {VKEY_OEM_104, DomKey::MEDIA_FAST_FORWARD},
#endif
};

bool IsRightSideDomCode(DomCode code) {
  return (code == DomCode::SHIFT_RIGHT) || (code == DomCode::CONTROL_RIGHT) ||
         (code == DomCode::ALT_RIGHT) || (code == DomCode::OS_RIGHT);
}

bool IsModifierDomCode(DomCode code) {
  return (code == DomCode::CONTROL_LEFT) || (code == DomCode::CONTROL_RIGHT) ||
         (code == DomCode::SHIFT_LEFT) || (code == DomCode::SHIFT_RIGHT) ||
         (code == DomCode::ALT_LEFT) || (code == DomCode::ALT_RIGHT) ||
         (code == DomCode::OS_LEFT) || (code == DomCode::OS_RIGHT);
}

}  // anonymous namespace

base::char16 GetCharacterFromKeyCode(KeyboardCode key_code, int flags) {
  DomKey key = GetDomKeyFromKeyCode(key_code, flags);
  if (key.IsCharacter())
    return key.ToCharacter();
  return 0;
}

DomKey GetDomKeyFromKeyCode(KeyboardCode key_code, int flags) {
  const bool ctrl = (flags & EF_CONTROL_DOWN) != 0;
  const bool shift = (flags & EF_SHIFT_DOWN) != 0;
  const bool upper = shift ^ ((flags & EF_CAPS_LOCK_DOWN) != 0);

  // Control characters.
  if (ctrl) {
    // Following Windows behavior to map ctrl-a ~ ctrl-z to \x01 ~ \x1A.
    if (key_code >= VKEY_A && key_code <= VKEY_Z)
      return DomKey::FromCharacter(key_code - VKEY_A + 1);
    // Other control characters.
    if (shift) {
      // The following graphics characters require the shift key to input.
      switch (key_code) {
        // ctrl-@ maps to \x00 (Null byte)
        case VKEY_2:
          return DomKey::FromCharacter(0);
        // ctrl-^ maps to \x1E (Record separator, Information separator two)
        case VKEY_6:
          return DomKey::FromCharacter(0x1E);
        // ctrl-_ maps to \x1F (Unit separator, Information separator one)
        case VKEY_OEM_MINUS:
          return DomKey::FromCharacter(0x1F);
        // Returns UNIDENTIFIED for all other keys to avoid inputting
        // unexpected chars.
        default:
          return DomKey::UNIDENTIFIED;
      }
    } else {
      switch (key_code) {
        // ctrl-[ maps to \x1B (Escape)
        case VKEY_OEM_4:
          return DomKey::ESCAPE;
        // ctrl-\ maps to \x1C (File separator, Information separator four)
        case VKEY_OEM_5:
          return DomKey::FromCharacter(0x1C);
        // ctrl-] maps to \x1D (Group separator, Information separator three)
        case VKEY_OEM_6:
          return DomKey::FromCharacter(0x1D);
        // ctrl-Enter maps to \x0A (Line feed)
        case VKEY_RETURN:
          return DomKey::FromCharacter(0x0A);
        // Returns UNIDENTIFIED for all other keys to avoid inputting
        // unexpected chars.
        default:
          return DomKey::UNIDENTIFIED;
      }
    }
  }

  // ASCII alphanumeric characters.
  if (key_code >= VKEY_A && key_code <= VKEY_Z)
    return DomKey::FromCharacter(key_code - VKEY_A + (upper ? 'A' : 'a'));
  if (key_code >= VKEY_0 && key_code <= VKEY_9) {
    return DomKey::FromCharacter(shift ? ")!@#$%^&*("[key_code - VKEY_0]
                 : '0' + key_code - VKEY_0);
  }
  if (key_code >= VKEY_NUMPAD0 && key_code <= VKEY_NUMPAD9)
    return DomKey::FromCharacter(key_code - VKEY_NUMPAD0 + '0');

  // Function keys.
  if (key_code >= VKEY_F1 && key_code <= VKEY_F24)
    return DomKey::FromCharacter(key_code - VKEY_F1 + DomKey::F1);

  // Other keys.
  for (const auto& k : kKeyboardCodeToDomKey) {
    if (k.key_code == key_code)
      return (shift && k.shift) ? k.shift : k.plain;
  }
  return DomKey::UNIDENTIFIED;
}

bool DomCodeToUsLayoutDomKey(DomCode dom_code,
                             int flags,
                             DomKey* out_dom_key,
                             KeyboardCode* out_key_code) {
  if ((flags & EF_CONTROL_DOWN) == EF_CONTROL_DOWN) {
    if (DomCodeToControlCharacter(dom_code, flags, out_dom_key, out_key_code)) {
      return true;
    }
    if (!IsModifierDomCode(dom_code)) {
      *out_dom_key = DomKey::UNIDENTIFIED;
      *out_key_code = LocatedToNonLocatedKeyboardCode(
          DomCodeToUsLayoutKeyboardCode(dom_code));
      return true;
    }
  } else {
    for (const auto& it : kPrintableCodeMap) {
      if (it.dom_code == dom_code) {
        int state = ((flags & EF_SHIFT_DOWN) == EF_SHIFT_DOWN);
        base::char16 ch = it.character[state];
        if ((flags & EF_CAPS_LOCK_DOWN) == EF_CAPS_LOCK_DOWN) {
          ch |= 0x20;
          if ((ch >= 'a') && (ch <= 'z'))
            ch = it.character[state ^ 1];
        }
        *out_dom_key = DomKey::FromCharacter(ch);
        *out_key_code = LocatedToNonLocatedKeyboardCode(
            DomCodeToUsLayoutKeyboardCode(dom_code));
        return true;
      }
    }
  }
  for (const auto& it : kNonPrintableCodeMap) {
    if (it.dom_code == dom_code) {
      *out_dom_key = it.dom_key;
      *out_key_code = NonPrintableDomKeyToKeyboardCode(it.dom_key);
      return true;
    }
  }
  if ((flags & EF_CONTROL_DOWN) == EF_CONTROL_DOWN) {
    *out_dom_key = DomKey::UNIDENTIFIED;
    *out_key_code = LocatedToNonLocatedKeyboardCode(
        DomCodeToUsLayoutKeyboardCode(dom_code));
    return true;
  }
  return false;
}

bool DomCodeToControlCharacter(DomCode dom_code,
                               int flags,
                               DomKey* dom_key,
                               KeyboardCode* key_code) {
  if ((flags & EF_CONTROL_DOWN) == 0)
    return false;

  int code = static_cast<int>(dom_code);
  const int kKeyA = static_cast<int>(DomCode::KEY_A);
  // Control-A - Control-Z map to 0x01 - 0x1A.
  if (code >= kKeyA && code <= static_cast<int>(DomCode::KEY_Z)) {
    *dom_key = DomKey::FromCharacter(code - kKeyA + 1);
    *key_code = static_cast<KeyboardCode>(code - kKeyA + VKEY_A);
    switch (dom_code) {
      case DomCode::KEY_H:
        *key_code = VKEY_BACK;
        break;
      case DomCode::KEY_I:
        *key_code = VKEY_TAB;
        break;
      case DomCode::KEY_M:
        *key_code = VKEY_RETURN;
        break;
      default:
        break;
    }
    return true;
  }

  if (flags & EF_SHIFT_DOWN) {
    switch (dom_code) {
      case DomCode::DIGIT2:
        // NUL
        *dom_key = DomKey::FromCharacter(0);
        *key_code = VKEY_2;
        return true;
      case DomCode::DIGIT6:
        // RS
        *dom_key = DomKey::FromCharacter(0x1E);
        *key_code = VKEY_6;
        return true;
      case DomCode::MINUS:
        // US
        *dom_key = DomKey::FromCharacter(0x1F);
        *key_code = VKEY_OEM_MINUS;
        return true;
      default:
        return false;
    }
  }

  switch (dom_code) {
    case DomCode::ENTER:
      // NL
      *dom_key = DomKey::FromCharacter(0x0A);
      *key_code = VKEY_RETURN;
      return true;
    case DomCode::BRACKET_LEFT:
      // ESC
      *dom_key = DomKey::FromCharacter(0x1B);
      *key_code = VKEY_OEM_4;
      return true;
    case DomCode::BACKSLASH:
      // FS
      *dom_key = DomKey::FromCharacter(0x1C);
      *key_code = VKEY_OEM_5;
      return true;
    case DomCode::BRACKET_RIGHT:
      // GS
      *dom_key = DomKey::FromCharacter(0x1D);
      *key_code = VKEY_OEM_6;
      return true;
    default:
      return false;
  }
}

// Returns a Windows-based VKEY for a non-printable DOM Level 3 |key|.
// The returned VKEY is non-positional (e.g. VKEY_SHIFT).
KeyboardCode NonPrintableDomKeyToKeyboardCode(DomKey dom_key) {
  for (const auto& it : kDomKeyToKeyboardCodeMap) {
    if (it.dom_key == dom_key)
      return it.key_code;
  }
  return VKEY_UNKNOWN;
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

DomCode UsLayoutKeyboardCodeToDomCode(KeyboardCode key_code) {
  key_code = NonLocatedToLocatedKeyboardCode(key_code, DomCode::NONE);
  for (const auto& it : kDomCodeToKeyboardCodeMap) {
    if (it.key_code == key_code)
      return it.dom_code;
  }
  for (const auto& it : kFallbackKeyboardCodeToDomCodeMap) {
    if (it.key_code == key_code)
      return it.dom_code;
  }
  return DomCode::NONE;
}

KeyboardCode DomCodeToUsLayoutKeyboardCode(DomCode dom_code) {
  const DomCodeToKeyboardCodeEntry* end =
      kDomCodeToKeyboardCodeMap + arraysize(kDomCodeToKeyboardCodeMap);
  const DomCodeToKeyboardCodeEntry* found = std::lower_bound(
      kDomCodeToKeyboardCodeMap, end, dom_code,
      [](const DomCodeToKeyboardCodeEntry& a, DomCode b) {
        return static_cast<int>(a.dom_code) < static_cast<int>(b);
      });
  if ((found != end) && (found->dom_code == dom_code))
    return found->key_code;

  return VKEY_UNKNOWN;
}

}  // namespace ui
