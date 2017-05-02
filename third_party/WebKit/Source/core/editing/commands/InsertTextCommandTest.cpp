// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/commands/InsertTextCommand.h"

#include "core/editing/EditingTestBase.h"
#include "core/editing/FrameSelection.h"

namespace blink {

class InsertTextCommandTest : public EditingTestBase {};

// http://crbug.com/714311
TEST_F(InsertTextCommandTest, WithTypingStyle) {
  SetBodyContent("<div contenteditable=true><option id=sample></option></div>");
  Element* const sample = GetDocument().getElementById("sample");
  Selection().SetSelection(
      SelectionInDOMTree::Builder().Collapse(Position(sample, 0)).Build());
  // Register typing style to make |InsertTextCommand| to attempt to apply
  // style to inserted text.
  GetDocument().execCommand("fontSizeDelta", false, "+3", ASSERT_NO_EXCEPTION);
  CompositeEditCommand* const command =
      InsertTextCommand::Create(GetDocument(), "x");
  command->Apply();

  EXPECT_EQ(
      "<div contenteditable=\"true\"><option id=\"sample\">x</option></div>",
      GetDocument().body()->innerHTML())
      << "Content of OPTION is distributed into shadow node as text"
         "without applying typing style.";
}

}  // namespace blink
