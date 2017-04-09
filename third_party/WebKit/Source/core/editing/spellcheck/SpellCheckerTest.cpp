// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/spellcheck/SpellChecker.h"

#include "core/editing/Editor.h"
#include "core/editing/spellcheck/SpellCheckTestBase.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
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
  FrameView& frame_view = Page().GetFrameView();
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
  GetDocument().GetFrame()->GetSpellChecker().AdvanceToNextMisspelling(false);
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
  GetDocument().GetFrame()->GetSpellChecker().AdvanceToNextMisspelling(false);
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

  Position new_position(input->InnerEditorElement()->FirstChild(), 3);
  GetDocument().GetFrame()->Selection().SetSelection(
      SelectionInDOMTree::Builder().Collapse(new_position).Build());
  ASSERT_EQ(3u, input->selectionStart());

  EXPECT_TRUE(GetFrame().GetSpellChecker().IsSpellCheckingEnabled());
  ForceLayout();
  int start_count = LayoutCount();
  GetFrame().GetSpellChecker().RespondToChangedSelection(
      old_selection.Start(),
      FrameSelection::kCloseTyping | FrameSelection::kClearTypingStyle);
  EXPECT_EQ(start_count, LayoutCount());
}

}  // namespace blink
