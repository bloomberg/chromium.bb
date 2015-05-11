// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/keycodes/keyboard_code_conversion.h"

#include "base/basictypes.h"
#include "base/strings/stringprintf.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace {

struct Meaning {
  bool defined;
  ui::DomKey dom_key;
  base::char16 character;
  ui::KeyboardCode legacy_key_code;
};

const Meaning kUndefined = {false, ui::DomKey::NONE, 0, ui::VKEY_UNKNOWN};

void CheckDomCodeToMeaning(const char* label,
                           bool f(ui::DomCode dom_code,
                                  int flags,
                                  ui::DomKey* out_dom_key,
                                  base::char16* out_character,
                                  ui::KeyboardCode* out_key_code),
                           ui::DomCode dom_code,
                           int event_flags,
                           const Meaning& result) {
  ui::DomKey result_dom_key = ui::DomKey::NONE;
  base::char16 result_character = 0;
  ui::KeyboardCode result_legacy_key_code = ui::VKEY_UNKNOWN;
  bool success = f(dom_code, event_flags, &result_dom_key, &result_character,
                   &result_legacy_key_code);
  SCOPED_TRACE(
      base::StringPrintf("%s %s %06X:%04X", label,
                         ui::KeycodeConverter::DomCodeToCodeString(dom_code),
                         static_cast<int>(dom_code), event_flags));
  EXPECT_EQ(result.defined, success);
  if (success) {
    EXPECT_EQ(result.dom_key, result_dom_key);
    EXPECT_EQ(result.character, result_character);
    EXPECT_EQ(result.legacy_key_code, result_legacy_key_code);
  } else {
    // Should not have touched output parameters.
    EXPECT_EQ(ui::DomKey::NONE, result_dom_key);
    EXPECT_EQ(0, result_character);
    EXPECT_EQ(ui::VKEY_UNKNOWN, result_legacy_key_code);
  }
}

