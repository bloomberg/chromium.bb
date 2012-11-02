// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/size_base.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/size.h"
#include "ui/gfx/size_conversions.h"
#include "ui/gfx/size_f.h"

namespace gfx {

namespace {

int TestSizeF(const SizeF& s) {
  return s.width();
}

}  // namespace

TEST(SizeTest, ToSizeF) {
  // Check that implicit conversion from integer to float compiles.
  Size a(10, 20);
  float width = TestSizeF(a);
  EXPECT_EQ(width, a.width());

  SizeF b(10, 20);

  EXPECT_EQ(a, b);
  EXPECT_EQ(b, a);
}

TEST(SizeTest, ToFlooredSize) {
  EXPECT_EQ(Size(0, 0), ToFlooredSize(SizeF(0, 0)));
  EXPECT_EQ(Size(0, 0), ToFlooredSize(SizeF(0.0001f, 0.0001f)));
  EXPECT_EQ(Size(0, 0), ToFlooredSize(SizeF(0.4999f, 0.4999f)));
  EXPECT_EQ(Size(0, 0), ToFlooredSize(SizeF(0.5f, 0.5f)));
  EXPECT_EQ(Size(0, 0), ToFlooredSize(SizeF(0.9999f, 0.9999f)));

  EXPECT_EQ(Size(10, 10), ToFlooredSize(SizeF(10, 10)));
  EXPECT_EQ(Size(10, 10), ToFlooredSize(SizeF(10.0001f, 10.0001f)));
  EXPECT_EQ(Size(10, 10), ToFlooredSize(SizeF(10.4999f, 10.4999f)));
  EXPECT_EQ(Size(10, 10), ToFlooredSize(SizeF(10.5f, 10.5f)));
  EXPECT_EQ(Size(10, 10), ToFlooredSize(SizeF(10.9999f, 10.9999f)));

  EXPECT_EQ(Size(-10, -10), ToFlooredSize(SizeF(-10, -10)));
  EXPECT_EQ(Size(-11, -11), ToFlooredSize(SizeF(-10.0001f, -10.0001f)));
  EXPECT_EQ(Size(-11, -11), ToFlooredSize(SizeF(-10.4999f, -10.4999f)));
  EXPECT_EQ(Size(-11, -11), ToFlooredSize(SizeF(-10.5f, -10.5f)));
  EXPECT_EQ(Size(-11, -11), ToFlooredSize(SizeF(-10.9999f, -10.9999f)));
}

TEST(SizeTest, ToCeiledSize) {
  EXPECT_EQ(Size(0, 0), ToCeiledSize(SizeF(0, 0)));
  EXPECT_EQ(Size(1, 1), ToCeiledSize(SizeF(0.0001f, 0.0001f)));
  EXPECT_EQ(Size(1, 1), ToCeiledSize(SizeF(0.4999f, 0.4999f)));
  EXPECT_EQ(Size(1, 1), ToCeiledSize(SizeF(0.5f, 0.5f)));
  EXPECT_EQ(Size(1, 1), ToCeiledSize(SizeF(0.9999f, 0.9999f)));

  EXPECT_EQ(Size(10, 10), ToCeiledSize(SizeF(10, 10)));
  EXPECT_EQ(Size(11, 11), ToCeiledSize(SizeF(10.0001f, 10.0001f)));
  EXPECT_EQ(Size(11, 11), ToCeiledSize(SizeF(10.4999f, 10.4999f)));
  EXPECT_EQ(Size(11, 11), ToCeiledSize(SizeF(10.5f, 10.5f)));
  EXPECT_EQ(Size(11, 11), ToCeiledSize(SizeF(10.9999f, 10.9999f)));

  EXPECT_EQ(Size(-10, -10), ToCeiledSize(SizeF(-10, -10)));
  EXPECT_EQ(Size(-10, -10), ToCeiledSize(SizeF(-10.0001f, -10.0001f)));
  EXPECT_EQ(Size(-10, -10), ToCeiledSize(SizeF(-10.4999f, -10.4999f)));
  EXPECT_EQ(Size(-10, -10), ToCeiledSize(SizeF(-10.5f, -10.5f)));
  EXPECT_EQ(Size(-10, -10), ToCeiledSize(SizeF(-10.9999f, -10.9999f)));
}

TEST(SizeTest, ToRoundedSize) {
  EXPECT_EQ(Size(0, 0), ToRoundedSize(SizeF(0, 0)));
  EXPECT_EQ(Size(0, 0), ToRoundedSize(SizeF(0.0001f, 0.0001f)));
  EXPECT_EQ(Size(0, 0), ToRoundedSize(SizeF(0.4999f, 0.4999f)));
  EXPECT_EQ(Size(1, 1), ToRoundedSize(SizeF(0.5f, 0.5f)));
  EXPECT_EQ(Size(1, 1), ToRoundedSize(SizeF(0.9999f, 0.9999f)));

  EXPECT_EQ(Size(10, 10), ToRoundedSize(SizeF(10, 10)));
  EXPECT_EQ(Size(10, 10), ToRoundedSize(SizeF(10.0001f, 10.0001f)));
  EXPECT_EQ(Size(10, 10), ToRoundedSize(SizeF(10.4999f, 10.4999f)));
  EXPECT_EQ(Size(11, 11), ToRoundedSize(SizeF(10.5f, 10.5f)));
  EXPECT_EQ(Size(11, 11), ToRoundedSize(SizeF(10.9999f, 10.9999f)));

  EXPECT_EQ(Size(-10, -10), ToRoundedSize(SizeF(-10, -10)));
  EXPECT_EQ(Size(-10, -10), ToRoundedSize(SizeF(-10.0001f, -10.0001f)));
  EXPECT_EQ(Size(-10, -10), ToRoundedSize(SizeF(-10.4999f, -10.4999f)));
  EXPECT_EQ(Size(-11, -11), ToRoundedSize(SizeF(-10.5f, -10.5f)));
  EXPECT_EQ(Size(-11, -11), ToRoundedSize(SizeF(-10.9999f, -10.9999f)));
}

}  // namespace gfx
