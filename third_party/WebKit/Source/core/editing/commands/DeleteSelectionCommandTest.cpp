// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/commands/DeleteSelectionCommand.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/Document.h"
#include "core/editing/EditingTestBase.h"
#include "core/editing/FrameSelection.h"
#include "core/editing/Position.h"
#include "core/editing/VisibleSelection.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/LocalFrameView.h"
#include "core/testing/DummyPageHolder.h"
#include "testing/gtest/include/gtest/gtest.h"

#include <memory>

namespace blink {

class DeleteSelectionCommandTest : public EditingTestBase {};

// This is a regression test for https://crbug.com/668765
TEST_F(DeleteSelectionCommandTest, deleteListFromTable) {
  SetBodyContent(
      "<div contenteditable=true>"
      "<table><tr><td><ol>"
      "<li><br></li>"
      "<li>foo</li>"
      "</ol></td></tr></table>"
      "</div>");

  Element* div = GetDocument().QuerySelector("div");
  Element* table = GetDocument().QuerySelector("table");
  Element* br = GetDocument().QuerySelector("br");

  LocalFrame* frame = GetDocument().GetFrame();
  frame->Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .Collapse(Position(br, PositionAnchorType::kBeforeAnchor))
          .Extend(Position(table, PositionAnchorType::kAfterAnchor))
          .Build());

  const bool kNoSmartDelete = false;
  const bool kMergeBlocksAfterDelete = true;
  const bool kNoExpandForSpecialElements = false;
  const bool kSanitizeMarkup = true;
  DeleteSelectionCommand* command = DeleteSelectionCommand::Create(
      GetDocument(), kNoSmartDelete, kMergeBlocksAfterDelete,
      kNoExpandForSpecialElements, kSanitizeMarkup,
      InputEvent::InputType::kDeleteByCut);

  EXPECT_TRUE(command->Apply()) << "the delete command should have succeeded";
  EXPECT_EQ("<div contenteditable=\"true\"><br></div>",
            GetDocument().body()->innerHTML());
  EXPECT_TRUE(frame->Selection().GetSelectionInDOMTree().IsCaret());
  EXPECT_EQ(Position(div, 0), frame->Selection()
                                  .ComputeVisibleSelectionInDOMTree()
                                  .Base()
                                  .ToOffsetInAnchor());
}

}  // namespace blink
