// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_constraint_space.h"

#include "core/layout/ng/ng_layout_opportunity_iterator.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

TEST(NGConstraintSpaceTest, WritingMode) {
  NGConstraintSpace* horz_space = new NGConstraintSpace(
      HorizontalTopBottom, NGLogicalSize(LayoutUnit(200), LayoutUnit(100)));
  horz_space->SetOverflowTriggersScrollbar(true, false);
  horz_space->SetFixedSize(true, false);
  horz_space->SetFragmentationType(FragmentColumn);

  NGConstraintSpace* vert_space =
      new NGConstraintSpace(VerticalRightLeft, horz_space);

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

TEST(NGConstraintSpaceTest, LayoutOpportunitiesNoExclusions) {
  NGPhysicalSize physical_size;
  physical_size.width = LayoutUnit(600);
  physical_size.height = LayoutUnit(400);
  auto* physical_space = new NGPhysicalConstraintSpace(physical_size);
  auto* space = new NGConstraintSpace(HorizontalTopBottom, physical_space);

  bool for_inline_or_bfc = true;
  auto* iterator = space->LayoutOpportunities(NGClearNone, for_inline_or_bfc);

  const NGConstraintSpace* firstOpportunity = iterator->Next();
  EXPECT_NE(nullptr, firstOpportunity);
  EXPECT_EQ(LayoutUnit(600), firstOpportunity->Size().inline_size);
  EXPECT_EQ(LayoutUnit(400), firstOpportunity->Size().block_size);

  const NGConstraintSpace* secondOpportunity = iterator->Next();
  EXPECT_EQ(nullptr, secondOpportunity);
}

TEST(NGConstraintSpaceTest, LayoutOpportunitiesOneExclusion) {
  NGPhysicalSize physical_size;
  physical_size.width = LayoutUnit(600);
  physical_size.height = LayoutUnit(400);
  auto* physical_space = new NGPhysicalConstraintSpace(physical_size);

  // Add a 100x100 exclusion in the top right corner.
  physical_space->AddExclusion(NGExclusion(LayoutUnit(0), LayoutUnit(600),
                                           LayoutUnit(100), LayoutUnit(500)));

  auto* space = new NGConstraintSpace(HorizontalTopBottom, physical_space);
  bool for_inline_or_bfc = true;
  auto* iterator = space->LayoutOpportunities(NGClearNone, for_inline_or_bfc);

  // First opportunity should be to the left of the exclusion.
  const NGConstraintSpace* firstOpportunity = iterator->Next();
  EXPECT_NE(nullptr, firstOpportunity);
  EXPECT_EQ(LayoutUnit(0), firstOpportunity->Offset().inline_offset);
  EXPECT_EQ(LayoutUnit(0), firstOpportunity->Offset().block_offset);
  EXPECT_EQ(LayoutUnit(500), firstOpportunity->Size().inline_size);
  EXPECT_EQ(LayoutUnit(400), firstOpportunity->Size().block_size);

  const NGConstraintSpace* secondOpportunity = iterator->Next();
  EXPECT_EQ(nullptr, secondOpportunity);
}

}  // namespace

}  // namespace blink
