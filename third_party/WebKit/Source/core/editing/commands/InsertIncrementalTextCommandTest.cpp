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
  setBodyContent("<div id=sample contenteditable><a>a</a>b&#x1F63A;</div>");
  Element* const sample = document().getElementById("sample");
  const String newText(Vector<UChar>{0xD83D, 0xDE38});  // U+1F638
  selection().setSelection(SelectionInDOMTree::Builder()
                               .collapse(Position(sample->lastChild(), 1))
                               .extend(Position(sample->lastChild(), 3))
                               .build());
  CompositeEditCommand* const command =
      InsertIncrementalTextCommand::create(document(), newText);
  command->apply();

  EXPECT_EQ(String(Vector<UChar>{'b', 0xD83D, 0xDE38}),
            sample->lastChild()->nodeValue())
      << "Replace 'U+D83D U+DE3A (U+1F63A) with 'U+D83D U+DE38'(U+1F638)";
}

TEST_F(InsertIncrementalTextCommandTest, SurrogatePairsNoReplace) {
  setBodyContent("<div id=sample contenteditable><a>a</a>b&#x1F63A;</div>");
  Element* const sample = document().getElementById("sample");
  const String newText(Vector<UChar>{0xD83D, 0xDE3A});  // U+1F63A
  selection().setSelection(SelectionInDOMTree::Builder()
                               .collapse(Position(sample->lastChild(), 1))
                               .extend(Position(sample->lastChild(), 3))
                               .build());
  CompositeEditCommand* const command =
      InsertIncrementalTextCommand::create(document(), newText);
  command->apply();

  EXPECT_EQ(String(Vector<UChar>{'b', 0xD83D, 0xDE3A}),
            sample->lastChild()->nodeValue())
      << "Replace 'U+D83D U+DE3A(U+1F63A) with 'U+D83D U+DE3A'(U+1F63A)";
}

// http://crbug.com/706166
TEST_F(InsertIncrementalTextCommandTest, SurrogatePairsTwo) {
  setBodyContent(
      "<div id=sample contenteditable><a>a</a>b&#x1F63A;&#x1F63A;</div>");
  Element* const sample = document().getElementById("sample");
  const String newText(Vector<UChar>{0xD83D, 0xDE38});  // U+1F638
  selection().setSelection(SelectionInDOMTree::Builder()
                               .collapse(Position(sample->lastChild(), 1))
                               .extend(Position(sample->lastChild(), 5))
                               .build());
  CompositeEditCommand* const command =
      InsertIncrementalTextCommand::create(document(), newText);
  command->apply();

  EXPECT_EQ(String(Vector<UChar>{'b', 0xD83D, 0xDE38}),
            sample->lastChild()->nodeValue())
      << "Replace 'U+1F63A U+1F63A with U+1F638";
}

}  // namespace blink
