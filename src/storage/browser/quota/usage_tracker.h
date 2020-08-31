// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_QUOTA_USAGE_TRACKER_H_
#define STORAGE_BROWSER_QUOTA_USAGE_TRACKER_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "base/sequence_checker.h"
#include "storage/browser/quota/quota_callbacks.h"
#include "storage/browser/quota/quota_client.h"
#include "storage/browser/quota/quota_task.h"
#include "storage/browser/quota/special_storage_policy.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"
#include "url/origin.h"

namespace storage {

class ClientUsageTracker;

// A helper class that gathers and tracks the amount of data stored in
// all quota clients.
//
// Ownership: Each QuotaManager instance owns 3 instances of this class (one per
// storage type: Persistent, Temporary, Syncable).
// Thread-safety: All methods except the constructor must be called on the same
// sequence.
class COMPONENT_EXPORT(STORAGE_BROWSER) UsageTracker
    : public QuotaTaskObserver {
 public:
  UsageTracker(const std::vector<scoped_refptr<QuotaClient>>& clients,
               blink::mojom::StorageType type,
               SpecialStoragePolicy* special_storage_policy);
  ~UsageTracker() override;

  blink::mojom::StorageType type() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return type_;
  }

  void GetGlobalLimitedUsage(UsageCallback callback);
  void GetGlobalUsage(GlobalUsageCallback callback);
  void GetHostUsage(const std::string& host, UsageCallback callback);
  void GetHostUsageWithBreakdown(const std::string& host,
                                 UsageWithBreakdownCallback callback);
  void UpdateUsageCache(QuotaClientType client_type,
                        const url::Origin& origin,
                        int64_t delta);
  int64_t GetCachedUsage() const;
  std::map<std::string, int64_t> GetCachedHostsUsage() const;
  std::map<url::Origin, int64_t> GetCachedOriginsUsage() const;
  std::set<url::Origin> GetCachedOrigins() const;
  bool IsWorking() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return !global_usage_callbacks_.empty() || !host_usage_callbacks_.empty();
  }

  void SetUsageCacheEnabled(QuotaClientType client_type,
                            const url::Origin& origin,
                            bool enabled);

 private:
  struct AccumulateInfo;
  friend class ClientUsageTracker;

  void AccumulateClientGlobalLimitedUsage(AccumulateInfo* info,
                                          int64_t limited_usage);
  void AccumulateClientGlobalUsage(AccumulateInfo* info,
                                   int64_t usage,
                                   int64_t unlimited_usage);
  void AccumulateClientHostUsage(base::OnceClosure callback,
                                 AccumulateInfo* info,
                                 const std::string& host,
                                 QuotaClientType client,
                                 int64_t usage);
  void FinallySendHostUsageWithBreakdown(AccumulateInfo* info,
                                         const std::string& host);

  const blink::mojom::StorageType type_;
  std::map<QuotaClientType, std::unique_ptr<ClientUsageTracker>>
      client_tracker_map_;

  std::vector<UsageCallback> global_limited_usage_callbacks_;
  std::vector<GlobalUsageCallback> global_usage_callbacks_;
  std::map<std::string, std::vector<UsageWithBreakdownCallback>>
      host_usage_callbacks_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<UsageTracker> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(UsageTracker);
};

}  // namespace storage

#endif  // STORAGE_BROWSER_QUOTA_USAGE_TRACKER_H_
