// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/VisibleUnits.h"

#include <ostream>  // NOLINT
#include "bindings/core/v8/V8BindingForTesting.h"
#include "core/dom/Text.h"
#include "core/editing/EditingTestBase.h"
#include "core/editing/VisiblePosition.h"
#include "core/html/TextControlElement.h"
#include "core/layout/LayoutTextFragment.h"
#include "core/layout/line/InlineTextBox.h"

namespace blink {

namespace {

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

}  // namespace

std::ostream& operator<<(std::ostream& ostream,
                         const InlineBoxPosition& inline_box_position) {
  if (!inline_box_position.inline_box)
    return ostream << "null";
  return ostream
         << inline_box_position.inline_box->GetLineLayoutItem().GetNode() << "@"
         << inline_box_position.offset_in_box;
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

TEST_F(VisibleUnitsTest, associatedLayoutObjectOfFirstLetterPunctuations) {
  const char* body_content =
      "<style>p:first-letter {color:red;}</style><p id=sample>(a)bc</p>";
  SetBodyContent(body_content);

  Node* sample = GetDocument().getElementById("sample");
  Node* text = sample->firstChild();

  LayoutTextFragment* layout_object0 =
      ToLayoutTextFragment(AssociatedLayoutObjectOf(*text, 0));
  EXPECT_FALSE(layout_object0->IsRemainingTextLayoutObject());

  LayoutTextFragment* layout_object1 =
      ToLayoutTextFragment(AssociatedLayoutObjectOf(*text, 1));
  EXPECT_EQ(layout_object0, layout_object1)
      << "A character 'a' should be part of first letter.";

  LayoutTextFragment* layout_object2 =
      ToLayoutTextFragment(AssociatedLayoutObjectOf(*text, 2));
  EXPECT_EQ(layout_object0, layout_object2)
      << "close parenthesis should be part of first letter.";

  LayoutTextFragment* layout_object3 =
      ToLayoutTextFragment(AssociatedLayoutObjectOf(*text, 3));
  EXPECT_TRUE(layout_object3->IsRemainingTextLayoutObject());
}

TEST_F(VisibleUnitsTest, associatedLayoutObjectOfFirstLetterSplit) {
  V8TestingScope scope;

  const char* body_content =
      "<style>p:first-letter {color:red;}</style><p id=sample>abc</p>";
  SetBodyContent(body_content);

  Node* sample = GetDocument().getElementById("sample");
  Node* first_letter = sample->firstChild();
  // Split "abc" into "a" "bc"
  ToText(first_letter)->splitText(1, ASSERT_NO_EXCEPTION);
  UpdateAllLifecyclePhases();

  LayoutTextFragment* layout_object0 =
      ToLayoutTextFragment(AssociatedLayoutObjectOf(*first_letter, 0));
  EXPECT_FALSE(layout_object0->IsRemainingTextLayoutObject());

  LayoutTextFragment* layout_object1 =
      ToLayoutTextFragment(AssociatedLayoutObjectOf(*first_letter, 1));
  EXPECT_EQ(layout_object0, layout_object1);
}

TEST_F(VisibleUnitsTest,
       associatedLayoutObjectOfFirstLetterWithTrailingWhitespace) {
  const char* body_content =
      "<style>div:first-letter {color:red;}</style><div id=sample>a\n "
      "<div></div></div>";
  SetBodyContent(body_content);

  Node* sample = GetDocument().getElementById("sample");
  Node* text = sample->firstChild();

  LayoutTextFragment* layout_object0 =
      ToLayoutTextFragment(AssociatedLayoutObjectOf(*text, 0));
  EXPECT_FALSE(layout_object0->IsRemainingTextLayoutObject());

  LayoutTextFragment* layout_object1 =
      ToLayoutTextFragment(AssociatedLayoutObjectOf(*text, 1));
  EXPECT_TRUE(layout_object1->IsRemainingTextLayoutObject());

  LayoutTextFragment* layout_object2 =
      ToLayoutTextFragment(AssociatedLayoutObjectOf(*text, 2));
  EXPECT_EQ(layout_object1, layout_object2);
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
  Element* html = GetDocument().createElement("html");
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
                GetDocument().documentElement())));

  EXPECT_EQ(PositionInFlatTree::BeforeNode(*input),
            CanonicalPositionOf(PositionInFlatTree::FirstPositionInNode(
                GetDocument().documentElement())));
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

TEST_F(VisibleUnitsTest, computeInlineBoxPositionBidiIsolate) {
  // "|" is bidi-level 0, and "foo" and "bar" are bidi-level 2
  SetBodyContent(
      "|<span id=sample style='unicode-bidi: isolate;'>foo<br>bar</span>|");

  Element* sample = GetDocument().getElementById("sample");
  Node* text = sample->firstChild();

  const InlineBoxPosition& actual =
      ComputeInlineBoxPosition(Position(text, 0), TextAffinity::kDownstream);
  EXPECT_EQ(ToLayoutText(text->GetLayoutObject())->FirstTextBox(),
            actual.inline_box);
}

