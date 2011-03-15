// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/range/range.h"

TEST(RangeTest, EmptyInit) {
  ui::Range r;
  EXPECT_EQ(0U, r.start());
  EXPECT_EQ(0U, r.end());
  EXPECT_EQ(0U, r.length());
  EXPECT_FALSE(r.is_reversed());
  EXPECT_TRUE(r.is_empty());
  EXPECT_TRUE(r.IsValid());
  EXPECT_EQ(0U, r.GetMin());
  EXPECT_EQ(0U, r.GetMax());
}

TEST(RangeTest, StartEndInit) {
  ui::Range r(10, 15);
  EXPECT_EQ(10U, r.start());
  EXPECT_EQ(15U, r.end());
  EXPECT_EQ(5U, r.length());
  EXPECT_FALSE(r.is_reversed());
  EXPECT_FALSE(r.is_empty());
  EXPECT_TRUE(r.IsValid());
  EXPECT_EQ(10U, r.GetMin());
  EXPECT_EQ(15U, r.GetMax());
}

TEST(RangeTest, StartEndReversedInit) {
  ui::Range r(10, 5);
  EXPECT_EQ(10U, r.start());
  EXPECT_EQ(5U, r.end());
  EXPECT_EQ(5U, r.length());
  EXPECT_TRUE(r.is_reversed());
  EXPECT_FALSE(r.is_empty());
  EXPECT_TRUE(r.IsValid());
  EXPECT_EQ(5U, r.GetMin());
  EXPECT_EQ(10U, r.GetMax());
}

TEST(RangeTest, PositionInit) {
  ui::Range r(12);
  EXPECT_EQ(12U, r.start());
  EXPECT_EQ(12U, r.end());
  EXPECT_EQ(0U, r.length());
  EXPECT_FALSE(r.is_reversed());
  EXPECT_TRUE(r.is_empty());
  EXPECT_TRUE(r.IsValid());
  EXPECT_EQ(12U, r.GetMin());
  EXPECT_EQ(12U, r.GetMax());
}

TEST(RangeTest, InvalidRange) {
  ui::Range r(ui::Range::InvalidRange());
  EXPECT_EQ(0U, r.length());
  EXPECT_EQ(r.start(), r.end());
  EXPECT_FALSE(r.is_reversed());
  EXPECT_TRUE(r.is_empty());
  EXPECT_FALSE(r.IsValid());
}

TEST(RangeTest, Equality) {
  ui::Range r1(10, 4);
  ui::Range r2(10, 4);
  ui::Range r3(10, 2);
  EXPECT_EQ(r1, r2);
  EXPECT_NE(r1, r3);
  EXPECT_NE(r2, r3);

  ui::Range r4(11, 4);
  EXPECT_NE(r1, r4);
  EXPECT_NE(r2, r4);
  EXPECT_NE(r3, r4);

  ui::Range r5(12, 5);
  EXPECT_NE(r1, r5);
  EXPECT_NE(r2, r5);
  EXPECT_NE(r3, r5);
}

TEST(RangeTest, EqualsIgnoringDirection) {
  ui::Range r1(10, 5);
  ui::Range r2(5, 10);
  EXPECT_TRUE(r1.EqualsIgnoringDirection(r2));
}

TEST(RangeTest, SetStart) {
  ui::Range r(10, 20);
  EXPECT_EQ(10U, r.start());
  EXPECT_EQ(10U, r.length());

  r.set_start(42);
  EXPECT_EQ(42U, r.start());
  EXPECT_EQ(20U, r.end());
  EXPECT_EQ(22U, r.length());
  EXPECT_TRUE(r.is_reversed());
}

TEST(RangeTest, SetEnd) {
  ui::Range r(10, 13);
  EXPECT_EQ(10U, r.start());
  EXPECT_EQ(3U, r.length());

  r.set_end(20);
  EXPECT_EQ(10U, r.start());
  EXPECT_EQ(20U, r.end());
  EXPECT_EQ(10U, r.length());
}

