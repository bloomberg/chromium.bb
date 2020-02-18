// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "base/stl_util.h"
#include "base/test/simple_test_tick_clock.h"
#include "remoting/base/rate_counter.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

static const int64_t kTestValues[] = { 10, 20, 30, 10, 25, 16, 15 };

// One second window and one sample per second, so rate equals each sample.
TEST(RateCounterTest, OneSecondWindow) {
  RateCounter rate_counter(base::TimeDelta::FromSeconds(1));
  EXPECT_EQ(0, rate_counter.Rate());

  base::SimpleTestTickClock tick_clock;
  rate_counter.set_tick_clock_for_tests(&tick_clock);

  for (size_t i = 0; i < base::size(kTestValues); ++i) {
    tick_clock.Advance(base::TimeDelta::FromSeconds(1));
    rate_counter.Record(kTestValues[i]);
    EXPECT_EQ(static_cast<double>(kTestValues[i]), rate_counter.Rate());
  }
}

// Record all samples instantaneously, so the rate is the total of the samples.
TEST(RateCounterTest, OneSecondWindowAllSamples) {
  RateCounter rate_counter(base::TimeDelta::FromSeconds(1));
  EXPECT_EQ(0, rate_counter.Rate());

  base::SimpleTestTickClock tick_clock;
  rate_counter.set_tick_clock_for_tests(&tick_clock);

  double expected = 0.0;
  for (size_t i = 0; i < base::size(kTestValues); ++i) {
    rate_counter.Record(kTestValues[i]);
    expected += kTestValues[i];
  }

  EXPECT_EQ(expected, rate_counter.Rate());
}

// Two second window, one sample per second.  For all but the first sample, the
// rate should be the average of it and the preceding one.  For the first it
// will be the average of the sample with zero.
TEST(RateCounterTest, TwoSecondWindow) {
  RateCounter rate_counter(base::TimeDelta::FromSeconds(2));
  EXPECT_EQ(0, rate_counter.Rate());

  base::SimpleTestTickClock tick_clock;
  rate_counter.set_tick_clock_for_tests(&tick_clock);

  for (size_t i = 0; i < base::size(kTestValues); ++i) {
    tick_clock.Advance(base::TimeDelta::FromSeconds(1));
    rate_counter.Record(kTestValues[i]);
    double expected = kTestValues[i];
    if (i > 0)
      expected += kTestValues[i-1];
    expected /= 2;
    EXPECT_EQ(expected, rate_counter.Rate());
  }
}

// Sample over a window one second shorter than the number of samples.
// Rate should be the average of all but the first sample.
TEST(RateCounterTest, LongWindow) {
  const size_t kWindowSeconds = base::size(kTestValues) - 1;

  RateCounter rate_counter(base::TimeDelta::FromSeconds(kWindowSeconds));
  EXPECT_EQ(0, rate_counter.Rate());

  base::SimpleTestTickClock tick_clock;
  rate_counter.set_tick_clock_for_tests(&tick_clock);

  double expected = 0.0;
  for (size_t i = 0; i < base::size(kTestValues); ++i) {
    tick_clock.Advance(base::TimeDelta::FromSeconds(1));
    rate_counter.Record(kTestValues[i]);
    if (i != 0)
      expected += kTestValues[i];
  }
  expected /= kWindowSeconds;

  EXPECT_EQ(expected, rate_counter.Rate());
}

}  // namespace remoting
