/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/editing/EditingBehavior.h"
#include "core/editing/Editor.h"
#include "core/events/EventTarget.h"
#include "core/events/KeyboardEvent.h"
#include "core/frame/Settings.h"
#include "platform/KeyboardCodes.h"
#include "public/platform/Platform.h"
#include "public/platform/WebInputEvent.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <memory>

namespace blink {

class KeyboardTest : public testing::Test {
 public:
  // Pass a WebKeyboardEvent into the EditorClient and get back the string
  // name of which editing event that key causes.
  // E.g., sending in the enter key gives back "InsertNewline".
  const char* interpretKeyEvent(const WebKeyboardEvent& webKeyboardEvent) {
    KeyboardEvent* keyboardEvent = KeyboardEvent::create(webKeyboardEvent, 0);
    std::unique_ptr<Settings> settings = Settings::create();
    EditingBehavior behavior(settings->getEditingBehaviorType());
    return behavior.interpretKeyEvent(*keyboardEvent);
  }

  WebKeyboardEvent createFakeKeyboardEvent(char keyCode,
                                           int modifiers,
                                           WebInputEvent::Type type,
                                           const String& key = emptyString) {
    WebKeyboardEvent event(type, modifiers, WebInputEvent::TimeStampForTesting);
    event.text[0] = keyCode;
    event.windowsKeyCode = keyCode;
    event.domKey = Platform::current()->domKeyEnumFromString(key);
    return event;
  }

  // Like interpretKeyEvent, but with pressing down OSModifier+|keyCode|.
  // OSModifier is the platform's standard modifier key: control on most
  // platforms, but meta (command) on Mac.
  const char* interpretOSModifierKeyPress(char keyCode) {
#if OS(MACOSX)
    WebInputEvent::Modifiers osModifier = WebInputEvent::MetaKey;
#else
    WebInputEvent::Modifiers osModifier = WebInputEvent::ControlKey;
#endif
    return interpretKeyEvent(createFakeKeyboardEvent(
        keyCode, osModifier, WebInputEvent::RawKeyDown));
  }

  // Like interpretKeyEvent, but with pressing down ctrl+|keyCode|.
  const char* interpretCtrlKeyPress(char keyCode) {
    return interpretKeyEvent(createFakeKeyboardEvent(
        keyCode, WebInputEvent::ControlKey, WebInputEvent::RawKeyDown));
  }

  // Like interpretKeyEvent, but with typing a tab.
  const char* interpretTab(int modifiers) {
    return interpretKeyEvent(
        createFakeKeyboardEvent('\t', modifiers, WebInputEvent::Char));
  }

  // Like interpretKeyEvent, but with typing a newline.
  const char* interpretNewLine(int modifiers) {
    return interpretKeyEvent(
        createFakeKeyboardEvent('\r', modifiers, WebInputEvent::Char));
  }

  const char* interpretDomKey(const char* key) {
    return interpretKeyEvent(createFakeKeyboardEvent(
        0, noModifiers, WebInputEvent::RawKeyDown, key));
  }

  // A name for "no modifiers set".
  static const int noModifiers = 0;
};

TEST_F(KeyboardTest, TestCtrlReturn) {
  EXPECT_STREQ("InsertNewline", interpretCtrlKeyPress(0xD));
}

TEST_F(KeyboardTest, TestOSModifierZ) {
#if !OS(MACOSX)
  EXPECT_STREQ("Undo", interpretOSModifierKeyPress('Z'));
#endif
}

TEST_F(KeyboardTest, TestOSModifierY) {
#if !OS(MACOSX)
  EXPECT_STREQ("Redo", interpretOSModifierKeyPress('Y'));
#endif
}

TEST_F(KeyboardTest, TestOSModifierA) {
#if !OS(MACOSX)
  EXPECT_STREQ("SelectAll", interpretOSModifierKeyPress('A'));
#endif
}

TEST_F(KeyboardTest, TestOSModifierX) {
#if !OS(MACOSX)
  EXPECT_STREQ("Cut", interpretOSModifierKeyPress('X'));
#endif
}

TEST_F(KeyboardTest, TestOSModifierC) {
#if !OS(MACOSX)
  EXPECT_STREQ("Copy", interpretOSModifierKeyPress('C'));
#endif
}

TEST_F(KeyboardTest, TestOSModifierV) {
#if !OS(MACOSX)
  EXPECT_STREQ("Paste", interpretOSModifierKeyPress('V'));
#endif
}

TEST_F(KeyboardTest, TestEscape) {
  const char* result = interpretKeyEvent(createFakeKeyboardEvent(
      VKEY_ESCAPE, noModifiers, WebInputEvent::RawKeyDown));
  EXPECT_STREQ("Cancel", result);
}

TEST_F(KeyboardTest, TestInsertTab) {
  EXPECT_STREQ("InsertTab", interpretTab(noModifiers));
}

TEST_F(KeyboardTest, TestInsertBackTab) {
  EXPECT_STREQ("InsertBacktab", interpretTab(WebInputEvent::ShiftKey));
}

TEST_F(KeyboardTest, TestInsertNewline) {
  EXPECT_STREQ("InsertNewline", interpretNewLine(noModifiers));
}

TEST_F(KeyboardTest, TestInsertLineBreak) {
  EXPECT_STREQ("InsertLineBreak", interpretNewLine(WebInputEvent::ShiftKey));
}

TEST_F(KeyboardTest, TestDomKeyMap) {
  struct TestCase {
    const char* key;
    const char* command;
  } kDomKeyTestCases[] = {
      {"Copy", "Copy"}, {"Cut", "Cut"}, {"Paste", "Paste"},
  };

  for (const auto& test_case : kDomKeyTestCases)
    EXPECT_STREQ(test_case.command, interpretDomKey(test_case.key));
}

}  // namespace blink
