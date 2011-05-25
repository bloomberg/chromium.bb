// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_QUOTA_QUOTA_TEMPORARY_STORAGE_EVICTOR_H_
#define WEBKIT_QUOTA_QUOTA_TEMPORARY_STORAGE_EVICTOR_H_
#pragma once

#include "base/memory/scoped_callback_factory.h"
#include "base/timer.h"
#include "webkit/quota/quota_types.h"

class GURL;

namespace base {
class MessageLoopProxy;
}

namespace quota {

class QuotaEvictionHandler;

class QuotaTemporaryStorageEvictor {
 public:
  QuotaTemporaryStorageEvictor(
      QuotaEvictionHandler* quota_eviction_handler,
      int64 interval_ms,
      scoped_refptr<base::MessageLoopProxy> io_thread);
  virtual ~QuotaTemporaryStorageEvictor();

  void Start();

  bool repeated_eviction() const { return repeated_eviction_; }
  void set_repeated_eviction(bool repeated_eviction) {
    repeated_eviction_ = repeated_eviction;
  }

 private:
  friend class QuotaTemporaryStorageEvictorTest;

  void StartEvictionTimerWithDelay(int delay_ms);
  void ConsiderEviction();
  void OnGotUsageAndQuotaForEviction(
      QuotaStatusCode status,
      int64 usage,
      int64 quota,
      int64 available_disk_space);
  void OnGotLRUOrigin(const GURL& origin);
  void OnEvictionComplete(QuotaStatusCode status);

  static const double kUsageRatioToStartEviction;
  static const int64 kDefaultMinAvailableDiskSpaceToStartEviction;

  const int64 min_available_disk_space_to_start_eviction_;

  // Not owned; quota_eviction_handler owns us.
  QuotaEvictionHandler* quota_eviction_handler_;

  int64 interval_ms_;
  bool repeated_eviction_;
  scoped_refptr<base::MessageLoopProxy> io_thread_;

  base::OneShotTimer<QuotaTemporaryStorageEvictor> timer_;

  base::ScopedCallbackFactory<QuotaTemporaryStorageEvictor> callback_factory_;

  DISALLOW_COPY_AND_ASSIGN(QuotaTemporaryStorageEvictor);
};

}  // namespace quota

#endif  // WEBKIT_QUOTA_QUOTA_TEMPORARY_STORAGE_EVICTOR_H_
