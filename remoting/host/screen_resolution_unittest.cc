// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/screen_resolution.h"

#include <limits>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

TEST(ScreenResolutionTest, Empty) {
  ScreenResolution resolution;
  EXPECT_TRUE(resolution.IsEmpty());

  resolution.dimensions_.set(-1, 0);
  EXPECT_TRUE(resolution.IsEmpty());

  resolution.dimensions_.set(0, -1);
  EXPECT_TRUE(resolution.IsEmpty());

  resolution.dimensions_.set(-1, -1);
  EXPECT_TRUE(resolution.IsEmpty());

  resolution.dpi_.set(-1, 0);
  EXPECT_TRUE(resolution.IsEmpty());

  resolution.dpi_.set(0, -1);
  EXPECT_TRUE(resolution.IsEmpty());

  resolution.dpi_.set(-1, -1);
  EXPECT_TRUE(resolution.IsEmpty());
}

TEST(ScreenResolutionTest, Invalid) {
  ScreenResolution resolution;
  EXPECT_TRUE(resolution.IsValid());

  resolution.dimensions_.set(-1, 0);
  EXPECT_FALSE(resolution.IsValid());

  resolution.dimensions_.set(0, -1);
  EXPECT_FALSE(resolution.IsValid());

  resolution.dimensions_.set(-1, -1);
  EXPECT_FALSE(resolution.IsValid());

  resolution.dpi_.set(-1, 0);
  EXPECT_FALSE(resolution.IsValid());

  resolution.dpi_.set(0, -1);
  EXPECT_FALSE(resolution.IsValid());

  resolution.dpi_.set(-1, -1);
  EXPECT_FALSE(resolution.IsValid());
}

TEST(ScreenResolutionTest, Scaling) {
  ScreenResolution resolution(
      SkISize::Make(100, 100), SkIPoint::Make(10, 10));

  EXPECT_EQ(resolution.ScaleDimensionsToDpi(SkIPoint::Make(5, 5)),
            SkISize::Make(50, 50));

  EXPECT_EQ(resolution.ScaleDimensionsToDpi(SkIPoint::Make(20, 20)),
            SkISize::Make(200, 200));
}

TEST(ScreenResolutionTest, ScalingSaturation) {
  ScreenResolution resolution(
      SkISize::Make(10000000, 1000000), SkIPoint::Make(1, 1));

  EXPECT_EQ(resolution.ScaleDimensionsToDpi(SkIPoint::Make(1000000, 1000000)),
            SkISize::Make(std::numeric_limits<int32>::max(),
                          std::numeric_limits<int32>::max()));
}

}  // namespace remoting
