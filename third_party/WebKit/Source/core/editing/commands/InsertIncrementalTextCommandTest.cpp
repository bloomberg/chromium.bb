// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/commands/InsertIncrementalTextCommand.h"

#include "core/editing/EditingTestBase.h"
#include "core/editing/FrameSelection.h"

namespace blink {

class InsertIncrementalTextCommandTest : public EditingTestBase {};

// http://crbug.com/706166
TEST_F(InsertIncrementalTextCommandTest, SurrogatePairsReplace) {
  SetBodyContent("<div id=sample contenteditable><a>a</a>b&#x1F63A;</div>");
  Element* const sample = GetDocument().GetElementById("sample");
  const String new_text(Vector<UChar>{0xD83D, 0xDE38});  // U+1F638
  Selection().SetSelection(SelectionInDOMTree::Builder()
                               .Collapse(Position(sample->LastChild(), 1))
                               .Extend(Position(sample->LastChild(), 3))
                               .Build());
  CompositeEditCommand* const command =
      InsertIncrementalTextCommand::Create(GetDocument(), new_text);
  command->Apply();

  EXPECT_EQ(String(Vector<UChar>{'b', 0xD83D, 0xDE38}),
            sample->LastChild()->nodeValue())
      << "Replace 'U+D83D U+DE3A (U+1F63A) with 'U+D83D U+DE38'(U+1F638)";
}

TEST_F(InsertIncrementalTextCommandTest, SurrogatePairsNoReplace) {
  SetBodyContent("<div id=sample contenteditable><a>a</a>b&#x1F63A;</div>");
  Element* const sample = GetDocument().GetElementById("sample");
  const String new_text(Vector<UChar>{0xD83D, 0xDE3A});  // U+1F63A
  Selection().SetSelection(SelectionInDOMTree::Builder()
                               .Collapse(Position(sample->LastChild(), 1))
                               .Extend(Position(sample->LastChild(), 3))
                               .Build());
  CompositeEditCommand* const command =
      InsertIncrementalTextCommand::Create(GetDocument(), new_text);
  command->Apply();

  EXPECT_EQ(String(Vector<UChar>{'b', 0xD83D, 0xDE3A}),
            sample->LastChild()->nodeValue())
      << "Replace 'U+D83D U+DE3A(U+1F63A) with 'U+D83D U+DE3A'(U+1F63A)";
}

// http://crbug.com/706166
TEST_F(InsertIncrementalTextCommandTest, SurrogatePairsTwo) {
  SetBodyContent(
      "<div id=sample contenteditable><a>a</a>b&#x1F63A;&#x1F63A;</div>");
  Element* const sample = GetDocument().GetElementById("sample");
  const String new_text(Vector<UChar>{0xD83D, 0xDE38});  // U+1F638
  Selection().SetSelection(SelectionInDOMTree::Builder()
                               .Collapse(Position(sample->LastChild(), 1))
                               .Extend(Position(sample->LastChild(), 5))
                               .Build());
  CompositeEditCommand* const command =
      InsertIncrementalTextCommand::Create(GetDocument(), new_text);
  command->Apply();

  EXPECT_EQ(String(Vector<UChar>{'b', 0xD83D, 0xDE38}),
            sample->LastChild()->nodeValue())
      << "Replace 'U+1F63A U+1F63A with U+1F638";
}

}  // namespace blink
