// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/EphemeralRange.h"

#include "core/dom/Range.h"
#include "core/editing/EditingTestBase.h"
#include <sstream>

namespace blink {

class EphemeralRangeTest : public EditingTestBase {
 protected:
  template <typename Traversal = NodeTraversal>
  std::string traverseRange(Range*) const;

  template <typename Strategy>
  std::string traverseRange(const EphemeralRangeTemplate<Strategy>&) const;

  Range* getBodyRange() const;
};

template <typename Traversal>
std::string EphemeralRangeTest::traverseRange(Range* range) const {
  std::stringstream nodesContent;
  for (Node* node = range->firstNode(); node != range->pastLastNode();
       node = Traversal::next(*node)) {
    nodesContent << "[" << *node << "]";
  }

  return nodesContent.str();
}

template <typename Strategy>
std::string EphemeralRangeTest::traverseRange(
    const EphemeralRangeTemplate<Strategy>& range) const {
  std::stringstream nodesContent;
  for (const Node& node : range.nodes())
    nodesContent << "[" << node << "]";

  return nodesContent.str();
}

Range* EphemeralRangeTest::getBodyRange() const {
  Range* range = Range::create(document());
  range->selectNode(document().body());
  return range;
}

// Tests that |EphemeralRange::nodes()| will traverse the whole range exactly as
// |for (Node* n = firstNode(); n != pastLastNode(); n = Traversal::next(*n))|
// does.
TEST_F(EphemeralRangeTest, rangeTraversal) {
  const char* bodyContent =
      "<p id='host'><b id='one'></b><b id='two'>22</b></p>";
  setBodyContent(bodyContent);

  const std::string expectedNodes(
      "[BODY][P id=\"host\"][B id=\"one\"][B id=\"two\"][#text \"22\"]");

  // Check two ways to traverse.
  EXPECT_EQ(expectedNodes, traverseRange<>(getBodyRange()));
  EXPECT_EQ(traverseRange<>(getBodyRange()),
            traverseRange(EphemeralRange(getBodyRange())));

  // The same with FlatTree traversing.
  EXPECT_EQ(expectedNodes, traverseRange<FlatTreeTraversal>(getBodyRange()));
  EXPECT_EQ(traverseRange<FlatTreeTraversal>(getBodyRange()),
            traverseRange(EphemeralRangeInFlatTree(getBodyRange())));
}

// Tests that |inRange| helper will traverse the whole range with shadow DOM.
TEST_F(EphemeralRangeTest, rangeShadowTraversal) {
  const char* bodyContent =
      "<p id='host'><b id='one'></b><input type='text' value='some'></p>";
  setBodyContent(bodyContent);

  EXPECT_EQ("[BODY][P id=\"host\"][B id=\"one\"][INPUT]",
            traverseRange<>(getBodyRange()));
  EXPECT_EQ(traverseRange<>(getBodyRange()),
            traverseRange(EphemeralRange(getBodyRange())));

  // In this case FlatTree traverse should differs from DOM tree traverse.
  EXPECT_EQ(
      "[BODY][P id=\"host\"][B id=\"one\"][INPUT][DIV id=\"inner-editor\" "
      "(editable)][#text \"some\"]",
      traverseRange<FlatTreeTraversal>(getBodyRange()));
  EXPECT_EQ(traverseRange<FlatTreeTraversal>(getBodyRange()),
            traverseRange(EphemeralRangeInFlatTree(getBodyRange())));
}

// Limit a range and check that it will be traversed correctly.
TEST_F(EphemeralRangeTest, rangeTraversalLimited) {
  const char* bodyContent =
      "<p id='host'><b id='one'></b><input type='text' value='some'><span "
      "id='two'></p>";
  setBodyContent(bodyContent);

  // Get a limited range from <body> to <b> nodes.
  Range* untilB = getBodyRange();
  untilB->setEnd(document().getElementById("one"), 0,
                 IGNORE_EXCEPTION_FOR_TESTING);
  EXPECT_EQ("[BODY][P id=\"host\"][B id=\"one\"]", traverseRange<>(untilB));

  EXPECT_EQ(traverseRange<>(untilB), traverseRange(EphemeralRange(untilB)));

  EXPECT_EQ("[BODY][P id=\"host\"][B id=\"one\"]",
            traverseRange<FlatTreeTraversal>(untilB));
  EXPECT_EQ(traverseRange<FlatTreeTraversal>(untilB),
            traverseRange(EphemeralRangeInFlatTree(untilB)));

  // Get a limited range from <b> to <span> nodes.
  Range* fromBToSpan = getBodyRange();
  fromBToSpan->setStart(document().getElementById("one"), 0,
                        IGNORE_EXCEPTION_FOR_TESTING);
  fromBToSpan->setEnd(document().getElementById("two"), 0,
                      IGNORE_EXCEPTION_FOR_TESTING);

  EXPECT_EQ("[B id=\"one\"][INPUT][SPAN id=\"two\"]",
            traverseRange<>(fromBToSpan));
  EXPECT_EQ(traverseRange<>(fromBToSpan),
            traverseRange(EphemeralRange(fromBToSpan)));

  EXPECT_EQ(
      "[B id=\"one\"][INPUT][DIV id=\"inner-editor\" (editable)][#text "
      "\"some\"][SPAN id=\"two\"]",
      traverseRange<FlatTreeTraversal>(fromBToSpan));
  EXPECT_EQ(traverseRange<FlatTreeTraversal>(fromBToSpan),
            traverseRange(EphemeralRangeInFlatTree(fromBToSpan)));
}

TEST_F(EphemeralRangeTest, traversalEmptyRanges) {
  const char* bodyContent = "<p id='host'><b id='one'></b></p>";
  setBodyContent(bodyContent);

  // Expect no iterations in loop for an empty EphemeralRange.
  EXPECT_EQ(std::string(), traverseRange(EphemeralRange()));

  auto iterable = EphemeralRange().nodes();
  // Tree iterators have only |operator !=| ATM.
  EXPECT_FALSE(iterable.begin() != iterable.end());

  const EphemeralRange singlePositionRange(getBodyRange()->startPosition());
  EXPECT_FALSE(singlePositionRange.isNull());
  EXPECT_EQ(std::string(), traverseRange(singlePositionRange));
  EXPECT_EQ(singlePositionRange.startPosition().nodeAsRangeFirstNode(),
            singlePositionRange.endPosition().nodeAsRangePastLastNode());
}

TEST_F(EphemeralRangeTest, commonAncesstorDOM) {
  const char* bodyContent =
      "<p id='host'>00"
      "<b id='one'>11</b>"
      "<b id='two'>22</b>"
      "<b id='three'>33</b>"
      "</p>";
  setBodyContent(bodyContent);

  const Position startPosition(document().getElementById("one"), 0);
  const Position endPosition(document().getElementById("two"), 0);
  const EphemeralRange range(startPosition, endPosition);
  EXPECT_EQ(document().getElementById("host"), range.commonAncestorContainer());
}

TEST_F(EphemeralRangeTest, commonAncesstorFlatTree) {
  const char* bodyContent =
      "<b id='zero'>0</b>"
      "<p id='host'>"
      "<b id='one'>1</b>"
      "<b id='two'>22</b>"
      "</p>"
      "<b id='three'>333</b>";
  const char* shadowContent =
      "<p id='four'>4444</p>"
      "<content select=#two></content>"
      "<content select=#one></content>"
      "<p id='five'>55555</p>";
  setBodyContent(bodyContent);
  ShadowRoot* shadowRoot = setShadowContent(shadowContent, "host");

  const PositionInFlatTree startPosition(document().getElementById("one"), 0);
  const PositionInFlatTree endPosition(shadowRoot->getElementById("five"), 0);
  const EphemeralRangeInFlatTree range(startPosition, endPosition);
  EXPECT_EQ(document().getElementById("host"), range.commonAncestorContainer());
}

}  // namespace blink
