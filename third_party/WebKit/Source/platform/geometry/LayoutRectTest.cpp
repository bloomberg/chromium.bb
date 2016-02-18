// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/geometry/LayoutRect.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/text/WTFString.h"

namespace blink {

#ifndef NDEBUG
TEST(LayoutRectTest, ToString)
{
    LayoutRect emptyRect = LayoutRect();
    EXPECT_EQ(String("0.000000,0.000000 0.000000x0.000000"), emptyRect.toString());

    LayoutRect rect(1, 2, 3, 4);
    EXPECT_EQ(String("1.000000,2.000000 3.000000x4.000000"), rect.toString());

    LayoutRect granularRect(LayoutUnit(1.6f), LayoutUnit(2.7f), LayoutUnit(3.8f), LayoutUnit(4.9f));
    EXPECT_EQ(String("1.593750,2.687500 3.796875x4.890625"), granularRect.toString());
}
#endif

} // namespace blink
