// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_block_layout_algorithm.h"

#include "core/dom/NodeComputedStyle.h"
#include "core/dom/TagCollection.h"
#include "core/layout/ng/layout_ng_block_flow.h"
#include "core/layout/ng/ng_block_node.h"
#include "core/layout/ng/ng_constraint_space.h"
#include "core/layout/ng/ng_constraint_space_builder.h"
#include "core/layout/ng/ng_floating_object.h"
#include "core/layout/ng/ng_length_utils.h"
#include "core/layout/LayoutTestHelper.h"
#include "core/layout/ng/ng_physical_box_fragment.h"
#include "core/layout/ng/ng_physical_fragment.h"
#include "core/layout/ng/ng_units.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "core/style/ComputedStyle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace {

using testing::ElementsAre;
using testing::Pointee;

NGConstraintSpace* ConstructConstraintSpace(NGWritingMode writing_mode,
                                            TextDirection direction,
                                            NGLogicalSize size,
                                            bool shrink_to_fit = false) {
  return NGConstraintSpaceBuilder(writing_mode)
      .SetAvailableSize(size)
      .SetPercentageResolutionSize(size)
      .SetTextDirection(direction)
      .SetWritingMode(writing_mode)
      .SetIsShrinkToFit(shrink_to_fit)
      .ToConstraintSpace();
}

typedef bool TestParamLayoutNG;
class NGBlockLayoutAlgorithmTest
    : public ::testing::WithParamInterface<TestParamLayoutNG>,
      public RenderingTest {
 public:
  NGBlockLayoutAlgorithmTest() {}

 protected:
  void SetUp() override {
    style_ = ComputedStyle::create();
    RenderingTest::SetUp();
    enableCompositing();
  }

  NGPhysicalBoxFragment* RunBlockLayoutAlgorithm(NGConstraintSpace* space,
                                                 NGBlockNode* first_child) {
    NGBlockNode parent(style_.get());
    parent.SetFirstChild(first_child);

    NGBlockLayoutAlgorithm algorithm(/* layout_object */ nullptr, style_.get(),
                                     first_child, space);

    NGPhysicalFragment* fragment = algorithm.Layout();
    return toNGPhysicalBoxFragment(fragment);
  }

  std::pair<NGPhysicalBoxFragment*, NGConstraintSpace*>
  RunBlockLayoutAlgorithmForElement(Element* element) {
    LayoutNGBlockFlow* block_flow =
        toLayoutNGBlockFlow(element->layoutObject());
    NGConstraintSpace* space =
        NGConstraintSpace::CreateFromLayoutObject(*block_flow);
    NGPhysicalBoxFragment* fragment = RunBlockLayoutAlgorithm(
        space, new NGBlockNode(element->layoutObject()->slowFirstChild()));
    return std::make_pair(fragment, space);
  }

  MinAndMaxContentSizes RunComputeMinAndMax(NGBlockNode* first_child) {
    // The constraint space is not used for min/max computation, but we need
    // it to create the algorithm.
    NGConstraintSpace* space =
        ConstructConstraintSpace(kHorizontalTopBottom, TextDirection::kLtr,
                                 NGLogicalSize(LayoutUnit(), LayoutUnit()));
    NGBlockLayoutAlgorithm algorithm(/* layout_object */ nullptr, style_.get(),
                                     first_child, space);
    MinAndMaxContentSizes sizes;
    EXPECT_TRUE(algorithm.ComputeMinAndMaxContentSizes(&sizes));
    return sizes;
  }

  RefPtr<ComputedStyle> style_;
};

TEST_F(NGBlockLayoutAlgorithmTest, FixedSize) {
  style_->setWidth(Length(30, Fixed));
  style_->setHeight(Length(40, Fixed));

  auto* space = ConstructConstraintSpace(
      kHorizontalTopBottom, TextDirection::kLtr,
      NGLogicalSize(LayoutUnit(100), NGSizeIndefinite));
  NGPhysicalFragment* frag = RunBlockLayoutAlgorithm(space, nullptr);

  EXPECT_EQ(LayoutUnit(30), frag->Width());
  EXPECT_EQ(LayoutUnit(40), frag->Height());
}

// Verifies that two children are laid out with the correct size and position.
TEST_F(NGBlockLayoutAlgorithmTest, LayoutBlockChildren) {
  const int kWidth = 30;
  const int kHeight1 = 20;
  const int kHeight2 = 30;
  const int kMarginTop = 5;
  const int kMarginBottom = 20;
  style_->setWidth(Length(kWidth, Fixed));

  RefPtr<ComputedStyle> first_style = ComputedStyle::create();
  first_style->setHeight(Length(kHeight1, Fixed));
  NGBlockNode* first_child = new NGBlockNode(first_style.get());

  RefPtr<ComputedStyle> second_style = ComputedStyle::create();
  second_style->setHeight(Length(kHeight2, Fixed));
  second_style->setMarginTop(Length(kMarginTop, Fixed));
  second_style->setMarginBottom(Length(kMarginBottom, Fixed));
  NGBlockNode* second_child = new NGBlockNode(second_style.get());

  first_child->SetNextSibling(second_child);

  auto* space = ConstructConstraintSpace(
      kHorizontalTopBottom, TextDirection::kLtr,
      NGLogicalSize(LayoutUnit(100), NGSizeIndefinite));
  NGPhysicalBoxFragment* frag = RunBlockLayoutAlgorithm(space, first_child);

  EXPECT_EQ(LayoutUnit(kWidth), frag->Width());
  EXPECT_EQ(LayoutUnit(kHeight1 + kHeight2 + kMarginTop), frag->Height());
  EXPECT_EQ(NGPhysicalFragment::kFragmentBox, frag->Type());
  ASSERT_EQ(frag->Children().size(), 2UL);

  const NGPhysicalFragment* child = frag->Children()[0];
  EXPECT_EQ(kHeight1, child->Height());
  EXPECT_EQ(0, child->TopOffset());

  child = frag->Children()[1];
  EXPECT_EQ(kHeight2, child->Height());
  EXPECT_EQ(kHeight1 + kMarginTop, child->TopOffset());
}

// Verifies that a child is laid out correctly if it's writing mode is different
// from the parent's one.
//
// Test case's HTML representation:
// <div style="writing-mode: vertical-lr;">
//   <div style="width:50px;
//       height: 50px; margin-left: 100px;
//       writing-mode: horizontal-tb;"></div>
// </div>
TEST_F(NGBlockLayoutAlgorithmTest, LayoutBlockChildrenWithWritingMode) {
  const int kWidth = 50;
  const int kHeight = 50;
  const int kMarginLeft = 100;

  RefPtr<ComputedStyle> div1_style = ComputedStyle::create();
  div1_style->setWritingMode(WritingMode::kVerticalLr);
  NGBlockNode* div1 = new NGBlockNode(div1_style.get());

  RefPtr<ComputedStyle> div2_style = ComputedStyle::create();
  div2_style->setHeight(Length(kHeight, Fixed));
  div2_style->setWidth(Length(kWidth, Fixed));
  div1_style->setWritingMode(WritingMode::kHorizontalTb);
  div2_style->setMarginLeft(Length(kMarginLeft, Fixed));
  NGBlockNode* div2 = new NGBlockNode(div2_style.get());

  div1->SetFirstChild(div2);

  auto* space =
      ConstructConstraintSpace(kHorizontalTopBottom, TextDirection::kLtr,
                               NGLogicalSize(LayoutUnit(500), LayoutUnit(500)));
  NGPhysicalBoxFragment* frag = RunBlockLayoutAlgorithm(space, div1);

  const NGPhysicalFragment* child = frag->Children()[0];
  // DIV2
  child = static_cast<const NGPhysicalBoxFragment*>(child)->Children()[0];

  EXPECT_EQ(kHeight, child->Height());
  EXPECT_EQ(0, child->TopOffset());
  EXPECT_EQ(kMarginLeft, child->LeftOffset());
}

