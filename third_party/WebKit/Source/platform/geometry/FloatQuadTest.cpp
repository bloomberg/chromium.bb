// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/geometry/FloatQuad.h"

#include <limits>
#include "testing/gtest/include/gtest/gtest.h"
#include "wtf/text/WTFString.h"

namespace blink {

TEST(FloatQuadTest, ToString) {
  FloatQuad quad(FloatPoint(2, 3), FloatPoint(5, 7), FloatPoint(11, 13),
                 FloatPoint(17, 19));
  EXPECT_EQ("2,3; 5,7; 11,13; 17,19", quad.toString());
}

TEST(FloatQuadTest, BoundingBox) {
  FloatQuad quad(FloatPoint(2, 3), FloatPoint(5, 7), FloatPoint(11, 13),
                 FloatPoint(17, 19));
  FloatRect rect = quad.boundingBox();
  EXPECT_EQ(rect.x(), 2);
  EXPECT_EQ(rect.y(), 3);
  EXPECT_EQ(rect.width(), 17 - 2);
  EXPECT_EQ(rect.height(), 19 - 3);
}

TEST(FloatQuadTest, BoundingBoxSaturateInf) {
  double inf = std::numeric_limits<double>::infinity();
  FloatQuad quad(FloatPoint(-inf, 3), FloatPoint(5, inf), FloatPoint(11, 13),
                 FloatPoint(17, 19));
  FloatRect rect = quad.boundingBox();
  EXPECT_EQ(rect.x(), std::numeric_limits<int>::min());
  EXPECT_EQ(rect.y(), 3.0f);
  EXPECT_EQ(rect.width(), 17.0f - std::numeric_limits<int>::min());
  EXPECT_EQ(rect.height(), std::numeric_limits<int>::max() - 3.0f);
}
}  // namespace blink
