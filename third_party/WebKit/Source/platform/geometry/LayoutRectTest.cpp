// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/geometry/LayoutRect.h"

#include "platform/wtf/text/WTFString.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(LayoutRectTest, ToString) {
  LayoutRect empty_rect = LayoutRect();
  EXPECT_EQ("0,0 0x0", empty_rect.ToString());

  LayoutRect rect(1, 2, 3, 4);
  EXPECT_EQ("1,2 3x4", rect.ToString());

  LayoutRect granular_rect(LayoutUnit(1.6f), LayoutUnit(2.7f), LayoutUnit(3.8f),
                           LayoutUnit(4.9f));
  EXPECT_EQ("1.59375,2.6875 3.79688x4.89063", granular_rect.ToString());
}

#ifndef NDEBUG
TEST(LayoutRectTest, InclusiveIntersect) {
  LayoutRect rect(11, 12, 0, 0);
  EXPECT_TRUE(rect.InclusiveIntersect(LayoutRect(11, 12, 13, 14)));
  EXPECT_EQ(rect, LayoutRect(11, 12, 0, 0));

  rect = LayoutRect(11, 12, 13, 14);
  EXPECT_TRUE(rect.InclusiveIntersect(LayoutRect(24, 8, 0, 7)));
  EXPECT_EQ(rect, LayoutRect(24, 12, 0, 3));

  rect = LayoutRect(11, 12, 13, 14);
  EXPECT_TRUE(rect.InclusiveIntersect(LayoutRect(9, 15, 4, 0)));
  EXPECT_EQ(rect, LayoutRect(11, 15, 2, 0));

  rect = LayoutRect(11, 12, 0, 14);
  EXPECT_FALSE(rect.InclusiveIntersect(LayoutRect(12, 13, 15, 16)));
  EXPECT_EQ(rect, LayoutRect());
}
#endif

}  // namespace blink
