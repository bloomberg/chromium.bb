/*
 * Copyright (c) 2013, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/editing/markers/document_marker_controller.h"

#include <memory>
#include "base/memory/scoped_refptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/exception_state.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/range.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/markers/suggestion_marker.h"
#include "third_party/blink/renderer/core/editing/markers/suggestion_marker_properties.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"

namespace blink {

class DocumentMarkerControllerTest : public EditingTestBase {
 protected:
  DocumentMarkerController& MarkerController() const {
    return GetDocument().Markers();
  }

  Text* CreateTextNode(const char*);
  void MarkNodeContents(Node*);
  void MarkNodeContentsTextMatch(Node*);
};

Text* DocumentMarkerControllerTest::CreateTextNode(const char* text_contents) {
  return GetDocument().createTextNode(String::FromUTF8(text_contents));
}

void DocumentMarkerControllerTest::MarkNodeContents(Node* node) {
  // Force layoutObjects to be created; TextIterator, which is used in
  // DocumentMarkerControllerTest::addMarker(), needs them.
  GetDocument().UpdateStyleAndLayout();
  auto range = EphemeralRange::RangeOfContents(*node);
  MarkerController().AddSpellingMarker(range);
}

void DocumentMarkerControllerTest::MarkNodeContentsTextMatch(Node* node) {
  // Force layoutObjects to be created; TextIterator, which is used in
  // DocumentMarkerControllerTest::addMarker(), needs them.
  GetDocument().UpdateStyleAndLayout();
  auto range = EphemeralRange::RangeOfContents(*node);
  MarkerController().AddTextMatchMarker(range,
                                        TextMatchMarker::MatchStatus::kActive);
}

TEST_F(DocumentMarkerControllerTest, DidMoveToNewDocument) {
  SetBodyContent("<b><i>foo</i></b>");
  Element* parent = ToElement(GetDocument().body()->firstChild()->firstChild());
  MarkNodeContents(parent);
  EXPECT_EQ(1u, MarkerController().Markers().size());
  Persistent<Document> another_document = Document::CreateForTest();
  another_document->adoptNode(parent, ASSERT_NO_EXCEPTION);

  // No more reference to marked node.
  ThreadState::Current()->CollectAllGarbage();
  EXPECT_EQ(0u, MarkerController().Markers().size());
  EXPECT_EQ(0u, another_document->Markers().Markers().size());
}

TEST_F(DocumentMarkerControllerTest, NodeWillBeRemovedMarkedByNormalize) {
  SetBodyContent("<b><i>foo</i></b>");
  {
    Element* parent =
        ToElement(GetDocument().body()->firstChild()->firstChild());
    parent->AppendChild(CreateTextNode("bar"));
    MarkNodeContents(parent);
    EXPECT_EQ(2u, MarkerController().Markers().size());
    parent->normalize();
  }
  // No more reference to marked node.
  ThreadState::Current()->CollectAllGarbage();
  EXPECT_EQ(1u, MarkerController().Markers().size());
}

TEST_F(DocumentMarkerControllerTest, NodeWillBeRemovedMarkedByRemoveChildren) {
  SetBodyContent("<b><i>foo</i></b>");
  Element* parent = ToElement(GetDocument().body()->firstChild()->firstChild());
  MarkNodeContents(parent);
  EXPECT_EQ(1u, MarkerController().Markers().size());
  parent->RemoveChildren();
  // No more reference to marked node.
  ThreadState::Current()->CollectAllGarbage();
  EXPECT_EQ(0u, MarkerController().Markers().size());
}

TEST_F(DocumentMarkerControllerTest, NodeWillBeRemovedByRemoveMarked) {
  SetBodyContent("<b><i>foo</i></b>");
  {
    Element* parent =
        ToElement(GetDocument().body()->firstChild()->firstChild());
    MarkNodeContents(parent);
    EXPECT_EQ(1u, MarkerController().Markers().size());
    parent->RemoveChild(parent->firstChild());
  }
  // No more reference to marked node.
  ThreadState::Current()->CollectAllGarbage();
  EXPECT_EQ(0u, MarkerController().Markers().size());
}

TEST_F(DocumentMarkerControllerTest, NodeWillBeRemovedMarkedByRemoveAncestor) {
  SetBodyContent("<b><i>foo</i></b>");
  {
    Element* parent =
        ToElement(GetDocument().body()->firstChild()->firstChild());
    MarkNodeContents(parent);
    EXPECT_EQ(1u, MarkerController().Markers().size());
    parent->parentNode()->parentNode()->RemoveChild(parent->parentNode());
  }
  // No more reference to marked node.
  ThreadState::Current()->CollectAllGarbage();
  EXPECT_EQ(0u, MarkerController().Markers().size());
}

TEST_F(DocumentMarkerControllerTest, NodeWillBeRemovedMarkedByRemoveParent) {
  SetBodyContent("<b><i>foo</i></b>");
  {
    Element* parent =
        ToElement(GetDocument().body()->firstChild()->firstChild());
    MarkNodeContents(parent);
    EXPECT_EQ(1u, MarkerController().Markers().size());
    parent->parentNode()->RemoveChild(parent);
  }
  // No more reference to marked node.
  ThreadState::Current()->CollectAllGarbage();
  EXPECT_EQ(0u, MarkerController().Markers().size());
}

TEST_F(DocumentMarkerControllerTest, NodeWillBeRemovedMarkedByReplaceChild) {
  SetBodyContent("<b><i>foo</i></b>");
  {
    Element* parent =
        ToElement(GetDocument().body()->firstChild()->firstChild());
    MarkNodeContents(parent);
    EXPECT_EQ(1u, MarkerController().Markers().size());
    parent->ReplaceChild(CreateTextNode("bar"), parent->firstChild());
  }
  // No more reference to marked node.
  ThreadState::Current()->CollectAllGarbage();
  EXPECT_EQ(0u, MarkerController().Markers().size());
}

TEST_F(DocumentMarkerControllerTest, NodeWillBeRemovedBySetInnerHTML) {
  SetBodyContent("<b><i>foo</i></b>");
  {
    Element* parent =
        ToElement(GetDocument().body()->firstChild()->firstChild());
    MarkNodeContents(parent);
    EXPECT_EQ(1u, MarkerController().Markers().size());
    SetBodyContent("");
  }
  // No more reference to marked node.
  ThreadState::Current()->CollectAllGarbage();
  EXPECT_EQ(0u, MarkerController().Markers().size());
}

TEST_F(DocumentMarkerControllerTest, UpdateRenderedRects) {
  SetBodyContent("<div style='margin: 100px'>foo</div>");
  Element* div = ToElement(GetDocument().body()->firstChild());
  MarkNodeContentsTextMatch(div);
  Vector<IntRect> rendered_rects =
      MarkerController().LayoutRectsForTextMatchMarkers();
  EXPECT_EQ(1u, rendered_rects.size());

  div->setAttribute(HTMLNames::styleAttr, "margin: 200px");
  GetDocument().UpdateStyleAndLayout();
  Vector<IntRect> new_rendered_rects =
      MarkerController().LayoutRectsForTextMatchMarkers();
  EXPECT_EQ(1u, new_rendered_rects.size());
  EXPECT_NE(rendered_rects[0], new_rendered_rects[0]);
}

TEST_F(DocumentMarkerControllerTest, CompositionMarkersNotMerged) {
  SetBodyContent("<div style='margin: 100px'>foo</div>");
  Node* text = GetDocument().body()->firstChild()->firstChild();
  MarkerController().AddCompositionMarker(
      EphemeralRange(Position(text, 0), Position(text, 1)), Color::kTransparent,
      ui::mojom::ImeTextSpanThickness::kThin, Color::kBlack);
  MarkerController().AddCompositionMarker(
      EphemeralRange(Position(text, 1), Position(text, 3)), Color::kTransparent,
      ui::mojom::ImeTextSpanThickness::kThick, Color::kBlack);

  EXPECT_EQ(2u, MarkerController().Markers().size());
}

TEST_F(DocumentMarkerControllerTest, SetMarkerActiveTest) {
  SetBodyContent("<b>foo</b>");
  Element* b_element = ToElement(GetDocument().body()->firstChild());
  EphemeralRange ephemeral_range = EphemeralRange::RangeOfContents(*b_element);
  Position start_b_element =
      ToPositionInDOMTree(ephemeral_range.StartPosition());
  Position end_b_element = ToPositionInDOMTree(ephemeral_range.EndPosition());
  const EphemeralRange range(start_b_element, end_b_element);
  // Try to make active a marker that doesn't exist.
  EXPECT_FALSE(MarkerController().SetTextMatchMarkersActive(range, true));

  // Add a marker and try it once more.
  MarkerController().AddTextMatchMarker(
      range, TextMatchMarker::MatchStatus::kInactive);
  EXPECT_EQ(1u, MarkerController().Markers().size());
  EXPECT_TRUE(MarkerController().SetTextMatchMarkersActive(range, true));
}

TEST_F(DocumentMarkerControllerTest, RemoveStartOfMarker) {
  SetBodyContent("<b>abc</b>");
  Node* b_element = GetDocument().body()->firstChild();
  Node* text = b_element->firstChild();

  // Add marker under "abc"
  EphemeralRange marker_range =
      EphemeralRange(Position(text, 0), Position(text, 3));
  GetDocument().Markers().AddTextMatchMarker(
      marker_range, TextMatchMarker::MatchStatus::kInactive);

  // Remove markers that overlap "a"
  marker_range = EphemeralRange(Position(text, 0), Position(text, 1));
  GetDocument().Markers().RemoveMarkersInRange(marker_range,
                                               DocumentMarker::AllMarkers());

  EXPECT_EQ(0u, MarkerController().Markers().size());
}

TEST_F(DocumentMarkerControllerTest, RemoveMiddleOfMarker) {
  SetBodyContent("<b>abc</b>");
  Node* b_element = GetDocument().body()->firstChild();
  Node* text = b_element->firstChild();

  // Add marker under "abc"
  EphemeralRange marker_range =
      EphemeralRange(Position(text, 0), Position(text, 3));
  GetDocument().Markers().AddTextMatchMarker(
      marker_range, TextMatchMarker::MatchStatus::kInactive);

  // Remove markers that overlap "b"
  marker_range = EphemeralRange(Position(text, 1), Position(text, 2));
  GetDocument().Markers().RemoveMarkersInRange(marker_range,
                                               DocumentMarker::AllMarkers());

  EXPECT_EQ(0u, MarkerController().Markers().size());
}

TEST_F(DocumentMarkerControllerTest, RemoveEndOfMarker) {
  SetBodyContent("<b>abc</b>");
  Node* b_element = GetDocument().body()->firstChild();
  Node* text = b_element->firstChild();

  // Add marker under "abc"
  EphemeralRange marker_range =
      EphemeralRange(Position(text, 0), Position(text, 3));
  GetDocument().Markers().AddTextMatchMarker(
      marker_range, TextMatchMarker::MatchStatus::kInactive);

  // Remove markers that overlap "c"
  marker_range = EphemeralRange(Position(text, 2), Position(text, 3));
  GetDocument().Markers().RemoveMarkersInRange(marker_range,
                                               DocumentMarker::AllMarkers());

  EXPECT_EQ(0u, MarkerController().Markers().size());
}

TEST_F(DocumentMarkerControllerTest, RemoveSpellingMarkersUnderWords) {
  SetBodyContent("<div contenteditable>foo</div>");
  Element* div = GetDocument().QuerySelector("div");
  Node* text = div->firstChild();

  // Add a spelling marker and a text match marker to "foo".
  const EphemeralRange marker_range(Position(text, 0), Position(text, 3));
  MarkerController().AddSpellingMarker(marker_range);
  MarkerController().AddTextMatchMarker(
      marker_range, TextMatchMarker::MatchStatus::kInactive);

  MarkerController().RemoveSpellingMarkersUnderWords({"foo"});

  // RemoveSpellingMarkersUnderWords does not remove text match marker.
  ASSERT_EQ(1u, MarkerController().Markers().size());
  const DocumentMarker& marker = *MarkerController().Markers()[0];
  EXPECT_EQ(0u, marker.StartOffset());
  EXPECT_EQ(3u, marker.EndOffset());
  EXPECT_EQ(DocumentMarker::kTextMatch, marker.GetType());
}

TEST_F(DocumentMarkerControllerTest, RemoveSuggestionMarkerByTag) {
  SetBodyContent("<div contenteditable>foo</div>");
  Element* div = GetDocument().QuerySelector("div");
  Node* text = div->firstChild();

  MarkerController().AddSuggestionMarker(
      EphemeralRange(Position(text, 0), Position(text, 1)),
      SuggestionMarkerProperties());

  ASSERT_EQ(1u, MarkerController().Markers().size());
  const SuggestionMarker& marker =
      *ToSuggestionMarker(MarkerController().Markers()[0]);
  MarkerController().RemoveSuggestionMarkerByTag(text, marker.Tag());
  EXPECT_EQ(0u, MarkerController().Markers().size());
}

TEST_F(DocumentMarkerControllerTest, FirstMarkerIntersectingOffsetRange) {
  SetBodyContent("<div contenteditable>123456789</div>");
  GetDocument().UpdateStyleAndLayout();
  Element* div = GetDocument().QuerySelector("div");
  Text* text = ToText(div->firstChild());

  // Add a spelling marker on "123"
  MarkerController().AddSpellingMarker(
      EphemeralRange(Position(text, 0), Position(text, 3)));

  // Query for a spellcheck marker intersecting "3456"
  const DocumentMarker* const result =
      MarkerController().FirstMarkerIntersectingOffsetRange(
          *text, 2, 6, DocumentMarker::MisspellingMarkers());

  EXPECT_EQ(DocumentMarker::kSpelling, result->GetType());
  EXPECT_EQ(0u, result->StartOffset());
  EXPECT_EQ(3u, result->EndOffset());
}

TEST_F(DocumentMarkerControllerTest,
       FirstMarkerIntersectingOffsetRange_collapsed) {
  SetBodyContent("<div contenteditable>123456789</div>");
  GetDocument().UpdateStyleAndLayout();
  Element* div = GetDocument().QuerySelector("div");
  Text* text = ToText(div->firstChild());

  // Add a spelling marker on "123"
  MarkerController().AddSpellingMarker(
      EphemeralRange(Position(text, 0), Position(text, 3)));

  // Query for a spellcheck marker containing the position between "1" and "2"
  const DocumentMarker* const result =
      MarkerController().FirstMarkerIntersectingOffsetRange(
          *text, 1, 1, DocumentMarker::MisspellingMarkers());

  EXPECT_EQ(DocumentMarker::kSpelling, result->GetType());
  EXPECT_EQ(0u, result->StartOffset());
  EXPECT_EQ(3u, result->EndOffset());
}

TEST_F(DocumentMarkerControllerTest, MarkersIntersectingRange) {
  SetBodyContent("<div contenteditable>123456789</div>");
  Element* div = GetDocument().QuerySelector("div");
  Node* text = div->firstChild();

  // Add a spelling marker on "123"
  MarkerController().AddSpellingMarker(
      EphemeralRange(Position(text, 0), Position(text, 3)));
  // Add a text match marker on "456"
  MarkerController().AddTextMatchMarker(
      EphemeralRange(Position(text, 3), Position(text, 6)),
      TextMatchMarker::MatchStatus::kInactive);
  // Add a grammar marker on "789"
  MarkerController().AddSpellingMarker(
      EphemeralRange(Position(text, 6), Position(text, 9)));

  // Query for spellcheck markers intersecting "3456". The text match marker
  // should not be returned, nor should the spelling marker touching the range.
  const HeapVector<std::pair<Member<Node>, Member<DocumentMarker>>>& results =
      MarkerController().MarkersIntersectingRange(
          EphemeralRangeInFlatTree(PositionInFlatTree(text, 2),
                                   PositionInFlatTree(text, 6)),
          DocumentMarker::MisspellingMarkers());

  EXPECT_EQ(1u, results.size());
  EXPECT_EQ(DocumentMarker::kSpelling, results[0].second->GetType());
  EXPECT_EQ(0u, results[0].second->StartOffset());
  EXPECT_EQ(3u, results[0].second->EndOffset());
}

TEST_F(DocumentMarkerControllerTest, MarkersIntersectingCollapsedRange) {
  SetBodyContent("<div contenteditable>123456789</div>");
  Element* div = GetDocument().QuerySelector("div");
  Node* text = div->firstChild();

  // Add a spelling marker on "123"
  MarkerController().AddSpellingMarker(
      EphemeralRange(Position(text, 0), Position(text, 3)));

  // Query for spellcheck markers containing the position between "1" and "2"
  const HeapVector<std::pair<Member<Node>, Member<DocumentMarker>>>& results =
      MarkerController().MarkersIntersectingRange(
          EphemeralRangeInFlatTree(PositionInFlatTree(text, 1),
                                   PositionInFlatTree(text, 1)),
          DocumentMarker::MisspellingMarkers());

  EXPECT_EQ(1u, results.size());
  EXPECT_EQ(DocumentMarker::kSpelling, results[0].second->GetType());
  EXPECT_EQ(0u, results[0].second->StartOffset());
  EXPECT_EQ(3u, results[0].second->EndOffset());
}

TEST_F(DocumentMarkerControllerTest, MarkersIntersectingRangeWithShadowDOM) {
  // Set up some shadow elements in a way we know doesn't work properly when
  // using EphemeralRange instead of EphemeralRangeInFlatTree:
  // <div>not shadow</div>
  // <div> (shadow DOM host)
  //   #shadow-root
  //     <div>shadow1</div>
  //     <div>shadow2</div>
  // Caling MarkersIntersectingRange with an EphemeralRange starting in the
  // "not shadow" text and ending in the "shadow1" text will crash.
  SetBodyContent(
      "<div id=\"not_shadow\">not shadow</div><div id=\"shadow_root\" />");
  ShadowRoot* shadow_root = SetShadowContent(
      "<div id=\"shadow1\">shadow1</div><div id=\"shadow2\">shadow2</div>",
      "shadow_root");

  Element* not_shadow_div = GetDocument().QuerySelector("#not_shadow");
  Node* not_shadow_text = not_shadow_div->firstChild();

  Element* shadow1 = shadow_root->QuerySelector("#shadow1");
  Node* shadow1_text = shadow1->firstChild();

  MarkerController().AddTextMatchMarker(
      EphemeralRange(Position(not_shadow_text, 0),
                     Position(not_shadow_text, 10)),
      TextMatchMarker::MatchStatus::kInactive);

  const HeapVector<std::pair<Member<Node>, Member<DocumentMarker>>>& results =
      MarkerController().MarkersIntersectingRange(
          EphemeralRangeInFlatTree(PositionInFlatTree(not_shadow_text, 9),
                                   PositionInFlatTree(shadow1_text, 1)),
          DocumentMarker::kTextMatch);
  EXPECT_EQ(1u, results.size());
}

TEST_F(DocumentMarkerControllerTest, SuggestionMarkersHaveUniqueTags) {
  SetBodyContent("<div contenteditable>foo</div>");
  Element* div = GetDocument().QuerySelector("div");
  Node* text = div->firstChild();

  MarkerController().AddSuggestionMarker(
      EphemeralRange(Position(text, 0), Position(text, 1)),
      SuggestionMarkerProperties());
  MarkerController().AddSuggestionMarker(
      EphemeralRange(Position(text, 0), Position(text, 1)),
      SuggestionMarkerProperties());

  EXPECT_EQ(2u, MarkerController().Markers().size());
  EXPECT_NE(ToSuggestionMarker(MarkerController().Markers()[0])->Tag(),
            ToSuggestionMarker(MarkerController().Markers()[1])->Tag());
}

}  // namespace blink
