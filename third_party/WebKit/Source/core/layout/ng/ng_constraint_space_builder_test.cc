// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_constraint_space_builder.h"

#include "core/layout/LayoutTestHelper.h"

namespace blink {
namespace {

class NGConstraintSpaceBuilderTest {
 public:
  NGConstraintSpaceBuilderTest() {
    RuntimeEnabledFeatures::SetLayoutNGEnabled(true);
  };
  ~NGConstraintSpaceBuilderTest() {
    RuntimeEnabledFeatures::SetLayoutNGEnabled(false);
  };
};

// Asserts that indefinite inline length becomes initial containing
// block width for horizontal-tb inside vertical document.
TEST(NGConstraintSpaceBuilderTest, AvailableSizeFromHorizontalICB) {
  NGConstraintSpaceBuilder horizontal_builder(kHorizontalTopBottom);
  NGLogicalSize fixed_size{LayoutUnit(100), LayoutUnit(200)};
  NGLogicalSize indefinite_size{NGSizeIndefinite, NGSizeIndefinite};
  NGPhysicalSize icb_size{NGSizeIndefinite, LayoutUnit(51)};

  horizontal_builder.SetInitialContainingBlockSize(icb_size);
  horizontal_builder.SetAvailableSize(fixed_size);
  horizontal_builder.SetPercentageResolutionSize(fixed_size);

  NGConstraintSpaceBuilder vertical_builder(
      horizontal_builder.ToConstraintSpace(kHorizontalTopBottom).Get());

  vertical_builder.SetAvailableSize(indefinite_size);
  vertical_builder.SetPercentageResolutionSize(indefinite_size);
  RefPtr<NGConstraintSpace> space =
      vertical_builder.ToConstraintSpace(kVerticalLeftRight);

  EXPECT_EQ(space->AvailableSize().inline_size, icb_size.height);
  EXPECT_EQ(space->PercentageResolutionSize().inline_size, icb_size.height);
};

// Asserts that indefinite inline length becomes initial containing
// block height for vertical-lr inside horizontal document.
TEST(NGConstraintSpaceBuilderTest, AvailableSizeFromVerticalICB) {
  NGConstraintSpaceBuilder horizontal_builder(kVerticalLeftRight);
  NGLogicalSize fixed_size{LayoutUnit(100), LayoutUnit(200)};
  NGLogicalSize indefinite_size{NGSizeIndefinite, NGSizeIndefinite};
  NGPhysicalSize icb_size{LayoutUnit(51), NGSizeIndefinite};

  horizontal_builder.SetInitialContainingBlockSize(icb_size);
  horizontal_builder.SetAvailableSize(fixed_size);
  horizontal_builder.SetPercentageResolutionSize(fixed_size);

  NGConstraintSpaceBuilder vertical_builder(
      horizontal_builder.ToConstraintSpace(kVerticalLeftRight).Get());

  vertical_builder.SetAvailableSize(indefinite_size);
  vertical_builder.SetPercentageResolutionSize(indefinite_size);
  RefPtr<NGConstraintSpace> space =
      vertical_builder.ToConstraintSpace(kHorizontalTopBottom);

  EXPECT_EQ(space->AvailableSize().inline_size, icb_size.width);
  EXPECT_EQ(space->PercentageResolutionSize().inline_size, icb_size.width);
};

}  // namespace

}  // namespace blink
