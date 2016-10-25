// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_constraint_space.h"

#include "core/layout/ng/ng_constraint_space.h"
#include "core/layout/ng/ng_layout_opportunity_iterator.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

TEST(NGConstraintSpaceTest, WritingMode) {
  NGConstraintSpace* horz_space =
      new NGConstraintSpace(HorizontalTopBottom, LeftToRight,
                            NGLogicalSize(LayoutUnit(200), LayoutUnit(100)));
  horz_space->SetOverflowTriggersScrollbar(true, false);
  horz_space->SetFixedSize(true, false);
  horz_space->SetFragmentationType(FragmentColumn);

  NGConstraintSpace* vert_space = new NGConstraintSpace(
      VerticalRightLeft, LeftToRight, horz_space->MutablePhysicalSpace());

  EXPECT_EQ(LayoutUnit(200), horz_space->ContainerSize().inline_size);
  EXPECT_EQ(LayoutUnit(200), vert_space->ContainerSize().block_size);

  EXPECT_EQ(LayoutUnit(100), horz_space->ContainerSize().block_size);
  EXPECT_EQ(LayoutUnit(100), vert_space->ContainerSize().inline_size);

  EXPECT_TRUE(horz_space->InlineTriggersScrollbar());
  EXPECT_TRUE(vert_space->BlockTriggersScrollbar());

  EXPECT_FALSE(horz_space->BlockTriggersScrollbar());
  EXPECT_FALSE(vert_space->InlineTriggersScrollbar());

  EXPECT_TRUE(horz_space->FixedInlineSize());
  EXPECT_TRUE(vert_space->FixedBlockSize());

  EXPECT_FALSE(horz_space->FixedBlockSize());
  EXPECT_FALSE(vert_space->FixedInlineSize());

  EXPECT_EQ(FragmentColumn, horz_space->BlockFragmentationType());
  EXPECT_EQ(FragmentNone, vert_space->BlockFragmentationType());
}

static String OpportunityToString(const NGLayoutOpportunity& opportunity) {
  return opportunity.IsEmpty() ? String("(empty)") : opportunity.ToString();
}

TEST(NGConstraintSpaceTest, LayoutOpportunitiesNoExclusions) {
  NGPhysicalSize physical_size;
  physical_size.width = LayoutUnit(600);
  physical_size.height = LayoutUnit(400);
  auto* physical_space = new NGPhysicalConstraintSpace(physical_size);
  auto* space =
      new NGConstraintSpace(HorizontalTopBottom, LeftToRight, physical_space);

  bool for_inline_or_bfc = true;
  auto* iterator = space->LayoutOpportunities(NGClearNone, for_inline_or_bfc);

  EXPECT_EQ("0,0 600x400", OpportunityToString(iterator->Next()));
  EXPECT_EQ("(empty)", OpportunityToString(iterator->Next()));
}

TEST(NGConstraintSpaceTest, LayoutOpportunitiesTopRightExclusion) {
  NGPhysicalSize physical_size;
  physical_size.width = LayoutUnit(600);
  physical_size.height = LayoutUnit(400);
  auto* physical_space = new NGPhysicalConstraintSpace(physical_size);

  // Add a 100x100 exclusion in the top right corner.
  physical_space->AddExclusion(new NGExclusion(
      LayoutUnit(0), LayoutUnit(600), LayoutUnit(100), LayoutUnit(500)));

  auto* space =
      new NGConstraintSpace(HorizontalTopBottom, LeftToRight, physical_space);
  bool for_inline_or_bfc = true;
  auto* iterator = space->LayoutOpportunities(NGClearNone, for_inline_or_bfc);

  // First opportunity should be to the left of the exclusion.
  EXPECT_EQ("0,0 500x400", OpportunityToString(iterator->Next()));

  // Second opportunity should be below the exclusion.
  EXPECT_EQ("0,100 600x300", OpportunityToString(iterator->Next()));

  // There should be no third opportunity.
  EXPECT_EQ("(empty)", OpportunityToString(iterator->Next()));
}

