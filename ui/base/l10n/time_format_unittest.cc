// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/l10n/time_format.h"

#include "base/files/file_path.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/formatter.h"

using base::ASCIIToUTF16;

namespace ui {
namespace {

using base::TimeDelta;

class TimeFormatTest : public ::testing::Test {
 public:
  TimeFormatTest()
      : delta_0s_(TimeDelta::FromSeconds(0)),
        delta_1ms_(TimeDelta::FromMilliseconds(1)),
        delta_499ms_(TimeDelta::FromMilliseconds(499)),
        delta_500ms_(TimeDelta::FromMilliseconds(500)),
        delta_999ms_(TimeDelta::FromMilliseconds(999)),
        delta_1s_(TimeDelta::FromSeconds(1)),
        delta_1s499ms_(delta_1s_ + delta_499ms_),
        delta_1s500ms_(delta_1s_ + delta_500ms_),
        delta_2s_(TimeDelta::FromSeconds(2)),
        delta_29s_(TimeDelta::FromSeconds(29)),
        delta_30s_(TimeDelta::FromSeconds(30)),
        delta_59s_(TimeDelta::FromSeconds(59)),
        delta_59s499ms_(delta_59s_ + delta_499ms_),
        delta_59s500ms_(delta_59s_ + delta_500ms_),
        delta_1m_(TimeDelta::FromMinutes(1)),
        delta_1m2s_(delta_1m_ + delta_2s_),
        delta_1m29s999ms_(delta_1m_ + delta_29s_ + delta_999ms_),
        delta_1m30s_(delta_1m_ + delta_30s_),
        delta_2m_(TimeDelta::FromMinutes(2)),
        delta_2m1s_(delta_2m_ + delta_1s_),
        delta_29m_(TimeDelta::FromMinutes(29)),
        delta_30m_(TimeDelta::FromMinutes(30)),
        delta_59m_(TimeDelta::FromMinutes(59)),
        delta_59m29s999ms_(delta_59m_ + delta_29s_ + delta_999ms_),
        delta_59m30s_(delta_59m_ + delta_30s_),
        delta_59m59s499ms_(delta_59m_ + delta_59s_ + delta_499ms_),
        delta_59m59s500ms_(delta_59m_ + delta_59s_ + delta_500ms_),
        delta_1h_(TimeDelta::FromHours(1)),
        delta_1h2m_(delta_1h_ + delta_2m_),
        delta_1h29m59s999ms_(delta_1h_ + delta_29m_ + delta_59s_ +
                             delta_999ms_),
        delta_1h30m_(delta_1h_ + delta_30m_),
        delta_2h_(TimeDelta::FromHours(2)),
        delta_2h1m_(delta_2h_ + delta_1m_),
        delta_11h_(TimeDelta::FromHours(11)),
        delta_12h_(TimeDelta::FromHours(12)),
        delta_23h_(TimeDelta::FromHours(23)),
        delta_23h29m59s999ms_(delta_23h_ + delta_29m_ + delta_59s_ +
                              delta_999ms_),
        delta_23h30m_(delta_23h_ + delta_30m_),
        delta_23h59m29s999ms_(delta_23h_ + delta_59m_ + delta_29s_ +
                              delta_999ms_),
        delta_23h59m30s_(delta_23h_ + delta_59m_ + delta_30s_),
        delta_1d_(TimeDelta::FromDays(1)),
        delta_1d2h_(delta_1d_ + delta_2h_),
        delta_1d11h59m59s999ms_(
            delta_1d_ + delta_11h_ + delta_59m_ + delta_29s_ + delta_999ms_),
        delta_1d12h_(delta_1d_ + delta_12h_),
        delta_2d_(TimeDelta::FromDays(2)),
        delta_2d1h_(delta_2d_ + delta_1h_),
        delta_1y_(delta_1d_ * 365),
        delta_2y_(delta_1y_ * 2),
        delta_1mo_(delta_1y_ / 12),
        delta_2mo_(delta_1mo_ * 2),
        delta_1mo10d_(delta_1mo_ + delta_1d_ * 10) {}

