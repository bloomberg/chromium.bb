// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/commands/delete_selection_command.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/position.h"
#include "third_party/blink/renderer/core/editing/selection_template.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"
#include "third_party/blink/renderer/core/editing/visible_selection.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

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
          .Build(),
      SetSelectionOptions());

  DeleteSelectionCommand* command =
      MakeGarbageCollected<DeleteSelectionCommand>(
          GetDocument(),
          DeleteSelectionOptions::Builder()
              .SetMergeBlocksAfterDelete(true)
              .SetSanitizeMarkup(true)
              .Build(),
          InputEvent::InputType::kDeleteByCut);

  EXPECT_TRUE(command->Apply()) << "the delete command should have succeeded";
  EXPECT_EQ("<div contenteditable=\"true\"><br></div>",
            GetDocument().body()->InnerHTMLAsString());
  EXPECT_TRUE(frame->Selection().GetSelectionInDOMTree().IsCaret());
  EXPECT_EQ(Position(div, 0), frame->Selection()
                                  .ComputeVisibleSelectionInDOMTree()
                                  .Base()
                                  .ToOffsetInAnchor());
}

TEST_F(DeleteSelectionCommandTest, ForwardDeleteWithFirstLetter) {
  InsertStyleElement("p::first-letter {font-size:200%;}");
  Selection().SetSelection(
      SetSelectionTextToBody("<p contenteditable>a^b|c</p>"),
      SetSelectionOptions());

  DeleteSelectionCommand& command =
      *MakeGarbageCollected<DeleteSelectionCommand>(
          GetDocument(), DeleteSelectionOptions::Builder()
                             .SetMergeBlocksAfterDelete(true)
                             .SetSanitizeMarkup(true)
                             .Build());
  EXPECT_TRUE(command.Apply()) << "the delete command should have succeeded";
  EXPECT_EQ("<p contenteditable>a|c</p>", GetSelectionTextFromBody());
}

}  // namespace blink
