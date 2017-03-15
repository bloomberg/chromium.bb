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
  int layoutCount() const { return page().frameView().layoutCount(); }
  DummyPageHolder& page() const { return dummyPageHolder(); }

  void forceLayout();
};

void SpellCheckerTest::forceLayout() {
  FrameView& frameView = page().frameView();
  IntRect frameRect = frameView.frameRect();
  frameRect.setWidth(frameRect.width() + 1);
  frameRect.setHeight(frameRect.height() + 1);
  page().frameView().setFrameRect(frameRect);
  document().updateStyleAndLayoutIgnorePendingStylesheets();
}

TEST_F(SpellCheckerTest, AdvanceToNextMisspellingWithEmptyInputNoCrash) {
  setBodyContent("<input placeholder='placeholder'>abc");
  updateAllLifecyclePhases();
  Element* input = document().querySelector("input");
  input->focus();
  // Do not crash in AdvanceToNextMisspelling command.
  EXPECT_TRUE(
      document().frame()->editor().executeCommand("AdvanceToNextMisspelling"));
}

// Regression test for crbug.com/701309
TEST_F(SpellCheckerTest, AdvanceToNextMisspellingWithImageInTableNoCrash) {
  setBodyContent(
      "<div contenteditable>"
      "<table><tr><td>"
      "<img src=foo.jpg>"
      "</td></tr></table>"
      "zz zz zz"
      "</div>");
  document().querySelector("div")->focus();
  updateAllLifecyclePhases();

  // Do not crash in advanceToNextMisspelling.
  document().frame()->spellChecker().advanceToNextMisspelling(false);
}

TEST_F(SpellCheckerTest, SpellCheckDoesNotCauseUpdateLayout) {
  setBodyContent("<input>");
  HTMLInputElement* input =
      toHTMLInputElement(document().querySelector("input"));
  input->focus();
  input->setValue("Hello, input field");
  document().updateStyleAndLayout();
  VisibleSelection oldSelection =
      document()
          .frame()
          ->selection()
          .computeVisibleSelectionInDOMTreeDeprecated();

  Position newPosition(input->innerEditorElement()->firstChild(), 3);
  document().frame()->selection().setSelection(
      SelectionInDOMTree::Builder().collapse(newPosition).build());
  ASSERT_EQ(3u, input->selectionStart());

  EXPECT_TRUE(frame().spellChecker().isSpellCheckingEnabled());
  forceLayout();
  int startCount = layoutCount();
  frame().spellChecker().respondToChangedSelection(
      oldSelection.start(),
      FrameSelection::CloseTyping | FrameSelection::ClearTypingStyle);
  EXPECT_EQ(startCount, layoutCount());
}

}  // namespace blink
