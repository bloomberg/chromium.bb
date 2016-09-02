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
  EXPECT_EQ(frag->Height(),
            LayoutUnit(kHeight1 + kHeight2 + kMarginTop + kMarginBottom));
  EXPECT_EQ(frag->Type(), NGPhysicalFragmentBase::FragmentBox);
  ASSERT_EQ(frag->Children().size(), 2UL);

  const NGPhysicalFragmentBase* child = frag->Children()[0];
  EXPECT_EQ(child->Height(), kHeight1);
  EXPECT_EQ(child->TopOffset(), 0);

  child = frag->Children()[1];
  EXPECT_EQ(child->Height(), kHeight2);
  EXPECT_EQ(child->TopOffset(), kHeight1 + kMarginTop);
}

}  // namespace
}  // namespace blink
