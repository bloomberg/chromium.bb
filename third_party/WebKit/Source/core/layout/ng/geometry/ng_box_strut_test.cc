// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/geometry/ng_box_strut.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

// Ideally, this would be tested by NGBoxStrut::ConvertToPhysical, but
// this has not been implemented yet.
TEST(NGGeometryUnitsTest, ConvertPhysicalStrutToLogical) {
  LayoutUnit left{5}, right{10}, top{15}, bottom{20};
  NGPhysicalBoxStrut physical{top, right, bottom, left};

  NGBoxStrut logical =
      physical.ConvertToLogical(kHorizontalTopBottom, TextDirection::kLtr);
  EXPECT_EQ(left, logical.inline_start);
  EXPECT_EQ(top, logical.block_start);

  logical =
      physical.ConvertToLogical(kHorizontalTopBottom, TextDirection::kRtl);
  EXPECT_EQ(right, logical.inline_start);
  EXPECT_EQ(top, logical.block_start);

  logical = physical.ConvertToLogical(kVerticalLeftRight, TextDirection::kLtr);
  EXPECT_EQ(top, logical.inline_start);
  EXPECT_EQ(left, logical.block_start);

  logical = physical.ConvertToLogical(kVerticalLeftRight, TextDirection::kRtl);
  EXPECT_EQ(bottom, logical.inline_start);
  EXPECT_EQ(left, logical.block_start);

  logical = physical.ConvertToLogical(kVerticalRightLeft, TextDirection::kLtr);
  EXPECT_EQ(top, logical.inline_start);
  EXPECT_EQ(right, logical.block_start);

  logical = physical.ConvertToLogical(kVerticalRightLeft, TextDirection::kRtl);
  EXPECT_EQ(bottom, logical.inline_start);
  EXPECT_EQ(right, logical.block_start);
}

}  // namespace

}  // namespace blink
