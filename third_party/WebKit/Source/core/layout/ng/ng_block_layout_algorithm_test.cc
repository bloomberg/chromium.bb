// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_block_layout_algorithm.h"

#include "core/layout/ng/ng_box.h"
#include "core/layout/ng/ng_constraint_space.h"
#include "core/layout/ng/ng_physical_fragment.h"
#include "core/layout/ng/ng_length_utils.h"
#include "core/layout/ng/ng_units.h"
#include "core/style/ComputedStyle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace {

class NGBlockLayoutAlgorithmTest : public ::testing::Test {
 protected:
  void SetUp() override { style_ = ComputedStyle::create(); }

  NGPhysicalFragment* RunBlockLayoutAlgorithm(const NGConstraintSpace* space,
                                              NGBox* first_child) {
    NGBlockLayoutAlgorithm algorithm(style_, first_child);
    NGPhysicalFragment* frag;
    while (!algorithm.Layout(space, &frag))
      continue;
    return frag;
  }

  RefPtr<ComputedStyle> style_;
};

TEST_F(NGBlockLayoutAlgorithmTest, FixedSize) {
  style_->setWidth(Length(30, Fixed));
  style_->setHeight(Length(40, Fixed));

  auto* space = new NGConstraintSpace(
      HorizontalTopBottom, NGLogicalSize(LayoutUnit(100), NGSizeIndefinite));
  NGPhysicalFragment* frag = RunBlockLayoutAlgorithm(space, nullptr);

  EXPECT_EQ(frag->Width(), LayoutUnit(30));
  EXPECT_EQ(frag->Height(), LayoutUnit(40));
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
  NGBox* first_child = new NGBox(first_style.get());

  RefPtr<ComputedStyle> second_style = ComputedStyle::create();
  second_style->setHeight(Length(kHeight2, Fixed));
  second_style->setMarginTop(Length(kMarginTop, Fixed));
  second_style->setMarginBottom(Length(kMarginBottom, Fixed));
  NGBox* second_child = new NGBox(second_style.get());

  first_child->SetNextSibling(second_child);

  auto* space = new NGConstraintSpace(
      HorizontalTopBottom, NGLogicalSize(LayoutUnit(100), NGSizeIndefinite));
  NGPhysicalFragment* frag = RunBlockLayoutAlgorithm(space, first_child);

  EXPECT_EQ(frag->Width(), LayoutUnit(kWidth));
  EXPECT_EQ(frag->Height(), LayoutUnit(kHeight1 + kHeight2 + kMarginTop));
  EXPECT_EQ(frag->Type(), NGPhysicalFragmentBase::FragmentBox);
  ASSERT_EQ(frag->Children().size(), 2UL);

  const NGPhysicalFragmentBase* child = frag->Children()[0];
  EXPECT_EQ(child->Height(), kHeight1);
  EXPECT_EQ(child->TopOffset(), 0);

  child = frag->Children()[1];
  EXPECT_EQ(child->Height(), kHeight2);
  EXPECT_EQ(child->TopOffset(), kHeight1 + kMarginTop);
}

// Verifies the collapsing margins case for the next pair:
// - top margin of a box and top margin of its first in-flow child.
//
// Test case's HTML representation:
// <div style="margin-top: 20px; height: 50px;">  <!-- DIV1 -->
//    <div style="margin-top: 10px"></div>        <!-- DIV2 -->
// </div>
//
// Expected:
//   Margins are collapsed resulting a single margin 20px = max(20px, 10px)
TEST_F(NGBlockLayoutAlgorithmTest, CollapsingMarginsCase1) {
  const int kHeight = 50;
  const int kDiv1MarginTop = 20;
  const int kDiv2MarginTop = 10;

  // DIV1
  RefPtr<ComputedStyle> div1_style = ComputedStyle::create();
  div1_style->setHeight(Length(kHeight, Fixed));
  div1_style->setMarginTop(Length(kDiv1MarginTop, Fixed));
  NGBox* div1 = new NGBox(div1_style.get());

  // DIV2
  RefPtr<ComputedStyle> div2_style = ComputedStyle::create();
  div2_style->setMarginTop(Length(kDiv2MarginTop, Fixed));
  NGBox* div2 = new NGBox(div2_style.get());

  div1->SetFirstChild(div2);

  auto* space = new NGConstraintSpace(
      HorizontalTopBottom, NGLogicalSize(LayoutUnit(100), NGSizeIndefinite));
  NGPhysicalFragment* frag = RunBlockLayoutAlgorithm(space, div1);

  EXPECT_EQ(frag->MarginStrut(), NGMarginStrut({LayoutUnit(kDiv1MarginTop)}));
  ASSERT_EQ(frag->Children().size(), 1UL);
  const NGPhysicalFragmentBase* div2_fragment = frag->Children()[0];
  EXPECT_EQ(div2_fragment->MarginStrut(),
            NGMarginStrut({LayoutUnit(kDiv2MarginTop)}));
}

