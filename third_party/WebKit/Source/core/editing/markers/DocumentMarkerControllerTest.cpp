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
#include "core/editing/EphemeralRange.h"
#include "core/editing/markers/RenderedDocumentMarker.h"
#include "core/html/HTMLElement.h"
#include "core/testing/DummyPageHolder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"

namespace blink {

class DocumentMarkerControllerTest : public ::testing::Test {
 protected:
  DocumentMarkerControllerTest()
      : dummy_page_holder_(DummyPageHolder::Create(IntSize(800, 600))) {}

  Document& GetDocument() const { return dummy_page_holder_->GetDocument(); }
  DocumentMarkerController& MarkerController() const {
    return GetDocument().Markers();
  }

  Text* CreateTextNode(const char*);
  void MarkNodeContents(Node*);
  void MarkNodeContentsWithComposition(Node*);
  void SetBodyInnerHTML(const char*);

 private:
  std::unique_ptr<DummyPageHolder> dummy_page_holder_;
};

Text* DocumentMarkerControllerTest::CreateTextNode(const char* text_contents) {
  return GetDocument().createTextNode(String::FromUTF8(text_contents));
}

void DocumentMarkerControllerTest::MarkNodeContents(Node* node) {
  // Force layoutObjects to be created; TextIterator, which is used in
  // DocumentMarkerControllerTest::addMarker(), needs them.
  GetDocument().UpdateStyleAndLayout();
  auto range = EphemeralRange::RangeOfContents(*node);
  MarkerController().AddMarker(range.StartPosition(), range.EndPosition(),
                               DocumentMarker::kSpelling);
}

void DocumentMarkerControllerTest::MarkNodeContentsWithComposition(Node* node) {
  // Force layoutObjects to be created; TextIterator, which is used in
  // DocumentMarkerControllerTest::addMarker(), needs them.
  GetDocument().UpdateStyleAndLayout();
  auto range = EphemeralRange::RangeOfContents(*node);
  MarkerController().AddCompositionMarker(range.StartPosition(),
                                          range.EndPosition(), Color::kBlack,
                                          false, Color::kBlack);
}

void DocumentMarkerControllerTest::SetBodyInnerHTML(const char* body_content) {
  GetDocument().body()->setInnerHTML(String::FromUTF8(body_content));
}

TEST_F(DocumentMarkerControllerTest, DidMoveToNewDocument) {
  SetBodyInnerHTML("<b><i>foo</i></b>");
  Element* parent = ToElement(GetDocument().body()->FirstChild()->firstChild());
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
  SetBodyInnerHTML("<b><i>foo</i></b>");
  {
    Element* parent =
        ToElement(GetDocument().body()->FirstChild()->firstChild());
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
  SetBodyInnerHTML("<b><i>foo</i></b>");
  Element* parent = ToElement(GetDocument().body()->FirstChild()->firstChild());
  MarkNodeContents(parent);
  EXPECT_EQ(1u, MarkerController().Markers().size());
  parent->RemoveChildren();
  // No more reference to marked node.
  ThreadState::Current()->CollectAllGarbage();
  EXPECT_EQ(0u, MarkerController().Markers().size());
}

TEST_F(DocumentMarkerControllerTest, NodeWillBeRemovedByRemoveMarked) {
  SetBodyInnerHTML("<b><i>foo</i></b>");
  {
    Element* parent =
        ToElement(GetDocument().body()->FirstChild()->firstChild());
    MarkNodeContents(parent);
    EXPECT_EQ(1u, MarkerController().Markers().size());
    parent->RemoveChild(parent->FirstChild());
  }
  // No more reference to marked node.
  ThreadState::Current()->CollectAllGarbage();
  EXPECT_EQ(0u, MarkerController().Markers().size());
}

TEST_F(DocumentMarkerControllerTest, NodeWillBeRemovedMarkedByRemoveAncestor) {
  SetBodyInnerHTML("<b><i>foo</i></b>");
  {
    Element* parent =
        ToElement(GetDocument().body()->FirstChild()->firstChild());
    MarkNodeContents(parent);
    EXPECT_EQ(1u, MarkerController().Markers().size());
    parent->parentNode()->parentNode()->RemoveChild(parent->parentNode());
  }
  // No more reference to marked node.
  ThreadState::Current()->CollectAllGarbage();
  EXPECT_EQ(0u, MarkerController().Markers().size());
}

TEST_F(DocumentMarkerControllerTest, NodeWillBeRemovedMarkedByRemoveParent) {
  SetBodyInnerHTML("<b><i>foo</i></b>");
  {
    Element* parent =
        ToElement(GetDocument().body()->FirstChild()->firstChild());
    MarkNodeContents(parent);
    EXPECT_EQ(1u, MarkerController().Markers().size());
    parent->parentNode()->RemoveChild(parent);
  }
  // No more reference to marked node.
  ThreadState::Current()->CollectAllGarbage();
  EXPECT_EQ(0u, MarkerController().Markers().size());
}

TEST_F(DocumentMarkerControllerTest, NodeWillBeRemovedMarkedByReplaceChild) {
  SetBodyInnerHTML("<b><i>foo</i></b>");
  {
    Element* parent =
        ToElement(GetDocument().body()->FirstChild()->firstChild());
    MarkNodeContents(parent);
    EXPECT_EQ(1u, MarkerController().Markers().size());
    parent->ReplaceChild(CreateTextNode("bar"), parent->FirstChild());
  }
  // No more reference to marked node.
  ThreadState::Current()->CollectAllGarbage();
  EXPECT_EQ(0u, MarkerController().Markers().size());
}

TEST_F(DocumentMarkerControllerTest, NodeWillBeRemovedBySetInnerHTML) {
  SetBodyInnerHTML("<b><i>foo</i></b>");
  {
    Element* parent =
        ToElement(GetDocument().body()->FirstChild()->firstChild());
    MarkNodeContents(parent);
    EXPECT_EQ(1u, MarkerController().Markers().size());
    SetBodyInnerHTML("");
  }
  // No more reference to marked node.
  ThreadState::Current()->CollectAllGarbage();
  EXPECT_EQ(0u, MarkerController().Markers().size());
}

TEST_F(DocumentMarkerControllerTest, UpdateRenderedRects) {
  SetBodyInnerHTML("<div style='margin: 100px'>foo</div>");
  Element* div = ToElement(GetDocument().body()->FirstChild());
  MarkNodeContents(div);
  Vector<IntRect> rendered_rects =
      MarkerController().RenderedRectsForMarkers(DocumentMarker::kSpelling);
  EXPECT_EQ(1u, rendered_rects.size());

  div->setAttribute(HTMLNames::styleAttr, "margin: 200px");
  GetDocument().UpdateStyleAndLayout();
  Vector<IntRect> new_rendered_rects =
      MarkerController().RenderedRectsForMarkers(DocumentMarker::kSpelling);
  EXPECT_EQ(1u, new_rendered_rects.size());
  EXPECT_NE(rendered_rects[0], new_rendered_rects[0]);
}

TEST_F(DocumentMarkerControllerTest, UpdateRenderedRectsForComposition) {
  SetBodyInnerHTML("<div style='margin: 100px'>foo</div>");
  Element* div = ToElement(GetDocument().body()->FirstChild());
  MarkNodeContentsWithComposition(div);
  Vector<IntRect> rendered_rects =
      MarkerController().RenderedRectsForMarkers(DocumentMarker::kComposition);
  EXPECT_EQ(1u, rendered_rects.size());

  div->setAttribute(HTMLNames::styleAttr, "margin: 200px");
  GetDocument().UpdateStyleAndLayout();
  Vector<IntRect> new_rendered_rects =
      MarkerController().RenderedRectsForMarkers(DocumentMarker::kComposition);
  EXPECT_EQ(1u, new_rendered_rects.size());
  EXPECT_NE(rendered_rects[0], new_rendered_rects[0]);
}

TEST_F(DocumentMarkerControllerTest, CompositionMarkersNotMerged) {
  SetBodyInnerHTML("<div style='margin: 100px'>foo</div>");
  Node* text = GetDocument().body()->FirstChild()->firstChild();
  GetDocument().UpdateStyleAndLayout();
  MarkerController().AddCompositionMarker(Position(text, 0), Position(text, 1),
                                          Color::kBlack, false, Color::kBlack);
  MarkerController().AddCompositionMarker(Position(text, 1), Position(text, 3),
                                          Color::kBlack, true, Color::kBlack);

  EXPECT_EQ(2u, MarkerController().Markers().size());
}

TEST_F(DocumentMarkerControllerTest, SetMarkerActiveTest) {
  SetBodyInnerHTML("<b>foo</b>");
  GetDocument().UpdateStyleAndLayout();
  Element* b_element = ToElement(GetDocument().body()->FirstChild());
  EphemeralRange ephemeral_range = EphemeralRange::RangeOfContents(*b_element);
  Position start_b_element =
      ToPositionInDOMTree(ephemeral_range.StartPosition());
  Position end_b_element = ToPositionInDOMTree(ephemeral_range.EndPosition());
  const EphemeralRange range(start_b_element, end_b_element);
  // Try to make active a marker that doesn't exist.
  EXPECT_FALSE(MarkerController().SetMarkersActive(range, true));

  // Add a marker and try it once more.
  MarkerController().AddTextMatchMarker(range,
                                        DocumentMarker::MatchStatus::kInactive);
  EXPECT_EQ(1u, MarkerController().Markers().size());
  EXPECT_TRUE(MarkerController().SetMarkersActive(range, true));
}

TEST_F(DocumentMarkerControllerTest,
       RemoveStartOfMarkerDoRemovePartiallyOverlapping) {
  SetBodyInnerHTML("<b>abc</b>");
  GetDocument().UpdateStyleAndLayout();
  Node* b_element = GetDocument().body()->FirstChild();
  Node* text = b_element->firstChild();

  // Add marker under "abc"
  EphemeralRange marker_range =
      EphemeralRange(Position(text, 0), Position(text, 3));
  GetDocument().Markers().AddTextMatchMarker(
      marker_range, DocumentMarker::MatchStatus::kInactive);

  // Remove markers that overlap "a"
  marker_range = EphemeralRange(Position(text, 0), Position(text, 1));
  GetDocument().Markers().RemoveMarkers(
      marker_range, DocumentMarker::AllMarkers(),
      DocumentMarkerController::kRemovePartiallyOverlappingMarker);

  EXPECT_EQ(0u, MarkerController().Markers().size());
}

TEST_F(DocumentMarkerControllerTest,
       RemoveStartOfMarkerDontRemovePartiallyOverlapping) {
  SetBodyInnerHTML("<b>abc</b>");
  GetDocument().UpdateStyleAndLayout();
  Node* b_element = GetDocument().body()->FirstChild();
  Node* text = b_element->firstChild();

  // Add marker under "abc"
  EphemeralRange marker_range =
      EphemeralRange(Position(text, 0), Position(text, 3));
  GetDocument().Markers().AddTextMatchMarker(
      marker_range, DocumentMarker::MatchStatus::kInactive);

  // Remove portion of marker that overlaps "a"
  marker_range = EphemeralRange(Position(text, 0), Position(text, 1));
  GetDocument().Markers().RemoveMarkers(
      marker_range, DocumentMarker::AllMarkers(),
      DocumentMarkerController::kDoNotRemovePartiallyOverlappingMarker);

  EXPECT_EQ(1u, MarkerController().Markers().size());

  EXPECT_EQ(1u, MarkerController().Markers()[0]->StartOffset());
  EXPECT_EQ(3u, MarkerController().Markers()[0]->EndOffset());
}

TEST_F(DocumentMarkerControllerTest,
       RemoveMiddleOfMarkerDoRemovePartiallyOverlapping) {
  SetBodyInnerHTML("<b>abc</b>");
  GetDocument().UpdateStyleAndLayout();
  Node* b_element = GetDocument().body()->FirstChild();
  Node* text = b_element->firstChild();

  // Add marker under "abc"
  EphemeralRange marker_range =
      EphemeralRange(Position(text, 0), Position(text, 3));
  GetDocument().Markers().AddTextMatchMarker(
      marker_range, DocumentMarker::MatchStatus::kInactive);

  // Remove markers that overlap "b"
  marker_range = EphemeralRange(Position(text, 1), Position(text, 2));
  GetDocument().Markers().RemoveMarkers(
      marker_range, DocumentMarker::AllMarkers(),
      DocumentMarkerController::kRemovePartiallyOverlappingMarker);

  EXPECT_EQ(0u, MarkerController().Markers().size());
}

TEST_F(DocumentMarkerControllerTest,
       RemoveMiddleOfMarkerDontRemovePartiallyOverlapping) {
  SetBodyInnerHTML("<b>abc</b>");
  GetDocument().UpdateStyleAndLayout();
  Node* b_element = GetDocument().body()->FirstChild();
  Node* text = b_element->firstChild();

  // Add marker under "abc"
  EphemeralRange marker_range =
      EphemeralRange(Position(text, 0), Position(text, 3));
  GetDocument().Markers().AddTextMatchMarker(
      marker_range, DocumentMarker::MatchStatus::kInactive);

  // Remove portion of marker that overlaps "b"
  marker_range = EphemeralRange(Position(text, 1), Position(text, 2));
  GetDocument().Markers().RemoveMarkers(
      marker_range, DocumentMarker::AllMarkers(),
      DocumentMarkerController::kDoNotRemovePartiallyOverlappingMarker);

  EXPECT_EQ(2u, MarkerController().Markers().size());

  EXPECT_EQ(0u, MarkerController().Markers()[0]->StartOffset());
  EXPECT_EQ(1u, MarkerController().Markers()[0]->EndOffset());

  EXPECT_EQ(2u, MarkerController().Markers()[1]->StartOffset());
  EXPECT_EQ(3u, MarkerController().Markers()[1]->EndOffset());
}

TEST_F(DocumentMarkerControllerTest,
       RemoveEndOfMarkerDoRemovePartiallyOverlapping) {
  SetBodyInnerHTML("<b>abc</b>");
  GetDocument().UpdateStyleAndLayout();
  Node* b_element = GetDocument().body()->FirstChild();
  Node* text = b_element->firstChild();

  // Add marker under "abc"
  EphemeralRange marker_range =
      EphemeralRange(Position(text, 0), Position(text, 3));
  GetDocument().Markers().AddTextMatchMarker(
      marker_range, DocumentMarker::MatchStatus::kInactive);

  // Remove markers that overlap "c"
  marker_range = EphemeralRange(Position(text, 2), Position(text, 3));
  GetDocument().Markers().RemoveMarkers(
      marker_range, DocumentMarker::AllMarkers(),
      DocumentMarkerController::kRemovePartiallyOverlappingMarker);

  EXPECT_EQ(0u, MarkerController().Markers().size());
}

TEST_F(DocumentMarkerControllerTest,
       RemoveEndOfMarkerDontRemovePartiallyOverlapping) {
  SetBodyInnerHTML("<b>abc</b>");
  GetDocument().UpdateStyleAndLayout();
  Node* b_element = GetDocument().body()->FirstChild();
  Node* text = b_element->firstChild();

  // Add marker under "abc"
  EphemeralRange marker_range =
      EphemeralRange(Position(text, 0), Position(text, 3));
  GetDocument().Markers().AddTextMatchMarker(
      marker_range, DocumentMarker::MatchStatus::kInactive);

  // Remove portion of marker that overlaps "c"
  marker_range = EphemeralRange(Position(text, 2), Position(text, 3));
  GetDocument().Markers().RemoveMarkers(
      marker_range, DocumentMarker::AllMarkers(),
      DocumentMarkerController::kDoNotRemovePartiallyOverlappingMarker);

  EXPECT_EQ(1u, MarkerController().Markers().size());

  EXPECT_EQ(0u, MarkerController().Markers()[0]->StartOffset());
  EXPECT_EQ(2u, MarkerController().Markers()[0]->EndOffset());
}

}  // namespace blink
