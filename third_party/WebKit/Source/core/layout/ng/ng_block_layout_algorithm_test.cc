// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_block_layout_algorithm.h"

#include "core/dom/NodeComputedStyle.h"
#include "core/dom/TagCollection.h"
#include "core/layout/LayoutTestHelper.h"
#include "core/layout/ng/layout_ng_block_flow.h"
#include "core/layout/ng/ng_block_break_token.h"
#include "core/layout/ng/ng_block_node.h"
#include "core/layout/ng/ng_constraint_space.h"
#include "core/layout/ng/ng_constraint_space_builder.h"
#include "core/layout/ng/ng_floating_object.h"
#include "core/layout/ng/ng_length_utils.h"
#include "core/layout/ng/ng_physical_box_fragment.h"
#include "core/layout/ng/ng_physical_fragment.h"
#include "core/layout/ng/ng_units.h"
#include "core/style/ComputedStyle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace {

using testing::ElementsAre;
using testing::Pointee;

RefPtr<NGConstraintSpace> ConstructConstraintSpace(
    NGWritingMode writing_mode,
    TextDirection direction,
    NGLogicalSize size,
    bool shrink_to_fit = false,
    LayoutUnit fragmentainer_space_available = LayoutUnit()) {
  NGFragmentationType block_fragmentation =
      fragmentainer_space_available != LayoutUnit()
          ? NGFragmentationType::kFragmentColumn
          : NGFragmentationType::kFragmentNone;

  return NGConstraintSpaceBuilder(writing_mode)
      .SetAvailableSize(size)
      .SetPercentageResolutionSize(size)
      .SetInitialContainingBlockSize(size.ConvertToPhysical(writing_mode))
      .SetTextDirection(direction)
      .SetIsShrinkToFit(shrink_to_fit)
      .SetFragmentainerSpaceAvailable(fragmentainer_space_available)
      .SetFragmentationType(block_fragmentation)
      .ToConstraintSpace(writing_mode);
}

typedef bool TestParamLayoutNG;
class NGBlockLayoutAlgorithmTest
    : public ::testing::WithParamInterface<TestParamLayoutNG>,
      public RenderingTest {
 public:
  NGBlockLayoutAlgorithmTest() {
    RuntimeEnabledFeatures::setLayoutNGEnabled(true);
    RuntimeEnabledFeatures::setLayoutNGInlineEnabled(true);
  }
  ~NGBlockLayoutAlgorithmTest() {
    RuntimeEnabledFeatures::setLayoutNGEnabled(false);
    RuntimeEnabledFeatures::setLayoutNGInlineEnabled(false);
  }

 protected:
  void SetUp() override {
    style_ = ComputedStyle::create();
    RenderingTest::SetUp();
    enableCompositing();
  }

  RefPtr<NGPhysicalBoxFragment> RunBlockLayoutAlgorithm(
      NGConstraintSpace* space,
      NGBlockNode* node) {
    RefPtr<NGLayoutResult> result =
        NGBlockLayoutAlgorithm(node, space).Layout();

    return toNGPhysicalBoxFragment(result->PhysicalFragment().get());
  }

  std::pair<RefPtr<NGPhysicalBoxFragment>, RefPtr<NGConstraintSpace>>
  RunBlockLayoutAlgorithmForElement(Element* element) {
    LayoutNGBlockFlow* block_flow =
        toLayoutNGBlockFlow(element->layoutObject());
    NGBlockNode* node = new NGBlockNode(block_flow);
    RefPtr<NGConstraintSpace> space =
        NGConstraintSpace::CreateFromLayoutObject(*block_flow);

    RefPtr<NGLayoutResult> result =
        NGBlockLayoutAlgorithm(node, space.get()).Layout();
    return std::make_pair(
        toNGPhysicalBoxFragment(result->PhysicalFragment().get()), space);
  }

  MinAndMaxContentSizes RunComputeMinAndMax(NGBlockNode* node) {
    // The constraint space is not used for min/max computation, but we need
    // it to create the algorithm.
    RefPtr<NGConstraintSpace> space =
        ConstructConstraintSpace(kHorizontalTopBottom, TextDirection::kLtr,
                                 NGLogicalSize(LayoutUnit(), LayoutUnit()));

    NGBlockLayoutAlgorithm algorithm(node, space.get());
    EXPECT_TRUE(algorithm.ComputeMinAndMaxContentSizes().has_value());
    return *algorithm.ComputeMinAndMaxContentSizes();
  }

  RefPtr<ComputedStyle> style_;
};

TEST_F(NGBlockLayoutAlgorithmTest, FixedSize) {
  setBodyInnerHTML(R"HTML(
    <div id="box" style="width:30px; height:40px"></div>
  )HTML");

  RefPtr<NGConstraintSpace> space = ConstructConstraintSpace(
      kHorizontalTopBottom, TextDirection::kLtr,
      NGLogicalSize(LayoutUnit(100), NGSizeIndefinite));

  auto* box = new NGBlockNode(getLayoutObjectByElementId("box"));

  RefPtr<NGPhysicalFragment> frag = RunBlockLayoutAlgorithm(space.get(), box);

  EXPECT_EQ(LayoutUnit(30), frag->Width());
  EXPECT_EQ(LayoutUnit(40), frag->Height());
}

// Verifies that two children are laid out with the correct size and position.
TEST_F(NGBlockLayoutAlgorithmTest, LayoutBlockChildren) {
  setBodyInnerHTML(R"HTML(
    <div id="container" style="width: 30px">
      <div style="height: 20px">
      </div>
      <div style="height: 30px; margin-top: 5px; margin-bottom: 20px">
      </div>
    </div>
  )HTML");
  const int kWidth = 30;
  const int kHeight1 = 20;
  const int kHeight2 = 30;
  const int kMarginTop = 5;

  auto* container = new NGBlockNode(getLayoutObjectByElementId("container"));
  RefPtr<NGConstraintSpace> space = ConstructConstraintSpace(
      kHorizontalTopBottom, TextDirection::kLtr,
      NGLogicalSize(LayoutUnit(100), NGSizeIndefinite));

  RefPtr<NGPhysicalBoxFragment> frag =
      RunBlockLayoutAlgorithm(space.get(), container);

  EXPECT_EQ(LayoutUnit(kWidth), frag->Width());
  EXPECT_EQ(LayoutUnit(kHeight1 + kHeight2 + kMarginTop), frag->Height());
  EXPECT_EQ(NGPhysicalFragment::kFragmentBox, frag->Type());
  ASSERT_EQ(frag->Children().size(), 2UL);

  const NGPhysicalFragment* first_child_fragment = frag->Children()[0].get();
  EXPECT_EQ(kHeight1, first_child_fragment->Height());
  EXPECT_EQ(0, first_child_fragment->TopOffset());

  const NGPhysicalFragment* second_child_fragment = frag->Children()[1].get();
  EXPECT_EQ(kHeight2, second_child_fragment->Height());
  EXPECT_EQ(kHeight1 + kMarginTop, second_child_fragment->TopOffset());
}

// Verifies that a child is laid out correctly if it's writing mode is different
// from the parent's one.
TEST_F(NGBlockLayoutAlgorithmTest, LayoutBlockChildrenWithWritingMode) {
  setBodyInnerHTML(R"HTML(
    <style>
      #div2 {
        width: 50px;
        height: 50px;
        margin-left: 100px;
        writing-mode: horizontal-tb;
      }
    </style>
    <div id="container">
      <div id="div1" style="writing-mode: vertical-lr;">
        <div id="div2">
        </div>
      </div>
    </div>
  )HTML");
  const int kHeight = 50;
  const int kMarginLeft = 100;

  auto* container = new NGBlockNode(getLayoutObjectByElementId("container"));
  RefPtr<NGConstraintSpace> space =
      ConstructConstraintSpace(kHorizontalTopBottom, TextDirection::kLtr,
                               NGLogicalSize(LayoutUnit(500), LayoutUnit(500)));
  RefPtr<NGPhysicalBoxFragment> frag =
      RunBlockLayoutAlgorithm(space.get(), container);

  const NGPhysicalFragment* child = frag->Children()[0].get();
  // DIV2
  child = static_cast<const NGPhysicalBoxFragment*>(child)->Children()[0].get();

  EXPECT_EQ(kHeight, child->Height());
  EXPECT_EQ(0, child->TopOffset());
  EXPECT_EQ(kMarginLeft, child->LeftOffset());
}

// Verifies that floats are positioned at the top of the first child that can
// determine its position after margins collapsed.
TEST_F(NGBlockLayoutAlgorithmTest, CollapsingMarginsCase1WithFloats) {
  setBodyInnerHTML(R"HTML(
      <style>
        #container {
          height: 200px;
          width: 200px;
          margin-top: 10px;
          padding: 0 7px;
          background-color: red;
        }
        #first-child {
          margin-top: 20px;
          height: 10px;
          background-color: blue;
        }
        #float-child-left {
          float: left;
          height: 10px;
          width: 10px;
          padding: 10px;
          margin: 10px;
          background-color: green;
        }
        #float-child-right {
          float: right;
          height: 30px;
          width: 30px;
          background-color: pink;
        }
      </style>
      <div id='container'>
        <div id='float-child-left'></div>
        <div id='float-child-right'></div>
        <div id='first-child'></div>
      </div>
    )HTML");

  // ** Run LayoutNG algorithm **
  RefPtr<NGConstraintSpace> space;
  RefPtr<NGPhysicalBoxFragment> fragment;
  std::tie(fragment, space) = RunBlockLayoutAlgorithmForElement(
      document().getElementsByTagName("html")->item(0));
  ASSERT_EQ(fragment->Children().size(), 1UL);
  auto* body_fragment = toNGPhysicalBoxFragment(fragment->Children()[0].get());
  // 20 = max(first child's margin top, containers's margin top)
  int body_top_offset = 20;
  EXPECT_THAT(LayoutUnit(body_top_offset), body_fragment->TopOffset());
  // 8 = body's margin
  int body_left_offset = 8;
  EXPECT_THAT(LayoutUnit(body_left_offset), body_fragment->LeftOffset());
  ASSERT_EQ(1UL, body_fragment->Children().size());
  auto* container_fragment =
      toNGPhysicalBoxFragment(body_fragment->Children()[0].get());
  // 0 = collapsed with body's margin
  EXPECT_THAT(LayoutUnit(0), container_fragment->TopOffset());
  ASSERT_EQ(1UL, container_fragment->Children().size());
  auto* first_child_fragment =
      toNGPhysicalBoxFragment(container_fragment->Children()[0].get());
  // 0 = collapsed with container's margin
  EXPECT_THAT(LayoutUnit(0), first_child_fragment->TopOffset());

  // ** Verify layout tree **
  Element* first_child = document().getElementById("first-child");
  int first_child_block_offset = body_top_offset;
  EXPECT_EQ(first_child_block_offset, first_child->offsetTop());

  // float-child-left is positioned at the top edge of the container padding box
  Element* float_child_left = document().getElementById("float-child-left");
  // 30 = std::max(first-child's margin 20, container's margin 10,
  //      body's margin 8) + float-child-left's margin 10
  int float_child_left_block_offset = 30;
  EXPECT_EQ(float_child_left_block_offset, float_child_left->offsetTop());

  // float-child-right is positioned at the top edge of container padding box
  Element* float_child_right = document().getElementById("float-child-right");
  // Should be equal to first_child_block_offset
  // 20 = std::max(first-child's margin 20, container's margin 10,
  //      body's margin 8)
  int float_child_right_block_offset = 20;
  EXPECT_EQ(float_child_right_block_offset, float_child_right->offsetTop());

  // ** Verify exclusions **
  // float-child-left's height(10) + padding(2x10) + margin(2x10) = 50px
  NGLogicalSize exclusion1_size = {LayoutUnit(50), LayoutUnit(50)};
  // float-child-left's inline offset
  // 15 = body's margin(8) + container's inline padding(7)
  NGLogicalOffset exclusion1_offset = {LayoutUnit(15),
                                       LayoutUnit(first_child_block_offset)};
  NGLogicalRect exclusion1_rect = {exclusion1_offset, exclusion1_size};
  NGExclusion expected_exclusion1 = {exclusion1_rect, NGExclusion::kFloatLeft};

  NGLogicalSize exclusion2_size = {LayoutUnit(30), LayoutUnit(30)};
  // float-child-right's inline offset
  // right_float_offset = 200 container's width - right float width 30 = 170
  // 185 = body's margin(8) + right_float_offset(170) + container's padding(7)
  NGLogicalOffset exclusion2_offset = {LayoutUnit(185),
                                       LayoutUnit(first_child_block_offset)};
  NGLogicalRect exclusion2_rect = {exclusion2_offset, exclusion2_size};
  NGExclusion expected_exclusion2 = {exclusion2_rect, NGExclusion::kFloatRight};

  EXPECT_THAT(space->Exclusions()->storage,
              (ElementsAre(Pointee(expected_exclusion1),
                           Pointee(expected_exclusion2))));
}

