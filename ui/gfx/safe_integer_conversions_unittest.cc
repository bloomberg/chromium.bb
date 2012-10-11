// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/safe_integer_conversions.h"

#include <limits>

#include "testing/gtest/include/gtest/gtest.h"

namespace ui {

TEST(SafeIntegerConversions, ClampToInt) {
  EXPECT_EQ(0, gfx::ClampToInt(std::numeric_limits<float>::quiet_NaN()));

  float max = std::numeric_limits<int>::max();
  float min = std::numeric_limits<int>::min();
  float infinity = std::numeric_limits<float>::infinity();

  int int_max = std::numeric_limits<int>::max();
  int int_min = std::numeric_limits<int>::min();

  EXPECT_EQ(int_max, gfx::ClampToInt(infinity));
  EXPECT_EQ(int_max, gfx::ClampToInt(max));
  EXPECT_EQ(int_max, gfx::ClampToInt(max + 100));

  EXPECT_EQ(-100, gfx::ClampToInt(-100.5f));
  EXPECT_EQ(0, gfx::ClampToInt(0));
  EXPECT_EQ(100, gfx::ClampToInt(100.5f));

  EXPECT_EQ(int_min, gfx::ClampToInt(-infinity));
  EXPECT_EQ(int_min, gfx::ClampToInt(min));
  EXPECT_EQ(int_min, gfx::ClampToInt(min - 100));
}

TEST(SafeIntegerConversions, ToFlooredInt) {
  EXPECT_EQ(0, gfx::ToFlooredInt(std::numeric_limits<float>::quiet_NaN()));

  float max = std::numeric_limits<int>::max();
  float min = std::numeric_limits<int>::min();
  float infinity = std::numeric_limits<float>::infinity();

  int int_max = std::numeric_limits<int>::max();
  int int_min = std::numeric_limits<int>::min();

  EXPECT_EQ(int_max, gfx::ToFlooredInt(infinity));
  EXPECT_EQ(int_max, gfx::ToFlooredInt(max));
  EXPECT_EQ(int_max, gfx::ToFlooredInt(max + 100));

  EXPECT_EQ(-101, gfx::ToFlooredInt(-100.5f));
  EXPECT_EQ(0, gfx::ToFlooredInt(0));
  EXPECT_EQ(100, gfx::ToFlooredInt(100.5f));

  EXPECT_EQ(int_min, gfx::ToFlooredInt(-infinity));
  EXPECT_EQ(int_min, gfx::ToFlooredInt(min));
  EXPECT_EQ(int_min, gfx::ToFlooredInt(min - 100));
}

TEST(SafeIntegerConversions, ToCeiledInt) {
  EXPECT_EQ(0, gfx::ToCeiledInt(std::numeric_limits<float>::quiet_NaN()));

  float max = std::numeric_limits<int>::max();
  float min = std::numeric_limits<int>::min();
  float infinity = std::numeric_limits<float>::infinity();

  int int_max = std::numeric_limits<int>::max();
  int int_min = std::numeric_limits<int>::min();

  EXPECT_EQ(int_max, gfx::ToCeiledInt(infinity));
  EXPECT_EQ(int_max, gfx::ToCeiledInt(max));
  EXPECT_EQ(int_max, gfx::ToCeiledInt(max + 100));

  EXPECT_EQ(-100, gfx::ToCeiledInt(-100.5f));
  EXPECT_EQ(0, gfx::ToCeiledInt(0));
  EXPECT_EQ(101, gfx::ToCeiledInt(100.5f));

  EXPECT_EQ(int_min, gfx::ToCeiledInt(-infinity));
  EXPECT_EQ(int_min, gfx::ToCeiledInt(min));
  EXPECT_EQ(int_min, gfx::ToCeiledInt(min - 100));
}

TEST(SafeIntegerConversions, ToRoundedInt) {
  EXPECT_EQ(0, gfx::ToRoundedInt(std::numeric_limits<float>::quiet_NaN()));

  float max = std::numeric_limits<int>::max();
  float min = std::numeric_limits<int>::min();
  float infinity = std::numeric_limits<float>::infinity();

  int int_max = std::numeric_limits<int>::max();
  int int_min = std::numeric_limits<int>::min();

  EXPECT_EQ(int_max, gfx::ToRoundedInt(infinity));
  EXPECT_EQ(int_max, gfx::ToRoundedInt(max));
  EXPECT_EQ(int_max, gfx::ToRoundedInt(max + 100));

  EXPECT_EQ(-100, gfx::ToRoundedInt(-100.1f));
  EXPECT_EQ(-101, gfx::ToRoundedInt(-100.5f));
  EXPECT_EQ(-101, gfx::ToRoundedInt(-100.9f));
  EXPECT_EQ(0, gfx::ToRoundedInt(0));
  EXPECT_EQ(100, gfx::ToRoundedInt(100.1f));
  EXPECT_EQ(101, gfx::ToRoundedInt(100.5f));
  EXPECT_EQ(101, gfx::ToRoundedInt(100.9f));

  EXPECT_EQ(int_min, gfx::ToRoundedInt(-infinity));
  EXPECT_EQ(int_min, gfx::ToRoundedInt(min));
  EXPECT_EQ(int_min, gfx::ToRoundedInt(min - 100));
}

}  // namespace ui
