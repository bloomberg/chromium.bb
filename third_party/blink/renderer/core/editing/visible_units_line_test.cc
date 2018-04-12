// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/editing/visible_units.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/core/editing/testing/editing_test_base.h"
#include "third_party/blink/renderer/core/editing/visible_position.h"
#include "third_party/blink/renderer/core/html/forms/text_control_element.h"
#include "third_party/blink/renderer/core/layout/layout_text_fragment.h"

namespace blink {

class VisibleUnitsLineTest : public EditingTestBase {
 protected:
  static PositionWithAffinity PositionWithAffinityInDOMTree(
      Node& anchor,
      int offset,
      TextAffinity affinity = TextAffinity::kDownstream) {
    return PositionWithAffinity(CanonicalPositionOf(Position(&anchor, offset)),
                                affinity);
  }

  static VisiblePosition CreateVisiblePositionInDOMTree(
      Node& anchor,
      int offset,
      TextAffinity affinity = TextAffinity::kDownstream) {
    return CreateVisiblePosition(Position(&anchor, offset), affinity);
  }

  static PositionInFlatTreeWithAffinity PositionWithAffinityInFlatTree(
      Node& anchor,
      int offset,
      TextAffinity affinity = TextAffinity::kDownstream) {
    return PositionInFlatTreeWithAffinity(
        CanonicalPositionOf(PositionInFlatTree(&anchor, offset)), affinity);
  }

  static VisiblePositionInFlatTree CreateVisiblePositionInFlatTree(
      Node& anchor,
      int offset,
      TextAffinity affinity = TextAffinity::kDownstream) {
    return CreateVisiblePosition(PositionInFlatTree(&anchor, offset), affinity);
  }
};

TEST_F(VisibleUnitsLineTest, endOfLine) {
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

TEST_F(VisibleUnitsLineTest, isEndOfLine) {
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

TEST_F(VisibleUnitsLineTest, isLogicalEndOfLine) {
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

TEST_F(VisibleUnitsLineTest, inSameLine) {
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

TEST_F(VisibleUnitsLineTest, isStartOfLine) {
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

TEST_F(VisibleUnitsLineTest, logicalEndOfLine) {
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

TEST_F(VisibleUnitsLineTest, logicalStartOfLine) {
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

TEST_F(VisibleUnitsLineTest, startOfLine) {
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

TEST_F(VisibleUnitsLineTest,
       PreviousRootInlineBoxCandidatePositionWithDisplayNone) {
  SetBodyContent(
      "<div contenteditable>"
      "<div id=one>one abc</div>"
      "<div id=two>two <b id=none style=display:none>def</b> ghi</div>"
      "</div>");
  Element* const one = GetDocument().getElementById("one");
  Element* const two = GetDocument().getElementById("two");
  const VisiblePosition& visible_position =
      CreateVisiblePosition(Position::LastPositionInNode(*two));
  EXPECT_EQ(Position(one->firstChild(), 7),
            PreviousRootInlineBoxCandidatePosition(
                two->lastChild(), visible_position, kContentIsEditable));
}

TEST_F(VisibleUnitsLineTest, InSameLineSkippingEmptyEditableDiv) {
  // This test records the InSameLine() results in
  // editing/selection/skip-over-contenteditable.html
  SetBodyContent(
      "<p id=foo>foo</p>"
      "<div contenteditable></div>"
      "<p id=bar>bar</p>");
  const Node* const foo = GetElementById("foo")->firstChild();
  const Node* const bar = GetElementById("bar")->firstChild();

  EXPECT_TRUE(InSameLine(
      PositionWithAffinity(Position(foo, 3), TextAffinity::kDownstream),
      PositionWithAffinity(Position(foo, 3), TextAffinity::kUpstream)));
  EXPECT_FALSE(InSameLine(
      PositionWithAffinity(Position(bar, 0), TextAffinity::kDownstream),
      PositionWithAffinity(Position(foo, 3), TextAffinity::kDownstream)));
  EXPECT_TRUE(InSameLine(
      PositionWithAffinity(Position(bar, 3), TextAffinity::kDownstream),
      PositionWithAffinity(Position(bar, 3), TextAffinity::kUpstream)));
  EXPECT_FALSE(InSameLine(
      PositionWithAffinity(Position(foo, 0), TextAffinity::kDownstream),
      PositionWithAffinity(Position(bar, 0), TextAffinity::kDownstream)));
}

}  // namespace blink