// http://crbug.com/716093
TEST_F(VisibleUnitsTest, ComputeInlineBoxPositionMixedEditable) {
  SetBodyContent(
      "<div contenteditable id=sample>abc<input contenteditable=false></div>");
  Element* const sample = GetDocument().getElementById("sample");

  const InlineBoxPosition& actual = ComputeInlineBoxPosition(
      Position::LastPositionInNode(sample), TextAffinity::kDownstream);
  // Should not be in infinite-loop
  EXPECT_EQ(nullptr, actual.inline_box);
  EXPECT_EQ(0, actual.offset_in_box);
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

TEST_F(VisibleUnitsTest, endOfLine) {
  const char* body_content =
      "<a id=host><b id=one>11</b><b id=two>22</b></a><i id=three>333</i><i "
      "id=four>4444</i><br>";
  const char* shadow_content =
      "<div><u id=five>55555</u><content select=#two></content><br><u "
      "id=six>666666</u><br><content select=#one></content><u "
      "id=seven>7777777</u></div>";
  SetBodyContent(body_content);
  ShadowRoot* shadow_root = SetShadowContent(shadow_content, "host");

  Node* one = GetDocument().getElementById("one")->firstChild();
  Node* two = GetDocument().getElementById("two")->firstChild();
  Node* three = GetDocument().getElementById("three")->firstChild();
  Node* four = GetDocument().getElementById("four")->firstChild();
  Node* five = shadow_root->getElementById("five")->firstChild();
  Node* six = shadow_root->getElementById("six")->firstChild();
  Node* seven = shadow_root->getElementById("seven")->firstChild();

  EXPECT_EQ(
      Position(seven, 7),
      EndOfLine(CreateVisiblePositionInDOMTree(*one, 0)).DeepEquivalent());
  EXPECT_EQ(
      PositionInFlatTree(seven, 7),
      EndOfLine(CreateVisiblePositionInFlatTree(*one, 0)).DeepEquivalent());

  EXPECT_EQ(
      Position(seven, 7),
      EndOfLine(CreateVisiblePositionInDOMTree(*one, 1)).DeepEquivalent());
  EXPECT_EQ(
      PositionInFlatTree(seven, 7),
      EndOfLine(CreateVisiblePositionInFlatTree(*one, 1)).DeepEquivalent());

  EXPECT_EQ(
      Position(seven, 7),
      EndOfLine(CreateVisiblePositionInDOMTree(*two, 0)).DeepEquivalent());
  EXPECT_EQ(
      PositionInFlatTree(two, 2),
      EndOfLine(CreateVisiblePositionInFlatTree(*two, 0)).DeepEquivalent());

  // TODO(yosin) endOfLine(two, 1) -> (five, 5) is a broken result. We keep
  // it as a marker for future change.
  EXPECT_EQ(
      Position(five, 5),
      EndOfLine(CreateVisiblePositionInDOMTree(*two, 1)).DeepEquivalent());
  EXPECT_EQ(
      PositionInFlatTree(two, 2),
      EndOfLine(CreateVisiblePositionInFlatTree(*two, 1)).DeepEquivalent());

  EXPECT_EQ(
      Position(five, 5),
      EndOfLine(CreateVisiblePositionInDOMTree(*three, 0)).DeepEquivalent());
  EXPECT_EQ(
      PositionInFlatTree(four, 4),
      EndOfLine(CreateVisiblePositionInFlatTree(*three, 1)).DeepEquivalent());

  EXPECT_EQ(
      Position(four, 4),
      EndOfLine(CreateVisiblePositionInDOMTree(*four, 1)).DeepEquivalent());
  EXPECT_EQ(
      PositionInFlatTree(four, 4),
      EndOfLine(CreateVisiblePositionInFlatTree(*four, 1)).DeepEquivalent());

  EXPECT_EQ(
      Position(five, 5),
      EndOfLine(CreateVisiblePositionInDOMTree(*five, 1)).DeepEquivalent());
  EXPECT_EQ(
      PositionInFlatTree(two, 2),
      EndOfLine(CreateVisiblePositionInFlatTree(*five, 1)).DeepEquivalent());

  EXPECT_EQ(
      Position(six, 6),
      EndOfLine(CreateVisiblePositionInDOMTree(*six, 1)).DeepEquivalent());
  EXPECT_EQ(
      PositionInFlatTree(six, 6),
      EndOfLine(CreateVisiblePositionInFlatTree(*six, 1)).DeepEquivalent());

  EXPECT_EQ(
      Position(seven, 7),
      EndOfLine(CreateVisiblePositionInDOMTree(*seven, 1)).DeepEquivalent());
  EXPECT_EQ(
      PositionInFlatTree(seven, 7),
      EndOfLine(CreateVisiblePositionInFlatTree(*seven, 1)).DeepEquivalent());
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

TEST_F(VisibleUnitsTest, endOfSentence) {
  const char* body_content = "<a id=host><b id=one>1</b><b id=two>22</b></a>";
  const char* shadow_content =
      "<p><i id=three>333</i> <content select=#two></content> <content "
      "select=#one></content> <i id=four>4444</i></p>";
  SetBodyContent(body_content);
  SetShadowContent(shadow_content, "host");
  ShadowRoot* shadow_root = SetShadowContent(shadow_content, "host");

  Node* one = GetDocument().getElementById("one")->firstChild();
  Node* two = GetDocument().getElementById("two")->firstChild();
  Node* three = shadow_root->getElementById("three")->firstChild();
  Node* four = shadow_root->getElementById("four")->firstChild();

  EXPECT_EQ(
      Position(two, 2),
      EndOfSentence(CreateVisiblePositionInDOMTree(*one, 0)).DeepEquivalent());
  EXPECT_EQ(
      PositionInFlatTree(four, 4),
      EndOfSentence(CreateVisiblePositionInFlatTree(*one, 0)).DeepEquivalent());

  EXPECT_EQ(
      Position(two, 2),
      EndOfSentence(CreateVisiblePositionInDOMTree(*one, 1)).DeepEquivalent());
  EXPECT_EQ(
      PositionInFlatTree(four, 4),
      EndOfSentence(CreateVisiblePositionInFlatTree(*one, 1)).DeepEquivalent());

  EXPECT_EQ(
      Position(two, 2),
      EndOfSentence(CreateVisiblePositionInDOMTree(*two, 0)).DeepEquivalent());
  EXPECT_EQ(
      PositionInFlatTree(four, 4),
      EndOfSentence(CreateVisiblePositionInFlatTree(*two, 0)).DeepEquivalent());

  EXPECT_EQ(
      Position(two, 2),
      EndOfSentence(CreateVisiblePositionInDOMTree(*two, 1)).DeepEquivalent());
  EXPECT_EQ(
      PositionInFlatTree(four, 4),
      EndOfSentence(CreateVisiblePositionInFlatTree(*two, 1)).DeepEquivalent());

  EXPECT_EQ(Position(four, 4),
            EndOfSentence(CreateVisiblePositionInDOMTree(*three, 1))
                .DeepEquivalent());
  EXPECT_EQ(PositionInFlatTree(four, 4),
            EndOfSentence(CreateVisiblePositionInFlatTree(*three, 1))
                .DeepEquivalent());

  EXPECT_EQ(
      Position(four, 4),
      EndOfSentence(CreateVisiblePositionInDOMTree(*four, 1)).DeepEquivalent());
  EXPECT_EQ(PositionInFlatTree(four, 4),
            EndOfSentence(CreateVisiblePositionInFlatTree(*four, 1))
                .DeepEquivalent());
}

TEST_F(VisibleUnitsTest, endOfWord) {
  const char* body_content =
      "<a id=host><b id=one>1</b> <b id=two>22</b></a><i id=three>333</i>";
  const char* shadow_content =
      "<p><u id=four>44444</u><content select=#two></content><span id=space> "
      "</span><content select=#one></content><u id=five>55555</u></p>";
  SetBodyContent(body_content);
  ShadowRoot* shadow_root = SetShadowContent(shadow_content, "host");

  Node* one = GetDocument().getElementById("one")->firstChild();
  Node* two = GetDocument().getElementById("two")->firstChild();
  Node* three = GetDocument().getElementById("three")->firstChild();
  Node* four = shadow_root->getElementById("four")->firstChild();
  Node* five = shadow_root->getElementById("five")->firstChild();

  EXPECT_EQ(
      Position(three, 3),
      EndOfWord(CreateVisiblePositionInDOMTree(*one, 0)).DeepEquivalent());
  EXPECT_EQ(
      PositionInFlatTree(five, 5),
      EndOfWord(CreateVisiblePositionInFlatTree(*one, 0)).DeepEquivalent());

  EXPECT_EQ(
      Position(three, 3),
      EndOfWord(CreateVisiblePositionInDOMTree(*one, 1)).DeepEquivalent());
  EXPECT_EQ(
      PositionInFlatTree(five, 5),
      EndOfWord(CreateVisiblePositionInFlatTree(*one, 1)).DeepEquivalent());

  EXPECT_EQ(
      Position(three, 3),
      EndOfWord(CreateVisiblePositionInDOMTree(*two, 0)).DeepEquivalent());
  EXPECT_EQ(
      PositionInFlatTree(two, 2),
      EndOfWord(CreateVisiblePositionInFlatTree(*two, 0)).DeepEquivalent());

  EXPECT_EQ(
      Position(three, 3),
      EndOfWord(CreateVisiblePositionInDOMTree(*two, 1)).DeepEquivalent());
  EXPECT_EQ(
      PositionInFlatTree(two, 2),
      EndOfWord(CreateVisiblePositionInFlatTree(*two, 1)).DeepEquivalent());

  EXPECT_EQ(
      Position(three, 3),
      EndOfWord(CreateVisiblePositionInDOMTree(*three, 1)).DeepEquivalent());
  EXPECT_EQ(
      PositionInFlatTree(three, 3),
      EndOfWord(CreateVisiblePositionInFlatTree(*three, 1)).DeepEquivalent());

  EXPECT_EQ(
      Position(four, 5),
      EndOfWord(CreateVisiblePositionInDOMTree(*four, 1)).DeepEquivalent());
  EXPECT_EQ(
      PositionInFlatTree(two, 2),
      EndOfWord(CreateVisiblePositionInFlatTree(*four, 1)).DeepEquivalent());

  EXPECT_EQ(
      Position(five, 5),
      EndOfWord(CreateVisiblePositionInDOMTree(*five, 1)).DeepEquivalent());
  EXPECT_EQ(
      PositionInFlatTree(five, 5),
      EndOfWord(CreateVisiblePositionInFlatTree(*five, 1)).DeepEquivalent());
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

TEST_F(VisibleUnitsTest, isEndOfLine) {
  const char* body_content =
      "<a id=host><b id=one>11</b><b id=two>22</b></a><i id=three>333</i><i "
      "id=four>4444</i><br>";
  const char* shadow_content =
      "<div><u id=five>55555</u><content select=#two></content><br><u "
      "id=six>666666</u><br><content select=#one></content><u "
      "id=seven>7777777</u></div>";
  SetBodyContent(body_content);
  ShadowRoot* shadow_root = SetShadowContent(shadow_content, "host");

  Node* one = GetDocument().getElementById("one")->firstChild();
  Node* two = GetDocument().getElementById("two")->firstChild();
  Node* three = GetDocument().getElementById("three")->firstChild();
  Node* four = GetDocument().getElementById("four")->firstChild();
  Node* five = shadow_root->getElementById("five")->firstChild();
  Node* six = shadow_root->getElementById("six")->firstChild();
  Node* seven = shadow_root->getElementById("seven")->firstChild();

  EXPECT_FALSE(IsEndOfLine(CreateVisiblePositionInDOMTree(*one, 0)));
  EXPECT_FALSE(IsEndOfLine(CreateVisiblePositionInFlatTree(*one, 0)));

  EXPECT_FALSE(IsEndOfLine(CreateVisiblePositionInDOMTree(*one, 1)));
  EXPECT_FALSE(IsEndOfLine(CreateVisiblePositionInFlatTree(*one, 1)));

  EXPECT_FALSE(IsEndOfLine(CreateVisiblePositionInDOMTree(*two, 2)));
  EXPECT_TRUE(IsEndOfLine(CreateVisiblePositionInFlatTree(*two, 2)));

  EXPECT_FALSE(IsEndOfLine(CreateVisiblePositionInDOMTree(*three, 3)));
  EXPECT_FALSE(IsEndOfLine(CreateVisiblePositionInFlatTree(*three, 3)));

  EXPECT_TRUE(IsEndOfLine(CreateVisiblePositionInDOMTree(*four, 4)));
  EXPECT_TRUE(IsEndOfLine(CreateVisiblePositionInFlatTree(*four, 4)));

  EXPECT_TRUE(IsEndOfLine(CreateVisiblePositionInDOMTree(*five, 5)));
  EXPECT_FALSE(IsEndOfLine(CreateVisiblePositionInFlatTree(*five, 5)));

  EXPECT_TRUE(IsEndOfLine(CreateVisiblePositionInDOMTree(*six, 6)));
  EXPECT_TRUE(IsEndOfLine(CreateVisiblePositionInFlatTree(*six, 6)));

  EXPECT_TRUE(IsEndOfLine(CreateVisiblePositionInDOMTree(*seven, 7)));
  EXPECT_TRUE(IsEndOfLine(CreateVisiblePositionInFlatTree(*seven, 7)));
}

TEST_F(VisibleUnitsTest, isLogicalEndOfLine) {
  const char* body_content =
      "<a id=host><b id=one>11</b><b id=two>22</b></a><i id=three>333</i><i "
      "id=four>4444</i><br>";
  const char* shadow_content =
      "<div><u id=five>55555</u><content select=#two></content><br><u "
      "id=six>666666</u><br><content select=#one></content><u "
      "id=seven>7777777</u></div>";
  SetBodyContent(body_content);
  ShadowRoot* shadow_root = SetShadowContent(shadow_content, "host");

  Node* one = GetDocument().getElementById("one")->firstChild();
  Node* two = GetDocument().getElementById("two")->firstChild();
  Node* three = GetDocument().getElementById("three")->firstChild();
  Node* four = GetDocument().getElementById("four")->firstChild();
  Node* five = shadow_root->getElementById("five")->firstChild();
  Node* six = shadow_root->getElementById("six")->firstChild();
  Node* seven = shadow_root->getElementById("seven")->firstChild();

  EXPECT_FALSE(IsLogicalEndOfLine(CreateVisiblePositionInDOMTree(*one, 0)));
  EXPECT_FALSE(IsLogicalEndOfLine(CreateVisiblePositionInFlatTree(*one, 0)));

  EXPECT_FALSE(IsLogicalEndOfLine(CreateVisiblePositionInDOMTree(*one, 1)));
  EXPECT_FALSE(IsLogicalEndOfLine(CreateVisiblePositionInFlatTree(*one, 1)));

  EXPECT_FALSE(IsLogicalEndOfLine(CreateVisiblePositionInDOMTree(*two, 2)));
  EXPECT_TRUE(IsLogicalEndOfLine(CreateVisiblePositionInFlatTree(*two, 2)));

  EXPECT_FALSE(IsLogicalEndOfLine(CreateVisiblePositionInDOMTree(*three, 3)));
  EXPECT_FALSE(IsLogicalEndOfLine(CreateVisiblePositionInFlatTree(*three, 3)));

  EXPECT_TRUE(IsLogicalEndOfLine(CreateVisiblePositionInDOMTree(*four, 4)));
  EXPECT_TRUE(IsLogicalEndOfLine(CreateVisiblePositionInFlatTree(*four, 4)));

  EXPECT_TRUE(IsLogicalEndOfLine(CreateVisiblePositionInDOMTree(*five, 5)));
  EXPECT_FALSE(IsLogicalEndOfLine(CreateVisiblePositionInFlatTree(*five, 5)));

  EXPECT_TRUE(IsLogicalEndOfLine(CreateVisiblePositionInDOMTree(*six, 6)));
  EXPECT_TRUE(IsLogicalEndOfLine(CreateVisiblePositionInFlatTree(*six, 6)));

  EXPECT_TRUE(IsLogicalEndOfLine(CreateVisiblePositionInDOMTree(*seven, 7)));
  EXPECT_TRUE(IsLogicalEndOfLine(CreateVisiblePositionInFlatTree(*seven, 7)));
}

TEST_F(VisibleUnitsTest, inSameLine) {
  const char* body_content =
      "<p id='host'>00<b id='one'>11</b><b id='two'>22</b>33</p>";
  const char* shadow_content =
      "<div><span id='s4'>44</span><content select=#two></content><br><span "
      "id='s5'>55</span><br><content select=#one></content><span "
      "id='s6'>66</span></div>";
  SetBodyContent(body_content);
  ShadowRoot* shadow_root = SetShadowContent(shadow_content, "host");

  Element* body = GetDocument().body();
  Element* one = body->QuerySelector("#one");
  Element* two = body->QuerySelector("#two");
  Element* four = shadow_root->QuerySelector("#s4");
  Element* five = shadow_root->QuerySelector("#s5");

  EXPECT_TRUE(InSameLine(PositionWithAffinityInDOMTree(*one, 0),
                         PositionWithAffinityInDOMTree(*two, 0)));
  EXPECT_TRUE(InSameLine(PositionWithAffinityInDOMTree(*one->firstChild(), 0),
                         PositionWithAffinityInDOMTree(*two->firstChild(), 0)));
  EXPECT_FALSE(
      InSameLine(PositionWithAffinityInDOMTree(*one->firstChild(), 0),
                 PositionWithAffinityInDOMTree(*five->firstChild(), 0)));
  EXPECT_FALSE(
      InSameLine(PositionWithAffinityInDOMTree(*two->firstChild(), 0),
                 PositionWithAffinityInDOMTree(*four->firstChild(), 0)));

  EXPECT_TRUE(InSameLine(CreateVisiblePositionInDOMTree(*one, 0),
                         CreateVisiblePositionInDOMTree(*two, 0)));
  EXPECT_TRUE(
      InSameLine(CreateVisiblePositionInDOMTree(*one->firstChild(), 0),
                 CreateVisiblePositionInDOMTree(*two->firstChild(), 0)));
  EXPECT_FALSE(
      InSameLine(CreateVisiblePositionInDOMTree(*one->firstChild(), 0),
                 CreateVisiblePositionInDOMTree(*five->firstChild(), 0)));
  EXPECT_FALSE(
      InSameLine(CreateVisiblePositionInDOMTree(*two->firstChild(), 0),
                 CreateVisiblePositionInDOMTree(*four->firstChild(), 0)));

  EXPECT_FALSE(InSameLine(PositionWithAffinityInFlatTree(*one, 0),
                          PositionWithAffinityInFlatTree(*two, 0)));
  EXPECT_FALSE(
      InSameLine(PositionWithAffinityInFlatTree(*one->firstChild(), 0),
                 PositionWithAffinityInFlatTree(*two->firstChild(), 0)));
  EXPECT_FALSE(
      InSameLine(PositionWithAffinityInFlatTree(*one->firstChild(), 0),
                 PositionWithAffinityInFlatTree(*five->firstChild(), 0)));
  EXPECT_TRUE(
      InSameLine(PositionWithAffinityInFlatTree(*two->firstChild(), 0),
                 PositionWithAffinityInFlatTree(*four->firstChild(), 0)));

  EXPECT_FALSE(InSameLine(CreateVisiblePositionInFlatTree(*one, 0),
                          CreateVisiblePositionInFlatTree(*two, 0)));
  EXPECT_FALSE(
      InSameLine(CreateVisiblePositionInFlatTree(*one->firstChild(), 0),
                 CreateVisiblePositionInFlatTree(*two->firstChild(), 0)));
  EXPECT_FALSE(
      InSameLine(CreateVisiblePositionInFlatTree(*one->firstChild(), 0),
                 CreateVisiblePositionInFlatTree(*five->firstChild(), 0)));
  EXPECT_TRUE(
      InSameLine(CreateVisiblePositionInFlatTree(*two->firstChild(), 0),
                 CreateVisiblePositionInFlatTree(*four->firstChild(), 0)));
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

TEST_F(VisibleUnitsTest, isStartOfLine) {
  const char* body_content =
      "<a id=host><b id=one>11</b><b id=two>22</b></a><i id=three>333</i><i "
      "id=four>4444</i><br>";
  const char* shadow_content =
      "<div><u id=five>55555</u><content select=#two></content><br><u "
      "id=six>666666</u><br><content select=#one></content><u "
      "id=seven>7777777</u></div>";
  SetBodyContent(body_content);
  ShadowRoot* shadow_root = SetShadowContent(shadow_content, "host");

  Node* one = GetDocument().getElementById("one")->firstChild();
  Node* two = GetDocument().getElementById("two")->firstChild();
  Node* three = GetDocument().getElementById("three")->firstChild();
  Node* four = GetDocument().getElementById("four")->firstChild();
  Node* five = shadow_root->getElementById("five")->firstChild();
  Node* six = shadow_root->getElementById("six")->firstChild();
  Node* seven = shadow_root->getElementById("seven")->firstChild();

  EXPECT_TRUE(IsStartOfLine(CreateVisiblePositionInDOMTree(*one, 0)));
  EXPECT_TRUE(IsStartOfLine(CreateVisiblePositionInFlatTree(*one, 0)));

  EXPECT_FALSE(IsStartOfLine(CreateVisiblePositionInDOMTree(*one, 1)));
  EXPECT_FALSE(IsStartOfLine(CreateVisiblePositionInFlatTree(*one, 1)));

  EXPECT_FALSE(IsStartOfLine(CreateVisiblePositionInDOMTree(*two, 0)));
  EXPECT_FALSE(IsStartOfLine(CreateVisiblePositionInFlatTree(*two, 0)));

  EXPECT_FALSE(IsStartOfLine(CreateVisiblePositionInDOMTree(*three, 0)));
  EXPECT_TRUE(IsStartOfLine(CreateVisiblePositionInFlatTree(*three, 0)));

  EXPECT_FALSE(IsStartOfLine(CreateVisiblePositionInDOMTree(*four, 0)));
  EXPECT_FALSE(IsStartOfLine(CreateVisiblePositionInFlatTree(*four, 0)));

  EXPECT_TRUE(IsStartOfLine(CreateVisiblePositionInDOMTree(*five, 0)));
  EXPECT_TRUE(IsStartOfLine(CreateVisiblePositionInFlatTree(*five, 0)));

  EXPECT_TRUE(IsStartOfLine(CreateVisiblePositionInDOMTree(*six, 0)));
  EXPECT_TRUE(IsStartOfLine(CreateVisiblePositionInFlatTree(*six, 0)));

  EXPECT_FALSE(IsStartOfLine(CreateVisiblePositionInDOMTree(*seven, 0)));
  EXPECT_FALSE(IsStartOfLine(CreateVisiblePositionInFlatTree(*seven, 0)));
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
  Element* html = GetDocument().createElement("html");
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

TEST_F(VisibleUnitsTest, localCaretRectOfPosition) {
  const char* body_content =
      "<p id='host'><b id='one'>1</b></p><b id='two'>22</b>";
  const char* shadow_content =
      "<b id='two'>22</b><content select=#one></content><b id='three'>333</b>";
  SetBodyContent(body_content);
  SetShadowContent(shadow_content, "host");

  Element* one = GetDocument().getElementById("one");

  const LocalCaretRect& caret_rect_from_dom_tree =
      LocalCaretRectOfPosition(Position(one->firstChild(), 0));

  const LocalCaretRect& caret_rect_from_flat_tree =
      LocalCaretRectOfPosition(PositionInFlatTree(one->firstChild(), 0));

  EXPECT_FALSE(caret_rect_from_dom_tree.IsEmpty());
  EXPECT_EQ(caret_rect_from_dom_tree.layout_object,
            caret_rect_from_flat_tree.layout_object);
  EXPECT_EQ(caret_rect_from_dom_tree.rect, caret_rect_from_flat_tree.rect);
}

TEST_F(VisibleUnitsTest, logicalEndOfLine) {
  const char* body_content =
      "<a id=host><b id=one>11</b><b id=two>22</b></a><i id=three>333</i><i "
      "id=four>4444</i><br>";
  const char* shadow_content =
      "<div><u id=five>55555</u><content select=#two></content><br><u "
      "id=six>666666</u><br><content select=#one></content><u "
      "id=seven>7777777</u></div>";
  SetBodyContent(body_content);
  ShadowRoot* shadow_root = SetShadowContent(shadow_content, "host");

  Node* one = GetDocument().getElementById("one")->firstChild();
  Node* two = GetDocument().getElementById("two")->firstChild();
  Node* three = GetDocument().getElementById("three")->firstChild();
  Node* four = GetDocument().getElementById("four")->firstChild();
  Node* five = shadow_root->getElementById("five")->firstChild();
  Node* six = shadow_root->getElementById("six")->firstChild();
  Node* seven = shadow_root->getElementById("seven")->firstChild();

  EXPECT_EQ(Position(seven, 7),
            LogicalEndOfLine(CreateVisiblePositionInDOMTree(*one, 0))
                .DeepEquivalent());
  EXPECT_EQ(PositionInFlatTree(seven, 7),
            LogicalEndOfLine(CreateVisiblePositionInFlatTree(*one, 0))
                .DeepEquivalent());

  EXPECT_EQ(Position(seven, 7),
            LogicalEndOfLine(CreateVisiblePositionInDOMTree(*one, 1))
                .DeepEquivalent());
  EXPECT_EQ(PositionInFlatTree(seven, 7),
            LogicalEndOfLine(CreateVisiblePositionInFlatTree(*one, 1))
                .DeepEquivalent());

  EXPECT_EQ(Position(seven, 7),
            LogicalEndOfLine(CreateVisiblePositionInDOMTree(*two, 0))
                .DeepEquivalent());
  EXPECT_EQ(PositionInFlatTree(two, 2),
            LogicalEndOfLine(CreateVisiblePositionInFlatTree(*two, 0))
                .DeepEquivalent());

  // TODO(yosin) logicalEndOfLine(two, 1) -> (five, 5) is a broken result. We
  // keep it as a marker for future change.
  EXPECT_EQ(Position(five, 5),
            LogicalEndOfLine(CreateVisiblePositionInDOMTree(*two, 1))
                .DeepEquivalent());
  EXPECT_EQ(PositionInFlatTree(two, 2),
            LogicalEndOfLine(CreateVisiblePositionInFlatTree(*two, 1))
                .DeepEquivalent());

  EXPECT_EQ(Position(five, 5),
            LogicalEndOfLine(CreateVisiblePositionInDOMTree(*three, 0))
                .DeepEquivalent());
  EXPECT_EQ(PositionInFlatTree(four, 4),
            LogicalEndOfLine(CreateVisiblePositionInFlatTree(*three, 1))
                .DeepEquivalent());

  EXPECT_EQ(Position(four, 4),
            LogicalEndOfLine(CreateVisiblePositionInDOMTree(*four, 1))
                .DeepEquivalent());
  EXPECT_EQ(PositionInFlatTree(four, 4),
            LogicalEndOfLine(CreateVisiblePositionInFlatTree(*four, 1))
                .DeepEquivalent());

  EXPECT_EQ(Position(five, 5),
            LogicalEndOfLine(CreateVisiblePositionInDOMTree(*five, 1))
                .DeepEquivalent());
  EXPECT_EQ(PositionInFlatTree(two, 2),
            LogicalEndOfLine(CreateVisiblePositionInFlatTree(*five, 1))
                .DeepEquivalent());

  EXPECT_EQ(Position(six, 6),
            LogicalEndOfLine(CreateVisiblePositionInDOMTree(*six, 1))
                .DeepEquivalent());
  EXPECT_EQ(PositionInFlatTree(six, 6),
            LogicalEndOfLine(CreateVisiblePositionInFlatTree(*six, 1))
                .DeepEquivalent());

  EXPECT_EQ(Position(seven, 7),
            LogicalEndOfLine(CreateVisiblePositionInDOMTree(*seven, 1))
                .DeepEquivalent());
  EXPECT_EQ(PositionInFlatTree(seven, 7),
            LogicalEndOfLine(CreateVisiblePositionInFlatTree(*seven, 1))
                .DeepEquivalent());
}

TEST_F(VisibleUnitsTest, logicalStartOfLine) {
  const char* body_content =
      "<a id=host><b id=one>11</b><b id=two>22</b></a><i id=three>333</i><i "
      "id=four>4444</i><br>";
  const char* shadow_content =
      "<div><u id=five>55555</u><content select=#two></content><br><u "
      "id=six>666666</u><br><content select=#one></content><u "
      "id=seven>7777777</u></div>";
  SetBodyContent(body_content);
  ShadowRoot* shadow_root = SetShadowContent(shadow_content, "host");

  Node* one = GetDocument().getElementById("one")->firstChild();
  Node* two = GetDocument().getElementById("two")->firstChild();
  Node* three = GetDocument().getElementById("three")->firstChild();
  Node* four = GetDocument().getElementById("four")->firstChild();
  Node* five = shadow_root->getElementById("five")->firstChild();
  Node* six = shadow_root->getElementById("six")->firstChild();
  Node* seven = shadow_root->getElementById("seven")->firstChild();

  EXPECT_EQ(Position(one, 0),
            LogicalStartOfLine(CreateVisiblePositionInDOMTree(*one, 0))
                .DeepEquivalent());
  EXPECT_EQ(PositionInFlatTree(one, 0),
            LogicalStartOfLine(CreateVisiblePositionInFlatTree(*one, 0))
                .DeepEquivalent());

  EXPECT_EQ(Position(one, 0),
            LogicalStartOfLine(CreateVisiblePositionInDOMTree(*one, 1))
                .DeepEquivalent());
  EXPECT_EQ(PositionInFlatTree(one, 0),
            LogicalStartOfLine(CreateVisiblePositionInFlatTree(*one, 1))
                .DeepEquivalent());

  EXPECT_EQ(Position(one, 0),
            LogicalStartOfLine(CreateVisiblePositionInDOMTree(*two, 0))
                .DeepEquivalent());
  EXPECT_EQ(PositionInFlatTree(five, 0),
            LogicalStartOfLine(CreateVisiblePositionInFlatTree(*two, 0))
                .DeepEquivalent());

  EXPECT_EQ(Position(five, 0),
            LogicalStartOfLine(CreateVisiblePositionInDOMTree(*two, 1))
                .DeepEquivalent());
  EXPECT_EQ(PositionInFlatTree(five, 0),
            LogicalStartOfLine(CreateVisiblePositionInFlatTree(*two, 1))
                .DeepEquivalent());

  EXPECT_EQ(Position(five, 0),
            LogicalStartOfLine(CreateVisiblePositionInDOMTree(*three, 0))
                .DeepEquivalent());
  EXPECT_EQ(PositionInFlatTree(three, 0),
            LogicalStartOfLine(CreateVisiblePositionInFlatTree(*three, 1))
                .DeepEquivalent());

  // TODO(yosin) logicalStartOfLine(four, 1) -> (two, 2) is a broken result.
  // We keep it as a marker for future change.
  EXPECT_EQ(Position(two, 2),
            LogicalStartOfLine(CreateVisiblePositionInDOMTree(*four, 1))
                .DeepEquivalent());
  EXPECT_EQ(PositionInFlatTree(three, 0),
            LogicalStartOfLine(CreateVisiblePositionInFlatTree(*four, 1))
                .DeepEquivalent());

  EXPECT_EQ(Position(five, 0),
            LogicalStartOfLine(CreateVisiblePositionInDOMTree(*five, 1))
                .DeepEquivalent());
  EXPECT_EQ(PositionInFlatTree(five, 0),
            LogicalStartOfLine(CreateVisiblePositionInFlatTree(*five, 1))
                .DeepEquivalent());

  EXPECT_EQ(Position(six, 0),
            LogicalStartOfLine(CreateVisiblePositionInDOMTree(*six, 1))
                .DeepEquivalent());
  EXPECT_EQ(PositionInFlatTree(six, 0),
            LogicalStartOfLine(CreateVisiblePositionInFlatTree(*six, 1))
                .DeepEquivalent());

  EXPECT_EQ(Position(one, 0),
            LogicalStartOfLine(CreateVisiblePositionInDOMTree(*seven, 1))
                .DeepEquivalent());
  EXPECT_EQ(PositionInFlatTree(one, 0),
            LogicalStartOfLine(CreateVisiblePositionInFlatTree(*seven, 1))
                .DeepEquivalent());
}

TEST_F(VisibleUnitsTest, mostBackwardCaretPositionAfterAnchor) {
  const char* body_content =
      "<p id='host'><b id='one'>1</b></p><b id='two'>22</b>";
  const char* shadow_content =
      "<b id='two'>22</b><content select=#one></content><b id='three'>333</b>";
  SetBodyContent(body_content);
  SetShadowContent(shadow_content, "host");

  Element* host = GetDocument().getElementById("host");

  EXPECT_EQ(Position::LastPositionInNode(host),
            MostForwardCaretPosition(Position::AfterNode(host)));
  EXPECT_EQ(PositionInFlatTree::LastPositionInNode(host),
            MostForwardCaretPosition(PositionInFlatTree::AfterNode(host)));
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
                Position::LastPositionInNode(sample->parentNode())));
  EXPECT_EQ(
      Position(sample, 6),
      MostBackwardCaretPosition(Position::AfterNode(sample->parentNode())));
  EXPECT_EQ(Position::LastPositionInNode(GetDocument().body()),
            MostBackwardCaretPosition(
                Position::LastPositionInNode(GetDocument().body())));
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
            MostBackwardCaretPosition(Position::LastPositionInNode(sample)));
  EXPECT_EQ(Position(remaining, 2),
            MostBackwardCaretPosition(Position::AfterNode(sample)));
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
            MostBackwardCaretPosition(Position::AfterNode(host)));
  EXPECT_EQ(PositionInFlatTree(three->firstChild(), 3),
            MostBackwardCaretPosition(PositionInFlatTree::AfterNode(host)));
}

TEST_F(VisibleUnitsTest, mostForwardCaretPositionFirstLetter) {
  // Note: first-letter pseudo element contains letter and punctuations.
  const char* body_content =
      "<style>p:first-letter {color:red;}</style><p id=sample> (2)45 </p>";
  SetBodyContent(body_content);

  Node* sample = GetDocument().getElementById("sample")->firstChild();

  EXPECT_EQ(Position(GetDocument().body(), 0),
            MostForwardCaretPosition(
                Position::FirstPositionInNode(GetDocument().body())));
  EXPECT_EQ(
      Position(sample, 1),
      MostForwardCaretPosition(Position::BeforeNode(*sample->parentNode())));
  EXPECT_EQ(Position(sample, 1),
            MostForwardCaretPosition(
                Position::FirstPositionInNode(sample->parentNode())));
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
      RendersInDifferentPosition(Position(), Position::AfterNode(sample)))
      << "if one of position is null, the reuslt is false.";
  EXPECT_FALSE(RendersInDifferentPosition(Position::AfterNode(sample),
                                          Position(sample, 1)));
  EXPECT_FALSE(RendersInDifferentPosition(Position::LastPositionInNode(sample),
                                          Position(sample, 1)));
}

