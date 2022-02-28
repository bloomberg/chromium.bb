// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/geolocation/geolocation_controller.h"

#include <algorithm>

#include "ash/components/geolocation/geoposition.h"
#include "ash/system/time/time_of_day.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/time/clock.h"
#include "third_party/icu/source/i18n/astro.h"

namespace ash {

namespace {

// Delay to wait for a response to our geolocation request, if we get a response
// after which, we will consider the request a failure.
constexpr base::TimeDelta kGeolocationRequestTimeout = base::Seconds(60);

// Minimum delay to wait to fire a new request after a previous one failing.
constexpr base::TimeDelta kMinimumDelayAfterFailure = base::Seconds(60);

// Delay to wait to fire a new request after a previous one succeeding.
constexpr base::TimeDelta kNextRequestDelayAfterSuccess = base::Days(1);

// Default sunset time at 6:00 PM as an offset from 00:00.
constexpr int kDefaultSunsetTimeOffsetMinutes = 18 * 60;

// Default sunrise time at 6:00 AM as an offset from 00:00.
constexpr int kDefaultSunriseTimeOffsetMinutes = 6 * 60;

}  // namespace

GeolocationController::GeolocationController(
    scoped_refptr<network::SharedURLLoaderFactory> factory)
    : provider_(std::move(factory),
                SimpleGeolocationProvider::DefaultGeolocationProviderURL()),
      backoff_delay_(kMinimumDelayAfterFailure),
      timer_(std::make_unique<base::OneShotTimer>()) {
  auto* timezone_settings = chromeos::system::TimezoneSettings::GetInstance();
  current_timezone_id_ = timezone_settings->GetCurrentTimezoneID();
  timezone_settings->AddObserver(this);
}

GeolocationController::~GeolocationController() {
  chromeos::system::TimezoneSettings::GetInstance()->RemoveObserver(this);
}

void GeolocationController::AddObserver(Observer* observer) {
  const bool is_first_observer = observers_.empty();
  observers_.AddObserver(observer);
  if (is_first_observer)
    ScheduleNextRequest(base::Seconds(0));
}

void GeolocationController::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
  if (observers_.empty())
    timer_->Stop();
}

void GeolocationController::TimezoneChanged(const icu::TimeZone& timezone) {
  const std::u16string timezone_id =
      chromeos::system::TimezoneSettings::GetTimezoneID(timezone);
  if (current_timezone_id_ == timezone_id)
    return;

  current_timezone_id_ = timezone_id;

  // On timezone changes, request an immediate geoposition.
  ScheduleNextRequest(base::Seconds(0));
}

// static
base::TimeDelta
GeolocationController::GetNextRequestDelayAfterSuccessForTesting() {
  return kNextRequestDelayAfterSuccess;
}

void GeolocationController::SetTimerForTesting(
    std::unique_ptr<base::OneShotTimer> timer) {
  timer_ = std::move(timer);
}

void GeolocationController::SetClockForTesting(base::Clock* clock) {
  clock_ = clock;
}

void GeolocationController::SetCurrentTimezoneIdForTesting(
    const std::u16string& timezone_id) {
  current_timezone_id_ = timezone_id;
}

void GeolocationController::OnGeoposition(const Geoposition& position,
                                          bool server_error,
                                          const base::TimeDelta elapsed) {
  if (server_error || !position.Valid() ||
      elapsed > kGeolocationRequestTimeout) {
    VLOG(1) << "Failed to get a valid geoposition. Trying again later.";
    // Don't send invalid positions to ash.
    // On failure, we schedule another request after the current backoff delay.
    ScheduleNextRequest(backoff_delay_);

    // If another failure occurs next, our backoff delay should double.
    backoff_delay_ *= 2;
    return;
  }

  last_successful_geo_request_time_ = GetNow();

  geoposition_ = std::make_unique<SimpleGeoposition>();
  geoposition_->latitude = position.latitude;
  geoposition_->longitude = position.longitude;
  NotifyWithCurrentGeoposition(*geoposition_);

  // On success, reset the backoff delay to its minimum value, and schedule
  // another request.
  backoff_delay_ = kMinimumDelayAfterFailure;
  ScheduleNextRequest(kNextRequestDelayAfterSuccess);
}

base::Time GeolocationController::GetNow() const {
  return clock_ ? clock_->Now() : base::Time::Now();
}

void GeolocationController::ScheduleNextRequest(base::TimeDelta delay) {
  timer_->Start(FROM_HERE, delay, this,
                &GeolocationController::RequestGeoposition);
}

void GeolocationController::NotifyWithCurrentGeoposition(
    SimpleGeoposition position) {
  for (Observer& observer : observers_)
    observer.OnGeopositionChanged(position);
}

void GeolocationController::RequestGeoposition() {
  VLOG(1) << "Requesting a new geoposition";
  provider_.RequestGeolocation(
      kGeolocationRequestTimeout, /*send_wifi_access_points=*/false,
      /*send_cell_towers=*/false,
      base::BindOnce(&GeolocationController::OnGeoposition,
                     base::Unretained(this)));
}

base::Time GeolocationController::GetSunRiseSet(bool sunrise) const {
  if (!geoposition_) {
    LOG(ERROR) << "Invalid geoposition. Using default time for "
               << (sunrise ? "sunrise." : "sunset.");
    return sunrise ? TimeOfDay(kDefaultSunriseTimeOffsetMinutes).ToTimeToday()
                   : TimeOfDay(kDefaultSunsetTimeOffsetMinutes).ToTimeToday();
  }

  icu::CalendarAstronomer astro(geoposition_->longitude,
                                geoposition_->latitude);
  // For sunset and sunrise times calculations to be correct, the time of the
  // icu::CalendarAstronomer object should be set to a time near local noon.
  // This avoids having the computation flopping over into an adjacent day.
  // See the documentation of icu::CalendarAstronomer::getSunRiseSet().
  // Note that the icu calendar works with milliseconds since epoch, and
  // base::Time::FromDoubleT() / ToDoubleT() work with seconds since epoch.
  const double midday_today_sec = TimeOfDay(12 * 60).ToTimeToday().ToDoubleT();
  astro.setTime(midday_today_sec * 1000.0);
  const double sun_rise_set_ms = astro.getSunRiseSet(sunrise);
  return base::Time::FromDoubleT(sun_rise_set_ms / 1000.0);
}

}  // namespace ash