// Verifies the collapsing margins case for the next pairs:
// - bottom margin of box and top margin of its next in-flow following sibling.
// - top and bottom margins of a box that does not establish a new block
//   formatting context and that has zero computed 'min-height', zero or 'auto'
//   computed 'height', and no in-flow children
TEST_F(NGBlockLayoutAlgorithmTest, CollapsingMarginsCase2WithFloats) {
  setBodyInnerHTML(R"HTML(
      <style>
      #first-child {
        background-color: red;
        height: 50px;
        margin-bottom: 20px;
      }
      #float-between-empties {
        background-color: green;
        float: left;
        height: 30px;
        width: 30px;
      }
      #float-between-nonempties {
        background-color: lightgreen;
        float: left;
        height: 40px;
        width: 40px;
      }
      #float-top-align {
        background-color: seagreen;
        float: left;
        height: 50px;
        width: 50px;
      }
      #second-child {
        background-color: blue;
        height: 50px;
        margin-top: 10px;
      }
      </style>
      <div id='first-child'>
        <div id='empty1' style='margin-bottom: -15px'></div>
        <div id='float-between-empties'></div>
        <div id='empty2'></div>
      </div>
      <div id='float-between-nonempties'></div>
      <div id='second-child'>
        <div id='float-top-align'></div>
        <div id='empty3'></div>
        <div id='empty4' style='margin-top: -30px'></div>
      </div>
      <div id='empty5'></div>
    )HTML");

  // ** Run LayoutNG algorithm **
  RefPtr<NGConstraintSpace> space;
  RefPtr<NGPhysicalBoxFragment> fragment;
  std::tie(fragment, space) = RunBlockLayoutAlgorithmForElement(
      document().getElementsByTagName("html")->item(0));

  auto* body_fragment = toNGPhysicalBoxFragment(fragment->Children()[0].get());
  // -7 = empty1's margin(-15) + body's margin(8)
  int body_top_offset = -7;
  EXPECT_THAT(LayoutUnit(body_top_offset), body_fragment->TopOffset());
  int body_left_offset = 8;
  EXPECT_THAT(LayoutUnit(body_top_offset), body_fragment->TopOffset());
  ASSERT_EQ(3UL, body_fragment->Children().size());

  auto* first_child_fragment =
      toNGPhysicalBoxFragment(body_fragment->Children()[0].get());
  EXPECT_THAT(LayoutUnit(), first_child_fragment->TopOffset());

  auto* second_child_fragment =
      toNGPhysicalBoxFragment(body_fragment->Children()[1].get());
  // 40 = first_child's height(50) - margin's collapsing result(10)
  int second_child_block_offset = 40;
  EXPECT_THAT(LayoutUnit(second_child_block_offset),
              second_child_fragment->TopOffset());

  auto* empty5_fragment =
      toNGPhysicalBoxFragment(body_fragment->Children()[2].get());
  // 90 = first_child's height(50) + collapsed margins(-10) +
  // second child's height(50)
  int empty5_fragment_block_offset = 90;
  EXPECT_THAT(LayoutUnit(empty5_fragment_block_offset),
              empty5_fragment->TopOffset());

  ASSERT_EQ(1UL, body_fragment->PositionedFloats().size());
  ASSERT_EQ(1UL, body_fragment->PositionedFloats().size());
  auto float_nonempties_fragment =
      body_fragment->PositionedFloats().at(0)->fragment;
  // 70 = first_child's height(50) + first child's margin-bottom(20)
  EXPECT_THAT(LayoutUnit(70), float_nonempties_fragment->TopOffset());
  EXPECT_THAT(LayoutUnit(0), float_nonempties_fragment->LeftOffset());

  // ** Verify layout tree **
  Element* first_child = document().getElementById("first-child");
  // -7 = body_top_offset
  EXPECT_EQ(body_top_offset, first_child->offsetTop());

  NGLogicalSize float_empties_exclusion_size = {LayoutUnit(30), LayoutUnit(30)};
  NGLogicalOffset float_empties_exclusion_offset = {
      LayoutUnit(body_left_offset), LayoutUnit(body_top_offset)};
  NGLogicalRect float_empties_exclusion_rect = {float_empties_exclusion_offset,
                                                float_empties_exclusion_size};
  NGExclusion float_empties_exclusion = {float_empties_exclusion_rect,
                                         NGExclusion::kFloatLeft};

  NGLogicalSize float_nonempties_exclusion_size = {LayoutUnit(40),
                                                   LayoutUnit(40)};
  // 63 = first_child_margin_strut(20) + first-child's height(50) +
  // body_top_offset(-7)
  NGLogicalOffset float_nonempties_exclusion_offset = {
      LayoutUnit(body_left_offset), LayoutUnit(63)};
  NGLogicalRect float_nonempties_exclusion_rect = {
      float_nonempties_exclusion_offset, float_nonempties_exclusion_size};
  NGExclusion float_nonempties_exclusion = {float_nonempties_exclusion_rect,
                                            NGExclusion::kFloatLeft};

  NGLogicalSize float_top_align_exclusion_size = {LayoutUnit(50),
                                                  LayoutUnit(50)};
  // 63 = float_nonempties_exclusion_offset because of the top edge alignment
  // rule.
  // 48 = body's margin + float_nonempties_exclusion_size
  NGLogicalOffset float_top_align_exclusion_offset = {LayoutUnit(48),
                                                      LayoutUnit(63)};
  NGLogicalRect float_top_align_exclusion_rect = {
      float_top_align_exclusion_offset, float_top_align_exclusion_size};
  NGExclusion float_top_align_exclusion = {float_top_align_exclusion_rect,
                                           NGExclusion::kFloatLeft};

  EXPECT_THAT(space->Exclusions()->storage,
              (ElementsAre(Pointee(float_empties_exclusion),
                           Pointee(float_nonempties_exclusion),
                           Pointee(float_top_align_exclusion))));
}

// Verifies the collapsing margins case for the next pair:
// - bottom margin of a last in-flow child and bottom margin of its parent if
//   the parent has 'auto' computed height
TEST_F(NGBlockLayoutAlgorithmTest, CollapsingMarginsCase3) {
  setBodyInnerHTML(R"HTML(
      <style>
       #container {
         margin-bottom: 20px;
       }
       #child {
         margin-bottom: 200px;
         height: 50px;
       }
      </style>
      <div id='container'>
        <div id='child'></div>
      </div>
    )HTML");

  const NGPhysicalBoxFragment* body_fragment;
  const NGPhysicalBoxFragment* container_fragment;
  const NGPhysicalBoxFragment* child_fragment;
  RefPtr<const NGPhysicalBoxFragment> fragment;
  auto run_test = [&](const Length& container_height) {
    Element* container = document().getElementById("container");
    container->mutableComputedStyle()->setHeight(container_height);
    std::tie(fragment, std::ignore) = RunBlockLayoutAlgorithmForElement(
        document().getElementsByTagName("html")->item(0));
    ASSERT_EQ(1UL, fragment->Children().size());
    body_fragment = toNGPhysicalBoxFragment(fragment->Children()[0].get());
    container_fragment =
        toNGPhysicalBoxFragment(body_fragment->Children()[0].get());
    ASSERT_EQ(1UL, container_fragment->Children().size());
    child_fragment =
        toNGPhysicalBoxFragment(container_fragment->Children()[0].get());
  };

  // height == auto
  run_test(Length(Auto));
  // Margins are collapsed with the result 200 = std::max(20, 200)
  // The fragment size 258 == body's margin 8 + child's height 50 + 200
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(800), LayoutUnit(258)), fragment->Size());
  EXPECT_EQ(NGMarginStrut({LayoutUnit(200)}),
            container_fragment->EndMarginStrut());

  // height == fixed
  run_test(Length(50, Fixed));
  // Margins are not collapsed, so fragment still has margins == 20.
  // The fragment size 78 == body's margin 8 + child's height 50 + 20
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(800), LayoutUnit(78)), fragment->Size());
  EXPECT_EQ(NGMarginStrut(), container_fragment->EndMarginStrut());
}

// Verifies that 2 adjoining margins are not collapsed if there is padding or
// border that separates them.
TEST_F(NGBlockLayoutAlgorithmTest, CollapsingMarginsCase4) {
  setBodyInnerHTML(R"HTML(
      <style>
        #container {
          margin: 30px 0px;
          width: 200px;
        }
        #child {
         margin: 200px 0px;
          height: 50px;
          background-color: blue;
        }
      </style>
      <div id='container'>
        <div id='child'></div>
      </div>
    )HTML");

  const NGPhysicalBoxFragment* body_fragment;
  const NGPhysicalBoxFragment* container_fragment;
  const NGPhysicalBoxFragment* child_fragment;
  RefPtr<const NGPhysicalBoxFragment> fragment;
  auto run_test = [&](const Length& container_padding_top) {
    Element* container = document().getElementById("container");
    container->mutableComputedStyle()->setPaddingTop(container_padding_top);
    std::tie(fragment, std::ignore) = RunBlockLayoutAlgorithmForElement(
        document().getElementsByTagName("html")->item(0));
    ASSERT_EQ(1UL, fragment->Children().size());
    body_fragment = toNGPhysicalBoxFragment(fragment->Children()[0].get());
    container_fragment =
        toNGPhysicalBoxFragment(body_fragment->Children()[0].get());
    ASSERT_EQ(1UL, container_fragment->Children().size());
    child_fragment =
        toNGPhysicalBoxFragment(container_fragment->Children()[0].get());
  };

  // with padding
  run_test(Length(20, Fixed));
  // 500 = child's height 50 + 2xmargin 400 + paddint-top 20 +
  // container's margin 30
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(800), LayoutUnit(500)), fragment->Size());
  // 30 = max(body's margin 8, container margin 30)
  EXPECT_EQ(LayoutUnit(30), body_fragment->TopOffset());
  // 220 = container's padding top 20 + child's margin
  EXPECT_EQ(LayoutUnit(220), child_fragment->TopOffset());

  // without padding
  run_test(Length(0, Fixed));
  // 450 = 2xmax(body's margin 8, container's margin 30, child's margin 200) +
  //       child's height 50
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(800), LayoutUnit(450)), fragment->Size());
  // 200 = (body's margin 8, container's margin 30, child's margin 200)
  EXPECT_EQ(LayoutUnit(200), body_fragment->TopOffset());
  // 0 = collapsed margins
  EXPECT_EQ(LayoutUnit(0), child_fragment->TopOffset());
}