TEST(KeyboardCodeConversion, ControlCharacters) {
  // The codes in this table are handled by |DomCodeToControlCharacter()|.
  static const struct {
    ui::DomCode dom_code;
    Meaning control;
    Meaning control_shift;
  } kControlCharacters[] = {
      {ui::DomCode::KEY_A,
       {true, ui::DomKey::CHARACTER, 0x01, ui::VKEY_A},
       {true, ui::DomKey::CHARACTER, 0x01, ui::VKEY_A}},
      {ui::DomCode::KEY_B,
       {true, ui::DomKey::CHARACTER, 0x02, ui::VKEY_B},
       {true, ui::DomKey::CHARACTER, 0x02, ui::VKEY_B}},
      {ui::DomCode::KEY_C,
       {true, ui::DomKey::CHARACTER, 0x03, ui::VKEY_C},
       {true, ui::DomKey::CHARACTER, 0x03, ui::VKEY_C}},
      {ui::DomCode::KEY_D,
       {true, ui::DomKey::CHARACTER, 0x04, ui::VKEY_D},
       {true, ui::DomKey::CHARACTER, 0x04, ui::VKEY_D}},
      {ui::DomCode::KEY_E,
       {true, ui::DomKey::CHARACTER, 0x05, ui::VKEY_E},
       {true, ui::DomKey::CHARACTER, 0x05, ui::VKEY_E}},
      {ui::DomCode::KEY_F,
       {true, ui::DomKey::CHARACTER, 0x06, ui::VKEY_F},
       {true, ui::DomKey::CHARACTER, 0x06, ui::VKEY_F}},
      {ui::DomCode::KEY_G,
       {true, ui::DomKey::CHARACTER, 0x07, ui::VKEY_G},
       {true, ui::DomKey::CHARACTER, 0x07, ui::VKEY_G}},
      {ui::DomCode::KEY_H,
       {true, ui::DomKey::BACKSPACE, 0x08, ui::VKEY_BACK},
       {true, ui::DomKey::BACKSPACE, 0x08, ui::VKEY_BACK}},
      {ui::DomCode::KEY_I,
       {true, ui::DomKey::TAB, 0x09, ui::VKEY_TAB},
       {true, ui::DomKey::TAB, 0x09, ui::VKEY_TAB}},
      {ui::DomCode::KEY_J,
       {true, ui::DomKey::CHARACTER, 0x0A, ui::VKEY_J},
       {true, ui::DomKey::CHARACTER, 0x0A, ui::VKEY_J}},
      {ui::DomCode::KEY_K,
       {true, ui::DomKey::CHARACTER, 0x0B, ui::VKEY_K},
       {true, ui::DomKey::CHARACTER, 0x0B, ui::VKEY_K}},
      {ui::DomCode::KEY_L,
       {true, ui::DomKey::CHARACTER, 0x0C, ui::VKEY_L},
       {true, ui::DomKey::CHARACTER, 0x0C, ui::VKEY_L}},
      {ui::DomCode::KEY_M,
       {true, ui::DomKey::ENTER, 0x0D, ui::VKEY_RETURN},
       {true, ui::DomKey::ENTER, 0x0D, ui::VKEY_RETURN}},
      {ui::DomCode::KEY_N,
       {true, ui::DomKey::CHARACTER, 0x0E, ui::VKEY_N},
       {true, ui::DomKey::CHARACTER, 0x0E, ui::VKEY_N}},
      {ui::DomCode::KEY_O,
       {true, ui::DomKey::CHARACTER, 0x0F, ui::VKEY_O},
       {true, ui::DomKey::CHARACTER, 0x0F, ui::VKEY_O}},
      {ui::DomCode::KEY_P,
       {true, ui::DomKey::CHARACTER, 0x10, ui::VKEY_P},
       {true, ui::DomKey::CHARACTER, 0x10, ui::VKEY_P}},
      {ui::DomCode::KEY_Q,
       {true, ui::DomKey::CHARACTER, 0x11, ui::VKEY_Q},
       {true, ui::DomKey::CHARACTER, 0x11, ui::VKEY_Q}},
      {ui::DomCode::KEY_R,
       {true, ui::DomKey::CHARACTER, 0x12, ui::VKEY_R},
       {true, ui::DomKey::CHARACTER, 0x12, ui::VKEY_R}},
      {ui::DomCode::KEY_S,
       {true, ui::DomKey::CHARACTER, 0x13, ui::VKEY_S},
       {true, ui::DomKey::CHARACTER, 0x13, ui::VKEY_S}},
      {ui::DomCode::KEY_T,
       {true, ui::DomKey::CHARACTER, 0x14, ui::VKEY_T},
       {true, ui::DomKey::CHARACTER, 0x14, ui::VKEY_T}},
      {ui::DomCode::KEY_U,
       {true, ui::DomKey::CHARACTER, 0x15, ui::VKEY_U},
       {true, ui::DomKey::CHARACTER, 0x15, ui::VKEY_U}},
      {ui::DomCode::KEY_V,
       {true, ui::DomKey::CHARACTER, 0x16, ui::VKEY_V},
       {true, ui::DomKey::CHARACTER, 0x16, ui::VKEY_V}},
      {ui::DomCode::KEY_W,
       {true, ui::DomKey::CHARACTER, 0x17, ui::VKEY_W},
       {true, ui::DomKey::CHARACTER, 0x17, ui::VKEY_W}},
      {ui::DomCode::KEY_X,
       {true, ui::DomKey::CHARACTER, 0x18, ui::VKEY_X},
       {true, ui::DomKey::CHARACTER, 0x18, ui::VKEY_X}},
      {ui::DomCode::KEY_Y,
       {true, ui::DomKey::CHARACTER, 0x19, ui::VKEY_Y},
       {true, ui::DomKey::CHARACTER, 0x19, ui::VKEY_Y}},
      {ui::DomCode::KEY_Z,
       {true, ui::DomKey::CHARACTER, 0x1A, ui::VKEY_Z},
       {true, ui::DomKey::CHARACTER, 0x1A, ui::VKEY_Z}},
  };
  for (const auto& it : kControlCharacters) {
    // Verify |DomCodeToControlCharacter()|.
    CheckDomCodeToMeaning("c_cc_n", ui::DomCodeToControlCharacter, it.dom_code,
                          ui::EF_NONE, kUndefined);
    CheckDomCodeToMeaning("c_cc_c", ui::DomCodeToControlCharacter, it.dom_code,
                           ui::EF_CONTROL_DOWN, it.control);
    CheckDomCodeToMeaning("c_cc_cs", ui::DomCodeToControlCharacter, it.dom_code,
                          ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN,
                          it.control_shift);
    // Verify |DomCodeToUsLayoutMeaning()|.
    CheckDomCodeToMeaning("c_us_c", ui::DomCodeToUsLayoutMeaning, it.dom_code,
                          ui::EF_CONTROL_DOWN, it.control);
    CheckDomCodeToMeaning("c_us_cs", ui::DomCodeToUsLayoutMeaning, it.dom_code,
                          ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN,
                          it.control_shift);
  }

  // The codes in this table are sensitive to the Shift state, so they are
  // handled differently by |DomCodeToControlCharacter()|, which returns false
  // for unknown combinations, vs |DomCodeToUsLayoutMeaning()|, which returns
  // true with DomKey::UNIDENTIFIED.
  static const struct {
    ui::DomCode dom_code;
    Meaning cc_control;
    Meaning cc_control_shift;
    Meaning us_control;
    Meaning us_control_shift;
  } kShiftControlCharacters[] = {
      {ui::DomCode::DIGIT2,
       {false, ui::DomKey::NONE, 0, ui::VKEY_UNKNOWN},
       {true, ui::DomKey::CHARACTER, 0, ui::VKEY_2},
       {true, ui::DomKey::UNIDENTIFIED, 0, ui::VKEY_2},
       {true, ui::DomKey::CHARACTER, 0, ui::VKEY_2}},
      {ui::DomCode::DIGIT6,
       {false, ui::DomKey::NONE, 0, ui::VKEY_UNKNOWN},
       {true, ui::DomKey::CHARACTER, 0x1E, ui::VKEY_6},
       {true, ui::DomKey::UNIDENTIFIED, 0, ui::VKEY_6},
       {true, ui::DomKey::CHARACTER, 0x1E, ui::VKEY_6}},
      {ui::DomCode::MINUS,
       {false, ui::DomKey::NONE, 0, ui::VKEY_UNKNOWN},
       {true, ui::DomKey::CHARACTER, 0x1F, ui::VKEY_OEM_MINUS},
       {true, ui::DomKey::UNIDENTIFIED, 0, ui::VKEY_OEM_MINUS},
       {true, ui::DomKey::CHARACTER, 0x1F, ui::VKEY_OEM_MINUS}},
      {ui::DomCode::ENTER,
       {true, ui::DomKey::CHARACTER, 0x0A, ui::VKEY_RETURN},
       {false, ui::DomKey::NONE, 0, ui::VKEY_UNKNOWN},
       {true, ui::DomKey::CHARACTER, 0x0A, ui::VKEY_RETURN},
       {true, ui::DomKey::UNIDENTIFIED, 0, ui::VKEY_RETURN}},
      {ui::DomCode::BRACKET_LEFT,
       {true, ui::DomKey::ESCAPE, 0x1B, ui::VKEY_OEM_4},
       {false, ui::DomKey::NONE, 0, ui::VKEY_UNKNOWN},
       {true, ui::DomKey::ESCAPE, 0x1B, ui::VKEY_OEM_4},
       {true, ui::DomKey::UNIDENTIFIED, 0, ui::VKEY_OEM_4}},
      {ui::DomCode::BACKSLASH,
       {true, ui::DomKey::CHARACTER, 0x1C, ui::VKEY_OEM_5},
       {false, ui::DomKey::NONE, 0, ui::VKEY_UNKNOWN},
       {true, ui::DomKey::CHARACTER, 0x1C, ui::VKEY_OEM_5},
       {true, ui::DomKey::UNIDENTIFIED, 0, ui::VKEY_OEM_5}},
      {ui::DomCode::BRACKET_RIGHT,
       {true, ui::DomKey::CHARACTER, 0x1D, ui::VKEY_OEM_6},
       {false, ui::DomKey::NONE, 0, ui::VKEY_UNKNOWN},
       {true, ui::DomKey::CHARACTER, 0x1D, ui::VKEY_OEM_6},
       {true, ui::DomKey::UNIDENTIFIED, 0, ui::VKEY_OEM_6}},
  };
  for (const auto& it : kShiftControlCharacters) {
    // Verify |DomCodeToControlCharacter()|.
    CheckDomCodeToMeaning("s_cc_n", ui::DomCodeToControlCharacter, it.dom_code,
                          ui::EF_NONE, kUndefined);
    CheckDomCodeToMeaning("s_cc_c", ui::DomCodeToControlCharacter, it.dom_code,
                           ui::EF_CONTROL_DOWN, it.cc_control);
    CheckDomCodeToMeaning("s_cc_cs", ui::DomCodeToControlCharacter, it.dom_code,
                          ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN,
                          it.cc_control_shift);
    // Verify |DomCodeToUsLayoutMeaning()|.
    CheckDomCodeToMeaning("s_us_c", ui::DomCodeToUsLayoutMeaning, it.dom_code,
                          ui::EF_CONTROL_DOWN, it.us_control);
    CheckDomCodeToMeaning("s_us_cs", ui::DomCodeToUsLayoutMeaning, it.dom_code,
                          ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN,
                          it.us_control_shift);
  }

  // These codes are not handled by |DomCodeToControlCharacter()| directly.
  static const struct {
    ui::DomCode dom_code;
    Meaning normal;
    Meaning control;
  } kNonControlCharacters[] = {
      // Modifiers are handled by |DomCodeToUsLayoutMeaning()| without regard
      // to whether Control is down.
      {ui::DomCode::CONTROL_LEFT,
       {true, ui::DomKey::CONTROL, 0, ui::VKEY_CONTROL},
       {true, ui::DomKey::CONTROL, 0, ui::VKEY_CONTROL}},
      {ui::DomCode::CONTROL_RIGHT,
       {true, ui::DomKey::CONTROL, 0, ui::VKEY_CONTROL},
       {true, ui::DomKey::CONTROL, 0, ui::VKEY_CONTROL}},
      {ui::DomCode::SHIFT_LEFT,
       {true, ui::DomKey::SHIFT, 0, ui::VKEY_SHIFT},
       {true, ui::DomKey::SHIFT, 0, ui::VKEY_SHIFT}},
      {ui::DomCode::SHIFT_RIGHT,
       {true, ui::DomKey::SHIFT, 0, ui::VKEY_SHIFT},
       {true, ui::DomKey::SHIFT, 0, ui::VKEY_SHIFT}},
      {ui::DomCode::ALT_LEFT,
       {true, ui::DomKey::ALT, 0, ui::VKEY_MENU},
       {true, ui::DomKey::ALT, 0, ui::VKEY_MENU}},
      {ui::DomCode::ALT_RIGHT,
       {true, ui::DomKey::ALT, 0, ui::VKEY_MENU},
       {true, ui::DomKey::ALT, 0, ui::VKEY_MENU}},
      {ui::DomCode::OS_LEFT,
       {true, ui::DomKey::OS, 0, ui::VKEY_LWIN},
       {true, ui::DomKey::OS, 0, ui::VKEY_LWIN}},
      {ui::DomCode::OS_RIGHT,
       {true, ui::DomKey::OS, 0, ui::VKEY_LWIN},
       {true, ui::DomKey::OS, 0, ui::VKEY_LWIN}},
      // Non-modifiers (a representative sample here) succeed with
      // DomKey::UNIDENTIFIED when Control is down.
      {ui::DomCode::DIGIT1,
       {true, ui::DomKey::CHARACTER, '1', ui::VKEY_1},
       {true, ui::DomKey::UNIDENTIFIED, 0, ui::VKEY_1}},
      {ui::DomCode::EQUAL,
       {true, ui::DomKey::CHARACTER, '=', ui::VKEY_OEM_PLUS},
       {true, ui::DomKey::UNIDENTIFIED, 0, ui::VKEY_OEM_PLUS}},
      {ui::DomCode::TAB,
       {true, ui::DomKey::TAB, 9, ui::VKEY_TAB},
       {true, ui::DomKey::UNIDENTIFIED, 0, ui::VKEY_TAB}},
      {ui::DomCode::F1,
       {true, ui::DomKey::F1, 0, ui::VKEY_F1},
       {true, ui::DomKey::UNIDENTIFIED, 0, ui::VKEY_F1}},
      {ui::DomCode::VOLUME_UP,
       {true, ui::DomKey::VOLUME_UP, 0, ui::VKEY_VOLUME_UP},
       {true, ui::DomKey::UNIDENTIFIED, 0, ui::VKEY_VOLUME_UP}},
  };
  for (const auto& it : kNonControlCharacters) {
    // Verify |DomCodeToControlCharacter()|.
    CheckDomCodeToMeaning("n_cc_n", ui::DomCodeToControlCharacter, it.dom_code,
                          ui::EF_NONE, kUndefined);
    CheckDomCodeToMeaning("n_cc_c", ui::DomCodeToControlCharacter, it.dom_code,
                          ui::EF_CONTROL_DOWN, kUndefined);
    CheckDomCodeToMeaning("n_cc_cs", ui::DomCodeToControlCharacter, it.dom_code,
                          ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN, kUndefined);
    // Verify |DomCodeToUsLayoutMeaning()|.
    CheckDomCodeToMeaning("n_us_n", ui::DomCodeToUsLayoutMeaning, it.dom_code,
                          ui::EF_NONE, it.normal);
    CheckDomCodeToMeaning("n_us_c", ui::DomCodeToUsLayoutMeaning, it.dom_code,
                          ui::EF_CONTROL_DOWN, it.control);
    CheckDomCodeToMeaning("n_us_cs", ui::DomCodeToUsLayoutMeaning, it.dom_code,
                          ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN, it.control);
  }
}