 protected:
  void TestStrings() {
    // Test English strings (simple, singular).
    EXPECT_EQ(ASCIIToUTF16("1 sec"), TimeFormat::Simple(
        TimeFormat::FORMAT_DURATION, TimeFormat::LENGTH_SHORT, delta_1s_));
    EXPECT_EQ(ASCIIToUTF16("1 min"), TimeFormat::Simple(
        TimeFormat::FORMAT_DURATION, TimeFormat::LENGTH_SHORT, delta_1m_));
    EXPECT_EQ(ASCIIToUTF16("1 hour"), TimeFormat::Simple(
        TimeFormat::FORMAT_DURATION, TimeFormat::LENGTH_SHORT, delta_1h_));
    EXPECT_EQ(ASCIIToUTF16("1 day"), TimeFormat::Simple(
        TimeFormat::FORMAT_DURATION, TimeFormat::LENGTH_SHORT, delta_1d_));
    EXPECT_EQ(ASCIIToUTF16("1 second"), TimeFormat::Simple(
        TimeFormat::FORMAT_DURATION, TimeFormat::LENGTH_LONG, delta_1s_));
    EXPECT_EQ(ASCIIToUTF16("1 minute"), TimeFormat::Simple(
        TimeFormat::FORMAT_DURATION, TimeFormat::LENGTH_LONG, delta_1m_));
    EXPECT_EQ(ASCIIToUTF16("1 hour"), TimeFormat::Simple(
        TimeFormat::FORMAT_DURATION, TimeFormat::LENGTH_LONG, delta_1h_));
    EXPECT_EQ(ASCIIToUTF16("1 day"), TimeFormat::Simple(
        TimeFormat::FORMAT_DURATION, TimeFormat::LENGTH_LONG, delta_1d_));
    EXPECT_EQ(ASCIIToUTF16("1 month"),
              TimeFormat::SimpleWithMonthAndYear(TimeFormat::FORMAT_DURATION,
                                                 TimeFormat::LENGTH_LONG,
                                                 delta_1mo_, true));
    EXPECT_EQ(ASCIIToUTF16("1 month"),
              TimeFormat::SimpleWithMonthAndYear(TimeFormat::FORMAT_DURATION,
                                                 TimeFormat::LENGTH_LONG,
                                                 delta_1mo10d_, true));
    EXPECT_EQ(ASCIIToUTF16("1 year"),
              TimeFormat::SimpleWithMonthAndYear(TimeFormat::FORMAT_DURATION,
                                                 TimeFormat::LENGTH_LONG,
                                                 delta_1y_, true));
    EXPECT_EQ(ASCIIToUTF16("1 sec left"), TimeFormat::Simple(
        TimeFormat::FORMAT_REMAINING, TimeFormat::LENGTH_SHORT, delta_1s_));
    EXPECT_EQ(ASCIIToUTF16("1 min left"), TimeFormat::Simple(
        TimeFormat::FORMAT_REMAINING, TimeFormat::LENGTH_SHORT, delta_1m_));
    EXPECT_EQ(ASCIIToUTF16("1 hour left"), TimeFormat::Simple(
        TimeFormat::FORMAT_REMAINING, TimeFormat::LENGTH_SHORT, delta_1h_));
    EXPECT_EQ(ASCIIToUTF16("1 day left"), TimeFormat::Simple(
        TimeFormat::FORMAT_REMAINING, TimeFormat::LENGTH_SHORT, delta_1d_));
    EXPECT_EQ(ASCIIToUTF16("1 second left"), TimeFormat::Simple(
        TimeFormat::FORMAT_REMAINING, TimeFormat::LENGTH_LONG, delta_1s_));
    EXPECT_EQ(ASCIIToUTF16("1 minute left"), TimeFormat::Simple(
        TimeFormat::FORMAT_REMAINING, TimeFormat::LENGTH_LONG, delta_1m_));
    EXPECT_EQ(ASCIIToUTF16("1 hour left"), TimeFormat::Simple(
        TimeFormat::FORMAT_REMAINING, TimeFormat::LENGTH_LONG, delta_1h_));
    EXPECT_EQ(ASCIIToUTF16("1 day left"), TimeFormat::Simple(
        TimeFormat::FORMAT_REMAINING, TimeFormat::LENGTH_LONG, delta_1d_));
    EXPECT_EQ(ASCIIToUTF16("1 month left"),
              TimeFormat::SimpleWithMonthAndYear(TimeFormat::FORMAT_REMAINING,
                                                 TimeFormat::LENGTH_LONG,
                                                 delta_1mo_, true));
    EXPECT_EQ(ASCIIToUTF16("1 month left"),
              TimeFormat::SimpleWithMonthAndYear(TimeFormat::FORMAT_REMAINING,
                                                 TimeFormat::LENGTH_LONG,
                                                 delta_1mo10d_, true));
    EXPECT_EQ(ASCIIToUTF16("1 year left"),
              TimeFormat::SimpleWithMonthAndYear(TimeFormat::FORMAT_REMAINING,
                                                 TimeFormat::LENGTH_LONG,
                                                 delta_1y_, true));
    EXPECT_EQ(ASCIIToUTF16("1 sec ago"), TimeFormat::Simple(
        TimeFormat::FORMAT_ELAPSED, TimeFormat::LENGTH_SHORT, delta_1s_));
    EXPECT_EQ(ASCIIToUTF16("1 min ago"), TimeFormat::Simple(
        TimeFormat::FORMAT_ELAPSED, TimeFormat::LENGTH_SHORT, delta_1m_));
    EXPECT_EQ(ASCIIToUTF16("1 hour ago"), TimeFormat::Simple(
        TimeFormat::FORMAT_ELAPSED, TimeFormat::LENGTH_SHORT, delta_1h_));
    EXPECT_EQ(ASCIIToUTF16("1 day ago"), TimeFormat::Simple(
        TimeFormat::FORMAT_ELAPSED, TimeFormat::LENGTH_SHORT, delta_1d_));
    EXPECT_EQ(ASCIIToUTF16("1 month ago"),
              TimeFormat::SimpleWithMonthAndYear(TimeFormat::FORMAT_ELAPSED,
                                                 TimeFormat::LENGTH_LONG,
                                                 delta_1mo_, true));
    EXPECT_EQ(ASCIIToUTF16("1 month ago"),
              TimeFormat::SimpleWithMonthAndYear(TimeFormat::FORMAT_ELAPSED,
                                                 TimeFormat::LENGTH_LONG,
                                                 delta_1mo10d_, true));
    EXPECT_EQ(ASCIIToUTF16("1 year ago"),
              TimeFormat::SimpleWithMonthAndYear(TimeFormat::FORMAT_ELAPSED,
                                                 TimeFormat::LENGTH_LONG,
                                                 delta_1y_, true));
    EXPECT_EQ(ASCIIToUTF16("1 second ago"),
              TimeFormat::Simple(TimeFormat::FORMAT_ELAPSED,
                                 TimeFormat::LENGTH_LONG, delta_1s_));
    EXPECT_EQ(ASCIIToUTF16("1 minute ago"),
              TimeFormat::Simple(TimeFormat::FORMAT_ELAPSED,
                                 TimeFormat::LENGTH_LONG, delta_1m_));
    EXPECT_EQ(ASCIIToUTF16("1 hour ago"),
              TimeFormat::Simple(TimeFormat::FORMAT_ELAPSED,
                                 TimeFormat::LENGTH_LONG, delta_1h_));
    EXPECT_EQ(ASCIIToUTF16("1 day ago"),
              TimeFormat::Simple(TimeFormat::FORMAT_ELAPSED,
                                 TimeFormat::LENGTH_LONG, delta_1d_));

    // Test English strings (simple, plural).
    EXPECT_EQ(ASCIIToUTF16("2 secs"), TimeFormat::Simple(
        TimeFormat::FORMAT_DURATION, TimeFormat::LENGTH_SHORT, delta_2s_));
    EXPECT_EQ(ASCIIToUTF16("2 mins"), TimeFormat::Simple(
        TimeFormat::FORMAT_DURATION, TimeFormat::LENGTH_SHORT, delta_2m_));
    EXPECT_EQ(ASCIIToUTF16("2 hours"), TimeFormat::Simple(
        TimeFormat::FORMAT_DURATION, TimeFormat::LENGTH_SHORT, delta_2h_));
    EXPECT_EQ(ASCIIToUTF16("2 days"), TimeFormat::Simple(
        TimeFormat::FORMAT_DURATION, TimeFormat::LENGTH_SHORT, delta_2d_));
    EXPECT_EQ(ASCIIToUTF16("2 seconds"), TimeFormat::Simple(
        TimeFormat::FORMAT_DURATION, TimeFormat::LENGTH_LONG, delta_2s_));
    EXPECT_EQ(ASCIIToUTF16("2 minutes"), TimeFormat::Simple(
        TimeFormat::FORMAT_DURATION, TimeFormat::LENGTH_LONG, delta_2m_));
    EXPECT_EQ(ASCIIToUTF16("2 hours"), TimeFormat::Simple(
        TimeFormat::FORMAT_DURATION, TimeFormat::LENGTH_LONG, delta_2h_));
    EXPECT_EQ(ASCIIToUTF16("2 days"), TimeFormat::Simple(
        TimeFormat::FORMAT_DURATION, TimeFormat::LENGTH_LONG, delta_2d_));
    EXPECT_EQ(ASCIIToUTF16("30 days"),
              TimeFormat::Simple(TimeFormat::FORMAT_DURATION,
                                 TimeFormat::LENGTH_LONG, delta_1mo_));
    EXPECT_EQ(ASCIIToUTF16("365 days"),
              TimeFormat::Simple(TimeFormat::FORMAT_DURATION,
                                 TimeFormat::LENGTH_LONG, delta_1y_));
    EXPECT_EQ(ASCIIToUTF16("2 months"),
              TimeFormat::SimpleWithMonthAndYear(TimeFormat::FORMAT_DURATION,
                                                 TimeFormat::LENGTH_LONG,
                                                 delta_2mo_, true));
    EXPECT_EQ(ASCIIToUTF16("2 years"),
              TimeFormat::SimpleWithMonthAndYear(TimeFormat::FORMAT_DURATION,
                                                 TimeFormat::LENGTH_LONG,
                                                 delta_2y_, true));
    EXPECT_EQ(ASCIIToUTF16("2 secs left"), TimeFormat::Simple(
        TimeFormat::FORMAT_REMAINING, TimeFormat::LENGTH_SHORT, delta_2s_));
    EXPECT_EQ(ASCIIToUTF16("2 mins left"), TimeFormat::Simple(
        TimeFormat::FORMAT_REMAINING, TimeFormat::LENGTH_SHORT, delta_2m_));
    EXPECT_EQ(ASCIIToUTF16("2 hours left"), TimeFormat::Simple(
        TimeFormat::FORMAT_REMAINING, TimeFormat::LENGTH_SHORT, delta_2h_));
    EXPECT_EQ(ASCIIToUTF16("2 days left"), TimeFormat::Simple(
        TimeFormat::FORMAT_REMAINING, TimeFormat::LENGTH_SHORT, delta_2d_));
    EXPECT_EQ(ASCIIToUTF16("2 seconds left"), TimeFormat::Simple(
        TimeFormat::FORMAT_REMAINING, TimeFormat::LENGTH_LONG, delta_2s_));
    EXPECT_EQ(ASCIIToUTF16("2 minutes left"), TimeFormat::Simple(
        TimeFormat::FORMAT_REMAINING, TimeFormat::LENGTH_LONG, delta_2m_));
    EXPECT_EQ(ASCIIToUTF16("2 hours left"), TimeFormat::Simple(
        TimeFormat::FORMAT_REMAINING, TimeFormat::LENGTH_LONG, delta_2h_));
    EXPECT_EQ(ASCIIToUTF16("2 days left"), TimeFormat::Simple(
        TimeFormat::FORMAT_REMAINING, TimeFormat::LENGTH_LONG, delta_2d_));
    EXPECT_EQ(ASCIIToUTF16("30 days left"),
              TimeFormat::Simple(TimeFormat::FORMAT_REMAINING,
                                 TimeFormat::LENGTH_LONG, delta_1mo_));
    EXPECT_EQ(ASCIIToUTF16("365 days left"),
              TimeFormat::Simple(TimeFormat::FORMAT_REMAINING,
                                 TimeFormat::LENGTH_LONG, delta_1y_));
    EXPECT_EQ(ASCIIToUTF16("2 months left"),
              TimeFormat::SimpleWithMonthAndYear(TimeFormat::FORMAT_REMAINING,
                                                 TimeFormat::LENGTH_LONG,
                                                 delta_2mo_, true));
    EXPECT_EQ(ASCIIToUTF16("2 years left"),
              TimeFormat::SimpleWithMonthAndYear(TimeFormat::FORMAT_REMAINING,
                                                 TimeFormat::LENGTH_LONG,
                                                 delta_2y_, true));
    EXPECT_EQ(ASCIIToUTF16("2 secs ago"), TimeFormat::Simple(
        TimeFormat::FORMAT_ELAPSED, TimeFormat::LENGTH_SHORT, delta_2s_));
    EXPECT_EQ(ASCIIToUTF16("2 mins ago"), TimeFormat::Simple(
        TimeFormat::FORMAT_ELAPSED, TimeFormat::LENGTH_SHORT, delta_2m_));
    EXPECT_EQ(ASCIIToUTF16("2 hours ago"), TimeFormat::Simple(
        TimeFormat::FORMAT_ELAPSED, TimeFormat::LENGTH_SHORT, delta_2h_));
    EXPECT_EQ(ASCIIToUTF16("2 days ago"), TimeFormat::Simple(
        TimeFormat::FORMAT_ELAPSED, TimeFormat::LENGTH_SHORT, delta_2d_));
    EXPECT_EQ(ASCIIToUTF16("2 seconds ago"),
              TimeFormat::Simple(TimeFormat::FORMAT_ELAPSED,
                                 TimeFormat::LENGTH_LONG, delta_2s_));
    EXPECT_EQ(ASCIIToUTF16("2 minutes ago"),
              TimeFormat::Simple(TimeFormat::FORMAT_ELAPSED,
                                 TimeFormat::LENGTH_LONG, delta_2m_));
    EXPECT_EQ(ASCIIToUTF16("2 hours ago"),
              TimeFormat::Simple(TimeFormat::FORMAT_ELAPSED,
                                 TimeFormat::LENGTH_SHORT, delta_2h_));
    EXPECT_EQ(ASCIIToUTF16("2 days ago"),
              TimeFormat::Simple(TimeFormat::FORMAT_ELAPSED,
                                 TimeFormat::LENGTH_SHORT, delta_2d_));
    EXPECT_EQ(ASCIIToUTF16("30 days ago"),
              TimeFormat::Simple(TimeFormat::FORMAT_ELAPSED,
                                 TimeFormat::LENGTH_LONG, delta_1mo_));
    EXPECT_EQ(ASCIIToUTF16("365 days ago"),
              TimeFormat::Simple(TimeFormat::FORMAT_ELAPSED,
                                 TimeFormat::LENGTH_LONG, delta_1y_));
    EXPECT_EQ(ASCIIToUTF16("2 months ago"),
              TimeFormat::SimpleWithMonthAndYear(TimeFormat::FORMAT_ELAPSED,
                                                 TimeFormat::LENGTH_LONG,
                                                 delta_2mo_, true));
    EXPECT_EQ(ASCIIToUTF16("2 years ago"),
              TimeFormat::SimpleWithMonthAndYear(TimeFormat::FORMAT_ELAPSED,
                                                 TimeFormat::LENGTH_LONG,
                                                 delta_2y_, true));

    // Test English strings (detailed, singular and plural).
    EXPECT_EQ(ASCIIToUTF16("1 minute and 2 seconds"), TimeFormat::Detailed(
        TimeFormat::FORMAT_DURATION, TimeFormat::LENGTH_LONG, 3, delta_1m2s_));
    EXPECT_EQ(ASCIIToUTF16("2 minutes and 1 second"), TimeFormat::Detailed(
        TimeFormat::FORMAT_DURATION, TimeFormat::LENGTH_LONG, 3, delta_2m1s_));
    EXPECT_EQ(ASCIIToUTF16("1 hour and 2 minutes"), TimeFormat::Detailed(
        TimeFormat::FORMAT_DURATION, TimeFormat::LENGTH_LONG, 3, delta_1h2m_));
    EXPECT_EQ(ASCIIToUTF16("2 hours and 1 minute"), TimeFormat::Detailed(
        TimeFormat::FORMAT_DURATION, TimeFormat::LENGTH_LONG, 3, delta_2h1m_));
    EXPECT_EQ(ASCIIToUTF16("1 day and 2 hours"), TimeFormat::Detailed(
        TimeFormat::FORMAT_DURATION, TimeFormat::LENGTH_LONG, 3, delta_1d2h_));
    EXPECT_EQ(ASCIIToUTF16("2 days and 1 hour"), TimeFormat::Detailed(
        TimeFormat::FORMAT_DURATION, TimeFormat::LENGTH_LONG, 3, delta_2d1h_));
  }