// Verifies that margins of 2 adjoining blocks with different writing modes
// get collapsed.
TEST_F(NGBlockLayoutAlgorithmTest, CollapsingMarginsCase5) {
  setBodyInnerHTML(R"HTML(
      <style>
        #container {
          margin-top: 10px;
          writing-mode: vertical-lr;
        }
        #vertical {
          margin-right: 90px;
          background-color: red;
          height: 70px;
          width: 30px;
        }
        #horizontal {
         background-color: blue;
          margin-left: 100px;
          writing-mode: horizontal-tb;
          height: 60px;
          width: 30px;
        }
      </style>
      <div id='container'>
        <div id='vertical'></div>
        <div id='horizontal'></div>
      </div>
    )HTML");
  RefPtr<NGPhysicalBoxFragment> fragment;
  std::tie(fragment, std::ignore) = RunBlockLayoutAlgorithmForElement(
      document().getElementsByTagName("html")->item(0));

  // body
  auto* body_fragment = toNGPhysicalBoxFragment(fragment->Children()[0].get());
  // 10 = std::max(body's margin 8, container's margin top)
  int body_top_offset = 10;
  EXPECT_THAT(body_fragment->TopOffset(), LayoutUnit(body_top_offset));
  int body_left_offset = 8;
  EXPECT_THAT(body_fragment->LeftOffset(), LayoutUnit(body_left_offset));
  // height = 70. std::max(vertical height's 70, horizontal's height's 60)
  // TODO(glebl): Should be 70! Fix this once min/max algorithm handles
  // orthogonal children.
  int body_fragment_block_size = 130;
  ASSERT_EQ(
      NGPhysicalSize(LayoutUnit(784), LayoutUnit(body_fragment_block_size)),
      body_fragment->Size());
  ASSERT_EQ(1UL, body_fragment->Children().size());

  // container
  auto* container_fragment =
      toNGPhysicalBoxFragment(body_fragment->Children()[0].get());
  // Container's margins are collapsed with body's fragment.
  EXPECT_THAT(container_fragment->TopOffset(), LayoutUnit());
  EXPECT_THAT(container_fragment->LeftOffset(), LayoutUnit());
  ASSERT_EQ(2UL, container_fragment->Children().size());

  // vertical
  auto* vertical_fragment =
      toNGPhysicalBoxFragment(container_fragment->Children()[0].get());
  EXPECT_THAT(vertical_fragment->TopOffset(), LayoutUnit());
  EXPECT_THAT(vertical_fragment->LeftOffset(), LayoutUnit());

  // horizontal
  auto* horizontal_fragment =
      toNGPhysicalBoxFragment(container_fragment->Children()[1].get());
  EXPECT_THAT(horizontal_fragment->TopOffset(), LayoutUnit());
  // 130 = vertical's width 30 +
  //       std::max(vertical's margin right 90, horizontal's margin-left 100)
  EXPECT_THAT(horizontal_fragment->LeftOffset(), LayoutUnit(130));
}

// Verifies that margins collapsing logic works with Layout Inline.
TEST_F(NGBlockLayoutAlgorithmTest, CollapsingMarginsWithText) {
  setBodyInnerHTML(R"HTML(
      <!DOCTYPE html>
      <style>
        body {
          margin: 10px;
        }
        p {
          margin: 20px;
        }
      </style>
      <p>Some text</p>
    )HTML");
  RefPtr<NGPhysicalBoxFragment> html_fragment;
  std::tie(html_fragment, std::ignore) = RunBlockLayoutAlgorithmForElement(
      document().getElementsByTagName("html")->item(0));

  auto* body_fragment =
      toNGPhysicalBoxFragment(html_fragment->Children()[0].get());
  // 20 = std::max(body's margin, p's margin)
  EXPECT_THAT(body_fragment->Offset(),
              NGPhysicalOffset(LayoutUnit(10), LayoutUnit(20)));

  auto* p_fragment =
      toNGPhysicalBoxFragment(body_fragment->Children()[0].get());
  // Collapsed margins with result = 0.
  EXPECT_THAT(p_fragment->Offset(),
              NGPhysicalOffset(LayoutUnit(20), LayoutUnit(0)));
}

// Verifies that the margin strut of a child with a different writing mode does
// not get used in the collapsing margins calculation.
//
// TODO(glebl): Disabled for now. Follow-up with kojii@ on
// https://software.hixie.ch/utilities/js/live-dom-viewer/?saved=4844
TEST_F(NGBlockLayoutAlgorithmTest, DISABLED_CollapsingMarginsCase6) {
  setBodyInnerHTML(R"HTML(
    <style>
      #div1 {
        margin-bottom: 10px;
        width: 10px;
        height: 60px;
        writing-mode: vertical-rl;
      }
      #div2 { margin-left: -20px; width: 10px; }
      #div3 { margin-top: 40px; height: 60px; }
    </style>
    <div id="container" style="width:500px;height:500px">
      <div id="div1">
         <div id="div2">vertical</div>
      </div>
      <div id="div3"></div>
    </div>
  )HTML");
  const int kHeight = 60;
  const int kMarginBottom = 10;
  const int kMarginTop = 40;

  auto* container = new NGBlockNode(getLayoutObjectByElementId("container"));
  RefPtr<NGConstraintSpace> space =
      ConstructConstraintSpace(kHorizontalTopBottom, TextDirection::kLtr,
                               NGLogicalSize(LayoutUnit(500), LayoutUnit(500)));
  RefPtr<NGPhysicalBoxFragment> frag =
      RunBlockLayoutAlgorithm(space.get(), container);

  ASSERT_EQ(frag->Children().size(), 2UL);

  const NGPhysicalFragment* child1 = frag->Children()[0].get();
  EXPECT_EQ(0, child1->TopOffset());
  EXPECT_EQ(kHeight, child1->Height());

  const NGPhysicalFragment* child2 = frag->Children()[1].get();
  EXPECT_EQ(kHeight + std::max(kMarginBottom, kMarginTop), child2->TopOffset());
}

// Verifies that a box's size includes its borders and padding, and that
// children are positioned inside the content box.
TEST_F(NGBlockLayoutAlgorithmTest, BorderAndPadding) {
  setBodyInnerHTML(R"HTML(
    <style>
      #div1 {
        width: 100px;
        height: 100px;
        border-style: solid;
        border-width: 1px 2px 3px 4px;
        padding: 5px 6px 7px 8px;
      }
    </style>
    <div id="container">
      <div id="div1">
         <div id="div2"></div>
      </div>
    </div>
  )HTML");
  const int kWidth = 100;
  const int kHeight = 100;
  const int kBorderTop = 1;
  const int kBorderRight = 2;
  const int kBorderBottom = 3;
  const int kBorderLeft = 4;
  const int kPaddingTop = 5;
  const int kPaddingRight = 6;
  const int kPaddingBottom = 7;
  const int kPaddingLeft = 8;

  auto* container = new NGBlockNode(getLayoutObjectByElementId("container"));

  RefPtr<NGConstraintSpace> space = ConstructConstraintSpace(
      kHorizontalTopBottom, TextDirection::kLtr,
      NGLogicalSize(LayoutUnit(1000), NGSizeIndefinite));

  RefPtr<NGPhysicalBoxFragment> frag =
      RunBlockLayoutAlgorithm(space.get(), container);

  ASSERT_EQ(frag->Children().size(), 1UL);

  // div1
  const NGPhysicalFragment* child = frag->Children()[0].get();
  EXPECT_EQ(kBorderLeft + kPaddingLeft + kWidth + kPaddingRight + kBorderRight,
            child->Width());
  EXPECT_EQ(kBorderTop + kPaddingTop + kHeight + kPaddingBottom + kBorderBottom,
            child->Height());

  ASSERT_TRUE(child->Type() == NGPhysicalFragment::kFragmentBox);
  ASSERT_EQ(static_cast<const NGPhysicalBoxFragment*>(child)->Children().size(),
            1UL);

  // div2
  child = static_cast<const NGPhysicalBoxFragment*>(child)->Children()[0].get();
  EXPECT_EQ(kBorderTop + kPaddingTop, child->TopOffset());
  EXPECT_EQ(kBorderLeft + kPaddingLeft, child->LeftOffset());
}

TEST_F(NGBlockLayoutAlgorithmTest, PercentageResolutionSize) {
  setBodyInnerHTML(R"HTML(
    <div id="container" style="width: 30px; padding-left: 10px">
      <div id="div1" style="width: 40%"></div>
    </div>
  )HTML");
  const int kPaddingLeft = 10;
  const int kWidth = 30;

  auto* container = new NGBlockNode(getLayoutObjectByElementId("container"));

  RefPtr<NGConstraintSpace> space = ConstructConstraintSpace(
      kHorizontalTopBottom, TextDirection::kLtr,
      NGLogicalSize(LayoutUnit(100), NGSizeIndefinite));
  RefPtr<NGPhysicalBoxFragment> frag =
      RunBlockLayoutAlgorithm(space.get(), container);

  EXPECT_EQ(LayoutUnit(kWidth + kPaddingLeft), frag->Width());
  EXPECT_EQ(NGPhysicalFragment::kFragmentBox, frag->Type());
  ASSERT_EQ(frag->Children().size(), 1UL);

  const NGPhysicalFragment* child = frag->Children()[0].get();
  EXPECT_EQ(LayoutUnit(12), child->Width());
}

// A very simple auto margin case. We rely on the tests in ng_length_utils_test
// for the more complex cases; just make sure we handle auto at all here.
TEST_F(NGBlockLayoutAlgorithmTest, AutoMargin) {
  setBodyInnerHTML(R"HTML(
    <style>
      #first { width: 10px; margin-left: auto; margin-right: auto; }
    </style>
    <div id="container" style="width: 30px; padding-left: 10px">
      <div id="first">
      </div>
    </div>
  )HTML");
  const int kPaddingLeft = 10;
  const int kWidth = 30;
  const int kChildWidth = 10;

  auto* container = new NGBlockNode(getLayoutObjectByElementId("container"));

  RefPtr<NGConstraintSpace> space = ConstructConstraintSpace(
      kHorizontalTopBottom, TextDirection::kLtr,
      NGLogicalSize(LayoutUnit(100), NGSizeIndefinite));
  RefPtr<NGPhysicalBoxFragment> frag =
      RunBlockLayoutAlgorithm(space.get(), container);

  EXPECT_EQ(LayoutUnit(kWidth + kPaddingLeft), frag->Width());
  EXPECT_EQ(NGPhysicalFragment::kFragmentBox, frag->Type());
  EXPECT_EQ(LayoutUnit(kWidth + kPaddingLeft), frag->WidthOverflow());
  ASSERT_EQ(1UL, frag->Children().size());

  const NGPhysicalFragment* child = frag->Children()[0].get();
  EXPECT_EQ(LayoutUnit(kChildWidth), child->Width());
  EXPECT_EQ(LayoutUnit(kPaddingLeft + 10), child->LeftOffset());
  EXPECT_EQ(LayoutUnit(0), child->TopOffset());
}

