// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ng/ng_min_max_content_size.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

TEST(NGUnitsTest, ShrinkToFit) {
  MinMaxContentSize sizes;

  sizes.min_content = LayoutUnit(100);
  sizes.max_content = LayoutUnit(200);
  EXPECT_EQ(LayoutUnit(200), sizes.ShrinkToFit(LayoutUnit(300)));

  sizes.min_content = LayoutUnit(100);
  sizes.max_content = LayoutUnit(300);
  EXPECT_EQ(LayoutUnit(200), sizes.ShrinkToFit(LayoutUnit(200)));

  sizes.min_content = LayoutUnit(200);
  sizes.max_content = LayoutUnit(300);
  EXPECT_EQ(LayoutUnit(200), sizes.ShrinkToFit(LayoutUnit(100)));
}

}  // namespace

}  // namespace blink
