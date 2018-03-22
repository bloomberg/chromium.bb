// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/VisibleUnits.h"

#include "bindings/core/v8/V8BindingForTesting.h"
#include "core/dom/Text.h"
#include "core/editing/PositionWithAffinity.h"
#include "core/editing/VisiblePosition.h"
#include "core/editing/testing/EditingTestBase.h"
#include "core/html/forms/TextControlElement.h"
#include "core/layout/LayoutTextFragment.h"

namespace blink {
namespace visible_units_test {

PositionWithAffinity PositionWithAffinityInDOMTree(
    Node& anchor,
    int offset,
    TextAffinity affinity = TextAffinity::kDownstream) {
  return PositionWithAffinity(CanonicalPositionOf(Position(&anchor, offset)),
                              affinity);
}

VisiblePosition CreateVisiblePositionInDOMTree(
    Node& anchor,
    int offset,
    TextAffinity affinity = TextAffinity::kDownstream) {
  return CreateVisiblePosition(Position(&anchor, offset), affinity);
}

PositionInFlatTreeWithAffinity PositionWithAffinityInFlatTree(
    Node& anchor,
    int offset,
    TextAffinity affinity = TextAffinity::kDownstream) {
  return PositionInFlatTreeWithAffinity(
      CanonicalPositionOf(PositionInFlatTree(&anchor, offset)), affinity);
}

VisiblePositionInFlatTree CreateVisiblePositionInFlatTree(
    Node& anchor,
    int offset,
    TextAffinity affinity = TextAffinity::kDownstream) {
  return CreateVisiblePosition(PositionInFlatTree(&anchor, offset), affinity);
}

class VisibleUnitsTest : public EditingTestBase {};

TEST_F(VisibleUnitsTest, absoluteCaretBoundsOf) {
  const char* body_content =
      "<p id='host'><b id='one'>11</b><b id='two'>22</b></p>";
  const char* shadow_content =
      "<div><content select=#two></content><content "
      "select=#one></content></div>";
  SetBodyContent(body_content);
  SetShadowContent(shadow_content, "host");

  Element* body = GetDocument().body();
  Element* one = body->QuerySelector("#one");

  IntRect bounds_in_dom_tree =
      AbsoluteCaretBoundsOf(CreateVisiblePosition(Position(one, 0)));
  IntRect bounds_in_flat_tree =
      AbsoluteCaretBoundsOf(CreateVisiblePosition(PositionInFlatTree(one, 0)));

  EXPECT_FALSE(bounds_in_dom_tree.IsEmpty());
  EXPECT_EQ(bounds_in_dom_tree, bounds_in_flat_tree);
}

TEST_F(VisibleUnitsTest, caretMinOffset) {
  const char* body_content = "<p id=one>one</p>";
  SetBodyContent(body_content);

  Element* one = GetDocument().getElementById("one");

  EXPECT_EQ(0, CaretMinOffset(one->firstChild()));
}

TEST_F(VisibleUnitsTest, caretMinOffsetWithFirstLetter) {
  const char* body_content =
      "<style>#one:first-letter { font-size: 200%; }</style><p id=one>one</p>";
  SetBodyContent(body_content);

  Element* one = GetDocument().getElementById("one");

  EXPECT_EQ(0, CaretMinOffset(one->firstChild()));
}

TEST_F(VisibleUnitsTest, characterAfter) {
  const char* body_content =
      "<p id='host'><b id='one'>1</b><b id='two'>22</b></p><b "
      "id='three'>333</b>";
  const char* shadow_content =
      "<b id='four'>4444</b><content select=#two></content><content "
      "select=#one></content><b id='five'>5555</b>";
  SetBodyContent(body_content);
  SetShadowContent(shadow_content, "host");

  Element* one = GetDocument().getElementById("one");
  Element* two = GetDocument().getElementById("two");

  EXPECT_EQ('2', CharacterAfter(
                     CreateVisiblePositionInDOMTree(*one->firstChild(), 1)));
  EXPECT_EQ('5', CharacterAfter(
                     CreateVisiblePositionInFlatTree(*one->firstChild(), 1)));

  EXPECT_EQ(
      0, CharacterAfter(CreateVisiblePositionInDOMTree(*two->firstChild(), 2)));
  EXPECT_EQ('1', CharacterAfter(
                     CreateVisiblePositionInFlatTree(*two->firstChild(), 2)));
}

TEST_F(VisibleUnitsTest, canonicalPositionOfWithHTMLHtmlElement) {
  const char* body_content =
      "<html><div id=one contenteditable>1</div><span id=two "
      "contenteditable=false>22</span><span id=three "
      "contenteditable=false>333</span><span id=four "
      "contenteditable=false>333</span></html>";
  SetBodyContent(body_content);

  Node* one = GetDocument().QuerySelector("#one");
  Node* two = GetDocument().QuerySelector("#two");
  Node* three = GetDocument().QuerySelector("#three");
  Node* four = GetDocument().QuerySelector("#four");
  Element* html = GetDocument().CreateRawElement(HTMLNames::htmlTag);
  // Move two, three and four into second html element.
  html->AppendChild(two);
  html->AppendChild(three);
  html->AppendChild(four);
  one->appendChild(html);
  UpdateAllLifecyclePhases();

  EXPECT_EQ(Position(),
            CanonicalPositionOf(Position(GetDocument().documentElement(), 0)));

  EXPECT_EQ(Position(one->firstChild(), 0),
            CanonicalPositionOf(Position(one, 0)));
  EXPECT_EQ(Position(one->firstChild(), 1),
            CanonicalPositionOf(Position(one, 1)));

  EXPECT_EQ(Position(one->firstChild(), 0),
            CanonicalPositionOf(Position(one->firstChild(), 0)));
  EXPECT_EQ(Position(one->firstChild(), 1),
            CanonicalPositionOf(Position(one->firstChild(), 1)));

  EXPECT_EQ(Position(html, 0), CanonicalPositionOf(Position(html, 0)));
  EXPECT_EQ(Position(html, 1), CanonicalPositionOf(Position(html, 1)));
  EXPECT_EQ(Position(html, 2), CanonicalPositionOf(Position(html, 2)));

  EXPECT_EQ(Position(two->firstChild(), 0),
            CanonicalPositionOf(Position(two, 0)));
  EXPECT_EQ(Position(two->firstChild(), 2),
            CanonicalPositionOf(Position(two, 1)));
}

// For http://crbug.com/695317
TEST_F(VisibleUnitsTest, canonicalPositionOfWithInputElement) {
  SetBodyContent("<input>123");
  Element* const input = GetDocument().QuerySelector("input");

  EXPECT_EQ(Position::BeforeNode(*input),
            CanonicalPositionOf(Position::FirstPositionInNode(
                *GetDocument().documentElement())));

  EXPECT_EQ(PositionInFlatTree::BeforeNode(*input),
            CanonicalPositionOf(PositionInFlatTree::FirstPositionInNode(
                *GetDocument().documentElement())));
}

TEST_F(VisibleUnitsTest, characterBefore) {
  const char* body_content =
      "<p id=host><b id=one>1</b><b id=two>22</b></p><b id=three>333</b>";
  const char* shadow_content =
      "<b id=four>4444</b><content select=#two></content><content "
      "select=#one></content><b id=five>5555</b>";
  SetBodyContent(body_content);
  ShadowRoot* shadow_root = SetShadowContent(shadow_content, "host");

  Node* one = GetDocument().getElementById("one")->firstChild();
  Node* two = GetDocument().getElementById("two")->firstChild();
  Node* five = shadow_root->getElementById("five")->firstChild();

  EXPECT_EQ(0, CharacterBefore(CreateVisiblePositionInDOMTree(*one, 0)));
  EXPECT_EQ('2', CharacterBefore(CreateVisiblePositionInFlatTree(*one, 0)));

  EXPECT_EQ('1', CharacterBefore(CreateVisiblePositionInDOMTree(*one, 1)));
  EXPECT_EQ('1', CharacterBefore(CreateVisiblePositionInFlatTree(*one, 1)));

  EXPECT_EQ('1', CharacterBefore(CreateVisiblePositionInDOMTree(*two, 0)));
  EXPECT_EQ('4', CharacterBefore(CreateVisiblePositionInFlatTree(*two, 0)));

  EXPECT_EQ('4', CharacterBefore(CreateVisiblePositionInDOMTree(*five, 0)));
  EXPECT_EQ('1', CharacterBefore(CreateVisiblePositionInFlatTree(*five, 0)));
}

TEST_F(VisibleUnitsTest, endOfDocument) {
  const char* body_content = "<a id=host><b id=one>1</b><b id=two>22</b></a>";
  const char* shadow_content =
      "<p><content select=#two></content></p><p><content "
      "select=#one></content></p>";
  SetBodyContent(body_content);
  SetShadowContent(shadow_content, "host");

  Element* one = GetDocument().getElementById("one");
  Element* two = GetDocument().getElementById("two");

  EXPECT_EQ(Position(two->firstChild(), 2),
            EndOfDocument(CreateVisiblePositionInDOMTree(*one->firstChild(), 0))
                .DeepEquivalent());
  EXPECT_EQ(
      PositionInFlatTree(one->firstChild(), 1),
      EndOfDocument(CreateVisiblePositionInFlatTree(*one->firstChild(), 0))
          .DeepEquivalent());

  EXPECT_EQ(Position(two->firstChild(), 2),
            EndOfDocument(CreateVisiblePositionInDOMTree(*two->firstChild(), 1))
                .DeepEquivalent());
  EXPECT_EQ(
      PositionInFlatTree(one->firstChild(), 1),
      EndOfDocument(CreateVisiblePositionInFlatTree(*two->firstChild(), 1))
          .DeepEquivalent());
}

TEST_F(VisibleUnitsTest, endOfParagraphFirstLetter) {
  SetBodyContent(
      "<style>div::first-letter { color: red }</style><div "
      "id=sample>1ab\nde</div>");

  Node* sample = GetDocument().getElementById("sample");
  Node* text = sample->firstChild();

  EXPECT_EQ(Position(text, 6),
            EndOfParagraph(CreateVisiblePositionInDOMTree(*text, 0))
                .DeepEquivalent());
  EXPECT_EQ(Position(text, 6),
            EndOfParagraph(CreateVisiblePositionInDOMTree(*text, 1))
                .DeepEquivalent());
  EXPECT_EQ(Position(text, 6),
            EndOfParagraph(CreateVisiblePositionInDOMTree(*text, 2))
                .DeepEquivalent());
  EXPECT_EQ(Position(text, 6),
            EndOfParagraph(CreateVisiblePositionInDOMTree(*text, 3))
                .DeepEquivalent());
  EXPECT_EQ(Position(text, 6),
            EndOfParagraph(CreateVisiblePositionInDOMTree(*text, 4))
                .DeepEquivalent());
  EXPECT_EQ(Position(text, 6),
            EndOfParagraph(CreateVisiblePositionInDOMTree(*text, 5))
                .DeepEquivalent());
  EXPECT_EQ(Position(text, 6),
            EndOfParagraph(CreateVisiblePositionInDOMTree(*text, 6))
                .DeepEquivalent());
}

TEST_F(VisibleUnitsTest, endOfParagraphFirstLetterPre) {
  SetBodyContent(
      "<style>pre::first-letter { color: red }</style><pre "
      "id=sample>1ab\nde</pre>");

  Node* sample = GetDocument().getElementById("sample");
  Node* text = sample->firstChild();

  EXPECT_EQ(Position(text, 3),
            EndOfParagraph(CreateVisiblePositionInDOMTree(*text, 0))
                .DeepEquivalent());
  EXPECT_EQ(Position(text, 3),
            EndOfParagraph(CreateVisiblePositionInDOMTree(*text, 1))
                .DeepEquivalent());
  EXPECT_EQ(Position(text, 3),
            EndOfParagraph(CreateVisiblePositionInDOMTree(*text, 2))
                .DeepEquivalent());
  EXPECT_EQ(Position(text, 6),
            EndOfParagraph(CreateVisiblePositionInDOMTree(*text, 3))
                .DeepEquivalent());
  EXPECT_EQ(Position(text, 6),
            EndOfParagraph(CreateVisiblePositionInDOMTree(*text, 4))
                .DeepEquivalent());
  EXPECT_EQ(Position(text, 6),
            EndOfParagraph(CreateVisiblePositionInDOMTree(*text, 5))
                .DeepEquivalent());
  EXPECT_EQ(Position(text, 6),
            EndOfParagraph(CreateVisiblePositionInDOMTree(*text, 6))
                .DeepEquivalent());
}

TEST_F(VisibleUnitsTest, endOfParagraphShadow) {
  const char* body_content =
      "<a id=host><b id=one>1</b><b id=two>22</b></a><b id=three>333</b>";
  const char* shadow_content =
      "<p><content select=#two></content></p><p><content "
      "select=#one></content></p>";
  SetBodyContent(body_content);
  SetShadowContent(shadow_content, "host");

  Element* one = GetDocument().getElementById("one");
  Element* two = GetDocument().getElementById("two");
  Element* three = GetDocument().getElementById("three");

  EXPECT_EQ(
      Position(three->firstChild(), 3),
      EndOfParagraph(CreateVisiblePositionInDOMTree(*one->firstChild(), 1))
          .DeepEquivalent());
  EXPECT_EQ(
      PositionInFlatTree(one->firstChild(), 1),
      EndOfParagraph(CreateVisiblePositionInFlatTree(*one->firstChild(), 1))
          .DeepEquivalent());

  EXPECT_EQ(
      Position(three->firstChild(), 3),
      EndOfParagraph(CreateVisiblePositionInDOMTree(*two->firstChild(), 2))
          .DeepEquivalent());
  EXPECT_EQ(
      PositionInFlatTree(two->firstChild(), 2),
      EndOfParagraph(CreateVisiblePositionInFlatTree(*two->firstChild(), 2))
          .DeepEquivalent());
}

TEST_F(VisibleUnitsTest, endOfParagraphSimple) {
  SetBodyContent("<div id=sample>1ab\nde</div>");

  Node* sample = GetDocument().getElementById("sample");
  Node* text = sample->firstChild();

  EXPECT_EQ(Position(text, 6),
            EndOfParagraph(CreateVisiblePositionInDOMTree(*text, 0))
                .DeepEquivalent());
  EXPECT_EQ(Position(text, 6),
            EndOfParagraph(CreateVisiblePositionInDOMTree(*text, 1))
                .DeepEquivalent());
  EXPECT_EQ(Position(text, 6),
            EndOfParagraph(CreateVisiblePositionInDOMTree(*text, 2))
                .DeepEquivalent());
  EXPECT_EQ(Position(text, 6),
            EndOfParagraph(CreateVisiblePositionInDOMTree(*text, 3))
                .DeepEquivalent());
  EXPECT_EQ(Position(text, 6),
            EndOfParagraph(CreateVisiblePositionInDOMTree(*text, 4))
                .DeepEquivalent());
  EXPECT_EQ(Position(text, 6),
            EndOfParagraph(CreateVisiblePositionInDOMTree(*text, 5))
                .DeepEquivalent());
  EXPECT_EQ(Position(text, 6),
            EndOfParagraph(CreateVisiblePositionInDOMTree(*text, 6))
                .DeepEquivalent());
}

TEST_F(VisibleUnitsTest, endOfParagraphSimplePre) {
  SetBodyContent("<pre id=sample>1ab\nde</pre>");

  Node* sample = GetDocument().getElementById("sample");
  Node* text = sample->firstChild();

  EXPECT_EQ(Position(text, 3),
            EndOfParagraph(CreateVisiblePositionInDOMTree(*text, 0))
                .DeepEquivalent());
  EXPECT_EQ(Position(text, 3),
            EndOfParagraph(CreateVisiblePositionInDOMTree(*text, 1))
                .DeepEquivalent());
  EXPECT_EQ(Position(text, 3),
            EndOfParagraph(CreateVisiblePositionInDOMTree(*text, 2))
                .DeepEquivalent());
  EXPECT_EQ(Position(text, 3),
            EndOfParagraph(CreateVisiblePositionInDOMTree(*text, 3))
                .DeepEquivalent());
  EXPECT_EQ(Position(text, 6),
            EndOfParagraph(CreateVisiblePositionInDOMTree(*text, 4))
                .DeepEquivalent());
  EXPECT_EQ(Position(text, 6),
            EndOfParagraph(CreateVisiblePositionInDOMTree(*text, 5))
                .DeepEquivalent());
  EXPECT_EQ(Position(text, 6),
            EndOfParagraph(CreateVisiblePositionInDOMTree(*text, 6))
                .DeepEquivalent());
}

TEST_F(VisibleUnitsTest, isEndOfEditableOrNonEditableContent) {
  const char* body_content =
      "<a id=host><b id=one contenteditable>1</b><b id=two>22</b></a>";
  const char* shadow_content =
      "<content select=#two></content></p><p><content select=#one></content>";
  SetBodyContent(body_content);
  SetShadowContent(shadow_content, "host");

  Element* one = GetDocument().getElementById("one");
  Element* two = GetDocument().getElementById("two");

  EXPECT_FALSE(IsEndOfEditableOrNonEditableContent(
      CreateVisiblePositionInDOMTree(*one->firstChild(), 1)));
  EXPECT_TRUE(IsEndOfEditableOrNonEditableContent(
      CreateVisiblePositionInFlatTree(*one->firstChild(), 1)));

  EXPECT_TRUE(IsEndOfEditableOrNonEditableContent(
      CreateVisiblePositionInDOMTree(*two->firstChild(), 2)));
  EXPECT_FALSE(IsEndOfEditableOrNonEditableContent(
      CreateVisiblePositionInFlatTree(*two->firstChild(), 2)));
}

TEST_F(VisibleUnitsTest, isEndOfEditableOrNonEditableContentWithInput) {
  const char* body_content = "<input id=sample value=ab>cde";
  SetBodyContent(body_content);

  Node* text = ToTextControlElement(GetDocument().getElementById("sample"))
                   ->InnerEditorElement()
                   ->firstChild();

  EXPECT_FALSE(IsEndOfEditableOrNonEditableContent(
      CreateVisiblePositionInDOMTree(*text, 0)));
  EXPECT_FALSE(IsEndOfEditableOrNonEditableContent(
      CreateVisiblePositionInFlatTree(*text, 0)));

  EXPECT_FALSE(IsEndOfEditableOrNonEditableContent(
      CreateVisiblePositionInDOMTree(*text, 1)));
  EXPECT_FALSE(IsEndOfEditableOrNonEditableContent(
      CreateVisiblePositionInFlatTree(*text, 1)));

  EXPECT_TRUE(IsEndOfEditableOrNonEditableContent(
      CreateVisiblePositionInDOMTree(*text, 2)));
  EXPECT_TRUE(IsEndOfEditableOrNonEditableContent(
      CreateVisiblePositionInFlatTree(*text, 2)));
}

TEST_F(VisibleUnitsTest, isEndOfParagraph) {
  const char* body_content =
      "<a id=host><b id=one>1</b><b id=two>22</b></a><b id=three>333</b>";
  const char* shadow_content =
      "<p><content select=#two></content></p><p><content "
      "select=#one></content></p>";
  SetBodyContent(body_content);
  SetShadowContent(shadow_content, "host");

  Node* one = GetDocument().getElementById("one")->firstChild();
  Node* two = GetDocument().getElementById("two")->firstChild();
  Node* three = GetDocument().getElementById("three")->firstChild();

  EXPECT_FALSE(IsEndOfParagraph(CreateVisiblePositionInDOMTree(*one, 0)));
  EXPECT_FALSE(IsEndOfParagraph(CreateVisiblePositionInFlatTree(*one, 0)));

  EXPECT_FALSE(IsEndOfParagraph(CreateVisiblePositionInDOMTree(*one, 1)));
  EXPECT_TRUE(IsEndOfParagraph(CreateVisiblePositionInFlatTree(*one, 1)));

  EXPECT_FALSE(IsEndOfParagraph(CreateVisiblePositionInDOMTree(*two, 2)));
  EXPECT_TRUE(IsEndOfParagraph(CreateVisiblePositionInFlatTree(*two, 2)));

  EXPECT_FALSE(IsEndOfParagraph(CreateVisiblePositionInDOMTree(*three, 0)));
  EXPECT_FALSE(IsEndOfParagraph(CreateVisiblePositionInFlatTree(*three, 0)));

  EXPECT_TRUE(IsEndOfParagraph(CreateVisiblePositionInDOMTree(*three, 3)));
  EXPECT_TRUE(IsEndOfParagraph(CreateVisiblePositionInFlatTree(*three, 3)));
}

TEST_F(VisibleUnitsTest, isStartOfParagraph) {
  const char* body_content =
      "<b id=zero>0</b><a id=host><b id=one>1</b><b id=two>22</b></a><b "
      "id=three>333</b>";
  const char* shadow_content =
      "<p><content select=#two></content></p><p><content "
      "select=#one></content></p>";
  SetBodyContent(body_content);
  SetShadowContent(shadow_content, "host");

  Node* zero = GetDocument().getElementById("zero")->firstChild();
  Node* one = GetDocument().getElementById("one")->firstChild();
  Node* two = GetDocument().getElementById("two")->firstChild();
  Node* three = GetDocument().getElementById("three")->firstChild();

  EXPECT_TRUE(IsStartOfParagraph(CreateVisiblePositionInDOMTree(*zero, 0)));
  EXPECT_TRUE(IsStartOfParagraph(CreateVisiblePositionInFlatTree(*zero, 0)));

  EXPECT_FALSE(IsStartOfParagraph(CreateVisiblePositionInDOMTree(*one, 0)));
  EXPECT_TRUE(IsStartOfParagraph(CreateVisiblePositionInFlatTree(*one, 0)));

  EXPECT_FALSE(IsStartOfParagraph(CreateVisiblePositionInDOMTree(*one, 1)));
  EXPECT_FALSE(IsStartOfParagraph(CreateVisiblePositionInFlatTree(*one, 1)));

  EXPECT_FALSE(IsStartOfParagraph(CreateVisiblePositionInDOMTree(*two, 0)));
  EXPECT_TRUE(IsStartOfParagraph(CreateVisiblePositionInFlatTree(*two, 0)));

  EXPECT_FALSE(IsStartOfParagraph(CreateVisiblePositionInDOMTree(*three, 0)));
  EXPECT_TRUE(IsStartOfParagraph(CreateVisiblePositionInFlatTree(*three, 0)));
}

TEST_F(VisibleUnitsTest, isVisuallyEquivalentCandidateWithHTMLHtmlElement) {
  const char* body_content =
      "<html><div id=one contenteditable>1</div><span id=two "
      "contenteditable=false>22</span><span id=three "
      "contenteditable=false>333</span><span id=four "
      "contenteditable=false>333</span></html>";
  SetBodyContent(body_content);

  Node* one = GetDocument().QuerySelector("#one");
  Node* two = GetDocument().QuerySelector("#two");
  Node* three = GetDocument().QuerySelector("#three");
  Node* four = GetDocument().QuerySelector("#four");
  Element* html = GetDocument().CreateRawElement(HTMLNames::htmlTag);
  // Move two, three and four into second html element.
  html->AppendChild(two);
  html->AppendChild(three);
  html->AppendChild(four);
  one->appendChild(html);
  UpdateAllLifecyclePhases();

  EXPECT_FALSE(IsVisuallyEquivalentCandidate(
      Position(GetDocument().documentElement(), 0)));

  EXPECT_FALSE(IsVisuallyEquivalentCandidate(Position(one, 0)));
  EXPECT_FALSE(IsVisuallyEquivalentCandidate(Position(one, 1)));

  EXPECT_TRUE(IsVisuallyEquivalentCandidate(Position(one->firstChild(), 0)));
  EXPECT_TRUE(IsVisuallyEquivalentCandidate(Position(one->firstChild(), 1)));

  EXPECT_TRUE(IsVisuallyEquivalentCandidate(Position(html, 0)));
  EXPECT_TRUE(IsVisuallyEquivalentCandidate(Position(html, 1)));
  EXPECT_TRUE(IsVisuallyEquivalentCandidate(Position(html, 2)));

  EXPECT_FALSE(IsVisuallyEquivalentCandidate(Position(two, 0)));
  EXPECT_FALSE(IsVisuallyEquivalentCandidate(Position(two, 1)));
}

TEST_F(VisibleUnitsTest, isVisuallyEquivalentCandidateWithDocument) {
  UpdateAllLifecyclePhases();

  EXPECT_FALSE(IsVisuallyEquivalentCandidate(Position(&GetDocument(), 0)));
}

TEST_F(VisibleUnitsTest, mostBackwardCaretPositionAfterAnchor) {
  const char* body_content =
      "<p id='host'><b id='one'>1</b></p><b id='two'>22</b>";
  const char* shadow_content =
      "<b id='two'>22</b><content select=#one></content><b id='three'>333</b>";
  SetBodyContent(body_content);
  SetShadowContent(shadow_content, "host");

  Element* host = GetDocument().getElementById("host");

  EXPECT_EQ(Position::LastPositionInNode(*host),
            MostForwardCaretPosition(Position::AfterNode(*host)));
  EXPECT_EQ(PositionInFlatTree::LastPositionInNode(*host),
            MostForwardCaretPosition(PositionInFlatTree::AfterNode(*host)));
}

TEST_F(VisibleUnitsTest, mostBackwardCaretPositionFirstLetter) {
  // Note: first-letter pseudo element contains letter and punctuations.
  const char* body_content =
      "<style>p:first-letter {color:red;}</style><p id=sample> (2)45 </p>";
  SetBodyContent(body_content);

  Node* sample = GetDocument().getElementById("sample")->firstChild();

  EXPECT_EQ(Position(sample->parentNode(), 0),
            MostBackwardCaretPosition(Position(sample, 0)));
  EXPECT_EQ(Position(sample->parentNode(), 0),
            MostBackwardCaretPosition(Position(sample, 1)));
  EXPECT_EQ(Position(sample, 2),
            MostBackwardCaretPosition(Position(sample, 2)));
  EXPECT_EQ(Position(sample, 3),
            MostBackwardCaretPosition(Position(sample, 3)));
  EXPECT_EQ(Position(sample, 4),
            MostBackwardCaretPosition(Position(sample, 4)));
  EXPECT_EQ(Position(sample, 5),
            MostBackwardCaretPosition(Position(sample, 5)));
  EXPECT_EQ(Position(sample, 6),
            MostBackwardCaretPosition(Position(sample, 6)));
  EXPECT_EQ(Position(sample, 6),
            MostBackwardCaretPosition(Position(sample, 7)));
  EXPECT_EQ(Position(sample, 6),
            MostBackwardCaretPosition(
                Position::LastPositionInNode(*sample->parentNode())));
  EXPECT_EQ(
      Position(sample, 6),
      MostBackwardCaretPosition(Position::AfterNode(*sample->parentNode())));
  EXPECT_EQ(Position::LastPositionInNode(*GetDocument().body()),
            MostBackwardCaretPosition(
                Position::LastPositionInNode(*GetDocument().body())));
}

TEST_F(VisibleUnitsTest, mostBackwardCaretPositionFirstLetterSplit) {
  V8TestingScope scope;

  const char* body_content =
      "<style>p:first-letter {color:red;}</style><p id=sample>abc</p>";
  SetBodyContent(body_content);

  Node* sample = GetDocument().getElementById("sample");
  Node* first_letter = sample->firstChild();
  // Split "abc" into "a" "bc"
  Text* remaining = ToText(first_letter)->splitText(1, ASSERT_NO_EXCEPTION);
  UpdateAllLifecyclePhases();

  EXPECT_EQ(Position(sample, 0),
            MostBackwardCaretPosition(Position(first_letter, 0)));
  EXPECT_EQ(Position(first_letter, 1),
            MostBackwardCaretPosition(Position(first_letter, 1)));
  EXPECT_EQ(Position(first_letter, 1),
            MostBackwardCaretPosition(Position(remaining, 0)));
  EXPECT_EQ(Position(remaining, 1),
            MostBackwardCaretPosition(Position(remaining, 1)));
  EXPECT_EQ(Position(remaining, 2),
            MostBackwardCaretPosition(Position(remaining, 2)));
  EXPECT_EQ(Position(remaining, 2),
            MostBackwardCaretPosition(Position::LastPositionInNode(*sample)));
  EXPECT_EQ(Position(remaining, 2),
            MostBackwardCaretPosition(Position::AfterNode(*sample)));
}

TEST_F(VisibleUnitsTest, mostForwardCaretPositionAfterAnchor) {
  const char* body_content = "<p id='host'><b id='one'>1</b></p>";
  const char* shadow_content =
      "<b id='two'>22</b><content select=#one></content><b id='three'>333</b>";
  SetBodyContent(body_content);
  ShadowRoot* shadow_root = SetShadowContent(shadow_content, "host");
  UpdateAllLifecyclePhases();

  Element* host = GetDocument().getElementById("host");
  Element* one = GetDocument().getElementById("one");
  Element* three = shadow_root->getElementById("three");

  EXPECT_EQ(Position(one->firstChild(), 1),
            MostBackwardCaretPosition(Position::AfterNode(*host)));
  EXPECT_EQ(PositionInFlatTree(three->firstChild(), 3),
            MostBackwardCaretPosition(PositionInFlatTree::AfterNode(*host)));
}

TEST_F(VisibleUnitsTest, mostForwardCaretPositionFirstLetter) {
  // Note: first-letter pseudo element contains letter and punctuations.
  const char* body_content =
      "<style>p:first-letter {color:red;}</style><p id=sample> (2)45 </p>";
  SetBodyContent(body_content);

  Node* sample = GetDocument().getElementById("sample")->firstChild();

  EXPECT_EQ(Position(GetDocument().body(), 0),
            MostForwardCaretPosition(
                Position::FirstPositionInNode(*GetDocument().body())));
  EXPECT_EQ(
      Position(sample, 1),
      MostForwardCaretPosition(Position::BeforeNode(*sample->parentNode())));
  EXPECT_EQ(Position(sample, 1),
            MostForwardCaretPosition(
                Position::FirstPositionInNode(*sample->parentNode())));
  EXPECT_EQ(Position(sample, 1), MostForwardCaretPosition(Position(sample, 0)));
  EXPECT_EQ(Position(sample, 1), MostForwardCaretPosition(Position(sample, 1)));
  EXPECT_EQ(Position(sample, 2), MostForwardCaretPosition(Position(sample, 2)));
  EXPECT_EQ(Position(sample, 3), MostForwardCaretPosition(Position(sample, 3)));
  EXPECT_EQ(Position(sample, 4), MostForwardCaretPosition(Position(sample, 4)));
  EXPECT_EQ(Position(sample, 5), MostForwardCaretPosition(Position(sample, 5)));
  EXPECT_EQ(Position(sample, 7), MostForwardCaretPosition(Position(sample, 6)));
  EXPECT_EQ(Position(sample, 7), MostForwardCaretPosition(Position(sample, 7)));
}

TEST_F(VisibleUnitsTest, nextPositionOf) {
  const char* body_content =
      "<b id=zero>0</b><p id=host><b id=one>1</b><b id=two>22</b></p><b "
      "id=three>333</b>";
  const char* shadow_content =
      "<b id=four>4444</b><content select=#two></content><content "
      "select=#one></content><b id=five>55555</b>";
  SetBodyContent(body_content);
  ShadowRoot* shadow_root = SetShadowContent(shadow_content, "host");

  Element* zero = GetDocument().getElementById("zero");
  Element* one = GetDocument().getElementById("one");
  Element* two = GetDocument().getElementById("two");
  Element* three = GetDocument().getElementById("three");
  Element* four = shadow_root->getElementById("four");
  Element* five = shadow_root->getElementById("five");

  EXPECT_EQ(Position(one->firstChild(), 0),
            NextPositionOf(CreateVisiblePosition(Position(zero, 1)))
                .DeepEquivalent());
  EXPECT_EQ(PositionInFlatTree(four->firstChild(), 0),
            NextPositionOf(CreateVisiblePosition(PositionInFlatTree(zero, 1)))
                .DeepEquivalent());

  EXPECT_EQ(
      Position(one->firstChild(), 1),
      NextPositionOf(CreateVisiblePosition(Position(one, 0))).DeepEquivalent());
  EXPECT_EQ(PositionInFlatTree(one->firstChild(), 1),
            NextPositionOf(CreateVisiblePosition(PositionInFlatTree(one, 0)))
                .DeepEquivalent());

  EXPECT_EQ(
      Position(two->firstChild(), 1),
      NextPositionOf(CreateVisiblePosition(Position(one, 1))).DeepEquivalent());
  EXPECT_EQ(PositionInFlatTree(five->firstChild(), 1),
            NextPositionOf(CreateVisiblePosition(PositionInFlatTree(one, 1)))
                .DeepEquivalent());

  EXPECT_EQ(
      Position(three->firstChild(), 0),
      NextPositionOf(CreateVisiblePosition(Position(two, 2))).DeepEquivalent());
  EXPECT_EQ(PositionInFlatTree(one->firstChild(), 1),
            NextPositionOf(CreateVisiblePosition(PositionInFlatTree(two, 2)))
                .DeepEquivalent());
}

TEST_F(VisibleUnitsTest, previousPositionOf) {
  const char* body_content =
      "<b id=zero>0</b><p id=host><b id=one>1</b><b id=two>22</b></p><b "
      "id=three>333</b>";
  const char* shadow_content =
      "<b id=four>4444</b><content select=#two></content><content "
      "select=#one></content><b id=five>55555</b>";
  SetBodyContent(body_content);
  ShadowRoot* shadow_root = SetShadowContent(shadow_content, "host");

  Node* zero = GetDocument().getElementById("zero")->firstChild();
  Node* one = GetDocument().getElementById("one")->firstChild();
  Node* two = GetDocument().getElementById("two")->firstChild();
  Node* three = GetDocument().getElementById("three")->firstChild();
  Node* four = shadow_root->getElementById("four")->firstChild();
  Node* five = shadow_root->getElementById("five")->firstChild();

  EXPECT_EQ(Position(zero, 0),
            PreviousPositionOf(CreateVisiblePosition(Position(zero, 1)))
                .DeepEquivalent());
  EXPECT_EQ(
      PositionInFlatTree(zero, 0),
      PreviousPositionOf(CreateVisiblePosition(PositionInFlatTree(zero, 1)))
          .DeepEquivalent());

  EXPECT_EQ(Position(zero, 1),
            PreviousPositionOf(CreateVisiblePosition(Position(one, 0)))
                .DeepEquivalent());
  EXPECT_EQ(
      PositionInFlatTree(two, 1),
      PreviousPositionOf(CreateVisiblePosition(PositionInFlatTree(one, 0)))
          .DeepEquivalent());

  EXPECT_EQ(Position(one, 0),
            PreviousPositionOf(CreateVisiblePosition(Position(one, 1)))
                .DeepEquivalent());
  EXPECT_EQ(
      PositionInFlatTree(two, 2),
      PreviousPositionOf(CreateVisiblePosition(PositionInFlatTree(one, 1)))
          .DeepEquivalent());

  EXPECT_EQ(Position(one, 0),
            PreviousPositionOf(CreateVisiblePosition(Position(two, 0)))
                .DeepEquivalent());
  EXPECT_EQ(
      PositionInFlatTree(four, 3),
      PreviousPositionOf(CreateVisiblePosition(PositionInFlatTree(two, 0)))
          .DeepEquivalent());

  // DOM tree to shadow tree
  EXPECT_EQ(Position(two, 2),
            PreviousPositionOf(CreateVisiblePosition(Position(three, 0)))
                .DeepEquivalent());
  EXPECT_EQ(
      PositionInFlatTree(five, 5),
      PreviousPositionOf(CreateVisiblePosition(PositionInFlatTree(three, 0)))
          .DeepEquivalent());

  // Shadow tree to DOM tree
  EXPECT_EQ(Position(),
            PreviousPositionOf(CreateVisiblePosition(Position(four, 0)))
                .DeepEquivalent());
  EXPECT_EQ(
      PositionInFlatTree(zero, 1),
      PreviousPositionOf(CreateVisiblePosition(PositionInFlatTree(four, 0)))
          .DeepEquivalent());

  // Note: Canonicalization maps (five, 0) to (four, 4) in DOM tree and
  // (one, 1) in flat tree.
  EXPECT_EQ(Position(four, 4),
            PreviousPositionOf(CreateVisiblePosition(Position(five, 1)))
                .DeepEquivalent());
  EXPECT_EQ(
      PositionInFlatTree(one, 1),
      PreviousPositionOf(CreateVisiblePosition(PositionInFlatTree(five, 1)))
          .DeepEquivalent());
}

TEST_F(VisibleUnitsTest, previousPositionOfOneCharPerLine) {
  const char* body_content =
      "<div id=sample style='font-size: 500px'>A&#x714a;&#xfa67;</div>";
  SetBodyContent(body_content);

  Node* sample = GetDocument().getElementById("sample")->firstChild();

  // In case of each line has one character, VisiblePosition are:
  // [C,Dn]   [C,Up]  [B, Dn]   [B, Up]
  //  A        A       A         A|
  //  B        B|     |B         B
  // |C        C       C         C
  EXPECT_EQ(PositionWithAffinity(Position(sample, 1)),
            PreviousPositionOf(CreateVisiblePosition(Position(sample, 2)))
                .ToPositionWithAffinity());
  EXPECT_EQ(PositionWithAffinity(Position(sample, 1)),
            PreviousPositionOf(CreateVisiblePosition(Position(sample, 2),
                                                     TextAffinity::kUpstream))
                .ToPositionWithAffinity());
}

TEST_F(VisibleUnitsTest, previousPositionOfNoPreviousPosition) {
  SetBodyContent(
      "<span contenteditable='true'>"
      "<span> </span>"
      " "  // This whitespace causes no previous position.
      "<div id='anchor'> bar</div>"
      "</span>");
  const Position position(GetDocument().getElementById("anchor")->firstChild(),
                          1);
  EXPECT_EQ(
      Position(),
      PreviousPositionOf(CreateVisiblePosition(position)).DeepEquivalent());
}

TEST_F(VisibleUnitsTest, rendersInDifferentPositionAfterAnchor) {
  const char* body_content = "<p id='sample'>00</p>";
  SetBodyContent(body_content);
  Element* sample = GetDocument().getElementById("sample");

  EXPECT_FALSE(RendersInDifferentPosition(Position(), Position()));
  EXPECT_FALSE(
      RendersInDifferentPosition(Position(), Position::AfterNode(*sample)))
      << "if one of position is null, the reuslt is false.";
  EXPECT_FALSE(RendersInDifferentPosition(Position::AfterNode(*sample),
                                          Position(sample, 1)));
  EXPECT_FALSE(RendersInDifferentPosition(Position::LastPositionInNode(*sample),
                                          Position(sample, 1)));
}

TEST_F(VisibleUnitsTest, rendersInDifferentPositionAfterAnchorWithHidden) {
  const char* body_content =
      "<p><span id=one>11</span><span id=two style='display:none'>  "
      "</span></p>";
  SetBodyContent(body_content);
  Element* one = GetDocument().getElementById("one");
  Element* two = GetDocument().getElementById("two");

  EXPECT_TRUE(RendersInDifferentPosition(Position::LastPositionInNode(*one),
                                         Position(two, 0)))
      << "two doesn't have layout object";
}

TEST_F(VisibleUnitsTest,
       rendersInDifferentPositionAfterAnchorWithDifferentLayoutObjects) {
  const char* body_content =
      "<p><span id=one>11</span><span id=two>  </span></p>";
  SetBodyContent(body_content);
  Element* one = GetDocument().getElementById("one");
  Element* two = GetDocument().getElementById("two");

  EXPECT_FALSE(RendersInDifferentPosition(Position::LastPositionInNode(*one),
                                          Position(two, 0)));
  EXPECT_FALSE(RendersInDifferentPosition(Position::LastPositionInNode(*one),
                                          Position(two, 1)))
      << "width of two is zero since contents is collapsed whitespaces";
}

TEST_F(VisibleUnitsTest, renderedOffset) {
  const char* body_content =
      "<div contenteditable><span id='sample1'>1</span><span "
      "id='sample2'>22</span></div>";
  SetBodyContent(body_content);
  Element* sample1 = GetDocument().getElementById("sample1");
  Element* sample2 = GetDocument().getElementById("sample2");

  EXPECT_FALSE(
      RendersInDifferentPosition(Position::AfterNode(*sample1->firstChild()),
                                 Position(sample2->firstChild(), 0)));
  EXPECT_FALSE(RendersInDifferentPosition(
      Position::LastPositionInNode(*sample1->firstChild()),
      Position(sample2->firstChild(), 0)));
}

TEST_F(VisibleUnitsTest, startOfDocument) {
  const char* body_content = "<a id=host><b id=one>1</b><b id=two>22</b></a>";
  const char* shadow_content =
      "<p><content select=#two></content></p><p><content "
      "select=#one></content></p>";
  SetBodyContent(body_content);
  SetShadowContent(shadow_content, "host");

  Node* one = GetDocument().getElementById("one")->firstChild();
  Node* two = GetDocument().getElementById("two")->firstChild();

  EXPECT_EQ(Position(one, 0),
            StartOfDocument(CreateVisiblePositionInDOMTree(*one, 0))
                .DeepEquivalent());
  EXPECT_EQ(PositionInFlatTree(two, 0),
            StartOfDocument(CreateVisiblePositionInFlatTree(*one, 0))
                .DeepEquivalent());

  EXPECT_EQ(Position(one, 0),
            StartOfDocument(CreateVisiblePositionInDOMTree(*two, 1))
                .DeepEquivalent());
  EXPECT_EQ(PositionInFlatTree(two, 0),
            StartOfDocument(CreateVisiblePositionInFlatTree(*two, 1))
                .DeepEquivalent());
}

TEST_F(VisibleUnitsTest, startOfParagraph) {
  const char* body_content =
      "<b id=zero>0</b><a id=host><b id=one>1</b><b id=two>22</b></a><b "
      "id=three>333</b>";
  const char* shadow_content =
      "<p><content select=#two></content></p><p><content "
      "select=#one></content></p>";
  SetBodyContent(body_content);
  SetShadowContent(shadow_content, "host");

  Node* zero = GetDocument().getElementById("zero")->firstChild();
  Node* one = GetDocument().getElementById("one")->firstChild();
  Node* two = GetDocument().getElementById("two")->firstChild();
  Node* three = GetDocument().getElementById("three")->firstChild();

  EXPECT_EQ(Position(zero, 0),
            StartOfParagraph(CreateVisiblePositionInDOMTree(*one, 1))
                .DeepEquivalent());
  EXPECT_EQ(PositionInFlatTree(one, 0),
            StartOfParagraph(CreateVisiblePositionInFlatTree(*one, 1))
                .DeepEquivalent());

  EXPECT_EQ(Position(zero, 0),
            StartOfParagraph(CreateVisiblePositionInDOMTree(*two, 2))
                .DeepEquivalent());
  EXPECT_EQ(PositionInFlatTree(two, 0),
            StartOfParagraph(CreateVisiblePositionInFlatTree(*two, 2))
                .DeepEquivalent());

  // DOM version of |startOfParagraph()| doesn't take account contents in
  // shadow tree.
  EXPECT_EQ(Position(zero, 0),
            StartOfParagraph(CreateVisiblePositionInDOMTree(*three, 2))
                .DeepEquivalent());
  EXPECT_EQ(PositionInFlatTree(three, 0),
            StartOfParagraph(CreateVisiblePositionInFlatTree(*three, 2))
                .DeepEquivalent());

  // crbug.com/563777. startOfParagraph() unexpectedly returned a null
  // position with nested editable <BODY>s.
  Element* root = GetDocument().documentElement();
  root->SetInnerHTMLFromString(
      "<style>* { display:inline-table; }</style><body "
      "contenteditable=true><svg><svg><foreignObject>abc<svg></svg></"
      "foreignObject></svg></svg></body>");
  Element* old_body = GetDocument().body();
  root->SetInnerHTMLFromString(
      "<body contenteditable=true><svg><foreignObject><style>def</style>");
  DCHECK_NE(old_body, GetDocument().body());
  Node* foreign_object = GetDocument().body()->firstChild()->firstChild();
  foreign_object->insertBefore(old_body, foreign_object->firstChild());
  Node* style_text = foreign_object->lastChild()->firstChild();
  DCHECK(style_text->IsTextNode()) << style_text;
  UpdateAllLifecyclePhases();

  EXPECT_FALSE(StartOfParagraph(CreateVisiblePosition(Position(style_text, 0)))
                   .IsNull());
}

TEST_F(VisibleUnitsTest,
       endsOfNodeAreVisuallyDistinctPositionsWithInvisibleChild) {
  // Repro case of crbug.com/582247
  const char* body_content =
      "<button> </button><script>document.designMode = 'on'</script>";
  SetBodyContent(body_content);

  Node* button = GetDocument().QuerySelector("button");
  EXPECT_TRUE(EndsOfNodeAreVisuallyDistinctPositions(button));
}

TEST_F(VisibleUnitsTest,
       endsOfNodeAreVisuallyDistinctPositionsWithEmptyLayoutChild) {
  // Repro case of crbug.com/584030
  const char* body_content =
      "<button><rt><script>document.designMode = 'on'</script></rt></button>";
  SetBodyContent(body_content);

  Node* button = GetDocument().QuerySelector("button");
  EXPECT_TRUE(EndsOfNodeAreVisuallyDistinctPositions(button));
}

// Repro case of crbug.com/680428
TEST_F(VisibleUnitsTest, localSelectionRectOfPositionTemplateNotCrash) {
  SetBodyContent("<div>foo<img></div>");

  Node* node = GetDocument().QuerySelector("img");
  IntRect rect =
      AbsoluteSelectionBoundsOf(VisiblePosition::Create(PositionWithAffinity(
          Position(node, PositionAnchorType::kAfterChildren))));
  EXPECT_FALSE(rect.IsEmpty());
}

// Regression test for crbug.com/675429
TEST_F(VisibleUnitsTest,
       canonicalizationWithCollapsedSpaceAndIsolatedCombiningCharacter) {
  SetBodyContent("<p>  &#x20E3;</p>");  // Leading space is necessary

  Node* paragraph = GetDocument().QuerySelector("p");
  Node* text = paragraph->firstChild();
  Position start = CanonicalPositionOf(Position::BeforeNode(*paragraph));
  EXPECT_EQ(Position(text, 2), start);
}

static unsigned MockBoundarySearch(const UChar*,
                                   unsigned,
                                   unsigned,
                                   BoundarySearchContextAvailability,
                                   bool&) {
  return true;
}

// Regression test for crbug.com/788661
TEST_F(VisibleUnitsTest, NextBoundaryOfEditableTableWithLeadingSpaceInOutput) {
  VisiblePosition pos = CreateVisiblePosition(SetCaretTextToBody(
      // The leading whitespace is necessary for bug repro
      "<output> <table contenteditable><!--|--></table></output>"));
  Position result = NextBoundary(pos, MockBoundarySearch);
  EXPECT_EQ("<output> <table contenteditable>|</table></output>",
            GetCaretTextFromBody(result));
}

}  // namespace visible_units_test
}  // namespace blink
