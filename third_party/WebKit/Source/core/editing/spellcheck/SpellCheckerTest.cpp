// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/spellcheck/SpellChecker.h"

#include "core/editing/Editor.h"
#include "core/editing/markers/DocumentMarkerController.h"
#include "core/editing/markers/SpellCheckMarker.h"
#include "core/editing/spellcheck/SpellCheckRequester.h"
#include "core/editing/spellcheck/SpellCheckTestBase.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/LocalFrameView.h"
#include "core/frame/Settings.h"
#include "core/html/HTMLInputElement.h"

namespace blink {

class SpellCheckerTest : public SpellCheckTestBase {
 protected:
  int LayoutCount() const { return Page().GetFrameView().LayoutCount(); }
  DummyPageHolder& Page() const { return GetDummyPageHolder(); }

  void ForceLayout();
};

void SpellCheckerTest::ForceLayout() {
  LocalFrameView& frame_view = Page().GetFrameView();
  IntRect frame_rect = frame_view.FrameRect();
  frame_rect.SetWidth(frame_rect.Width() + 1);
  frame_rect.SetHeight(frame_rect.Height() + 1);
  Page().GetFrameView().SetFrameRect(frame_rect);
  GetDocument().UpdateStyleAndLayoutIgnorePendingStylesheets();
}

TEST_F(SpellCheckerTest, AdvanceToNextMisspellingWithEmptyInputNoCrash) {
  SetBodyContent("<input placeholder='placeholder'>abc");
  UpdateAllLifecyclePhases();
  Element* input = GetDocument().QuerySelector("input");
  input->focus();
  // Do not crash in advanceToNextMisspelling.
  GetSpellChecker().AdvanceToNextMisspelling(false);
}

// Regression test for crbug.com/701309
TEST_F(SpellCheckerTest, AdvanceToNextMisspellingWithImageInTableNoCrash) {
  SetBodyContent(
      "<div contenteditable>"
      "<table><tr><td>"
      "<img src=foo.jpg>"
      "</td></tr></table>"
      "zz zz zz"
      "</div>");
  GetDocument().QuerySelector("div")->focus();
  UpdateAllLifecyclePhases();

  // Do not crash in advanceToNextMisspelling.
  GetSpellChecker().AdvanceToNextMisspelling(false);
}

// Regression test for crbug.com/728801
TEST_F(SpellCheckerTest, AdvancedToNextMisspellingWrapSearchNoCrash) {
  SetBodyContent("<div contenteditable>  zz zz zz  </div>");

  Element* div = GetDocument().QuerySelector("div");
  div->focus();
  Selection().SetSelection(SelectionInDOMTree::Builder()
                               .Collapse(Position::LastPositionInNode(*div))
                               .Build());
  UpdateAllLifecyclePhases();

  GetSpellChecker().AdvanceToNextMisspelling(false);
}

TEST_F(SpellCheckerTest, SpellCheckDoesNotCauseUpdateLayout) {
  SetBodyContent("<input>");
  HTMLInputElement* input =
      toHTMLInputElement(GetDocument().QuerySelector("input"));
  input->focus();
  input->setValue("Hello, input field");
  GetDocument().UpdateStyleAndLayout();
  VisibleSelection old_selection =
      GetDocument()
          .GetFrame()
          ->Selection()
          .ComputeVisibleSelectionInDOMTreeDeprecated();

  Position new_position(input->InnerEditorElement()->firstChild(), 3);
  GetDocument().GetFrame()->Selection().SetSelection(
      SelectionInDOMTree::Builder().Collapse(new_position).Build());
  ASSERT_EQ(3u, input->selectionStart());

  EXPECT_TRUE(GetSpellChecker().IsSpellCheckingEnabled());
  ForceLayout();
  int start_count = LayoutCount();
  GetSpellChecker().RespondToChangedSelection(old_selection.Start(),
                                              TypingContinuation::kEnd);
  EXPECT_EQ(start_count, LayoutCount());
}

TEST_F(SpellCheckerTest, MarkAndReplaceForHandlesMultipleReplacements) {
  SetBodyContent(
      "<div contenteditable>"
      "spllchck"
      "</div>");
  Element* div = GetDocument().QuerySelector("div");
  Node* text = div->firstChild();
  EphemeralRange range_to_check =
      EphemeralRange(Position(text, 0), Position(text, 8));

  SpellCheckRequest* request = SpellCheckRequest::Create(range_to_check, 0);

  TextCheckingResult result;
  result.decoration = TextDecorationType::kTextDecorationTypeSpelling;
  result.location = 0;
  result.length = 8;
  result.replacements = Vector<String>({"spellcheck", "spillchuck"});

  GetDocument().GetFrame()->GetSpellChecker().MarkAndReplaceFor(
      request, Vector<TextCheckingResult>({result}));

  ASSERT_EQ(1u, GetDocument().Markers().Markers().size());

  // The Spelling marker's description should be a newline-separated list of the
  // suggested replacements
  EXPECT_EQ(
      "spellcheck\nspillchuck",
      ToSpellCheckMarker(GetDocument().Markers().Markers()[0])->Description());
}

TEST_F(SpellCheckerTest,
       GetSpellCheckMarkerTouchingSelection_FirstCharSelected) {
  SetBodyContent(
      "<div contenteditable>"
      "spllchck"
      "</div>");
  Element* div = GetDocument().QuerySelector("div");
  Node* text = div->firstChild();

  GetDocument().Markers().AddSpellingMarker(
      EphemeralRange(Position(text, 0), Position(text, 8)));

  GetDocument().GetFrame()->Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(Position(text, 0), Position(text, 1))
          .Build());

