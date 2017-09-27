// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/commands/ReplaceSelectionCommand.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/HTMLNames.h"
#include "core/dom/Document.h"
#include "core/dom/DocumentFragment.h"
#include "core/dom/ParserContentPolicy.h"
#include "core/editing/FrameSelection.h"
#include "core/editing/Position.h"
#include "core/editing/SelectionTemplate.h"
#include "core/editing/VisibleSelection.h"
#include "core/editing/testing/EditingTestBase.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/LocalFrameView.h"
#include "core/frame/Settings.h"
#include "core/layout/api/LayoutViewItem.h"
#include "core/testing/DummyPageHolder.h"
#include "testing/gtest/include/gtest/gtest.h"

#include <memory>

namespace blink {

class ReplaceSelectionCommandTest : public EditingTestBase {};

// This is a regression test for https://crbug.com/619131
TEST_F(ReplaceSelectionCommandTest, pastingEmptySpan) {
  GetDocument().setDesignMode("on");
  SetBodyContent("foo");

  LocalFrame* frame = GetDocument().GetFrame();
  frame->Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .Collapse(Position(GetDocument().body(), 0))
          .Build());

  DocumentFragment* fragment = GetDocument().createDocumentFragment();
  fragment->AppendChild(GetDocument().createElement("span"));

  // |options| are taken from |Editor::replaceSelectionWithFragment()| with
  // |selectReplacement| and |smartReplace|.
  ReplaceSelectionCommand::CommandOptions options =
      ReplaceSelectionCommand::kPreventNesting |
      ReplaceSelectionCommand::kSanitizeFragment |
      ReplaceSelectionCommand::kSelectReplacement |
      ReplaceSelectionCommand::kSmartReplace;
  ReplaceSelectionCommand* command =
      ReplaceSelectionCommand::Create(GetDocument(), fragment, options);

  EXPECT_TRUE(command->Apply()) << "the replace command should have succeeded";
  EXPECT_EQ("foo", GetDocument().body()->InnerHTMLAsString())
      << "no DOM tree mutation";
}

// This is a regression test for https://crbug.com/668808
TEST_F(ReplaceSelectionCommandTest, pasteSpanInText) {
  GetDocument().setDesignMode("on");
  SetBodyContent("<b>text</b>");

  Element* b_element = GetDocument().QuerySelector("b");
  LocalFrame* frame = GetDocument().GetFrame();
  frame->Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .Collapse(Position(b_element->firstChild(), 1))
          .Build());

  DocumentFragment* fragment = GetDocument().createDocumentFragment();
  fragment->ParseHTML("<span><div>bar</div></span>", b_element);

  ReplaceSelectionCommand::CommandOptions options = 0;
  ReplaceSelectionCommand* command =
      ReplaceSelectionCommand::Create(GetDocument(), fragment, options);

  EXPECT_TRUE(command->Apply()) << "the replace command should have succeeded";
  EXPECT_EQ("<b>t</b>bar<b>ext</b>", GetDocument().body()->InnerHTMLAsString())
      << "'bar' should have been inserted";
}

// This is a regression test for https://crbug.com/121163
TEST_F(ReplaceSelectionCommandTest, styleTagsInPastedHeadIncludedInContent) {
  GetDocument().setDesignMode("on");
  UpdateAllLifecyclePhases();
  GetDummyPageHolder().GetFrame().Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .Collapse(Position(GetDocument().body(), 0))
          .Build());

  DocumentFragment* fragment = GetDocument().createDocumentFragment();
  fragment->ParseHTML(
      "<head><style>foo { bar: baz; }</style></head>"
      "<body><p>Text</p></body>",
      GetDocument().documentElement(), kDisallowScriptingAndPluginContent);

  ReplaceSelectionCommand::CommandOptions options = 0;
  ReplaceSelectionCommand* command =
      ReplaceSelectionCommand::Create(GetDocument(), fragment, options);
  EXPECT_TRUE(command->Apply()) << "the replace command should have succeeded";

  EXPECT_EQ(
      "<head><style>foo { bar: baz; }</style></head>"
      "<body><p>Text</p></body>",
      GetDocument().body()->InnerHTMLAsString())
      << "the STYLE and P elements should have been pasted into the body "
      << "of the document";
}

// Helper function to set autosizing multipliers on a document.
bool SetTextAutosizingMultiplier(Document* document, float multiplier) {
  bool multiplier_set = false;
  for (LayoutItem layout_item = document->GetLayoutViewItem();
       !layout_item.IsNull(); layout_item = layout_item.NextInPreOrder()) {
    if (layout_item.Style()) {
      layout_item.MutableStyleRef().SetTextAutosizingMultiplier(multiplier);

      EXPECT_EQ(multiplier, layout_item.Style()->TextAutosizingMultiplier());
      multiplier_set = true;
    }
  }
  return multiplier_set;
}

// This is a regression test for https://crbug.com/768261
TEST_F(ReplaceSelectionCommandTest, TextAutosizingDoesntInflateText) {
  GetDocument().GetSettings()->SetTextAutosizingEnabled(true);
  GetDocument().setDesignMode("on");
  SetBodyContent("<div><span style='font-size: 12px;'>foo bar</span></div>");
  SetTextAutosizingMultiplier(&GetDocument(), 2.0);

  Element* div = GetDocument().QuerySelector("div");
  Element* span = GetDocument().QuerySelector("span");

  // Select "bar".
  GetDocument().GetFrame()->Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .Collapse(Position(span->firstChild(), 4))
          .Extend(Position(span->firstChild(), 7))
          .Build());

  DocumentFragment* fragment = GetDocument().createDocumentFragment();
  fragment->ParseHTML("baz", span);

  ReplaceSelectionCommand::CommandOptions options =
      ReplaceSelectionCommand::kMatchStyle;

  ReplaceSelectionCommand* command =
      ReplaceSelectionCommand::Create(GetDocument(), fragment, options);

  EXPECT_TRUE(command->Apply()) << "the replace command should have succeeded";
  // The span element should not have been split to increase the font size.
  EXPECT_EQ(1u, div->CountChildren());
}

}  // namespace blink
