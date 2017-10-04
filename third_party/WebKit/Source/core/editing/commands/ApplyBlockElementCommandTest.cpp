// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/Document.h"
#include "core/dom/QualifiedName.h"
#include "core/editing/FrameSelection.h"
#include "core/editing/Position.h"
#include "core/editing/SelectionTemplate.h"
#include "core/editing/VisibleSelection.h"
#include "core/editing/commands/FormatBlockCommand.h"
#include "core/editing/commands/IndentOutdentCommand.h"
#include "core/editing/testing/EditingTestBase.h"
#include "core/html/HTMLHeadElement.h"
#include "core/html_names.h"

#include <memory>

namespace blink {

class ApplyBlockElementCommandTest : public EditingTestBase {};

// This is a regression test for https://crbug.com/639534
TEST_F(ApplyBlockElementCommandTest, selectionCrossingOverBody) {
  GetDocument().head()->insertAdjacentHTML(
      "afterbegin",
      "<style> .CLASS13 { -webkit-user-modify: read-write; }</style></head>",
      ASSERT_NO_EXCEPTION);
  GetDocument().body()->insertAdjacentHTML(
      "afterbegin",
      "\n<pre><var id='va' class='CLASS13'>\nC\n</var></pre><input />",
      ASSERT_NO_EXCEPTION);
  GetDocument().body()->insertAdjacentText("beforebegin", "foo",
                                           ASSERT_NO_EXCEPTION);

  GetDocument().body()->setContentEditable("false", ASSERT_NO_EXCEPTION);
  GetDocument().setDesignMode("on");
  GetDocument().UpdateStyleAndLayoutIgnorePendingStylesheets();
  Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(
              Position(GetDocument().documentElement(), 1),
              Position(GetDocument().getElementById("va")->firstChild(), 2))
          .Build());

  FormatBlockCommand* command =
      FormatBlockCommand::Create(GetDocument(), HTMLNames::footerTag);
  command->Apply();

  EXPECT_EQ(
      "<body contenteditable=\"false\">\n"
      "<pre><var id=\"va\" class=\"CLASS13\">\nC\n</var></pre><input></body>",
      GetDocument().documentElement()->InnerHTMLAsString());
}

// This is a regression test for https://crbug.com/660801
TEST_F(ApplyBlockElementCommandTest, visibilityChangeDuringCommand) {
  GetDocument().head()->insertAdjacentHTML(
      "afterbegin", "<style>li:first-child { visibility:visible; }</style>",
      ASSERT_NO_EXCEPTION);
  SetBodyContent("<ul style='visibility:hidden'><li>xyz</li></ul>");
  GetDocument().setDesignMode("on");

  UpdateAllLifecyclePhases();
  Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .Collapse(Position(GetDocument().QuerySelector("li"), 0))
          .Build());

  IndentOutdentCommand* command = IndentOutdentCommand::Create(
      GetDocument(), IndentOutdentCommand::kIndent);
  command->Apply();

  EXPECT_EQ(
      "<head><style>li:first-child { visibility:visible; }</style></head>"
      "<body><ul style=\"visibility:hidden\"><ul></ul><li>xyz</li></ul></body>",
      GetDocument().documentElement()->InnerHTMLAsString());
}

// This is a regression test for https://crbug.com/712510
TEST_F(ApplyBlockElementCommandTest, IndentHeadingIntoBlockquote) {
  SetBodyContent(
      "<div contenteditable=\"true\">"
      "<h6><button><table></table></button></h6>"
      "<object></object>"
      "</div>");
  Element* button = GetDocument().QuerySelector("button");
  Element* object = GetDocument().QuerySelector("object");
  Selection().SetSelection(SelectionInDOMTree::Builder()
                               .Collapse(Position(button, 0))
                               .Extend(Position(object, 0))
                               .Build());

  IndentOutdentCommand* command = IndentOutdentCommand::Create(
      GetDocument(), IndentOutdentCommand::kIndent);
  command->Apply();

  // This only records the current behavior, which can be wrong.
  EXPECT_EQ(
      "<div contenteditable=\"true\">"
      "<blockquote style=\"margin: 0 0 0 40px; border: none; padding: 0px;\">"
      "<h6><button></button></h6>"
      "<h6><button><table></table></button></h6>"
      "</blockquote>"
      "<h6><button></button></h6><br>"
      "<object></object>"
      "</div>",
      GetDocument().body()->InnerHTMLAsString());
}

}  // namespace blink
