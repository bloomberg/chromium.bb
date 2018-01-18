// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/commands/InsertListCommand.h"

#include "core/dom/ParentNode.h"
#include "core/dom/Text.h"
#include "core/editing/FrameSelection.h"
#include "core/editing/SelectionTemplate.h"
#include "core/editing/testing/EditingTestBase.h"

namespace blink {

class InsertListCommandTest : public EditingTestBase {};

TEST_F(InsertListCommandTest, ShouldCleanlyRemoveSpuriousTextNode) {
  GetDocument().SetCompatibilityMode(Document::kQuirksMode);
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
  GetDocument().GetFrame()->Selection().SetSelectionAndEndTyping(
      SelectionInDOMTree::Builder()
          .Collapse(Position(GetDocument().body(), 0))
          .Extend(Position(GetDocument().body(), 2))
          .Build());

  InsertListCommand* command =
      InsertListCommand::Create(GetDocument(), InsertListCommand::kOrderedList);
  // This should not DCHECK.
  EXPECT_TRUE(command->Apply())
      << "The insert ordered list command should have succeeded";
  EXPECT_EQ("<ol><li>d</li></ol>", GetDocument().body()->InnerHTMLAsString());
}

// Refer https://crbug.com/794356
TEST_F(InsertListCommandTest, UnlistifyParagraphCrashOnVisuallyEmptyParagraph) {
  GetDocument().setDesignMode("on");
  Selection().SetSelectionAndEndTyping(
      SetSelectionTextToBody("^<dl>"
                             "<textarea style='float:left;'></textarea>"
                             "</dl>|"));
  InsertListCommand* command = InsertListCommand::Create(
      GetDocument(), InsertListCommand::kUnorderedList);
  // Crash happens here.
  EXPECT_FALSE(command->Apply());
  EXPECT_EQ(
      "<dl><ul>"
      "|<textarea style=\"float:left;\"></textarea>"
      "</ul></dl>",
      GetSelectionTextFromBody(Selection().GetSelectionInDOMTree()));
}

// Refer https://crbug.com/798176
TEST_F(InsertListCommandTest, CleanupNodeSameAsDestinationNode) {
  GetDocument().setDesignMode("on");
  InsertStyleElement(
      "* { -webkit-appearance:checkbox; }"
      "br { visibility:hidden; }"
      "colgroup { -webkit-column-count:2; }");
  Selection().SetSelectionAndEndTyping(
      SetSelectionTextToBody("^<table><col></table>"
                             "<button></button>|"));

  InsertListCommand* command = InsertListCommand::Create(
      GetDocument(), InsertListCommand::kUnorderedList);

  // Crash happens here.
  EXPECT_FALSE(command->Apply());
  EXPECT_EQ(
      "<ul><li><br></li></ul>"
      "<br>"
      "<table>|<colgroup><col>"
      "<ul><li><br></li></ul>"
      "</col></colgroup></table>"
      "<button></button>",
      GetSelectionTextFromBody(Selection().GetSelectionInDOMTree()));
}

TEST_F(InsertListCommandTest, InsertListOnEmptyHiddenElements) {
  GetDocument().setDesignMode("on");
  InsertStyleElement("br { visibility:hidden; }");
  Selection().SetSelectionAndEndTyping(
      SetSelectionTextToBody("^<button></button>|"));
  InsertListCommand* command = InsertListCommand::Create(
      GetDocument(), InsertListCommand::kUnorderedList);

  // Crash happens here.
  EXPECT_FALSE(command->Apply());
  EXPECT_EQ(
      "<button>"
      "|<ul><li><br></li></ul>"
      "</button>",
      GetSelectionTextFromBody(Selection().GetSelectionInDOMTree()));
}

// Refer https://crbug.com/797520
TEST_F(InsertListCommandTest, InsertListWithCollapsedVisibility) {
  GetDocument().setDesignMode("on");
  InsertStyleElement(
      "ul { visibility:collapse; }"
      "dl { visibility:visible; }");

  Selection().SetSelectionAndEndTyping(SetSelectionTextToBody("^<dl>a</dl>|"));
  InsertListCommand* command =
      InsertListCommand::Create(GetDocument(), InsertListCommand::kOrderedList);

  // Crash happens here.
  EXPECT_FALSE(command->Apply());
  EXPECT_EQ(
      "<dl>"
      "<ol></ol><ul>^a|</ul>"
      "</dl>",
      GetSelectionTextFromBody(Selection().GetSelectionInDOMTree()));
}
}
