// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/geometry/double_point.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(DoublePointTest, RoundedIntPoint) {
  // Value not exactly representable as a float.
  DoublePoint p1(16777217.0, -16777217.0);
  IntPoint rounded_p1 = RoundedIntPoint(p1);
  EXPECT_EQ(16777217, rounded_p1.X());
  EXPECT_EQ(-16777217, rounded_p1.Y());
}

}  // namespace blink
