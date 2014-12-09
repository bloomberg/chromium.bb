// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/keycodes/dom4/keycode_converter.h"

#include "ui/events/keycodes/dom3/dom_code.h"
#include "ui/events/keycodes/dom3/dom_key.h"

namespace ui {

namespace {

// Table of USB codes (equivalent to DomCode values), native scan codes,
// and DOM Level 3 |code| strings.
#if defined(OS_WIN)
#define USB_KEYMAP(usb, xkb, win, mac, code, id) {usb, win, code}
#elif defined(OS_LINUX)
#define USB_KEYMAP(usb, xkb, win, mac, code, id) {usb, xkb, code}
#elif defined(OS_MACOSX)
#define USB_KEYMAP(usb, xkb, win, mac, code, id) {usb, mac, code}
#else
#define USB_KEYMAP(usb, xkb, win, mac, code, id) {usb, 0, code}
#endif
#define USB_KEYMAP_DECLARATION const KeycodeMapEntry usb_keycode_map[] =
#include "ui/events/keycodes/dom4/keycode_converter_data.h"
#undef USB_KEYMAP
#undef USB_KEYMAP_DECLARATION

const size_t kKeycodeMapEntries = arraysize(usb_keycode_map);

// Table of DomKey enum values and DOM Level 3 |key| strings.
struct DomKeyMapEntry {
  DomKey dom_key;
  const char* string;
};

#define DOM_KEY_MAP(key, id) {DomKey::id, key}
#define DOM_KEY_MAP_DECLARATION const DomKeyMapEntry dom_key_map[] =
#include "ui/events/keycodes/dom3/dom_key_data.h"
#undef DOM_KEY_MAP
#undef DOM_KEY_MAP_DECLARATION

const size_t kDomKeyMapEntries = arraysize(dom_key_map);

}  // namespace

// static
size_t KeycodeConverter::NumKeycodeMapEntriesForTest() {
  return kKeycodeMapEntries;
}

// static
const KeycodeMapEntry* KeycodeConverter::GetKeycodeMapForTest() {
  return &usb_keycode_map[0];
}

// static
const char* KeycodeConverter::DomKeyStringForTest(size_t index) {
  if (index >= kDomKeyMapEntries)
    return nullptr;
  return dom_key_map[index].string;
}

// static
int KeycodeConverter::InvalidNativeKeycode() {
  return usb_keycode_map[0].native_keycode;
}

// static
const char* KeycodeConverter::NativeKeycodeToCode(int native_keycode) {
  for (size_t i = 0; i < kKeycodeMapEntries; ++i) {
    if (usb_keycode_map[i].native_keycode == native_keycode) {
      if (usb_keycode_map[i].code != NULL)
        return usb_keycode_map[i].code;
      break;
    }
  }
  return "";
}

// static
DomCode KeycodeConverter::NativeKeycodeToDomCode(int native_keycode) {
  for (size_t i = 0; i < kKeycodeMapEntries; ++i) {
    if (usb_keycode_map[i].native_keycode == native_keycode) {
      if (usb_keycode_map[i].code != NULL)
        return static_cast<DomCode>(usb_keycode_map[i].usb_keycode);
      break;
    }
  }
  return DomCode::NONE;
}

// static
int KeycodeConverter::CodeToNativeKeycode(const char* code) {
  if (!code || !*code)
    return InvalidNativeKeycode();

  for (size_t i = 0; i < kKeycodeMapEntries; ++i) {
    if (usb_keycode_map[i].code &&
        strcmp(usb_keycode_map[i].code, code) == 0) {
      return usb_keycode_map[i].native_keycode;
    }
  }
  return InvalidNativeKeycode();
}

// static
int KeycodeConverter::DomCodeToNativeKeycode(DomCode code) {
  if (code == DomCode::NONE)
    return InvalidNativeKeycode();
  for (size_t i = 0; i < kKeycodeMapEntries; ++i) {
    if (usb_keycode_map[i].usb_keycode == static_cast<uint32_t>(code))
      return usb_keycode_map[i].native_keycode;
  }
  return InvalidNativeKeycode();
}

// static
DomCode KeycodeConverter::CodeStringToDomCode(const char* code) {
  if (!code || !*code)
    return DomCode::NONE;
  for (size_t i = 0; i < kKeycodeMapEntries; ++i) {
    if (usb_keycode_map[i].code &&
        strcmp(usb_keycode_map[i].code, code) == 0) {
      return static_cast<DomCode>(usb_keycode_map[i].usb_keycode);
    }
  }
  return DomCode::NONE;
}

// static
const char* KeycodeConverter::DomCodeToCodeString(DomCode dom_code) {
  return UsbKeycodeToCode(static_cast<uint32_t>(dom_code));
}

// static
DomKey KeycodeConverter::KeyStringToDomKey(const char* key) {
  if (!key || !*key)
    return DomKey::NONE;
  for (size_t i = 0; i < kDomKeyMapEntries; ++i) {
    if (dom_key_map[i].string &&
        strcmp(dom_key_map[i].string, key) == 0) {
      return dom_key_map[i].dom_key;
    }
  }
  return DomKey::NONE;
}

// static
const char* KeycodeConverter::DomKeyToKeyString(DomKey dom_key) {
  for (size_t i = 0; i < kDomKeyMapEntries; ++i) {
    if (dom_key_map[i].dom_key == dom_key)
      return dom_key_map[i].string;
  }
  return "";
}

// USB keycodes
// Note that USB keycodes are not part of any web standard.
// Please don't use USB keycodes in new code.

// static
uint32_t KeycodeConverter::InvalidUsbKeycode() {
  return usb_keycode_map[0].usb_keycode;
}

// static
int KeycodeConverter::UsbKeycodeToNativeKeycode(uint32_t usb_keycode) {
  // Deal with some special-cases that don't fit the 1:1 mapping.
  if (usb_keycode == 0x070032) // non-US hash.
    usb_keycode = 0x070031; // US backslash.
#if defined(OS_MACOSX)
  if (usb_keycode == 0x070046) // PrintScreen.
    usb_keycode = 0x070068; // F13.
#endif

  for (size_t i = 0; i < kKeycodeMapEntries; ++i) {
    if (usb_keycode_map[i].usb_keycode == usb_keycode)
      return usb_keycode_map[i].native_keycode;
  }
  return InvalidNativeKeycode();
}

// static
uint32_t KeycodeConverter::NativeKeycodeToUsbKeycode(int native_keycode) {
  for (size_t i = 0; i < kKeycodeMapEntries; ++i) {
    if (usb_keycode_map[i].native_keycode == native_keycode)
      return usb_keycode_map[i].usb_keycode;
  }
  return InvalidUsbKeycode();
}

// static
const char* KeycodeConverter::UsbKeycodeToCode(uint32_t usb_keycode) {
  for (size_t i = 0; i < kKeycodeMapEntries; ++i) {
    if (usb_keycode_map[i].usb_keycode == usb_keycode) {
      if (usb_keycode_map[i].code)
        return usb_keycode_map[i].code;
      break;
    }
  }
  return "";
}

// static
DomCode KeycodeConverter::UsbKeycodeToDomCode(uint32_t usb_keycode) {
  for (size_t i = 0; i < kKeycodeMapEntries; ++i) {
    if (usb_keycode_map[i].usb_keycode == usb_keycode)
      return static_cast<DomCode>(usb_keycode);
  }
  return DomCode::NONE;
}

// static
uint32_t KeycodeConverter::CodeToUsbKeycode(const char* code) {
  if (!code || !*code)
    return InvalidUsbKeycode();

  for (size_t i = 0; i < kKeycodeMapEntries; ++i) {
    if (usb_keycode_map[i].code &&
        strcmp(usb_keycode_map[i].code, code) == 0) {
      return usb_keycode_map[i].usb_keycode;
    }
  }
  return InvalidUsbKeycode();
}

}  // namespace ui
