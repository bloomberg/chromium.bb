// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/commands/SetCharacterDataCommand.h"

#include "core/editing/EditingTestBase.h"
#include "core/editing/commands/EditingState.h"

namespace blink {

class SetCharacterDataCommandTest : public EditingTestBase {};

TEST_F(SetCharacterDataCommandTest, replaceTextWithSameLength) {
  setBodyContent("<div contenteditable>This is a good test case</div>");

  SimpleEditCommand* command = SetCharacterDataCommand::create(
      toText(document().body()->firstChild()->firstChild()), 10, 4, "lame");

  command->doReapply();
  EXPECT_EQ("This is a lame test case",
            toText(document().body()->firstChild()->firstChild())->wholeText());

  command->doUnapply();
  EXPECT_EQ("This is a good test case",
            toText(document().body()->firstChild()->firstChild())->wholeText());
}

TEST_F(SetCharacterDataCommandTest, replaceTextWithLongerText) {
  setBodyContent("<div contenteditable>This is a good test case</div>");

  SimpleEditCommand* command = SetCharacterDataCommand::create(
      toText(document().body()->firstChild()->firstChild()), 10, 4, "lousy");

  command->doReapply();
  EXPECT_EQ("This is a lousy test case",
            toText(document().body()->firstChild()->firstChild())->wholeText());

  command->doUnapply();
  EXPECT_EQ("This is a good test case",
            toText(document().body()->firstChild()->firstChild())->wholeText());
}

TEST_F(SetCharacterDataCommandTest, replaceTextWithShorterText) {
  setBodyContent("<div contenteditable>This is a good test case</div>");

  SimpleEditCommand* command = SetCharacterDataCommand::create(
      toText(document().body()->firstChild()->firstChild()), 10, 4, "meh");

  command->doReapply();
  EXPECT_EQ("This is a meh test case",
            toText(document().body()->firstChild()->firstChild())->wholeText());

  command->doUnapply();
  EXPECT_EQ("This is a good test case",
            toText(document().body()->firstChild()->firstChild())->wholeText());
}

TEST_F(SetCharacterDataCommandTest, insertTextIntoEmptyNode) {
  setBodyContent("<div contenteditable />");

  document().body()->firstChild()->appendChild(
      document().createEditingTextNode(""));

  SimpleEditCommand* command = SetCharacterDataCommand::create(
      toText(document().body()->firstChild()->firstChild()), 0, 0, "hello");

  command->doReapply();
  EXPECT_EQ("hello",
            toText(document().body()->firstChild()->firstChild())->wholeText());

  command->doUnapply();
  EXPECT_EQ("",
            toText(document().body()->firstChild()->firstChild())->wholeText());
}

TEST_F(SetCharacterDataCommandTest, insertTextAtEndOfNonEmptyNode) {
  setBodyContent("<div contenteditable>Hello</div>");

  SimpleEditCommand* command = SetCharacterDataCommand::create(
      toText(document().body()->firstChild()->firstChild()), 5, 0, ", world!");

  command->doReapply();
  EXPECT_EQ("Hello, world!",
            toText(document().body()->firstChild()->firstChild())->wholeText());

  command->doUnapply();
  EXPECT_EQ("Hello",
            toText(document().body()->firstChild()->firstChild())->wholeText());
}

TEST_F(SetCharacterDataCommandTest, replaceEntireNode) {
  setBodyContent("<div contenteditable>Hello</div>");

  SimpleEditCommand* command = SetCharacterDataCommand::create(
      toText(document().body()->firstChild()->firstChild()), 0, 5, "Bye");

  command->doReapply();
  EXPECT_EQ("Bye",
            toText(document().body()->firstChild()->firstChild())->wholeText());

  command->doUnapply();
  EXPECT_EQ("Hello",
            toText(document().body()->firstChild()->firstChild())->wholeText());
}

}  // namespace blink
