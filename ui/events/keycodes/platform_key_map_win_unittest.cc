// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/keycodes/platform_key_map_win.h"

#include "base/macros.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/dom/keycode_converter.h"

namespace ui {

namespace {

const wchar_t* LAYOUT_US = L"00000409";
const wchar_t* LAYOUT_FR = L"0000040c";

struct TestKey {
  // Have to use KeyboardCode instead of DomCode because we don't know the
  // physical keyboard layout for try bots.
  KeyboardCode key_code;
  const char* normal;
  const char* shift;
  const char* capslock;
  const char* altgr;
  const char* shift_capslock;
  const char* shift_altgr;
  const char* altgr_capslock;
};

}  // anonymous namespace

class PlatformKeyMapTest : public testing::Test {
 public:
  PlatformKeyMapTest() {}
  ~PlatformKeyMapTest() override {}

  void CheckDomCodeToKeyString(const char* label,
                               const PlatformKeyMap& keymap,
                               const TestKey& test_case,
                               HKL layout) {
    KeyboardCode key_code = test_case.key_code;
    int scan_code = ::MapVirtualKeyEx(key_code, MAPVK_VK_TO_VSC, layout);
    DomCode dom_code = KeycodeConverter::NativeKeycodeToDomCode(scan_code);
    EXPECT_STREQ(test_case.normal,
                 KeycodeConverter::DomKeyToKeyString(
                     keymap.DomKeyFromNativeImpl(dom_code, key_code, EF_NONE))
                     .c_str())
        << label;
    EXPECT_STREQ(test_case.shift, KeycodeConverter::DomKeyToKeyString(
                                      keymap.DomKeyFromNativeImpl(
                                          dom_code, key_code, EF_SHIFT_DOWN))
                                      .c_str())
        << label;
    EXPECT_STREQ(
        test_case.capslock,
        KeycodeConverter::DomKeyToKeyString(
            keymap.DomKeyFromNativeImpl(dom_code, key_code, EF_CAPS_LOCK_ON))
            .c_str())
        << label;
    EXPECT_STREQ(test_case.altgr, KeycodeConverter::DomKeyToKeyString(
                                      keymap.DomKeyFromNativeImpl(
                                          dom_code, key_code, EF_ALTGR_DOWN))
                                      .c_str())
        << label;
    EXPECT_STREQ(test_case.shift_capslock,
                 KeycodeConverter::DomKeyToKeyString(
                     keymap.DomKeyFromNativeImpl(
                         dom_code, key_code, EF_SHIFT_DOWN | EF_CAPS_LOCK_ON))
                     .c_str())
        << label;
    EXPECT_STREQ(test_case.shift_altgr,
                 KeycodeConverter::DomKeyToKeyString(
                     keymap.DomKeyFromNativeImpl(dom_code, key_code,
                                                 EF_SHIFT_DOWN | EF_ALTGR_DOWN))
                     .c_str())
        << label;
    EXPECT_STREQ(test_case.altgr_capslock,
                 KeycodeConverter::DomKeyToKeyString(
                     keymap.DomKeyFromNativeImpl(
                         dom_code, key_code, EF_ALTGR_DOWN | EF_CAPS_LOCK_ON))
                     .c_str())
        << label;
  }

