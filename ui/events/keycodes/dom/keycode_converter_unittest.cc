// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/keycodes/dom/keycode_converter.h"

#include <stddef.h>
#include <stdint.h>

#include <map>

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"

using ui::KeycodeConverter;

namespace {

#if defined(OS_WIN)
const size_t kExpectedMappedKeyCount = 157;
#elif defined(OS_LINUX) || defined(OS_ANDROID)
const size_t kExpectedMappedKeyCount = 178;
#elif defined(OS_MACOSX)
const size_t kExpectedMappedKeyCount = 118;
#else
const size_t kExpectedMappedKeyCount = 0;
#endif

const uint32_t kUsbNonExistentKeycode = 0xffffff;
const uint32_t kUsbUsBackslash =        0x070031;
const uint32_t kUsbNonUsHash =          0x070032;

TEST(UsbKeycodeMap, Basic) {
  // Verify that the first element in the table is the "invalid" code.
  const ui::KeycodeMapEntry* keycode_map =
      ui::KeycodeConverter::GetKeycodeMapForTest();
  EXPECT_EQ(ui::KeycodeConverter::InvalidUsbKeycode(),
            keycode_map[0].usb_keycode);
  EXPECT_EQ(ui::KeycodeConverter::InvalidNativeKeycode(),
            keycode_map[0].native_keycode);
  EXPECT_EQ(ui::KeycodeConverter::InvalidNativeKeycode(),
            ui::KeycodeConverter::DomCodeToNativeKeycode(ui::DomCode::NONE));

  // Verify that there are no duplicate entries in the mapping.
  std::map<uint32_t, uint16_t> usb_to_native;
  std::map<uint16_t, uint32_t> native_to_usb;
  size_t numEntries = ui::KeycodeConverter::NumKeycodeMapEntriesForTest();
  for (size_t i = 0; i < numEntries; ++i) {
    const ui::KeycodeMapEntry* entry = &keycode_map[i];
    // Don't test keys with no native keycode mapping on this platform.
    if (entry->native_keycode == ui::KeycodeConverter::InvalidNativeKeycode())
      continue;

    // Verify UsbKeycodeToNativeKeycode works for this key.
    EXPECT_EQ(
        entry->native_keycode,
        ui::KeycodeConverter::UsbKeycodeToNativeKeycode(entry->usb_keycode));

    // Verify DomCodeToNativeKeycode works correctly.
    if (entry->code && *entry->code) {
      ui::DomCode dom_code =
          ui::KeycodeConverter::CodeStringToDomCode(entry->code);
      EXPECT_EQ(entry->native_keycode,
                ui::KeycodeConverter::DomCodeToNativeKeycode(dom_code));
    }

    // Verify that the USB or native codes aren't duplicated.
    EXPECT_EQ(0U, usb_to_native.count(entry->usb_keycode))
        << " duplicate of USB code 0x" << std::hex << std::setfill('0')
        << std::setw(6) << entry->usb_keycode
        << " to native 0x"
        << std::setw(4) << entry->native_keycode
        << " (previous was 0x"
        << std::setw(4) << usb_to_native[entry->usb_keycode]
        << ")";
    usb_to_native[entry->usb_keycode] = entry->native_keycode;
    EXPECT_EQ(0U, native_to_usb.count(entry->native_keycode))
        << " duplicate of native code 0x" << std::hex << std::setfill('0')
        << std::setw(4) << entry->native_keycode
        << " to USB 0x"
        << std::setw(6) << entry->usb_keycode
        << " (previous was 0x"
        << std::setw(6) << native_to_usb[entry->native_keycode]
        << ")";
    native_to_usb[entry->native_keycode] = entry->usb_keycode;
  }
  ASSERT_EQ(usb_to_native.size(), native_to_usb.size());

  // Verify that the number of mapped keys is what we expect, i.e. we haven't
  // lost any, and if we've added some then the expectation has been updated.
  EXPECT_EQ(kExpectedMappedKeyCount, usb_to_native.size());
}

TEST(UsbKeycodeMap, NonExistent) {
  // Verify that UsbKeycodeToNativeKeycode works for a non-existent USB keycode.
  EXPECT_EQ(
      ui::KeycodeConverter::InvalidNativeKeycode(),
      ui::KeycodeConverter::UsbKeycodeToNativeKeycode(kUsbNonExistentKeycode));
}

TEST(UsbKeycodeMap, UsBackslashIsNonUsHash) {
  // Verify that UsbKeycodeToNativeKeycode treats the non-US "hash" key
  // as equivalent to the US "backslash" key.
  EXPECT_EQ(ui::KeycodeConverter::UsbKeycodeToNativeKeycode(kUsbUsBackslash),
            ui::KeycodeConverter::UsbKeycodeToNativeKeycode(kUsbNonUsHash));
}

TEST(KeycodeConverter, DomCode) {
  // Test invalid and unknown arguments to CodeStringToDomCode()
  EXPECT_EQ(ui::DomCode::NONE, ui::KeycodeConverter::CodeStringToDomCode("-"));
  EXPECT_EQ(ui::DomCode::NONE, ui::KeycodeConverter::CodeStringToDomCode(""));
  // Round-trip test DOM Level 3 .code strings.
  const ui::KeycodeMapEntry* keycode_map =
      ui::KeycodeConverter::GetKeycodeMapForTest();
  size_t numEntries = ui::KeycodeConverter::NumKeycodeMapEntriesForTest();
  for (size_t i = 0; i < numEntries; ++i) {
    SCOPED_TRACE(i);
    const ui::KeycodeMapEntry* entry = &keycode_map[i];
    if (entry->code) {
      ui::DomCode code = ui::KeycodeConverter::CodeStringToDomCode(entry->code);
      EXPECT_STREQ(entry->code,
                   ui::KeycodeConverter::DomCodeToCodeString(code));
    }
  }
}

TEST(KeycodeConverter, DomKey) {
  const struct {
    ui::DomKey::Base key;
    bool is_character;
    bool is_dead;
    bool test_to_string;
    const char* const string;
  } test_cases[] = {
      // Invalid arguments to KeyStringToDomKey().
      {ui::DomKey::NONE, false, false, true, ""},
      {ui::DomKey::NONE, false, false, false, "?!?"},
      {ui::DomKey::NONE, false, false, false, "\x61\xCC\x81"},
      // Some single Unicode characters.
      {ui::DomKey::Constant<'-'>::Character, true, false, true, "-"},
      {ui::DomKey::Constant<'A'>::Character, true, false, true, "A"},
      {ui::DomKey::Constant<0xE1>::Character, true, false, true, "\xC3\xA1"},
      {ui::DomKey::Constant<0x1F648>::Character, true, false, true,
       "\xF0\x9F\x99\x88"},
      // Unicode-equivalent named values.
      {ui::DomKey::BACKSPACE, true, false, true, "Backspace"},
      {ui::DomKey::TAB, true, false, true, "Tab"},
      {ui::DomKey::ENTER, true, false, true, "Enter"},
      {ui::DomKey::ESCAPE, true, false, true, "Escape"},
      {ui::DomKey::DEL, true, false, true, "Delete"},
      {ui::DomKey::BACKSPACE, true, false, false, "\b"},
      {ui::DomKey::TAB, true, false, false, "\t"},
      {ui::DomKey::ENTER, true, false, false, "\r"},
      {ui::DomKey::ESCAPE, true, false, false, "\x1B"},
      {ui::DomKey::DEL, true, false, false, "\x7F"},
      {ui::DomKey::Constant<'\b'>::Character, true, false, true, "Backspace"},
      {ui::DomKey::Constant<'\t'>::Character, true, false, true, "Tab"},
      {ui::DomKey::Constant<'\r'>::Character, true, false, true, "Enter"},
      {ui::DomKey::Constant<0x1B>::Character, true, false, true, "Escape"},
      {ui::DomKey::Constant<0x7F>::Character, true, false, true, "Delete"},
      // 'Dead' key.
      {ui::DomKey::Constant<0xFFFF>::Dead, false, true, true, "Dead"},
      // Sample non-Unicode key names.
      {ui::DomKey::SHIFT, false, false, true, "Shift"},
      {ui::DomKey::F16, false, false, true, "F16"},
      {ui::DomKey::ZOOM_IN, false, false, true, "ZoomIn"},
      {ui::DomKey::UNIDENTIFIED, false, false, true, "Unidentified"},
  };
  for (const auto& test : test_cases) {
    // Check KeyStringToDomKey().
    ui::DomKey key = ui::KeycodeConverter::KeyStringToDomKey(test.string);
    EXPECT_EQ(test.is_character, key.IsCharacter());
    EXPECT_EQ(test.is_dead, key.IsDeadKey());
    EXPECT_EQ(test.key, key);
    // Check |DomKeyToKeyString()|.
    if (test.test_to_string) {
      std::string s(ui::KeycodeConverter::DomKeyToKeyString(test.key));
      EXPECT_STREQ(test.string, s.c_str());
    }
  }
  // Round-trip test all UI Events KeyboardEvent.key strings.
  const char* s = nullptr;
  for (size_t i = 0;
       (s = ui::KeycodeConverter::DomKeyStringForTest(i)) != nullptr; ++i) {
    SCOPED_TRACE(i);
    ui::DomKey key = ui::KeycodeConverter::KeyStringToDomKey(s);
    if (s) {
      EXPECT_STREQ(s, ui::KeycodeConverter::DomKeyToKeyString(key).c_str());
    } else {
      EXPECT_EQ(ui::DomKey::NONE, key);
    }
  }
}

}  // namespace
