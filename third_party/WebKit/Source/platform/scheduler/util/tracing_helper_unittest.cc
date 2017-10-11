// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/scheduler/util/tracing_helper.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace scheduler {

namespace {

struct {} g_object;

const char* SignOfInt(int value) {
  if (value > 0) {
    return "positive";
  } else if (value < 0) {
    return "negative";
  } else {
    return "zero";
  }
}

}  // namespace

// TODO(kraynov): Tracing tests.

TEST(TracingHelperTest, Operators) {
  TraceableState<int, kTracingCategoryNameDebug> x(
      -1, "X", &g_object, SignOfInt);
  TraceableState<int, kTracingCategoryNameDebug> y(
      1, "Y", &g_object, SignOfInt);
  EXPECT_EQ(0, x + y);
  EXPECT_FALSE(x == y);
  EXPECT_TRUE(x != y);
  x = 1;
  EXPECT_EQ(0, y - x);
  EXPECT_EQ(2, x + y);
  EXPECT_TRUE(x == y);
  EXPECT_FALSE(x != y);
  EXPECT_TRUE((x + y) != 3);
  EXPECT_TRUE((2 - y + 1 + x) == 3);
  x = 3;
  y = 2;
  int z = x = y;
  EXPECT_EQ(2, z);
}

}  // namespace scheduler
}  // namespace blink
