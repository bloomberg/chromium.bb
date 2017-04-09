// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/geometry/FloatQuad.h"

#include <limits>
#include "platform/wtf/text/WTFString.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(FloatQuadTest, ToString) {
  FloatQuad quad(FloatPoint(2, 3), FloatPoint(5, 7), FloatPoint(11, 13),
                 FloatPoint(17, 19));
  EXPECT_EQ("2,3; 5,7; 11,13; 17,19", quad.ToString());
}

TEST(FloatQuadTest, BoundingBox) {
  FloatQuad quad(FloatPoint(2, 3), FloatPoint(5, 7), FloatPoint(11, 13),
                 FloatPoint(17, 19));
  FloatRect rect = quad.BoundingBox();
  EXPECT_EQ(rect.X(), 2);
  EXPECT_EQ(rect.Y(), 3);
  EXPECT_EQ(rect.Width(), 17 - 2);
  EXPECT_EQ(rect.Height(), 19 - 3);
}

TEST(FloatQuadTest, BoundingBoxSaturateInf) {
  double inf = std::numeric_limits<double>::infinity();
  FloatQuad quad(FloatPoint(-inf, 3), FloatPoint(5, inf), FloatPoint(11, 13),
                 FloatPoint(17, 19));
  FloatRect rect = quad.BoundingBox();
  EXPECT_EQ(rect.X(), std::numeric_limits<int>::min());
  EXPECT_EQ(rect.Y(), 3.0f);
  EXPECT_EQ(rect.Width(), 17.0f - std::numeric_limits<int>::min());
  EXPECT_EQ(rect.Height(), std::numeric_limits<int>::max() - 3.0f);
}
}  // namespace blink
