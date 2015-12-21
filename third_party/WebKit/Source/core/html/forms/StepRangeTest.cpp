// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/html/forms/StepRange.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(StepRangeTest, ClampValueWithOutStepMatchedValue)
{
    // <input type=range value=200 min=0 max=100 step=1000>
    StepRange stepRange(Decimal(200), Decimal(0), Decimal(100), Decimal(1000), StepRange::StepDescription());

    EXPECT_EQ(Decimal(100), stepRange.clampValue(Decimal(200)));
    EXPECT_EQ(Decimal(0), stepRange.clampValue(Decimal(-100)));
}

} // namespace blink
