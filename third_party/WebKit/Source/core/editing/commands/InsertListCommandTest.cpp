// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/commands/InsertListCommand.h"

#include "core/dom/ParentNode.h"
#include "core/dom/Text.h"
#include "core/editing/EditingTestBase.h"
#include "core/editing/FrameSelection.h"

namespace blink {

class InsertListCommandTest : public EditingTestBase {};

TEST_F(InsertListCommandTest, ShouldCleanlyRemoveSpuriousTextNode) {
  // Needs to be editable to use InsertListCommand.
  GetDocument().setDesignMode("on");
  // Set up the condition:
  // * Selection is a range, to go down into
  //   InsertListCommand::listifyParagraph.
  // * The selection needs to have a sibling list to go down into
  //   InsertListCommand::mergeWithNeighboringLists.
  // * Should be the same type (ordered list) to go into
  //   CompositeEditCommand::mergeIdenticalElements.
  // * Should have no actual children to fail the listChildNode check
  //   in InsertListCommand::doApplyForSingleParagraph.
  // * There needs to be an extra text node to trigger its removal in
  //   CompositeEditCommand::mergeIdenticalElements.
  // The removeNode is what updates document lifecycle to VisualUpdatePending
  // and makes FrameView::needsLayout return true.
  SetBodyContent("\nd\n<ol>");
  Text* empty_text = GetDocument().createTextNode("");
  GetDocument().body()->InsertBefore(empty_text,
                                     GetDocument().body()->firstChild());
  UpdateAllLifecyclePhases();
  GetDocument().GetFrame()->Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .Collapse(Position(GetDocument().body(), 0))
          .Extend(Position(GetDocument().body(), 2))
          .Build());

  InsertListCommand* command =
      InsertListCommand::Create(GetDocument(), InsertListCommand::kOrderedList);
  // This should not DCHECK.
  EXPECT_TRUE(command->Apply())
      << "The insert ordered list command should have succeeded";
  EXPECT_EQ("<ol><li>d</li></ol>", GetDocument().body()->innerHTML());
}
}