// Verifies the collapsing margins case for the next pair:
// - top margin of a box and top margin of its first in-flow child.
// Verifies that floats are positioned at the top of the first child that can
// determine its position after margins collapsed.
// TODO(glebl): Enable with new the float/margins collapsing algorithm.
TEST_F(NGBlockLayoutAlgorithmTest, DISABLED_CollapsingMarginsCase1WithFloats) {
  setBodyInnerHTML(
      "<style>"
      "  #container {"
      "    height: 200px;"
      "    width: 200px;"
      "    margin-top: 10px;"
      "    padding: 0 7px;"
      "    background-color: red;"
      "  }"
      "  #first-child {"
      "    margin-top: 20px;"
      "    height: 10px;"
      "    background-color: blue;"
      "  }"
      "  #float-child-left {"
      "    float: left;"
      "    height: 10px;"
      "    width: 10px;"
      "    padding: 10px;"
      "    margin: 10px;"
      "    background-color: green;"
      "  }"
      "  #float-child-right {"
      "    float: right;"
      "    height: 30px;"
      "    width: 30px;"
      "    background-color: pink;"
      "  }"
      "</style>"
      "<div id='container'>"
      "  <div id='float-child-left'></div>"
      "  <div id='float-child-right'></div>"
      "  <div id='first-child'></div>"
      "</div>");

  // ** Run LayoutNG algorithm **
  NGConstraintSpace* space;
  NGPhysicalBoxFragment* fragment;
  std::tie(fragment, space) = RunBlockLayoutAlgorithmForElement(
      document().getElementsByTagName("html")->item(0));
  ASSERT_EQ(fragment->Children().size(), 1UL);
  auto* body_fragment = toNGPhysicalBoxFragment(fragment->Children()[0]);
  // 20 = max(first child's margin top, containers's margin top)
  int body_top_offset = 20;
  EXPECT_THAT(LayoutUnit(body_top_offset), body_fragment->TopOffset());
  // 8 = body's margin
  int body_left_offset = 8;
  EXPECT_THAT(LayoutUnit(body_left_offset), body_fragment->LeftOffset());
  ASSERT_EQ(1UL, body_fragment->Children().size());
  auto* container_fragment =
      toNGPhysicalBoxFragment(body_fragment->Children()[0]);
  // 0 = collapsed with body's margin
  EXPECT_THAT(LayoutUnit(0), container_fragment->TopOffset());
  ASSERT_EQ(1UL, container_fragment->Children().size());
  auto* first_child_fragment =
      toNGPhysicalBoxFragment(container_fragment->Children()[0]);
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

// Verifies the collapsing margins case for the next pair:
// - bottom margin of box and top margin of its next in-flow following sibling.
// - top and bottom margins of a box that does not establish a new block
//   formatting context and that has zero computed 'min-height', zero or 'auto'
//   computed 'height', and no in-flow children
// TODO(glebl): Enable with new the float/margins collapsing algorithm.
TEST_F(NGBlockLayoutAlgorithmTest, DISABLED_CollapsingMarginsCase2WithFloats) {
  setBodyInnerHTML(
      "<style>"
      "#first-child {"
      "  background-color: red;"
      "  height: 50px;"
      "  margin-bottom: 20px;"
      "}"
      "#float-between-empties {"
      "  background-color: green;"
      "  float: left;"
      "  height: 30px;"
      "  width: 30px;"
      "}"
      "#float-between-nonempties {"
      "  background-color: lightgreen;"
      "  float: left;"
      "  height: 40px;"
      "  width: 40px;"
      "}"
      "#float-top-align {"
      "  background-color: seagreen;"
      "  float: left;"
      "  height: 50px;"
      "  width: 50px;"
      "}"
      "#second-child {"
      "  background-color: blue;"
      "  height: 50px;"
      "  margin-top: 10px;"
      "}"
      "</style>"
      "<div id='first-child'>"
      "  <div id='empty1' style='margin-bottom: -15px'></div>"
      "  <div id='float-between-empties'></div>"
      "  <div id='empty2'></div>"
      "</div>"
      "<div id='float-between-nonempties'></div>"
      "<div id='second-child'>"
      "  <div id='float-top-align'></div>"
      "  <div id='empty3'></div>"
      "  <div id='empty4' style='margin-top: -30px'></div>"
      "</div>"
      "<div id='empty5'></div>");

  // ** Run LayoutNG algorithm **
  NGConstraintSpace* space;
  NGPhysicalBoxFragment* fragment;
  std::tie(fragment, space) = RunBlockLayoutAlgorithmForElement(
      document().getElementsByTagName("html")->item(0));

  auto* body_fragment = toNGPhysicalBoxFragment(fragment->Children()[0]);
  // -7 = empty1's margin(-15) + body's margin(8)
  int body_top_offset = -7;
  EXPECT_THAT(LayoutUnit(body_top_offset), body_fragment->TopOffset());
  int body_left_offset = 8;
  EXPECT_THAT(LayoutUnit(body_top_offset), body_fragment->TopOffset());
  ASSERT_EQ(3UL, body_fragment->Children().size());

  auto* first_child_fragment =
      toNGPhysicalBoxFragment(body_fragment->Children()[0]);
  EXPECT_THAT(LayoutUnit(), first_child_fragment->TopOffset());

  auto* second_child_fragment =
      toNGPhysicalBoxFragment(body_fragment->Children()[1]);
  // 40 = first_child's height(50) - margin's collapsing result(10)
  int second_child_block_offset = 40;
  EXPECT_THAT(LayoutUnit(second_child_block_offset),
              second_child_fragment->TopOffset());

  auto* empty5_fragment = toNGPhysicalBoxFragment(body_fragment->Children()[2]);
  // 90 = first_child's height(50) + collapsed margins(-10) +
  // second child's height(50)
  int empty5_fragment_block_offset = 90;
  EXPECT_THAT(LayoutUnit(empty5_fragment_block_offset),
              empty5_fragment->TopOffset());

  ASSERT_EQ(3UL, body_fragment->PositionedFloats().size());
  auto float_nonempties_fragment =
      body_fragment->PositionedFloats().at(1)->fragment;
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
// TODO(glebl): Enable with new the float/margins collapsing algorithm.
TEST_F(NGBlockLayoutAlgorithmTest, DISABLED_CollapsingMarginsCase3) {
  setBodyInnerHTML(
      "<style>"
      " #container {"
      "   margin-bottom: 20px;"
      " }"
      " #child {"
      "   margin-bottom: 200px;"
      "   height: 50px;"
      " }"
      "</style>"
      "<div id='container'>"
      "  <div id='child'></div>"
      "</div>");

  const NGPhysicalBoxFragment* body_fragment;
  const NGPhysicalBoxFragment* container_fragment;
  const NGPhysicalBoxFragment* child_fragment;
  const NGPhysicalBoxFragment* fragment;
  auto run_test = [&](const Length& container_height) {
    Element* container = document().getElementById("container");
    container->mutableComputedStyle()->setHeight(container_height);
    std::tie(fragment, std::ignore) = RunBlockLayoutAlgorithmForElement(
        document().getElementsByTagName("html")->item(0));
    ASSERT_EQ(1UL, fragment->Children().size());
    body_fragment = toNGPhysicalBoxFragment(fragment->Children()[0]);
    container_fragment = toNGPhysicalBoxFragment(body_fragment->Children()[0]);
    ASSERT_EQ(1UL, container_fragment->Children().size());
    child_fragment = toNGPhysicalBoxFragment(container_fragment->Children()[0]);
  };

  // height == auto
  run_test(Length(Auto));
  // Margins are collapsed with the result 200 = std::max(20, 200)
  // The fragment size 258 == body's margin 8 + child's height 50 + 200
  EXPECT_EQ(NGPhysicalSize(LayoutUnit(800), LayoutUnit(258)), fragment->Size());
  //  EXPECT_EQ(NGMarginStrut({LayoutUnit(200)}),
  //            container_fragment->EndMarginStrut());

  // height == fixed
  run_test(Length(50, Fixed));
  // Margins are not collapsed, so fragment still has margins == 20.
  // The fragment size 78 == body's margin 8 + child's height 50 + 20
  //  EXPECT_EQ(NGPhysicalSize(LayoutUnit(800), LayoutUnit(78)),
  //            fragment->Size());
  //  EXPECT_EQ(NGMarginStrut(), container_fragment->EndMarginStrut());
}

// Verifies that 2 adjoining margins are not collapsed if there is padding or
// border that separates them.
// TODO(glebl): Enable with new the float/margins collapsing algorithm.
TEST_F(NGBlockLayoutAlgorithmTest, DISABLED_CollapsingMarginsCase4) {
  setBodyInnerHTML(
      "<style>"
      "  #container {"
      "    margin: 30px 0px;"
      "    width: 200px;"
      "  }"
      "  #child {"
      "    margin: 200px 0px;"
      "    height: 50px;"
      "    background-color: blue;"
      "  }"
      "</style>"
      "<div id='container'>"
      "  <div id='child'></div>"
      "</div>");

  const NGPhysicalBoxFragment* body_fragment;
  const NGPhysicalBoxFragment* container_fragment;
  const NGPhysicalBoxFragment* child_fragment;
  const NGPhysicalBoxFragment* fragment;
  auto run_test = [&](const Length& container_padding_top) {
    Element* container = document().getElementById("container");
    container->mutableComputedStyle()->setPaddingTop(container_padding_top);
    std::tie(fragment, std::ignore) = RunBlockLayoutAlgorithmForElement(
        document().getElementsByTagName("html")->item(0));
    ASSERT_EQ(1UL, fragment->Children().size());
    body_fragment = toNGPhysicalBoxFragment(fragment->Children()[0]);
    container_fragment = toNGPhysicalBoxFragment(body_fragment->Children()[0]);
    ASSERT_EQ(1UL, container_fragment->Children().size());
    child_fragment = toNGPhysicalBoxFragment(container_fragment->Children()[0]);
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
//
// Test case's HTML representation:
//   <div style="writing-mode: vertical-lr;">
//     <div style="margin-right: 60px; width: 60px;">vertical</div>
//     <div style="margin-left: 100px; writing-mode: horizontal-tb;">
//       horizontal
//     </div>
//   </div>
TEST_F(NGBlockLayoutAlgorithmTest, CollapsingMarginsCase5) {
  const int kVerticalDivMarginRight = 60;
  const int kVerticalDivWidth = 50;
  const int kHorizontalDivMarginLeft = 100;

  style_->setWidth(Length(500, Fixed));
  style_->setHeight(Length(500, Fixed));
  style_->setWritingMode(WritingMode::kVerticalLr);

  // Vertical DIV
  RefPtr<ComputedStyle> vertical_style = ComputedStyle::create();
  vertical_style->setMarginRight(Length(kVerticalDivMarginRight, Fixed));
  vertical_style->setWidth(Length(kVerticalDivWidth, Fixed));
  NGBlockNode* vertical_div = new NGBlockNode(vertical_style.get());

  // Horizontal DIV
  RefPtr<ComputedStyle> horizontal_style = ComputedStyle::create();
  horizontal_style->setMarginLeft(Length(kHorizontalDivMarginLeft, Fixed));
  horizontal_style->setWritingMode(WritingMode::kHorizontalTb);
  NGBlockNode* horizontal_div = new NGBlockNode(horizontal_style.get());

  vertical_div->SetNextSibling(horizontal_div);

  auto* space =
      ConstructConstraintSpace(kVerticalLeftRight, TextDirection::kLtr,
                               NGLogicalSize(LayoutUnit(500), LayoutUnit(500)));
  NGPhysicalBoxFragment* frag = RunBlockLayoutAlgorithm(space, vertical_div);

  ASSERT_EQ(frag->Children().size(), 2UL);
  const NGPhysicalFragment* child = frag->Children()[1];
  // Horizontal div
  EXPECT_EQ(0, child->TopOffset());
  EXPECT_EQ(kVerticalDivWidth + kHorizontalDivMarginLeft, child->LeftOffset());
}

// Verifies that the margin strut of a child with a different writing mode does
// not get used in the collapsing margins calculation.
//
// Test case's HTML representation:
//   <style>
//     #div1 { margin-bottom: 10px; height: 60px; writing-mode: vertical-rl; }
//     #div2 { margin-left: -20px; width: 10px; }
//     #div3 { margin-top: 40px; height: 60px; }
//   </style>
//   <div id="div1">
//      <div id="div2">vertical</div>
//   </div>
//   <div id="div3"></div>
TEST_F(NGBlockLayoutAlgorithmTest, CollapsingMarginsCase6) {
  const int kHeight = 60;
  const int kWidth = 10;
  const int kMarginBottom = 10;
  const int kMarginLeft = -20;
  const int kMarginTop = 40;

  style_->setWidth(Length(500, Fixed));
  style_->setHeight(Length(500, Fixed));

  // DIV1
  RefPtr<ComputedStyle> div1_style = ComputedStyle::create();
  div1_style->setWidth(Length(kWidth, Fixed));
  div1_style->setHeight(Length(kHeight, Fixed));
  div1_style->setWritingMode(WritingMode::kVerticalRl);
  div1_style->setMarginBottom(Length(kMarginBottom, Fixed));
  NGBlockNode* div1 = new NGBlockNode(div1_style.get());

  // DIV2
  RefPtr<ComputedStyle> div2_style = ComputedStyle::create();
  div2_style->setWidth(Length(kWidth, Fixed));
  div2_style->setMarginLeft(Length(kMarginLeft, Fixed));
  NGBlockNode* div2 = new NGBlockNode(div2_style.get());

  // DIV3
  RefPtr<ComputedStyle> div3_style = ComputedStyle::create();
  div3_style->setHeight(Length(kHeight, Fixed));
  div3_style->setMarginTop(Length(kMarginTop, Fixed));
  NGBlockNode* div3 = new NGBlockNode(div3_style.get());

  div1->SetFirstChild(div2);
  div1->SetNextSibling(div3);

  auto* space =
      ConstructConstraintSpace(kHorizontalTopBottom, TextDirection::kLtr,
                               NGLogicalSize(LayoutUnit(500), LayoutUnit(500)));
  NGPhysicalBoxFragment* frag = RunBlockLayoutAlgorithm(space, div1);

  ASSERT_EQ(frag->Children().size(), 2UL);

  const NGPhysicalFragment* child1 = frag->Children()[0];
  EXPECT_EQ(0, child1->TopOffset());
  EXPECT_EQ(kHeight, child1->Height());

  const NGPhysicalFragment* child2 = frag->Children()[1];
  EXPECT_EQ(kHeight + std::max(kMarginBottom, kMarginTop), child2->TopOffset());
}

// Verifies that a box's size includes its borders and padding, and that
// children are positioned inside the content box.
//
// Test case's HTML representation:
// <style>
//   #div1 { width:100px; height:100px; }
//   #div1 { border-style:solid; border-width:1px 2px 3px 4px; }
//   #div1 { padding:5px 6px 7px 8px; }
// </style>
// <div id="div1">
//    <div id="div2"></div>
// </div>
TEST_F(NGBlockLayoutAlgorithmTest, BorderAndPadding) {
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
  RefPtr<ComputedStyle> div1_style = ComputedStyle::create();

  div1_style->setWidth(Length(kWidth, Fixed));
  div1_style->setHeight(Length(kHeight, Fixed));

  div1_style->setBorderTopWidth(kBorderTop);
  div1_style->setBorderTopStyle(BorderStyleSolid);
  div1_style->setBorderRightWidth(kBorderRight);
  div1_style->setBorderRightStyle(BorderStyleSolid);
  div1_style->setBorderBottomWidth(kBorderBottom);
  div1_style->setBorderBottomStyle(BorderStyleSolid);
  div1_style->setBorderLeftWidth(kBorderLeft);
  div1_style->setBorderLeftStyle(BorderStyleSolid);

  div1_style->setPaddingTop(Length(kPaddingTop, Fixed));
  div1_style->setPaddingRight(Length(kPaddingRight, Fixed));
  div1_style->setPaddingBottom(Length(kPaddingBottom, Fixed));
  div1_style->setPaddingLeft(Length(kPaddingLeft, Fixed));
  NGBlockNode* div1 = new NGBlockNode(div1_style.get());

  RefPtr<ComputedStyle> div2_style = ComputedStyle::create();
  NGBlockNode* div2 = new NGBlockNode(div2_style.get());

  div1->SetFirstChild(div2);

  auto* space = ConstructConstraintSpace(
      kHorizontalTopBottom, TextDirection::kLtr,
      NGLogicalSize(LayoutUnit(1000), NGSizeIndefinite));
  NGPhysicalBoxFragment* frag = RunBlockLayoutAlgorithm(space, div1);

  ASSERT_EQ(frag->Children().size(), 1UL);

  // div1
  const NGPhysicalFragment* child = frag->Children()[0];
  EXPECT_EQ(kBorderLeft + kPaddingLeft + kWidth + kPaddingRight + kBorderRight,
            child->Width());
  EXPECT_EQ(kBorderTop + kPaddingTop + kHeight + kPaddingBottom + kBorderBottom,
            child->Height());

  ASSERT_TRUE(child->Type() == NGPhysicalFragment::kFragmentBox);
  ASSERT_EQ(static_cast<const NGPhysicalBoxFragment*>(child)->Children().size(),
            1UL);

  // div2
  child = static_cast<const NGPhysicalBoxFragment*>(child)->Children()[0];
  EXPECT_EQ(kBorderTop + kPaddingTop, child->TopOffset());
  EXPECT_EQ(kBorderLeft + kPaddingLeft, child->LeftOffset());
}

TEST_F(NGBlockLayoutAlgorithmTest, PercentageResolutionSize) {
  const int kPaddingLeft = 10;
  const int kWidth = 30;
  style_->setWidth(Length(kWidth, Fixed));
  style_->setPaddingLeft(Length(kPaddingLeft, Fixed));

  RefPtr<ComputedStyle> first_style = ComputedStyle::create();
  first_style->setWidth(Length(40, Percent));
  NGBlockNode* first_child = new NGBlockNode(first_style.get());

  auto* space = ConstructConstraintSpace(
      kHorizontalTopBottom, TextDirection::kLtr,
      NGLogicalSize(LayoutUnit(100), NGSizeIndefinite));
  NGPhysicalBoxFragment* frag = RunBlockLayoutAlgorithm(space, first_child);

  EXPECT_EQ(LayoutUnit(kWidth + kPaddingLeft), frag->Width());
  EXPECT_EQ(NGPhysicalFragment::kFragmentBox, frag->Type());
  ASSERT_EQ(frag->Children().size(), 1UL);

  const NGPhysicalFragment* child = frag->Children()[0];
  EXPECT_EQ(LayoutUnit(12), child->Width());
}

// A very simple auto margin case. We rely on the tests in ng_length_utils_test
// for the more complex cases; just make sure we handle auto at all here.
TEST_F(NGBlockLayoutAlgorithmTest, AutoMargin) {
  const int kPaddingLeft = 10;
  const int kWidth = 30;
  style_->setWidth(Length(kWidth, Fixed));
  style_->setPaddingLeft(Length(kPaddingLeft, Fixed));

  RefPtr<ComputedStyle> first_style = ComputedStyle::create();
  const int kChildWidth = 10;
  first_style->setWidth(Length(kChildWidth, Fixed));
  first_style->setMarginLeft(Length(Auto));
  first_style->setMarginRight(Length(Auto));
  NGBlockNode* first_child = new NGBlockNode(first_style.get());

  auto* space = ConstructConstraintSpace(
      kHorizontalTopBottom, TextDirection::kLtr,
      NGLogicalSize(LayoutUnit(100), NGSizeIndefinite));
  NGPhysicalBoxFragment* frag = RunBlockLayoutAlgorithm(space, first_child);

  EXPECT_EQ(LayoutUnit(kWidth + kPaddingLeft), frag->Width());
  EXPECT_EQ(NGPhysicalFragment::kFragmentBox, frag->Type());
  EXPECT_EQ(LayoutUnit(kWidth + kPaddingLeft), frag->WidthOverflow());
  ASSERT_EQ(1UL, frag->Children().size());

  const NGPhysicalFragment* child = frag->Children()[0];
  EXPECT_EQ(LayoutUnit(kChildWidth), child->Width());
  EXPECT_EQ(LayoutUnit(kPaddingLeft + 10), child->LeftOffset());
  EXPECT_EQ(LayoutUnit(0), child->TopOffset());
}

// Verifies that floats can be correctly positioned if they are inside of nested
// empty blocks.
// TODO(glebl): Enable with new the float/margins collapsing algorithm.
TEST_F(NGBlockLayoutAlgorithmTest, DISABLED_PositionFloatInsideEmptyBlocks) {
  setBodyInnerHTML(
      "<!DOCTYPE html>"
      "<style>"
      "  #container {"
      "    height: 200px;"
      "    width: 200px;"
      "  }"
      "  #empty1 {"
      "    margin: 20px;"
      "    padding: 0 20px;"
      "  }"
      "  #empty2 {"
      "    margin: 15px;"
      "    padding: 0 15px;"
      "  }"
      "  #float {"
      "    float: left;"
      "    height: 5px;"
      "    width: 5px;"
      "    padding: 10px;"
      "    margin: 10px;"
      "    background-color: green;"
      "  }"
      "</style>"
      "<div id='container'>"
      "  <div id='empty1'>"
      "    <div id='empty2'>"
      "      <div id='float'></div>"
      "    </div>"
      "  </div>"
      "</div>");

  // ** Run LayoutNG algorithm **
  NGConstraintSpace* space;
  NGPhysicalBoxFragment* fragment;
  std::tie(fragment, space) = RunBlockLayoutAlgorithmForElement(
      document().getElementsByTagName("html")->item(0));

  auto* body_fragment = toNGPhysicalBoxFragment(fragment->Children()[0]);
  // 20 = std::max(empty1's margin, empty2's margin, body's margin)
  int body_top_offset = 20;
  EXPECT_THAT(LayoutUnit(body_top_offset), body_fragment->TopOffset());
  ASSERT_EQ(1UL, body_fragment->Children().size());
  auto* container_fragment =
      toNGPhysicalBoxFragment(body_fragment->Children()[0]);
  ASSERT_EQ(1UL, container_fragment->Children().size());

  auto* empty1_fragment =
      toNGPhysicalBoxFragment(container_fragment->Children()[0]);
  // 0, vertical margins got collapsed
  EXPECT_THAT(LayoutUnit(), empty1_fragment->TopOffset());
  // 20 empty1's margin
  int empty1_inline_offset = 20;
  EXPECT_THAT(LayoutUnit(empty1_inline_offset), empty1_fragment->LeftOffset());
  ASSERT_EQ(empty1_fragment->Children().size(), 1UL);

  auto* empty2_fragment =
      toNGPhysicalBoxFragment(empty1_fragment->Children()[0]);
  // 0, vertical margins got collapsed
  EXPECT_THAT(LayoutUnit(), empty2_fragment->TopOffset());
  // 35 = empty1's padding(20) + empty2's padding(15)
  int empty2_inline_offset = 35;
  EXPECT_THAT(LayoutUnit(empty2_inline_offset), empty2_fragment->LeftOffset());

  ASSERT_EQ(1UL, body_fragment->PositionedFloats().size());
  auto float_fragment = body_fragment->PositionedFloats().at(0)->fragment;
  // 10 = float's padding
  EXPECT_THAT(LayoutUnit(10), float_fragment->TopOffset());
  // 25 = empty2's padding(15) + float's padding(10)
  int float_inline_offset = 25;
  EXPECT_THAT(float_fragment->LeftOffset(), LayoutUnit(float_inline_offset));

  // ** Verify layout tree **
  Element* left_float = document().getElementById("float");
  // 88 = body's margin(8) +
  // empty1's padding and margin + empty2's padding and margins + float's
  // padding
  EXPECT_THAT(left_float->offsetLeft(), 88);
  // 30 = body_top_offset(collapsed margins result) + float's padding
  EXPECT_THAT(body_top_offset + 10, left_float->offsetTop());

  // ** Legacy Floating objects **
  Element* body = document().getElementsByTagName("body")->item(0);
  auto& floating_objects =
      const_cast<FloatingObjects*>(
          toLayoutBlockFlow(body->layoutObject())->floatingObjects())
          ->mutableSet();
  ASSERT_EQ(1UL, floating_objects.size());
  auto floating_object = floating_objects.takeFirst();
  ASSERT_TRUE(floating_object->isPlaced());
  // 80 = float_inline_offset(25) + accumulative offset of empty blocks(35 + 20)
  EXPECT_THAT(LayoutUnit(80), floating_object->x());
  // 10 = float's padding
  EXPECT_THAT(LayoutUnit(10), floating_object->y());
}

// Verifies that left/right floating and regular blocks can be positioned
// correctly by the algorithm.
// TODO(glebl): Enable with new the float/margins collapsing algorithm.
TEST_F(NGBlockLayoutAlgorithmTest, DISABLED_PositionFloatFragments) {
  setBodyInnerHTML(
      "<style>"
      "  #container {"
      "    height: 200px;"
      "    width: 200px;"
      "  }"
      "  #left-float {"
      "    background-color: red;"
      "    float:left;"
      "    height: 30px;"
      "    width: 30px;"
      "  }"
      "  #left-wide-float {"
      "    background-color: greenyellow;"
      "    float:left;"
      "    height: 30px;"
      "    width: 180px;"
      "  }"
      "  #regular {"
      "    width: 40px;"
      "    height: 40px;"
      "    background-color: green;"
      "  }"
      "  #right-float {"
      "    background-color: cyan;"
      "    float:right;"
      "    width: 50px;"
      "    height: 50px;"
      "  }"
      "  #left-float-with-margin {"
      "    background-color: black;"
      "    float:left;"
      "    height: 120px;"
      "    margin: 10px;"
      "    width: 120px;"
      "  }"
      "</style>"
      "<div id='container'>"
      "  <div id='left-float'></div>"
      "  <div id='left-wide-float'></div>"
      "  <div id='regular'></div>"
      "  <div id='right-float'></div>"
      "  <div id='left-float-with-margin'></div>"
      "</div>");

  // ** Run LayoutNG algorithm **
  NGConstraintSpace* space;
  NGPhysicalBoxFragment* fragment;
  std::tie(fragment, space) = RunBlockLayoutAlgorithmForElement(
      document().getElementsByTagName("html")->item(0));

  // ** Verify LayoutNG fragments and the list of positioned floats **
  EXPECT_THAT(LayoutUnit(), fragment->TopOffset());
  ASSERT_EQ(1UL, fragment->Children().size());
  auto* body_fragment = toNGPhysicalBoxFragment(fragment->Children()[0]);
  EXPECT_THAT(LayoutUnit(8), body_fragment->TopOffset());
  auto* container_fragment =
      toNGPhysicalBoxFragment(body_fragment->Children()[0]);
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
      toNGPhysicalBoxFragment(container_fragment->Children()[0]);
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
// TODO(glebl): Enable with new the float/margins collapsing algorithm.
TEST_F(NGBlockLayoutAlgorithmTest, DISABLED_PositionFragmentsWithClear) {
  setBodyInnerHTML(
      "<style>"
      "  #container {"
      "    height: 200px;"
      "    width: 200px;"
      "  }"
      "  #float-left {"
      "    background-color: red;"
      "    float: left;"
      "    height: 30px;"
      "    width: 30px;"
      "  }"
      "  #float-right {"
      "    background-color: blue;"
      "    float: right;"
      "    height: 170px;"
      "    width: 40px;"
      "  }"
      "  #clearance {"
      "    background-color: yellow;"
      "    height: 60px;"
      "    width: 60px;"
      "    margin: 20px;"
      "  }"
      "  #block {"
      "    margin: 40px;"
      "    background-color: black;"
      "    height: 60px;"
      "    width: 60px;"
      "  }"
      "  #adjoining-clearance {"
      "    background-color: green;"
      "    clear: left;"
      "    height: 20px;"
      "    width: 20px;"
      "    margin: 30px;"
      "  }"
      "</style>"
      "<div id='container'>"
      "  <div id='float-left'></div>"
      "  <div id='float-right'></div>"
      "  <div id='clearance'></div>"
      "  <div id='block'></div>"
      "  <div id='adjoining-clearance'></div>"
      "</div>");

  const NGPhysicalBoxFragment* clerance_fragment;
  const NGPhysicalBoxFragment* body_fragment;
  const NGPhysicalBoxFragment* container_fragment;
  const NGPhysicalBoxFragment* block_fragment;
  const NGPhysicalBoxFragment* adjoining_clearance_fragment;
  auto run_with_clearance = [&](EClear clear_value) {
    NGPhysicalBoxFragment* fragment;
    Element* el_with_clear = document().getElementById("clearance");
    el_with_clear->mutableComputedStyle()->setClear(clear_value);
    std::tie(fragment, std::ignore) = RunBlockLayoutAlgorithmForElement(
        document().getElementsByTagName("html")->item(0));
    ASSERT_EQ(1UL, fragment->Children().size());
    body_fragment = toNGPhysicalBoxFragment(fragment->Children()[0]);
    container_fragment = toNGPhysicalBoxFragment(body_fragment->Children()[0]);
    ASSERT_EQ(3UL, container_fragment->Children().size());
    clerance_fragment =
        toNGPhysicalBoxFragment(container_fragment->Children()[0]);
    block_fragment = toNGPhysicalBoxFragment(container_fragment->Children()[1]);
    adjoining_clearance_fragment =
        toNGPhysicalBoxFragment(container_fragment->Children()[2]);
  };

  // clear: none
  run_with_clearance(EClear::ClearNone);
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
  run_with_clearance(EClear::ClearRight);
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
  run_with_clearance(EClear::ClearLeft);
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
  run_with_clearance(EClear::ClearBoth);
  EXPECT_EQ(LayoutUnit(8), body_fragment->TopOffset());
  EXPECT_EQ(LayoutUnit(0), container_fragment->TopOffset());
  EXPECT_EQ(LayoutUnit(170), clerance_fragment->TopOffset());
  EXPECT_EQ(LayoutUnit(270), block_fragment->TopOffset());
  EXPECT_EQ(LayoutUnit(370), adjoining_clearance_fragment->TopOffset());
}

// Verifies that we compute the right min and max-content size.
TEST_F(NGBlockLayoutAlgorithmTest, ComputeMinMaxContent) {
  const int kWidth = 50;
  const int kWidthChild1 = 20;
  const int kWidthChild2 = 30;

  // This should have no impact on the min/max content size.
  style_->setWidth(Length(kWidth, Fixed));

  RefPtr<ComputedStyle> first_style = ComputedStyle::create();
  first_style->setWidth(Length(kWidthChild1, Fixed));
  NGBlockNode* first_child = new NGBlockNode(first_style.get());

  RefPtr<ComputedStyle> second_style = ComputedStyle::create();
  second_style->setWidth(Length(kWidthChild2, Fixed));
  NGBlockNode* second_child = new NGBlockNode(second_style.get());

  first_child->SetNextSibling(second_child);

  MinAndMaxContentSizes sizes = RunComputeMinAndMax(first_child);
  EXPECT_EQ(kWidthChild2, sizes.min_content);
  EXPECT_EQ(kWidthChild2, sizes.max_content);
}

// Tests that we correctly handle shrink-to-fit
TEST_F(NGBlockLayoutAlgorithmTest, ShrinkToFit) {
  const int kWidthChild1 = 20;
  const int kWidthChild2 = 30;

  RefPtr<ComputedStyle> first_style = ComputedStyle::create();
  first_style->setWidth(Length(kWidthChild1, Fixed));
  NGBlockNode* first_child = new NGBlockNode(first_style.get());

  RefPtr<ComputedStyle> second_style = ComputedStyle::create();
  second_style->setWidth(Length(kWidthChild2, Fixed));
  NGBlockNode* second_child = new NGBlockNode(second_style.get());

  first_child->SetNextSibling(second_child);

  auto* space = ConstructConstraintSpace(
      kHorizontalTopBottom, TextDirection::kLtr,
      NGLogicalSize(LayoutUnit(100), NGSizeIndefinite), true);
  NGPhysicalFragment* frag = RunBlockLayoutAlgorithm(space, first_child);

  EXPECT_EQ(LayoutUnit(30), frag->Width());
}

class FragmentChildIterator
    : public GarbageCollectedFinalized<FragmentChildIterator> {
 public:
  FragmentChildIterator() {}
  FragmentChildIterator(const NGPhysicalBoxFragment* parent) {
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
    return toNGPhysicalBoxFragment(parent_->Children()[index_++]);
  }

  DEFINE_INLINE_TRACE() { visitor->trace(parent_); }

 private:
  Member<const NGPhysicalBoxFragment> parent_;
  unsigned index_;
};

// Test case's HTML representation:
//  <div id="parent" style="columns:2; column-fill:auto; column-gap:10px;
//                          width:210px; height:100px;">
//  </div>
TEST_F(NGBlockLayoutAlgorithmTest, EmptyMulticol) {
  // parent
  RefPtr<ComputedStyle> parent_style = ComputedStyle::create();
  parent_style->setColumnCount(2);
  parent_style->setColumnFill(ColumnFillAuto);
  parent_style->setColumnGap(10);
  parent_style->setHeight(Length(100, Fixed));
  parent_style->setWidth(Length(210, Fixed));
  NGBlockNode* parent = new NGBlockNode(parent_style.get());

  auto* space = ConstructConstraintSpace(
      kHorizontalTopBottom, TextDirection::kLtr,
      NGLogicalSize(LayoutUnit(1000), NGSizeIndefinite));
  const auto* fragment = RunBlockLayoutAlgorithm(space, parent);
  FragmentChildIterator iterator(fragment);
  fragment = iterator.NextChild();
  ASSERT_TRUE(fragment);
  EXPECT_EQ(LayoutUnit(210), fragment->Width());
  EXPECT_EQ(LayoutUnit(100), fragment->Height());
  EXPECT_FALSE(iterator.NextChild());

  // There should be nothing inside the multicol container.
  EXPECT_FALSE(FragmentChildIterator(fragment).NextChild());
}

// Test case's HTML representation:
//  <div id="parent" style="columns:2; column-fill:auto; column-gap:10px;
//                          width:210px; height:100px;">
//    <div id="child"></div>
//  </div>
TEST_F(NGBlockLayoutAlgorithmTest, EmptyBlock) {
  // parent
  RefPtr<ComputedStyle> parent_style = ComputedStyle::create();
  parent_style->setColumnCount(2);
  parent_style->setColumnFill(ColumnFillAuto);
  parent_style->setColumnGap(10);
  parent_style->setHeight(Length(100, Fixed));
  parent_style->setWidth(Length(210, Fixed));
  NGBlockNode* parent = new NGBlockNode(parent_style.get());

  // child
  RefPtr<ComputedStyle> child_style = ComputedStyle::create();
  NGBlockNode* child = new NGBlockNode(child_style.get());

  parent->SetFirstChild(child);

  auto* space = ConstructConstraintSpace(
      kHorizontalTopBottom, TextDirection::kLtr,
      NGLogicalSize(LayoutUnit(1000), NGSizeIndefinite));
  const auto* fragment = RunBlockLayoutAlgorithm(space, parent);
  FragmentChildIterator iterator(fragment);
  fragment = iterator.NextChild();
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

// Test case's HTML representation:
//  <div id="parent" style="columns:2; column-fill:auto; column-gap:10px;
//                          width:310px; height:100px;">
//    <div id="child" style="width:60%; height:100px;"></div>
//  </div>
TEST_F(NGBlockLayoutAlgorithmTest, BlockInOneColumn) {
  // parent
  RefPtr<ComputedStyle> parent_style = ComputedStyle::create();
  parent_style->setColumnCount(2);
  parent_style->setColumnFill(ColumnFillAuto);
  parent_style->setColumnGap(10);
  parent_style->setHeight(Length(100, Fixed));
  parent_style->setWidth(Length(310, Fixed));
  NGBlockNode* parent = new NGBlockNode(parent_style.get());

  // child
  RefPtr<ComputedStyle> child_style = ComputedStyle::create();
  child_style->setWidth(Length(60, Percent));
  child_style->setHeight(Length(100, Fixed));
  NGBlockNode* child = new NGBlockNode(child_style.get());

  parent->SetFirstChild(child);

  auto* space = ConstructConstraintSpace(
      kHorizontalTopBottom, TextDirection::kLtr,
      NGLogicalSize(LayoutUnit(1000), NGSizeIndefinite));
  const auto* fragment = RunBlockLayoutAlgorithm(space, parent);

  FragmentChildIterator iterator(fragment);
  fragment = iterator.NextChild();
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

// Test case's HTML representation:
//  <div id="parent" style="columns:2; column-fill:auto; column-gap:10px;
//                          width:210px; height:100px;">
//    <div id="child" style="width:75%; height:150px;"></div>
//  </div>
TEST_F(NGBlockLayoutAlgorithmTest, BlockInTwoColumns) {
  // parent
  RefPtr<ComputedStyle> parent_style = ComputedStyle::create();
  parent_style->setColumnCount(2);
  parent_style->setColumnFill(ColumnFillAuto);
  parent_style->setColumnGap(10);
  parent_style->setHeight(Length(100, Fixed));
  parent_style->setWidth(Length(210, Fixed));
  NGBlockNode* parent = new NGBlockNode(parent_style.get());

  // child
  RefPtr<ComputedStyle> child_style = ComputedStyle::create();
  child_style->setWidth(Length(75, Percent));
  child_style->setHeight(Length(150, Fixed));
  NGBlockNode* child = new NGBlockNode(child_style.get());

  parent->SetFirstChild(child);

  auto* space = ConstructConstraintSpace(
      kHorizontalTopBottom, TextDirection::kLtr,
      NGLogicalSize(LayoutUnit(1000), NGSizeIndefinite));
  const auto* fragment = RunBlockLayoutAlgorithm(space, parent);

  FragmentChildIterator iterator(fragment);
  fragment = iterator.NextChild();
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

// Test case's HTML representation:
//  <div id="parent" style="columns:3; column-fill:auto; column-gap:10px;
//                          width:320px; height:100px;">
//    <div id="child" style="width:75%; height:250px;"></div>
//  </div>
TEST_F(NGBlockLayoutAlgorithmTest, BlockInThreeColumns) {
  // parent
  RefPtr<ComputedStyle> parent_style = ComputedStyle::create();
  parent_style->setColumnCount(3);
  parent_style->setColumnFill(ColumnFillAuto);
  parent_style->setColumnGap(10);
  parent_style->setHeight(Length(100, Fixed));
  parent_style->setWidth(Length(320, Fixed));
  NGBlockNode* parent = new NGBlockNode(parent_style.get());

  // child
  RefPtr<ComputedStyle> child_style = ComputedStyle::create();
  child_style->setWidth(Length(75, Percent));
  child_style->setHeight(Length(250, Fixed));
  NGBlockNode* child = new NGBlockNode(child_style.get());

  parent->SetFirstChild(child);

  auto* space = ConstructConstraintSpace(
      kHorizontalTopBottom, TextDirection::kLtr,
      NGLogicalSize(LayoutUnit(1000), NGSizeIndefinite));
  const auto* fragment = RunBlockLayoutAlgorithm(space, parent);

  FragmentChildIterator iterator(fragment);
  fragment = iterator.NextChild();
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

// Test case's HTML representation:
//  <div id="parent" style="columns:2; column-fill:auto; column-gap:10px;
//                          width:210px; height:100px;">
//    <div id="child" style="width:1px; height:250px;"></div>
//  </div>
TEST_F(NGBlockLayoutAlgorithmTest, ActualColumnCountGreaterThanSpecified) {
  // parent
  RefPtr<ComputedStyle> parent_style = ComputedStyle::create();
  parent_style->setColumnCount(2);
  parent_style->setColumnFill(ColumnFillAuto);
  parent_style->setColumnGap(10);
  parent_style->setHeight(Length(100, Fixed));
  parent_style->setWidth(Length(210, Fixed));
  NGBlockNode* parent = new NGBlockNode(parent_style.get());

  // child
  RefPtr<ComputedStyle> child_style = ComputedStyle::create();
  child_style->setWidth(Length(1, Fixed));
  child_style->setHeight(Length(250, Fixed));
  NGBlockNode* child = new NGBlockNode(child_style.get());

  parent->SetFirstChild(child);

  auto* space = ConstructConstraintSpace(
      kHorizontalTopBottom, TextDirection::kLtr,
      NGLogicalSize(LayoutUnit(1000), NGSizeIndefinite));
  const auto* fragment = RunBlockLayoutAlgorithm(space, parent);

  FragmentChildIterator iterator(fragment);
  fragment = iterator.NextChild();
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

// Test case's HTML representation:
//  <div id="parent" style="columns:3; column-fill:auto; column-gap:10px;
//                          width:320px; height:100px;">
//    <div id="child1" style="width:75%; height:60px;"></div>
//    <div id="child2" style="width:85%; height:60px;"></div>
//  </div>
TEST_F(NGBlockLayoutAlgorithmTest, TwoBlocksInTwoColumns) {
  // parent
  RefPtr<ComputedStyle> parent_style = ComputedStyle::create();
  parent_style->setColumnCount(3);
  parent_style->setColumnFill(ColumnFillAuto);
  parent_style->setColumnGap(10);
  parent_style->setHeight(Length(100, Fixed));
  parent_style->setWidth(Length(320, Fixed));
  NGBlockNode* parent = new NGBlockNode(parent_style.get());

  // child1
  RefPtr<ComputedStyle> child1_style = ComputedStyle::create();
  child1_style->setWidth(Length(75, Percent));
  child1_style->setHeight(Length(60, Fixed));
  NGBlockNode* child1 = new NGBlockNode(child1_style.get());

  // child2
  RefPtr<ComputedStyle> child2_style = ComputedStyle::create();
  child2_style->setWidth(Length(85, Percent));
  child2_style->setHeight(Length(60, Fixed));
  NGBlockNode* child2 = new NGBlockNode(child2_style.get());

  parent->SetFirstChild(child1);
  child1->SetNextSibling(child2);

  auto* space = ConstructConstraintSpace(
      kHorizontalTopBottom, TextDirection::kLtr,
      NGLogicalSize(LayoutUnit(1000), NGSizeIndefinite));
  const auto* fragment = RunBlockLayoutAlgorithm(space, parent);

  FragmentChildIterator iterator(fragment);
  fragment = iterator.NextChild();
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

// Test case's HTML representation:
//  <div id="parent" style="columns:3; column-fill:auto; column-gap:10px;
//                          width:320px; height:100px;">
//    <div id="child1" style="width:75%; height:60px;">
//      <div id="grandchild1" style="width:50px; height:120px;"></div>
//      <div id="grandchild2" style="width:40px; height:20px;"></div>
//    </div>
//    <div id="child2" style="width:85%; height:10px;"></div>
//  </div>
TEST_F(NGBlockLayoutAlgorithmTest, OverflowedBlock) {
  // parent
  RefPtr<ComputedStyle> parent_style = ComputedStyle::create();
  parent_style->setColumnCount(3);
  parent_style->setColumnFill(ColumnFillAuto);
  parent_style->setColumnGap(10);
  parent_style->setHeight(Length(100, Fixed));
  parent_style->setWidth(Length(320, Fixed));
  NGBlockNode* parent = new NGBlockNode(parent_style.get());

  // child1
  RefPtr<ComputedStyle> child1_style = ComputedStyle::create();
  child1_style->setWidth(Length(75, Percent));
  child1_style->setHeight(Length(60, Fixed));
  NGBlockNode* child1 = new NGBlockNode(child1_style.get());

  // grandchild1
  RefPtr<ComputedStyle> grandchild1_style = ComputedStyle::create();
  grandchild1_style->setWidth(Length(50, Fixed));
  grandchild1_style->setHeight(Length(120, Fixed));
  NGBlockNode* grandchild1 = new NGBlockNode(grandchild1_style.get());

  // grandchild2
  RefPtr<ComputedStyle> grandchild2_style = ComputedStyle::create();
  grandchild2_style->setWidth(Length(40, Fixed));
  grandchild2_style->setHeight(Length(20, Fixed));
  NGBlockNode* grandchild2 = new NGBlockNode(grandchild2_style.get());

  // child2
  RefPtr<ComputedStyle> child2_style = ComputedStyle::create();
  child2_style->setWidth(Length(85, Percent));
  child2_style->setHeight(Length(10, Fixed));
  NGBlockNode* child2 = new NGBlockNode(child2_style.get());

  parent->SetFirstChild(child1);
  child1->SetNextSibling(child2);
  child1->SetFirstChild(grandchild1);
  grandchild1->SetNextSibling(grandchild2);

  auto* space = ConstructConstraintSpace(
      kHorizontalTopBottom, TextDirection::kLtr,
      NGLogicalSize(LayoutUnit(1000), NGSizeIndefinite));
  const auto* fragment = RunBlockLayoutAlgorithm(space, parent);

  FragmentChildIterator iterator(fragment);
  fragment = iterator.NextChild();
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

// Test case's HTML representation:
//  <div id="parent" style="columns:3; column-fill:auto; column-gap:10px;
//                          width:320px; height:100px;">
//    <div id="child" style="float:left; width:75%; height:100px;"></div>
//  </div>
TEST_F(NGBlockLayoutAlgorithmTest, FloatInOneColumn) {
  // parent
  RefPtr<ComputedStyle> parent_style = ComputedStyle::create();
  parent_style->setColumnCount(3);
  parent_style->setColumnFill(ColumnFillAuto);
  parent_style->setColumnGap(10);
  parent_style->setHeight(Length(100, Fixed));
  parent_style->setWidth(Length(320, Fixed));
  NGBlockNode* parent = new NGBlockNode(parent_style.get());

  // child
  RefPtr<ComputedStyle> child_style = ComputedStyle::create();
  child_style->setFloating(EFloat::kLeft);
  child_style->setWidth(Length(75, Percent));
  child_style->setHeight(Length(100, Fixed));
  NGBlockNode* child = new NGBlockNode(child_style.get());

  parent->SetFirstChild(child);

  auto* space = ConstructConstraintSpace(
      kHorizontalTopBottom, TextDirection::kLtr,
      NGLogicalSize(LayoutUnit(1000), NGSizeIndefinite));
  const auto* fragment = RunBlockLayoutAlgorithm(space, parent);

  FragmentChildIterator iterator(fragment);
  fragment = iterator.NextChild();
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

// Test case's HTML representation:
//  <div id="parent" style="columns:3; column-fill:auto; column-gap:10px;
//                          width:320px; height:100px;">
//    <div id="child1" style="float:left; width:15%; height:100px;"></div>
//    <div id="child2" style="float:right; width:16%; height:100px;"></div>
//  </div>
TEST_F(NGBlockLayoutAlgorithmTest, TwoFloatsInOneColumn) {
  // parent
  RefPtr<ComputedStyle> parent_style = ComputedStyle::create();
  parent_style->setColumnCount(3);
  parent_style->setColumnFill(ColumnFillAuto);
  parent_style->setColumnGap(10);
  parent_style->setHeight(Length(100, Fixed));
  parent_style->setWidth(Length(320, Fixed));
  NGBlockNode* parent = new NGBlockNode(parent_style.get());

  // child1
  RefPtr<ComputedStyle> child1_style = ComputedStyle::create();
  child1_style->setFloating(EFloat::kLeft);
  child1_style->setWidth(Length(15, Percent));
  child1_style->setHeight(Length(100, Fixed));
  NGBlockNode* child1 = new NGBlockNode(child1_style.get());

  // child2
  RefPtr<ComputedStyle> child2_style = ComputedStyle::create();
  child2_style->setFloating(EFloat::kRight);
  child2_style->setWidth(Length(16, Percent));
  child2_style->setHeight(Length(100, Fixed));
  NGBlockNode* child2 = new NGBlockNode(child2_style.get());

  parent->SetFirstChild(child1);
  child1->SetNextSibling(child2);

  auto* space = ConstructConstraintSpace(
      kHorizontalTopBottom, TextDirection::kLtr,
      NGLogicalSize(LayoutUnit(1000), NGSizeIndefinite));
  const auto* fragment = RunBlockLayoutAlgorithm(space, parent);

  FragmentChildIterator iterator(fragment);
  fragment = iterator.NextChild();
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

// Test case's HTML representation:
//  <div id="parent" style="columns:3; column-fill:auto; column-gap:10px;
//                          width:320px; height:100px;">
//    <div id="child1" style="float:left; width:15%; height:150px;"></div>
//    <div id="child2" style="float:right; width:16%; height:150px;"></div>
//  </div>
TEST_F(NGBlockLayoutAlgorithmTest, TwoFloatsInTwoColumns) {
  // parent
  RefPtr<ComputedStyle> parent_style = ComputedStyle::create();
  parent_style->setColumnCount(3);
  parent_style->setColumnFill(ColumnFillAuto);
  parent_style->setColumnGap(10);
  parent_style->setHeight(Length(100, Fixed));
  parent_style->setWidth(Length(320, Fixed));
  NGBlockNode* parent = new NGBlockNode(parent_style.get());

  // child1
  RefPtr<ComputedStyle> child1_style = ComputedStyle::create();
  child1_style->setFloating(EFloat::kLeft);
  child1_style->setWidth(Length(15, Percent));
  child1_style->setHeight(Length(150, Fixed));
  NGBlockNode* child1 = new NGBlockNode(child1_style.get());

  // child2
  RefPtr<ComputedStyle> child2_style = ComputedStyle::create();
  child2_style->setFloating(EFloat::kRight);
  child2_style->setWidth(Length(16, Percent));
  child2_style->setHeight(Length(150, Fixed));
  NGBlockNode* child2 = new NGBlockNode(child2_style.get());

  parent->SetFirstChild(child1);
  child1->SetNextSibling(child2);

  auto* space = ConstructConstraintSpace(
      kHorizontalTopBottom, TextDirection::kLtr,
      NGLogicalSize(LayoutUnit(1000), NGSizeIndefinite));
  const auto* fragment = RunBlockLayoutAlgorithm(space, parent);

  FragmentChildIterator iterator(fragment);
  fragment = iterator.NextChild();
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

}  // namespace
}  // namespace blink
