// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/FloatClipRect.h"

#include "platform/geometry/FloatRect.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class FloatClipRectTest : public ::testing::Test {
 public:
};

TEST_F(FloatClipRectTest, InfinitRect) {
  FloatClipRect rect;
  EXPECT_TRUE(rect.IsInfinite());

  FloatClipRect rect2((FloatRect(1, 2, 3, 4)));
  EXPECT_FALSE(rect2.IsInfinite());
}

TEST_F(FloatClipRectTest, MoveBy) {
  FloatClipRect rect;
  rect.MoveBy(FloatPoint(1, 2));
  EXPECT_EQ(rect.Rect(), FloatClipRect().Rect());
}

TEST_F(FloatClipRectTest, SetRect) {
  FloatClipRect rect;
  rect.SetRect(FloatRect(1, 2, 3, 4));
  EXPECT_FALSE(rect.IsInfinite());
  EXPECT_EQ(FloatRect(1, 2, 3, 4), rect.Rect());
}

TEST_F(FloatClipRectTest, SetHasRadius) {
  FloatClipRect rect;
  rect.SetHasRadius();
  EXPECT_FALSE(rect.IsInfinite());
  EXPECT_TRUE(rect.HasRadius());
}

}  // namespace blink
