// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/histogram.h"

#include "base/metrics/histogram_samples.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/testing/wtf/scoped_mock_clock.h"
#include "third_party/blink/renderer/platform/wtf/time.h"

namespace blink {

class TestCustomCountHistogram : public CustomCountHistogram {
 public:
  TestCustomCountHistogram(const char* name,
                           base::HistogramBase::Sample min,
                           base::HistogramBase::Sample max,
                           int32_t bucket_count)
      : CustomCountHistogram(name, min, max, bucket_count) {}

  base::HistogramBase* Histogram() { return histogram_; }
};

TEST(ScopedUsHistogramTimerTest, Basic) {
  TestCustomCountHistogram scoped_us_counter("ScopedUsHistogramTimerTest.Basic",
                                             0, 10000000, 50);
  {
    WTF::ScopedMockClock clock;
    ScopedUsHistogramTimer timer(scoped_us_counter);
    clock.Advance(TimeDelta::FromMilliseconds(500));
  }
  // 500ms == 500000us
  EXPECT_EQ(500000, scoped_us_counter.Histogram()->SnapshotSamples()->sum());
}

TEST(ScopedHighResUsHistogramTimerTest, Basic) {
  TestCustomCountHistogram scoped_us_counter(
      "ScopedHighResUsHistogramTimerTest.Basic", 0, 10000000, 50);
  {
    WTF::ScopedMockClock clock;
    ScopedHighResUsHistogramTimer timer(scoped_us_counter);
    clock.Advance(TimeDelta::FromMilliseconds(500));
  }
  int64_t expected = TimeTicks::IsHighResolution() ? 500000 : 0;
  EXPECT_EQ(expected, scoped_us_counter.Histogram()->SnapshotSamples()->sum());
}

}  // namespace blink
