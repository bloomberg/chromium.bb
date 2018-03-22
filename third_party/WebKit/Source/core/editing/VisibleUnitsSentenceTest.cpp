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

class VisibleUnitsSentenceTest : public EditingTestBase {
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

TEST_F(VisibleUnitsSentenceTest, endOfSentence) {
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

TEST_F(VisibleUnitsSentenceTest, startOfSentence) {
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

}  // namespace blink
