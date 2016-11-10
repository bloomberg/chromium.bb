// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_units.h"

#include "platform/text/TextDirection.h"
#include "core/layout/ng/ng_writing_mode.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

TEST(NGUnitsTest, ConvertLogicalOffsetToPhysicalOffset) {
  NGLogicalOffset logical_offset(LayoutUnit(20), LayoutUnit(30));
  NGPhysicalSize outer_size(LayoutUnit(300), LayoutUnit(400));
  NGPhysicalSize inner_size(LayoutUnit(5), LayoutUnit(65));
  NGPhysicalOffset offset;

  offset = logical_offset.ConvertToPhysical(HorizontalTopBottom, LTR,
                                            outer_size, inner_size);
  EXPECT_EQ(LayoutUnit(20), offset.left);
  EXPECT_EQ(LayoutUnit(30), offset.top);

  offset = logical_offset.ConvertToPhysical(HorizontalTopBottom, RTL,
                                            outer_size, inner_size);
  EXPECT_EQ(LayoutUnit(275), offset.left);
  EXPECT_EQ(LayoutUnit(30), offset.top);

  offset = logical_offset.ConvertToPhysical(VerticalRightLeft, LTR, outer_size,
                                            inner_size);
  EXPECT_EQ(LayoutUnit(265), offset.left);
  EXPECT_EQ(LayoutUnit(20), offset.top);

  offset = logical_offset.ConvertToPhysical(VerticalRightLeft, RTL, outer_size,
                                            inner_size);
  EXPECT_EQ(LayoutUnit(265), offset.left);
  EXPECT_EQ(LayoutUnit(315), offset.top);

  offset = logical_offset.ConvertToPhysical(SidewaysRightLeft, LTR, outer_size,
                                            inner_size);
  EXPECT_EQ(LayoutUnit(265), offset.left);
  EXPECT_EQ(LayoutUnit(20), offset.top);

  offset = logical_offset.ConvertToPhysical(SidewaysRightLeft, RTL, outer_size,
                                            inner_size);
  EXPECT_EQ(LayoutUnit(265), offset.left);
  EXPECT_EQ(LayoutUnit(315), offset.top);

  offset = logical_offset.ConvertToPhysical(VerticalLeftRight, LTR, outer_size,
                                            inner_size);
  EXPECT_EQ(LayoutUnit(30), offset.left);
  EXPECT_EQ(LayoutUnit(20), offset.top);

  offset = logical_offset.ConvertToPhysical(VerticalLeftRight, RTL, outer_size,
                                            inner_size);
  EXPECT_EQ(LayoutUnit(30), offset.left);
  EXPECT_EQ(LayoutUnit(315), offset.top);

  offset = logical_offset.ConvertToPhysical(SidewaysLeftRight, LTR, outer_size,
                                            inner_size);
  EXPECT_EQ(LayoutUnit(30), offset.left);
  EXPECT_EQ(LayoutUnit(315), offset.top);

  offset = logical_offset.ConvertToPhysical(SidewaysLeftRight, RTL, outer_size,
                                            inner_size);
  EXPECT_EQ(LayoutUnit(30), offset.left);
  EXPECT_EQ(LayoutUnit(20), offset.top);
}

}  // namespace

}  // namespace blink
