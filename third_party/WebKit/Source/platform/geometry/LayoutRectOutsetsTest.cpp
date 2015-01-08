// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "platform/geometry/LayoutRectOutsets.h"

#include <gtest/gtest.h>

namespace blink {
namespace {

TEST(LayoutRectOutsetsTest, LogicalOutsets_Horizontal)
{
    LayoutRectOutsets outsets(1, 2, 3, 4);
    EXPECT_EQ(LayoutRectOutsets(1, 2, 3, 4), outsets.logicalOutsets(TopToBottomWritingMode));
    EXPECT_EQ(LayoutRectOutsets(1, 2, 3, 4), outsets.logicalOutsets(BottomToTopWritingMode));
}

TEST(LayoutRectOutsetsTest, LogicalOutsets_Vertical)
{
    LayoutRectOutsets outsets(1, 2, 3, 4);
    EXPECT_EQ(LayoutRectOutsets(4, 3, 2, 1), outsets.logicalOutsets(LeftToRightWritingMode));
    EXPECT_EQ(LayoutRectOutsets(4, 3, 2, 1), outsets.logicalOutsets(RightToLeftWritingMode));
}

} // namespace
} // namespace blink
