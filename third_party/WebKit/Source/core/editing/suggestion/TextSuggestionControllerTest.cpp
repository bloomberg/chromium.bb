// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/suggestion/TextSuggestionController.h"

#include "core/editing/EditingTestBase.h"
#include "core/editing/FrameSelection.h"
#include "core/editing/markers/DocumentMarkerController.h"
#include "core/editing/spellcheck/SpellChecker.h"

namespace blink {

class TextSuggestionControllerTest : public EditingTestBase {};

TEST_F(TextSuggestionControllerTest, ApplySpellCheckSuggestion) {
  SetBodyContent(
      "<div contenteditable>"
      "spllchck"
      "</div>");
  Element* div = GetDocument().QuerySelector("div");
  Node* text = div->firstChild();

  GetDocument().Markers().AddSpellingMarker(
      EphemeralRange(Position(text, 0), Position(text, 8)));
  // Select immediately before misspelling
  GetDocument().GetFrame()->Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(Position(text, 0), Position(text, 0))
          .Build());
  GetDocument()
      .GetFrame()
      ->GetTextSuggestionController()
      .ApplySpellCheckSuggestion("spellcheck");

  EXPECT_EQ("spellcheck", text->textContent());
}

TEST_F(TextSuggestionControllerTest, DeleteActiveSuggestionRange_DeleteAtEnd) {
  SetBodyContent(
      "<div contenteditable>"
      "word1 word2"
      "</div>");
  Element* div = GetDocument().QuerySelector("div");
  Node* text = div->firstChild();

  // Mark "word2" as the active suggestion range
  GetDocument().Markers().AddActiveSuggestionMarker(
      EphemeralRange(Position(text, 6), Position(text, 11)), Color::kBlack,
      StyleableMarker::Thickness::kThin, Color::kBlack);
  // Select immediately before word2
  GetDocument().GetFrame()->Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(Position(text, 6), Position(text, 6))
          .Build());
  GetDocument()
      .GetFrame()
      ->GetTextSuggestionController()
      .DeleteActiveSuggestionRange();

  EXPECT_EQ("word1\xA0", text->textContent());
}

TEST_F(TextSuggestionControllerTest,
       DeleteActiveSuggestionRange_DeleteInMiddle) {
  SetBodyContent(
      "<div contenteditable>"
      "word1 word2 word3"
      "</div>");
  Element* div = GetDocument().QuerySelector("div");
  Node* text = div->firstChild();

  // Mark "word2" as the active suggestion range
  GetDocument().Markers().AddActiveSuggestionMarker(
      EphemeralRange(Position(text, 6), Position(text, 11)), Color::kBlack,
      StyleableMarker::Thickness::kThin, Color::kBlack);
  // Select immediately before word2
  GetDocument().GetFrame()->Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(Position(text, 6), Position(text, 6))
          .Build());
  GetDocument()
      .GetFrame()
      ->GetTextSuggestionController()
      .DeleteActiveSuggestionRange();

  // One of the extra spaces around "word2" should have been removed
  EXPECT_EQ("word1\xA0word3", text->textContent());
}

TEST_F(TextSuggestionControllerTest,
       DeleteActiveSuggestionRange_DeleteAtBeginningWithSpaceAfter) {
  SetBodyContent(
      "<div contenteditable>"
      "word1 word2"
      "</div>");
  Element* div = GetDocument().QuerySelector("div");
  Node* text = div->firstChild();

  // Mark "word1" as the active suggestion range
  GetDocument().Markers().AddActiveSuggestionMarker(
      EphemeralRange(Position(text, 0), Position(text, 5)), Color::kBlack,
      StyleableMarker::Thickness::kThin, Color::kBlack);
  // Select immediately before word1
  GetDocument().GetFrame()->Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(Position(text, 0), Position(text, 0))
          .Build());
  GetDocument()
      .GetFrame()
      ->GetTextSuggestionController()
      .DeleteActiveSuggestionRange();

  // The space after "word1" should have been removed (to avoid leaving an
  // empty space at the beginning of the composition)
  EXPECT_EQ("word2", text->textContent());
}

TEST_F(TextSuggestionControllerTest,
       DeleteActiveSuggestionRange_DeleteEntireRange) {
  SetBodyContent(
      "<div contenteditable>"
      "word1"
      "</div>");
  Element* div = GetDocument().QuerySelector("div");
  Node* text = div->firstChild();

  // Mark "word1" as the active suggestion range
  GetDocument().Markers().AddActiveSuggestionMarker(
      EphemeralRange(Position(text, 0), Position(text, 5)), Color::kBlack,
      StyleableMarker::Thickness::kThin, Color::kBlack);
  // Select immediately before word1
  GetDocument().GetFrame()->Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(Position(text, 0), Position(text, 0))
          .Build());
  GetDocument()
      .GetFrame()
      ->GetTextSuggestionController()
      .DeleteActiveSuggestionRange();

  EXPECT_EQ("", text->textContent());
}

