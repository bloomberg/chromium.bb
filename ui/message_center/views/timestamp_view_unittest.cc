// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/timestamp_view.h"

#include "base/test/scoped_task_environment.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/test/views_test_base.h"

namespace message_center {

namespace {

// In Android, DateUtils.YEAR_IN_MILLIS is 364 days.
constexpr int64_t kYearInMillis = 364LL * base::Time::kMillisecondsPerDay;
constexpr base::TimeDelta kTimeAdvance = base::TimeDelta::FromMilliseconds(1);

}  // namespace

class TimestampViewTest : public views::ViewsTestBase {
 public:
  TimestampViewTest() = default;
  ~TimestampViewTest() override = default;

  // ViewsTestBase:
  void SetUp() override {
    // Setup a mocked time environment.
    scoped_task_environment_ = new ScopedTaskEnvironment(
        ScopedTaskEnvironment::MainThreadType::MOCK_TIME,
        ScopedTaskEnvironment::NowSource::MAIN_THREAD_MOCK_TIME);
    set_scoped_task_environment(base::WrapUnique(scoped_task_environment_));

    // Advance time a little bit so that TimeTicks::Now().is_null() becomes
    // false.
    scoped_task_environment_->FastForwardBy(kTimeAdvance);

    ViewsTestBase::SetUp();

    views::Widget::InitParams params =
        CreateParams(views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    params.bounds = gfx::Rect(200, 200);
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    widget_.Init(params);
    views::View* container = new views::View();
    widget_.SetContentsView(container);

    timestamp_view_ = new TimestampView();
    container->AddChildView(timestamp_view_);

    widget_.Show();
  }

  void TearDown() override {
    widget_.Close();
    ViewsTestBase::TearDown();
  }

 protected:
  TimestampView* view() { return timestamp_view_; }

 private:
  TimestampView* timestamp_view_ = nullptr;
  views::Widget widget_;
  ScopedTaskEnvironment* scoped_task_environment_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(TimestampViewTest);
};

TEST_F(TimestampViewTest, FormatToRelativeTime) {
  // Test 30 seconds in the future.
  view()->SetTimestamp(base::Time::Now() + base::TimeDelta::FromSeconds(30));
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_MESSAGE_NOTIFICATION_NOW_STRING_SHORTEST),
      view()->text());
  // Test 30 seconds in the past.
  view()->SetTimestamp(base::Time::Now() + base::TimeDelta::FromSeconds(-30));
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_MESSAGE_NOTIFICATION_NOW_STRING_SHORTEST),
      view()->text());

  // Test 60 seconds in the future.
  view()->SetTimestamp(base::Time::Now() + base::TimeDelta::FromSeconds(60));
  EXPECT_EQ(l10n_util::GetPluralStringFUTF16(
                IDS_MESSAGE_NOTIFICATION_DURATION_MINUTES_SHORTEST_FUTURE, 1),
            view()->text());
  // Test 60 seconds in the past.
  view()->SetTimestamp(base::Time::Now() + base::TimeDelta::FromSeconds(-60));
  EXPECT_EQ(l10n_util::GetPluralStringFUTF16(
                IDS_MESSAGE_NOTIFICATION_DURATION_MINUTES_SHORTEST, 1),
            view()->text());

  // Test 5 minutes in the future.
  view()->SetTimestamp(base::Time::Now() + base::TimeDelta::FromMinutes(5));
  EXPECT_EQ(l10n_util::GetPluralStringFUTF16(
                IDS_MESSAGE_NOTIFICATION_DURATION_MINUTES_SHORTEST_FUTURE, 5),
            view()->text());
  // Test 5 minutes in the past.
  view()->SetTimestamp(base::Time::Now() + base::TimeDelta::FromMinutes(-5));
  EXPECT_EQ(l10n_util::GetPluralStringFUTF16(
                IDS_MESSAGE_NOTIFICATION_DURATION_MINUTES_SHORTEST, 5),
            view()->text());

  // Test 60 minutes in the future.
  view()->SetTimestamp(base::Time::Now() + base::TimeDelta::FromMinutes(60));
  EXPECT_EQ(l10n_util::GetPluralStringFUTF16(
                IDS_MESSAGE_NOTIFICATION_DURATION_HOURS_SHORTEST_FUTURE, 1),
            view()->text());
  // Test 60 minutes in the past.
  view()->SetTimestamp(base::Time::Now() + base::TimeDelta::FromMinutes(-60));
  EXPECT_EQ(l10n_util::GetPluralStringFUTF16(
                IDS_MESSAGE_NOTIFICATION_DURATION_HOURS_SHORTEST, 1),
            view()->text());

  // Test 10 hours in the future.
  view()->SetTimestamp(base::Time::Now() + base::TimeDelta::FromHours(10));
  EXPECT_EQ(l10n_util::GetPluralStringFUTF16(
                IDS_MESSAGE_NOTIFICATION_DURATION_HOURS_SHORTEST_FUTURE, 10),
            view()->text());
  // Test 10 hours in the past.
  view()->SetTimestamp(base::Time::Now() + base::TimeDelta::FromHours(-10));
  EXPECT_EQ(l10n_util::GetPluralStringFUTF16(
                IDS_MESSAGE_NOTIFICATION_DURATION_HOURS_SHORTEST, 10),
            view()->text());

  // Test 24 hours in the future.
  view()->SetTimestamp(base::Time::Now() + base::TimeDelta::FromHours(24));
  EXPECT_EQ(l10n_util::GetPluralStringFUTF16(
                IDS_MESSAGE_NOTIFICATION_DURATION_DAYS_SHORTEST_FUTURE, 1),
            view()->text());
  // Test 24 hours in the past.
  view()->SetTimestamp(base::Time::Now() + base::TimeDelta::FromHours(-24));
  EXPECT_EQ(l10n_util::GetPluralStringFUTF16(
                IDS_MESSAGE_NOTIFICATION_DURATION_DAYS_SHORTEST, 1),
            view()->text());

  // Test 1 year in the future.
  view()->SetTimestamp(base::Time::Now() +
                       base::TimeDelta::FromMilliseconds(kYearInMillis));
  EXPECT_EQ(l10n_util::GetPluralStringFUTF16(
                IDS_MESSAGE_NOTIFICATION_DURATION_YEARS_SHORTEST_FUTURE, 1),
            view()->text());
  // Test 1 year in the past.
  view()->SetTimestamp(base::Time::Now() +
                       base::TimeDelta::FromMilliseconds(-kYearInMillis));
  EXPECT_EQ(l10n_util::GetPluralStringFUTF16(
                IDS_MESSAGE_NOTIFICATION_DURATION_YEARS_SHORTEST, 1),
            view()->text());

  // Test 10 years in the future.
  view()->SetTimestamp(base::Time::Now() +
                       base::TimeDelta::FromMilliseconds(kYearInMillis * 10));
  EXPECT_EQ(l10n_util::GetPluralStringFUTF16(
                IDS_MESSAGE_NOTIFICATION_DURATION_YEARS_SHORTEST_FUTURE, 10),
            view()->text());
  // Test 10 years in the past.
  view()->SetTimestamp(base::Time::Now() +
                       base::TimeDelta::FromMilliseconds(-kYearInMillis * 10));
  EXPECT_EQ(l10n_util::GetPluralStringFUTF16(
                IDS_MESSAGE_NOTIFICATION_DURATION_YEARS_SHORTEST, 10),
            view()->text());
}

}  // namespace message_center
