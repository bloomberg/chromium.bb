// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/scheduling/nearby_share_scheduler_base.h"

#include <algorithm>
#include <sstream>
#include <utility>

#include "base/i18n/time_formatting.h"
#include "base/json/values_util.h"
#include "base/numerics/clamped_math.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/clock.h"
#include "chrome/browser/nearby_sharing/logging/logging.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/network_service_instance.h"

namespace {

constexpr base::TimeDelta kZeroTimeDelta = base::Seconds(0);
constexpr base::TimeDelta kBaseRetryDelay = base::Seconds(5);
constexpr base::TimeDelta kMaxRetryDelay = base::Hours(1);

const char kLastAttemptTimeKeyName[] = "a";
const char kLastSuccessTimeKeyName[] = "s";
const char kNumConsecutiveFailuresKeyName[] = "f";
const char kHasPendingImmediateRequestKeyName[] = "p";
const char kIsWaitingForResultKeyName[] = "w";

}  // namespace

NearbyShareSchedulerBase::NearbyShareSchedulerBase(bool retry_failures,
                                                   bool require_connectivity,
                                                   const std::string& pref_name,
                                                   PrefService* pref_service,
                                                   OnRequestCallback callback,
                                                   const base::Clock* clock)
    : NearbyShareScheduler(std::move(callback)),
      retry_failures_(retry_failures),
      require_connectivity_(require_connectivity),
      pref_name_(pref_name),
      pref_service_(pref_service),
      clock_(clock) {
  DCHECK(pref_service_);

  InitializePersistedRequest();

  if (require_connectivity_) {
    content::GetNetworkConnectionTracker()->AddNetworkConnectionObserver(this);
  }
}

NearbyShareSchedulerBase::~NearbyShareSchedulerBase() {
  if (require_connectivity_) {
    content::GetNetworkConnectionTracker()->RemoveNetworkConnectionObserver(
        this);
  }
}

void NearbyShareSchedulerBase::MakeImmediateRequest() {
  timer_.Stop();
  SetHasPendingImmediateRequest(true);
  Reschedule();
}

void NearbyShareSchedulerBase::HandleResult(bool success) {
  base::Time now = clock_->Now();
  SetLastAttemptTime(now);

  NS_LOG(VERBOSE) << "Nearby Share scheduler \"" << pref_name_
                  << "\" latest attempt " << (success ? "succeeded" : "failed");

  if (success) {
    SetLastSuccessTime(now);
    SetNumConsecutiveFailures(0);
  } else {
    SetNumConsecutiveFailures(base::ClampAdd(GetNumConsecutiveFailures(), 1));
  }

  SetIsWaitingForResult(false);
  Reschedule();
  PrintSchedulerState();
}

void NearbyShareSchedulerBase::Reschedule() {
  if (!is_running())
    return;

  timer_.Stop();

  absl::optional<base::TimeDelta> delay = GetTimeUntilNextRequest();
  if (!delay)
    return;

  timer_.Start(FROM_HERE, *delay,
               base::BindOnce(&NearbyShareSchedulerBase::OnTimerFired,
                              base::Unretained(this)));
}

absl::optional<base::Time> NearbyShareSchedulerBase::GetLastSuccessTime()
    const {
  return base::ValueToTime(pref_service_->GetDictionary(pref_name_)
                               ->FindKey(kLastSuccessTimeKeyName));
}

absl::optional<base::TimeDelta>
NearbyShareSchedulerBase::GetTimeUntilNextRequest() const {
  if (!is_running() || IsWaitingForResult())
    return absl::nullopt;

  if (HasPendingImmediateRequest())
    return kZeroTimeDelta;

  base::Time now = clock_->Now();

  // Recover from failures using exponential backoff strategy if necessary.
  absl::optional<base::TimeDelta> time_until_retry = TimeUntilRetry(now);
  if (time_until_retry)
    return time_until_retry;

  // Schedule the periodic request if applicable.
  return TimeUntilRecurringRequest(now);
}

bool NearbyShareSchedulerBase::IsWaitingForResult() const {
  return pref_service_->GetDictionary(pref_name_)
      ->FindBoolKey(kIsWaitingForResultKeyName)
      .value_or(false);
}

size_t NearbyShareSchedulerBase::GetNumConsecutiveFailures() const {
  const std::string* str = pref_service_->GetDictionary(pref_name_)
                               ->FindStringKey(kNumConsecutiveFailuresKeyName);
  if (!str)
    return 0;

  size_t num_failures = 0;
  if (!base::StringToSizeT(*str, &num_failures))
    return 0;

  return num_failures;
}

void NearbyShareSchedulerBase::OnStart() {
  Reschedule();
  NS_LOG(VERBOSE) << "Starting Nearby Share scheduler \"" << pref_name_ << "\"";
  PrintSchedulerState();
}

void NearbyShareSchedulerBase::OnStop() {
  timer_.Stop();
}

