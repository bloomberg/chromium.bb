// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_block_layout_algorithm.h"

#include "core/layout/ng/ng_box.h"
#include "core/layout/ng/ng_constraint_space.h"
#include "core/layout/ng/ng_physical_fragment.h"
#include "core/layout/ng/ng_length_utils.h"
#include "core/style/ComputedStyle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace {

class NGBlockLayoutAlgorithmTest : public ::testing::Test {
 protected:
  void SetUp() override { style_ = ComputedStyle::create(); }

  RefPtr<ComputedStyle> style_;
};

TEST_F(NGBlockLayoutAlgorithmTest, FixedSize) {
  style_->setWidth(Length(30, Fixed));
  style_->setHeight(Length(40, Fixed));

  NGConstraintSpace* space = new NGConstraintSpace(
      HorizontalTopBottom, NGLogicalSize(LayoutUnit(100), NGSizeIndefinite));

  NGBlockLayoutAlgorithm algorithm(style_, nullptr);
  NGPhysicalFragment* frag;
  while (!algorithm.Layout(space, &frag))
    ;
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

  NGConstraintSpace* space = new NGConstraintSpace(
      HorizontalTopBottom, NGLogicalSize(LayoutUnit(100), NGSizeIndefinite));

  RefPtr<ComputedStyle> first_style = ComputedStyle::create();
  first_style->setHeight(Length(kHeight1, Fixed));
  NGBox* first_child = new NGBox(first_style.get());

  RefPtr<ComputedStyle> second_style = ComputedStyle::create();
  second_style->setHeight(Length(kHeight2, Fixed));
  second_style->setMarginTop(Length(kMarginTop, Fixed));
  second_style->setMarginBottom(Length(kMarginBottom, Fixed));
  NGBox* second_child = new NGBox(second_style.get());

  first_child->SetNextSibling(second_child);

  NGBlockLayoutAlgorithm algorithm(style_, first_child);
  NGPhysicalFragment* frag;
  while (!algorithm.Layout(space, &frag))
    ;
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

  NGConstraintSpace* space = new NGConstraintSpace(
      HorizontalTopBottom, NGLogicalSize(LayoutUnit(100), NGSizeIndefinite));
  NGBlockLayoutAlgorithm algorithm(style_, div1);
  NGPhysicalFragment* frag;
  while (!algorithm.Layout(space, &frag))
    ;

  EXPECT_EQ(frag->MarginStrut().margin_block_start, kDiv1MarginTop);
  ASSERT_EQ(frag->Children().size(), 1UL);
  const NGPhysicalFragmentBase* div2_fragment = frag->Children()[0];
  EXPECT_EQ(div2_fragment->MarginStrut().margin_block_start, kDiv2MarginTop);
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

  NGConstraintSpace* space = new NGConstraintSpace(
      HorizontalTopBottom, NGLogicalSize(LayoutUnit(100), NGSizeIndefinite));
  NGBlockLayoutAlgorithm algorithm(style_, div1);
  NGPhysicalFragment* frag;
  while (!algorithm.Layout(space, &frag))
    ;

  ASSERT_EQ(frag->Children().size(), 2UL);

  const NGPhysicalFragmentBase* child = frag->Children()[0];
  EXPECT_EQ(child->Height(), kHeight);
  EXPECT_EQ(child->TopOffset(), 0);

  child = frag->Children()[1];
  EXPECT_EQ(child->Height(), kHeight);
  EXPECT_EQ(child->TopOffset(), kHeight + kExpectedCollapsedMargin);
}
}  // namespace
}  // namespace blink
