// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/commands/SetCharacterDataCommand.h"

#include "core/editing/EditingTestBase.h"
#include "core/editing/commands/EditingState.h"

namespace blink {

class SetCharacterDataCommandTest : public EditingTestBase {};

TEST_F(SetCharacterDataCommandTest, replaceTextWithSameLength) {
  SetBodyContent("<div contenteditable>This is a good test case</div>");

  SimpleEditCommand* command = SetCharacterDataCommand::Create(
      ToText(GetDocument().body()->FirstChild()->firstChild()), 10, 4, "lame");

  command->DoReapply();
  EXPECT_EQ(
      "This is a lame test case",
      ToText(GetDocument().body()->FirstChild()->firstChild())->wholeText());

  command->DoUnapply();
  EXPECT_EQ(
      "This is a good test case",
      ToText(GetDocument().body()->FirstChild()->firstChild())->wholeText());
}

TEST_F(SetCharacterDataCommandTest, replaceTextWithLongerText) {
  SetBodyContent("<div contenteditable>This is a good test case</div>");

  SimpleEditCommand* command = SetCharacterDataCommand::Create(
      ToText(GetDocument().body()->FirstChild()->firstChild()), 10, 4, "lousy");

  command->DoReapply();
  EXPECT_EQ(
      "This is a lousy test case",
      ToText(GetDocument().body()->FirstChild()->firstChild())->wholeText());

  command->DoUnapply();
  EXPECT_EQ(
      "This is a good test case",
      ToText(GetDocument().body()->FirstChild()->firstChild())->wholeText());
}

TEST_F(SetCharacterDataCommandTest, replaceTextWithShorterText) {
  SetBodyContent("<div contenteditable>This is a good test case</div>");

  SimpleEditCommand* command = SetCharacterDataCommand::Create(
      ToText(GetDocument().body()->FirstChild()->firstChild()), 10, 4, "meh");

  command->DoReapply();
  EXPECT_EQ(
      "This is a meh test case",
      ToText(GetDocument().body()->FirstChild()->firstChild())->wholeText());

  command->DoUnapply();
  EXPECT_EQ(
      "This is a good test case",
      ToText(GetDocument().body()->FirstChild()->firstChild())->wholeText());
}

TEST_F(SetCharacterDataCommandTest, insertTextIntoEmptyNode) {
  SetBodyContent("<div contenteditable />");

  GetDocument().body()->FirstChild()->appendChild(
      GetDocument().CreateEditingTextNode(""));

  SimpleEditCommand* command = SetCharacterDataCommand::Create(
      ToText(GetDocument().body()->FirstChild()->firstChild()), 0, 0, "hello");

  command->DoReapply();
  EXPECT_EQ(
      "hello",
      ToText(GetDocument().body()->FirstChild()->firstChild())->wholeText());

  command->DoUnapply();
  EXPECT_EQ(
      "",
      ToText(GetDocument().body()->FirstChild()->firstChild())->wholeText());
}

TEST_F(SetCharacterDataCommandTest, insertTextAtEndOfNonEmptyNode) {
  SetBodyContent("<div contenteditable>Hello</div>");

  SimpleEditCommand* command = SetCharacterDataCommand::Create(
      ToText(GetDocument().body()->FirstChild()->firstChild()), 5, 0,
      ", world!");

  command->DoReapply();
  EXPECT_EQ(
      "Hello, world!",
      ToText(GetDocument().body()->FirstChild()->firstChild())->wholeText());

  command->DoUnapply();
  EXPECT_EQ(
      "Hello",
      ToText(GetDocument().body()->FirstChild()->firstChild())->wholeText());
}

TEST_F(SetCharacterDataCommandTest, replaceEntireNode) {
  SetBodyContent("<div contenteditable>Hello</div>");

  SimpleEditCommand* command = SetCharacterDataCommand::Create(
      ToText(GetDocument().body()->FirstChild()->firstChild()), 0, 5, "Bye");

  command->DoReapply();
  EXPECT_EQ(
      "Bye",
      ToText(GetDocument().body()->FirstChild()->firstChild())->wholeText());

  command->DoUnapply();
  EXPECT_EQ(
      "Hello",
      ToText(GetDocument().body()->FirstChild()->firstChild())->wholeText());
}

}  // namespace blink
