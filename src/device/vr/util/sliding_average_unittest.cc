// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/util/sliding_average.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace device {

TEST(SlidingAverage, Basics) {
  SlidingAverage meter(5);

  // No values yet
  EXPECT_EQ(42, meter.GetAverageOrDefault(42));
  EXPECT_EQ(0, meter.GetAverage());

  meter.AddSample(100);
  EXPECT_EQ(100, meter.GetAverageOrDefault(42));
  EXPECT_EQ(100, meter.GetAverage());

  meter.AddSample(200);
  EXPECT_EQ(150, meter.GetAverage());

  meter.AddSample(10);
  EXPECT_EQ(103, meter.GetAverage());

  meter.AddSample(10);
  EXPECT_EQ(80, meter.GetAverage());

  meter.AddSample(10);
  EXPECT_EQ(66, meter.GetAverage());

  meter.AddSample(10);
  EXPECT_EQ(48, meter.GetAverage());

  meter.AddSample(10);
  EXPECT_EQ(10, meter.GetAverage());

  meter.AddSample(110);
  EXPECT_EQ(30, meter.GetAverage());
}

TEST(SlidingTimeDeltaAverage, Basics) {
  SlidingTimeDeltaAverage meter(5);

  EXPECT_EQ(base::TimeDelta::FromSeconds(42),
            meter.GetAverageOrDefault(base::TimeDelta::FromSeconds(42)));
  EXPECT_EQ(base::TimeDelta(), meter.GetAverage());

  meter.AddSample(base::TimeDelta::FromSeconds(100));
  EXPECT_EQ(base::TimeDelta::FromSeconds(100),
            meter.GetAverageOrDefault(base::TimeDelta::FromSeconds(42)));
  EXPECT_EQ(base::TimeDelta::FromSeconds(100), meter.GetAverage());

  meter.AddSample(base::TimeDelta::FromMilliseconds(200000));
  EXPECT_EQ(base::TimeDelta::FromSeconds(150), meter.GetAverage());
}

}  // namespace device
