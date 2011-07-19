// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/quota/quota_temporary_storage_evictor.h"

#include "base/metrics/histogram.h"
#include "googleurl/src/gurl.h"
#include "webkit/quota/quota_manager.h"

#define UMA_HISTOGRAM_MBYTES(name, sample)          \
  UMA_HISTOGRAM_CUSTOM_COUNTS(                      \
      (name), static_cast<int>((sample) / kMBytes), \
      1, 10 * 1024 * 1024 /* 10TB */, 100)

#define UMA_HISTOGRAM_MINUTES(name, sample) \
  UMA_HISTOGRAM_CUSTOM_TIMES(             \
      (name), (sample),                   \
      base::TimeDelta::FromMinutes(1),    \
      base::TimeDelta::FromDays(1), 50)

namespace {
const int64 kMBytes = 1024 * 1024;
}

namespace quota {

const double QuotaTemporaryStorageEvictor::kUsageRatioToStartEviction = 0.7;
const int64 QuotaTemporaryStorageEvictor::
    kDefaultMinAvailableDiskSpaceToStartEviction = 1000 * 1000 * 500;
const int QuotaTemporaryStorageEvictor::kThresholdOfErrorsToStopEviction = 5;

const base::TimeDelta QuotaTemporaryStorageEvictor::kHistogramReportInterval =
  base::TimeDelta::FromMilliseconds(60 * 60 * 1000);  // 1 hour

QuotaTemporaryStorageEvictor::QuotaTemporaryStorageEvictor(
    QuotaEvictionHandler* quota_eviction_handler,
    int64 interval_ms)
    : min_available_disk_space_to_start_eviction_(
          kDefaultMinAvailableDiskSpaceToStartEviction),
      quota_eviction_handler_(quota_eviction_handler),
      interval_ms_(interval_ms),
      repeated_eviction_(true),
      callback_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
  DCHECK(quota_eviction_handler);
}

QuotaTemporaryStorageEvictor::~QuotaTemporaryStorageEvictor() {
}

void QuotaTemporaryStorageEvictor::GetStatistics(
    std::map<std::string, int64>* statistics) {
  DCHECK(statistics);

  (*statistics)["errors-on-evicting-origin"] =
      statistics_.num_errors_on_evicting_origin;
  (*statistics)["errors-on-getting-usage-and-quota"] =
      statistics_.num_errors_on_getting_usage_and_quota;
  (*statistics)["evicted-origins"] =
      statistics_.num_evicted_origins;
  (*statistics)["eviction-rounds"] =
      statistics_.num_eviction_rounds;
  (*statistics)["skipped-eviction-rounds"] =
      statistics_.num_skipped_eviction_rounds;
}

void QuotaTemporaryStorageEvictor::ReportHistogram() {
  UMA_HISTOGRAM_COUNTS("Quota.ErrorsOnEvictingOrigin",
                       statistics_.num_errors_on_evicting_origin);
  UMA_HISTOGRAM_COUNTS("Quota.ErrorsOnGettingUsageAndQuota",
                       statistics_.num_errors_on_getting_usage_and_quota);
  UMA_HISTOGRAM_COUNTS("Quota.EvictedOrigins",
                       statistics_.num_evicted_origins);
  UMA_HISTOGRAM_COUNTS("Quota.EvictionRounds",
                       statistics_.num_eviction_rounds);
  UMA_HISTOGRAM_COUNTS("Quota.SkippedEvictionRounds",
                       statistics_.num_skipped_eviction_rounds);

  statistics_ = Statistics();
}

void QuotaTemporaryStorageEvictor::Start() {
  DCHECK(CalledOnValidThread());
  StartEvictionTimerWithDelay(0);

  if (histogram_timer_.IsRunning())
    return;
  histogram_timer_.Start(kHistogramReportInterval, this,
                         &QuotaTemporaryStorageEvictor::ReportHistogram);
}

void QuotaTemporaryStorageEvictor::StartEvictionTimerWithDelay(int delay_ms) {
  if (eviction_timer_.IsRunning())
    return;
  num_evicted_origins_in_round_ = 0;
  usage_on_beginning_of_round_ = -1;
  time_of_beginning_of_round_ = base::Time::Now();
  ++statistics_.num_eviction_rounds;
  eviction_timer_.Start(base::TimeDelta::FromMilliseconds(delay_ms), this,
                        &QuotaTemporaryStorageEvictor::ConsiderEviction);
}

