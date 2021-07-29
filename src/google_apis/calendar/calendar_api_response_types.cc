// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/calendar/calendar_api_response_types.h"

#include <stddef.h>

#include <memory>

#include "base/json/json_value_converter.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "google_apis/drive/time_util.h"

namespace google_apis {

namespace calendar {

namespace {

// EventList
const char kKind[] = "kind";
const char kTimeZone[] = "timeZone";
const char kETag[] = "etag";
const char kItems[] = "items";
const char kCalendarEventListKind[] = "calendar#events";

// DateTime
const char kDateTime[] = "dateTime";

// CalendarEvent
const char kId[] = "id";
const char kSummary[] = "summary";
const char kStart[] = "start";
const char kEnd[] = "end";
const char kColorId[] = "colorId";
const char kStatus[] = "status";
const char kHtmlLink[] = "htmlLink";
const char kCalendarEventKind[] = "calendar#event";

// Checks if the JSON is expected kind.
// TODO(https://crbug.com/1222483): move this to common and share with drive api
// parsers.
bool IsResourceKindExpected(const base::Value& value,
                            const std::string& expected_kind) {
  const std::string* kind = value.FindStringKey(kKind);
  return kind && *kind == expected_kind;
}
}  // namespace

DateTime::DateTime() = default;

DateTime::DateTime(const DateTime& src) = default;

DateTime& DateTime::operator=(const DateTime& src) = default;

DateTime::~DateTime() = default;

// static
void DateTime::RegisterJSONConverter(
    base::JSONValueConverter<DateTime>* converter) {
  converter->RegisterCustomField<base::Time>(kDateTime, &DateTime::date_time_,
                                             &util::GetTimeFromString);
}

// static
bool DateTime::CreateDateTimeFromValue(const base::Value* value,
                                       DateTime* time) {
  base::JSONValueConverter<DateTime> converter;
  if (!converter.Convert(*value, time)) {
    DVLOG(1) << "Unable to create: Invalid DateTime JSON!";
    return false;
  }
  return true;
}

CalendarEvent::CalendarEvent() = default;

CalendarEvent::~CalendarEvent() = default;

// static
void CalendarEvent::RegisterJSONConverter(
    base::JSONValueConverter<CalendarEvent>* converter) {
  converter->RegisterStringField(kId, &CalendarEvent::id_);
  converter->RegisterStringField(kSummary, &CalendarEvent::summary_);
  converter->RegisterStringField(kHtmlLink, &CalendarEvent::html_link_);
  converter->RegisterStringField(kColorId, &CalendarEvent::color_id_);
  converter->RegisterStringField(kStatus, &CalendarEvent::status_);
  converter->RegisterCustomValueField(kStart, &CalendarEvent::start_time_,
                                      &DateTime::CreateDateTimeFromValue);
  converter->RegisterCustomValueField(kEnd, &CalendarEvent::end_time_,
                                      &DateTime::CreateDateTimeFromValue);
}

// static
std::unique_ptr<CalendarEvent> CalendarEvent::CreateFrom(
    const base::Value& value) {
  auto event = std::make_unique<CalendarEvent>();
  base::JSONValueConverter<CalendarEvent> converter;
  if (!IsResourceKindExpected(value, kCalendarEventKind) ||
      !converter.Convert(value, event.get())) {
    DVLOG(1) << "Unable to create: Invalid CalendarEvent JSON!";
    return nullptr;
  }

  return event;
}

EventList::EventList() = default;

EventList::~EventList() = default;

// static
void EventList::RegisterJSONConverter(
    base::JSONValueConverter<EventList>* converter) {
  converter->RegisterStringField(kTimeZone, &EventList::time_zone_);
  converter->RegisterStringField(kETag, &EventList::etag_);
  converter->RegisterStringField(kKind, &EventList::kind_);
  converter->RegisterRepeatedMessage<CalendarEvent>(kItems, &EventList::items_);
}

// static
std::unique_ptr<EventList> EventList::CreateFrom(const base::Value& value) {
  auto events = std::make_unique<EventList>();
  base::JSONValueConverter<EventList> converter;
  if (!IsResourceKindExpected(value, kCalendarEventListKind) ||
      !converter.Convert(value, events.get())) {
    DVLOG(1) << "Unable to create: Invalid EventList JSON!";
    return nullptr;
  }
  return events;
}

}  // namespace calendar
}  // namespace google_apis
