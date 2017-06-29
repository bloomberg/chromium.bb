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

#include "core/editing/markers/DocumentMarkerController.h"

#include <memory>
#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/Document.h"
#include "core/dom/Range.h"
#include "core/dom/Text.h"
#include "core/editing/EditingTestBase.h"
#include "core/editing/EphemeralRange.h"
#include "core/html/HTMLElement.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/wtf/PassRefPtr.h"
#include "platform/wtf/RefPtr.h"
#include "testing/gtest/include/gtest/gtest.h"

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
  Persistent<Document> another_document = Document::Create();
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
      EphemeralRange(Position(text, 0), Position(text, 1)), Color::kBlack,
      StyleableMarker::Thickness::kThin, Color::kBlack);
  MarkerController().AddCompositionMarker(
      EphemeralRange(Position(text, 1), Position(text, 3)), Color::kBlack,
      StyleableMarker::Thickness::kThick, Color::kBlack);

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

}  // namespace blink
