// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/style/BorderValue.h"

#include <limits.h>
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(BorderValueTest, BorderValueWidth) {
  const float tolerance = 1.0f / kBorderWidthDenominator;
  BorderValue border;

  border.setWidth(1.0f);
  EXPECT_FLOAT_EQ(1.0f, border.width());
  border.setWidth(1.25f);
  EXPECT_FLOAT_EQ(1.25f, border.width());
  border.setWidth(1.1f);
  EXPECT_NEAR(border.width(), 1.1f, tolerance);
  border.setWidth(1.33f);
  EXPECT_NEAR(border.width(), 1.33f, tolerance);
  border.setWidth(1.3333f);
  EXPECT_NEAR(border.width(), 1.3333f, tolerance);
  border.setWidth(1.53434f);
  EXPECT_NEAR(border.width(), 1.53434f, tolerance);
  border.setWidth(345634);
  EXPECT_NEAR(border.width(), 345634.0f, tolerance);
  border.setWidth(345634.12335f);
  EXPECT_NEAR(border.width(), 345634.12335f, tolerance);

  border.setWidth(0);
  EXPECT_EQ(0, border.width());
  border.setWidth(1);
  EXPECT_EQ(1, border.width());
  border.setWidth(100);
  EXPECT_EQ(100, border.width());
  border.setWidth(1000);
  EXPECT_EQ(1000, border.width());
  border.setWidth(10000);
  EXPECT_EQ(10000, border.width());
  border.setWidth(kMaxForBorderWidth / 2);
  EXPECT_EQ(kMaxForBorderWidth / 2, border.width());
  border.setWidth(kMaxForBorderWidth - 1);
  EXPECT_EQ(kMaxForBorderWidth - 1, border.width());
  border.setWidth(kMaxForBorderWidth);
  EXPECT_EQ(kMaxForBorderWidth, border.width());
  border.setWidth(kMaxForBorderWidth + 1);
  EXPECT_EQ(kMaxForBorderWidth, border.width());
  border.setWidth(INT_MAX / 2);
  EXPECT_EQ(kMaxForBorderWidth, border.width());
  border.setWidth(INT_MAX);
  EXPECT_EQ(kMaxForBorderWidth, border.width());
}

}  // namespace blink
