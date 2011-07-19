// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_QUOTA_QUOTA_TEMPORARY_STORAGE_EVICTOR_H_
#define WEBKIT_QUOTA_QUOTA_TEMPORARY_STORAGE_EVICTOR_H_
#pragma once

#include <map>
#include <string>

#include "base/memory/scoped_callback_factory.h"
#include "base/threading/non_thread_safe.h"
#include "base/timer.h"
#include "webkit/quota/quota_types.h"

class GURL;

namespace base {
class MessageLoopProxy;
}

namespace quota {

class QuotaEvictionHandler;

class QuotaTemporaryStorageEvictor : public base::NonThreadSafe {
 public:
  struct Statistics {
    Statistics()
        : num_errors_on_evicting_origin(0),
          num_errors_on_getting_usage_and_quota(0),
          num_evicted_origins(0),
          num_eviction_rounds(0),
          num_skipped_eviction_rounds(0) {}
    int64 num_errors_on_evicting_origin;
    int64 num_errors_on_getting_usage_and_quota;
    int64 num_evicted_origins;
    int64 num_eviction_rounds;
    int64 num_skipped_eviction_rounds;
  };

  QuotaTemporaryStorageEvictor(
      QuotaEvictionHandler* quota_eviction_handler,
      int64 interval_ms);
  virtual ~QuotaTemporaryStorageEvictor();

  void GetStatistics(std::map<std::string, int64>* statistics);
  void ReportHistogram();
  void Start();

 private:
  friend class QuotaTemporaryStorageEvictorTest;

  void StartEvictionTimerWithDelay(int delay_ms);
  void ConsiderEviction();
  void OnGotUsageAndQuotaForEviction(
      QuotaStatusCode status,
      int64 usage,
      int64 unlimited_usage,
      int64 quota,
      int64 available_disk_space);
  void OnGotLRUOrigin(const GURL& origin);
  void OnEvictionComplete(QuotaStatusCode status);

  // This is only used for tests.
  void set_repeated_eviction(bool repeated_eviction) {
    repeated_eviction_ = repeated_eviction;
  }

  static const double kUsageRatioToStartEviction;
  static const int64 kDefaultMinAvailableDiskSpaceToStartEviction;
  static const int kThresholdOfErrorsToStopEviction;
  static const base::TimeDelta kHistogramReportInterval;

  const int64 min_available_disk_space_to_start_eviction_;

  // Not owned; quota_eviction_handler owns us.
  QuotaEvictionHandler* quota_eviction_handler_;

  Statistics statistics_;

  int64 interval_ms_;
  bool repeated_eviction_;
  int num_evicted_origins_in_round_;

  int64 usage_on_beginning_of_round_;
  base::Time time_of_beginning_of_round_;
  base::Time time_of_end_of_last_round_;

  base::OneShotTimer<QuotaTemporaryStorageEvictor> eviction_timer_;
  base::RepeatingTimer<QuotaTemporaryStorageEvictor> histogram_timer_;

  base::ScopedCallbackFactory<QuotaTemporaryStorageEvictor> callback_factory_;

  DISALLOW_COPY_AND_ASSIGN(QuotaTemporaryStorageEvictor);
};

}  // namespace quota

#endif  // WEBKIT_QUOTA_QUOTA_TEMPORARY_STORAGE_EVICTOR_H_
