// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

  NGConstraintSpace* vert_space =
      new NGConstraintSpace(VerticalRightLeft, LeftToRight, horz_space);

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

static String OpportunityToString(const NGConstraintSpace* opportunity) {
  return opportunity ? opportunity->ToString() : String("(null)");
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
  EXPECT_EQ("(null)", OpportunityToString(iterator->Next()));
}

TEST(NGConstraintSpaceTest, LayoutOpportunitiesTopRightExclusion) {
  NGPhysicalSize physical_size;
  physical_size.width = LayoutUnit(600);
  physical_size.height = LayoutUnit(400);
  auto* physical_space = new NGPhysicalConstraintSpace(physical_size);

  // Add a 100x100 exclusion in the top right corner.
  physical_space->AddExclusion(NGExclusion(LayoutUnit(0), LayoutUnit(600),
                                           LayoutUnit(100), LayoutUnit(500)));

  auto* space =
      new NGConstraintSpace(HorizontalTopBottom, LeftToRight, physical_space);
  bool for_inline_or_bfc = true;
  auto* iterator = space->LayoutOpportunities(NGClearNone, for_inline_or_bfc);

  // First opportunity should be to the left of the exclusion.
  EXPECT_EQ("0,0 500x400", OpportunityToString(iterator->Next()));

  // Second opportunity should be below the exclusion.
  EXPECT_EQ("0,100 600x300", OpportunityToString(iterator->Next()));

  // There should be no third opportunity.
  EXPECT_EQ("(null)", OpportunityToString(iterator->Next()));
}

TEST(NGConstraintSpaceTest, LayoutOpportunitiesTopLeftExclusion) {
  NGPhysicalSize physical_size;
  physical_size.width = LayoutUnit(600);
  physical_size.height = LayoutUnit(400);
  auto* physical_space = new NGPhysicalConstraintSpace(physical_size);

  // Add a 100x100 exclusion in the top left corner.
  physical_space->AddExclusion(NGExclusion(LayoutUnit(0), LayoutUnit(100),
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
  EXPECT_EQ("(null)", OpportunityToString(iterator->Next()));
}

//         100  200  300  400  500
//     +----|----|----|----|----|----+
//  50 |                             |
// 100 |                             |
// 150 |                             |
// 200 |       **********            |
// 250 |       **********            |
// 300 |                             |
// 350 |                        ***  |
//     +-----------------------------+
TEST(NGConstraintSpaceTest, LayoutOpportunitiesTwoInMiddle) {
  NGPhysicalSize physical_size;
  physical_size.width = LayoutUnit(600);
  physical_size.height = LayoutUnit(400);
  auto* physical_space = new NGPhysicalConstraintSpace(physical_size);

  // Add a 200x100 exclusion at 150x200
  physical_space->AddExclusion(NGExclusion(LayoutUnit(200), LayoutUnit(250),
                                           LayoutUnit(300), LayoutUnit(150)));
  // Add a 50x50 exclusion at 500x350
  physical_space->AddExclusion(NGExclusion(LayoutUnit(350), LayoutUnit(550),
                                           LayoutUnit(400), LayoutUnit(500)));

  auto* space =
      new NGConstraintSpace(HorizontalTopBottom, LeftToRight, physical_space);
  bool for_inline_or_bfc = true;
  auto* iterator = space->LayoutOpportunities(NGClearNone, for_inline_or_bfc);

  // First opportunity should be above the first exclusion.
  EXPECT_EQ("0,0 600x200", OpportunityToString(iterator->Next()));

  // Second opportunity should be full height to the left.
  EXPECT_EQ("0,0 150x400", OpportunityToString(iterator->Next()));

  // Third opportunity should be to the left of the first exclusion. This is a
  // subset of the second opportunity but has a different location and might
  // have a different alignment.
  EXPECT_EQ("0,200 150x200", OpportunityToString(iterator->Next()));

  // Fourth opportunity should be to the right of the first exclusion extending
  // down until the top of the second exclusion.
  EXPECT_EQ("250,200 350x150", OpportunityToString(iterator->Next()));

  // Fifth opportunity should be to the right of the first exclusion until the
  // left edge of the second exclusion and extending all the way down.
  EXPECT_EQ("250,200 250x200", OpportunityToString(iterator->Next()));

  // Sixth opportunity should be below first exclusion with full width.
  EXPECT_EQ("0,300 600x50", OpportunityToString(iterator->Next()));

  // Seventh opportunity should be below first exclusion until the left edge of
  // the second exclusion extending all the way down.
  EXPECT_EQ("0,300 500x100", OpportunityToString(iterator->Next()));

  // Eight exclusion should be to the left of the last exclusion.
  EXPECT_EQ("0,350 500x50", OpportunityToString(iterator->Next()));

  // Ninth exclusion should be to the right of the last exclusion.
  EXPECT_EQ("550,350 50x50", OpportunityToString(iterator->Next()));

  // There should be no tenth opportunity.
  EXPECT_EQ("(null)", OpportunityToString(iterator->Next()));
}

}  // namespace

}  // namespace blink
