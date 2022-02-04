// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-utils-inl.h"
#include "src/builtins/builtins.h"
#include "src/objects/bigint.h"
#include "src/objects/js-temporal-objects-inl.h"

namespace v8 {
namespace internal {

#define TO_BE_IMPLEMENTED(id)   \
  BUILTIN_NO_RCS(id) {          \
    HandleScope scope(isolate); \
    UNIMPLEMENTED();            \
  }

/* Temporal #sec-temporal.now.timezone */
TO_BE_IMPLEMENTED(TemporalNowTimeZone)
/* Temporal #sec-temporal.now.instant */
TO_BE_IMPLEMENTED(TemporalNowInstant)
/* Temporal #sec-temporal.now.plaindatetime */
TO_BE_IMPLEMENTED(TemporalNowPlainDateTime)
/* Temporal #sec-temporal.now.plaindatetimeiso */
TO_BE_IMPLEMENTED(TemporalNowPlainDateTimeISO)
/* Temporal #sec-temporal.now.zoneddatetime */
TO_BE_IMPLEMENTED(TemporalNowZonedDateTime)
/* Temporal #sec-temporal.now.zoneddatetimeiso */
TO_BE_IMPLEMENTED(TemporalNowZonedDateTimeISO)
/* Temporal #sec-temporal.now.plaindate */
TO_BE_IMPLEMENTED(TemporalNowPlainDate)
/* Temporal #sec-temporal.now.plaindateiso */
TO_BE_IMPLEMENTED(TemporalNowPlainDateISO)

/* There is no Temporal.now.plainTime. See
 * https://github.com/tc39/proposal-temporal/issues/1540 */

/* Temporal #sec-temporal.now.plaintimeiso */
TO_BE_IMPLEMENTED(TemporalNowPlainTimeISO)

/* Temporal #sec-temporal.plaindate.from */
TO_BE_IMPLEMENTED(TemporalPlainDateFrom)
/* Temporal #sec-temporal.plaindate.compare */
TO_BE_IMPLEMENTED(TemporalPlainDateCompare)
/* Temporal #sec-get-temporal.plaindate.prototype.year */
TO_BE_IMPLEMENTED(TemporalPlainDatePrototypeYear)
/* Temporal #sec-get-temporal.plaindate.prototype.month */
TO_BE_IMPLEMENTED(TemporalPlainDatePrototypeMonth)
/* Temporal #sec-get-temporal.plaindate.prototype.monthcode */
TO_BE_IMPLEMENTED(TemporalPlainDatePrototypeMonthCode)
/* Temporal #sec-get-temporal.plaindate.prototype.day */
TO_BE_IMPLEMENTED(TemporalPlainDatePrototypeDay)
/* Temporal #sec-get-temporal.plaindate.prototype.dayofweek */
TO_BE_IMPLEMENTED(TemporalPlainDatePrototypeDayOfWeek)
/* Temporal #sec-get-temporal.plaindate.prototype.dayofyear */
TO_BE_IMPLEMENTED(TemporalPlainDatePrototypeDayOfYear)
/* Temporal #sec-get-temporal.plaindate.prototype.weekofyear */
TO_BE_IMPLEMENTED(TemporalPlainDatePrototypeWeekOfYear)
/* Temporal #sec-get-temporal.plaindate.prototype.daysinweek */
TO_BE_IMPLEMENTED(TemporalPlainDatePrototypeDaysInWeek)
/* Temporal #sec-get-temporal.plaindate.prototype.daysinmonth */
TO_BE_IMPLEMENTED(TemporalPlainDatePrototypeDaysInMonth)
/* Temporal #sec-get-temporal.plaindate.prototype.daysinyear */
TO_BE_IMPLEMENTED(TemporalPlainDatePrototypeDaysInYear)
/* Temporal #sec-get-temporal.plaindate.prototype.monthsinyear */
TO_BE_IMPLEMENTED(TemporalPlainDatePrototypeMonthsInYear)
/* Temporal #sec-get-temporal.plaindate.prototype.inleapyear */
TO_BE_IMPLEMENTED(TemporalPlainDatePrototypeInLeapYear)
/* Temporal #sec-temporal.plaindate.prototype.toplainyearmonth */
TO_BE_IMPLEMENTED(TemporalPlainDatePrototypeToPlainYearMonth)
/* Temporal #sec-temporal.plaindate.prototype.toplainmonthday */
TO_BE_IMPLEMENTED(TemporalPlainDatePrototypeToPlainMonthDay)
/* Temporal #sec-temporal.plaindate.prototype.add */
TO_BE_IMPLEMENTED(TemporalPlainDatePrototypeAdd)
/* Temporal #sec-temporal.plaindate.prototype.substract */
TO_BE_IMPLEMENTED(TemporalPlainDatePrototypeSubtract)
/* Temporal #sec-temporal.plaindate.prototype.with */
TO_BE_IMPLEMENTED(TemporalPlainDatePrototypeWith)
/* Temporal #sec-temporal.plaindate.prototype.withcalendar */
TO_BE_IMPLEMENTED(TemporalPlainDatePrototypeWithCalendar)
/* Temporal #sec-temporal.plaindate.prototype.until */
TO_BE_IMPLEMENTED(TemporalPlainDatePrototypeUntil)
/* Temporal #sec-temporal.plaindate.prototype.since */
TO_BE_IMPLEMENTED(TemporalPlainDatePrototypeSince)
/* Temporal #sec-temporal.plaindate.prototype.equals */
TO_BE_IMPLEMENTED(TemporalPlainDatePrototypeEquals)
/* Temporal #sec-temporal.plaindate.prototype.toplaindatetime */
TO_BE_IMPLEMENTED(TemporalPlainDatePrototypeToPlainDateTime)
/* Temporal #sec-temporal.plaindate.prototype.tozoneddatetime */
TO_BE_IMPLEMENTED(TemporalPlainDatePrototypeToZonedDateTime)
/* Temporal #sec-temporal.plaindate.prototype.tostring */
TO_BE_IMPLEMENTED(TemporalPlainDatePrototypeToString)
/* Temporal #sec-temporal.plaindate.prototype.tojson */
TO_BE_IMPLEMENTED(TemporalPlainDatePrototypeToJSON)

/* Temporal.PlaneTime */
/* Temporal #sec-temporal.plaintime.from */
TO_BE_IMPLEMENTED(TemporalPlainTimeFrom)
/* Temporal #sec-temporal.plaintime.compare */
TO_BE_IMPLEMENTED(TemporalPlainTimeCompare)
/* Temporal #sec-get-temporal.plaintime.prototype.hour */
TO_BE_IMPLEMENTED(TemporalPlainTimePrototypeHour)
/* Temporal #sec-get-temporal.plaintime.prototype.minute */
TO_BE_IMPLEMENTED(TemporalPlainTimePrototypeMinute)
/* Temporal #sec-get-temporal.plaintime.prototype.second */
TO_BE_IMPLEMENTED(TemporalPlainTimePrototypeSecond)
/* Temporal #sec-get-temporal.plaintime.prototype.millisecond */
TO_BE_IMPLEMENTED(TemporalPlainTimePrototypeMillisecond)
/* Temporal #sec-get-temporal.plaintime.prototype.microsecond */
TO_BE_IMPLEMENTED(TemporalPlainTimePrototypeMicrosecond)
/* Temporal #sec-get-temporal.plaintime.prototype.nanoseond */
TO_BE_IMPLEMENTED(TemporalPlainTimePrototypeNanosecond)
/* Temporal #sec-temporal.plaintime.prototype.add */
TO_BE_IMPLEMENTED(TemporalPlainTimePrototypeAdd)
/* Temporal #sec-temporal.plaintime.prototype.subtract */
TO_BE_IMPLEMENTED(TemporalPlainTimePrototypeSubtract)
/* Temporal #sec-temporal.plaintime.prototype.with */
TO_BE_IMPLEMENTED(TemporalPlainTimePrototypeWith)
/* Temporal #sec-temporal.plaintime.prototype.until */
TO_BE_IMPLEMENTED(TemporalPlainTimePrototypeUntil)
/* Temporal #sec-temporal.plaintime.prototype.since */
TO_BE_IMPLEMENTED(TemporalPlainTimePrototypeSince)
/* Temporal #sec-temporal.plaintime.prototype.round */
TO_BE_IMPLEMENTED(TemporalPlainTimePrototypeRound)
/* Temporal #sec-temporal.plaintime.prototype.equals */
TO_BE_IMPLEMENTED(TemporalPlainTimePrototypeEquals)
/* Temporal #sec-temporal.plaintime.prototype.toplaindatetime */
TO_BE_IMPLEMENTED(TemporalPlainTimePrototypeToPlainDateTime)
/* Temporal #sec-temporal.plaintime.prototype.tozoneddatetime */
TO_BE_IMPLEMENTED(TemporalPlainTimePrototypeToZonedDateTime)
/* Temporal #sec-temporal.plaintime.prototype.tostring */
TO_BE_IMPLEMENTED(TemporalPlainTimePrototypeToString)
/* Temporal #sec-temporal.plaindtimeprototype.tojson */
TO_BE_IMPLEMENTED(TemporalPlainTimePrototypeToJSON)

/* Temporal.PlaneDateTime */
/* Temporal #sec-temporal.plaindatetime.from */
TO_BE_IMPLEMENTED(TemporalPlainDateTimeFrom)
/* Temporal #sec-temporal.plaindatetime.compare */
TO_BE_IMPLEMENTED(TemporalPlainDateTimeCompare)
/* Temporal #sec-get-temporal.plaindatetime.prototype.year */
TO_BE_IMPLEMENTED(TemporalPlainDateTimePrototypeYear)
/* Temporal #sec-get-temporal.plaindatetime.prototype.month */
TO_BE_IMPLEMENTED(TemporalPlainDateTimePrototypeMonth)
/* Temporal #sec-get-temporal.plaindatetime.prototype.monthcode */
TO_BE_IMPLEMENTED(TemporalPlainDateTimePrototypeMonthCode)
/* Temporal #sec-get-temporal.plaindatetime.prototype.day */
TO_BE_IMPLEMENTED(TemporalPlainDateTimePrototypeDay)
/* Temporal #sec-get-temporal.plaindatetime.prototype.hour */
TO_BE_IMPLEMENTED(TemporalPlainDateTimePrototypeHour)
/* Temporal #sec-get-temporal.plaindatetime.prototype.minute */
TO_BE_IMPLEMENTED(TemporalPlainDateTimePrototypeMinute)
/* Temporal #sec-get-temporal.plaindatetime.prototype.second */
TO_BE_IMPLEMENTED(TemporalPlainDateTimePrototypeSecond)
/* Temporal #sec-get-temporal.plaindatetime.prototype.millisecond */
TO_BE_IMPLEMENTED(TemporalPlainDateTimePrototypeMillisecond)
/* Temporal #sec-get-temporal.plaindatetime.prototype.microsecond */
TO_BE_IMPLEMENTED(TemporalPlainDateTimePrototypeMicrosecond)
/* Temporal #sec-get-temporal.plaindatetime.prototype.nanosecond */
TO_BE_IMPLEMENTED(TemporalPlainDateTimePrototypeNanosecond)
/* Temporal #sec-get-temporal.plaindatetime.prototype.dayofweek */
TO_BE_IMPLEMENTED(TemporalPlainDateTimePrototypeDayOfWeek)
/* Temporal #sec-get-temporal.plaindatetime.prototype.dayofyear */
TO_BE_IMPLEMENTED(TemporalPlainDateTimePrototypeDayOfYear)
/* Temporal #sec-get-temporal.plaindatetime.prototype.weekofyear */
TO_BE_IMPLEMENTED(TemporalPlainDateTimePrototypeWeekOfYear)
/* Temporal #sec-get-temporal.plaindatetime.prototype.daysinweek */
TO_BE_IMPLEMENTED(TemporalPlainDateTimePrototypeDaysInWeek)
/* Temporal #sec-get-temporal.plaindatetime.prototype.daysinmonth */
TO_BE_IMPLEMENTED(TemporalPlainDateTimePrototypeDaysInMonth)
/* Temporal #sec-get-temporal.plaindatetime.prototype.daysinyear */
TO_BE_IMPLEMENTED(TemporalPlainDateTimePrototypeDaysInYear)
/* Temporal #sec-get-temporal.plaindatetime.prototype.monthsinyear */
TO_BE_IMPLEMENTED(TemporalPlainDateTimePrototypeMonthsInYear)
/* Temporal #sec-get-temporal.plaindatetime.prototype.inleapyear */
TO_BE_IMPLEMENTED(TemporalPlainDateTimePrototypeInLeapYear)
/* Temporal #sec-temporal.plaindatetime.prototype.with */
TO_BE_IMPLEMENTED(TemporalPlainDateTimePrototypeWith)
/* Temporal #sec-temporal.plaindatetime.prototype.withplainTime */
TO_BE_IMPLEMENTED(TemporalPlainDateTimePrototypeWithPlainTime)
/* Temporal #sec-temporal.plaindatetime.prototype.withplainDate */
TO_BE_IMPLEMENTED(TemporalPlainDateTimePrototypeWithPlainDate)
/* Temporal #sec-temporal.plaindatetime.prototype.withcalendar */
TO_BE_IMPLEMENTED(TemporalPlainDateTimePrototypeWithCalendar)
/* Temporal #sec-temporal.plaindatetime.prototype.add */
TO_BE_IMPLEMENTED(TemporalPlainDateTimePrototypeAdd)
/* Temporal #sec-temporal.plaindatetime.prototype.subtract */
TO_BE_IMPLEMENTED(TemporalPlainDateTimePrototypeSubtract)
/* Temporal #sec-temporal.plaindatetime.prototype.until */
TO_BE_IMPLEMENTED(TemporalPlainDateTimePrototypeUntil)
/* Temporal #sec-temporal.plaindatetime.prototype.since */
TO_BE_IMPLEMENTED(TemporalPlainDateTimePrototypeSince)
/* Temporal #sec-temporal.plaindatetime.prototype.round */
TO_BE_IMPLEMENTED(TemporalPlainDateTimePrototypeRound)
/* Temporal #sec-temporal.plaindatetime.prototype.equals */
TO_BE_IMPLEMENTED(TemporalPlainDateTimePrototypeEquals)
/* Temporal #sec-temporal.plaindatetime.prototype.tostring */
TO_BE_IMPLEMENTED(TemporalPlainDateTimePrototypeToString)
/* Temporal #sec-temporal.plainddatetimeprototype.tojson */
TO_BE_IMPLEMENTED(TemporalPlainDateTimePrototypeToJSON)
/* Temporal #sec-temporal.plaindatetime.prototype.tozoneddatetime */
TO_BE_IMPLEMENTED(TemporalPlainDateTimePrototypeToZonedDateTime)
/* Temporal #sec-temporal.plaindatetime.prototype.toplaindate */
TO_BE_IMPLEMENTED(TemporalPlainDateTimePrototypeToPlainDate)
/* Temporal #sec-temporal.plaindatetime.prototype.toplainyearmonth */
TO_BE_IMPLEMENTED(TemporalPlainDateTimePrototypeToPlainYearMonth)
/* Temporal #sec-temporal.plaindatetime.prototype.toplainmonthday */
TO_BE_IMPLEMENTED(TemporalPlainDateTimePrototypeToPlainMonthDay)
/* Temporal #sec-temporal.plaindatetime.prototype.toplaintime */
TO_BE_IMPLEMENTED(TemporalPlainDateTimePrototypeToPlainTime)

/* Temporal.ZonedDateTime */
/* Temporal #sec-temporal.zoneddatetime.from */
TO_BE_IMPLEMENTED(TemporalZonedDateTimeFrom)
/* Temporal #sec-temporal.zoneddatetime.compare */
TO_BE_IMPLEMENTED(TemporalZonedDateTimeCompare)
/* Temporal #sec-get-temporal.zoneddatetime.prototype.year */
TO_BE_IMPLEMENTED(TemporalZonedDateTimePrototypeYear)
/* Temporal #sec-get-temporal.zoneddatetime.prototype.month */
TO_BE_IMPLEMENTED(TemporalZonedDateTimePrototypeMonth)
/* Temporal #sec-get-temporal.zoneddatetime.prototype.monthcode */
TO_BE_IMPLEMENTED(TemporalZonedDateTimePrototypeMonthCode)
/* Temporal #sec-get-temporal.zoneddatetime.prototype.day */
TO_BE_IMPLEMENTED(TemporalZonedDateTimePrototypeDay)
/* Temporal #sec-get-temporal.zoneddatetime.prototype.dayofweek */
TO_BE_IMPLEMENTED(TemporalZonedDateTimePrototypeDayOfWeek)
/* Temporal #sec-get-temporal.zoneddatetime.prototype.dayofyear */
TO_BE_IMPLEMENTED(TemporalZonedDateTimePrototypeDayOfYear)
/* Temporal #sec-get-temporal.zoneddatetime.prototype.weekofyear */
TO_BE_IMPLEMENTED(TemporalZonedDateTimePrototypeWeekOfYear)
/* Temporal #sec-get-temporal.zoneddatetime.prototype.hoursinday */
TO_BE_IMPLEMENTED(TemporalZonedDateTimePrototypeHoursInDay)
/* Temporal #sec-get-temporal.zoneddatetime.prototype.daysinweek */
TO_BE_IMPLEMENTED(TemporalZonedDateTimePrototypeDaysInWeek)
/* Temporal #sec-get-temporal.zoneddatetime.prototype.daysinmonth */
TO_BE_IMPLEMENTED(TemporalZonedDateTimePrototypeDaysInMonth)
/* Temporal #sec-get-temporal.zoneddatetime.prototype.daysinyear */
TO_BE_IMPLEMENTED(TemporalZonedDateTimePrototypeDaysInYear)
/* Temporal #sec-get-temporal.zoneddatetime.prototype.monthsinyear */
TO_BE_IMPLEMENTED(TemporalZonedDateTimePrototypeMonthsInYear)
/* Temporal #sec-get-temporal.zoneddatetime.prototype.inleapyear */
TO_BE_IMPLEMENTED(TemporalZonedDateTimePrototypeInLeapYear)
/* Temporal #sec-get-temporal.zoneddatetime.prototype.offsetnanoseconds */
TO_BE_IMPLEMENTED(TemporalZonedDateTimePrototypeOffsetNanoseconds)
/* Temporal #sec-get-temporal.zoneddatetime.prototype.offset */
TO_BE_IMPLEMENTED(TemporalZonedDateTimePrototypeOffset)
/* Temporal #sec-temporal.zoneddatetime.prototype.with */
TO_BE_IMPLEMENTED(TemporalZonedDateTimePrototypeWith)
/* Temporal #sec-temporal.zoneddatetime.prototype.withplaintime */
TO_BE_IMPLEMENTED(TemporalZonedDateTimePrototypeWithPlainTime)
/* Temporal #sec-temporal.zoneddatetime.prototype.withplaindate */
TO_BE_IMPLEMENTED(TemporalZonedDateTimePrototypeWithPlainDate)
/* Temporal #sec-temporal.zoneddatetime.prototype.withtimezone */
TO_BE_IMPLEMENTED(TemporalZonedDateTimePrototypeWithTimeZone)
/* Temporal #sec-temporal.zoneddatetime.prototype.withcalendar */
TO_BE_IMPLEMENTED(TemporalZonedDateTimePrototypeWithCalendar)
/* Temporal #sec-temporal.zoneddatetime.prototype.add */
TO_BE_IMPLEMENTED(TemporalZonedDateTimePrototypeAdd)
/* Temporal #sec-temporal.zoneddatetime.prototype.subtract */
TO_BE_IMPLEMENTED(TemporalZonedDateTimePrototypeSubtract)
/* Temporal #sec-temporal.zoneddatetime.prototype.until */
TO_BE_IMPLEMENTED(TemporalZonedDateTimePrototypeUntil)
/* Temporal #sec-temporal.zoneddatetime.prototype.since */
TO_BE_IMPLEMENTED(TemporalZonedDateTimePrototypeSince)
/* Temporal #sec-temporal.zoneddatetime.prototype.round */
TO_BE_IMPLEMENTED(TemporalZonedDateTimePrototypeRound)
/* Temporal #sec-temporal.zoneddatetime.prototype.equals */
TO_BE_IMPLEMENTED(TemporalZonedDateTimePrototypeEquals)
/* Temporal #sec-temporal.zoneddatetime.prototype.tostring */
TO_BE_IMPLEMENTED(TemporalZonedDateTimePrototypeToString)
/* Temporal #sec-temporal.zonedddatetimeprototype.tojson */
TO_BE_IMPLEMENTED(TemporalZonedDateTimePrototypeToJSON)
/* Temporal #sec-temporal.zoneddatetime.prototype.startofday */
TO_BE_IMPLEMENTED(TemporalZonedDateTimePrototypeStartOfDay)
/* Temporal #sec-temporal.zoneddatetime.prototype.toinstant */
TO_BE_IMPLEMENTED(TemporalZonedDateTimePrototypeToInstant)
/* Temporal #sec-temporal.zoneddatetime.prototype.toplaindate */
TO_BE_IMPLEMENTED(TemporalZonedDateTimePrototypeToPlainDate)
/* Temporal #sec-temporal.zoneddatetime.prototype.toplaintime */
TO_BE_IMPLEMENTED(TemporalZonedDateTimePrototypeToPlainTime)
/* Temporal #sec-temporal.zoneddatetime.prototype.toplaindatetime */
TO_BE_IMPLEMENTED(TemporalZonedDateTimePrototypeToPlainDateTime)
/* Temporal #sec-temporal.zoneddatetime.prototype.toplainyearmonth */
TO_BE_IMPLEMENTED(TemporalZonedDateTimePrototypeToPlainYearMonth)
/* Temporal #sec-temporal.zoneddatetime.prototype.toplainmonthday */
TO_BE_IMPLEMENTED(TemporalZonedDateTimePrototypeToPlainMonthDay)

/* Temporal.Duration */
/* Temporal #sec-temporal.duration.from */
TO_BE_IMPLEMENTED(TemporalDurationFrom)
/* Temporal #sec-temporal.duration.compare */
TO_BE_IMPLEMENTED(TemporalDurationCompare)
/* Temporal #sec-temporal.duration.prototype.with */
TO_BE_IMPLEMENTED(TemporalDurationPrototypeWith)
/* Temporal #sec-temporal.duration.prototype.negated */
TO_BE_IMPLEMENTED(TemporalDurationPrototypeNegated)
/* Temporal #sec-temporal.duration.prototype.abs */
TO_BE_IMPLEMENTED(TemporalDurationPrototypeAbs)
/* Temporal #sec-temporal.duration.prototype.add */
TO_BE_IMPLEMENTED(TemporalDurationPrototypeAdd)
/* Temporal #sec-temporal.duration.prototype.subtract */
TO_BE_IMPLEMENTED(TemporalDurationPrototypeSubtract)
/* Temporal #sec-temporal.duration.prototype.round */
TO_BE_IMPLEMENTED(TemporalDurationPrototypeRound)
/* Temporal #sec-temporal.duration.prototype.total */
TO_BE_IMPLEMENTED(TemporalDurationPrototypeTotal)
/* Temporal #sec-temporal.duration.prototype.tostring */
TO_BE_IMPLEMENTED(TemporalDurationPrototypeToString)
/* Temporal #sec-temporal.duration.tojson */
TO_BE_IMPLEMENTED(TemporalDurationPrototypeToJSON)

/* Temporal.Instant */
/* Temporal #sec-temporal.instant.from */
TO_BE_IMPLEMENTED(TemporalInstantFrom)
/* Temporal #sec-temporal.instant.fromepochseconds */
TO_BE_IMPLEMENTED(TemporalInstantFromEpochSeconds)
/* Temporal #sec-temporal.instant.fromepochmilliseconds */
TO_BE_IMPLEMENTED(TemporalInstantFromEpochMilliseconds)
/* Temporal #sec-temporal.instant.fromepochmicroseconds */
TO_BE_IMPLEMENTED(TemporalInstantFromEpochMicroseconds)
/* Temporal #sec-temporal.instant.fromepochnanoseconds */
TO_BE_IMPLEMENTED(TemporalInstantFromEpochNanoseconds)
/* Temporal #sec-temporal.instant.compare */
TO_BE_IMPLEMENTED(TemporalInstantCompare)
/* Temporal #sec-temporal.instant.prototype.add */
TO_BE_IMPLEMENTED(TemporalInstantPrototypeAdd)
/* Temporal #sec-temporal.instant.prototype.subtract */
TO_BE_IMPLEMENTED(TemporalInstantPrototypeSubtract)
/* Temporal #sec-temporal.instant.prototype.until */
TO_BE_IMPLEMENTED(TemporalInstantPrototypeUntil)
/* Temporal #sec-temporal.instant.prototype.since */
TO_BE_IMPLEMENTED(TemporalInstantPrototypeSince)
/* Temporal #sec-temporal.instant.prototype.round */
TO_BE_IMPLEMENTED(TemporalInstantPrototypeRound)
/* Temporal #sec-temporal.instant.prototype.equals */
TO_BE_IMPLEMENTED(TemporalInstantPrototypeEquals)
/* Temporal #sec-temporal.instant.prototype.tostring */
TO_BE_IMPLEMENTED(TemporalInstantPrototypeToString)
/* Temporal #sec-temporal.instant.tojson */
TO_BE_IMPLEMENTED(TemporalInstantPrototypeToJSON)
/* Temporal #sec-temporal.instant.prototype.tozoneddatetime */
TO_BE_IMPLEMENTED(TemporalInstantPrototypeToZonedDateTime)
/* Temporal #sec-temporal.instant.prototype.tozoneddatetimeiso */
TO_BE_IMPLEMENTED(TemporalInstantPrototypeToZonedDateTimeISO)

/* Temporal.PlainYearMonth */
/* Temporal #sec-temporal.plainyearmonth.from */
TO_BE_IMPLEMENTED(TemporalPlainYearMonthFrom)
/* Temporal #sec-temporal.plainyearmonth.compare */
TO_BE_IMPLEMENTED(TemporalPlainYearMonthCompare)
/* Temporal #sec-get-temporal.plainyearmonth.prototype.year */
TO_BE_IMPLEMENTED(TemporalPlainYearMonthPrototypeYear)
/* Temporal #sec-get-temporal.plainyearmonth.prototype.month */
TO_BE_IMPLEMENTED(TemporalPlainYearMonthPrototypeMonth)
/* Temporal #sec-get-temporal.plainyearmonth.prototype.monthcode */
TO_BE_IMPLEMENTED(TemporalPlainYearMonthPrototypeMonthCode)
/* Temporal #sec-get-temporal.plainyearmonth.prototype.daysinyear */
TO_BE_IMPLEMENTED(TemporalPlainYearMonthPrototypeDaysInYear)
/* Temporal #sec-get-temporal.plainyearmonth.prototype.daysinmonth */
TO_BE_IMPLEMENTED(TemporalPlainYearMonthPrototypeDaysInMonth)
/* Temporal #sec-get-temporal.plainyearmonth.prototype.monthsinyear */
TO_BE_IMPLEMENTED(TemporalPlainYearMonthPrototypeMonthsInYear)
/* Temporal #sec-get-temporal.plainyearmonth.prototype.inleapyear */
TO_BE_IMPLEMENTED(TemporalPlainYearMonthPrototypeInLeapYear)
/* Temporal #sec-temporal.plainyearmonth.prototype.with */
TO_BE_IMPLEMENTED(TemporalPlainYearMonthPrototypeWith)
/* Temporal #sec-temporal.plainyearmonth.prototype.add */
TO_BE_IMPLEMENTED(TemporalPlainYearMonthPrototypeAdd)
/* Temporal #sec-temporal.plainyearmonth.prototype.subtract */
TO_BE_IMPLEMENTED(TemporalPlainYearMonthPrototypeSubtract)
/* Temporal #sec-temporal.plainyearmonth.prototype.until */
TO_BE_IMPLEMENTED(TemporalPlainYearMonthPrototypeUntil)
/* Temporal #sec-temporal.plainyearmonth.prototype.since */
TO_BE_IMPLEMENTED(TemporalPlainYearMonthPrototypeSince)
/* Temporal #sec-temporal.plainyearmonth.prototype.equals */
TO_BE_IMPLEMENTED(TemporalPlainYearMonthPrototypeEquals)
/* Temporal #sec-temporal.plainyearmonth.tostring */
TO_BE_IMPLEMENTED(TemporalPlainYearMonthPrototypeToString)
/* Temporal #sec-temporal.plainyearmonth.tojson */
TO_BE_IMPLEMENTED(TemporalPlainYearMonthPrototypeToJSON)
/* Temporal #sec-temporal.plainyearmonth.prototype.toplaindate */
TO_BE_IMPLEMENTED(TemporalPlainYearMonthPrototypeToPlainDate)

/* Temporal.PlainMonthDay */
/* Temporal #sec-temporal.plainmonthday.from */
TO_BE_IMPLEMENTED(TemporalPlainMonthDayFrom)

/* There is no compare for PlainMonthDay. See
 * https://github.com/tc39/proposal-temporal/issues/1547 */

/* Temporal #sec-get-temporal.plainmonthday.prototype.monthcode */
TO_BE_IMPLEMENTED(TemporalPlainMonthDayPrototypeMonthCode)
/* Temporal #sec-get-temporal.plainmonthday.prototype.day */
TO_BE_IMPLEMENTED(TemporalPlainMonthDayPrototypeDay)
/* Temporal #sec-temporal.plainmonthday.prototype.with */
TO_BE_IMPLEMENTED(TemporalPlainMonthDayPrototypeWith)
/* Temporal #sec-temporal.plainmonthday.prototype.equals */
TO_BE_IMPLEMENTED(TemporalPlainMonthDayPrototypeEquals)
/* Temporal #sec-temporal.plainmonthday.prototype.tostring */
TO_BE_IMPLEMENTED(TemporalPlainMonthDayPrototypeToString)
/* Temporal #sec-temporal.plainmonthday.tojson */
TO_BE_IMPLEMENTED(TemporalPlainMonthDayPrototypeToJSON)
/* Temporal #sec-temporal.plainmonthday.prototype.toplaindate */
TO_BE_IMPLEMENTED(TemporalPlainMonthDayPrototypeToPlainDate)

/* Temporal.TimeZone */
/* Temporal #sec-temporal.timezone.from */
TO_BE_IMPLEMENTED(TemporalTimeZoneFrom)
/* Temporal #sec-temporal.timezone.prototype.getoffsetnanosecondsfor */
TO_BE_IMPLEMENTED(TemporalTimeZonePrototypeGetOffsetNanosecondsFor)
/* Temporal #sec-temporal.timezone.prototype.getoffsetstringfor */
TO_BE_IMPLEMENTED(TemporalTimeZonePrototypeGetOffsetStringFor)
/* Temporal #sec-temporal.timezone.prototype.getplaindatetimefor */
TO_BE_IMPLEMENTED(TemporalTimeZonePrototypeGetPlainDateTimeFor)
/* Temporal #sec-temporal.timezone.prototype.getinstantfor */
TO_BE_IMPLEMENTED(TemporalTimeZonePrototypeGetInstantFor)
/* Temporal #sec-temporal.timezone.prototype.getpossibleinstantsfor */
TO_BE_IMPLEMENTED(TemporalTimeZonePrototypeGetPossibleInstantsFor)
/* Temporal #sec-temporal.timezone.prototype.getnexttransition */
TO_BE_IMPLEMENTED(TemporalTimeZonePrototypeGetNextTransition)
/* Temporal #sec-temporal.timezone.prototype.getprevioustransition */
TO_BE_IMPLEMENTED(TemporalTimeZonePrototypeGetPreviousTransition)
/* Temporal #sec-temporal.timezone.prototype.tojson */
TO_BE_IMPLEMENTED(TemporalTimeZonePrototypeToJSON)

/* Temporal.Calendar */
/* Temporal #sec-temporal.calendar.from */
TO_BE_IMPLEMENTED(TemporalCalendarFrom)
/* Temporal #sec-temporal.calendar.prototype.datefromfields */
TO_BE_IMPLEMENTED(TemporalCalendarPrototypeDateFromFields)
/* Temporal #sec-temporal.calendar.prototype.yearmonthfromfields */
TO_BE_IMPLEMENTED(TemporalCalendarPrototypeYearMonthFromFields)
/* Temporal #sec-temporal.calendar.prototype.monthdayfromfields */
TO_BE_IMPLEMENTED(TemporalCalendarPrototypeMonthDayFromFields)
/* Temporal #sec-temporal.calendar.prototype.dateadd */
TO_BE_IMPLEMENTED(TemporalCalendarPrototypeDateAdd)
/* Temporal #sec-temporal.calendar.prototype.dateuntil */
TO_BE_IMPLEMENTED(TemporalCalendarPrototypeDateUntil)
/* Temporal #sec-temporal.calendar.prototype.year */
TO_BE_IMPLEMENTED(TemporalCalendarPrototypeYear)
/* Temporal #sec-temporal.calendar.prototype.month */
TO_BE_IMPLEMENTED(TemporalCalendarPrototypeMonth)
/* Temporal #sec-temporal.calendar.prototype.monthcode */
TO_BE_IMPLEMENTED(TemporalCalendarPrototypeMonthCode)
/* Temporal #sec-temporal.calendar.prototype.day */
TO_BE_IMPLEMENTED(TemporalCalendarPrototypeDay)
/* Temporal #sec-temporal.calendar.prototype.dayofweek */
TO_BE_IMPLEMENTED(TemporalCalendarPrototypeDayOfWeek)
/* Temporal #sec-temporal.calendar.prototype.dayofyear */
TO_BE_IMPLEMENTED(TemporalCalendarPrototypeDayOfYear)
/* Temporal #sec-temporal.calendar.prototype.weekofyear */
TO_BE_IMPLEMENTED(TemporalCalendarPrototypeWeekOfYear)
/* Temporal #sec-temporal.calendar.prototype.daysinweek */
TO_BE_IMPLEMENTED(TemporalCalendarPrototypeDaysInWeek)
/* Temporal #sec-temporal.calendar.prototype.daysinmonth */
TO_BE_IMPLEMENTED(TemporalCalendarPrototypeDaysInMonth)
/* Temporal #sec-temporal.calendar.prototype.daysinyear */
TO_BE_IMPLEMENTED(TemporalCalendarPrototypeDaysInYear)
/* Temporal #sec-temporal.calendar.prototype.monthsinyear */
TO_BE_IMPLEMENTED(TemporalCalendarPrototypeMonthsInYear)
/* Temporal #sec-temporal.calendar.prototype.inleapyear */
TO_BE_IMPLEMENTED(TemporalCalendarPrototypeInLeapYear)
/* Temporal #sec-temporal.calendar.prototype.mergefields */
TO_BE_IMPLEMENTED(TemporalCalendarPrototypeMergeFields)
/* Temporal #sec-temporal.calendar.prototype.tojson */
TO_BE_IMPLEMENTED(TemporalCalendarPrototypeToJSON)

// to be switch to TFJ later
/* Temporal #sec-temporal.calendar.prototype.fields */
TO_BE_IMPLEMENTED(TemporalCalendarPrototypeFields)

#ifdef V8_INTL_SUPPORT
/* Temporal */
/* Temporal #sec-temporal.calendar.prototype.era */
TO_BE_IMPLEMENTED(TemporalCalendarPrototypeEra)
/* Temporal #sec-temporal.calendar.prototype.erayear */
TO_BE_IMPLEMENTED(TemporalCalendarPrototypeEraYear)
/* Temporal #sec-temporal.duration.prototype.tolocalestring */
TO_BE_IMPLEMENTED(TemporalDurationPrototypeToLocaleString)
/* Temporal #sec-temporal.instant.prototype.tolocalestring */
TO_BE_IMPLEMENTED(TemporalInstantPrototypeToLocaleString)
/* Temporal #sec-get-temporal.plaindate.prototype.era */
TO_BE_IMPLEMENTED(TemporalPlainDatePrototypeEra)
/* Temporal #sec-get-temporal.plaindate.prototype.erayear */
TO_BE_IMPLEMENTED(TemporalPlainDatePrototypeEraYear)
/* Temporal #sec-temporal.plaindate.prototype.tolocalestring */
TO_BE_IMPLEMENTED(TemporalPlainDatePrototypeToLocaleString)
/* Temporal #sec-get-temporal.plaindatetime.prototype.era */
TO_BE_IMPLEMENTED(TemporalPlainDateTimePrototypeEra)
/* Temporal #sec-get-temporal.plaindatetime.prototype.erayear */
TO_BE_IMPLEMENTED(TemporalPlainDateTimePrototypeEraYear)
/* Temporal #sec-temporal.plaindatetime.prototype.tolocalestring */
TO_BE_IMPLEMENTED(TemporalPlainDateTimePrototypeToLocaleString)
/* Temporal #sec-temporal.plainmonthday.prototype.tolocalestring */
TO_BE_IMPLEMENTED(TemporalPlainMonthDayPrototypeToLocaleString)
/* Temporal #sec-temporal.plaintime.prototype.tolocalestring */
TO_BE_IMPLEMENTED(TemporalPlainTimePrototypeToLocaleString)
/* Temporal #sec-get-temporal.plainyearmonth.prototype.era */
TO_BE_IMPLEMENTED(TemporalPlainYearMonthPrototypeEra)
/* Temporal #sec-get-temporal.plainyearmonth.prototype.erayear */
TO_BE_IMPLEMENTED(TemporalPlainYearMonthPrototypeEraYear)
/* Temporal #sec-temporal.plainyearmonth.prototype.tolocalestring */
TO_BE_IMPLEMENTED(TemporalPlainYearMonthPrototypeToLocaleString)
/* Temporal #sec-get-temporal.zoneddatetime.prototype.era */
TO_BE_IMPLEMENTED(TemporalZonedDateTimePrototypeEra)
/* Temporal #sec-get-temporal.zoneddatetime.prototype.erayear */
TO_BE_IMPLEMENTED(TemporalZonedDateTimePrototypeEraYear)
/* Temporal #sec-temporal.zoneddatetime.prototype.tolocalestring */
TO_BE_IMPLEMENTED(TemporalZonedDateTimePrototypeToLocaleString)
#endif  // V8_INTL_SUPPORT

#define TEMPORAL_CONSTRUCTOR1(T)                                              \
  BUILTIN(Temporal##T##Constructor) {                                         \
    HandleScope scope(isolate);                                               \
    RETURN_RESULT_OR_FAILURE(                                                 \
        isolate,                                                              \
        JSTemporal##T::Constructor(isolate, args.target(), args.new_target(), \
                                   args.atOrUndefined(isolate, 1)));          \
  }

#define TEMPORAL_ID_BY_TO_STRING(T)                               \
  BUILTIN(Temporal##T##PrototypeId) {                             \
    HandleScope scope(isolate);                                   \
    Handle<String> id;                                            \
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(                           \
        isolate, id, Object::ToString(isolate, args.receiver())); \
    return *id;                                                   \
  }

#define TEMPORAL_TO_STRING(T)                                       \
  BUILTIN(Temporal##T##PrototypeToString) {                         \
    HandleScope scope(isolate);                                     \
    const char* method = "Temporal." #T ".prototype.toString";      \
    CHECK_RECEIVER(JSTemporal##T, t, method);                       \
    Handle<Object> ret;                                             \
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(                             \
        isolate, ret, JSTemporal##T::ToString(isolate, t, method)); \
    return *ret;                                                    \
  }

#define TEMPORAL_PROTOTYPE_METHOD0(T, METHOD, name)                          \
  BUILTIN(Temporal##T##Prototype##METHOD) {                                  \
    HandleScope scope(isolate);                                              \
    const char* method = "Temporal." #T ".prototype." #name;                 \
    CHECK_RECEIVER(JSTemporal##T, obj, method);                              \
    RETURN_RESULT_OR_FAILURE(isolate, JSTemporal##T ::METHOD(isolate, obj)); \
  }

#define TEMPORAL_PROTOTYPE_METHOD3(T, METHOD, name)                          \
  BUILTIN(Temporal##T##Prototype##METHOD) {                                  \
    HandleScope scope(isolate);                                              \
    const char* method = "Temporal." #T ".prototype." #name;                 \
    CHECK_RECEIVER(JSTemporal##T, obj, method);                              \
    RETURN_RESULT_OR_FAILURE(                                                \
        isolate,                                                             \
        JSTemporal##T ::METHOD(isolate, obj, args.atOrUndefined(isolate, 1), \
                               args.atOrUndefined(isolate, 2),               \
                               args.atOrUndefined(isolate, 3)));             \
  }

#define TEMPORAL_VALUE_OF(T)                                                 \
  BUILTIN(Temporal##T##PrototypeValueOf) {                                   \
    HandleScope scope(isolate);                                              \
    THROW_NEW_ERROR_RETURN_FAILURE(                                          \
        isolate, NewTypeError(MessageTemplate::kDoNotUse,                    \
                              isolate->factory()->NewStringFromAsciiChecked( \
                                  "Temporal." #T ".prototype.valueOf"),      \
                              isolate->factory()->NewStringFromAsciiChecked( \
                                  "use Temporal." #T                         \
                                  ".prototype.compare for comparison.")));   \
  }

#define TEMPORAL_GET(T, METHOD, field)                            \
  BUILTIN(Temporal##T##Prototype##METHOD) {                       \
    HandleScope scope(isolate);                                   \
    const char* method = "get Temporal." #T ".prototype." #field; \
    CHECK_RECEIVER(JSTemporal##T, obj, method);                   \
    return obj->field();                                          \
  }

#define TEMPORAL_GET_NUMBER_AFTER_DIVID(T, M, field, scale, name)         \
  BUILTIN(Temporal##T##Prototype##M) {                                    \
    HandleScope scope(isolate);                                           \
    const char* method = "get Temporal." #T ".prototype." #name;          \
    CHECK_RECEIVER(JSTemporal##T, handle, method);                        \
    Handle<BigInt> value;                                                 \
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(                                   \
        isolate, value,                                                   \
        BigInt::Divide(isolate, Handle<BigInt>(handle->field(), isolate), \
                       BigInt::FromUint64(isolate, scale)));              \
    Handle<Object> number = BigInt::ToNumber(isolate, value);             \
    DCHECK(std::isfinite(number->Number()));                              \
    return *number;                                                       \
  }

#define TEMPORAL_GET_BIGINT_AFTER_DIVID(T, M, field, scale, name)         \
  BUILTIN(Temporal##T##Prototype##M) {                                    \
    HandleScope scope(isolate);                                           \
    const char* method = "get Temporal." #T ".prototype." #name;          \
    CHECK_RECEIVER(JSTemporal##T, handle, method);                        \
    Handle<BigInt> value;                                                 \
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(                                   \
        isolate, value,                                                   \
        BigInt::Divide(isolate, Handle<BigInt>(handle->field(), isolate), \
                       BigInt::FromUint64(isolate, scale)));              \
    return *value;                                                        \
  }

// PlainDate
BUILTIN(TemporalPlainDateConstructor) {
  HandleScope scope(isolate);
  RETURN_RESULT_OR_FAILURE(
      isolate, JSTemporalPlainDate::Constructor(
                   isolate, args.target(), args.new_target(),
                   args.atOrUndefined(isolate, 1),    // iso_year
                   args.atOrUndefined(isolate, 2),    // iso_month
                   args.atOrUndefined(isolate, 3),    // iso_day
                   args.atOrUndefined(isolate, 4)));  // calendar_like
}
TEMPORAL_GET(PlainDate, Calendar, calendar)
TEMPORAL_PROTOTYPE_METHOD0(PlainDate, GetISOFields, getISOFields)
TEMPORAL_VALUE_OF(PlainDate)

// PlainTime
BUILTIN(TemporalPlainTimeConstructor) {
  HandleScope scope(isolate);
  RETURN_RESULT_OR_FAILURE(isolate,
                           JSTemporalPlainTime::Constructor(
                               isolate, args.target(), args.new_target(),
                               args.atOrUndefined(isolate, 1),    // hour
                               args.atOrUndefined(isolate, 2),    // minute
                               args.atOrUndefined(isolate, 3),    // second
                               args.atOrUndefined(isolate, 4),    // millisecond
                               args.atOrUndefined(isolate, 5),    // microsecond
                               args.atOrUndefined(isolate, 6)));  // nanosecond
}
TEMPORAL_GET(PlainTime, Calendar, calendar)
TEMPORAL_PROTOTYPE_METHOD0(PlainTime, GetISOFields, getISOFields)
TEMPORAL_VALUE_OF(PlainTime)

// PlainDateTime
BUILTIN(TemporalPlainDateTimeConstructor) {
  HandleScope scope(isolate);
  RETURN_RESULT_OR_FAILURE(
      isolate, JSTemporalPlainDateTime::Constructor(
                   isolate, args.target(), args.new_target(),
                   args.atOrUndefined(isolate, 1),     // iso_year
                   args.atOrUndefined(isolate, 2),     // iso_month
                   args.atOrUndefined(isolate, 3),     // iso_day
                   args.atOrUndefined(isolate, 4),     // hour
                   args.atOrUndefined(isolate, 5),     // minute
                   args.atOrUndefined(isolate, 6),     // second
                   args.atOrUndefined(isolate, 7),     // millisecond
                   args.atOrUndefined(isolate, 8),     // microsecond
                   args.atOrUndefined(isolate, 9),     // nanosecond
                   args.atOrUndefined(isolate, 10)));  // calendar_like
}
TEMPORAL_GET(PlainDateTime, Calendar, calendar)
TEMPORAL_PROTOTYPE_METHOD0(PlainDateTime, GetISOFields, getISOFields)
TEMPORAL_VALUE_OF(PlainDateTime)

// PlainYearMonth
BUILTIN(TemporalPlainYearMonthConstructor) {
  HandleScope scope(isolate);
  RETURN_RESULT_OR_FAILURE(
      isolate, JSTemporalPlainYearMonth::Constructor(
                   isolate, args.target(), args.new_target(),
                   args.atOrUndefined(isolate, 1),    // iso_year
                   args.atOrUndefined(isolate, 2),    // iso_month
                   args.atOrUndefined(isolate, 3),    // calendar_like
                   args.atOrUndefined(isolate, 4)));  // reference_iso_day
}
TEMPORAL_GET(PlainYearMonth, Calendar, calendar)
TEMPORAL_PROTOTYPE_METHOD0(PlainYearMonth, GetISOFields, getISOFields)
TEMPORAL_VALUE_OF(PlainYearMonth)

// PlainMonthDay
BUILTIN(TemporalPlainMonthDayConstructor) {
  HandleScope scope(isolate);
  RETURN_RESULT_OR_FAILURE(
      isolate, JSTemporalPlainMonthDay::Constructor(
                   isolate, args.target(), args.new_target(),
                   args.atOrUndefined(isolate, 1),    // iso_month
                   args.atOrUndefined(isolate, 2),    // iso_day
                   args.atOrUndefined(isolate, 3),    // calendar_like
                   args.atOrUndefined(isolate, 4)));  // reference_iso_year
}
TEMPORAL_GET(PlainMonthDay, Calendar, calendar)
TEMPORAL_PROTOTYPE_METHOD0(PlainMonthDay, GetISOFields, getISOFields)
TEMPORAL_VALUE_OF(PlainMonthDay)

// ZonedDateTime

#define TEMPORAL_ZONED_DATE_TIME_GET_PREPARE(M)                               \
  HandleScope scope(isolate);                                                 \
  const char* method = "get Temporal.ZonedDateTime.prototype." #M;            \
  /* 1. Let zonedDateTime be the this value. */                               \
  /* 2. Perform ? RequireInternalSlot(zonedDateTime, */                       \
  /* [[InitializedTemporalZonedDateTime]]). */                                \
  CHECK_RECEIVER(JSTemporalZonedDateTime, zoned_date_time, method);           \
  /* 3. Let timeZone be zonedDateTime.[[TimeZone]]. */                        \
  Handle<JSReceiver> time_zone =                                              \
      handle(zoned_date_time->time_zone(), isolate);                          \
  /* 4. Let instant be ?                                   */                 \
  /* CreateTemporalInstant(zonedDateTime.[[Nanoseconds]]). */                 \
  Handle<JSTemporalInstant> instant;                                          \
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(                                         \
      isolate, instant,                                                       \
      temporal::CreateTemporalInstant(                                        \
          isolate, Handle<BigInt>(zoned_date_time->nanoseconds(), isolate))); \
  /* 5. Let calendar be zonedDateTime.[[Calendar]]. */                        \
  Handle<JSReceiver> calendar = handle(zoned_date_time->calendar(), isolate); \
  /* 6. Let temporalDateTime be ?                 */                          \
  /* BuiltinTimeZoneGetPlainDateTimeFor(timeZone, */                          \
  /* instant, calendar). */                                                   \
  Handle<JSTemporalPlainDateTime> temporal_date_time;                         \
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(                                         \
      isolate, temporal_date_time,                                            \
      temporal::BuiltinTimeZoneGetPlainDateTimeFor(                           \
          isolate, time_zone, instant, calendar, method));

#define TEMPORAL_ZONED_DATE_TIME_GET_INT_BY_FORWARD_TIME_ZONE(M, field) \
  BUILTIN(TemporalZonedDateTimePrototype##M) {                          \
    TEMPORAL_ZONED_DATE_TIME_GET_PREPARE(M)                             \
    /* 7. Return 𝔽(temporalDateTime.[[ #field ]]). */                \
    return Smi::FromInt(temporal_date_time->field());                   \
  }

BUILTIN(TemporalZonedDateTimeConstructor) {
  HandleScope scope(isolate);
  RETURN_RESULT_OR_FAILURE(
      isolate, JSTemporalZonedDateTime::Constructor(
                   isolate, args.target(), args.new_target(),
                   args.atOrUndefined(isolate, 1),    // epoch_nanoseconds
                   args.atOrUndefined(isolate, 2),    // time_zone_like
                   args.atOrUndefined(isolate, 3)));  // calendar_like
}
TEMPORAL_GET(ZonedDateTime, Calendar, calendar)
TEMPORAL_GET(ZonedDateTime, TimeZone, time_zone)
TEMPORAL_GET(ZonedDateTime, EpochNanoseconds, nanoseconds)
TEMPORAL_GET_NUMBER_AFTER_DIVID(ZonedDateTime, EpochSeconds, nanoseconds,
                                1000000000, epochSeconds)
TEMPORAL_GET_NUMBER_AFTER_DIVID(ZonedDateTime, EpochMilliseconds, nanoseconds,
                                1000000, epochMilliseconds)
TEMPORAL_GET_BIGINT_AFTER_DIVID(ZonedDateTime, EpochMicroseconds, nanoseconds,
                                1000, epochMicroseconds)
TEMPORAL_ZONED_DATE_TIME_GET_INT_BY_FORWARD_TIME_ZONE(Hour, iso_hour)
TEMPORAL_ZONED_DATE_TIME_GET_INT_BY_FORWARD_TIME_ZONE(Minute, iso_minute)
TEMPORAL_ZONED_DATE_TIME_GET_INT_BY_FORWARD_TIME_ZONE(Second, iso_second)
TEMPORAL_ZONED_DATE_TIME_GET_INT_BY_FORWARD_TIME_ZONE(Millisecond,
                                                      iso_millisecond)
TEMPORAL_ZONED_DATE_TIME_GET_INT_BY_FORWARD_TIME_ZONE(Microsecond,
                                                      iso_microsecond)
TEMPORAL_ZONED_DATE_TIME_GET_INT_BY_FORWARD_TIME_ZONE(Nanosecond,
                                                      iso_nanosecond)
TEMPORAL_PROTOTYPE_METHOD0(ZonedDateTime, GetISOFields, getISOFields)
TEMPORAL_VALUE_OF(ZonedDateTime)

// Duration
BUILTIN(TemporalDurationConstructor) {
  HandleScope scope(isolate);
  RETURN_RESULT_OR_FAILURE(
      isolate, JSTemporalDuration::Constructor(
                   isolate, args.target(), args.new_target(),
                   args.atOrUndefined(isolate, 1),     // years
                   args.atOrUndefined(isolate, 2),     // months
                   args.atOrUndefined(isolate, 3),     // weeks
                   args.atOrUndefined(isolate, 4),     // days
                   args.atOrUndefined(isolate, 5),     // hours
                   args.atOrUndefined(isolate, 6),     // minutes
                   args.atOrUndefined(isolate, 7),     // seconds
                   args.atOrUndefined(isolate, 8),     // milliseconds
                   args.atOrUndefined(isolate, 9),     // microseconds
                   args.atOrUndefined(isolate, 10)));  // nanoseconds
}
TEMPORAL_GET(Duration, Years, years)
TEMPORAL_GET(Duration, Months, months)
TEMPORAL_GET(Duration, Weeks, weeks)
TEMPORAL_GET(Duration, Days, days)
TEMPORAL_GET(Duration, Hours, hours)
TEMPORAL_GET(Duration, Minutes, minutes)
TEMPORAL_GET(Duration, Seconds, seconds)
TEMPORAL_GET(Duration, Milliseconds, milliseconds)
TEMPORAL_GET(Duration, Microseconds, microseconds)
TEMPORAL_GET(Duration, Nanoseconds, nanoseconds)
TEMPORAL_PROTOTYPE_METHOD0(Duration, Sign, sign)
TEMPORAL_PROTOTYPE_METHOD0(Duration, Blank, blank)
TEMPORAL_VALUE_OF(Duration)

// Instant
TEMPORAL_CONSTRUCTOR1(Instant)
TEMPORAL_VALUE_OF(Instant)
TEMPORAL_GET(Instant, EpochNanoseconds, nanoseconds)
TEMPORAL_GET_NUMBER_AFTER_DIVID(Instant, EpochSeconds, nanoseconds, 1000000000,
                                epochSeconds)
TEMPORAL_GET_NUMBER_AFTER_DIVID(Instant, EpochMilliseconds, nanoseconds,
                                1000000, epochMilliseconds)
TEMPORAL_GET_BIGINT_AFTER_DIVID(Instant, EpochMicroseconds, nanoseconds, 1000,
                                epochMicroseconds)

// Calendar
TEMPORAL_CONSTRUCTOR1(Calendar)
TEMPORAL_ID_BY_TO_STRING(Calendar)
TEMPORAL_TO_STRING(Calendar)

// TimeZone
TEMPORAL_CONSTRUCTOR1(TimeZone)
TEMPORAL_ID_BY_TO_STRING(TimeZone)
TEMPORAL_TO_STRING(TimeZone)

}  // namespace internal
}  // namespace v8