TEST(KeyboardCodeConversion, UsLayout) {
  static const struct {
    ui::DomCode dom_code;
    Meaning normal;
    Meaning shift;
  } kPrintableUsLayout[] = {
      {ui::DomCode::KEY_A,
       {true, ui::DomKey::CHARACTER, 'a', ui::VKEY_A},
       {true, ui::DomKey::CHARACTER, 'A', ui::VKEY_A}},
      {ui::DomCode::KEY_B,
       {true, ui::DomKey::CHARACTER, 'b', ui::VKEY_B},
       {true, ui::DomKey::CHARACTER, 'B', ui::VKEY_B}},
      {ui::DomCode::KEY_C,
       {true, ui::DomKey::CHARACTER, 'c', ui::VKEY_C},
       {true, ui::DomKey::CHARACTER, 'C', ui::VKEY_C}},
      {ui::DomCode::KEY_D,
       {true, ui::DomKey::CHARACTER, 'd', ui::VKEY_D},
       {true, ui::DomKey::CHARACTER, 'D', ui::VKEY_D}},
      {ui::DomCode::KEY_E,
       {true, ui::DomKey::CHARACTER, 'e', ui::VKEY_E},
       {true, ui::DomKey::CHARACTER, 'E', ui::VKEY_E}},
      {ui::DomCode::KEY_F,
       {true, ui::DomKey::CHARACTER, 'f', ui::VKEY_F},
       {true, ui::DomKey::CHARACTER, 'F', ui::VKEY_F}},
      {ui::DomCode::KEY_G,
       {true, ui::DomKey::CHARACTER, 'g', ui::VKEY_G},
       {true, ui::DomKey::CHARACTER, 'G', ui::VKEY_G}},
      {ui::DomCode::KEY_H,
       {true, ui::DomKey::CHARACTER, 'h', ui::VKEY_H},
       {true, ui::DomKey::CHARACTER, 'H', ui::VKEY_H}},
      {ui::DomCode::KEY_I,
       {true, ui::DomKey::CHARACTER, 'i', ui::VKEY_I},
       {true, ui::DomKey::CHARACTER, 'I', ui::VKEY_I}},
      {ui::DomCode::KEY_J,
       {true, ui::DomKey::CHARACTER, 'j', ui::VKEY_J},
       {true, ui::DomKey::CHARACTER, 'J', ui::VKEY_J}},
      {ui::DomCode::KEY_K,
       {true, ui::DomKey::CHARACTER, 'k', ui::VKEY_K},
       {true, ui::DomKey::CHARACTER, 'K', ui::VKEY_K}},
      {ui::DomCode::KEY_L,
       {true, ui::DomKey::CHARACTER, 'l', ui::VKEY_L},
       {true, ui::DomKey::CHARACTER, 'L', ui::VKEY_L}},
      {ui::DomCode::KEY_M,
       {true, ui::DomKey::CHARACTER, 'm', ui::VKEY_M},
       {true, ui::DomKey::CHARACTER, 'M', ui::VKEY_M}},
      {ui::DomCode::KEY_N,
       {true, ui::DomKey::CHARACTER, 'n', ui::VKEY_N},
       {true, ui::DomKey::CHARACTER, 'N', ui::VKEY_N}},
      {ui::DomCode::KEY_O,
       {true, ui::DomKey::CHARACTER, 'o', ui::VKEY_O},
       {true, ui::DomKey::CHARACTER, 'O', ui::VKEY_O}},
      {ui::DomCode::KEY_P,
       {true, ui::DomKey::CHARACTER, 'p', ui::VKEY_P},
       {true, ui::DomKey::CHARACTER, 'P', ui::VKEY_P}},
      {ui::DomCode::KEY_Q,
       {true, ui::DomKey::CHARACTER, 'q', ui::VKEY_Q},
       {true, ui::DomKey::CHARACTER, 'Q', ui::VKEY_Q}},
      {ui::DomCode::KEY_R,
       {true, ui::DomKey::CHARACTER, 'r', ui::VKEY_R},
       {true, ui::DomKey::CHARACTER, 'R', ui::VKEY_R}},
      {ui::DomCode::KEY_S,
       {true, ui::DomKey::CHARACTER, 's', ui::VKEY_S},
       {true, ui::DomKey::CHARACTER, 'S', ui::VKEY_S}},
      {ui::DomCode::KEY_T,
       {true, ui::DomKey::CHARACTER, 't', ui::VKEY_T},
       {true, ui::DomKey::CHARACTER, 'T', ui::VKEY_T}},
      {ui::DomCode::KEY_U,
       {true, ui::DomKey::CHARACTER, 'u', ui::VKEY_U},
       {true, ui::DomKey::CHARACTER, 'U', ui::VKEY_U}},
      {ui::DomCode::KEY_V,
       {true, ui::DomKey::CHARACTER, 'v', ui::VKEY_V},
       {true, ui::DomKey::CHARACTER, 'V', ui::VKEY_V}},
      {ui::DomCode::KEY_W,
       {true, ui::DomKey::CHARACTER, 'w', ui::VKEY_W},
       {true, ui::DomKey::CHARACTER, 'W', ui::VKEY_W}},
      {ui::DomCode::KEY_X,
       {true, ui::DomKey::CHARACTER, 'x', ui::VKEY_X},
       {true, ui::DomKey::CHARACTER, 'X', ui::VKEY_X}},
      {ui::DomCode::KEY_Y,
       {true, ui::DomKey::CHARACTER, 'y', ui::VKEY_Y},
       {true, ui::DomKey::CHARACTER, 'Y', ui::VKEY_Y}},
      {ui::DomCode::KEY_Z,
       {true, ui::DomKey::CHARACTER, 'z', ui::VKEY_Z},
       {true, ui::DomKey::CHARACTER, 'Z', ui::VKEY_Z}},
      {ui::DomCode::DIGIT1,
       {true, ui::DomKey::CHARACTER, '1', ui::VKEY_1},
       {true, ui::DomKey::CHARACTER, '!', ui::VKEY_1}},
      {ui::DomCode::DIGIT2,
       {true, ui::DomKey::CHARACTER, '2', ui::VKEY_2},
       {true, ui::DomKey::CHARACTER, '@', ui::VKEY_2}},
      {ui::DomCode::DIGIT3,
       {true, ui::DomKey::CHARACTER, '3', ui::VKEY_3},
       {true, ui::DomKey::CHARACTER, '#', ui::VKEY_3}},
      {ui::DomCode::DIGIT4,
       {true, ui::DomKey::CHARACTER, '4', ui::VKEY_4},
       {true, ui::DomKey::CHARACTER, '$', ui::VKEY_4}},
      {ui::DomCode::DIGIT5,
       {true, ui::DomKey::CHARACTER, '5', ui::VKEY_5},
       {true, ui::DomKey::CHARACTER, '%', ui::VKEY_5}},
      {ui::DomCode::DIGIT6,
       {true, ui::DomKey::CHARACTER, '6', ui::VKEY_6},
       {true, ui::DomKey::CHARACTER, '^', ui::VKEY_6}},
      {ui::DomCode::DIGIT7,
       {true, ui::DomKey::CHARACTER, '7', ui::VKEY_7},
       {true, ui::DomKey::CHARACTER, '&', ui::VKEY_7}},
      {ui::DomCode::DIGIT8,
       {true, ui::DomKey::CHARACTER, '8', ui::VKEY_8},
       {true, ui::DomKey::CHARACTER, '*', ui::VKEY_8}},
      {ui::DomCode::DIGIT9,
       {true, ui::DomKey::CHARACTER, '9', ui::VKEY_9},
       {true, ui::DomKey::CHARACTER, '(', ui::VKEY_9}},
      {ui::DomCode::DIGIT0,
       {true, ui::DomKey::CHARACTER, '0', ui::VKEY_0},
       {true, ui::DomKey::CHARACTER, ')', ui::VKEY_0}},
      {ui::DomCode::SPACE,
       {true, ui::DomKey::CHARACTER, ' ', ui::VKEY_SPACE},
       {true, ui::DomKey::CHARACTER, ' ', ui::VKEY_SPACE}},
      {ui::DomCode::MINUS,
       {true, ui::DomKey::CHARACTER, '-', ui::VKEY_OEM_MINUS},
       {true, ui::DomKey::CHARACTER, '_', ui::VKEY_OEM_MINUS}},
      {ui::DomCode::EQUAL,
       {true, ui::DomKey::CHARACTER, '=', ui::VKEY_OEM_PLUS},
       {true, ui::DomKey::CHARACTER, '+', ui::VKEY_OEM_PLUS}},
      {ui::DomCode::BRACKET_LEFT,
       {true, ui::DomKey::CHARACTER, '[', ui::VKEY_OEM_4},
       {true, ui::DomKey::CHARACTER, '{', ui::VKEY_OEM_4}},
      {ui::DomCode::BRACKET_RIGHT,
       {true, ui::DomKey::CHARACTER, ']', ui::VKEY_OEM_6},
       {true, ui::DomKey::CHARACTER, '}', ui::VKEY_OEM_6}},
      {ui::DomCode::BACKSLASH,
       {true, ui::DomKey::CHARACTER, '\\', ui::VKEY_OEM_5},
       {true, ui::DomKey::CHARACTER, '|', ui::VKEY_OEM_5}},
      {ui::DomCode::SEMICOLON,
       {true, ui::DomKey::CHARACTER, ';', ui::VKEY_OEM_1},
       {true, ui::DomKey::CHARACTER, ':', ui::VKEY_OEM_1}},
      {ui::DomCode::QUOTE,
       {true, ui::DomKey::CHARACTER, '\'', ui::VKEY_OEM_7},
       {true, ui::DomKey::CHARACTER, '"', ui::VKEY_OEM_7}},
      {ui::DomCode::BACKQUOTE,
       {true, ui::DomKey::CHARACTER, '`', ui::VKEY_OEM_3},
       {true, ui::DomKey::CHARACTER, '~', ui::VKEY_OEM_3}},
      {ui::DomCode::COMMA,
       {true, ui::DomKey::CHARACTER, ',', ui::VKEY_OEM_COMMA},
       {true, ui::DomKey::CHARACTER, '<', ui::VKEY_OEM_COMMA}},
      {ui::DomCode::PERIOD,
       {true, ui::DomKey::CHARACTER, '.', ui::VKEY_OEM_PERIOD},
       {true, ui::DomKey::CHARACTER, '>', ui::VKEY_OEM_PERIOD}},
      {ui::DomCode::SLASH,
       {true, ui::DomKey::CHARACTER, '/', ui::VKEY_OEM_2},
       {true, ui::DomKey::CHARACTER, '?', ui::VKEY_OEM_2}},
      {ui::DomCode::INTL_BACKSLASH,
       {true, ui::DomKey::CHARACTER, '\\', ui::VKEY_OEM_102},
       {true, ui::DomKey::CHARACTER, '|', ui::VKEY_OEM_102}},
      {ui::DomCode::INTL_YEN,
       {true, ui::DomKey::CHARACTER, 0x00A5, ui::VKEY_OEM_5},
       {true, ui::DomKey::CHARACTER, '|', ui::VKEY_OEM_5}},
      {ui::DomCode::NUMPAD_DIVIDE,
       {true, ui::DomKey::CHARACTER, '/', ui::VKEY_DIVIDE},
       {true, ui::DomKey::CHARACTER, '/', ui::VKEY_DIVIDE}},
      {ui::DomCode::NUMPAD_MULTIPLY,
       {true, ui::DomKey::CHARACTER, '*', ui::VKEY_MULTIPLY},
       {true, ui::DomKey::CHARACTER, '*', ui::VKEY_MULTIPLY}},
      {ui::DomCode::NUMPAD_SUBTRACT,
       {true, ui::DomKey::CHARACTER, '-', ui::VKEY_SUBTRACT},
       {true, ui::DomKey::CHARACTER, '-', ui::VKEY_SUBTRACT}},
      {ui::DomCode::NUMPAD_ADD,
       {true, ui::DomKey::CHARACTER, '+', ui::VKEY_ADD},
       {true, ui::DomKey::CHARACTER, '+', ui::VKEY_ADD}},
      {ui::DomCode::NUMPAD1,
       {true, ui::DomKey::CHARACTER, '1', ui::VKEY_1},
       {true, ui::DomKey::CHARACTER, '1', ui::VKEY_1}},
      {ui::DomCode::NUMPAD2,
       {true, ui::DomKey::CHARACTER, '2', ui::VKEY_2},
       {true, ui::DomKey::CHARACTER, '2', ui::VKEY_2}},
      {ui::DomCode::NUMPAD3,
       {true, ui::DomKey::CHARACTER, '3', ui::VKEY_3},
       {true, ui::DomKey::CHARACTER, '3', ui::VKEY_3}},
      {ui::DomCode::NUMPAD4,
       {true, ui::DomKey::CHARACTER, '4', ui::VKEY_4},
       {true, ui::DomKey::CHARACTER, '4', ui::VKEY_4}},
      {ui::DomCode::NUMPAD5,
       {true, ui::DomKey::CHARACTER, '5', ui::VKEY_5},
       {true, ui::DomKey::CHARACTER, '5', ui::VKEY_5}},
      {ui::DomCode::NUMPAD6,
       {true, ui::DomKey::CHARACTER, '6', ui::VKEY_6},
       {true, ui::DomKey::CHARACTER, '6', ui::VKEY_6}},
      {ui::DomCode::NUMPAD7,
       {true, ui::DomKey::CHARACTER, '7', ui::VKEY_7},
       {true, ui::DomKey::CHARACTER, '7', ui::VKEY_7}},
      {ui::DomCode::NUMPAD8,
       {true, ui::DomKey::CHARACTER, '8', ui::VKEY_8},
       {true, ui::DomKey::CHARACTER, '8', ui::VKEY_8}},
      {ui::DomCode::NUMPAD9,
       {true, ui::DomKey::CHARACTER, '9', ui::VKEY_9},
       {true, ui::DomKey::CHARACTER, '9', ui::VKEY_9}},
      {ui::DomCode::NUMPAD0,
       {true, ui::DomKey::CHARACTER, '0', ui::VKEY_0},
       {true, ui::DomKey::CHARACTER, '0', ui::VKEY_0}},
      {ui::DomCode::NUMPAD_DECIMAL,
       {true, ui::DomKey::CHARACTER, '.', ui::VKEY_DECIMAL},
       {true, ui::DomKey::CHARACTER, '.', ui::VKEY_DECIMAL}},
      {ui::DomCode::NUMPAD_EQUAL,
       {true, ui::DomKey::CHARACTER, '=', ui::VKEY_UNKNOWN},
       {true, ui::DomKey::CHARACTER, '=', ui::VKEY_UNKNOWN}},
      {ui::DomCode::NUMPAD_COMMA,
       {true, ui::DomKey::CHARACTER, ',', ui::VKEY_OEM_COMMA},
       {true, ui::DomKey::CHARACTER, ',', ui::VKEY_OEM_COMMA}},
      {ui::DomCode::NUMPAD_PAREN_LEFT,
       {true, ui::DomKey::CHARACTER, '(', ui::VKEY_UNKNOWN},
       {true, ui::DomKey::CHARACTER, '(', ui::VKEY_UNKNOWN}},
      {ui::DomCode::NUMPAD_PAREN_RIGHT,
       {true, ui::DomKey::CHARACTER, ')', ui::VKEY_UNKNOWN},
       {true, ui::DomKey::CHARACTER, ')', ui::VKEY_UNKNOWN}},
      {ui::DomCode::NUMPAD_SIGN_CHANGE,
       {true, ui::DomKey::CHARACTER, 0xB1, ui::VKEY_UNKNOWN},
       {true, ui::DomKey::CHARACTER, 0xB1, ui::VKEY_UNKNOWN}},
  };

  for (const auto& it : kPrintableUsLayout) {
    CheckDomCodeToMeaning("p_us_n", ui::DomCodeToUsLayoutMeaning, it.dom_code,
                          ui::EF_NONE, it.normal);
    CheckDomCodeToMeaning("p_us_s", ui::DomCodeToUsLayoutMeaning, it.dom_code,
                          ui::EF_SHIFT_DOWN, it.shift);
    CheckDomCodeToMeaning("p_us_a", ui::DomCodeToUsLayoutMeaning, it.dom_code,
                          ui::EF_ALTGR_DOWN, it.normal);
    CheckDomCodeToMeaning("p_us_a", ui::DomCodeToUsLayoutMeaning, it.dom_code,
                          ui::EF_ALTGR_DOWN|ui::EF_SHIFT_DOWN, it.shift);
  }
}

}  // namespace
