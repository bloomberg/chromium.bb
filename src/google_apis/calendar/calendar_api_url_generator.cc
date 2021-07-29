// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/calendar/calendar_api_url_generator.h"

#include "google_apis/drive/time_util.h"
#include "net/base/url_util.h"

namespace google_apis {

namespace calendar {

namespace {

// Hard coded URLs for communication with a google calendar server.
const char kCalendarV3EventsUrl[] = "calendar/v3/calendars/primary/events";
const char kCalendarV3ColorUrl[] = "calendar/v3/colors";
const char kTimeMaxParameterName[] = "timeMax";
const char kTimeMinParameterName[] = "timeMin";

}  // namespace

CalendarApiUrlGenerator::CalendarApiUrlGenerator() = default;

CalendarApiUrlGenerator::CalendarApiUrlGenerator(
    const CalendarApiUrlGenerator& src) = default;

CalendarApiUrlGenerator& CalendarApiUrlGenerator::operator=(
    const CalendarApiUrlGenerator& src) = default;

CalendarApiUrlGenerator::~CalendarApiUrlGenerator() = default;

// TODO(https://crbug.com/1222483): get this from GaiaUrls class instead.
// The same for the DriveApiUrlGenerator.
const char CalendarApiUrlGenerator::kBaseUrlForProduction[] =
    "https://www.googleapis.com";

GURL CalendarApiUrlGenerator::GetCalendarEventListUrl(
    const base::Time& start_time,
    const base::Time& end_time) const {
  GURL url = base_url_.Resolve(kCalendarV3EventsUrl);
  std::string start_time_string = util::FormatTimeAsString(start_time);
  std::string end_time_string = util::FormatTimeAsString(end_time);
  url = net::AppendOrReplaceQueryParameter(url, kTimeMinParameterName,
                                           start_time_string);
  url = net::AppendOrReplaceQueryParameter(url, kTimeMaxParameterName,
                                           end_time_string);
  return url;
}

GURL CalendarApiUrlGenerator::GetCalendarColorListUrl() const {
  GURL url = base_url_.Resolve(kCalendarV3ColorUrl);
  return url;
}

}  // namespace calendar
}  // namespace google_apis
