// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/VisibleSelection.h"

#include "core/dom/Range.h"
#include "core/editing/EditingTestBase.h"
#include "core/editing/SelectionAdjuster.h"

#define LOREM_IPSUM                                                            \
  "Lorem ipsum dolor sit amet, consectetur adipisicing elit, sed do eiusmod "  \
  "tempor "                                                                    \
  "incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, "     \
  "quis nostrud "                                                              \
  "exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. "     \
  "Duis aute irure "                                                           \
  "dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat "    \
  "nulla pariatur."                                                            \
  "Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia " \
  "deserunt "                                                                  \
  "mollit anim id est laborum."

namespace blink {

class VisibleSelectionTest : public EditingTestBase {
 protected:
  // Helper function to set the VisibleSelection base/extent.
  template <typename Strategy>
  void SetSelection(VisibleSelectionTemplate<Strategy>& selection, int base) {
    SetSelection(selection, base, base);
  }

  // Helper function to set the VisibleSelection base/extent.
  template <typename Strategy>
  void SetSelection(VisibleSelectionTemplate<Strategy>& selection,
                    int base,
                    int extend) {
    Node* node = GetDocument().body()->firstChild();
    selection = CreateVisibleSelection(
        typename SelectionTemplate<Strategy>::Builder(selection.AsSelection())
            .Collapse(PositionTemplate<Strategy>(node, base))
            .Extend(PositionTemplate<Strategy>(node, extend))
            .Build());
  }
};

static void TestFlatTreePositionsToEqualToDOMTreePositions(
    const VisibleSelection& selection,
    const VisibleSelectionInFlatTree& selection_in_flat_tree) {
  // Since DOM tree positions can't be map to flat tree version, e.g.
  // shadow root, not distributed node, we map a position in flat tree
  // to DOM tree position.
  EXPECT_EQ(selection.Start(),
            ToPositionInDOMTree(selection_in_flat_tree.Start()));
  EXPECT_EQ(selection.End(), ToPositionInDOMTree(selection_in_flat_tree.End()));
  EXPECT_EQ(selection.Base(),
            ToPositionInDOMTree(selection_in_flat_tree.Base()));
  EXPECT_EQ(selection.Extent(),
            ToPositionInDOMTree(selection_in_flat_tree.Extent()));
}

template <typename Strategy>
VisibleSelectionTemplate<Strategy> ExpandUsingGranularity(
    const VisibleSelectionTemplate<Strategy>& selection,
    TextGranularity granularity) {
  return CreateVisibleSelectionWithGranularity(
      typename SelectionTemplate<Strategy>::Builder()
          .SetBaseAndExtent(selection.Base(), selection.Extent())
          .Build(),
      granularity);
}

// TODO(editing-dev): We should move this test to "SelectionControllerTest.cpp"
// For http://crbug.com/700368
TEST_F(VisibleSelectionTest, AdjustSelectionWithTrailingWhitespace) {
  SetBodyContent(
      "<input type=checkbox>"
      "<div style='user-select:none'>abc</div>");
  Element* const input = GetDocument().QuerySelector("input");

  // Simulate double-clicking "abc".
  // TODO(editing-dev): We should remove above comment once we fix [1].
  // [1] http://crbug.com/701657 double-click on user-select:none should not
  // compute selection.
  const VisibleSelectionInFlatTree& selection =
      CreateVisibleSelectionWithGranularity(
          SelectionInFlatTree::Builder()
              .Collapse(PositionInFlatTree::BeforeNode(*input))
              .Extend(PositionInFlatTree::AfterNode(*input))
              .Build(),
          TextGranularity::kWord);
  const SelectionInFlatTree& result =
      AdjustSelectionWithTrailingWhitespace(selection.AsSelection());

  EXPECT_EQ(PositionInFlatTree::BeforeNode(*input),
            result.ComputeStartPosition());
  EXPECT_EQ(PositionInFlatTree::AfterNode(*input), result.ComputeEndPosition());
}

TEST_F(VisibleSelectionTest, expandUsingGranularity) {
  const char* body_content =
      "<span id=host><a id=one>1</a><a id=two>22</a></span>";
  const char* shadow_content =
      "<p><b id=three>333</b><content select=#two></content><b "
      "id=four>4444</b><span id=space>  </span><content "
      "select=#one></content><b id=five>55555</b></p>";
  SetBodyContent(body_content);
  ShadowRoot* shadow_root = SetShadowContent(shadow_content, "host");

  Node* one = GetDocument().getElementById("one")->firstChild();
  Node* two = GetDocument().getElementById("two")->firstChild();
  Node* three = shadow_root->getElementById("three")->firstChild();
  Node* four = shadow_root->getElementById("four")->firstChild();
  Node* five = shadow_root->getElementById("five")->firstChild();

  VisibleSelection selection;
  VisibleSelectionInFlatTree selection_in_flat_tree;

  // From a position at distributed node
  selection = CreateVisibleSelection(
      SelectionInDOMTree::Builder().Collapse(Position(one, 1)).Build());
  selection = ExpandUsingGranularity(selection, TextGranularity::kWord);
  selection_in_flat_tree =
      CreateVisibleSelection(SelectionInFlatTree::Builder()
                                 .Collapse(PositionInFlatTree(one, 1))
                                 .Build());
  selection_in_flat_tree =
      ExpandUsingGranularity(selection_in_flat_tree, TextGranularity::kWord);

  EXPECT_EQ(selection.Start(), selection.Base());
  EXPECT_EQ(selection.End(), selection.Extent());
  EXPECT_EQ(Position(one, 0), selection.Start());
  EXPECT_EQ(Position(two, 2), selection.End());

  EXPECT_EQ(selection_in_flat_tree.Start(), selection_in_flat_tree.Base());
  EXPECT_EQ(selection_in_flat_tree.End(), selection_in_flat_tree.Extent());
  EXPECT_EQ(PositionInFlatTree(one, 0), selection_in_flat_tree.Start());
  EXPECT_EQ(PositionInFlatTree(five, 5), selection_in_flat_tree.End());

  // From a position at distributed node
  selection = CreateVisibleSelection(
      SelectionInDOMTree::Builder().Collapse(Position(two, 1)).Build());
  selection = ExpandUsingGranularity(selection, TextGranularity::kWord);
  selection_in_flat_tree =
      CreateVisibleSelection(SelectionInFlatTree::Builder()
                                 .Collapse(PositionInFlatTree(two, 1))
                                 .Build());
  selection_in_flat_tree =
      ExpandUsingGranularity(selection_in_flat_tree, TextGranularity::kWord);

  EXPECT_EQ(selection.Start(), selection.Base());
  EXPECT_EQ(selection.End(), selection.Extent());
  EXPECT_EQ(Position(one, 0), selection.Start());
  EXPECT_EQ(Position(two, 2), selection.End());

  EXPECT_EQ(selection_in_flat_tree.Start(), selection_in_flat_tree.Base());
  EXPECT_EQ(selection_in_flat_tree.End(), selection_in_flat_tree.Extent());
  EXPECT_EQ(PositionInFlatTree(three, 0), selection_in_flat_tree.Start());
  EXPECT_EQ(PositionInFlatTree(four, 4), selection_in_flat_tree.End());

  // From a position at node in shadow tree
  selection = CreateVisibleSelection(
      SelectionInDOMTree::Builder().Collapse(Position(three, 1)).Build());
  selection = ExpandUsingGranularity(selection, TextGranularity::kWord);
  selection_in_flat_tree =
      CreateVisibleSelection(SelectionInFlatTree::Builder()
                                 .Collapse(PositionInFlatTree(three, 1))
                                 .Build());
  selection_in_flat_tree =
      ExpandUsingGranularity(selection_in_flat_tree, TextGranularity::kWord);

  EXPECT_EQ(selection.Start(), selection.Base());
  EXPECT_EQ(selection.End(), selection.Extent());
  EXPECT_EQ(Position(three, 0), selection.Start());
  EXPECT_EQ(Position(four, 4), selection.End());

  EXPECT_EQ(selection_in_flat_tree.Start(), selection_in_flat_tree.Base());
  EXPECT_EQ(selection_in_flat_tree.End(), selection_in_flat_tree.Extent());
  EXPECT_EQ(PositionInFlatTree(three, 0), selection_in_flat_tree.Start());
  EXPECT_EQ(PositionInFlatTree(four, 4), selection_in_flat_tree.End());

  // From a position at node in shadow tree
  selection = CreateVisibleSelection(
      SelectionInDOMTree::Builder().Collapse(Position(four, 1)).Build());
  selection = ExpandUsingGranularity(selection, TextGranularity::kWord);
  selection_in_flat_tree =
      CreateVisibleSelection(SelectionInFlatTree::Builder()
                                 .Collapse(PositionInFlatTree(four, 1))
                                 .Build());
  selection_in_flat_tree =
      ExpandUsingGranularity(selection_in_flat_tree, TextGranularity::kWord);

  EXPECT_EQ(selection.Start(), selection.Base());
  EXPECT_EQ(selection.End(), selection.Extent());
  EXPECT_EQ(Position(three, 0), selection.Start());
  EXPECT_EQ(Position(four, 4), selection.End());

  EXPECT_EQ(selection_in_flat_tree.Start(), selection_in_flat_tree.Base());
  EXPECT_EQ(selection_in_flat_tree.End(), selection_in_flat_tree.Extent());
  EXPECT_EQ(PositionInFlatTree(three, 0), selection_in_flat_tree.Start());
  EXPECT_EQ(PositionInFlatTree(four, 4), selection_in_flat_tree.End());

  // From a position at node in shadow tree
  selection = CreateVisibleSelection(
      SelectionInDOMTree::Builder().Collapse(Position(five, 1)).Build());
  selection = ExpandUsingGranularity(selection, TextGranularity::kWord);
  selection_in_flat_tree =
      CreateVisibleSelection(SelectionInFlatTree::Builder()
                                 .Collapse(PositionInFlatTree(five, 1))
                                 .Build());
  selection_in_flat_tree =
      ExpandUsingGranularity(selection_in_flat_tree, TextGranularity::kWord);

  EXPECT_EQ(selection.Start(), selection.Base());
  EXPECT_EQ(selection.End(), selection.Extent());
  EXPECT_EQ(Position(five, 0), selection.Start());
  EXPECT_EQ(Position(five, 5), selection.End());

  EXPECT_EQ(selection_in_flat_tree.Start(), selection_in_flat_tree.Base());
  EXPECT_EQ(selection_in_flat_tree.End(), selection_in_flat_tree.Extent());
  EXPECT_EQ(PositionInFlatTree(one, 0), selection_in_flat_tree.Start());
  EXPECT_EQ(PositionInFlatTree(five, 5), selection_in_flat_tree.End());
}

// For http://wkb.ug/32622
TEST_F(VisibleSelectionTest, ExpandUsingGranularityWithEmptyCell) {
  SetBodyContent(
      "<div contentEditable><table cellspacing=0><tr>"
      "<td id='first' width='50' height='25pt'></td>"
      "<td id='second' width='50' height='25pt'></td>"
      "</tr></table></div>");
  Element* const first = GetDocument().getElementById("first");
  const VisibleSelectionInFlatTree& selection =
      CreateVisibleSelectionWithGranularity(
          SelectionInFlatTree::Builder()
              .Collapse(PositionInFlatTree(first, 0))
              .Build(),
          TextGranularity::kWord);
  EXPECT_EQ(PositionInFlatTree(first, 0), selection.Start());
  EXPECT_EQ(PositionInFlatTree(first, 0), selection.End());
}

TEST_F(VisibleSelectionTest, Initialisation) {
  SetBodyContent(LOREM_IPSUM);

  VisibleSelection selection;
  VisibleSelectionInFlatTree selection_in_flat_tree;
  SetSelection(selection, 0);
  SetSelection(selection_in_flat_tree, 0);

  EXPECT_FALSE(selection.IsNone());
  EXPECT_FALSE(selection_in_flat_tree.IsNone());
  EXPECT_TRUE(selection.IsCaret());
  EXPECT_TRUE(selection_in_flat_tree.IsCaret());

  Range* range = CreateRange(FirstEphemeralRangeOf(selection));
  EXPECT_EQ(0u, range->startOffset());
  EXPECT_EQ(0u, range->endOffset());
  EXPECT_EQ("", range->GetText());
  TestFlatTreePositionsToEqualToDOMTreePositions(selection,
                                                 selection_in_flat_tree);

  const VisibleSelection no_selection =
      CreateVisibleSelection(SelectionInDOMTree::Builder().Build());
  EXPECT_TRUE(no_selection.IsNone());
}

// For http://crbug.com/695317
TEST_F(VisibleSelectionTest, SelectAllWithInputElement) {
  SetBodyContent("<input>123");
  Element* const html_element = GetDocument().documentElement();
  Element* const input = GetDocument().QuerySelector("input");
  Node* const last_child = GetDocument().body()->lastChild();

  const VisibleSelection& visible_selection_in_dom_tree =
      CreateVisibleSelection(
          SelectionInDOMTree::Builder()
              .Collapse(Position::FirstPositionInNode(*html_element))
              .Extend(Position::LastPositionInNode(*html_element))
              .Build());
  EXPECT_EQ(SelectionInDOMTree::Builder()
                .Collapse(Position::BeforeNode(*input))
                .Extend(Position(last_child, 3))
                .Build(),
            visible_selection_in_dom_tree.AsSelection());

  const VisibleSelectionInFlatTree& visible_selection_in_flat_tree =
      CreateVisibleSelection(
          SelectionInFlatTree::Builder()
              .Collapse(PositionInFlatTree::FirstPositionInNode(*html_element))
              .Extend(PositionInFlatTree::LastPositionInNode(*html_element))
              .Build());
  EXPECT_EQ(SelectionInFlatTree::Builder()
                .Collapse(PositionInFlatTree::BeforeNode(*input))
                .Extend(PositionInFlatTree(last_child, 3))
                .Build(),
            visible_selection_in_flat_tree.AsSelection());
}

TEST_F(VisibleSelectionTest, ShadowCrossing) {
  const char* body_content =
      "<p id='host'>00<b id='one'>11</b><b id='two'>22</b>33</p>";
  const char* shadow_content =
      "<a><span id='s4'>44</span><content select=#two></content><span "
      "id='s5'>55</span><content select=#one></content><span "
      "id='s6'>66</span></a>";
  SetBodyContent(body_content);
  ShadowRoot* shadow_root = SetShadowContent(shadow_content, "host");

  Element* body = GetDocument().body();
  Element* host = body->QuerySelector("#host");
  Element* one = body->QuerySelector("#one");
  Element* six = shadow_root->QuerySelector("#s6");

  VisibleSelection selection = CreateVisibleSelection(
      SelectionInDOMTree::Builder()
          .Collapse(Position::FirstPositionInNode(*one))
          .Extend(Position::LastPositionInNode(*shadow_root))
          .Build());
  VisibleSelectionInFlatTree selection_in_flat_tree = CreateVisibleSelection(
      SelectionInFlatTree::Builder()
          .Collapse(PositionInFlatTree::FirstPositionInNode(*one))
          .Extend(PositionInFlatTree::LastPositionInNode(*host))
          .Build());

  EXPECT_EQ(Position(host, PositionAnchorType::kBeforeAnchor),
            selection.Start());
  EXPECT_EQ(Position(host, PositionAnchorType::kBeforeAnchor), selection.End());
  EXPECT_EQ(PositionInFlatTree(one->firstChild(), 0),
            selection_in_flat_tree.Start());
  EXPECT_EQ(PositionInFlatTree(six->firstChild(), 2),
            selection_in_flat_tree.End());
}

TEST_F(VisibleSelectionTest, ShadowV0DistributedNodes) {
  const char* body_content =
      "<p id='host'>00<b id='one'>11</b><b id='two'>22</b>33</p>";
  const char* shadow_content =
      "<a><span id='s4'>44</span><content select=#two></content><span "
      "id='s5'>55</span><content select=#one></content><span "
      "id='s6'>66</span></a>";
  SetBodyContent(body_content);
  ShadowRoot* shadow_root = SetShadowContent(shadow_content, "host");

  Element* body = GetDocument().body();
  Element* one = body->QuerySelector("#one");
  Element* two = body->QuerySelector("#two");
  Element* five = shadow_root->QuerySelector("#s5");

  VisibleSelection selection =
      CreateVisibleSelection(SelectionInDOMTree::Builder()
                                 .Collapse(Position::FirstPositionInNode(*one))
                                 .Extend(Position::LastPositionInNode(*two))
                                 .Build());
  VisibleSelectionInFlatTree selection_in_flat_tree = CreateVisibleSelection(
      SelectionInFlatTree::Builder()
          .Collapse(PositionInFlatTree::FirstPositionInNode(*one))
          .Extend(PositionInFlatTree::LastPositionInNode(*two))
          .Build());

  EXPECT_EQ(Position(one->firstChild(), 0), selection.Start());
  EXPECT_EQ(Position(two->firstChild(), 2), selection.End());
  EXPECT_EQ(PositionInFlatTree(five->firstChild(), 0),
            selection_in_flat_tree.Start());
  EXPECT_EQ(PositionInFlatTree(five->firstChild(), 2),
            selection_in_flat_tree.End());
}

TEST_F(VisibleSelectionTest, ShadowNested) {
  const char* body_content =
      "<p id='host'>00<b id='one'>11</b><b id='two'>22</b>33</p>";
  const char* shadow_content =
      "<a><span id='s4'>44</span><content select=#two></content><span "
      "id='s5'>55</span><content select=#one></content><span "
      "id='s6'>66</span></a>";
  const char* shadow_content2 =
      "<span id='s7'>77</span><content></content><span id='s8'>88</span>";
  SetBodyContent(body_content);
  ShadowRoot* shadow_root = SetShadowContent(shadow_content, "host");
  ShadowRoot* shadow_root2 = CreateShadowRootForElementWithIDAndSetInnerHTML(
      *shadow_root, "s5", shadow_content2);

  // Flat tree is something like below:
  //  <p id="host">
  //    <span id="s4">44</span>
  //    <b id="two">22</b>
  //    <span id="s5"><span id="s7">77>55</span id="s8">88</span>
  //    <b id="one">11</b>
  //    <span id="s6">66</span>
  //  </p>
  Element* body = GetDocument().body();
  Element* host = body->QuerySelector("#host");
  Element* one = body->QuerySelector("#one");
  Element* eight = shadow_root2->QuerySelector("#s8");

  VisibleSelection selection = CreateVisibleSelection(
      SelectionInDOMTree::Builder()
          .Collapse(Position::FirstPositionInNode(*one))
          .Extend(Position::LastPositionInNode(*shadow_root2))
          .Build());
  VisibleSelectionInFlatTree selection_in_flat_tree = CreateVisibleSelection(
      SelectionInFlatTree::Builder()
          .Collapse(PositionInFlatTree::FirstPositionInNode(*one))
          .Extend(PositionInFlatTree::AfterNode(*eight))
          .Build());

  EXPECT_EQ(Position(host, PositionAnchorType::kBeforeAnchor),
            selection.Start());
  EXPECT_EQ(Position(host, PositionAnchorType::kBeforeAnchor), selection.End());
  EXPECT_EQ(PositionInFlatTree(eight->firstChild(), 2),
            selection_in_flat_tree.Start());
  EXPECT_EQ(PositionInFlatTree(eight->firstChild(), 2),
            selection_in_flat_tree.End());
}

TEST_F(VisibleSelectionTest, WordGranularity) {
  SetBodyContent(LOREM_IPSUM);

  VisibleSelection selection;
  VisibleSelectionInFlatTree selection_in_flat_tree;

  // Beginning of a word.
  {
    SetSelection(selection, 0);
    SetSelection(selection_in_flat_tree, 0);
    selection = ExpandUsingGranularity(selection, TextGranularity::kWord);
    selection_in_flat_tree =
        ExpandUsingGranularity(selection_in_flat_tree, TextGranularity::kWord);

    Range* range = CreateRange(FirstEphemeralRangeOf(selection));
    EXPECT_EQ(0u, range->startOffset());
    EXPECT_EQ(5u, range->endOffset());
    EXPECT_EQ("Lorem", range->GetText());
    TestFlatTreePositionsToEqualToDOMTreePositions(selection,
                                                   selection_in_flat_tree);
  }

  // Middle of a word.
  {
    SetSelection(selection, 8);
    SetSelection(selection_in_flat_tree, 8);
    selection = ExpandUsingGranularity(selection, TextGranularity::kWord);
    selection_in_flat_tree =
        ExpandUsingGranularity(selection_in_flat_tree, TextGranularity::kWord);

    Range* range = CreateRange(FirstEphemeralRangeOf(selection));
    EXPECT_EQ(6u, range->startOffset());
    EXPECT_EQ(11u, range->endOffset());
    EXPECT_EQ("ipsum", range->GetText());
    TestFlatTreePositionsToEqualToDOMTreePositions(selection,
                                                   selection_in_flat_tree);
  }

  // End of a word.
  // FIXME: that sounds buggy, we might want to select the word _before_ instead
  // of the space...
  {
    SetSelection(selection, 5);
    SetSelection(selection_in_flat_tree, 5);
    selection = ExpandUsingGranularity(selection, TextGranularity::kWord);
    selection_in_flat_tree =
        ExpandUsingGranularity(selection_in_flat_tree, TextGranularity::kWord);

    Range* range = CreateRange(FirstEphemeralRangeOf(selection));
    EXPECT_EQ(5u, range->startOffset());
    EXPECT_EQ(6u, range->endOffset());
    EXPECT_EQ(" ", range->GetText());
    TestFlatTreePositionsToEqualToDOMTreePositions(selection,
                                                   selection_in_flat_tree);
  }

  // Before comma.
  // FIXME: that sounds buggy, we might want to select the word _before_ instead
  // of the comma.
  {
    SetSelection(selection, 26);
    SetSelection(selection_in_flat_tree, 26);
    selection = ExpandUsingGranularity(selection, TextGranularity::kWord);
    selection_in_flat_tree =
        ExpandUsingGranularity(selection_in_flat_tree, TextGranularity::kWord);

    Range* range = CreateRange(FirstEphemeralRangeOf(selection));
    EXPECT_EQ(26u, range->startOffset());
    EXPECT_EQ(27u, range->endOffset());
    EXPECT_EQ(",", range->GetText());
    TestFlatTreePositionsToEqualToDOMTreePositions(selection,
                                                   selection_in_flat_tree);
  }

  // After comma.
  {
    SetSelection(selection, 27);
    SetSelection(selection_in_flat_tree, 27);
    selection = ExpandUsingGranularity(selection, TextGranularity::kWord);
    selection_in_flat_tree =
        ExpandUsingGranularity(selection_in_flat_tree, TextGranularity::kWord);

    Range* range = CreateRange(FirstEphemeralRangeOf(selection));
    EXPECT_EQ(27u, range->startOffset());
    EXPECT_EQ(28u, range->endOffset());
    EXPECT_EQ(" ", range->GetText());
    TestFlatTreePositionsToEqualToDOMTreePositions(selection,
                                                   selection_in_flat_tree);
  }

  // When selecting part of a word.
  {
    SetSelection(selection, 0, 1);
    SetSelection(selection_in_flat_tree, 0, 1);
    selection = ExpandUsingGranularity(selection, TextGranularity::kWord);
    selection_in_flat_tree =
        ExpandUsingGranularity(selection_in_flat_tree, TextGranularity::kWord);

    Range* range = CreateRange(FirstEphemeralRangeOf(selection));
    EXPECT_EQ(0u, range->startOffset());
    EXPECT_EQ(5u, range->endOffset());
    EXPECT_EQ("Lorem", range->GetText());
    TestFlatTreePositionsToEqualToDOMTreePositions(selection,
                                                   selection_in_flat_tree);
  }

  // When selecting part of two words.
  {
    SetSelection(selection, 2, 8);
    SetSelection(selection_in_flat_tree, 2, 8);
    selection = ExpandUsingGranularity(selection, TextGranularity::kWord);
    selection_in_flat_tree =
        ExpandUsingGranularity(selection_in_flat_tree, TextGranularity::kWord);

    Range* range = CreateRange(FirstEphemeralRangeOf(selection));
    EXPECT_EQ(0u, range->startOffset());
    EXPECT_EQ(11u, range->endOffset());
    EXPECT_EQ("Lorem ipsum", range->GetText());
    TestFlatTreePositionsToEqualToDOMTreePositions(selection,
                                                   selection_in_flat_tree);
  }
}

// This is for crbug.com/627783, simulating restoring selection
// in undo stack.
TEST_F(VisibleSelectionTest, updateIfNeededWithShadowHost) {
  SetBodyContent("<div id=host></div><div id=sample>foo</div>");
  SetShadowContent("<content>", "host");
  Element* sample = GetDocument().getElementById("sample");

  // Simulates saving selection in undo stack.
  VisibleSelection selection =
      CreateVisibleSelection(SelectionInDOMTree::Builder()
                                 .Collapse(Position(sample->firstChild(), 0))
                                 .Build());
  EXPECT_EQ(Position(sample->firstChild(), 0), selection.Start());

  // Simulates modifying DOM tree to invalidate distribution.
  Element* host = GetDocument().getElementById("host");
  host->AppendChild(sample);
  GetDocument().UpdateStyleAndLayout();

  // Simulates to restore selection from undo stack.
  selection = CreateVisibleSelection(selection.AsSelection());
  EXPECT_EQ(Position(sample->firstChild(), 0), selection.Start());
}

}  // namespace blink