// Verifies that floats can be correctly positioned if they are inside of nested
// empty blocks.
TEST_F(NGBlockLayoutAlgorithmTest, PositionFloatInsideEmptyBlocks) {
  setBodyInnerHTML(R"HTML(
      <style>
        #container {
          height: 300px;
          width: 300px;
          outline: blue solid;
        }
        #empty1 {
          margin: 20px;
          padding: 0 20px;
        }
        #empty2 {
          margin: 15px;
          padding: 0 15px;
        }
        #left-float {
          float: left;
          height: 5px;
          width: 5px;
          padding: 10px;
          margin: 10px;
          background-color: green;
        }
        #right-float {
          float: right;
          height: 15px;
          width: 15px;
          margin: 15px 10px;
          background-color: red;
        }
      </style>
      <div id='container'>
        <div id='empty1'>
          <div id='empty2'>
            <div id='left-float'></div>
            <div id='right-float'></div>
          </div>
        </div>
      </div>
    )HTML");

  // ** Run LayoutNG algorithm **
  RefPtr<NGConstraintSpace> space;
  RefPtr<NGPhysicalBoxFragment> fragment;
  std::tie(fragment, space) = RunBlockLayoutAlgorithmForElement(
      document().getElementsByTagName("html")->item(0));

  auto* body_fragment = toNGPhysicalBoxFragment(fragment->Children()[0].get());
  // 20 = std::max(empty1's margin, empty2's margin, body's margin)
  int body_top_offset = 20;
  EXPECT_THAT(LayoutUnit(body_top_offset), body_fragment->TopOffset());
  ASSERT_EQ(1UL, body_fragment->Children().size());
  auto* container_fragment =
      toNGPhysicalBoxFragment(body_fragment->Children()[0].get());
  ASSERT_EQ(1UL, container_fragment->Children().size());

  auto* empty1_fragment =
      toNGPhysicalBoxFragment(container_fragment->Children()[0].get());
  // 0, vertical margins got collapsed
  EXPECT_THAT(LayoutUnit(), empty1_fragment->TopOffset());
  // 20 empty1's margin
  int empty1_inline_offset = 20;
  EXPECT_THAT(LayoutUnit(empty1_inline_offset), empty1_fragment->LeftOffset());
  ASSERT_EQ(empty1_fragment->Children().size(), 1UL);

  auto* empty2_fragment =
      toNGPhysicalBoxFragment(empty1_fragment->Children()[0].get());
  // 0, vertical margins got collapsed
  EXPECT_THAT(LayoutUnit(), empty2_fragment->TopOffset());
  // 35 = empty1's padding(20) + empty2's padding(15)
  int empty2_inline_offset = 35;
  EXPECT_THAT(LayoutUnit(empty2_inline_offset), empty2_fragment->LeftOffset());

  ASSERT_EQ(2UL, container_fragment->PositionedFloats().size());
  RefPtr<NGPhysicalFragment> left_float_fragment =
      container_fragment->PositionedFloats().at(0)->fragment;
  // inline 25 = empty2's padding(15) + left float's margin(10)
  // block 10 = left float's margin
  EXPECT_THAT(NGPhysicalOffset(LayoutUnit(25), LayoutUnit(10)),
              left_float_fragment->Offset());

  auto right_float_fragment =
      container_fragment->PositionedFloats().at(1)->fragment;
  LayoutUnit right_float_offset = LayoutUnit(125);
  // inline offset 150 = empty2's padding(15) + right float's margin(10) + right
  // float offset(125)
  // block offset 15 = right float's margin
  EXPECT_THAT(
      NGPhysicalOffset(LayoutUnit(25) + right_float_offset, LayoutUnit(15)),
      right_float_fragment->Offset());

  // ** Verify layout tree **
  Element* left_float = document().getElementById("left-float");
  // 88 = body's margin(8) +
  // empty1's padding and margin + empty2's padding and margins + float's
  // padding
  EXPECT_THAT(left_float->offsetLeft(), 88);
  // 30 = body_top_offset(collapsed margins result) + float's padding
  EXPECT_THAT(body_top_offset + 10, left_float->offsetTop());

  // ** Legacy Floating objects **
  // #container is the 1st non-empty block so floats are attached to it.
  Element* container = document().getElementById("container");
  auto& floating_objects =
      const_cast<FloatingObjects*>(
          toLayoutBlockFlow(container->layoutObject())->floatingObjects())
          ->mutableSet();
  ASSERT_EQ(2UL, floating_objects.size());
  auto left_floating_object = floating_objects.takeFirst();
  ASSERT_TRUE(left_floating_object->isPlaced());
  // 80 = float_inline_offset(25) + accumulative offset of empty blocks(35 + 20)
  EXPECT_THAT(LayoutUnit(80), left_floating_object->x());
  // 10 = left float's margin
  EXPECT_THAT(LayoutUnit(10), left_floating_object->y());

  auto right_floating_object = floating_objects.takeFirst();
  ASSERT_TRUE(right_floating_object->isPlaced());
  // 205 = float_inline_offset(25) +
  //       accumulative offset of empty blocks(35 + 20)
  //       + right float offset(125)
  EXPECT_THAT(LayoutUnit(80) + right_float_offset, right_floating_object->x());
  // 15 = right float's margin
  EXPECT_THAT(LayoutUnit(15), right_floating_object->y());
}

// Verifies that left/right floating and regular blocks can be positioned
// correctly by the algorithm.
TEST_F(NGBlockLayoutAlgorithmTest, PositionFloatFragments) {
  setBodyInnerHTML(R"HTML(
      <style>
        #container {
          height: 200px;
          width: 200px;
        }
        #left-float {
          background-color: red;
          float: left;
          height: 30px;
          width: 30px;
        }
        #left-wide-float {
          background-color: greenyellow;
          float: left;
          height: 30px;
          width: 180px;
        }
        #regular {
          width: 40px;
          height: 40px;
          background-color: green;
        }
        #right-float {
          background-color: cyan;
          float: right;
          width: 50px;
          height: 50px;
        }
        #left-float-with-margin {
          background-color: black;
          float: left;
          height: 120px;
          margin: 10px;
          width: 120px;
        }
      </style>
      <div id='container'>
        <div id='left-float'></div>
        <div id='left-wide-float'></div>
        <div id='regular'></div>
        <div id='right-float'></div>
        <div id='left-float-with-margin'></div>
      </div>
      )HTML");

  // ** Run LayoutNG algorithm **
  RefPtr<NGConstraintSpace> space;
  RefPtr<NGPhysicalBoxFragment> fragment;
  std::tie(fragment, space) = RunBlockLayoutAlgorithmForElement(
      document().getElementsByTagName("html")->item(0));

  // ** Verify LayoutNG fragments and the list of positioned floats **
  ASSERT_EQ(1UL, fragment->Children().size());
  auto* body_fragment = toNGPhysicalBoxFragment(fragment->Children()[0].get());
  EXPECT_THAT(LayoutUnit(8), body_fragment->TopOffset());
  auto* container_fragment =
      toNGPhysicalBoxFragment(body_fragment->Children()[0].get());
  ASSERT_EQ(1UL, container_fragment->Children().size());
  ASSERT_EQ(4UL, container_fragment->PositionedFloats().size());

  // ** Verify layout tree **
  Element* left_float = document().getElementById("left-float");
  // 8 = body's margin-top
  int left_float_block_offset = 8;
  EXPECT_EQ(left_float_block_offset, left_float->offsetTop());
  auto left_float_fragment =
      container_fragment->PositionedFloats().at(0)->fragment;
  EXPECT_THAT(LayoutUnit(), left_float_fragment->TopOffset());

  Element* left_wide_float = document().getElementById("left-wide-float");
  // left-wide-float is positioned right below left-float as it's too wide.
  // 38 = left_float_block_offset +
  //      left-float's height 30
  int left_wide_float_block_offset = 38;
  EXPECT_EQ(left_wide_float_block_offset, left_wide_float->offsetTop());
  auto left_wide_float_fragment =
      container_fragment->PositionedFloats().at(1)->fragment;
  // 30 = left-float's height.
  EXPECT_THAT(LayoutUnit(30), left_wide_float_fragment->TopOffset());

  Element* regular = document().getElementById("regular");
  // regular_block_offset = body's margin-top 8
  int regular_block_offset = 8;
  EXPECT_EQ(regular_block_offset, regular->offsetTop());
  auto* regular_block_fragment =
      toNGPhysicalBoxFragment(container_fragment->Children()[0].get());
  EXPECT_THAT(LayoutUnit(), regular_block_fragment->TopOffset());

  Element* right_float = document().getElementById("right-float");
  // 158 = body's margin-left 8 + container's width 200 - right_float's width 50
  int right_float_inline_offset = 158;
  // it's positioned right after our left_wide_float
  // 68 = left_wide_float_block_offset 38 + left-wide-float's height 30
  int right_float_block_offset = left_wide_float_block_offset + 30;
  EXPECT_EQ(right_float_inline_offset, right_float->offsetLeft());
  EXPECT_EQ(right_float_block_offset, right_float->offsetTop());
  auto right_float_fragment =
      container_fragment->PositionedFloats().at(2)->fragment;
  // 60 = right_float_block_offset(68) - body's margin(8)
  EXPECT_THAT(LayoutUnit(right_float_block_offset - 8),
              right_float_fragment->TopOffset());
  // 150 = right_float_inline_offset(158) - body's margin(8)
  EXPECT_THAT(LayoutUnit(right_float_inline_offset - 8),
              right_float_fragment->LeftOffset());

  Element* left_float_with_margin =
      document().getElementById("left-float-with-margin");
  // 18 = body's margin(8) + left-float-with-margin's margin(10)
  int left_float_with_margin_inline_offset = 18;
  EXPECT_EQ(left_float_with_margin_inline_offset,
            left_float_with_margin->offsetLeft());
  // 78 = left_wide_float_block_offset 38 + left-wide-float's height 30 +
  //      left-float-with-margin's margin(10)
  int left_float_with_margin_block_offset = 78;
  EXPECT_EQ(left_float_with_margin_block_offset,
            left_float_with_margin->offsetTop());
  auto left_float_with_margin_fragment =
      container_fragment->PositionedFloats().at(3)->fragment;
  // 70 = left_float_with_margin_block_offset(78) - body's margin(8)
  EXPECT_THAT(LayoutUnit(left_float_with_margin_block_offset - 8),
              left_float_with_margin_fragment->TopOffset());
  // 10 = left_float_with_margin_inline_offset(18) - body's margin(8)
  EXPECT_THAT(LayoutUnit(left_float_with_margin_inline_offset - 8),
              left_float_with_margin_fragment->LeftOffset());

  // ** Verify exclusions **
  NGLogicalSize left_float_exclusion_size = {LayoutUnit(30), LayoutUnit(30)};
  // this should be equal to body's margin(8)
  NGLogicalOffset left_float_exclusion_offset = {LayoutUnit(8), LayoutUnit(8)};
  NGLogicalRect left_float_exclusion_rect = {left_float_exclusion_offset,
                                             left_float_exclusion_size};
  NGExclusion left_float_exclusion = {left_float_exclusion_rect,
                                      NGExclusion::kFloatLeft};

  NGLogicalSize left_wide_exclusion_size = {LayoutUnit(180), LayoutUnit(30)};
  NGLogicalOffset left_wide_exclusion_offset = {
      LayoutUnit(8), LayoutUnit(left_wide_float_block_offset)};
  NGLogicalRect left_wide_exclusion_rect = {left_wide_exclusion_offset,
                                            left_wide_exclusion_size};
  NGExclusion left_wide_exclusion = {left_wide_exclusion_rect,
                                     NGExclusion::kFloatLeft};

  NGLogicalSize right_float_exclusion_size = {LayoutUnit(50), LayoutUnit(50)};
  NGLogicalOffset right_float_exclusion_offset = {
      LayoutUnit(right_float_inline_offset),
      LayoutUnit(right_float_block_offset)};
  NGLogicalRect right_float_exclusion_rect = {right_float_exclusion_offset,
                                              right_float_exclusion_size};
  NGExclusion right_float_exclusion = {right_float_exclusion_rect,
                                       NGExclusion::kFloatRight};

  // left-float-with-margin's size(120) + margin(2x10)
  NGLogicalSize left_float_with_margin_exclusion_size = {LayoutUnit(140),
                                                         LayoutUnit(140)};
  // Exclusion starts from the right_float_block_offset position.
  NGLogicalOffset left_float_with_margin_exclusion_offset = {
      LayoutUnit(8), LayoutUnit(right_float_block_offset)};
  NGLogicalRect left_float_with_margin_exclusion_rect = {
      left_float_with_margin_exclusion_offset,
      left_float_with_margin_exclusion_size};
  NGExclusion left_float_with_margin_exclusion = {
      left_float_with_margin_exclusion_rect, NGExclusion::kFloatLeft};

  EXPECT_THAT(
      space->Exclusions()->storage,
      (ElementsAre(Pointee(left_float_exclusion), Pointee(left_wide_exclusion),
                   Pointee(right_float_exclusion),
                   Pointee(left_float_with_margin_exclusion))));
}

