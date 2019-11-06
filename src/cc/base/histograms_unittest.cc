// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/base/histograms.h"

#include <limits>

#include "testing/gtest/include/gtest/gtest.h"

using base::TimeDelta;
using Sample = base::HistogramBase::Sample;

namespace cc {

class ScopedUMAHistogramAreaTimerBaseTest : public ::testing::Test {
 protected:
  void ExpectValidHistogramValues(base::TimeDelta elapsed,
                                  int area,
                                  Sample expected_time_microseconds,
                                  Sample expected_pixels_per_ms) {
    Sample time_microseconds;
    Sample pixels_per_ms;
    ScopedUMAHistogramAreaTimerBase::GetHistogramValues(
        elapsed, area, &time_microseconds, &pixels_per_ms);
    EXPECT_EQ(expected_time_microseconds, time_microseconds);
    EXPECT_EQ(expected_pixels_per_ms, pixels_per_ms);
  }
};

namespace {

TEST_F(ScopedUMAHistogramAreaTimerBaseTest, CommonCase) {
  ExpectValidHistogramValues(TimeDelta::FromMicroseconds(500), 1000, 500, 2000);
  ExpectValidHistogramValues(TimeDelta::FromMicroseconds(300), 1000, 300, 3333);
}

TEST_F(ScopedUMAHistogramAreaTimerBaseTest, ZeroArea) {
  ExpectValidHistogramValues(TimeDelta::FromMicroseconds(500), 0, 500, 0);
}

TEST_F(ScopedUMAHistogramAreaTimerBaseTest, ZeroTime) {
  // 1M pixels/ms, since the time is limited to at least 1us.
  ExpectValidHistogramValues(TimeDelta(), 1000, 1, 1000000);
}

TEST_F(ScopedUMAHistogramAreaTimerBaseTest, ZeroTimeAndArea) {
  ExpectValidHistogramValues(TimeDelta(), 0, 1, 0);
}

TEST_F(ScopedUMAHistogramAreaTimerBaseTest, VeryLargeTime) {
  ExpectValidHistogramValues(TimeDelta::FromHours(24), 1000,
                             std::numeric_limits<Sample>::max(), 0);
}

TEST_F(ScopedUMAHistogramAreaTimerBaseTest, VeryLargeArea) {
  ExpectValidHistogramValues(TimeDelta::FromMicroseconds(500), 1000000000, 500,
                             2000000000);
  ExpectValidHistogramValues(TimeDelta::FromMicroseconds(1000),
                             std::numeric_limits<int>::max(), 1000,
                             std::numeric_limits<Sample>::max());
}

}  // namespace
}  // namespace cc
