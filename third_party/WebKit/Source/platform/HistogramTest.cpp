// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/Histogram.h"

#include "base/metrics/histogram_samples.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

class ScopedUsHistogramTimerTest : public ::testing::Test {
 public:
  static void AdvanceClock(double microseconds) {
    time_elapsed_ += microseconds;
  }

 protected:
  static double ReturnMockTime() { return time_elapsed_; }

  virtual void SetUp() {
    time_elapsed_ = 0.0;
    original_time_function_ = SetTimeFunctionsForTesting(ReturnMockTime);
  }

  virtual void TearDown() {
    SetTimeFunctionsForTesting(original_time_function_);
  }

 private:
  static double time_elapsed_;
  TimeFunction original_time_function_;
};

double ScopedUsHistogramTimerTest::time_elapsed_;

class TestCustomCountHistogram : public CustomCountHistogram {
 public:
  TestCustomCountHistogram(const char* name,
                           base::HistogramBase::Sample min,
                           base::HistogramBase::Sample max,
                           int32_t bucket_count)
      : CustomCountHistogram(name, min, max, bucket_count) {}

  base::HistogramBase* Histogram() { return histogram_; }
};

TEST_F(ScopedUsHistogramTimerTest, Basic) {
  TestCustomCountHistogram scoped_us_counter("test", 0, 10000000, 50);
  {
    ScopedUsHistogramTimer timer(scoped_us_counter);
    AdvanceClock(0.5);
  }
  // 0.5s == 500000us
  EXPECT_EQ(500000, scoped_us_counter.Histogram()->SnapshotSamples()->sum());
}

}  // namespace blink
