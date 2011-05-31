// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/quota/quota_temporary_storage_evictor.h"

#include "googleurl/src/gurl.h"
#include "webkit/quota/quota_manager.h"

namespace quota {

const char QuotaTemporaryStorageEvictor::
    kStatsLabelNumberOfErrorsOnEvictingOrigin[] =
        "errors-on-evicting-origin";
const char QuotaTemporaryStorageEvictor::
    kStatsLabelNumberOfErrorsOnGettingUsageAndQuota[] =
        "errors-on-getting-usage-and-quota";
const char QuotaTemporaryStorageEvictor::
    kStatsLabelNumberOfEvictedOrigins[] =
        "evicted-origins";
const char QuotaTemporaryStorageEvictor::
    kStatsLabelNumberOfEvictionRounds[] =
        "eviction-rounds";
const char QuotaTemporaryStorageEvictor::
    kStatsLabelNumberOfSkippedEvictionRounds[] =
        "skipped-eviction-rounds";

const double QuotaTemporaryStorageEvictor::kUsageRatioToStartEviction = 0.7;
const int64 QuotaTemporaryStorageEvictor::
    kDefaultMinAvailableDiskSpaceToStartEviction = 1000 * 1000 * 500;
const int QuotaTemporaryStorageEvictor::kThresholdOfErrorsToStopEviction = 5;

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

  (*statistics)[std::string(kStatsLabelNumberOfErrorsOnEvictingOrigin)] =
      statistics_.num_errors_on_evicting_origin;
  (*statistics)[std::string(kStatsLabelNumberOfErrorsOnGettingUsageAndQuota)] =
      statistics_.num_errors_on_getting_usage_and_quota;
  (*statistics)[std::string(kStatsLabelNumberOfEvictedOrigins)] =
      statistics_.num_evicted_origins;
  (*statistics)[std::string(kStatsLabelNumberOfEvictionRounds)] =
      statistics_.num_eviction_rounds;
  (*statistics)[std::string(kStatsLabelNumberOfSkippedEvictionRounds)] =
      statistics_.num_skipped_eviction_rounds;
}

void QuotaTemporaryStorageEvictor::Start() {
  DCHECK(CalledOnValidThread());
  StartEvictionTimerWithDelay(0);
}

void QuotaTemporaryStorageEvictor::StartEvictionTimerWithDelay(int delay_ms) {
  if (timer_.IsRunning())
    return;
  num_evicted_origins_in_round_ = 0;
  ++statistics_.num_eviction_rounds;
  timer_.Start(base::TimeDelta::FromMilliseconds(delay_ms), this,
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

  if (status == kQuotaStatusOk &&
      (usage > quota * kUsageRatioToStartEviction ||
       min_available_disk_space_to_start_eviction_ > available_disk_space)) {
    // Space is getting tight. Get the least recently used origin and continue.
    // TODO(michaeln): if the reason for eviction is low physical disk space,
    // make 'unlimited' origins subject to eviction too.
    quota_eviction_handler_->GetLRUOrigin(kStorageTypeTemporary,
        callback_factory_.NewCallback(
            &QuotaTemporaryStorageEvictor::OnGotLRUOrigin));
  } else if (repeated_eviction_) {
    // No action required, sleep for a while and check again later.
    if (num_evicted_origins_in_round_ == 0)
      ++statistics_.num_skipped_eviction_rounds;
    if (statistics_.num_errors_on_getting_usage_and_quota <
        kThresholdOfErrorsToStopEviction)
      StartEvictionTimerWithDelay(interval_ms_);
    else
      // TODO(dmikurube): Try restarting eviction after a while.
      LOG(WARNING) << "Stopped eviction of temporary storage due to errors "
                      "in GetUsageAndQuotaForEviction.";
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
