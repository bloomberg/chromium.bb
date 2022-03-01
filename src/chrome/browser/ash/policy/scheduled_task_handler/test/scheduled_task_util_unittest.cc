// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/scheduled_task_handler/scheduled_task_util.h"
#include <memory>

#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "base/time/time_delta_from_string.h"
#include "chrome/browser/ash/policy/scheduled_task_handler/scheduled_task_executor.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/icu/source/common/unicode/unistr.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"

namespace policy {

namespace {

base::Time TimeFromUtcString(const char* time) {
  base::Time delayult;
  bool success = base::Time::FromUTCString(time, &delayult);
  CHECK(success);
  return delayult;
}

std::unique_ptr<icu::TimeZone> GetUtcTimeZone() {
  return base::WrapUnique(
      icu::TimeZone::createTimeZone(icu::UnicodeString::fromUTF8("UTC")));
}

}  // namespace

using scheduled_task_util::CalculateNextScheduledTaskTimerDelay;

TEST(ScheduledTaskUtilTest, DailyTaskShouldBeScheduledInSameDay) {
  ScheduledTaskExecutor::ScheduledTaskData data;
  data.frequency = ScheduledTaskExecutor::Frequency::kDaily;
  data.hour = 12;
  data.minute = 52;

  absl::optional<base::TimeDelta> delay = CalculateNextScheduledTaskTimerDelay(
      data, TimeFromUtcString("Jan 3 2021, 09:33"), *GetUtcTimeZone());

  ASSERT_TRUE(delay.has_value());
  EXPECT_EQ(delay.value(), base::TimeDeltaFromString("3h19m"));
}

TEST(ScheduledTaskUtilTest, TaskShouldBeDelayedIfTimesMatch) {
  ScheduledTaskExecutor::ScheduledTaskData data;
  data.frequency = ScheduledTaskExecutor::Frequency::kDaily;
  data.hour = 12;
  data.minute = 52;

  absl::optional<base::TimeDelta> delay = CalculateNextScheduledTaskTimerDelay(
      data, TimeFromUtcString("Jan 3 2021, 12:52"), *GetUtcTimeZone());

  ASSERT_TRUE(delay.has_value());
  EXPECT_EQ(delay.value(), base::TimeDeltaFromString("24h"));
}

TEST(ScheduledTaskUtilTest, DailyTaskShouldBeScheduledNextDay) {
  ScheduledTaskExecutor::ScheduledTaskData data;
  data.frequency = ScheduledTaskExecutor::Frequency::kDaily;
  data.hour = 8;
  data.minute = 52;

  absl::optional<base::TimeDelta> delay = CalculateNextScheduledTaskTimerDelay(
      data, TimeFromUtcString("Jan 3 2021, 09:33"), *GetUtcTimeZone());

  ASSERT_TRUE(delay.has_value());
  EXPECT_EQ(delay.value(), base::TimeDeltaFromString("23h19m"));
}

TEST(ScheduledTaskUtilTest,
     DailyTaskShouldBeScheduledNextDayAcrossMonthBoundary) {
  ScheduledTaskExecutor::ScheduledTaskData data;
  data.frequency = ScheduledTaskExecutor::Frequency::kDaily;
  data.hour = 8;
  data.minute = 52;

  absl::optional<base::TimeDelta> delay = CalculateNextScheduledTaskTimerDelay(
      data, TimeFromUtcString("Jan 31 2021, 09:33"), *GetUtcTimeZone());

  ASSERT_TRUE(delay.has_value());
  EXPECT_EQ(delay.value(), base::TimeDeltaFromString("23h19m"));
}

TEST(ScheduledTaskUtilTest, WeeklyTaskShouldBeScheduled) {
  ScheduledTaskExecutor::ScheduledTaskData data;
  data.frequency = ScheduledTaskExecutor::Frequency::kWeekly;
  data.hour = 11;
  data.minute = 52;
  data.day_of_week = UCAL_TUESDAY;

  absl::optional<base::TimeDelta> delay = CalculateNextScheduledTaskTimerDelay(
      data, TimeFromUtcString("Sunday Jan 3 2021, 09:33"), *GetUtcTimeZone());

  ASSERT_TRUE(delay.has_value());
  EXPECT_EQ(delay.value(), base::TimeDeltaFromString("50h19m"));  // 2d2h19m
}

TEST(ScheduledTaskUtilTest, MonthlyTaskShouldBeScheduled) {
  ScheduledTaskExecutor::ScheduledTaskData data;
  data.frequency = ScheduledTaskExecutor::Frequency::kMonthly;
  data.hour = 11;
  data.minute = 52;
  data.day_of_month = 23;

  absl::optional<base::TimeDelta> delay = CalculateNextScheduledTaskTimerDelay(
      data, TimeFromUtcString("Jan 3 2021, 09:33"), *GetUtcTimeZone());

  ASSERT_TRUE(delay.has_value());
  EXPECT_EQ(delay.value(), base::TimeDeltaFromString("482h19m"));  // 20d2h19m
}

TEST(ScheduledTaskUtilTest, MonthlyTaskShouldBeScheduledForNextMonth) {
  ScheduledTaskExecutor::ScheduledTaskData data;
  data.frequency = ScheduledTaskExecutor::Frequency::kMonthly;
  data.hour = 11;
  data.minute = 52;
  data.day_of_month = 23;

  absl::optional<base::TimeDelta> delay = CalculateNextScheduledTaskTimerDelay(
      data, TimeFromUtcString("Jan 31 2021, 09:33"), *GetUtcTimeZone());

  ASSERT_TRUE(delay.has_value());
  EXPECT_EQ(delay.value(), base::TimeDeltaFromString("554h19m"));  // 23d2h19m
}

TEST(ScheduledTaskUtilTest,
     MonthlyTaskShouldBeScheduledForEndOfMonthIfMonthShorter) {
  ScheduledTaskExecutor::ScheduledTaskData data;
  data.frequency = ScheduledTaskExecutor::Frequency::kMonthly;
  data.hour = 8;
  data.minute = 52;
  data.day_of_month = 31;

  absl::optional<base::TimeDelta> delay = CalculateNextScheduledTaskTimerDelay(
      data, TimeFromUtcString("Sunday Jan 31 2021, 09:33"), *GetUtcTimeZone());

  ASSERT_TRUE(delay.has_value());
  EXPECT_EQ(delay.value(), base::TimeDeltaFromString("671h19m"));  // 27d23h19m
}

}  // namespace policy