  TimeDelta delta_0s_;
  TimeDelta delta_1ms_;
  TimeDelta delta_499ms_;
  TimeDelta delta_500ms_;
  TimeDelta delta_999ms_;
  TimeDelta delta_1s_;
  TimeDelta delta_1s499ms_;
  TimeDelta delta_1s500ms_;
  TimeDelta delta_2s_;
  TimeDelta delta_29s_;
  TimeDelta delta_30s_;
  TimeDelta delta_59s_;
  TimeDelta delta_59s499ms_;
  TimeDelta delta_59s500ms_;
  TimeDelta delta_1m_;
  TimeDelta delta_1m2s_;
  TimeDelta delta_1m29s999ms_;
  TimeDelta delta_1m30s_;
  TimeDelta delta_2m_;
  TimeDelta delta_2m1s_;
  TimeDelta delta_29m_;
  TimeDelta delta_30m_;
  TimeDelta delta_59m_;
  TimeDelta delta_59m29s999ms_;
  TimeDelta delta_59m30s_;
  TimeDelta delta_59m59s499ms_;
  TimeDelta delta_59m59s500ms_;
  TimeDelta delta_1h_;
  TimeDelta delta_1h2m_;
  TimeDelta delta_1h29m59s999ms_;
  TimeDelta delta_1h30m_;
  TimeDelta delta_2h_;
  TimeDelta delta_2h1m_;
  TimeDelta delta_11h_;
  TimeDelta delta_12h_;
  TimeDelta delta_23h_;
  TimeDelta delta_23h29m59s999ms_;
  TimeDelta delta_23h30m_;
  TimeDelta delta_23h59m29s999ms_;
  TimeDelta delta_23h59m30s_;
  TimeDelta delta_1d_;
  TimeDelta delta_1d2h_;
  TimeDelta delta_1d11h59m59s999ms_;
  TimeDelta delta_1d12h_;
  TimeDelta delta_2d_;
  TimeDelta delta_2d1h_;
  TimeDelta delta_1y_;
  TimeDelta delta_2y_;
  TimeDelta delta_1mo_;
  TimeDelta delta_2mo_;
  TimeDelta delta_1mo10d_;
};

TEST_F(TimeFormatTest, SimpleAndDetailedRounding) {
  // Test rounding behavior (simple).
  EXPECT_EQ(ASCIIToUTF16("0 secs"), TimeFormat::Simple(
      TimeFormat::FORMAT_DURATION, TimeFormat::LENGTH_SHORT, delta_0s_));
  EXPECT_EQ(ASCIIToUTF16("0 secs"), TimeFormat::Simple(
      TimeFormat::FORMAT_DURATION, TimeFormat::LENGTH_SHORT, delta_499ms_));
  EXPECT_EQ(ASCIIToUTF16("1 sec"), TimeFormat::Simple(
      TimeFormat::FORMAT_DURATION, TimeFormat::LENGTH_SHORT, delta_500ms_));
  EXPECT_EQ(ASCIIToUTF16("1 sec"), TimeFormat::Simple(
      TimeFormat::FORMAT_DURATION, TimeFormat::LENGTH_SHORT, delta_1s499ms_));
  EXPECT_EQ(ASCIIToUTF16("2 secs"), TimeFormat::Simple(
      TimeFormat::FORMAT_DURATION, TimeFormat::LENGTH_SHORT, delta_1s500ms_));
  EXPECT_EQ(ASCIIToUTF16("59 secs"), TimeFormat::Simple(
      TimeFormat::FORMAT_DURATION, TimeFormat::LENGTH_SHORT, delta_59s499ms_));
  EXPECT_EQ(ASCIIToUTF16("1 min"), TimeFormat::Simple(
      TimeFormat::FORMAT_DURATION, TimeFormat::LENGTH_SHORT, delta_59s500ms_));
  EXPECT_EQ(ASCIIToUTF16("1 min"), TimeFormat::Simple(
      TimeFormat::FORMAT_DURATION, TimeFormat::LENGTH_SHORT,
      delta_1m29s999ms_));
  EXPECT_EQ(ASCIIToUTF16("2 mins"), TimeFormat::Simple(
      TimeFormat::FORMAT_DURATION, TimeFormat::LENGTH_SHORT, delta_1m30s_));
  EXPECT_EQ(ASCIIToUTF16("59 mins"), TimeFormat::Simple(
      TimeFormat::FORMAT_DURATION, TimeFormat::LENGTH_SHORT,
      delta_59m29s999ms_));
  EXPECT_EQ(ASCIIToUTF16("1 hour"), TimeFormat::Simple(
      TimeFormat::FORMAT_DURATION, TimeFormat::LENGTH_SHORT, delta_59m30s_));
  EXPECT_EQ(ASCIIToUTF16("1 hour"), TimeFormat::Simple(
      TimeFormat::FORMAT_DURATION, TimeFormat::LENGTH_SHORT,
      delta_1h29m59s999ms_));
  EXPECT_EQ(ASCIIToUTF16("2 hours"), TimeFormat::Simple(
      TimeFormat::FORMAT_DURATION, TimeFormat::LENGTH_SHORT, delta_1h30m_));
  EXPECT_EQ(ASCIIToUTF16("23 hours"), TimeFormat::Simple(
      TimeFormat::FORMAT_DURATION, TimeFormat::LENGTH_SHORT,
      delta_23h29m59s999ms_));
  EXPECT_EQ(ASCIIToUTF16("1 day"), TimeFormat::Simple(
      TimeFormat::FORMAT_DURATION, TimeFormat::LENGTH_SHORT, delta_23h30m_));
  EXPECT_EQ(ASCIIToUTF16("1 day"), TimeFormat::Simple(
      TimeFormat::FORMAT_DURATION, TimeFormat::LENGTH_SHORT,
      delta_1d11h59m59s999ms_));
  EXPECT_EQ(ASCIIToUTF16("2 days"), TimeFormat::Simple(
      TimeFormat::FORMAT_DURATION, TimeFormat::LENGTH_SHORT, delta_1d12h_));

  // Test rounding behavior (detailed).
  EXPECT_EQ(ASCIIToUTF16("59 seconds"), TimeFormat::Detailed(
      TimeFormat::FORMAT_DURATION, TimeFormat::LENGTH_LONG, 100,
      delta_59s499ms_));
  EXPECT_EQ(ASCIIToUTF16("1 minute and 0 seconds"), TimeFormat::Detailed(
      TimeFormat::FORMAT_DURATION, TimeFormat::LENGTH_LONG, 2,
      delta_59s500ms_));
  EXPECT_EQ(ASCIIToUTF16("1 minute"), TimeFormat::Detailed(
      TimeFormat::FORMAT_DURATION, TimeFormat::LENGTH_LONG, 1,
      delta_59s500ms_));
  EXPECT_EQ(ASCIIToUTF16("59 minutes and 59 seconds"), TimeFormat::Detailed(
      TimeFormat::FORMAT_DURATION, TimeFormat::LENGTH_LONG, 60,
      delta_59m59s499ms_));
  EXPECT_EQ(ASCIIToUTF16("1 hour and 0 minutes"), TimeFormat::Detailed(
      TimeFormat::FORMAT_DURATION, TimeFormat::LENGTH_LONG, 59,
      delta_59m59s499ms_));
  EXPECT_EQ(ASCIIToUTF16("1 hour and 0 minutes"), TimeFormat::Detailed(
      TimeFormat::FORMAT_DURATION, TimeFormat::LENGTH_LONG, 2,
      delta_59m59s499ms_));
  EXPECT_EQ(ASCIIToUTF16("1 hour"), TimeFormat::Detailed(
      TimeFormat::FORMAT_DURATION, TimeFormat::LENGTH_LONG, 1,
      delta_59m59s499ms_));
  EXPECT_EQ(ASCIIToUTF16("1 hour"), TimeFormat::Detailed(
      TimeFormat::FORMAT_DURATION, TimeFormat::LENGTH_LONG, 1,
      delta_59m59s500ms_));
  EXPECT_EQ(ASCIIToUTF16("1 hour and 0 minutes"), TimeFormat::Detailed(
      TimeFormat::FORMAT_DURATION, TimeFormat::LENGTH_LONG, 2,
      delta_59m59s500ms_));
  EXPECT_EQ(ASCIIToUTF16("23 hours and 59 minutes"), TimeFormat::Detailed(
      TimeFormat::FORMAT_DURATION, TimeFormat::LENGTH_LONG, 24,
      delta_23h59m29s999ms_));
  EXPECT_EQ(ASCIIToUTF16("1 day and 0 hours"), TimeFormat::Detailed(
      TimeFormat::FORMAT_DURATION, TimeFormat::LENGTH_LONG, 23,
      delta_23h59m29s999ms_));
  EXPECT_EQ(ASCIIToUTF16("1 day and 0 hours"), TimeFormat::Detailed(
      TimeFormat::FORMAT_DURATION, TimeFormat::LENGTH_LONG, 2,
      delta_23h59m29s999ms_));
  EXPECT_EQ(ASCIIToUTF16("1 day"), TimeFormat::Detailed(
      TimeFormat::FORMAT_DURATION, TimeFormat::LENGTH_LONG, 1,
      delta_23h59m29s999ms_));
  EXPECT_EQ(ASCIIToUTF16("1 day"), TimeFormat::Detailed(
      TimeFormat::FORMAT_DURATION, TimeFormat::LENGTH_LONG, 1,
      delta_23h59m30s_));
  EXPECT_EQ(ASCIIToUTF16("1 day and 0 hours"), TimeFormat::Detailed(
      TimeFormat::FORMAT_DURATION, TimeFormat::LENGTH_LONG, 2,
      delta_23h59m30s_));
  EXPECT_EQ(ASCIIToUTF16("1 day and 0 hours"), TimeFormat::Detailed(
      TimeFormat::FORMAT_DURATION, TimeFormat::LENGTH_LONG, -1,
      delta_23h59m30s_));
}

// Test strings in default code path.
TEST_F(TimeFormatTest, SimpleAndDetailedStrings) {
  TestStrings();
}

// Test strings in fallback path in case of translator error.
TEST_F(TimeFormatTest, SimpleAndDetailedStringFallback) {
  formatter_force_fallback = true;
  g_container.Get().ResetForTesting();
  TestStrings();
  formatter_force_fallback = false;
  g_container.Get().ResetForTesting();
}

// crbug.com/159388: This test fails when daylight savings time ends.
TEST_F(TimeFormatTest, RelativeDate) {
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