  Optional<std::pair<Node*, SpellCheckMarker*>> result =
      GetDocument()
          .GetFrame()
          ->GetSpellChecker()
          .GetSpellCheckMarkerTouchingSelection();
  EXPECT_TRUE(result);
}

TEST_F(SpellCheckerTest,
       GetSpellCheckMarkerTouchingSelection_LastCharSelected) {
  SetBodyContent(
      "<div contenteditable>"
      "spllchck"
      "</div>");
  Element* div = GetDocument().QuerySelector("div");
  Node* text = div->firstChild();

  GetDocument().Markers().AddSpellingMarker(
      EphemeralRange(Position(text, 0), Position(text, 8)));

  GetDocument().GetFrame()->Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(Position(text, 7), Position(text, 8))
          .Build());

  Optional<std::pair<Node*, SpellCheckMarker*>> result =
      GetDocument()
          .GetFrame()
          ->GetSpellChecker()
          .GetSpellCheckMarkerTouchingSelection();
  EXPECT_TRUE(result);
}

TEST_F(SpellCheckerTest,
       GetSpellCheckMarkerTouchingSelection_SingleCharWordSelected) {
  SetBodyContent(
      "<div contenteditable>"
      "s"
      "</div>");
  Element* div = GetDocument().QuerySelector("div");
  Node* text = div->firstChild();

  GetDocument().Markers().AddSpellingMarker(
      EphemeralRange(Position(text, 0), Position(text, 1)));

  GetDocument().GetFrame()->Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(Position(text, 0), Position(text, 1))
          .Build());

  Optional<std::pair<Node*, SpellCheckMarker*>> result =
      GetDocument()
          .GetFrame()
          ->GetSpellChecker()
          .GetSpellCheckMarkerTouchingSelection();
  EXPECT_TRUE(result);
}

TEST_F(SpellCheckerTest,
       GetSpellCheckMarkerTouchingSelection_CaretLeftOfSingleCharWord) {
  SetBodyContent(
      "<div contenteditable>"
      "s"
      "</div>");
  Element* div = GetDocument().QuerySelector("div");
  Node* text = div->firstChild();

  GetDocument().Markers().AddSpellingMarker(
      EphemeralRange(Position(text, 0), Position(text, 1)));

  GetDocument().GetFrame()->Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(Position(text, 0), Position(text, 0))
          .Build());

  Optional<std::pair<Node*, SpellCheckMarker*>> result =
      GetDocument()
          .GetFrame()
          ->GetSpellChecker()
          .GetSpellCheckMarkerTouchingSelection();
  EXPECT_TRUE(result);
}

TEST_F(SpellCheckerTest,
       GetSpellCheckMarkerTouchingSelection_CaretRightOfSingleCharWord) {
  SetBodyContent(
      "<div contenteditable>"
      "s"
      "</div>");
  Element* div = GetDocument().QuerySelector("div");
  Node* text = div->firstChild();

  GetDocument().Markers().AddSpellingMarker(
      EphemeralRange(Position(text, 0), Position(text, 1)));

  GetDocument().GetFrame()->Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(Position(text, 1), Position(text, 1))
          .Build());

  Optional<std::pair<Node*, SpellCheckMarker*>> result =
      GetDocument()
          .GetFrame()
          ->GetSpellChecker()
          .GetSpellCheckMarkerTouchingSelection();
  EXPECT_TRUE(result);
}

