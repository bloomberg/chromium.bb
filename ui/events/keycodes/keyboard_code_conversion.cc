// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/keycodes/keyboard_code_conversion.h"

#include <algorithm>

#include "ui/events/event_constants.h"
#include "ui/events/keycodes/dom3/dom_code.h"
#include "ui/events/keycodes/dom3/dom_key.h"
#include "ui/events/keycodes/dom_us_layout_data.h"

namespace ui {

namespace {

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

// Returns the Windows-based VKEY value corresponding to a DOM Level 3 |code|,
// assuming a base US English layout. The returned VKEY is located
// (e.g. VKEY_LSHIFT).
KeyboardCode DomCodeToUsLayoutKeyboardCode(DomCode dom_code) {
  const DomCodeToKeyboardCodeEntry* end =
      kDomCodeToKeyboardCodeMap + arraysize(kDomCodeToKeyboardCodeMap);
  const DomCodeToKeyboardCodeEntry* found =
      std::lower_bound(kDomCodeToKeyboardCodeMap, end, dom_code,
                       [](const DomCodeToKeyboardCodeEntry& a, DomCode b) {
    return static_cast<int>(a.dom_code) < static_cast<int>(b);
  });
  if ((found != end) && (found->dom_code == dom_code))
    return found->key_code;
  return VKEY_UNKNOWN;
}

}  // anonymous namespace

base::char16 DomCodeToUsLayoutCharacter(DomCode dom_code, int flags) {
  base::char16 character;
  DomKey dom_key;
  KeyboardCode key_code;
  if (DomCodeToUsLayoutMeaning(dom_code, flags, &dom_key, &character,
                               &key_code))
    return character;
  return 0;
}

bool DomCodeToUsLayoutMeaning(DomCode dom_code,
                              int flags,
                              DomKey* out_dom_key,
                              base::char16* out_character,
                              KeyboardCode* out_key_code) {
  if ((flags & EF_CONTROL_DOWN) == EF_CONTROL_DOWN) {
    if (DomCodeToControlCharacter(dom_code, flags, out_dom_key, out_character,
                                  out_key_code)) {
      return true;
    }
    if (!IsModifierDomCode(dom_code)) {
      *out_dom_key = DomKey::UNIDENTIFIED;
      *out_character = 0;
      *out_key_code = LocatedToNonLocatedKeyboardCode(
          DomCodeToUsLayoutKeyboardCode(dom_code));
      return true;
    }
  } else {
    for (const auto& it : kPrintableCodeMap) {
      if (it.dom_code == dom_code) {
        int state = ((flags & EF_SHIFT_DOWN) == EF_SHIFT_DOWN);
        base::char16 ch = it.character[state];
        *out_dom_key = DomKey::CHARACTER;
        *out_character = ch;
        if ((flags & EF_CAPS_LOCK_DOWN) == EF_CAPS_LOCK_DOWN) {
          ch |= 0x20;
          if ((ch >= 'a') && (ch <= 'z'))
            *out_character = it.character[state ^ 1];
        }
        *out_key_code = LocatedToNonLocatedKeyboardCode(
            DomCodeToUsLayoutKeyboardCode(dom_code));
        return true;
      }
    }
  }
  for (const auto& it : kNonPrintableCodeMap) {
    if (it.dom_code == dom_code) {
      *out_dom_key = it.dom_key;
      *out_character = it.character;
      *out_key_code = NonPrintableDomKeyToKeyboardCode(it.dom_key);
      return true;
    }
  }
  return false;
}

bool DomCodeToControlCharacter(DomCode dom_code,
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
    switch (dom_code) {
      case DomCode::KEY_H:
        *dom_key = DomKey::BACKSPACE;
        *key_code = VKEY_BACK;
        break;
      case DomCode::KEY_I:
        *dom_key = DomKey::TAB;
        *key_code = VKEY_TAB;
        break;
      case DomCode::KEY_M:
        *dom_key = DomKey::ENTER;
        *key_code = VKEY_RETURN;
        break;
      default:
        *dom_key = DomKey::CHARACTER;
        *key_code = static_cast<KeyboardCode>(code - kKeyA + VKEY_A);
        break;
    }
    return true;
  }

  if (flags & EF_SHIFT_DOWN) {
    switch (dom_code) {
      case DomCode::DIGIT2:
        // NUL
        *character = 0;
        *dom_key = DomKey::CHARACTER;
        *key_code = VKEY_2;
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

  switch (dom_code) {
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

}  // namespace ui