// Verifies that NG block layout algorithm respects "clear" CSS property.
TEST_F(NGBlockLayoutAlgorithmTest, PositionFragmentsWithClear) {
  setBodyInnerHTML(R"HTML(
      <style>
        #container {
          height: 200px;
          width: 200px;
        }
        #float-left {
          background-color: red;
          float: left;
          height: 30px;
          width: 30px;
        }
        #float-right {
          background-color: blue;
          float: right;
          height: 170px;
          width: 40px;
        }
        #clearance {
          background-color: yellow;
          height: 60px;
          width: 60px;
          margin: 20px;
        }
        #block {
          margin: 40px;
          background-color: black;
          height: 60px;
          width: 60px;
        }
        #adjoining-clearance {
          background-color: green;
          clear: left;
          height: 20px;
          width: 20px;
          margin: 30px;
        }
      </style>
      <div id='container'>
        <div id='float-left'></div>
        <div id='float-right'></div>
        <div id='clearance'></div>
        <div id='block'></div>
        <div id='adjoining-clearance'></div>
      </div>
    )HTML");

  const NGPhysicalBoxFragment* clerance_fragment;
  const NGPhysicalBoxFragment* body_fragment;
  const NGPhysicalBoxFragment* container_fragment;
  const NGPhysicalBoxFragment* block_fragment;
  const NGPhysicalBoxFragment* adjoining_clearance_fragment;
  auto run_with_clearance = [&](EClear clear_value) {
    RefPtr<NGPhysicalBoxFragment> fragment;
    Element* el_with_clear = document().getElementById("clearance");
    el_with_clear->mutableComputedStyle()->setClear(clear_value);
    std::tie(fragment, std::ignore) = RunBlockLayoutAlgorithmForElement(
        document().getElementsByTagName("html")->item(0));
    ASSERT_EQ(1UL, fragment->Children().size());
    body_fragment = toNGPhysicalBoxFragment(fragment->Children()[0].get());
    container_fragment =
        toNGPhysicalBoxFragment(body_fragment->Children()[0].get());
    ASSERT_EQ(3UL, container_fragment->Children().size());
    clerance_fragment =
        toNGPhysicalBoxFragment(container_fragment->Children()[0].get());
    block_fragment =
        toNGPhysicalBoxFragment(container_fragment->Children()[1].get());
    adjoining_clearance_fragment =
        toNGPhysicalBoxFragment(container_fragment->Children()[2].get());
  };

  // clear: none
  run_with_clearance(EClear::kNone);
  // 20 = std::max(body's margin 8, clearance's margins 20)
  EXPECT_EQ(LayoutUnit(20), body_fragment->TopOffset());
  EXPECT_EQ(LayoutUnit(0), container_fragment->TopOffset());
  // 0 = collapsed margins
  EXPECT_EQ(LayoutUnit(0), clerance_fragment->TopOffset());
  // 100 = clearance's height 60 +
  //       std::max(clearance's margins 20, block's margins 40)
  EXPECT_EQ(LayoutUnit(100), block_fragment->TopOffset());
  // 200 = 100 + block's height 60 + max(adjoining_clearance's margins 30,
  //                                     block's margins 40)
  EXPECT_EQ(LayoutUnit(200), adjoining_clearance_fragment->TopOffset());

  // clear: right
  run_with_clearance(EClear::kRight);
  // 8 = body's margin. This doesn't collapse its margins with 'clearance' block
  // as it's not an adjoining block to body.
  EXPECT_EQ(LayoutUnit(8), body_fragment->TopOffset());
  EXPECT_EQ(LayoutUnit(0), container_fragment->TopOffset());
  // 170 = float-right's height
  EXPECT_EQ(LayoutUnit(170), clerance_fragment->TopOffset());
  // 270 = float-right's height + clearance's height 60 +
  //       max(clearance's margin 20, block margin 40)
  EXPECT_EQ(LayoutUnit(270), block_fragment->TopOffset());
  // 370 = block's offset 270 + block's height 60 +
  //       std::max(block's margin 40, adjoining_clearance's margin 30)
  EXPECT_EQ(LayoutUnit(370), adjoining_clearance_fragment->TopOffset());

  // clear: left
  run_with_clearance(EClear::kLeft);
  // 8 = body's margin. This doesn't collapse its margins with 'clearance' block
  // as it's not an adjoining block to body.
  EXPECT_EQ(LayoutUnit(8), body_fragment->TopOffset());
  EXPECT_EQ(LayoutUnit(0), container_fragment->TopOffset());
  // 30 = float_left's height
  EXPECT_EQ(LayoutUnit(30), clerance_fragment->TopOffset());
  // 130 = float_left's height + clearance's height 60 +
  //       max(clearance's margin 20, block margin 40)
  EXPECT_EQ(LayoutUnit(130), block_fragment->TopOffset());
  // 230 = block's offset 130 + block's height 60 +
  //       std::max(block's margin 40, adjoining_clearance's margin 30)
  EXPECT_EQ(LayoutUnit(230), adjoining_clearance_fragment->TopOffset());

  // clear: both
  // same as clear: right
  run_with_clearance(EClear::kBoth);
  EXPECT_EQ(LayoutUnit(8), body_fragment->TopOffset());
  EXPECT_EQ(LayoutUnit(0), container_fragment->TopOffset());
  EXPECT_EQ(LayoutUnit(170), clerance_fragment->TopOffset());
  EXPECT_EQ(LayoutUnit(270), block_fragment->TopOffset());
  EXPECT_EQ(LayoutUnit(370), adjoining_clearance_fragment->TopOffset());
}

// Verifies that we compute the right min and max-content size.
TEST_F(NGBlockLayoutAlgorithmTest, ComputeMinMaxContent) {
  setBodyInnerHTML(R"HTML(
    <div id="container" style="width: 50px">
      <div id="first-child" style="width: 20px"></div>
      <div id="second-child" style="width: 30px"></div>
    </div>
  )HTML");

  const int kSecondChildWidth = 30;

  auto* container = new NGBlockNode(getLayoutObjectByElementId("container"));

  MinAndMaxContentSizes sizes = RunComputeMinAndMax(container);
  EXPECT_EQ(kSecondChildWidth, sizes.min_content);
  EXPECT_EQ(kSecondChildWidth, sizes.max_content);
}

// Tests that we correctly handle shrink-to-fit
TEST_F(NGBlockLayoutAlgorithmTest, ShrinkToFit) {
  setBodyInnerHTML(R"HTML(
    <div id="container">
      <div id="first-child" style="width: 20px"></div>
      <div id="second-child" style="width: 30px"></div>
    </div>
  )HTML");
  const int kWidthChild2 = 30;

  auto* container = new NGBlockNode(getLayoutObjectByElementId("container"));

  RefPtr<NGConstraintSpace> space = ConstructConstraintSpace(
      kHorizontalTopBottom, TextDirection::kLtr,
      NGLogicalSize(LayoutUnit(100), NGSizeIndefinite), true);
  RefPtr<NGPhysicalFragment> frag =
      RunBlockLayoutAlgorithm(space.get(), container);

  EXPECT_EQ(LayoutUnit(kWidthChild2), frag->Width());
}

class FragmentChildIterator {
  STACK_ALLOCATED();

 public:
  explicit FragmentChildIterator(const NGPhysicalBoxFragment* parent) {
    SetParent(parent);
  }
  void SetParent(const NGPhysicalBoxFragment* parent) {
    parent_ = parent;
    index_ = 0;
  }

  const NGPhysicalBoxFragment* NextChild() {
    if (!parent_)
      return nullptr;
    if (index_ >= parent_->Children().size())
      return nullptr;
    while (parent_->Children()[index_]->Type() !=
           NGPhysicalFragment::kFragmentBox) {
      ++index_;
      if (index_ >= parent_->Children().size())
        return nullptr;
    }
    return toNGPhysicalBoxFragment(parent_->Children()[index_++].get());
  }

 private:
  const NGPhysicalBoxFragment* parent_;
  unsigned index_;
};

// TODO(glebl): reenable multicol after new margin collapsing/floats algorithm
// is checked in.
TEST_F(NGBlockLayoutAlgorithmTest, DISABLED_EmptyMulticol) {
  setBodyInnerHTML(R"HTML(
    <style>
      #parent {
        column-count: 2;
        column-fill: auto;
        column-gap: 10px;
        height: 100px;
        width: 210px;
      }
    </style>
    <div id="container">
      <div id="parent">
      </div>
    </div>
  )HTML");

  auto* container = new NGBlockNode(getLayoutObjectByElementId("container"));
  RefPtr<NGConstraintSpace> space = ConstructConstraintSpace(
      kHorizontalTopBottom, TextDirection::kLtr,
      NGLogicalSize(LayoutUnit(1000), NGSizeIndefinite));
  RefPtr<const NGPhysicalBoxFragment> parent_fragment =
      RunBlockLayoutAlgorithm(space.get(), container);
  FragmentChildIterator iterator(parent_fragment.get());
  const auto* fragment = iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(LayoutUnit(210), fragment->Width());
  EXPECT_EQ(LayoutUnit(100), fragment->Height());
  EXPECT_FALSE(iterator.NextChild());

  // There should be nothing inside the multicol container.
  EXPECT_FALSE(FragmentChildIterator(fragment).NextChild());
}

// TODO(glebl): reenable multicol after new margin collapsing/floats algorithm
// is checked in.
TEST_F(NGBlockLayoutAlgorithmTest, DISABLED_EmptyBlock) {
  setBodyInnerHTML(R"HTML(
    <style>
      #parent {
        height: 100px;
        width: 210px;
      }
    </style>
    <div id="container">
      <div id="parent">
        <div id="child">
        </div>
      </div>
    </div>
  )HTML");

  auto* container = new NGBlockNode(getLayoutObjectByElementId("container"));
  RefPtr<NGConstraintSpace> space = ConstructConstraintSpace(
      kHorizontalTopBottom, TextDirection::kLtr,
      NGLogicalSize(LayoutUnit(1000), NGSizeIndefinite));
  RefPtr<const NGPhysicalBoxFragment> parent_fragment =
      RunBlockLayoutAlgorithm(space.get(), container);
  FragmentChildIterator iterator(parent_fragment.get());
  const auto* fragment = iterator.NextChild();
  EXPECT_EQ(LayoutUnit(210), fragment->Width());
  EXPECT_EQ(LayoutUnit(100), fragment->Height());
  ASSERT_TRUE(fragment);
  EXPECT_FALSE(iterator.NextChild());
  iterator.SetParent(fragment);

  // #child fragment in first column
  fragment = iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(LayoutUnit(), fragment->LeftOffset());
  EXPECT_EQ(LayoutUnit(), fragment->TopOffset());
  EXPECT_EQ(LayoutUnit(100), fragment->Width());
  EXPECT_EQ(LayoutUnit(), fragment->Height());
  EXPECT_EQ(0UL, fragment->Children().size());
  EXPECT_FALSE(iterator.NextChild());
}

// TODO(glebl): reenable multicol after new margin collapsing/floats algorithm
// is checked in.
TEST_F(NGBlockLayoutAlgorithmTest, DISABLED_BlockInOneColumn) {
  setBodyInnerHTML(R"HTML(
    <style>
      #parent {
        column-count: 2;
        column-fill: auto;
        column-gap: 10px;
        height: 100px;
        width: 310px;
      }
    </style>
    <div id="container">
      <div id="parent">
        <div id="child" style="width: 60%; height: 100%">
        </div>
      </div>
    </div>
  )HTML");

  auto* container = new NGBlockNode(getLayoutObjectByElementId("container"));
  RefPtr<NGConstraintSpace> space = ConstructConstraintSpace(
      kHorizontalTopBottom, TextDirection::kLtr,
      NGLogicalSize(LayoutUnit(1000), NGSizeIndefinite));
  RefPtr<const NGPhysicalBoxFragment> parent_fragment =
      RunBlockLayoutAlgorithm(space.get(), container);

  FragmentChildIterator iterator(parent_fragment.get());
  const auto* fragment = iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(LayoutUnit(310), fragment->Width());
  EXPECT_EQ(LayoutUnit(100), fragment->Height());
  EXPECT_FALSE(iterator.NextChild());
  iterator.SetParent(fragment);

  // #child fragment in first column
  fragment = iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(LayoutUnit(), fragment->LeftOffset());
  EXPECT_EQ(LayoutUnit(), fragment->TopOffset());
  EXPECT_EQ(LayoutUnit(90), fragment->Width());
  EXPECT_EQ(LayoutUnit(100), fragment->Height());
  EXPECT_EQ(0UL, fragment->Children().size());
  EXPECT_FALSE(iterator.NextChild());
}

