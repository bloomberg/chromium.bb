// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/engine/throttled_data_type_tracker.h"

#include "sync/internal_api/public/syncable/model_type.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::TimeDelta;
using base::TimeTicks;

namespace browser_sync {

TEST(ThrottledDataTypeTrackerTest, AddUnthrottleTimeTest) {
  const syncable::ModelTypeSet types(syncable::BOOKMARKS, syncable::PASSWORDS);

  ThrottledDataTypeTracker throttler(NULL);
  TimeTicks now = TimeTicks::Now();
  throttler.SetUnthrottleTime(types, now);

  EXPECT_EQ(throttler.unthrottle_times_.size(), 2U);
  EXPECT_EQ(throttler.unthrottle_times_[syncable::BOOKMARKS], now);
  EXPECT_EQ(throttler.unthrottle_times_[syncable::PASSWORDS], now);
}

TEST(ThrottledDataTypeTrackerTest, GetCurrentlyThrottledTypesTest) {
  const syncable::ModelTypeSet types(syncable::BOOKMARKS, syncable::PASSWORDS);

  ThrottledDataTypeTracker throttler(NULL);
  TimeTicks now = TimeTicks::Now();

  // Now update the throttled types with time set to 10 seconds earlier from
  // now.
  throttler.SetUnthrottleTime(types, now - TimeDelta::FromSeconds(10));
  throttler.PruneUnthrottledTypes(TimeTicks::Now());
  EXPECT_TRUE(throttler.GetThrottledTypes().Empty());

  // Now update the throttled types with time set to 2 hours from now.
  throttler.SetUnthrottleTime(types, now + TimeDelta::FromSeconds(1200));
  throttler.PruneUnthrottledTypes(TimeTicks::Now());
  EXPECT_TRUE(throttler.GetThrottledTypes().Equals(types));
}

// Have two data types whose throttling is set to expire at different times.
TEST(ThrottledDataTypeTrackerTest, UnthrottleSomeTypesTest) {
  const syncable::ModelTypeSet long_throttled(syncable::BOOKMARKS);
  const syncable::ModelTypeSet short_throttled(syncable::PASSWORDS);

  const TimeTicks start_time = TimeTicks::Now();
  const TimeTicks short_throttle_time = start_time + TimeDelta::FromSeconds(1);
  const TimeTicks long_throttle_time = start_time + TimeDelta::FromSeconds(20);
  const TimeTicks in_between_time = start_time + TimeDelta::FromSeconds(5);

  ThrottledDataTypeTracker throttler(NULL);

  throttler.SetUnthrottleTime(long_throttled, long_throttle_time);
  throttler.SetUnthrottleTime(short_throttled, short_throttle_time);
  EXPECT_TRUE(throttler.GetThrottledTypes().Equals(
          Union(short_throttled, long_throttled)));

  throttler.PruneUnthrottledTypes(in_between_time);
  EXPECT_TRUE(throttler.GetThrottledTypes().Equals(long_throttled));
}

}  // namespace browser_sync