TEST_F(SpellCheckerTest,
       GetSpellCheckMarkerTouchingSelection_CaretLeftOfMultiCharWord) {
  SetBodyContent(
      "<div contenteditable>"
      "spllchck"
      "</div>");
  Element* div = GetDocument().QuerySelector("div");
  Node* text = div->firstChild();

  GetDocument().Markers().AddSpellingMarker(
      EphemeralRange(Position(text, 0), Position(text, 8)));

  GetDocument().GetFrame()->Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(Position(text, 0), Position(text, 0))
          .Build());

  Optional<std::pair<Node*, SpellCheckMarker*>> result =
      GetDocument()
          .GetFrame()
          ->GetSpellChecker()
          .GetSpellCheckMarkerTouchingSelection();
  EXPECT_TRUE(result);
}

TEST_F(SpellCheckerTest,
       GetSpellCheckMarkerTouchingSelection_CaretRightOfMultiCharWord) {
  SetBodyContent(
      "<div contenteditable>"
      "spllchck"
      "</div>");
  Element* div = GetDocument().QuerySelector("div");
  Node* text = div->firstChild();

  GetDocument().Markers().AddSpellingMarker(
      EphemeralRange(Position(text, 0), Position(text, 8)));

  GetDocument().GetFrame()->Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(Position(text, 8), Position(text, 8))
          .Build());

  Optional<std::pair<Node*, SpellCheckMarker*>> result =
      GetDocument()
          .GetFrame()
          ->GetSpellChecker()
          .GetSpellCheckMarkerTouchingSelection();
  EXPECT_TRUE(result);
}

TEST_F(SpellCheckerTest,
       GetSpellCheckMarkerTouchingSelection_CaretMiddleOfWord) {
  SetBodyContent(
      "<div contenteditable>"
      "spllchck"
      "</div>");
  Element* div = GetDocument().QuerySelector("div");
  Node* text = div->firstChild();

  GetDocument().Markers().AddSpellingMarker(
      EphemeralRange(Position(text, 0), Position(text, 8)));

  GetDocument().GetFrame()->Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(Position(text, 4), Position(text, 4))
          .Build());

  Optional<std::pair<Node*, SpellCheckMarker*>> result =
      GetDocument()
          .GetFrame()
          ->GetSpellChecker()
          .GetSpellCheckMarkerTouchingSelection();
  EXPECT_TRUE(result);
}

TEST_F(SpellCheckerTest,
       GetSpellCheckMarkerTouchingSelection_CaretOneCharLeftOfMisspelling) {
  SetBodyContent(
      "<div contenteditable>"
      "a spllchck"
      "</div>");
  Element* div = GetDocument().QuerySelector("div");
  Node* text = div->firstChild();

  GetDocument().Markers().AddSpellingMarker(
      EphemeralRange(Position(text, 2), Position(text, 10)));

  GetDocument().GetFrame()->Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(Position(text, 1), Position(text, 1))
          .Build());

  Optional<std::pair<Node*, SpellCheckMarker*>> result =
      GetDocument()
          .GetFrame()
          ->GetSpellChecker()
          .GetSpellCheckMarkerTouchingSelection();
  EXPECT_FALSE(result);
}

TEST_F(SpellCheckerTest,
       GetSpellCheckMarkerTouchingSelection_CaretOneCharRightOfMisspelling) {
  SetBodyContent(
      "<div contenteditable>"
      "spllchck a"
      "</div>");
  Element* div = GetDocument().QuerySelector("div");
  Node* text = div->firstChild();

  GetDocument().Markers().AddSpellingMarker(
      EphemeralRange(Position(text, 0), Position(text, 8)));

  GetDocument().GetFrame()->Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(Position(text, 9), Position(text, 9))
          .Build());

  Optional<std::pair<Node*, SpellCheckMarker*>> result =
      GetDocument()
          .GetFrame()
          ->GetSpellChecker()
          .GetSpellCheckMarkerTouchingSelection();
  EXPECT_FALSE(result);
}

}  // namespace blink