  DomKey DomKeyFromNativeImpl(const PlatformKeyMap& keymap,
                              DomCode dom_code,
                              KeyboardCode key_code,
                              int flags) {
    return keymap.DomKeyFromNativeImpl(dom_code, key_code, flags);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(PlatformKeyMapTest);
};

TEST_F(PlatformKeyMapTest, USLayout) {
  HKL layout = ::LoadKeyboardLayout(LAYOUT_US, 0);
  PlatformKeyMap keymap(layout);

  const TestKey kUSLayoutTestCases[] = {
      //       n    s    c    a    sc   sa   ac
      {VKEY_0, "0", ")", "0", "0", ")", ")", "0"},
      {VKEY_1, "1", "!", "1", "1", "!", "!", "1"},
      {VKEY_2, "2", "@", "2", "2", "@", "@", "2"},
      {VKEY_3, "3", "#", "3", "3", "#", "#", "3"},
      {VKEY_4, "4", "$", "4", "4", "$", "$", "4"},
      {VKEY_5, "5", "%", "5", "5", "%", "%", "5"},
      {VKEY_6, "6", "^", "6", "6", "^", "^", "6"},
      {VKEY_7, "7", "&", "7", "7", "&", "&", "7"},
      {VKEY_8, "8", "*", "8", "8", "*", "*", "8"},
      {VKEY_9, "9", "(", "9", "9", "(", "(", "9"},
      {VKEY_A, "a", "A", "A", "a", "a", "A", "A"},
      {VKEY_B, "b", "B", "B", "b", "b", "B", "B"},
      {VKEY_C, "c", "C", "C", "c", "c", "C", "C"},
      {VKEY_D, "d", "D", "D", "d", "d", "D", "D"},
      {VKEY_E, "e", "E", "E", "e", "e", "E", "E"},
      {VKEY_F, "f", "F", "F", "f", "f", "F", "F"},
      {VKEY_G, "g", "G", "G", "g", "g", "G", "G"},
      {VKEY_H, "h", "H", "H", "h", "h", "H", "H"},
      {VKEY_I, "i", "I", "I", "i", "i", "I", "I"},
      {VKEY_J, "j", "J", "J", "j", "j", "J", "J"},
      {VKEY_K, "k", "K", "K", "k", "k", "K", "K"},
      {VKEY_L, "l", "L", "L", "l", "l", "L", "L"},
      {VKEY_M, "m", "M", "M", "m", "m", "M", "M"},
      {VKEY_N, "n", "N", "N", "n", "n", "N", "N"},
      {VKEY_O, "o", "O", "O", "o", "o", "O", "O"},
      {VKEY_P, "p", "P", "P", "p", "p", "P", "P"},
      {VKEY_Q, "q", "Q", "Q", "q", "q", "Q", "Q"},
      {VKEY_R, "r", "R", "R", "r", "r", "R", "R"},
      {VKEY_S, "s", "S", "S", "s", "s", "S", "S"},
      {VKEY_T, "t", "T", "T", "t", "t", "T", "T"},
      {VKEY_U, "u", "U", "U", "u", "u", "U", "U"},
      {VKEY_V, "v", "V", "V", "v", "v", "V", "V"},
      {VKEY_W, "w", "W", "W", "w", "w", "W", "W"},
      {VKEY_X, "x", "X", "X", "x", "x", "X", "X"},
      {VKEY_Y, "y", "Y", "Y", "y", "y", "Y", "Y"},
      {VKEY_Z, "z", "Z", "Z", "z", "z", "Z", "Z"},
  };

  for (const auto& test_case : kUSLayoutTestCases) {
    CheckDomCodeToKeyString("USLayout", keymap, test_case, layout);
  }
}

TEST_F(PlatformKeyMapTest, FRLayout) {
  HKL layout = ::LoadKeyboardLayout(LAYOUT_FR, 0);
  PlatformKeyMap keymap(layout);

  const TestKey kFRLayoutTestCases[] = {
      //       n     s    c    a       sc    sa   ac
      {VKEY_0, "à",  "0", "0", "@",    "à",  "0", "@"},
      {VKEY_1, "&",  "1", "1", "&",    "&",  "1", "1"},
      {VKEY_2, "é",  "2", "2", "Dead", "é",  "2", "Dead"},
      {VKEY_3, "\"", "3", "3", "#",    "\"", "3", "#"},
      {VKEY_4, "\'", "4", "4", "{",    "\'", "4", "{"},
      {VKEY_5, "(",  "5", "5", "[",    "(",  "5", "["},
      {VKEY_6, "-",  "6", "6", "|",    "-",  "6", "|"},
      {VKEY_7, "è",  "7", "7", "Dead", "è",  "7", "Dead"},
      {VKEY_8, "_",  "8", "8", "\\",   "_",  "8", "\\"},
      {VKEY_9, "ç",  "9", "9", "^",    "ç",  "9", "^"},
      {VKEY_A, "a",  "A", "A", "a",    "a",  "A", "A"},
      {VKEY_B, "b",  "B", "B", "b",    "b",  "B", "B"},
      {VKEY_C, "c",  "C", "C", "c",    "c",  "C", "C"},
      {VKEY_D, "d",  "D", "D", "d",    "d",  "D", "D"},
      {VKEY_E, "e",  "E", "E", "€",    "e",  "E", "€"},
      {VKEY_F, "f",  "F", "F", "f",    "f",  "F", "F"},
      {VKEY_G, "g",  "G", "G", "g",    "g",  "G", "G"},
      {VKEY_H, "h",  "H", "H", "h",    "h",  "H", "H"},
      {VKEY_I, "i",  "I", "I", "i",    "i",  "I", "I"},
      {VKEY_J, "j",  "J", "J", "j",    "j",  "J", "J"},
      {VKEY_K, "k",  "K", "K", "k",    "k",  "K", "K"},
      {VKEY_L, "l",  "L", "L", "l",    "l",  "L", "L"},
      {VKEY_M, "m",  "M", "M", "m",    "m",  "M", "M"},
      {VKEY_N, "n",  "N", "N", "n",    "n",  "N", "N"},
      {VKEY_O, "o",  "O", "O", "o",    "o",  "O", "O"},
      {VKEY_P, "p",  "P", "P", "p",    "p",  "P", "P"},
      {VKEY_Q, "q",  "Q", "Q", "q",    "q",  "Q", "Q"},
      {VKEY_R, "r",  "R", "R", "r",    "r",  "R", "R"},
      {VKEY_S, "s",  "S", "S", "s",    "s",  "S", "S"},
      {VKEY_T, "t",  "T", "T", "t",    "t",  "T", "T"},
      {VKEY_U, "u",  "U", "U", "u",    "u",  "U", "U"},
      {VKEY_V, "v",  "V", "V", "v",    "v",  "V", "V"},
      {VKEY_W, "w",  "W", "W", "w",    "w",  "W", "W"},
      {VKEY_X, "x",  "X", "X", "x",    "x",  "X", "X"},
      {VKEY_Y, "y",  "Y", "Y", "y",    "y",  "Y", "Y"},
      {VKEY_Z, "z",  "Z", "Z", "z",    "z",  "Z", "Z"},
  };

  for (const auto& test_case : kFRLayoutTestCases) {
    CheckDomCodeToKeyString("FRLayout", keymap, test_case, layout);
  }
}

TEST_F(PlatformKeyMapTest, NumPad) {
  HKL layout = ::LoadKeyboardLayout(LAYOUT_US, 0);
  PlatformKeyMap keymap(layout);

  const struct TestCase {
    KeyboardCode key_code;
    DomKey key;
  } kNumPadTestCases[] = {
      {VKEY_NUMPAD0, DomKey::FromCharacter('0')},
      {VKEY_NUMPAD1, DomKey::FromCharacter('1')},
      {VKEY_NUMPAD2, DomKey::FromCharacter('2')},
      {VKEY_NUMPAD3, DomKey::FromCharacter('3')},
      {VKEY_NUMPAD4, DomKey::FromCharacter('4')},
      {VKEY_NUMPAD5, DomKey::FromCharacter('5')},
      {VKEY_NUMPAD6, DomKey::FromCharacter('6')},
      {VKEY_NUMPAD7, DomKey::FromCharacter('7')},
      {VKEY_NUMPAD8, DomKey::FromCharacter('8')},
      {VKEY_NUMPAD9, DomKey::FromCharacter('9')},
      {VKEY_CLEAR, DomKey::CLEAR},
      {VKEY_PRIOR, DomKey::PAGE_UP},
      {VKEY_NEXT, DomKey::PAGE_DOWN},
      {VKEY_END, DomKey::END},
      {VKEY_HOME, DomKey::HOME},
      {VKEY_LEFT, DomKey::ARROW_LEFT},
      {VKEY_UP, DomKey::ARROW_UP},
      {VKEY_RIGHT, DomKey::ARROW_RIGHT},
      {VKEY_DOWN, DomKey::ARROW_DOWN},
      {VKEY_INSERT, DomKey::INSERT},
      {VKEY_DELETE, DomKey::DEL},
  };

  for (const auto& test_case : kNumPadTestCases) {
    KeyboardCode key_code = test_case.key_code;
    int scan_code = ::MapVirtualKeyEx(key_code, MAPVK_VK_TO_VSC, layout);
    DomCode dom_code = KeycodeConverter::NativeKeycodeToDomCode(scan_code);

    EXPECT_EQ(test_case.key,
              DomKeyFromNativeImpl(keymap, dom_code, key_code, EF_NONE))
        << key_code;
    EXPECT_EQ(test_case.key,
              DomKeyFromNativeImpl(keymap, dom_code, key_code, EF_ALTGR_DOWN))
        << key_code;
    EXPECT_EQ(test_case.key,
              DomKeyFromNativeImpl(keymap, dom_code, key_code, EF_CONTROL_DOWN))
        << key_code;
    EXPECT_EQ(test_case.key,
              DomKeyFromNativeImpl(keymap, dom_code, key_code,
                                   EF_ALTGR_DOWN | EF_CONTROL_DOWN))
        << key_code;
  }
}

}  // namespace ui