// TODO(glebl): reenable multicol after new margin collapsing/floats algorithm
// is checked in.
TEST_F(NGBlockLayoutAlgorithmTest, DISABLED_BlockInTwoColumns) {
  setBodyInnerHTML(R"HTML(
    <style>
      #parent {
        column-count: 2;
        column-fill: auto;
        column-gap: 10px;
        height: 100px;
        width: 20px;
      }
    </style>
    <div id="container">
      <div id="parent">
        <div id="child" style="width: 75%; height: 150px"></div>
      </div>
    </div>
  )HTML");

  auto* container = new NGBlockNode(getLayoutObjectByElementId("container"));
  RefPtr<NGConstraintSpace> space = ConstructConstraintSpace(
      kHorizontalTopBottom, TextDirection::kLtr,
      NGLogicalSize(LayoutUnit(1000), NGSizeIndefinite));
  RefPtr<const NGPhysicalBoxFragment> parent_fragment =
      RunBlockLayoutAlgorithm(space.get(), container);

  FragmentChildIterator iterator(parent_fragment.get());
  const auto* fragment = iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(LayoutUnit(210), fragment->Width());
  EXPECT_EQ(LayoutUnit(100), fragment->Height());
  EXPECT_FALSE(iterator.NextChild());

  iterator.SetParent(fragment);
  // #child fragment in first column
  fragment = iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(LayoutUnit(), fragment->LeftOffset());
  EXPECT_EQ(LayoutUnit(), fragment->TopOffset());
  EXPECT_EQ(LayoutUnit(75), fragment->Width());
  EXPECT_EQ(LayoutUnit(100), fragment->Height());
  EXPECT_EQ(0UL, fragment->Children().size());

  // #child fragment in second column
  fragment = iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(LayoutUnit(110), fragment->LeftOffset());
  EXPECT_EQ(LayoutUnit(), fragment->TopOffset());
  EXPECT_EQ(LayoutUnit(75), fragment->Width());
  EXPECT_EQ(LayoutUnit(50), fragment->Height());
  EXPECT_EQ(0U, fragment->Children().size());
  EXPECT_FALSE(iterator.NextChild());
}

// TODO(glebl): reenable multicol after new margin collapsing/floats algorithm
// is checked in.
TEST_F(NGBlockLayoutAlgorithmTest, DISABLED_BlockInThreeColumns) {
  setBodyInnerHTML(R"HTML(
    <style>
      #parent {
        column-count: 3;
        column-fill: auto;
        column-gap: 10px;
        height: 100px;
        width: 320px;
      }
    </style>
    <div id="container">
      <div id="parent">
        <div id="child" style="width: 75%; height: 250px;">
        </div>
      </div>
    </div>
  )HTML");

  auto* container = new NGBlockNode(getLayoutObjectByElementId("container"));
  RefPtr<NGConstraintSpace> space = ConstructConstraintSpace(
      kHorizontalTopBottom, TextDirection::kLtr,
      NGLogicalSize(LayoutUnit(1000), NGSizeIndefinite));
  RefPtr<const NGPhysicalBoxFragment> parent_fragment =
      RunBlockLayoutAlgorithm(space.get(), container);

  FragmentChildIterator iterator(parent_fragment.get());
  const auto* fragment = iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(LayoutUnit(320), fragment->Width());
  EXPECT_EQ(LayoutUnit(100), fragment->Height());
  EXPECT_FALSE(iterator.NextChild());

  iterator.SetParent(fragment);
  // #child fragment in first column
  fragment = iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(LayoutUnit(), fragment->LeftOffset());
  EXPECT_EQ(LayoutUnit(), fragment->TopOffset());
  EXPECT_EQ(LayoutUnit(75), fragment->Width());
  EXPECT_EQ(LayoutUnit(100), fragment->Height());
  EXPECT_EQ(0UL, fragment->Children().size());

  // #child fragment in second column
  fragment = iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(LayoutUnit(110), fragment->LeftOffset());
  EXPECT_EQ(LayoutUnit(), fragment->TopOffset());
  EXPECT_EQ(LayoutUnit(75), fragment->Width());
  EXPECT_EQ(LayoutUnit(100), fragment->Height());
  EXPECT_EQ(0U, fragment->Children().size());

  // #child fragment in third column
  fragment = iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(LayoutUnit(220), fragment->LeftOffset());
  EXPECT_EQ(LayoutUnit(), fragment->TopOffset());
  EXPECT_EQ(LayoutUnit(75), fragment->Width());
  EXPECT_EQ(LayoutUnit(50), fragment->Height());
  EXPECT_EQ(0U, fragment->Children().size());
  EXPECT_FALSE(iterator.NextChild());
}

// TODO(glebl): reenable multicol after new margin collapsing/floats algorithm
// is checked in.
TEST_F(NGBlockLayoutAlgorithmTest,
       DISABLED_ActualColumnCountGreaterThanSpecified) {
  setBodyInnerHTML(R"HTML(
    <style>
      #parent {
        column-count: 2;
        column-fill: auto;
        column-gap: 10px;
        height: 100px;
        width: 210px;
      }
    </style>
    <div id="container">
      <div id="parent">
        <div id="child" style="width: 1px; height: 250px;">
        </div>
      </div>
    </div>
  )HTML");

  auto* container = new NGBlockNode(getLayoutObjectByElementId("container"));
  RefPtr<NGConstraintSpace> space = ConstructConstraintSpace(
      kHorizontalTopBottom, TextDirection::kLtr,
      NGLogicalSize(LayoutUnit(1000), NGSizeIndefinite));
  RefPtr<const NGPhysicalBoxFragment> parent_fragment =
      RunBlockLayoutAlgorithm(space.get(), container);

  FragmentChildIterator iterator(parent_fragment.get());
  const auto* fragment = iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(LayoutUnit(210), fragment->Width());
  EXPECT_EQ(LayoutUnit(100), fragment->Height());
  EXPECT_FALSE(iterator.NextChild());

  iterator.SetParent(fragment);
  // #child fragment in first column
  fragment = iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(LayoutUnit(), fragment->LeftOffset());
  EXPECT_EQ(LayoutUnit(), fragment->TopOffset());
  EXPECT_EQ(LayoutUnit(1), fragment->Width());
  EXPECT_EQ(LayoutUnit(100), fragment->Height());
  EXPECT_EQ(0UL, fragment->Children().size());

  // #child fragment in second column
  fragment = iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(LayoutUnit(110), fragment->LeftOffset());
  EXPECT_EQ(LayoutUnit(), fragment->TopOffset());
  EXPECT_EQ(LayoutUnit(1), fragment->Width());
  EXPECT_EQ(LayoutUnit(100), fragment->Height());
  EXPECT_EQ(0U, fragment->Children().size());

  // #child fragment in third column
  fragment = iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(LayoutUnit(220), fragment->LeftOffset());
  EXPECT_EQ(LayoutUnit(), fragment->TopOffset());
  EXPECT_EQ(LayoutUnit(1), fragment->Width());
  EXPECT_EQ(LayoutUnit(50), fragment->Height());
  EXPECT_EQ(0U, fragment->Children().size());
  EXPECT_FALSE(iterator.NextChild());
}

// TODO(glebl): reenable multicol after new margin collapsing/floats algorithm
// is checked in.
TEST_F(NGBlockLayoutAlgorithmTest, DISABLED_TwoBlocksInTwoColumns) {
  setBodyInnerHTML(R"HTML(
    <style>
      #parent {
        column-count: 3;
        column-fill: auto;
        column-gap: 10px;
        height: 100px;
        width: 320px;
      }
    </style>
    <div id="container">
      <div id="parent">
        <div id="child1" style="width: 75%; height: 60px;">
        </div>
        <div id="child2" style="width: 85%; height: 60px;">
        </div>
      </div>
    </div>
  )HTML");

  auto* container = new NGBlockNode(getLayoutObjectByElementId("container"));
  RefPtr<NGConstraintSpace> space = ConstructConstraintSpace(
      kHorizontalTopBottom, TextDirection::kLtr,
      NGLogicalSize(LayoutUnit(1000), NGSizeIndefinite));
  RefPtr<const NGPhysicalBoxFragment> parent_fragment =
      RunBlockLayoutAlgorithm(space.get(), container);

  FragmentChildIterator iterator(parent_fragment.get());
  const auto* fragment = iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(LayoutUnit(320), fragment->Width());
  EXPECT_EQ(LayoutUnit(100), fragment->Height());
  EXPECT_FALSE(iterator.NextChild());

  iterator.SetParent(fragment);
  // #child1 fragment in first column
  fragment = iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(LayoutUnit(), fragment->LeftOffset());
  EXPECT_EQ(LayoutUnit(), fragment->TopOffset());
  EXPECT_EQ(LayoutUnit(75), fragment->Width());
  EXPECT_EQ(LayoutUnit(60), fragment->Height());
  EXPECT_EQ(0UL, fragment->Children().size());
  // #child2 fragment in first column
  fragment = iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(LayoutUnit(), fragment->LeftOffset());
  EXPECT_EQ(LayoutUnit(60), fragment->TopOffset());
  EXPECT_EQ(LayoutUnit(85), fragment->Width());
  EXPECT_EQ(LayoutUnit(40), fragment->Height());
  EXPECT_EQ(0UL, fragment->Children().size());

  // #child2 fragment in second column
  fragment = iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(LayoutUnit(110), fragment->LeftOffset());
  EXPECT_EQ(LayoutUnit(), fragment->TopOffset());
  EXPECT_EQ(LayoutUnit(85), fragment->Width());
  EXPECT_EQ(LayoutUnit(20), fragment->Height());
  EXPECT_EQ(0U, fragment->Children().size());
  EXPECT_FALSE(iterator.NextChild());
}

// TODO(glebl): reenable multicol after new margin collapsing/floats algorithm
// is checked in.
TEST_F(NGBlockLayoutAlgorithmTest, DISABLED_OverflowedBlock) {
  setBodyInnerHTML(R"HTML(
    <style>
      #parent {
        column-count: 2;
        column-fill: auto;
        column-gap: 10px;
        height: 100px;
        width: 320px;
      }
    </style>
    <div id="container">
      <div id="parent">
        <div id="child1" style="width: 75%; height: 60px;">
          <div id="grandchild1" style="width: 50px; height: 120px;">
          </div>
          <div id="grandchild2" style="width: 40px; height: 20px;">
          </div>
        </div>
        <div id="child2" style="width: 85%; height: 10px;"></div>
        </div>
      </div>
    </div>
  )HTML");

  auto* container = new NGBlockNode(getLayoutObjectByElementId("container"));
  RefPtr<NGConstraintSpace> space = ConstructConstraintSpace(
      kHorizontalTopBottom, TextDirection::kLtr,
      NGLogicalSize(LayoutUnit(1000), NGSizeIndefinite));
  RefPtr<const NGPhysicalBoxFragment> parent_fragment =
      RunBlockLayoutAlgorithm(space.get(), container);

  FragmentChildIterator iterator(parent_fragment.get());
  const auto* fragment = iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(LayoutUnit(320), fragment->Width());
  EXPECT_EQ(LayoutUnit(100), fragment->Height());
  EXPECT_FALSE(iterator.NextChild());

  iterator.SetParent(fragment);
  // #child1 fragment in first column
  fragment = iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(LayoutUnit(), fragment->LeftOffset());
  EXPECT_EQ(LayoutUnit(), fragment->TopOffset());
  EXPECT_EQ(LayoutUnit(75), fragment->Width());
  EXPECT_EQ(LayoutUnit(60), fragment->Height());
  FragmentChildIterator grandchild_iterator(fragment);
  // #grandchild1 fragment in first column
  fragment = grandchild_iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(LayoutUnit(), fragment->LeftOffset());
  EXPECT_EQ(LayoutUnit(), fragment->TopOffset());
  EXPECT_EQ(LayoutUnit(50), fragment->Width());
  EXPECT_EQ(LayoutUnit(100), fragment->Height());
  EXPECT_FALSE(grandchild_iterator.NextChild());
  // #child2 fragment in first column
  fragment = iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(LayoutUnit(), fragment->LeftOffset());
  EXPECT_EQ(LayoutUnit(60), fragment->TopOffset());
  EXPECT_EQ(LayoutUnit(85), fragment->Width());
  EXPECT_EQ(LayoutUnit(10), fragment->Height());
  EXPECT_EQ(0UL, fragment->Children().size());

  // #child1 fragment in second column
  fragment = iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(LayoutUnit(110), fragment->LeftOffset());
  EXPECT_EQ(LayoutUnit(), fragment->TopOffset());
  EXPECT_EQ(LayoutUnit(75), fragment->Width());
  EXPECT_EQ(LayoutUnit(), fragment->Height());
  grandchild_iterator.SetParent(fragment);
  // #grandchild1 fragment in second column
  fragment = grandchild_iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(LayoutUnit(), fragment->LeftOffset());
  EXPECT_EQ(LayoutUnit(), fragment->TopOffset());
  EXPECT_EQ(LayoutUnit(50), fragment->Width());
  EXPECT_EQ(LayoutUnit(20), fragment->Height());
  // #grandchild2 fragment in second column
  fragment = grandchild_iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(LayoutUnit(), fragment->LeftOffset());
  EXPECT_EQ(LayoutUnit(20), fragment->TopOffset());
  EXPECT_EQ(LayoutUnit(40), fragment->Width());
  EXPECT_EQ(LayoutUnit(20), fragment->Height());
  EXPECT_FALSE(grandchild_iterator.NextChild());
  EXPECT_FALSE(iterator.NextChild());
}

