// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/quota/quota_temporary_storage_evictor.h"

#include "googleurl/src/gurl.h"
#include "webkit/quota/quota_manager.h"

namespace quota {

const double QuotaTemporaryStorageEvictor::kUsageRatioToStartEviction = 0.7;
const int64 QuotaTemporaryStorageEvictor::
    kDefaultMinAvailableDiskSpaceToStartEviction = 1000 * 1000 * 500;

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

void QuotaTemporaryStorageEvictor::Start() {
  DCHECK(CalledOnValidThread());
  StartEvictionTimerWithDelay(0);
}

void QuotaTemporaryStorageEvictor::StartEvictionTimerWithDelay(int delay_ms) {
  if (timer_.IsRunning())
    return;
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
    // TODO(dmikurube): Stop repeating if error happens frequently.
    StartEvictionTimerWithDelay(interval_ms_);
  }

  // TODO(dmikurube): Add stats on # of {error, eviction, skipped}.
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

  // Just calling ConsiderEviction() here is ok.  No need to deal with the
  // case that all of the Delete operations fail for a certain origin.  It
  // doesn't result in trying to evict the same origin permanently.  The
  // evictor skips origins which had deletion errors a few times.

  // We many need to get rid of more space so reconsider immediately.
  ConsiderEviction();
}

}  // namespace quota