// Verifies the collapsing margins case for the next pair:
// - bottom margin of box and top margin of its next in-flow following sibling.
//
// Test case's HTML representation:
// <div style="margin-bottom: 20px; height: 50px;">  <!-- DIV1 -->
//    <div style="margin-bottom: -15px"></div>       <!-- DIV2 -->
// </div>
// <div style="margin-top: 10px; height: 50px;">     <!-- DIV3 -->
//    <div style="margin-top: -30px"></div>          <!-- DIV4 -->
// </div>
//
// Expected:
//   Margins are collapsed resulting an overlap
//   -10px = max(20px, 10px) - max(abs(-15px), abs(-30px))
//   between DIV2 and DIV3.
TEST_F(NGBlockLayoutAlgorithmTest, CollapsingMarginsCase2) {
  const int kHeight = 50;
  const int kDiv1MarginBottom = 20;
  const int kDiv2MarginBottom = -15;
  const int kDiv3MarginTop = 10;
  const int kDiv4MarginTop = -30;
  const int kExpectedCollapsedMargin = -10;

  // DIV1
  RefPtr<ComputedStyle> div1_style = ComputedStyle::create();
  div1_style->setHeight(Length(kHeight, Fixed));
  div1_style->setMarginBottom(Length(kDiv1MarginBottom, Fixed));
  NGBox* div1 = new NGBox(div1_style.get());

  // DIV2
  RefPtr<ComputedStyle> div2_style = ComputedStyle::create();
  div2_style->setMarginBottom(Length(kDiv2MarginBottom, Fixed));
  NGBox* div2 = new NGBox(div2_style.get());

  // DIV3
  RefPtr<ComputedStyle> div3_style = ComputedStyle::create();
  div3_style->setHeight(Length(kHeight, Fixed));
  div3_style->setMarginTop(Length(kDiv3MarginTop, Fixed));
  NGBox* div3 = new NGBox(div3_style.get());

  // DIV4
  RefPtr<ComputedStyle> div4_style = ComputedStyle::create();
  div4_style->setMarginTop(Length(kDiv4MarginTop, Fixed));
  NGBox* div4 = new NGBox(div4_style.get());

  div1->SetFirstChild(div2);
  div3->SetFirstChild(div4);
  div1->SetNextSibling(div3);

  auto* space = new NGConstraintSpace(
      HorizontalTopBottom, NGLogicalSize(LayoutUnit(100), NGSizeIndefinite));
  NGPhysicalFragment* frag = RunBlockLayoutAlgorithm(space, div1);

  ASSERT_EQ(frag->Children().size(), 2UL);

  const NGPhysicalFragmentBase* child = frag->Children()[0];
  EXPECT_EQ(child->Height(), kHeight);
  EXPECT_EQ(child->TopOffset(), 0);

  child = frag->Children()[1];
  EXPECT_EQ(child->Height(), kHeight);
  EXPECT_EQ(child->TopOffset(), kHeight + kExpectedCollapsedMargin);
}

// Verifies the collapsing margins case for the next pair:
// - bottom margin of a last in-flow child and bottom margin of its parent if
//   the parent has 'auto' computed height
//
// Test case's HTML representation:
// <div style="margin-bottom: 20px; height: 50px;">            <!-- DIV1 -->
//   <div style="margin-bottom: 200px; height: 50px;"/>        <!-- DIV2 -->
// </div>
//
// Expected:
//   1) Margins are collapsed with the result = std::max(20, 200)
//      if DIV1.height == auto
//   2) Margins are NOT collapsed if DIV1.height != auto
TEST_F(NGBlockLayoutAlgorithmTest, CollapsingMarginsCase3) {
  const int kHeight = 50;
  const int kDiv1MarginBottom = 20;
  const int kDiv2MarginBottom = 200;

  // DIV1
  RefPtr<ComputedStyle> div1_style = ComputedStyle::create();
  div1_style->setMarginBottom(Length(kDiv1MarginBottom, Fixed));
  NGBox* div1 = new NGBox(div1_style.get());

  // DIV2
  RefPtr<ComputedStyle> div2_style = ComputedStyle::create();
  div2_style->setHeight(Length(kHeight, Fixed));
  div2_style->setMarginBottom(Length(kDiv2MarginBottom, Fixed));
  NGBox* div2 = new NGBox(div2_style.get());

  div1->SetFirstChild(div2);

  auto* space = new NGConstraintSpace(
      HorizontalTopBottom, NGLogicalSize(LayoutUnit(100), NGSizeIndefinite));
  NGPhysicalFragment* frag = RunBlockLayoutAlgorithm(space, div1);

  // Verify that margins are collapsed.
  EXPECT_EQ(frag->MarginStrut(),
            NGMarginStrut({LayoutUnit(0), LayoutUnit(kDiv2MarginBottom)}));

  // Verify that margins are NOT collapsed.
  div1_style->setHeight(Length(kHeight, Fixed));
  frag = RunBlockLayoutAlgorithm(space, div1);
  EXPECT_EQ(frag->MarginStrut(),
            NGMarginStrut({LayoutUnit(0), LayoutUnit(kDiv1MarginBottom)}));
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
  NGBox* div1 = new NGBox(div1_style.get());

  RefPtr<ComputedStyle> div2_style = ComputedStyle::create();
  NGBox* div2 = new NGBox(div2_style.get());

  div1->SetFirstChild(div2);

  auto* space = new NGConstraintSpace(
      HorizontalTopBottom, NGLogicalSize(LayoutUnit(1000), NGSizeIndefinite));
  NGPhysicalFragment* frag = RunBlockLayoutAlgorithm(space, div1);

  ASSERT_EQ(frag->Children().size(), 1UL);

  // div1
  const NGPhysicalFragmentBase* child = frag->Children()[0];
  EXPECT_EQ(kBorderLeft + kPaddingLeft + kWidth + kPaddingRight + kBorderRight,
            child->Width());
  EXPECT_EQ(kBorderTop + kPaddingTop + kHeight + kPaddingBottom + kBorderBottom,
            child->Height());

  ASSERT_TRUE(child->Type() == NGPhysicalFragmentBase::FragmentBox);
  ASSERT_EQ(static_cast<const NGPhysicalFragment*>(child)->Children().size(),
            1UL);

  // div2
  child = static_cast<const NGPhysicalFragment*>(child)->Children()[0];
  EXPECT_EQ(kBorderTop + kPaddingTop, child->TopOffset());
  EXPECT_EQ(kBorderLeft + kPaddingLeft, child->LeftOffset());
}
}  // namespace
}  // namespace blink
