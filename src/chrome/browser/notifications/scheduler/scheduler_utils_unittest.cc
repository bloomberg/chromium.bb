// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/scheduler_utils.h"

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace notifications {
namespace {

// Verifies we can get the correct time stamp at certain hour in yesterday.
TEST(SchedulerUtilsTest, ToLocalHour) {
  base::Time today, another_day, expected;

  // Timestamp of another day in the past.
  EXPECT_TRUE(base::Time::FromString("10/15/07 12:45:12 PM", &today));
  EXPECT_TRUE(ToLocalHour(6, today, -1, &another_day));
  EXPECT_TRUE(base::Time::FromString("10/14/07 06:00:00 AM", &expected));
  EXPECT_EQ(expected, another_day);

  EXPECT_TRUE(base::Time::FromString("03/25/19 00:00:00 AM", &today));
  EXPECT_TRUE(ToLocalHour(0, today, -1, &another_day));
  EXPECT_TRUE(base::Time::FromString("03/24/19 00:00:00 AM", &expected));
  EXPECT_EQ(expected, another_day);

  // Timestamp of the same day.
  EXPECT_TRUE(base::Time::FromString("03/25/19 00:00:00 AM", &today));
  EXPECT_TRUE(ToLocalHour(0, today, 0, &another_day));
  EXPECT_TRUE(base::Time::FromString("03/25/19 00:00:00 AM", &expected));
  EXPECT_EQ(expected, another_day);

  // Timestamp of another day in the future.
  EXPECT_TRUE(base::Time::FromString("03/25/19 06:35:27 AM", &today));
  EXPECT_TRUE(ToLocalHour(16, today, 7, &another_day));
  EXPECT_TRUE(base::Time::FromString("04/01/19 16:00:00 PM", &expected));
  EXPECT_EQ(expected, another_day);
}

}  // namespace
}  // namespace notifications
