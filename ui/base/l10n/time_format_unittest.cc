// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/l10n/time_format.h"

#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"

using base::ASCIIToUTF16;

namespace ui {
namespace {

using base::TimeDelta;

void TestTimeFormats(const TimeDelta& delta, const char* expected_ascii) {
  base::string16 expected = ASCIIToUTF16(expected_ascii);
  base::string16 expected_left = expected + ASCIIToUTF16(" left");
  base::string16 expected_ago = expected + ASCIIToUTF16(" ago");
  EXPECT_EQ(expected, TimeFormat::TimeRemainingShort(delta));
  EXPECT_EQ(expected_left, TimeFormat::TimeRemaining(delta));
  EXPECT_EQ(expected_ago, TimeFormat::TimeElapsed(delta));
}

TEST(TimeFormat, FormatTime) {
  const TimeDelta one_day = TimeDelta::FromDays(1);
  const TimeDelta one_hour = TimeDelta::FromHours(1);
  const TimeDelta one_min = TimeDelta::FromMinutes(1);
  const TimeDelta one_second = TimeDelta::FromSeconds(1);
  const TimeDelta one_millisecond = TimeDelta::FromMilliseconds(1);
  const TimeDelta zero = TimeDelta::FromMilliseconds(0);

  // TODO(jungshik) : These test only pass when the OS locale is 'en'.
  // We need to add SetUp() and TearDown() to set the locale to 'en'.
  TestTimeFormats(zero, "0 secs");
  TestTimeFormats(499 * one_millisecond, "0 secs");
  TestTimeFormats(500 * one_millisecond, "1 sec");
  TestTimeFormats(1 * one_second + 499 * one_millisecond, "1 sec");
  TestTimeFormats(1 * one_second + 500 * one_millisecond, "2 secs");
  TestTimeFormats(59 * one_second + 499 * one_millisecond, "59 secs");
  TestTimeFormats(59 * one_second + 500 * one_millisecond, "1 min");
  TestTimeFormats(1 * one_min + 30 * one_second - one_millisecond, "1 min");
  TestTimeFormats(1 * one_min + 30 * one_second, "2 mins");
  TestTimeFormats(59 * one_min + 30 * one_second - one_millisecond, "59 mins");
  TestTimeFormats(59 * one_min + 30 * one_second, "1 hour");
  TestTimeFormats(1 * one_hour + 30 * one_min - one_millisecond, "1 hour");
  TestTimeFormats(1 * one_hour + 30 * one_min, "2 hours");
  TestTimeFormats(23 * one_hour + 30 * one_min - one_millisecond, "23 hours");
  TestTimeFormats(23 * one_hour + 30 * one_min, "1 day");
  TestTimeFormats(1 * one_day + 12 * one_hour - one_millisecond, "1 day");
  TestTimeFormats(1 * one_day + 12 * one_hour, "2 days");
}

// crbug.com/159388: This test fails when daylight savings time ends.
TEST(TimeFormat, RelativeDate) {
  base::Time now = base::Time::Now();
  base::string16 today_str = TimeFormat::RelativeDate(now, NULL);
  EXPECT_EQ(ASCIIToUTF16("Today"), today_str);

  base::Time yesterday = now - TimeDelta::FromDays(1);
  base::string16 yesterday_str = TimeFormat::RelativeDate(yesterday, NULL);
  EXPECT_EQ(ASCIIToUTF16("Yesterday"), yesterday_str);

  base::Time two_days_ago = now - TimeDelta::FromDays(2);
  base::string16 two_days_ago_str =
      TimeFormat::RelativeDate(two_days_ago, NULL);
  EXPECT_TRUE(two_days_ago_str.empty());

  base::Time a_week_ago = now - TimeDelta::FromDays(7);
  base::string16 a_week_ago_str = TimeFormat::RelativeDate(a_week_ago, NULL);
  EXPECT_TRUE(a_week_ago_str.empty());
}

}  // namespace
}  // namespace ui