// The following two cases test situations that probably shouldn't occur in
// normal use (spell check/suggestoin markers not spanning a whole word), but
// are included anyway to verify that DeleteActiveSuggestionRange() is
// well-behaved in these cases

TEST_F(TextSuggestionControllerTest,
       DeleteActiveSuggestionRange_DeleteRangeWithTextBeforeAndSpaceAfter) {
  SetBodyContent(
      "<div contenteditable>"
      "word1word2 word3"
      "</div>");
  Element* div = GetDocument().QuerySelector("div");
  Node* text = div->firstChild();

  // Mark "word2" as the active suggestion range
  GetDocument().Markers().AddActiveSuggestionMarker(
      EphemeralRange(Position(text, 5), Position(text, 10)), Color::kBlack,
      StyleableMarker::Thickness::kThin, Color::kBlack);
  // Select immediately before word2
  GetDocument().GetFrame()->Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(Position(text, 5), Position(text, 5))
          .Build());
  GetDocument()
      .GetFrame()
      ->GetTextSuggestionController()
      .DeleteActiveSuggestionRange();

  EXPECT_EQ("word1\xA0word3", text->textContent());
}

TEST_F(TextSuggestionControllerTest,
       DeleteActiveSuggestionRange_DeleteRangeWithSpaceBeforeAndTextAfter) {
  SetBodyContent(
      "<div contenteditable>"
      "word1 word2word3"
      "</div>");
  Element* div = GetDocument().QuerySelector("div");
  Node* text = div->firstChild();

  // Mark "word2" as the active suggestion range
  GetDocument().Markers().AddActiveSuggestionMarker(
      EphemeralRange(Position(text, 6), Position(text, 11)), Color::kBlack,
      StyleableMarker::Thickness::kThin, Color::kBlack);
  // Select immediately before word2
  GetDocument().GetFrame()->Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(Position(text, 6), Position(text, 6))
          .Build());
  GetDocument()
      .GetFrame()
      ->GetTextSuggestionController()
      .DeleteActiveSuggestionRange();

  EXPECT_EQ("word1\xA0word3", text->textContent());
}

TEST_F(TextSuggestionControllerTest,
       DeleteActiveSuggestionRange_DeleteAtBeginningWithTextAfter) {
  SetBodyContent(
      "<div contenteditable>"
      "word1word2"
      "</div>");
  Element* div = GetDocument().QuerySelector("div");
  Node* text = div->firstChild();

  // Mark "word1" as the active suggestion range
  GetDocument().Markers().AddActiveSuggestionMarker(
      EphemeralRange(Position(text, 0), Position(text, 5)), Color::kBlack,
      StyleableMarker::Thickness::kThin, Color::kBlack);
  // Select immediately before word1
  GetDocument().GetFrame()->Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(Position(text, 0), Position(text, 0))
          .Build());
  GetDocument()
      .GetFrame()
      ->GetTextSuggestionController()
      .DeleteActiveSuggestionRange();

  EXPECT_EQ("word2", text->textContent());
}

TEST_F(TextSuggestionControllerTest,
       DeleteActiveSuggestionRange_NewWordAddedToDictionary) {
  SetBodyContent(
      "<div contenteditable>"
      "embiggen"
      "</div>");
  Element* div = GetDocument().QuerySelector("div");
  Node* text = div->firstChild();

  // Mark "embiggen" as misspelled
  GetDocument().Markers().AddSpellingMarker(
      EphemeralRange(Position(text, 0), Position(text, 8)));
  // Select inside before "embiggen"
  GetDocument().GetFrame()->Selection().SetSelection(
      SelectionInDOMTree::Builder()
          .SetBaseAndExtent(Position(text, 1), Position(text, 1))
          .Build());

  // Add some other word to the dictionary
  GetDocument()
      .GetFrame()
      ->GetTextSuggestionController()
      .NewWordAddedToDictionary("cromulent");
  // Verify the spelling marker is still present
  EXPECT_NE(nullptr, GetDocument()
                         .GetFrame()
                         ->GetSpellChecker()
                         .GetSpellCheckMarkerUnderSelection()
                         .first);

  // Add "embiggen" to the dictionary
  GetDocument()
      .GetFrame()
      ->GetTextSuggestionController()
      .NewWordAddedToDictionary("embiggen");
  // Verify the spelling marker is gone
  EXPECT_EQ(nullptr, GetDocument()
                         .GetFrame()
                         ->GetSpellChecker()
                         .GetSpellCheckMarkerUnderSelection()
                         .first);
}

}  // namespace blink
