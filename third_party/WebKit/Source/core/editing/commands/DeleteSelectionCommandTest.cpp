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
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/testing/DummyPageHolder.h"
#include "testing/gtest/include/gtest/gtest.h"

#include <memory>

namespace blink {

class DeleteSelectionCommandTest : public EditingTestBase {};

// This is a regression test for https://crbug.com/668765
TEST_F(DeleteSelectionCommandTest, deleteListFromTable) {
  setBodyContent(
      "<div contenteditable=true>"
      "<table><tr><td><ol>"
      "<li><br></li>"
      "<li>foo</li>"
      "</ol></td></tr></table>"
      "</div>");

  Element* div = document().querySelector("div");
  Element* table = document().querySelector("table");
  Element* br = document().querySelector("br");

  LocalFrame* frame = document().frame();
  frame->selection().setSelection(
      SelectionInDOMTree::Builder()
          .collapse(Position(br, PositionAnchorType::BeforeAnchor))
          .extend(Position(table, PositionAnchorType::AfterAnchor))
          .build());

  const bool kNoSmartDelete = false;
  const bool kMergeBlocksAfterDelete = true;
  const bool kNoExpandForSpecialElements = false;
  const bool kSanitizeMarkup = true;
  DeleteSelectionCommand* command = DeleteSelectionCommand::create(
      document(), kNoSmartDelete, kMergeBlocksAfterDelete,
      kNoExpandForSpecialElements, kSanitizeMarkup,
      InputEvent::InputType::DeleteByCut);

  EXPECT_TRUE(command->apply()) << "the delete command should have succeeded";
  EXPECT_EQ("<div contenteditable=\"true\"><br></div>",
            document().body()->innerHTML());
  EXPECT_TRUE(frame->selection()
                  .computeVisibleSelectionInDOMTreeDeprecated()
                  .isCaret());
  EXPECT_EQ(Position(div, 0), frame->selection()
                                  .computeVisibleSelectionInDOMTree()
                                  .base()
                                  .toOffsetInAnchor());
}

}  // namespace blink