void QuotaTemporaryStorageEvictor::ConsiderEviction() {
  // Get usage and disk space, then continue.
  quota_eviction_handler_->GetUsageAndQuotaForEviction(callback_factory_.
      NewCallback(
          &QuotaTemporaryStorageEvictor::OnGotUsageAndQuotaForEviction));
}

void QuotaTemporaryStorageEvictor::OnGotUsageAndQuotaForEviction(
    QuotaStatusCode status,
    int64 usage,
    int64 unlimited_usage,
    int64 quota,
    int64 available_disk_space) {
  DCHECK(CalledOnValidThread());
  DCHECK_GE(usage, unlimited_usage);  // unlimited_usage is a subset of usage

  usage -= unlimited_usage;

  if (status != kQuotaStatusOk)
    ++statistics_.num_errors_on_getting_usage_and_quota;

  int64 amount_to_evict = std::max(
      usage - static_cast<int64>(quota * kUsageRatioToStartEviction),
      min_available_disk_space_to_start_eviction_ - available_disk_space);
  if (status == kQuotaStatusOk && amount_to_evict > 0) {
    // Space is getting tight. Get the least recently used origin and continue.
    // TODO(michaeln): if the reason for eviction is low physical disk space,
    // make 'unlimited' origins subject to eviction too.

    if (usage_on_beginning_of_round_ < 0) {
      usage_on_beginning_of_round_ = usage;
      UMA_HISTOGRAM_MBYTES("Quota.TemporaryStorageSizeToEvict",
                           amount_to_evict);
    }

    quota_eviction_handler_->GetLRUOrigin(kStorageTypeTemporary,
        callback_factory_.NewCallback(
            &QuotaTemporaryStorageEvictor::OnGotLRUOrigin));
  } else if (repeated_eviction_) {
    // No action required, sleep for a while and check again later.
    if (num_evicted_origins_in_round_ == 0) {
      ++statistics_.num_skipped_eviction_rounds;
    } else if (usage_on_beginning_of_round_ >= 0) {
      int64 evicted_bytes = usage_on_beginning_of_round_ - usage;
      base::Time now = base::Time::Now();
      UMA_HISTOGRAM_MBYTES("Quota.EvictedBytesPerRound", evicted_bytes);
      UMA_HISTOGRAM_COUNTS("Quota.NumberOfEvictedOriginsPerRound",
                          num_evicted_origins_in_round_);
      UMA_HISTOGRAM_TIMES("Quota.TimeSpentToAEvictionRound",
                          now - time_of_beginning_of_round_);
      if (!time_of_end_of_last_round_.is_null()) {
        UMA_HISTOGRAM_MINUTES("Quota.TimeDeltaOfEvictionRounds",
                            now - time_of_end_of_last_round_);
      }
      time_of_end_of_last_round_ = now;
    }
    if (statistics_.num_errors_on_getting_usage_and_quota <
        kThresholdOfErrorsToStopEviction) {
      StartEvictionTimerWithDelay(interval_ms_);
    } else {
      // TODO(dmikurube): Try restarting eviction after a while.
      LOG(WARNING) << "Stopped eviction of temporary storage due to errors "
                      "in GetUsageAndQuotaForEviction.";
    }
  }

  // TODO(dmikurube): Add error handling for the case status != kQuotaStatusOk.
}

void QuotaTemporaryStorageEvictor::OnGotLRUOrigin(const GURL& origin) {
  DCHECK(CalledOnValidThread());

  if (origin.is_empty()) {
    if (repeated_eviction_)
      StartEvictionTimerWithDelay(interval_ms_);
    return;
  }

  quota_eviction_handler_->EvictOriginData(origin, kStorageTypeTemporary,
      callback_factory_.NewCallback(
          &QuotaTemporaryStorageEvictor::OnEvictionComplete));
}

void QuotaTemporaryStorageEvictor::OnEvictionComplete(
    QuotaStatusCode status) {
  DCHECK(CalledOnValidThread());

  // Just calling ConsiderEviction() or StartEvictionTimerWithDelay() here is
  // ok.  No need to deal with the case that all of the Delete operations fail
  // for a certain origin.  It doesn't result in trying to evict the same
  // origin permanently.  The evictor skips origins which had deletion errors
  // a few times.

  if (status == kQuotaStatusOk) {
    ++statistics_.num_evicted_origins;
    ++num_evicted_origins_in_round_;
    // We many need to get rid of more space so reconsider immediately.
    ConsiderEviction();
  } else {
    ++statistics_.num_errors_on_evicting_origin;
    if (repeated_eviction_) {
      // Sleep for a while and retry again until we see too many errors.
      StartEvictionTimerWithDelay(interval_ms_);
    }
  }
}

}  // namespace quota
