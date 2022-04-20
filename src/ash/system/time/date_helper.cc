// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/time/date_helper.h"

#include "base/i18n/unicodestring.h"
#include "base/time/time.h"
#include "third_party/icu/source/i18n/unicode/gregocal.h"

namespace ash {

namespace {

// Milliseconds per minute.
constexpr int kMillisecondsPerMinute = 60000;

}  // namespace

// static
DateHelper* DateHelper::GetInstance() {
  return base::Singleton<DateHelper>::get();
}

icu::SimpleDateFormat DateHelper::CreateSimpleDateFormatter(
    const char* pattern) {
  // Generate a locale-dependent format pattern. The generator will take
  // care of locale-dependent formatting issues like which separator to
  // use (some locales use '.' instead of ':'), and where to put the am/pm
  // marker.
  UErrorCode status = U_ZERO_ERROR;
  DCHECK(U_SUCCESS(status));
  std::unique_ptr<icu::DateTimePatternGenerator> generator(
      icu::DateTimePatternGenerator::createInstance(status));
  DCHECK(U_SUCCESS(status));
  icu::UnicodeString generated_pattern =
      generator->getBestPattern(icu::UnicodeString(pattern), status);
  DCHECK(U_SUCCESS(status));

  // Then, format the time using the generated pattern.
  icu::SimpleDateFormat formatter(generated_pattern, status);
  DCHECK(U_SUCCESS(status));

  return formatter;
}

std::u16string DateHelper::GetFormattedTime(const icu::DateFormat* formatter,
                                            const base::Time& time) {
  DCHECK(formatter);
  icu::UnicodeString date_string;

  formatter->format(
      static_cast<UDate>(time.ToDoubleT() * base::Time::kMillisecondsPerSecond),
      date_string);
  return base::i18n::UnicodeStringToString16(date_string);
}

int DateHelper::GetTimeDifferenceInMinutes(base::Time date) {
  const icu::TimeZone& time_zone =
      system::TimezoneSettings::GetInstance()->GetTimezone();
  const int raw_time_diff = time_zone.getRawOffset() / kMillisecondsPerMinute;

  // Calculates the time difference adjust by the possible daylight savings
  // offset. If the status of any step fails, returns the default time
  // difference without considering daylight savings.
  if (!gregorian_calendar_)
    return raw_time_diff;

  UDate current_date =
      static_cast<UDate>(date.ToDoubleT() * base::Time::kMillisecondsPerSecond);
  UErrorCode status = U_ZERO_ERROR;
  gregorian_calendar_->setTime(current_date, status);
  if (U_FAILURE(status))
    return raw_time_diff;

  status = U_ZERO_ERROR;
  UBool day_light = gregorian_calendar_->inDaylightTime(status);
  if (U_FAILURE(status))
    return raw_time_diff;

  int gmt_offset = time_zone.getRawOffset();
  if (day_light)
    gmt_offset += time_zone.getDSTSavings();

  return gmt_offset / kMillisecondsPerMinute;
}

DateHelper::DateHelper()
    : day_of_month_formatter_(CreateSimpleDateFormatter("d")),
      month_day_formatter_(CreateSimpleDateFormatter("MMMMd")),
      month_day_year_formatter_(CreateSimpleDateFormatter("MMMMdyyyy")),
      month_name_formatter_(CreateSimpleDateFormatter("MMMM")),
      month_name_year_formatter_(CreateSimpleDateFormatter("MMMM yyyy")),
      time_zone_formatter_(CreateSimpleDateFormatter("zzzz")),
      twelve_hour_clock_formatter_(CreateSimpleDateFormatter("h:mm a")),
      twenty_four_hour_clock_formatter_(CreateSimpleDateFormatter("HH:mm")),
      year_formatter_(CreateSimpleDateFormatter("YYYY")) {
  const icu::TimeZone& time_zone =
      system::TimezoneSettings::GetInstance()->GetTimezone();

  UErrorCode status = U_ZERO_ERROR;
  gregorian_calendar_ =
      std::make_unique<icu::GregorianCalendar>(time_zone, status);
  DCHECK(U_SUCCESS(status));
  time_zone_settings_observer_.Observe(system::TimezoneSettings::GetInstance());
}

DateHelper::~DateHelper() = default;

void DateHelper::ResetFormatters() {
  day_of_month_formatter_ = CreateSimpleDateFormatter("d");
  month_day_formatter_ = CreateSimpleDateFormatter("MMMMd");
  month_day_year_formatter_ = CreateSimpleDateFormatter("MMMMdyyyy");
  month_name_formatter_ = CreateSimpleDateFormatter("MMMM");
  month_name_year_formatter_ = CreateSimpleDateFormatter("MMMM yyyy");
  time_zone_formatter_ = CreateSimpleDateFormatter("zzzz");
  twelve_hour_clock_formatter_ = CreateSimpleDateFormatter("h:mm a");
  twenty_four_hour_clock_formatter_ = CreateSimpleDateFormatter("HH:mm");
  year_formatter_ = CreateSimpleDateFormatter("YYYY");
}

void DateHelper::TimezoneChanged(const icu::TimeZone& timezone) {
  ResetFormatters();
  gregorian_calendar_->setTimeZone(
      system::TimezoneSettings::GetInstance()->GetTimezone());
}

}  // namespace ash
