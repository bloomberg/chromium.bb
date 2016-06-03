// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/keycodes/platform_key_map_win.h"

#include <utility>

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/threading/thread_local_storage.h"

#include "ui/events/event_constants.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"

namespace ui {

namespace {

struct DomCodeEntry {
  DomCode dom_code;
  int scan_code;
};
#define USB_KEYMAP_DECLARATION const DomCodeEntry supported_dom_code_list[] =
#define USB_KEYMAP(usb, evdev, xkb, win, mac, code, id) {DomCode::id, win}
#include "ui/events/keycodes/dom/keycode_converter_data.inc"
#undef USB_KEYMAP
#undef USB_KEYMAP_DECLARATION

// List of modifiers mentioned in https://w3c.github.io/uievents/#keys-modifiers
// Some modifiers are commented out because they usually don't change keys.
const EventFlags modifier_flags[] = {
    EF_SHIFT_DOWN,
    EF_CONTROL_DOWN,
    EF_ALT_DOWN,
    // EF_COMMAND_DOWN,
    EF_ALTGR_DOWN,
    // EF_NUM_LOCK_ON,
    EF_CAPS_LOCK_ON,
    // EF_SCROLL_LOCK_ON
};
const int kModifierFlagsCombinations = (1 << arraysize(modifier_flags)) - 1;

int GetModifierFlags(int combination) {
  int flags = EF_NONE;
  for (size_t i = 0; i < arraysize(modifier_flags); ++i) {
    if (combination & (1 << i))
      flags |= modifier_flags[i];
  }
  return flags;
}

void SetModifierState(BYTE* keyboard_state, int flags) {
  // According to MSDN GetKeyState():
  // 1. If the high-order bit is 1, the key is down; otherwise, it is up.
  // 2. If the low-order bit is 1, the key is toggled. A key, such as the
  //    CAPS LOCK key, is toggled if it is turned on. The key is off and
  //    untoggled if the low-order bit is 0.
  // See https://msdn.microsoft.com/en-us/library/windows/desktop/ms646301.aspx
  if (flags & EF_SHIFT_DOWN)
    keyboard_state[VK_SHIFT] |= 0x80;

  if (flags & EF_CONTROL_DOWN)
    keyboard_state[VK_CONTROL] |= 0x80;

  if (flags & EF_ALT_DOWN)
    keyboard_state[VK_MENU] |= 0x80;

  if (flags & EF_ALTGR_DOWN) {
    // AltGr should be RightAlt+LeftControl within Windows, but actually only
    // the non-located keys will work here.
    keyboard_state[VK_MENU] |= 0x80;
    keyboard_state[VK_CONTROL] |= 0x80;
  }

  if (flags & EF_COMMAND_DOWN)
    keyboard_state[VK_LWIN] |= 0x80;

  if (flags & EF_NUM_LOCK_ON)
    keyboard_state[VK_NUMLOCK] |= 0x01;

  if (flags & EF_CAPS_LOCK_ON)
    keyboard_state[VK_CAPITAL] |= 0x01;

  if (flags & EF_SCROLL_LOCK_ON)
    keyboard_state[VK_SCROLL] |= 0x01;
}

DomKey NumPadKeyCodeToDomKey(KeyboardCode key_code) {
  switch (key_code) {
    case VKEY_NUMPAD0:
      return DomKey::Constant<'0'>::Character;
    case VKEY_NUMPAD1:
      return DomKey::Constant<'1'>::Character;
    case VKEY_NUMPAD2:
      return DomKey::Constant<'2'>::Character;
    case VKEY_NUMPAD3:
      return DomKey::Constant<'3'>::Character;
    case VKEY_NUMPAD4:
      return DomKey::Constant<'4'>::Character;
    case VKEY_NUMPAD5:
      return DomKey::Constant<'5'>::Character;
    case VKEY_NUMPAD6:
      return DomKey::Constant<'6'>::Character;
    case VKEY_NUMPAD7:
      return DomKey::Constant<'7'>::Character;
    case VKEY_NUMPAD8:
      return DomKey::Constant<'8'>::Character;
    case VKEY_NUMPAD9:
      return DomKey::Constant<'9'>::Character;
    case VKEY_CLEAR:
      return DomKey::CLEAR;
    case VKEY_PRIOR:
      return DomKey::PAGE_UP;
    case VKEY_NEXT:
      return DomKey::PAGE_DOWN;
    case VKEY_END:
      return DomKey::END;
    case VKEY_HOME:
      return DomKey::HOME;
    case VKEY_LEFT:
      return DomKey::ARROW_LEFT;
    case VKEY_UP:
      return DomKey::ARROW_UP;
    case VKEY_RIGHT:
      return DomKey::ARROW_RIGHT;
    case VKEY_DOWN:
      return DomKey::ARROW_DOWN;
    case VKEY_INSERT:
      return DomKey::INSERT;
    case VKEY_DELETE:
      return DomKey::DEL;
    default:
      return DomKey::NONE;
  }
}

// This table must be sorted by |key_code| for binary search.
const struct NonPrintableKeyEntry {
  KeyboardCode key_code;
  DomKey dom_key;
} kNonPrintableKeyMap[] = {
    {VKEY_CANCEL, DomKey::CANCEL},
    {VKEY_BACK, DomKey::BACKSPACE},
    {VKEY_TAB, DomKey::TAB},
    {VKEY_CLEAR, DomKey::CLEAR},
    {VKEY_RETURN, DomKey::ENTER},
    {VKEY_SHIFT, DomKey::SHIFT},
    {VKEY_CONTROL, DomKey::CONTROL},
    {VKEY_MENU, DomKey::ALT},
    {VKEY_PAUSE, DomKey::PAUSE},
    {VKEY_CAPITAL, DomKey::CAPS_LOCK},
    // VKEY_KANA == VKEY_HANGUL
    {VKEY_JUNJA, DomKey::JUNJA_MODE},
    {VKEY_FINAL, DomKey::FINAL_MODE},
    // VKEY_HANJA == VKEY_KANJI
    {VKEY_ESCAPE, DomKey::ESCAPE},
    {VKEY_CONVERT, DomKey::CONVERT},
    {VKEY_NONCONVERT, DomKey::NON_CONVERT},
    {VKEY_ACCEPT, DomKey::ACCEPT},
    {VKEY_MODECHANGE, DomKey::MODE_CHANGE},
    // VKEY_SPACE
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
    // VKEY_0..9
    // VKEY_A..Z
    {VKEY_LWIN, DomKey::META},
    // VKEY_COMMAND == VKEY_LWIN
    {VKEY_RWIN, DomKey::META},
    {VKEY_APPS, DomKey::CONTEXT_MENU},
    {VKEY_SLEEP, DomKey::STANDBY},
    // VKEY_NUMPAD0..9
    // VKEY_MULTIPLY, VKEY_ADD, VKEY_SEPARATOR, VKEY_SUBTRACT, VKEY_DECIMAL,
    // VKEY_DIVIDE
    {VKEY_F1, DomKey::F1},
    {VKEY_F2, DomKey::F2},
    {VKEY_F3, DomKey::F3},
    {VKEY_F4, DomKey::F4},
    {VKEY_F5, DomKey::F5},
    {VKEY_F6, DomKey::F6},
    {VKEY_F7, DomKey::F7},
    {VKEY_F8, DomKey::F8},
    {VKEY_F9, DomKey::F9},
    {VKEY_F10, DomKey::F10},
    {VKEY_F11, DomKey::F11},
    {VKEY_F12, DomKey::F12},
    {VKEY_F13, DomKey::F13},
    {VKEY_F14, DomKey::F14},
    {VKEY_F15, DomKey::F15},
    {VKEY_F16, DomKey::F16},
    {VKEY_F17, DomKey::F17},
    {VKEY_F18, DomKey::F18},
    {VKEY_F19, DomKey::F19},
    {VKEY_F20, DomKey::F20},
    {VKEY_F21, DomKey::F21},
    {VKEY_F22, DomKey::F22},
    {VKEY_F23, DomKey::F23},
    {VKEY_F24, DomKey::F24},
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
    {VKEY_VOLUME_MUTE, DomKey::AUDIO_VOLUME_MUTE},
    {VKEY_VOLUME_DOWN, DomKey::AUDIO_VOLUME_DOWN},
    {VKEY_VOLUME_UP, DomKey::AUDIO_VOLUME_UP},
    {VKEY_MEDIA_NEXT_TRACK, DomKey::MEDIA_TRACK_NEXT},
    {VKEY_MEDIA_PREV_TRACK, DomKey::MEDIA_TRACK_PREVIOUS},
    {VKEY_MEDIA_STOP, DomKey::MEDIA_STOP},
    {VKEY_MEDIA_PLAY_PAUSE, DomKey::MEDIA_PLAY_PAUSE},
    {VKEY_MEDIA_LAUNCH_MAIL, DomKey::LAUNCH_MAIL},
    {VKEY_MEDIA_LAUNCH_MEDIA_SELECT, DomKey::LAUNCH_MEDIA_PLAYER},
    {VKEY_MEDIA_LAUNCH_APP1, DomKey::LAUNCH_MY_COMPUTER},
    {VKEY_MEDIA_LAUNCH_APP2, DomKey::LAUNCH_CALCULATOR},
    // VKEY_OEM_1..8, 102, PLUS, COMMA, MINUS, PERIOD
    {VKEY_ALTGR, DomKey::ALT_GRAPH},
    {VKEY_PROCESSKEY, DomKey::PROCESS},
    // VKEY_PACKET - Used to pass Unicode char, considered as printable key.

    // TODO(chongz): Handle Japanese keyboard layout keys 0xF0..0xF5.
    // https://crbug.com/612694
    {VKEY_ATTN, DomKey::ATTN},
    {VKEY_CRSEL, DomKey::CR_SEL},
    {VKEY_EXSEL, DomKey::EX_SEL},
    {VKEY_EREOF, DomKey::ERASE_EOF},
    {VKEY_PLAY, DomKey::PLAY},
    {VKEY_ZOOM, DomKey::ZOOM_TOGGLE},
    // TODO(chongz): Handle VKEY_NONAME, VKEY_PA1.
    // https://crbug.com/616910
    {VKEY_OEM_CLEAR, DomKey::CLEAR},
};

DomKey NonPrintableKeyboardCodeToDomKey(KeyboardCode key_code) {
  const NonPrintableKeyEntry* result = std::lower_bound(
      std::begin(kNonPrintableKeyMap), std::end(kNonPrintableKeyMap), key_code,
      [](const NonPrintableKeyEntry& entry, KeyboardCode needle) {
        return entry.key_code < needle;
      });
  if (result != std::end(kNonPrintableKeyMap) && result->key_code == key_code)
    return result->dom_key;
  return DomKey::NONE;
}

void CleanupKeyMapTls(void* data) {
  PlatformKeyMap* key_map = reinterpret_cast<PlatformKeyMap*>(data);
  delete key_map;
}

struct PlatformKeyMapInstanceTlsTraits
    : public base::DefaultLazyInstanceTraits<base::ThreadLocalStorage::Slot> {
  static base::ThreadLocalStorage::Slot* New(void* instance) {
    // Use placement new to initialize our instance in our preallocated space.
    // TODO(chongz): Use std::default_delete instead of providing own function.
    return new (instance) base::ThreadLocalStorage::Slot(CleanupKeyMapTls);
  }
};

base::LazyInstance<base::ThreadLocalStorage::Slot,
                   PlatformKeyMapInstanceTlsTraits>
    g_platform_key_map_tls_lazy = LAZY_INSTANCE_INITIALIZER;

}  // anonymous namespace

PlatformKeyMap::PlatformKeyMap() {}

PlatformKeyMap::PlatformKeyMap(HKL layout) {
  UpdateLayout(layout);
}

PlatformKeyMap::~PlatformKeyMap() {}

DomKey PlatformKeyMap::DomKeyFromNativeImpl(DomCode code,
                                            KeyboardCode key_code,
                                            int flags) const {
  DomKey key = NonPrintableKeyboardCodeToDomKey(key_code);
  if (key != DomKey::NONE)
    return key;

  // TODO(chongz): Handle VKEY_KANA/VKEY_HANGUL, VKEY_HANJA/VKEY_KANJI based on
  // layout.
  // https://crbug.com/612736

  if (KeycodeConverter::DomCodeToLocation(code) == DomKeyLocation::NUMPAD) {
    // Derived the DOM Key value from |key_code| instead of |code|, to address
    // Windows Numlock/Shift interaction - see crbug.com/594552.
    return NumPadKeyCodeToDomKey(key_code);
  }

  const int flags_to_try[] = {
      // Trying to match Firefox's behavior and UIEvents DomKey guidelines.
      // If the combination doesn't produce a printable character, the key value
      // should be the key with no modifiers except for Shift and AltGr.
      // See https://w3c.github.io/uievents/#keys-guidelines
      flags,
      flags & (EF_SHIFT_DOWN | EF_ALTGR_DOWN | EF_CAPS_LOCK_ON),
      flags & (EF_SHIFT_DOWN | EF_CAPS_LOCK_ON),
      EF_NONE,
  };

  for (auto try_flags : flags_to_try) {
    const auto& it = code_to_key_.find(std::make_pair(static_cast<int>(code),
                                                      try_flags));
    if (it != code_to_key_.end()) {
      key = it->second;
      if (key != DomKey::NONE)
        break;
    }
  }
  return key;
}

// static
DomKey PlatformKeyMap::DomKeyFromNative(const base::NativeEvent& native_event) {
  // Use TLS because KeyboardLayout is per thread.
  // However currently PlatformKeyMap will only be used by the host application,
  // which is just one process and one thread.
  base::ThreadLocalStorage::Slot* platform_key_map_tls =
      g_platform_key_map_tls_lazy.Pointer();
  PlatformKeyMap* platform_key_map =
      reinterpret_cast<PlatformKeyMap*>(platform_key_map_tls->Get());
  if (!platform_key_map) {
    platform_key_map = new PlatformKeyMap();
    platform_key_map_tls->Set(platform_key_map);
  }

  HKL current_layout = ::GetKeyboardLayout(0);
  platform_key_map->UpdateLayout(current_layout);
  return platform_key_map->DomKeyFromNativeImpl(
      CodeFromNative(native_event), KeyboardCodeFromNative(native_event),
      EventFlagsFromNative(native_event));
}

void PlatformKeyMap::UpdateLayout(HKL layout) {
  if (layout == keyboard_layout_)
    return;

  BYTE keyboard_state_to_restore[256];
  if (!::GetKeyboardState(keyboard_state_to_restore))
    return;

  // TODO(chongz): Optimize layout switching (see crbug.com/587147).
  keyboard_layout_ = layout;
  code_to_key_.clear();
  // Map size for some sample keyboard layouts:
  // US: 428, French: 554, Persian: 434, Vietnamese: 1388
  code_to_key_.reserve(500);

  for (int eindex = 0; eindex <= kModifierFlagsCombinations; ++eindex) {
    BYTE keyboard_state[256];
    memset(keyboard_state, 0, sizeof(keyboard_state));
    int flags = GetModifierFlags(eindex);
    SetModifierState(keyboard_state, flags);
    for (const auto& dom_code_entry : supported_dom_code_list) {
      wchar_t translated_chars[5];
      int key_code = ::MapVirtualKeyEx(dom_code_entry.scan_code,
                                       MAPVK_VSC_TO_VK, keyboard_layout_);
      int rv = ::ToUnicodeEx(key_code, 0, keyboard_state, translated_chars,
                             arraysize(translated_chars), 0, keyboard_layout_);

      if (rv == -1) {
        // Dead key, injecting VK_SPACE to get character representation.
        BYTE empty_state[256];
        memset(empty_state, 0, sizeof(empty_state));
        rv = ::ToUnicodeEx(VK_SPACE, 0, empty_state, translated_chars,
                           arraysize(translated_chars), 0, keyboard_layout_);
        // Expecting a dead key character (not followed by a space).
        if (rv == 1) {
          code_to_key_[std::make_pair(static_cast<int>(dom_code_entry.dom_code),
                                      flags)] =
              DomKey::DeadKeyFromCombiningCharacter(translated_chars[0]);
        } else {
          // TODO(chongz): Check if this will actually happen.
        }
      } else if (rv == 1) {
        if (translated_chars[0] >= 0x20) {
          code_to_key_[std::make_pair(static_cast<int>(dom_code_entry.dom_code),
                                      flags)] =
              DomKey::FromCharacter(translated_chars[0]);
        } else {
          // Ignores legacy non-printable control characters.
        }
      } else {
        // TODO(chongz): Handle rv <= -2 and rv >= 2.
      }
    }
  }
  ::SetKeyboardState(keyboard_state_to_restore);
}

}  // namespace ui
