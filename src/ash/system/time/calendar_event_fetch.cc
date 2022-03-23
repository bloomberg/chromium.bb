// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/time/calendar_event_fetch.h"

#include "ash/calendar/calendar_client.h"
#include "ash/calendar/calendar_controller.h"
#include "ash/shell.h"
#include "ash/system/time/calendar_utils.h"
#include "base/bind.h"
#include "base/check.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"

namespace ash {

CalendarEventFetch::CalendarEventFetch(
    const base::Time& start_of_month,
    FetchCompleteCallback complete_callback,
    FetchInternalErrorCallback internal_error_callback,
    const base::TickClock* tick_clock)
    : start_of_month_(start_of_month),
      complete_callback_(std::move(complete_callback)),
      internal_error_callback_(std::move(internal_error_callback)),
      fetch_start_time_(base::Time::Now()),
      timeout_(tick_clock) {
  CalendarClient* client = Shell::Get()->calendar_controller()->GetClient();
  DCHECK(client);

  const base::Time start_of_next_month =
      calendar_utils::GetStartOfNextMonthUTC(start_of_month);
  client->GetEventList(base::BindOnce(&CalendarEventFetch::OnResultReceived,
                                      weak_factory_.GetWeakPtr()),
                       start_of_month, start_of_next_month);

  timeout_.Start(
      FROM_HERE, calendar_utils::kEventFetchTimeout,
      base::BindOnce(&CalendarEventFetch::OnTimeout, base::Unretained(this)));
}

CalendarEventFetch::~CalendarEventFetch() = default;

void CalendarEventFetch::OnResultReceived(
    google_apis::ApiErrorCode error,
    std::unique_ptr<google_apis::calendar::EventList> events) {
  // Cancel timeout timer.
  timeout_.Stop();

  base::UmaHistogramTimes("Ash.Calendar.FetchEvents.FetchDuration",
                          base::Time::Now() - fetch_start_time_);
  base::UmaHistogramBoolean("Ash.Calendar.FetchEvents.Timeout", false);

  // IMPORTANT: 'this' is NOT safe to use after `complete_callback_` has been
  // executed, as the last thing it does is destroy its
  // std::unique_ptr<CalendarEventFetch> to this object.
  std::move(complete_callback_).Run(start_of_month_, error, events.get());
}

void CalendarEventFetch::OnTimeout() {
  base::UmaHistogramBoolean("Ash.Calendar.FetchEvents.Timeout", true);

  // IMPORTANT: 'this' is NOT safe to use after `internal_error_callback_` has
  // been executed, as the last thing it does is destroy its
  // std::unique_ptr<CalendarEventFetch> to this object.
  std::move(internal_error_callback_)
      .Run(start_of_month_, CalendarEventFetchInternalErrorCode::kTimeout);
}

}  // namespace ash
