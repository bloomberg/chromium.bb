// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include "base/basictypes.h"
#include "views/events/event.h"

// Bug 99129.
#if defined(USE_AURA)
#define MAYBE_KeyEvent FAILS_KeyEvent
#define MAYBE_KeyEventDirectUnicode FAILS_KeyEventDirectUnicode
#else
#define MAYBE_KeyEvent KeyEvent
#define MAYBE_KeyEventDirectUnicode KeyEventDirectUnicode
#endif

namespace views {

class EventTest : public testing::Test {
 public:
  EventTest() {
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(EventTest);
};

TEST_F(EventTest, MAYBE_KeyEvent) {
  static const struct {
    ui::KeyboardCode key_code;
    int flags;
    uint16 character;
    uint16 unmodified_character;
  } kTestData[] = {
    { ui::VKEY_A, 0, 'a', 'a' },
    { ui::VKEY_A, ui::EF_SHIFT_DOWN, 'A', 'A' },
    { ui::VKEY_A, ui::EF_CAPS_LOCK_DOWN, 'A', 'a' },
    { ui::VKEY_A, ui::EF_SHIFT_DOWN | ui::EF_CAPS_LOCK_DOWN, 'a', 'A' },
    { ui::VKEY_A, ui::EF_CONTROL_DOWN, 0x01, 'a' },
    { ui::VKEY_A, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN, '\x01', 'A' },
    { ui::VKEY_Z, 0, 'z', 'z' },
    { ui::VKEY_Z, ui::EF_SHIFT_DOWN, 'Z', 'Z' },
    { ui::VKEY_Z, ui::EF_CAPS_LOCK_DOWN, 'Z', 'z' },
    { ui::VKEY_Z, ui::EF_SHIFT_DOWN | ui::EF_CAPS_LOCK_DOWN, 'z', 'Z' },
    { ui::VKEY_Z, ui::EF_CONTROL_DOWN, '\x1A', 'z' },
    { ui::VKEY_Z, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN, '\x1A', 'Z' },

    { ui::VKEY_2, ui::EF_CONTROL_DOWN, '\0', '2' },
    { ui::VKEY_2, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN, '\0', '@' },
    { ui::VKEY_6, ui::EF_CONTROL_DOWN, '\0', '6' },
    { ui::VKEY_6, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN, '\x1E', '^' },
    { ui::VKEY_OEM_MINUS, ui::EF_CONTROL_DOWN, '\0', '-' },
    { ui::VKEY_OEM_MINUS, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN, '\x1F', '_'},
    { ui::VKEY_OEM_4, ui::EF_CONTROL_DOWN, '\x1B', '[' },
    { ui::VKEY_OEM_4, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN, '\0', '{' },
    { ui::VKEY_OEM_5, ui::EF_CONTROL_DOWN, '\x1C', '\\' },
    { ui::VKEY_OEM_5, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN, '\0', '|' },
    { ui::VKEY_OEM_6, ui::EF_CONTROL_DOWN, '\x1D', ']' },
    { ui::VKEY_OEM_6, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN, '\0', '}' },
    { ui::VKEY_RETURN, ui::EF_CONTROL_DOWN, '\x0A', '\r' },

    { ui::VKEY_0, 0, '0', '0' },
    { ui::VKEY_0, ui::EF_SHIFT_DOWN, ')', ')' },
    { ui::VKEY_0, ui::EF_SHIFT_DOWN | ui::EF_CAPS_LOCK_DOWN, ')', ')' },
    { ui::VKEY_0, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN, '\0', ')' },

    { ui::VKEY_9, 0, '9', '9' },
    { ui::VKEY_9, ui::EF_SHIFT_DOWN, '(', '(' },
    { ui::VKEY_9, ui::EF_SHIFT_DOWN | ui::EF_CAPS_LOCK_DOWN, '(', '(' },
    { ui::VKEY_9, ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN, '\0', '(' },

    { ui::VKEY_NUMPAD0, ui::EF_CONTROL_DOWN, '\0', '0' },
    { ui::VKEY_NUMPAD0, ui::EF_SHIFT_DOWN, '0', '0' },

    { ui::VKEY_NUMPAD9, ui::EF_CONTROL_DOWN, '\0', '9' },
    { ui::VKEY_NUMPAD9, ui::EF_SHIFT_DOWN, '9', '9' },

    { ui::VKEY_TAB, ui::EF_CONTROL_DOWN, '\0', '\t' },
    { ui::VKEY_TAB, ui::EF_SHIFT_DOWN, '\t', '\t' },

    { ui::VKEY_MULTIPLY, ui::EF_CONTROL_DOWN, '\0', '*' },
    { ui::VKEY_MULTIPLY, ui::EF_SHIFT_DOWN, '*', '*' },
    { ui::VKEY_ADD, ui::EF_CONTROL_DOWN, '\0', '+' },
    { ui::VKEY_ADD, ui::EF_SHIFT_DOWN, '+', '+' },
    { ui::VKEY_SUBTRACT, ui::EF_CONTROL_DOWN, '\0', '-' },
    { ui::VKEY_SUBTRACT, ui::EF_SHIFT_DOWN, '-', '-' },
    { ui::VKEY_DECIMAL, ui::EF_CONTROL_DOWN, '\0', '.' },
    { ui::VKEY_DECIMAL, ui::EF_SHIFT_DOWN, '.', '.' },
    { ui::VKEY_DIVIDE, ui::EF_CONTROL_DOWN, '\0', '/' },
    { ui::VKEY_DIVIDE, ui::EF_SHIFT_DOWN, '/', '/' },

    { ui::VKEY_OEM_1, ui::EF_CONTROL_DOWN, '\0', ';' },
    { ui::VKEY_OEM_1, ui::EF_SHIFT_DOWN, ':', ':' },
    { ui::VKEY_OEM_PLUS, ui::EF_CONTROL_DOWN, '\0', '=' },
    { ui::VKEY_OEM_PLUS, ui::EF_SHIFT_DOWN, '+', '+' },
    { ui::VKEY_OEM_COMMA, ui::EF_CONTROL_DOWN, '\0', ',' },
    { ui::VKEY_OEM_COMMA, ui::EF_SHIFT_DOWN, '<', '<' },
    { ui::VKEY_OEM_PERIOD, ui::EF_CONTROL_DOWN, '\0', '.' },
    { ui::VKEY_OEM_PERIOD, ui::EF_SHIFT_DOWN, '>', '>' },
    { ui::VKEY_OEM_3, ui::EF_CONTROL_DOWN, '\0', '`' },
    { ui::VKEY_OEM_3, ui::EF_SHIFT_DOWN, '~', '~' },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kTestData); ++i) {
    KeyEvent key(ui::ET_KEY_PRESSED, kTestData[i].key_code, kTestData[i].flags);
    EXPECT_EQ(kTestData[i].character, key.GetCharacter())
        << " Index:" << i << " key_code:" << kTestData[i].key_code;
    EXPECT_EQ(kTestData[i].unmodified_character, key.GetUnmodifiedCharacter())
        << " Index:" << i << " key_code:" << kTestData[i].key_code;
  }
}

TEST_F(EventTest, MAYBE_KeyEventDirectUnicode) {
  KeyEvent key(ui::ET_KEY_PRESSED, ui::VKEY_UNKNOWN, ui::EF_SHIFT_DOWN);
  key.set_character(0x1234U);
  key.set_unmodified_character(0x4321U);
  EXPECT_EQ(0x1234U, key.GetCharacter());
  EXPECT_EQ(0x4321U, key.GetUnmodifiedCharacter());
  KeyEvent key2(ui::ET_KEY_RELEASED, ui::VKEY_UNKNOWN, ui::EF_CONTROL_DOWN);
  key2.set_character(0x4321U);
  key2.set_unmodified_character(0x1234U);
  EXPECT_EQ(0x4321U, key2.GetCharacter());
  EXPECT_EQ(0x1234U, key2.GetUnmodifiedCharacter());
}

}  // namespace views