TEST(NGConstraintSpaceTest, LayoutOpportunitiesTopLeftExclusion) {
  NGPhysicalSize physical_size;
  physical_size.width = LayoutUnit(600);
  physical_size.height = LayoutUnit(400);
  auto* physical_space = new NGPhysicalConstraintSpace(physical_size);

  // Add a 100x100 exclusion in the top left corner.
  physical_space->AddExclusion(new NGExclusion(LayoutUnit(0), LayoutUnit(100),
                                               LayoutUnit(100), LayoutUnit(0)));

  auto* space =
      new NGConstraintSpace(HorizontalTopBottom, LeftToRight, physical_space);
  bool for_inline_or_bfc = true;
  auto* iterator = space->LayoutOpportunities(NGClearNone, for_inline_or_bfc);

  // First opportunity should be to the right of the exclusion.
  EXPECT_EQ("100,0 500x400", OpportunityToString(iterator->Next()));

  // Second opportunity should be below the exclusion.
  EXPECT_EQ("0,100 600x300", OpportunityToString(iterator->Next()));

  // There should be no third opportunity.
  EXPECT_EQ("(empty)", OpportunityToString(iterator->Next()));
}

// Verifies that Layout Opportunity iterator produces 7 layout opportunities
// from 4 start points created by 2 CSS exclusions positioned in the middle of
// the main constraint space.
//
// Test case visual representation:
//
//         100  200  300  400  500
//     (1)--|----|----|-(2)|----|-(3)+
//  50 |                             |
// 100 |                             |
// 150 |                             |
// 200 |       **********            |
// 250 |       **********            |
// 300 (4)                           |
// 350 |                        ***  |
//     +-----------------------------+
//
// Expected:
//   Layout opportunity iterator generates the next opportunities:
//   - 1st Start Point: 0,0 600x200; 0,0 150x400
//   - 2nd Start Point: 250,0 350x350; 250,0 250x400
//   - 3rd Start Point: 550,0 50x400
//   - 4th Start Point: 0,300 600x50; 0,300 500x100
TEST(NGConstraintSpaceTest, LayoutOpportunitiesTwoInMiddle) {
  NGPhysicalSize physical_size;
  physical_size.width = LayoutUnit(600);
  physical_size.height = LayoutUnit(400);
  auto* physical_space = new NGPhysicalConstraintSpace(physical_size);

  // Add exclusions
  physical_space->AddExclusion(new NGExclusion(
      LayoutUnit(200), LayoutUnit(250), LayoutUnit(300), LayoutUnit(150)));
  physical_space->AddExclusion(new NGExclusion(
      LayoutUnit(350), LayoutUnit(550), LayoutUnit(400), LayoutUnit(500)));

  auto* space =
      new NGConstraintSpace(HorizontalTopBottom, LeftToRight, physical_space);
  bool for_inline_or_bfc = true;
  auto* iterator = space->LayoutOpportunities(NGClearNone, for_inline_or_bfc);

  // 1st Start point
  EXPECT_EQ("0,0 600x200", OpportunityToString(iterator->Next()));
  EXPECT_EQ("0,0 150x400", OpportunityToString(iterator->Next()));

  // 2nd Start point
  EXPECT_EQ("250,0 350x350", OpportunityToString(iterator->Next()));
  EXPECT_EQ("250,0 250x400", OpportunityToString(iterator->Next()));

  // 3rd Start point
  EXPECT_EQ("550,0 50x400", OpportunityToString(iterator->Next()));

  // 4th Start point
  EXPECT_EQ("0,300 600x50", OpportunityToString(iterator->Next()));
  EXPECT_EQ("0,300 500x100", OpportunityToString(iterator->Next()));

  // Iterator is exhausted.
  EXPECT_EQ("(empty)", OpportunityToString(iterator->Next()));
}

// Verifies that Layout Opportunity iterator ignores the exclusion that is not
// within constraint space.
//
// Test case visual representation:
//
//         100  200  300  400  500
//     +----|----|----|----|----|----+
//  50 |                             |
// 100 |                             |
//     +-----------------------------+
//      ***  <- Exclusion
//
// Expected:
//   Layout opportunity iterator generates only one opportunity that equals to
//   available constraint space, i.e. 0,0 600x200
TEST(NGConstraintSpaceTest, LayoutOpportunitiesWithOutOfBoundsExclusions) {
  NGPhysicalSize physical_size;
  physical_size.width = LayoutUnit(600);
  physical_size.height = LayoutUnit(100);

  auto* physical_space = new NGPhysicalConstraintSpace(physical_size);
  physical_space->AddExclusion(new NGExclusion(LayoutUnit(150), LayoutUnit(100),
                                               LayoutUnit(200), LayoutUnit(0)));
  auto* space =
      new NGConstraintSpace(HorizontalTopBottom, LeftToRight, physical_space);

  bool for_inline_or_bfc = true;
  auto* iterator = space->LayoutOpportunities(NGClearNone, for_inline_or_bfc);

  EXPECT_EQ("0,0 600x100", OpportunityToString(iterator->Next()));
  EXPECT_EQ("(empty)", OpportunityToString(iterator->Next()));
}

}  // namespace
}  // namespace blink
