// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/auto_screen_brightness/als_samples.h"

#include <cmath>

#include "base/logging.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace power {
namespace auto_screen_brightness {

TEST(AmbientLightSampleBufferTest, Basic) {
  base::SimpleTestTickClock tick_clock;
  AmbientLightSampleBuffer buffer(base::TimeDelta::FromSeconds(5));
  double sum = 0;
  for (int i = 1; i < 6; ++i) {
    tick_clock.Advance(base::TimeDelta::FromSeconds(1));
    const AmbientLightSampleBuffer::Sample sample = {i, tick_clock.NowTicks()};
    sum += i;
    buffer.SaveToBuffer(sample);
    EXPECT_EQ(buffer.NumberOfSamplesForTesting(), static_cast<size_t>(i));
    EXPECT_DOUBLE_EQ(buffer.AverageAmbient(tick_clock.NowTicks()).value(),
                     sum / i);
    EXPECT_EQ(buffer.NumberOfSamplesForTesting(), static_cast<size_t>(i));
  }

  // Add another two items that will push out the oldest.
  tick_clock.Advance(base::TimeDelta::FromSeconds(1));
  buffer.SaveToBuffer({10, tick_clock.NowTicks()});
  sum = sum - 1 + 10;
  EXPECT_EQ(buffer.NumberOfSamplesForTesting(), 5u);
  EXPECT_DOUBLE_EQ(buffer.AverageAmbient(tick_clock.NowTicks()).value(),
                   sum / 5);
  EXPECT_EQ(buffer.NumberOfSamplesForTesting(), 5u);

  tick_clock.Advance(base::TimeDelta::FromSeconds(1));
  buffer.SaveToBuffer({20, tick_clock.NowTicks()});
  sum = sum - 2 + 20;
  EXPECT_EQ(buffer.NumberOfSamplesForTesting(), 5u);
  EXPECT_DOUBLE_EQ(buffer.AverageAmbient(tick_clock.NowTicks()).value(),
                   sum / 5);
  EXPECT_EQ(buffer.NumberOfSamplesForTesting(), 5u);

  // Add another item but it doesn't push out the oldest.
  tick_clock.Advance(base::TimeDelta::FromMilliseconds(1));
  buffer.SaveToBuffer({100, tick_clock.NowTicks()});
  sum += 100;
  EXPECT_EQ(buffer.NumberOfSamplesForTesting(), 6u);
  EXPECT_DOUBLE_EQ(buffer.AverageAmbient(tick_clock.NowTicks()).value(),
                   sum / 6);
  EXPECT_EQ(buffer.NumberOfSamplesForTesting(), 6u);
}

TEST(AmbientLightSampleBufferTest, LargeSampleTimeGap) {
  base::SimpleTestTickClock tick_clock;
  AmbientLightSampleBuffer buffer(base::TimeDelta::FromSeconds(5));
  tick_clock.Advance(base::TimeDelta::FromSeconds(1));
  const AmbientLightSampleBuffer::Sample sample = {10, tick_clock.NowTicks()};
  buffer.SaveToBuffer(sample);
  EXPECT_EQ(buffer.NumberOfSamplesForTesting(), 1u);
  EXPECT_DOUBLE_EQ(buffer.AverageAmbient(tick_clock.NowTicks()).value(), 10.0);
  EXPECT_EQ(buffer.NumberOfSamplesForTesting(), 1u);

  // Another samples arrives sufficiently late so the 1st sample is pushed out.
  tick_clock.Advance(base::TimeDelta::FromSeconds(5));
  buffer.SaveToBuffer({20, tick_clock.NowTicks()});
  EXPECT_EQ(buffer.NumberOfSamplesForTesting(), 1u);
  EXPECT_DOUBLE_EQ(buffer.AverageAmbient(tick_clock.NowTicks()).value(), 20);
  EXPECT_EQ(buffer.NumberOfSamplesForTesting(), 1u);
}

TEST(AmbientLightSampleBufferTest, AverageTimeTooLate) {
  base::SimpleTestTickClock tick_clock;
  AmbientLightSampleBuffer buffer(base::TimeDelta::FromSeconds(5));
  tick_clock.Advance(base::TimeDelta::FromSeconds(1));
  const AmbientLightSampleBuffer::Sample sample = {10, tick_clock.NowTicks()};
  buffer.SaveToBuffer(sample);
  EXPECT_EQ(buffer.NumberOfSamplesForTesting(), 1u);
  EXPECT_DOUBLE_EQ(buffer.AverageAmbient(tick_clock.NowTicks()).value(), 10.0);
  EXPECT_EQ(buffer.NumberOfSamplesForTesting(), 1u);

  // When average is calculated, all samples are too old, hence average is
  // nullopt.
  tick_clock.Advance(base::TimeDelta::FromSeconds(5));
  EXPECT_EQ(buffer.NumberOfSamplesForTesting(), 1u);
  EXPECT_FALSE(buffer.AverageAmbient(tick_clock.NowTicks()).has_value());
  EXPECT_EQ(buffer.NumberOfSamplesForTesting(), 0u);
}

}  // namespace auto_screen_brightness
}  // namespace power
}  // namespace chromeos
