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

  EXPECT_EQ(max, gfx::ClampToInt(std::numeric_limits<float>::infinity()));
  EXPECT_EQ(max, gfx::ClampToInt(max));
  EXPECT_EQ(max, gfx::ClampToInt(max + 1));
  EXPECT_EQ(max - 1, gfx::ClampToInt(max - 1));

  EXPECT_EQ(-100, gfx::ClampToInt(-100.5f));
  EXPECT_EQ(0, gfx::ClampToInt(0));
  EXPECT_EQ(100, gfx::ClampToInt(100.5f));

  EXPECT_EQ(min, gfx::ClampToInt(-std::numeric_limits<float>::infinity()));
  EXPECT_EQ(min, gfx::ClampToInt(min));
  EXPECT_EQ(min, gfx::ClampToInt(min - 1));
  EXPECT_EQ(min + 1, gfx::ClampToInt(min + 1));
}

TEST(SafeIntegerConversions, ToFlooredInt) {
  EXPECT_EQ(0, gfx::ToFlooredInt(std::numeric_limits<float>::quiet_NaN()));

  float max = std::numeric_limits<int>::max();
  float min = std::numeric_limits<int>::min();

  EXPECT_EQ(max, gfx::ToFlooredInt(std::numeric_limits<float>::infinity()));
  EXPECT_EQ(max, gfx::ToFlooredInt(max));
  EXPECT_EQ(max, gfx::ToFlooredInt(max + 0.5f));
  EXPECT_EQ(max - 1, gfx::ToFlooredInt(max - 0.5f));

  EXPECT_EQ(-101, gfx::ToFlooredInt(-100.5f));
  EXPECT_EQ(0, gfx::ToFlooredInt(0));
  EXPECT_EQ(100, gfx::ToFlooredInt(100.5f));

  EXPECT_EQ(min, gfx::ToFlooredInt(-std::numeric_limits<float>::infinity()));
  EXPECT_EQ(min, gfx::ToFlooredInt(min));
  EXPECT_EQ(min, gfx::ToFlooredInt(min - 0.5f));
  EXPECT_EQ(min, gfx::ToFlooredInt(min + 0.5f));
}

TEST(SafeIntegerConversions, ToCeiledInt) {
  EXPECT_EQ(0, gfx::ToCeiledInt(std::numeric_limits<float>::quiet_NaN()));

  float max = std::numeric_limits<int>::max();
  float min = std::numeric_limits<int>::min();

  EXPECT_EQ(max, gfx::ToCeiledInt(std::numeric_limits<float>::infinity()));
  EXPECT_EQ(max, gfx::ToCeiledInt(max));
  EXPECT_EQ(max, gfx::ToCeiledInt(max + 0.5f));
  EXPECT_EQ(max, gfx::ToCeiledInt(max - 0.5f));

  EXPECT_EQ(-100, gfx::ToCeiledInt(-100.5f));
  EXPECT_EQ(0, gfx::ToCeiledInt(0));
  EXPECT_EQ(101, gfx::ToCeiledInt(100.5f));

  EXPECT_EQ(min, gfx::ToCeiledInt(-std::numeric_limits<float>::infinity()));
  EXPECT_EQ(min, gfx::ToCeiledInt(min));
  EXPECT_EQ(min, gfx::ToCeiledInt(min - 0.5f));
  EXPECT_EQ(min + 1, gfx::ToCeiledInt(min + 0.5f));
}

TEST(SafeIntegerConversions, ToRoundedInt) {
  EXPECT_EQ(0, gfx::ToRoundedInt(std::numeric_limits<float>::quiet_NaN()));

  float max = std::numeric_limits<int>::max();
  float min = std::numeric_limits<int>::min();

  EXPECT_EQ(max, gfx::ToRoundedInt(std::numeric_limits<float>::infinity()));
  EXPECT_EQ(max, gfx::ToRoundedInt(max));
  EXPECT_EQ(max, gfx::ToRoundedInt(max + 0.1f));
  EXPECT_EQ(max, gfx::ToRoundedInt(max + 0.5f));
  EXPECT_EQ(max, gfx::ToRoundedInt(max + 0.9f));
  EXPECT_EQ(max, gfx::ToRoundedInt(max - 0.1f));
  EXPECT_EQ(max, gfx::ToRoundedInt(max - 0.5f));
  EXPECT_EQ(max - 1, gfx::ToRoundedInt(max - 0.9f));

  EXPECT_EQ(-100, gfx::ToRoundedInt(-100.1f));
  EXPECT_EQ(-101, gfx::ToRoundedInt(-100.5f));
  EXPECT_EQ(-101, gfx::ToRoundedInt(-100.9f));
  EXPECT_EQ(0, gfx::ToRoundedInt(0));
  EXPECT_EQ(100, gfx::ToRoundedInt(100.1f));
  EXPECT_EQ(101, gfx::ToRoundedInt(100.5f));
  EXPECT_EQ(101, gfx::ToRoundedInt(100.9f));

  EXPECT_EQ(min, gfx::ToRoundedInt(-std::numeric_limits<float>::infinity()));
  EXPECT_EQ(min, gfx::ToRoundedInt(min));
  EXPECT_EQ(min, gfx::ToRoundedInt(min - 0.1f));
  EXPECT_EQ(min, gfx::ToRoundedInt(min - 0.5f));
  EXPECT_EQ(min, gfx::ToRoundedInt(min - 0.9f));
  EXPECT_EQ(min, gfx::ToRoundedInt(min + 0.1f));
  EXPECT_EQ(min + 1, gfx::ToRoundedInt(min + 0.5f));
  EXPECT_EQ(min + 1, gfx::ToRoundedInt(min + 0.9f));
}

}  // namespace ui