// TODO(glebl): reenable multicol after new margin collapsing/floats algorithm
// is checked in.
TEST_F(NGBlockLayoutAlgorithmTest, DISABLED_FloatInOneColumn) {
  setBodyInnerHTML(R"HTML(
    <style>
      #parent {
        column-count: 3;
        column-fill: auto;
        column-gap: 10px;
        height: 100px;
        width: 320px;
      }
    </style>
    <div id="container">
      <div id="parent">
        <div id="child" style="float: left; width: 75%; height: 100px;">
        </div>
      </div>
    </div>
  )HTML");

  auto* container = new NGBlockNode(getLayoutObjectByElementId("container"));
  RefPtr<NGConstraintSpace> space = ConstructConstraintSpace(
      kHorizontalTopBottom, TextDirection::kLtr,
      NGLogicalSize(LayoutUnit(1000), NGSizeIndefinite));
  RefPtr<const NGPhysicalBoxFragment> parent_fragment =
      RunBlockLayoutAlgorithm(space.get(), container);

  FragmentChildIterator iterator(parent_fragment.get());
  const auto* fragment = iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(LayoutUnit(320), fragment->Width());
  EXPECT_EQ(LayoutUnit(100), fragment->Height());
  EXPECT_FALSE(iterator.NextChild());

  iterator.SetParent(fragment);
  // #child fragment in first column
  fragment = iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(LayoutUnit(), fragment->LeftOffset());
  EXPECT_EQ(LayoutUnit(), fragment->TopOffset());
  EXPECT_EQ(LayoutUnit(75), fragment->Width());
  EXPECT_EQ(LayoutUnit(100), fragment->Height());
  EXPECT_EQ(0UL, fragment->Children().size());
  EXPECT_FALSE(iterator.NextChild());
}

// TODO(glebl): reenable multicol after new margin collapsing/floats algorithm
// is checked in.
TEST_F(NGBlockLayoutAlgorithmTest, DISABLED_TwoFloatsInOneColumn) {
  setBodyInnerHTML(R"HTML(
    <style>
      #parent {
        column-count: 3;
        column-fill: auto;
        column-gap: 10px;
        width: 320px;
        height: 100px;
      }
    </style>
    <div id="container">
      <div id="parent">
        <div id="child1" style="float: left; width: 15%; height: 100px;">
        </div>
        <div id="child2" style="float: right; width: 16%; height: 100px;">
        </div>
      </div>
    </div>
  )HTML");

  auto* container = new NGBlockNode(getLayoutObjectByElementId("container"));
  RefPtr<NGConstraintSpace> space = ConstructConstraintSpace(
      kHorizontalTopBottom, TextDirection::kLtr,
      NGLogicalSize(LayoutUnit(1000), NGSizeIndefinite));
  RefPtr<const NGPhysicalBoxFragment> parent_fragment =
      RunBlockLayoutAlgorithm(space.get(), container);

  FragmentChildIterator iterator(parent_fragment.get());
  const auto* fragment = iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(LayoutUnit(320), fragment->Width());
  EXPECT_EQ(LayoutUnit(100), fragment->Height());
  EXPECT_FALSE(iterator.NextChild());

  iterator.SetParent(fragment);
  // #child1 fragment in first column
  fragment = iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(LayoutUnit(), fragment->LeftOffset());
  EXPECT_EQ(LayoutUnit(), fragment->TopOffset());
  EXPECT_EQ(LayoutUnit(15), fragment->Width());
  EXPECT_EQ(LayoutUnit(100), fragment->Height());
  EXPECT_EQ(0UL, fragment->Children().size());
  // #child2 fragment in first column
  fragment = iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(LayoutUnit(84), fragment->LeftOffset());
  EXPECT_EQ(LayoutUnit(), fragment->TopOffset());
  EXPECT_EQ(LayoutUnit(16), fragment->Width());
  EXPECT_EQ(LayoutUnit(100), fragment->Height());
  EXPECT_EQ(0UL, fragment->Children().size());
  EXPECT_FALSE(iterator.NextChild());
}

// TODO(glebl): reenable multicol after new margin collapsing/floats algorithm
// is checked in.
TEST_F(NGBlockLayoutAlgorithmTest, DISABLED_TwoFloatsInTwoColumns) {
  setBodyInnerHTML(R"HTML(
    <style>
      #parent {
        column-count: 3;
        column-fill: auto;
        column-gap: 10px;
        width: 320px;
        height: 100px;
      }
    </style>
    <div id="container">
      <div id="parent">
        <div id="child1" style="float: left; width: 15%; height: 150px;">
        </div>
        <div id="child2" style="float: right; width: 16%; height: 150px;">
        </div>
      </div>
    </div>
  )HTML");

  auto* container = new NGBlockNode(getLayoutObjectByElementId("container"));
  RefPtr<NGConstraintSpace> space = ConstructConstraintSpace(
      kHorizontalTopBottom, TextDirection::kLtr,
      NGLogicalSize(LayoutUnit(1000), NGSizeIndefinite));
  RefPtr<const NGPhysicalBoxFragment> parent_fragment =
      RunBlockLayoutAlgorithm(space.get(), container);

  FragmentChildIterator iterator(parent_fragment.get());
  const auto* fragment = iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(LayoutUnit(320), fragment->Width());
  EXPECT_EQ(LayoutUnit(100), fragment->Height());
  EXPECT_FALSE(iterator.NextChild());

  iterator.SetParent(fragment);
  // #child1 fragment in first column
  fragment = iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(LayoutUnit(), fragment->LeftOffset());
  EXPECT_EQ(LayoutUnit(), fragment->TopOffset());
  EXPECT_EQ(LayoutUnit(15), fragment->Width());
  EXPECT_EQ(LayoutUnit(100), fragment->Height());
  EXPECT_EQ(0UL, fragment->Children().size());
  // #child2 fragment in first column
  fragment = iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(LayoutUnit(84), fragment->LeftOffset());
  EXPECT_EQ(LayoutUnit(), fragment->TopOffset());
  EXPECT_EQ(LayoutUnit(16), fragment->Width());
  EXPECT_EQ(LayoutUnit(100), fragment->Height());
  EXPECT_EQ(0UL, fragment->Children().size());

  // #child1 fragment in second column
  fragment = iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(LayoutUnit(110), fragment->LeftOffset());
  EXPECT_EQ(LayoutUnit(), fragment->TopOffset());
  EXPECT_EQ(LayoutUnit(15), fragment->Width());
  EXPECT_EQ(LayoutUnit(50), fragment->Height());
  EXPECT_EQ(0UL, fragment->Children().size());
  // #child2 fragment in second column
  fragment = iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(LayoutUnit(194), fragment->LeftOffset());
  EXPECT_EQ(LayoutUnit(), fragment->TopOffset());
  EXPECT_EQ(LayoutUnit(16), fragment->Width());
  EXPECT_EQ(LayoutUnit(50), fragment->Height());
  EXPECT_EQ(0UL, fragment->Children().size());
  EXPECT_FALSE(iterator.NextChild());
}

// Verifies that we position empty blocks and floats correctly inside of the
// block that establishes new BFC.
TEST_F(NGBlockLayoutAlgorithmTest, PositionEmptyBlocksInNewBfc) {
  setBodyInnerHTML(R"HTML(
    <style>
      #container {
        overflow: hidden;
      }
      #empty-block1 {
        margin: 8px;
      }
      #left-float {
        float: left;
        background: red;
        height: 20px;
        width: 10px;
        margin: 15px;
      }
      #empty-block2 {
        margin-top: 50px;
      }
    </style>
    <div id="container">
      <div id="left-float"></div>
      <div id="empty-block1"></div>
      <div id="empty-block2"></div>
    </div>
  )HTML");
  // #container is the new parent for our float because it's height != 0.
  Element* container = document().getElementById("container");
  auto& floating_objects =
      const_cast<FloatingObjects*>(
          toLayoutBlockFlow(container->layoutObject())->floatingObjects())
          ->mutableSet();
  ASSERT_EQ(1UL, floating_objects.size());
  auto floating_object = floating_objects.takeFirst();
  // left-float's margin = 15.
  EXPECT_THAT(LayoutUnit(15), floating_object->x());
  EXPECT_THAT(LayoutUnit(15), floating_object->y());

  RefPtr<const NGPhysicalBoxFragment> html_fragment;
  std::tie(html_fragment, std::ignore) = RunBlockLayoutAlgorithmForElement(
      document().getElementsByTagName("html")->item(0));
  auto* body_fragment =
      toNGPhysicalBoxFragment(html_fragment->Children()[0].get());
  auto* container_fragment =
      toNGPhysicalBoxFragment(body_fragment->Children()[0].get());
  auto* empty_block1 =
      toNGPhysicalBoxFragment(container_fragment->Children()[0].get());
  // empty-block1's margin == 8
  EXPECT_THAT(NGPhysicalOffset(LayoutUnit(8), LayoutUnit(8)),
              empty_block1->Offset());

  auto* empty_block2 =
      toNGPhysicalBoxFragment(container_fragment->Children()[1].get());
  // empty-block2's margin == 50
  EXPECT_THAT(NGPhysicalOffset(LayoutUnit(0), LayoutUnit(50)),
              empty_block2->Offset());
}

