// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/commands/ReplaceSelectionCommand.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/HTMLNames.h"
#include "core/dom/Document.h"
#include "core/dom/DocumentFragment.h"
#include "core/dom/ParserContentPolicy.h"
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
  EXPECT_EQ("foo", GetDocument().body()->innerHTML()) << "no DOM tree mutation";
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
  EXPECT_EQ("<b>t</b>bar<b>ext</b>", GetDocument().body()->innerHTML())
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
      GetDocument().body()->innerHTML())
      << "the STYLE and P elements should have been pasted into the body "
      << "of the document";
}

}  // namespace blink
