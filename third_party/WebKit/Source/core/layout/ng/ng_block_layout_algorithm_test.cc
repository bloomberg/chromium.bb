// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_block_layout_algorithm.h"

#include "core/layout/ng/ng_box_iterator.h"
#include "core/layout/ng/ng_constraint_space.h"
#include "core/layout/ng/ng_fragment.h"
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

  NGBlockLayoutAlgorithm algorithm(style_, NGBoxIterator(NGBox()));
  NGFragment* frag;
  while (!algorithm.Layout(space, &frag))
    ;
  EXPECT_EQ(frag->InlineSize(), LayoutUnit(30));
  EXPECT_EQ(frag->BlockSize(), LayoutUnit(40));
}

}  // namespace
}  // namespace blink
