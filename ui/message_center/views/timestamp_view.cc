// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/timestamp_view.h"

#include "base/strings/string16.h"
#include "base/time/time.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/strings/grit/ui_strings.h"

namespace message_center {

namespace {

// base::TimeBase has similar constants, but some of them are missing.
constexpr int64_t kMinuteInMillis = 60LL * 1000LL;
constexpr int64_t kHourInMillis = 60LL * kMinuteInMillis;
constexpr int64_t kDayInMillis = 24LL * kHourInMillis;
// In Android, DateUtils.YEAR_IN_MILLIS is 364 days.
constexpr int64_t kYearInMillis = 364LL * kDayInMillis;

// Do relative time string formatting that is similar to
// com.java.android.widget.DateTimeView.updateRelativeTime.
// Chromium has its own base::TimeFormat::Simple(), but none of the formats
// supported by the function is similar to Android's one.
base::string16 FormatToRelativeTime(base::TimeDelta delta) {
  int64_t duration = std::abs(delta.InMilliseconds());
  bool past = delta <= base::TimeDelta();
  if (duration < kMinuteInMillis) {
    return l10n_util::GetStringUTF16(
        IDS_MESSAGE_NOTIFICATION_NOW_STRING_SHORTEST);
  } else if (duration < kHourInMillis) {
    int count = static_cast<int>(duration / kMinuteInMillis);
    return l10n_util::GetPluralStringFUTF16(
        past ? IDS_MESSAGE_NOTIFICATION_DURATION_MINUTES_SHORTEST
             : IDS_MESSAGE_NOTIFICATION_DURATION_MINUTES_SHORTEST_FUTURE,
        count);
  } else if (duration < kDayInMillis) {
    int count = static_cast<int>(duration / kHourInMillis);
    return l10n_util::GetPluralStringFUTF16(
        past ? IDS_MESSAGE_NOTIFICATION_DURATION_HOURS_SHORTEST
             : IDS_MESSAGE_NOTIFICATION_DURATION_HOURS_SHORTEST_FUTURE,
        count);
  } else if (duration < kYearInMillis) {
    // TODO(https://crbug.com/914432): Use a calendar to calculate this.
    int count = static_cast<int>(duration / kDayInMillis);
    return l10n_util::GetPluralStringFUTF16(
        past ? IDS_MESSAGE_NOTIFICATION_DURATION_DAYS_SHORTEST
             : IDS_MESSAGE_NOTIFICATION_DURATION_DAYS_SHORTEST_FUTURE,
        count);
  } else {
    int count = static_cast<int>(duration / kYearInMillis);
    return l10n_util::GetPluralStringFUTF16(
        past ? IDS_MESSAGE_NOTIFICATION_DURATION_YEARS_SHORTEST
             : IDS_MESSAGE_NOTIFICATION_DURATION_YEARS_SHORTEST_FUTURE,
        count);
  }
}

}  // namespace

TimestampView::TimestampView() = default;

TimestampView::~TimestampView() = default;

void TimestampView::SetTimestamp(base::Time timestamp) {
  SetText(FormatToRelativeTime(timestamp - base::Time::Now()));
}

}  // namespace message_center