// Verifies that we can correctly position blocks with clearance and
// intruding floats.
TEST_F(NGBlockLayoutAlgorithmTest,
       PositionBlocksWithClearanceAndIntrudingFloats) {
  setBodyInnerHTML(R"HTML(
    <!DOCTYPE html>
    <style>
    body { margin: 80px; }
    #left-float {
      background: green;
      float: left;
      width: 50px;
      height: 50px;
    }
    #right-float {
      background: red;
      float: right;
      margin: 0 80px 0 10px;
      width: 50px;
      height: 80px;
    }
    #block1 {
      outline: purple solid;
      height: 30px;
      margin: 130px 0 20px 0;
    }
    #zero {
     margin-top: 30px;
    }
    #container-clear {
      clear: left;
      outline: orange solid;
    }
    #clears-right {
      clear: right;
      height: 20px;
      background: lightblue;
    }
    </style>

    <div id="left-float"></div>
    <div id="right-float"></div>
    <div id="block1"></div>
    <div id="container-clear">
      <div id="zero"></div>
      <div id="clears-right"></div>
    </div>
  )HTML");

  // Run LayoutNG algorithm.
  RefPtr<NGPhysicalBoxFragment> html_fragment;
  std::tie(html_fragment, std::ignore) = RunBlockLayoutAlgorithmForElement(
      document().getElementsByTagName("html")->item(0));
  auto* body_fragment =
      toNGPhysicalBoxFragment(html_fragment->Children()[0].get());
  ASSERT_EQ(2UL, body_fragment->Children().size());

  // Verify #container-clear block
  auto* container_clear_fragment =
      toNGPhysicalBoxFragment(body_fragment->Children()[1].get());
  // 60 = block1's height 30 + std::max(block1's margin 20, zero's margin 30)
  EXPECT_THAT(NGPhysicalOffset(LayoutUnit(0), LayoutUnit(60)),
              container_clear_fragment->Offset());
  Element* container_clear = document().getElementById("container-clear");
  // 190 = block1's margin 130 + block1's height 30 +
  //       std::max(block1's margin 20, zero's margin 30)
  EXPECT_THAT(container_clear->offsetTop(), 190);

  // Verify #clears-right block
  ASSERT_EQ(2UL, container_clear_fragment->Children().size());
  auto* clears_right_fragment =
      toNGPhysicalBoxFragment(container_clear_fragment->Children()[1].get());
  // 20 = right-float's block end offset (130 + 80) -
  //      container_clear->offsetTop() 190
  EXPECT_THAT(NGPhysicalOffset(LayoutUnit(0), LayoutUnit(20)),
              clears_right_fragment->Offset());
}

// Tests that a block won't fragment if it doesn't reach the fragmentation line.
TEST_F(NGBlockLayoutAlgorithmTest, NoFragmentation) {
  setBodyInnerHTML(R"HTML(
      <!DOCTYPE html>
      <style>
        #container {
          width: 150px;
          height: 200px;
        }
      </style>
      <div id='container'></div>
  )HTML");

  LayoutUnit kFragmentainerSpaceAvailable(200);

  NGBlockNode* node = new NGBlockNode(
      toLayoutBlockFlow(getLayoutObjectByElementId("container")));
  RefPtr<NGConstraintSpace> space = ConstructConstraintSpace(
      kHorizontalTopBottom, TextDirection::kLtr,
      NGLogicalSize(LayoutUnit(1000), NGSizeIndefinite), false,
      kFragmentainerSpaceAvailable);

  // We should only have one 150x200 fragment with no fragmentation.
  RefPtr<const NGPhysicalFragment> fragment =
      NGBlockLayoutAlgorithm(node, space.get()).Layout()->PhysicalFragment();
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(150), LayoutUnit(200)), fragment->Size());
  ASSERT_TRUE(fragment->BreakToken()->IsFinished());
}

// Tests that a block will fragment if it reaches the fragmentation line.
TEST_F(NGBlockLayoutAlgorithmTest, SimpleFragmentation) {
  setBodyInnerHTML(R"HTML(
      <!DOCTYPE html>
      <style>
        #container {
          width: 150px;
          height: 300px;
        }
      </style>
      <div id='container'></div>
  )HTML");

  LayoutUnit kFragmentainerSpaceAvailable(200);

  NGBlockNode* node = new NGBlockNode(
      toLayoutBlockFlow(getLayoutObjectByElementId("container")));
  RefPtr<NGConstraintSpace> space = ConstructConstraintSpace(
      kHorizontalTopBottom, TextDirection::kLtr,
      NGLogicalSize(LayoutUnit(1000), NGSizeIndefinite), false,
      kFragmentainerSpaceAvailable);

  RefPtr<const NGPhysicalFragment> fragment =
      NGBlockLayoutAlgorithm(node, space.get()).Layout()->PhysicalFragment();
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(150), LayoutUnit(200)), fragment->Size());
  ASSERT_FALSE(fragment->BreakToken()->IsFinished());

  fragment = NGBlockLayoutAlgorithm(node, space.get(),
                                    toNGBlockBreakToken(fragment->BreakToken()))
                 .Layout()
                 ->PhysicalFragment();
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(150), LayoutUnit(100)), fragment->Size());
  ASSERT_TRUE(fragment->BreakToken()->IsFinished());
}

// Tests that children inside the same block formatting context fragment when
// reaching a fragmentation line.
TEST_F(NGBlockLayoutAlgorithmTest, InnerChildrenFragmentation) {
  setBodyInnerHTML(R"HTML(
      <!DOCTYPE html>
      <style>
        #container {
          width: 150px;
          padding-top: 20px;
        }
        #child1 {
          height: 200px;
          margin-bottom: 20px;
        }
        #child2 {
          height: 100px;
          margin-top: 20px;
        }
      </style>
      <div id='container'>
        <div id='child1'></div>
        <div id='child2'></div>
      </div>
  )HTML");

  LayoutUnit kFragmentainerSpaceAvailable(200);

  NGBlockNode* node = new NGBlockNode(
      toLayoutBlockFlow(getLayoutObjectByElementId("container")));
  RefPtr<NGConstraintSpace> space = ConstructConstraintSpace(
      kHorizontalTopBottom, TextDirection::kLtr,
      NGLogicalSize(LayoutUnit(1000), NGSizeIndefinite), false,
      kFragmentainerSpaceAvailable);

  RefPtr<const NGPhysicalFragment> fragment =
      NGBlockLayoutAlgorithm(node, space.get()).Layout()->PhysicalFragment();
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(150), LayoutUnit(200)), fragment->Size());
  ASSERT_FALSE(fragment->BreakToken()->IsFinished());

  FragmentChildIterator iterator(toNGPhysicalBoxFragment(fragment.get()));
  const NGPhysicalBoxFragment* child = iterator.NextChild();
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(150), LayoutUnit(180)), child->Size());
  EXPECT_EQ(NGPhysicalOffset(LayoutUnit(0), LayoutUnit(20)), child->Offset());

  EXPECT_FALSE(iterator.NextChild());

  fragment = NGBlockLayoutAlgorithm(node, space.get(),
                                    toNGBlockBreakToken(fragment->BreakToken()))
                 .Layout()
                 ->PhysicalFragment();
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(150), LayoutUnit(140)), fragment->Size());
  ASSERT_TRUE(fragment->BreakToken()->IsFinished());

  iterator.SetParent(toNGPhysicalBoxFragment(fragment.get()));
  child = iterator.NextChild();
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(150), LayoutUnit(20)), child->Size());
  EXPECT_EQ(NGPhysicalOffset(LayoutUnit(0), LayoutUnit(0)), child->Offset());

  child = iterator.NextChild();
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(150), LayoutUnit(100)), child->Size());
  EXPECT_EQ(NGPhysicalOffset(LayoutUnit(0), LayoutUnit(40)), child->Offset());

  EXPECT_FALSE(iterator.NextChild());
}

// Tests that children which establish new formatting contexts fragment
// correctly.
TEST_F(NGBlockLayoutAlgorithmTest,
       InnerFormattingContextChildrenFragmentation) {
  setBodyInnerHTML(R"HTML(
      <!DOCTYPE html>
      <style>
        #container {
          width: 150px;
          padding-top: 20px;
        }
        #child1 {
          height: 200px;
          margin-bottom: 20px;
          contain: paint;
        }
        #child2 {
          height: 100px;
          margin-top: 20px;
          contain: paint;
        }
      </style>
      <div id='container'>
        <div id='child1'></div>
        <div id='child2'></div>
      </div>
  )HTML");

  LayoutUnit kFragmentainerSpaceAvailable(200);

  NGBlockNode* node = new NGBlockNode(
      toLayoutBlockFlow(getLayoutObjectByElementId("container")));
  RefPtr<NGConstraintSpace> space = ConstructConstraintSpace(
      kHorizontalTopBottom, TextDirection::kLtr,
      NGLogicalSize(LayoutUnit(1000), NGSizeIndefinite), false,
      kFragmentainerSpaceAvailable);

  RefPtr<const NGPhysicalFragment> fragment =
      NGBlockLayoutAlgorithm(node, space.get()).Layout()->PhysicalFragment();
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(150), LayoutUnit(200)), fragment->Size());
  ASSERT_FALSE(fragment->BreakToken()->IsFinished());

  FragmentChildIterator iterator(toNGPhysicalBoxFragment(fragment.get()));
  const NGPhysicalBoxFragment* child = iterator.NextChild();
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(150), LayoutUnit(180)), child->Size());
  EXPECT_EQ(NGPhysicalOffset(LayoutUnit(0), LayoutUnit(20)), child->Offset());

  EXPECT_FALSE(iterator.NextChild());

  fragment = NGBlockLayoutAlgorithm(node, space.get(),
                                    toNGBlockBreakToken(fragment->BreakToken()))
                 .Layout()
                 ->PhysicalFragment();
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(150), LayoutUnit(140)), fragment->Size());
  ASSERT_TRUE(fragment->BreakToken()->IsFinished());

  iterator.SetParent(toNGPhysicalBoxFragment(fragment.get()));
  child = iterator.NextChild();
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(150), LayoutUnit(20)), child->Size());
  EXPECT_EQ(NGPhysicalOffset(LayoutUnit(0), LayoutUnit(0)), child->Offset());

  child = iterator.NextChild();
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(150), LayoutUnit(100)), child->Size());
  EXPECT_EQ(NGPhysicalOffset(LayoutUnit(0), LayoutUnit(40)), child->Offset());

  EXPECT_FALSE(iterator.NextChild());
}

// Tests that children inside a block container will fragment if the container
// doesn't reach the fragmentation line.
TEST_F(NGBlockLayoutAlgorithmTest, InnerChildrenFragmentationSmallHeight) {
  setBodyInnerHTML(R"HTML(
      <!DOCTYPE html>
      <style>
        #container {
          width: 150px;
          padding-top: 20px;
          height: 50px;
        }
        #child1 {
          height: 200px;
          margin-bottom: 20px;
        }
        #child2 {
          height: 100px;
          margin-top: 20px;
        }
      </style>
      <div id='container'>
        <div id='child1'></div>
        <div id='child2'></div>
      </div>
  )HTML");

  LayoutUnit kFragmentainerSpaceAvailable(200);

  NGBlockNode* node = new NGBlockNode(
      toLayoutBlockFlow(getLayoutObjectByElementId("container")));
  RefPtr<NGConstraintSpace> space = ConstructConstraintSpace(
      kHorizontalTopBottom, TextDirection::kLtr,
      NGLogicalSize(LayoutUnit(1000), NGSizeIndefinite), false,
      kFragmentainerSpaceAvailable);

  RefPtr<const NGPhysicalFragment> fragment =
      NGBlockLayoutAlgorithm(node, space.get()).Layout()->PhysicalFragment();
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(150), LayoutUnit(70)), fragment->Size());
  ASSERT_FALSE(fragment->BreakToken()->IsFinished());

  FragmentChildIterator iterator(toNGPhysicalBoxFragment(fragment.get()));
  const NGPhysicalBoxFragment* child = iterator.NextChild();
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(150), LayoutUnit(180)), child->Size());
  EXPECT_EQ(NGPhysicalOffset(LayoutUnit(0), LayoutUnit(20)), child->Offset());

  EXPECT_FALSE(iterator.NextChild());

  fragment = NGBlockLayoutAlgorithm(node, space.get(),
                                    toNGBlockBreakToken(fragment->BreakToken()))
                 .Layout()
                 ->PhysicalFragment();
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(150), LayoutUnit(0)), fragment->Size());
  ASSERT_TRUE(fragment->BreakToken()->IsFinished());

  iterator.SetParent(toNGPhysicalBoxFragment(fragment.get()));
  child = iterator.NextChild();
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(150), LayoutUnit(20)), child->Size());
  EXPECT_EQ(NGPhysicalOffset(LayoutUnit(0), LayoutUnit(0)), child->Offset());

  child = iterator.NextChild();
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(150), LayoutUnit(100)), child->Size());
  EXPECT_EQ(NGPhysicalOffset(LayoutUnit(0), LayoutUnit(40)), child->Offset());

  EXPECT_FALSE(iterator.NextChild());
}

}  // namespace
}  // namespace blink
