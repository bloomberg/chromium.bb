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
  EXPECT_TRUE(rect.isInfinite());

  FloatClipRect rect2((FloatRect(1, 2, 3, 4)));
  EXPECT_FALSE(rect2.isInfinite());
}

TEST_F(FloatClipRectTest, MoveBy) {
  FloatClipRect rect;
  rect.moveBy(FloatPoint(1, 2));
  EXPECT_EQ(rect.rect(), FloatClipRect().rect());
}

TEST_F(FloatClipRectTest, SetRect) {
  FloatClipRect rect;
  rect.setRect(FloatRect(1, 2, 3, 4));
  EXPECT_FALSE(rect.isInfinite());
  EXPECT_EQ(FloatRect(1, 2, 3, 4), rect.rect());
}

TEST_F(FloatClipRectTest, SetHasRadius) {
  FloatClipRect rect;
  rect.setHasRadius();
  EXPECT_FALSE(rect.isInfinite());
  EXPECT_TRUE(rect.hasRadius());
}

}  // namespace blink