TEST(RangeTest, SetStartAndEnd) {
  ui::Range r;
  r.set_end(5);
  r.set_start(1);
  EXPECT_EQ(1U, r.start());
  EXPECT_EQ(5U, r.end());
  EXPECT_EQ(4U, r.length());
  EXPECT_EQ(1U, r.GetMin());
  EXPECT_EQ(5U, r.GetMax());
}

TEST(RangeTest, ReversedRange) {
  ui::Range r(10, 5);
  EXPECT_EQ(10U, r.start());
  EXPECT_EQ(5U, r.end());
  EXPECT_EQ(5U, r.length());
  EXPECT_TRUE(r.is_reversed());
  EXPECT_TRUE(r.IsValid());
  EXPECT_EQ(5U, r.GetMin());
  EXPECT_EQ(10U, r.GetMax());
}

TEST(RangeTest, SetReversedRange) {
  ui::Range r(10, 20);
  r.set_start(25);
  EXPECT_EQ(25U, r.start());
  EXPECT_EQ(20U, r.end());
  EXPECT_EQ(5U, r.length());
  EXPECT_TRUE(r.is_reversed());
  EXPECT_TRUE(r.IsValid());

  r.set_end(21);
  EXPECT_EQ(25U, r.start());
  EXPECT_EQ(21U, r.end());
  EXPECT_EQ(4U, r.length());
  EXPECT_TRUE(r.IsValid());
  EXPECT_EQ(21U, r.GetMin());
  EXPECT_EQ(25U, r.GetMax());
}

#if defined(OS_WIN)
TEST(RangeTest, FromCHARRANGE) {
  CHARRANGE cr = { 10, 32 };
  ui::Range r(cr, 50);
  EXPECT_EQ(10U, r.start());
  EXPECT_EQ(32U, r.end());
  EXPECT_EQ(22U, r.length());
  EXPECT_FALSE(r.is_reversed());
  EXPECT_TRUE(r.IsValid());
}

TEST(RangeTest, FromReversedCHARRANGE) {
  CHARRANGE cr = { 20, 10 };
  ui::Range r(cr, 40);
  EXPECT_EQ(20U, r.start());
  EXPECT_EQ(10U, r.end());
  EXPECT_EQ(10U, r.length());
  EXPECT_TRUE(r.is_reversed());
  EXPECT_TRUE(r.IsValid());
}

TEST(RangeTest, FromCHARRANGETotal) {
  CHARRANGE cr = { 0, -1 };
  ui::Range r(cr, 20);
  EXPECT_EQ(0U, r.start());
  EXPECT_EQ(20U, r.end());
  EXPECT_EQ(20U, r.length());
  EXPECT_FALSE(r.is_reversed());
  EXPECT_TRUE(r.IsValid());
}

TEST(RangeTest, ToCHARRANGE) {
  ui::Range r(10, 30);
  CHARRANGE cr = r.ToCHARRANGE();
  EXPECT_EQ(10, cr.cpMin);
  EXPECT_EQ(30, cr.cpMax);
}

TEST(RangeTest, ReversedToCHARRANGE) {
  ui::Range r(20, 10);
  CHARRANGE cr = r.ToCHARRANGE();
  EXPECT_EQ(20U, cr.cpMin);
  EXPECT_EQ(10U, cr.cpMax);
}

TEST(RangeTest, FromCHARRANGEInvalid) {
  CHARRANGE cr = { -1, -1 };
  ui::Range r(cr, 30);
  EXPECT_FALSE(r.IsValid());
}

TEST(RangeTest, ToCHARRANGEInvalid) {
  ui::Range r(ui::Range::InvalidRange());
  CHARRANGE cr = r.ToCHARRANGE();
  EXPECT_EQ(-1, cr.cpMin);
  EXPECT_EQ(-1, cr.cpMax);
}
#endif  // defined(OS_WIN)
