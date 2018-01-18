// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/commands/InsertParagraphSeparatorCommand.h"

#include "core/editing/FrameSelection.h"
#include "core/editing/SelectionTemplate.h"
#include "core/editing/testing/EditingTestBase.h"

namespace blink {

class InsertParagraphSeparatorCommandTest : public EditingTestBase {};

// http://crbug.com/777378
TEST_F(InsertParagraphSeparatorCommandTest,
       CrashWithAppearanceStyleOnEmptyColgroup) {
  Selection().SetSelectionAndEndTyping(SetSelectionTextToBody(
      "<table contenteditable>"
      "    <colgroup style='-webkit-appearance:radio;'><!--|--></colgroup>"
      "</table>"));

  InsertParagraphSeparatorCommand* command =
      InsertParagraphSeparatorCommand::Create(GetDocument());
  // Crash should not be observed here.
  command->Apply();

  EXPECT_EQ(
      "<table contenteditable>"
      "    <colgroup style=\"-webkit-appearance:radio;\">|<br></colgroup>"
      "</table>",
      GetSelectionTextFromBody(Selection().GetSelectionInDOMTree()));
}

// http://crbug.com/777378
TEST_F(InsertParagraphSeparatorCommandTest,
       CrashWithAppearanceStyleOnEmptyColumn) {
  Selection().SetSelectionAndEndTyping(
      SetSelectionTextToBody("<table contenteditable>"
                             "    <colgroup style='-webkit-appearance:radio;'>"
                             "        <col><!--|--></col>"
                             "    </colgroup>"
                             "</table>"));

  InsertParagraphSeparatorCommand* command =
      InsertParagraphSeparatorCommand::Create(GetDocument());
  // Crash should not be observed here.
  command->Apply();
  EXPECT_EQ(
      "<table contenteditable>"
      "    <colgroup style=\"-webkit-appearance:radio;\">|<br>"
      "        <col>"
      "    </colgroup>"
      "</table>",
      GetSelectionTextFromBody(Selection().GetSelectionInDOMTree()));
}

}  // namespace blink
