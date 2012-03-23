// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include "gestures/include/util.h"

namespace gestures {

class UtilTest : public ::testing::Test {};

TEST(UtilTest, DistSqTest) {
  FingerState fs[] = {
    // TM, Tm, WM, Wm, Press, Orientation, X, Y, TrID
    {0, 0, 0, 0, 1, 0, 1, 2, 1, 0},
    {0, 0, 0, 0, 1, 0, 4, 6, 1, 0}
  };
  EXPECT_FLOAT_EQ(DistSq(fs[0], fs[1]), 25);
  EXPECT_FLOAT_EQ(DistSqXY(fs[0], 4, 6), 25);
}

}  // namespace gestures