TEST_F(VisibleUnitsTest, rendersInDifferentPositionAfterAnchorWithHidden) {
  const char* body_content =
      "<p><span id=one>11</span><span id=two style='display:none'>  "
      "</span></p>";
  SetBodyContent(body_content);
  Element* one = GetDocument().getElementById("one");
  Element* two = GetDocument().getElementById("two");

  EXPECT_TRUE(RendersInDifferentPosition(Position::LastPositionInNode(one),
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

  EXPECT_FALSE(RendersInDifferentPosition(Position::LastPositionInNode(one),
                                          Position(two, 0)));
  EXPECT_FALSE(RendersInDifferentPosition(Position::LastPositionInNode(one),
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
      RendersInDifferentPosition(Position::AfterNode(sample1->firstChild()),
                                 Position(sample2->firstChild(), 0)));
  EXPECT_FALSE(RendersInDifferentPosition(
      Position::LastPositionInNode(sample1->firstChild()),
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

TEST_F(VisibleUnitsTest, startOfLine) {
  const char* body_content =
      "<a id=host><b id=one>11</b><b id=two>22</b></a><i id=three>333</i><i "
      "id=four>4444</i><br>";
  const char* shadow_content =
      "<div><u id=five>55555</u><content select=#two></content><br><u "
      "id=six>666666</u><br><content select=#one></content><u "
      "id=seven>7777777</u></div>";
  SetBodyContent(body_content);
  ShadowRoot* shadow_root = SetShadowContent(shadow_content, "host");

  Node* one = GetDocument().getElementById("one")->firstChild();
  Node* two = GetDocument().getElementById("two")->firstChild();
  Node* three = GetDocument().getElementById("three")->firstChild();
  Node* four = GetDocument().getElementById("four")->firstChild();
  Node* five = shadow_root->getElementById("five")->firstChild();
  Node* six = shadow_root->getElementById("six")->firstChild();
  Node* seven = shadow_root->getElementById("seven")->firstChild();

  EXPECT_EQ(
      Position(one, 0),
      StartOfLine(CreateVisiblePositionInDOMTree(*one, 0)).DeepEquivalent());
  EXPECT_EQ(
      PositionInFlatTree(one, 0),
      StartOfLine(CreateVisiblePositionInFlatTree(*one, 0)).DeepEquivalent());

  EXPECT_EQ(
      Position(one, 0),
      StartOfLine(CreateVisiblePositionInDOMTree(*one, 1)).DeepEquivalent());
  EXPECT_EQ(
      PositionInFlatTree(one, 0),
      StartOfLine(CreateVisiblePositionInFlatTree(*one, 1)).DeepEquivalent());

  EXPECT_EQ(
      Position(one, 0),
      StartOfLine(CreateVisiblePositionInDOMTree(*two, 0)).DeepEquivalent());
  EXPECT_EQ(
      PositionInFlatTree(five, 0),
      StartOfLine(CreateVisiblePositionInFlatTree(*two, 0)).DeepEquivalent());

  EXPECT_EQ(
      Position(five, 0),
      StartOfLine(CreateVisiblePositionInDOMTree(*two, 1)).DeepEquivalent());
  EXPECT_EQ(
      PositionInFlatTree(five, 0),
      StartOfLine(CreateVisiblePositionInFlatTree(*two, 1)).DeepEquivalent());

  EXPECT_EQ(
      Position(five, 0),
      StartOfLine(CreateVisiblePositionInDOMTree(*three, 0)).DeepEquivalent());
  EXPECT_EQ(
      PositionInFlatTree(three, 0),
      StartOfLine(CreateVisiblePositionInFlatTree(*three, 1)).DeepEquivalent());

  // TODO(yosin) startOfLine(four, 1) -> (two, 2) is a broken result. We keep
  // it as a marker for future change.
  EXPECT_EQ(
      Position(two, 2),
      StartOfLine(CreateVisiblePositionInDOMTree(*four, 1)).DeepEquivalent());
  EXPECT_EQ(
      PositionInFlatTree(three, 0),
      StartOfLine(CreateVisiblePositionInFlatTree(*four, 1)).DeepEquivalent());

  EXPECT_EQ(
      Position(five, 0),
      StartOfLine(CreateVisiblePositionInDOMTree(*five, 1)).DeepEquivalent());
  EXPECT_EQ(
      PositionInFlatTree(five, 0),
      StartOfLine(CreateVisiblePositionInFlatTree(*five, 1)).DeepEquivalent());

  EXPECT_EQ(
      Position(six, 0),
      StartOfLine(CreateVisiblePositionInDOMTree(*six, 1)).DeepEquivalent());
  EXPECT_EQ(
      PositionInFlatTree(six, 0),
      StartOfLine(CreateVisiblePositionInFlatTree(*six, 1)).DeepEquivalent());

  EXPECT_EQ(
      Position(one, 0),
      StartOfLine(CreateVisiblePositionInDOMTree(*seven, 1)).DeepEquivalent());
  EXPECT_EQ(
      PositionInFlatTree(one, 0),
      StartOfLine(CreateVisiblePositionInFlatTree(*seven, 1)).DeepEquivalent());
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
  root->setInnerHTML(
      "<style>* { display:inline-table; }</style><body "
      "contenteditable=true><svg><svg><foreignObject>abc<svg></svg></"
      "foreignObject></svg></svg></body>");
  Element* old_body = GetDocument().body();
  root->setInnerHTML(
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

TEST_F(VisibleUnitsTest, startOfSentence) {
  const char* body_content = "<a id=host><b id=one>1</b><b id=two>22</b></a>";
  const char* shadow_content =
      "<p><i id=three>333</i> <content select=#two></content> <content "
      "select=#one></content> <i id=four>4444</i></p>";
  SetBodyContent(body_content);
  ShadowRoot* shadow_root = SetShadowContent(shadow_content, "host");

  Node* one = GetDocument().getElementById("one")->firstChild();
  Node* two = GetDocument().getElementById("two")->firstChild();
  Node* three = shadow_root->getElementById("three")->firstChild();
  Node* four = shadow_root->getElementById("four")->firstChild();

  EXPECT_EQ(Position(one, 0),
            StartOfSentence(CreateVisiblePositionInDOMTree(*one, 0))
                .DeepEquivalent());
  EXPECT_EQ(PositionInFlatTree(three, 0),
            StartOfSentence(CreateVisiblePositionInFlatTree(*one, 0))
                .DeepEquivalent());

  EXPECT_EQ(Position(one, 0),
            StartOfSentence(CreateVisiblePositionInDOMTree(*one, 1))
                .DeepEquivalent());
  EXPECT_EQ(PositionInFlatTree(three, 0),
            StartOfSentence(CreateVisiblePositionInFlatTree(*one, 1))
                .DeepEquivalent());

  EXPECT_EQ(Position(one, 0),
            StartOfSentence(CreateVisiblePositionInDOMTree(*two, 0))
                .DeepEquivalent());
  EXPECT_EQ(PositionInFlatTree(three, 0),
            StartOfSentence(CreateVisiblePositionInFlatTree(*two, 0))
                .DeepEquivalent());

  EXPECT_EQ(Position(one, 0),
            StartOfSentence(CreateVisiblePositionInDOMTree(*two, 1))
                .DeepEquivalent());
  EXPECT_EQ(PositionInFlatTree(three, 0),
            StartOfSentence(CreateVisiblePositionInFlatTree(*two, 1))
                .DeepEquivalent());

  EXPECT_EQ(Position(three, 0),
            StartOfSentence(CreateVisiblePositionInDOMTree(*three, 1))
                .DeepEquivalent());
  EXPECT_EQ(PositionInFlatTree(three, 0),
            StartOfSentence(CreateVisiblePositionInFlatTree(*three, 1))
                .DeepEquivalent());

  EXPECT_EQ(Position(three, 0),
            StartOfSentence(CreateVisiblePositionInDOMTree(*four, 1))
                .DeepEquivalent());
  EXPECT_EQ(PositionInFlatTree(three, 0),
            StartOfSentence(CreateVisiblePositionInFlatTree(*four, 1))
                .DeepEquivalent());
}

TEST_F(VisibleUnitsTest, startOfWord) {
  const char* body_content =
      "<a id=host><b id=one>1</b> <b id=two>22</b></a><i id=three>333</i>";
  const char* shadow_content =
      "<p><u id=four>44444</u><content select=#two></content><span id=space> "
      "</span><content select=#one></content><u id=five>55555</u></p>";
  SetBodyContent(body_content);
  ShadowRoot* shadow_root = SetShadowContent(shadow_content, "host");

  Node* one = GetDocument().getElementById("one")->firstChild();
  Node* two = GetDocument().getElementById("two")->firstChild();
  Node* three = GetDocument().getElementById("three")->firstChild();
  Node* four = shadow_root->getElementById("four")->firstChild();
  Node* five = shadow_root->getElementById("five")->firstChild();
  Node* space = shadow_root->getElementById("space")->firstChild();

  EXPECT_EQ(
      Position(one, 0),
      StartOfWord(CreateVisiblePositionInDOMTree(*one, 0)).DeepEquivalent());
  EXPECT_EQ(
      PositionInFlatTree(space, 1),
      StartOfWord(CreateVisiblePositionInFlatTree(*one, 0)).DeepEquivalent());

  EXPECT_EQ(
      Position(one, 0),
      StartOfWord(CreateVisiblePositionInDOMTree(*one, 1)).DeepEquivalent());
  EXPECT_EQ(
      PositionInFlatTree(space, 1),
      StartOfWord(CreateVisiblePositionInFlatTree(*one, 1)).DeepEquivalent());

  EXPECT_EQ(
      Position(one, 0),
      StartOfWord(CreateVisiblePositionInDOMTree(*two, 0)).DeepEquivalent());
  EXPECT_EQ(
      PositionInFlatTree(four, 0),
      StartOfWord(CreateVisiblePositionInFlatTree(*two, 0)).DeepEquivalent());

  EXPECT_EQ(
      Position(one, 0),
      StartOfWord(CreateVisiblePositionInDOMTree(*two, 1)).DeepEquivalent());
  EXPECT_EQ(
      PositionInFlatTree(four, 0),
      StartOfWord(CreateVisiblePositionInFlatTree(*two, 1)).DeepEquivalent());

  EXPECT_EQ(
      Position(one, 0),
      StartOfWord(CreateVisiblePositionInDOMTree(*three, 1)).DeepEquivalent());
  EXPECT_EQ(
      PositionInFlatTree(space, 1),
      StartOfWord(CreateVisiblePositionInFlatTree(*three, 1)).DeepEquivalent());

  EXPECT_EQ(
      Position(four, 0),
      StartOfWord(CreateVisiblePositionInDOMTree(*four, 1)).DeepEquivalent());
  EXPECT_EQ(
      PositionInFlatTree(four, 0),
      StartOfWord(CreateVisiblePositionInFlatTree(*four, 1)).DeepEquivalent());

  EXPECT_EQ(
      Position(space, 1),
      StartOfWord(CreateVisiblePositionInDOMTree(*five, 1)).DeepEquivalent());
  EXPECT_EQ(
      PositionInFlatTree(space, 1),
      StartOfWord(CreateVisiblePositionInFlatTree(*five, 1)).DeepEquivalent());
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

TEST_F(VisibleUnitsTest,
       PreviousRootInlineBoxCandidatePositionWithDisplayNone) {
  SetBodyContent(
      "<div contenteditable>"
      "<div id=one>one abc</div>"
      "<div id=two>two <b id=none style=display:none>def</b> ghi</div>"
      "</div>");
  Element* const one = GetDocument().getElementById("one");
  Element* const two = GetDocument().getElementById("two");
  const VisiblePosition& visible_position =
      CreateVisiblePosition(Position::LastPositionInNode(two));
  EXPECT_EQ(Position(one->firstChild(), 7),
            PreviousRootInlineBoxCandidatePosition(
                two->lastChild(), visible_position, kContentIsEditable));
}

}  // namespace blink