void NearbyShareSchedulerBase::OnConnectionChanged(
    network::mojom::ConnectionType type) {
  if (content::GetNetworkConnectionTracker()->IsOffline())
    return;

  Reschedule();
}

absl::optional<base::Time> NearbyShareSchedulerBase::GetLastAttemptTime()
    const {
  return base::ValueToTime(pref_service_->GetDictionary(pref_name_)
                               ->FindKey(kLastAttemptTimeKeyName));
}

bool NearbyShareSchedulerBase::HasPendingImmediateRequest() const {
  return pref_service_->GetDictionary(pref_name_)
      ->FindBoolKey(kHasPendingImmediateRequestKeyName)
      .value_or(false);
}

void NearbyShareSchedulerBase::SetLastAttemptTime(
    base::Time last_attempt_time) {
  DictionaryPrefUpdate(pref_service_, pref_name_)
      .Get()
      ->SetKey(kLastAttemptTimeKeyName, base::TimeToValue(last_attempt_time));
}

void NearbyShareSchedulerBase::SetLastSuccessTime(
    base::Time last_success_time) {
  DictionaryPrefUpdate(pref_service_, pref_name_)
      .Get()
      ->SetKey(kLastSuccessTimeKeyName, base::TimeToValue(last_success_time));
}

void NearbyShareSchedulerBase::SetNumConsecutiveFailures(size_t num_failures) {
  DictionaryPrefUpdate(pref_service_, pref_name_)
      .Get()
      ->SetStringKey(kNumConsecutiveFailuresKeyName,
                     base::NumberToString(num_failures));
}

void NearbyShareSchedulerBase::SetHasPendingImmediateRequest(
    bool has_pending_immediate_request) {
  DictionaryPrefUpdate(pref_service_, pref_name_)
      .Get()
      ->SetBoolKey(kHasPendingImmediateRequestKeyName,
                   has_pending_immediate_request);
}

void NearbyShareSchedulerBase::SetIsWaitingForResult(
    bool is_waiting_for_result) {
  DictionaryPrefUpdate(pref_service_, pref_name_)
      .Get()
      ->SetBoolKey(kIsWaitingForResultKeyName, is_waiting_for_result);
}

void NearbyShareSchedulerBase::InitializePersistedRequest() {
  if (IsWaitingForResult()) {
    SetHasPendingImmediateRequest(true);
    SetIsWaitingForResult(false);
  }
}

absl::optional<base::TimeDelta> NearbyShareSchedulerBase::TimeUntilRetry(
    base::Time now) const {
  if (!retry_failures_)
    return absl::nullopt;

  size_t num_failures = GetNumConsecutiveFailures();
  if (num_failures == 0)
    return absl::nullopt;

  // The exponential back off is
  //
  //   base * 2^(num_failures - 1)
  //
  // up to a fixed maximum retry delay.
  base::TimeDelta delay =
      std::min(kMaxRetryDelay, kBaseRetryDelay * (1 << (num_failures - 1)));

  base::TimeDelta time_elapsed_since_last_attempt = now - *GetLastAttemptTime();

  return std::max(kZeroTimeDelta, delay - time_elapsed_since_last_attempt);
}

void NearbyShareSchedulerBase::OnTimerFired() {
  DCHECK(is_running());
  if (require_connectivity_ &&
      content::GetNetworkConnectionTracker()->IsOffline()) {
    return;
  }

  SetIsWaitingForResult(true);
  SetHasPendingImmediateRequest(false);
  NotifyOfRequest();
}

void NearbyShareSchedulerBase::PrintSchedulerState() const {
  absl::optional<base::Time> last_attempt_time = GetLastAttemptTime();
  absl::optional<base::Time> last_success_time = GetLastSuccessTime();
  absl::optional<base::TimeDelta> time_until_next_request =
      GetTimeUntilNextRequest();

  std::stringstream ss;
  ss << "State of Nearby Share scheduler \"" << pref_name_ << "\":"
     << "\n  Last attempt time: ";
  if (last_attempt_time) {
    ss << base::TimeFormatShortDateAndTimeWithTimeZone(*last_attempt_time);
  } else {
    ss << "Never";
  }

  ss << "\n  Last success time: ";
  if (last_success_time) {
    ss << base::TimeFormatShortDateAndTimeWithTimeZone(*last_success_time);
  } else {
    ss << "Never";
  }

  ss << "\n  Time until next request: ";
  if (time_until_next_request) {
    std::u16string next_request_delay;
    bool success = base::TimeDurationFormatWithSeconds(
        *time_until_next_request,
        base::DurationFormatWidth::DURATION_WIDTH_NARROW, &next_request_delay);
    if (success) {
      ss << next_request_delay;
    }
  } else {
    ss << "Never";
  }

  ss << "\n  Is waiting for result? " << (IsWaitingForResult() ? "Yes" : "No");
  ss << "\n  Pending immediate request? "
     << (HasPendingImmediateRequest() ? "Yes" : "No");
  ss << "\n  Num consecutive failures: " << GetNumConsecutiveFailures();

  NS_LOG(VERBOSE) << ss.str();
}
