// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/geometry/LayoutRectOutsets.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace {

TEST(LayoutRectOutsetsTest, LogicalOutsets_Horizontal) {
  LayoutRectOutsets outsets(1, 2, 3, 4);
  EXPECT_EQ(LayoutRectOutsets(1, 2, 3, 4),
            outsets.LogicalOutsets(WritingMode::kHorizontalTb));
}

TEST(LayoutRectOutsetsTest, LogicalOutsets_Vertical) {
  LayoutRectOutsets outsets(1, 2, 3, 4);
  EXPECT_EQ(LayoutRectOutsets(4, 3, 2, 1),
            outsets.LogicalOutsets(WritingMode::kVerticalLr));
  EXPECT_EQ(LayoutRectOutsets(4, 3, 2, 1),
            outsets.LogicalOutsets(WritingMode::kVerticalRl));
}

TEST(LayoutRectOutsetsTest, LogicalOutsetsWithFlippedLines) {
  LayoutRectOutsets outsets(1, 2, 3, 4);
  EXPECT_EQ(LayoutRectOutsets(1, 2, 3, 4),
            outsets.LogicalOutsetsWithFlippedLines(WritingMode::kHorizontalTb));
  EXPECT_EQ(LayoutRectOutsets(2, 3, 4, 1),
            outsets.LogicalOutsetsWithFlippedLines(WritingMode::kVerticalLr));
  EXPECT_EQ(LayoutRectOutsets(4, 3, 2, 1),
            outsets.LogicalOutsetsWithFlippedLines(WritingMode::kVerticalRl));
}

}  // namespace
}  // namespace blink
