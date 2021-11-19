// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/quota/quota_manager_impl.h"

#include <stddef.h>
#include <stdint.h>
#include <sstream>

#include <algorithm>
#include <functional>
#include <limits>
#include <memory>
#include <utility>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/rand_util.h"
#include "base/sequence_checker.h"
#include "base/strings/string_number_conversions.h"
#include "base/system/sys_info.h"
#include "base/task/post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_runner_util.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/types/pass_key.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "components/services/storage/public/mojom/quota_client.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "storage/browser/quota/client_usage_tracker.h"
#include "storage/browser/quota/quota_client_type.h"
#include "storage/browser/quota/quota_features.h"
#include "storage/browser/quota/quota_macros.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/quota/quota_override_handle.h"
#include "storage/browser/quota/quota_temporary_storage_evictor.h"
#include "storage/browser/quota/usage_tracker.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom-shared.h"
#include "url/origin.h"

using ::blink::StorageKey;
using ::blink::mojom::StorageType;

namespace storage {

namespace {

constexpr int64_t kReportHistogramInterval = 60 * 60 * 1000;  // 1 hour

// Take action on write errors if there is <= 2% disk space
// available.
constexpr double kStoragePressureThresholdRatio = 0.02;

// Limit how frequently QuotaManagerImpl polls for free disk space when
// only using that information to identify storage pressure.
constexpr base::TimeDelta kStoragePressureCheckDiskStatsInterval =
    base::Minutes(5);

// Modifies a given value by a uniformly random amount from
// -percent to +percent.
int64_t RandomizeByPercent(int64_t value, int percent) {
  double random_percent = (base::RandDouble() - 0.5) * percent * 2;
  return value * (1 + (random_percent / 100.0));
}
}  // namespace

// Heuristics: assuming average cloud server allows a few Gigs storage
// on the server side and the storage needs to be shared for user data
// and by multiple apps.
int64_t QuotaManagerImpl::kSyncableStorageDefaultHostQuota = 500 * kMBytes;

namespace {

bool IsSupportedType(StorageType type) {
  return type == StorageType::kTemporary || type == StorageType::kPersistent ||
         type == StorageType::kSyncable;
}

bool IsSupportedIncognitoType(StorageType type) {
  return type == StorageType::kTemporary || type == StorageType::kPersistent;
}

QuotaErrorOr<BucketInfo> GetOrCreateBucketOnDBThread(
    const StorageKey& storage_key,
    const std::string& bucket_name,
    QuotaDatabase* database) {
  DCHECK(database);
  return database->GetOrCreateBucket(storage_key, bucket_name);
}

QuotaErrorOr<BucketInfo> CreateBucketOnDBThread(
    const StorageKey& storage_key,
    const std::string& bucket_name,
    blink::mojom::StorageType storage_type,
    QuotaDatabase* database) {
  DCHECK(database);
  return database->CreateBucketForTesting(storage_key, bucket_name,  // IN-TEST
                                          storage_type);
}

QuotaErrorOr<BucketInfo> GetBucketOnDBThread(const StorageKey& storage_key,
                                             const std::string& bucket_name,
                                             blink::mojom::StorageType type,
                                             QuotaDatabase* database) {
  DCHECK(database);
  return database->GetBucket(storage_key, bucket_name, type);
}

QuotaErrorOr<std::set<StorageKey>> GetStorageKeysForTypeOnDBThread(
    StorageType type,
    QuotaDatabase* database) {
  DCHECK(database);
  return database->GetStorageKeysForType(type);
}

QuotaErrorOr<std::set<BucketLocator>> GetBucketsForTypeOnDBThread(
    StorageType type,
    QuotaDatabase* database) {
  DCHECK(database);
  return database->GetBucketsForType(type);
}

QuotaErrorOr<std::set<BucketLocator>> GetBucketsForHostOnDBThread(
    const std::string& host,
    StorageType type,
    QuotaDatabase* database) {
  DCHECK(database);
  return database->GetBucketsForHost(host, type);
}

QuotaErrorOr<std::set<BucketLocator>> GetBucketsForStorageKeyOnDBThread(
    const StorageKey& storage_key,
    StorageType type,
    QuotaDatabase* database) {
  DCHECK(database);
  return database->GetBucketsForStorageKey(storage_key, type);
}

QuotaErrorOr<std::set<BucketLocator>> GetModifiedBetweenOnDBThread(
    StorageType type,
    base::Time begin,
    base::Time end,
    QuotaDatabase* database) {
  DCHECK(database);
  return database->GetBucketsModifiedBetween(type, begin, end);
}

bool GetPersistentHostQuotaOnDBThread(const std::string& host,
                                      int64_t* quota,
                                      QuotaDatabase* database) {
  DCHECK(database);
  database->GetHostQuota(host, StorageType::kPersistent, quota);
  return true;
}

bool SetPersistentHostQuotaOnDBThread(const std::string& host,
                                      int64_t* new_quota,
                                      QuotaDatabase* database) {
  DCHECK(database);
  if (database->SetHostQuota(host, StorageType::kPersistent, *new_quota))
    return true;
  *new_quota = 0;
  return false;
}

QuotaErrorOr<BucketLocator> GetLRUBucketOnDBThread(
    StorageType type,
    const std::set<BucketId>& bucket_exceptions,
    SpecialStoragePolicy* policy,
    QuotaDatabase* database) {
  DCHECK(database);
  return database->GetLRUBucket(type, bucket_exceptions, policy);
}

bool DeleteStorageKeyInfoOnDBThread(const StorageKey& storage_key,
                                    StorageType type,
                                    QuotaDatabase* database) {
  DCHECK(database);
  return database->DeleteStorageKeyInfo(storage_key, type);
}

bool DeleteBucketInfoOnDBThread(BucketId bucket_id,
                                bool is_eviction,
                                QuotaDatabase* database) {
  DCHECK(database);

  if (is_eviction) {
    base::Time now = base::Time::Now();
    QuotaDatabase::BucketTableEntry entry;
    database->GetBucketInfo(bucket_id, &entry);
    base::UmaHistogramCounts1M(
        QuotaManagerImpl::kEvictedBucketAccessedCountHistogram,
        entry.use_count);
    base::UmaHistogramCounts1000(
        QuotaManagerImpl::kEvictedBucketDaysSinceAccessHistogram,
        (now - entry.last_accessed).InDays());
  }

  return database->DeleteBucketInfo(bucket_id);
}

bool BootstrapDatabaseOnDBThread(std::set<StorageKey> storage_keys,
                                 QuotaDatabase* database) {
  DCHECK(database);
  if (database->IsBootstrappedForEviction())
    return true;

  // Register existing storage keys with 0 last time access.
  if (database->RegisterInitialStorageKeyInfo(storage_keys,
                                              StorageType::kTemporary)) {
    database->SetBootstrappedForEviction(true);
    return true;
  }
  return false;
}

bool UpdateAccessTimeOnDBThread(const StorageKey& storage_key,
                                StorageType type,
                                base::Time accessed_time,
                                QuotaDatabase* database) {
  DCHECK(database);
  QuotaError result =
      database->SetStorageKeyLastAccessTime(storage_key, type, accessed_time);
  return result != QuotaError::kDatabaseError;
}

bool UpdateBucketAccessTimeOnDBThread(BucketId bucket_id,
                                      base::Time accessed_time,
                                      QuotaDatabase* database) {
  DCHECK(database);
  QuotaError result =
      database->SetBucketLastAccessTime(bucket_id, accessed_time);
  return result != QuotaError::kDatabaseError;
}

bool UpdateModifiedTimeOnDBThread(const StorageKey& storage_key,
                                  StorageType type,
                                  base::Time modified_time,
                                  QuotaDatabase* database) {
  DCHECK(database);
  QuotaError result =
      database->SetStorageKeyLastModifiedTime(storage_key, type, modified_time);
  return result != QuotaError::kDatabaseError;
}

bool UpdateBucketModifiedTimeOnDBThread(BucketId bucket_id,
                                        base::Time modified_time,
                                        QuotaDatabase* database) {
  DCHECK(database);
  QuotaError result =
      database->SetBucketLastModifiedTime(bucket_id, modified_time);
  return result != QuotaError::kDatabaseError;
}

void DidGetUsageAndQuotaStripBreakdown(
    QuotaManagerImpl::UsageAndQuotaCallback callback,
    blink::mojom::QuotaStatusCode status,
    int64_t usage,
    int64_t quota,
    blink::mojom::UsageBreakdownPtr usage_breakdown) {
  std::move(callback).Run(status, usage, quota);
}

void DidGetUsageAndQuotaStripOverride(
    QuotaManagerImpl::UsageAndQuotaWithBreakdownCallback callback,
    blink::mojom::QuotaStatusCode status,
    int64_t usage,
    int64_t quota,
    bool is_override_enabled,
    blink::mojom::UsageBreakdownPtr usage_breakdown) {
  std::move(callback).Run(status, usage, quota, std::move(usage_breakdown));
}

}  // namespace

constexpr int64_t QuotaManagerImpl::kGBytes;
constexpr int64_t QuotaManagerImpl::kNoLimit;
constexpr int64_t QuotaManagerImpl::kPerHostPersistentQuotaLimit;
constexpr int QuotaManagerImpl::kEvictionIntervalInMilliSeconds;
constexpr int QuotaManagerImpl::kThresholdOfErrorsToBeDenylisted;
constexpr int QuotaManagerImpl::kThresholdRandomizationPercent;
constexpr char QuotaManagerImpl::kDatabaseName[];
constexpr char QuotaManagerImpl::kEvictedBucketAccessedCountHistogram[];
constexpr char QuotaManagerImpl::kEvictedBucketDaysSinceAccessHistogram[];

QuotaManagerImpl::QuotaOverride::QuotaOverride() = default;
QuotaManagerImpl::QuotaOverride::~QuotaOverride() = default;

class QuotaManagerImpl::UsageAndQuotaInfoGatherer : public QuotaTask {
 public:
  UsageAndQuotaInfoGatherer(QuotaManagerImpl* manager,
                            const StorageKey& storage_key,
                            StorageType type,
                            bool is_unlimited,
                            bool is_session_only,
                            bool is_incognito,
                            absl::optional<int64_t> quota_override_size,
                            UsageAndQuotaForDevtoolsCallback callback)
      : QuotaTask(manager),
        storage_key_(storage_key),
        callback_(std::move(callback)),
        type_(type),
        is_unlimited_(is_unlimited),
        is_session_only_(is_session_only),
        is_incognito_(is_incognito),
        is_override_enabled_(quota_override_size.has_value()),
        quota_override_size_(quota_override_size) {}

 protected:
  void Run() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    // Start the async process of gathering the info we need.
    // Gather 4 pieces of info before computing an answer:
    // settings, device_storage_capacity, host_usage, and host_quota.
    base::RepeatingClosure barrier = base::BarrierClosure(
        4, base::BindOnce(&UsageAndQuotaInfoGatherer::OnBarrierComplete,
                          weak_factory_.GetWeakPtr()));

    const std::string& host = storage_key_.origin().host();

    manager()->GetQuotaSettings(
        base::BindOnce(&UsageAndQuotaInfoGatherer::OnGotSettings,
                       weak_factory_.GetWeakPtr(), barrier));
    manager()->GetStorageCapacity(
        base::BindOnce(&UsageAndQuotaInfoGatherer::OnGotCapacity,
                       weak_factory_.GetWeakPtr(), barrier));
    manager()->GetHostUsageWithBreakdown(
        host, type_,
        base::BindOnce(&UsageAndQuotaInfoGatherer::OnGotHostUsage,
                       weak_factory_.GetWeakPtr(), barrier));

    // Determine host_quota differently depending on type.
    if (is_unlimited_) {
      SetDesiredHostQuota(barrier, blink::mojom::QuotaStatusCode::kOk,
                          kNoLimit);
    } else if (type_ == StorageType::kSyncable) {
      SetDesiredHostQuota(barrier, blink::mojom::QuotaStatusCode::kOk,
                          kSyncableStorageDefaultHostQuota);
    } else if (type_ == StorageType::kPersistent) {
      manager()->GetPersistentHostQuota(
          host, base::BindOnce(&UsageAndQuotaInfoGatherer::SetDesiredHostQuota,
                               weak_factory_.GetWeakPtr(), barrier));
    } else {
      DCHECK_EQ(StorageType::kTemporary, type_);
      // For temporary storage,  OnGotSettings will set the host quota.
    }
  }

  void Aborted() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    weak_factory_.InvalidateWeakPtrs();
    std::move(callback_).Run(blink::mojom::QuotaStatusCode::kErrorAbort,
                             /*usage=*/0,
                             /*quota=*/0,
                             /*is_override_enabled=*/false,
                             /*usage_breakdown=*/nullptr);
    DeleteSoon();
  }

  void Completed() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    weak_factory_.InvalidateWeakPtrs();

    int64_t host_quota = quota_override_size_.has_value()
                             ? quota_override_size_.value()
                             : desired_host_quota_;
    int64_t temp_pool_free_space =
        std::max(static_cast<int64_t>(0),
                 available_space_ - settings_.must_remain_available);

    // Constrain the desired |host_quota| to something that fits.
    if (host_quota > temp_pool_free_space) {
      if (is_unlimited_) {
        host_quota = available_space_ + host_usage_;
      }
    }

    std::move(callback_).Run(blink::mojom::QuotaStatusCode::kOk, host_usage_,
                             host_quota, is_override_enabled_,
                             std::move(host_usage_breakdown_));
    if (type_ == StorageType::kTemporary && !is_incognito_ &&
        !is_unlimited_) {
      UMA_HISTOGRAM_MBYTES("Quota.QuotaForOrigin", host_quota);
      UMA_HISTOGRAM_MBYTES("Quota.UsageByOrigin", host_usage_);
      if (host_quota > 0) {
        UMA_HISTOGRAM_PERCENTAGE("Quota.PercentUsedByOrigin",
            std::min(100, static_cast<int>((host_usage_ * 100) / host_quota)));
      }
    }
    DeleteSoon();
  }

 private:
  QuotaManagerImpl* manager() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return static_cast<QuotaManagerImpl*>(observer());
  }

  void OnGotSettings(base::RepeatingClosure barrier_closure,
                     const QuotaSettings& settings) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    settings_ = settings;
    barrier_closure.Run();
    if (type_ == StorageType::kTemporary && !is_unlimited_) {
      int64_t host_quota = is_session_only_
                               ? settings.session_only_per_host_quota
                               : settings.per_host_quota;
      SetDesiredHostQuota(barrier_closure, blink::mojom::QuotaStatusCode::kOk,
                          host_quota);
    }
  }

  void OnGotCapacity(base::OnceClosure barrier_closure,
                     int64_t total_space,
                     int64_t available_space) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    total_space_ = total_space;
    available_space_ = available_space;
    std::move(barrier_closure).Run();
  }

  void OnGotHostUsage(base::OnceClosure barrier_closure,
                      int64_t usage,
                      blink::mojom::UsageBreakdownPtr usage_breakdown) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    host_usage_ = usage;
    host_usage_breakdown_ = std::move(usage_breakdown);
    std::move(barrier_closure).Run();
  }

  void SetDesiredHostQuota(base::OnceClosure barrier_closure,
                           blink::mojom::QuotaStatusCode status,
                           int64_t quota) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    desired_host_quota_ = quota;
    std::move(barrier_closure).Run();
  }

  void OnBarrierComplete() { CallCompleted(); }

  const StorageKey storage_key_;
  QuotaManagerImpl::UsageAndQuotaForDevtoolsCallback callback_;
  const StorageType type_;
  const bool is_unlimited_;
  const bool is_session_only_;
  const bool is_incognito_;
  int64_t available_space_ = 0;
  int64_t total_space_ = 0;
  int64_t desired_host_quota_ = 0;
  int64_t host_usage_ = 0;
  const bool is_override_enabled_;
  absl::optional<int64_t> quota_override_size_;
  blink::mojom::UsageBreakdownPtr host_usage_breakdown_;
  QuotaSettings settings_;
  SEQUENCE_CHECKER(sequence_checker_);

  // Weak pointers are used to support cancelling work.
  base::WeakPtrFactory<UsageAndQuotaInfoGatherer> weak_factory_{this};
};

class QuotaManagerImpl::EvictionRoundInfoHelper {
 public:
  // `callback` is called when the helper successfully retrieves quota settings
  // and capacity data. `completion_closure` is called after data has been
  // retrieved and `callback` has been run and to clean up itself and delete
  // this EvictionRoundInfoHelper instance.
  EvictionRoundInfoHelper(QuotaManagerImpl* manager,
                          EvictionRoundInfoCallback callback,
                          base::OnceClosure completion_closure)
      : manager_(manager),
        callback_(std::move(callback)),
        completion_closure_(std::move(completion_closure)) {
    DCHECK(manager_);
    DCHECK(callback_);
    DCHECK(completion_closure_);
  }

  void Run() {
#if DCHECK_IS_ON()
    DCHECK(!run_called_) << __func__ << " already called";
    run_called_ = true;
#endif  // DCHECK_IS_ON()

    // Gather 2 pieces of info before deciding if we need to get GlobalUsage:
    // settings and device_storage_capacity.
    base::RepeatingClosure barrier = base::BarrierClosure(
        2, base::BindOnce(&EvictionRoundInfoHelper::OnBarrierComplete,
                          weak_factory_.GetWeakPtr()));

    manager_->GetQuotaSettings(
        base::BindOnce(&EvictionRoundInfoHelper::OnGotSettings,
                       weak_factory_.GetWeakPtr(), barrier));
    manager_->GetStorageCapacity(
        base::BindOnce(&EvictionRoundInfoHelper::OnGotCapacity,
                       weak_factory_.GetWeakPtr(), barrier));
  }

 private:
  void Completed() {
#if DCHECK_IS_ON()
    DCHECK(!completed_called_) << __func__ << " already called";
    completed_called_ = true;
#endif  // DCHECK_IS_ON()
    std::move(callback_).Run(blink::mojom::QuotaStatusCode::kOk, settings_,
                             available_space_, total_space_, global_usage_,
                             global_usage_is_complete_);
    // May delete `this`.
    std::move(completion_closure_).Run();
  }

  void OnGotSettings(base::OnceClosure barrier_closure,
                     const QuotaSettings& settings) {
    settings_ = settings;
    std::move(barrier_closure).Run();
  }

  void OnGotCapacity(base::OnceClosure barrier_closure,
                     int64_t total_space,
                     int64_t available_space) {
    total_space_ = total_space;
    available_space_ = available_space;
    std::move(barrier_closure).Run();
  }

  void OnBarrierComplete() {
    // Avoid computing the full current_usage when there's no pressure.
    int64_t consumed_space = total_space_ - available_space_;
    if (consumed_space < settings_.pool_size &&
        available_space_ > settings_.should_remain_available) {
      DCHECK(!global_usage_is_complete_);
      global_usage_ =
          manager_->GetUsageTracker(StorageType::kTemporary)->GetCachedUsage();
      // `this` may be deleted during this Complete() call.
      Completed();
      return;
    }
    manager_->GetGlobalUsage(
        StorageType::kTemporary,
        base::BindOnce(&EvictionRoundInfoHelper::OnGotGlobalUsage,
                       weak_factory_.GetWeakPtr()));
  }

  void OnGotGlobalUsage(int64_t usage, int64_t unlimited_usage) {
    global_usage_ = std::max(int64_t{0}, usage - unlimited_usage);
    global_usage_is_complete_ = true;
    // `this` may be deleted during this Complete() call.
    Completed();
  }

  QuotaManagerImpl* const manager_;
  EvictionRoundInfoCallback callback_;
  base::OnceClosure completion_closure_;
  QuotaSettings settings_;
  int64_t available_space_ = 0;
  int64_t total_space_ = 0;
  int64_t global_usage_ = 0;
  bool global_usage_is_complete_ = false;

#if DCHECK_IS_ON()
  bool run_called_ = false;
  bool completed_called_ = false;
#endif  // DCHECK_IS_ON()

  base::WeakPtrFactory<EvictionRoundInfoHelper> weak_factory_{this};
};

class QuotaManagerImpl::GetUsageInfoTask : public QuotaTask {
 public:
  GetUsageInfoTask(QuotaManagerImpl* manager, GetUsageInfoCallback callback)
      : QuotaTask(manager), callback_(std::move(callback)) {}

 protected:
  void Run() override {
    remaining_trackers_ = 3;
    // This will populate cached hosts and usage info.
    manager()
        ->GetUsageTracker(StorageType::kTemporary)
        ->GetGlobalUsage(base::BindOnce(&GetUsageInfoTask::DidGetGlobalUsage,
                                        weak_factory_.GetWeakPtr(),
                                        StorageType::kTemporary));
    manager()
        ->GetUsageTracker(StorageType::kPersistent)
        ->GetGlobalUsage(base::BindOnce(&GetUsageInfoTask::DidGetGlobalUsage,
                                        weak_factory_.GetWeakPtr(),
                                        StorageType::kPersistent));
    manager()
        ->GetUsageTracker(StorageType::kSyncable)
        ->GetGlobalUsage(base::BindOnce(&GetUsageInfoTask::DidGetGlobalUsage,
                                        weak_factory_.GetWeakPtr(),
                                        StorageType::kSyncable));
  }

  void Completed() override {
    std::move(callback_).Run(std::move(entries_));
    DeleteSoon();
  }

 private:
  void AddEntries(StorageType type, UsageTracker* tracker) {
    std::map<std::string, int64_t> host_usage = tracker->GetCachedHostsUsage();
    for (const auto& host_usage_pair : host_usage) {
      entries_.emplace_back(host_usage_pair.first, type,
                            host_usage_pair.second);
    }
    if (--remaining_trackers_ == 0)
      CallCompleted();
  }

  void DidGetGlobalUsage(StorageType type, int64_t, int64_t) {
    DCHECK(manager()->GetUsageTracker(type));
    AddEntries(type, manager()->GetUsageTracker(type));
  }

  QuotaManagerImpl* manager() const {
    return static_cast<QuotaManagerImpl*>(observer());
  }

  GetUsageInfoCallback callback_;
  UsageInfoEntries entries_;
  int remaining_trackers_;
  base::WeakPtrFactory<GetUsageInfoTask> weak_factory_{this};
};

// Calls each QuotaClient for `quota_client_types` to delete `storage_key` data.
// If `storage_key` is delete for all registered client types, and there are no
// deletion errors, StorageKeyDataDeleter will call to delete `storage_key` from
// QuotaDatabase. It will return QuotaStatusCode::kOk to the `callback` on
// success and will finish by making a call to delete itself.
class QuotaManagerImpl::StorageKeyDataDeleter : public QuotaTask {
 public:
  StorageKeyDataDeleter(QuotaManagerImpl* manager,
                        const StorageKey& storage_key,
                        StorageType type,
                        QuotaClientTypes quota_client_types,
                        StatusCallback callback)
      : QuotaTask(manager),
        storage_key_(storage_key),
        type_(type),
        quota_client_types_(std::move(quota_client_types)),
        callback_(std::move(callback)) {}

 protected:
  void Run() override {
    DCHECK(manager()->client_types_.contains(type_));
    remaining_clients_ = manager()->client_types_[type_].size();

    for (const auto& client_and_type : manager()->client_types_[type_]) {
      mojom::QuotaClient* client = client_and_type.first;
      QuotaClientType client_type = client_and_type.second;
      if (quota_client_types_.contains(client_type)) {
        static int tracing_id = 0;
        TRACE_EVENT_NESTABLE_ASYNC_BEGIN2(
            "browsing_data", "QuotaManagerImpl::StorageKeyDataDeleter",
            ++tracing_id, "client_type", client_type, "storage_key",
            storage_key_.Serialize());
        client->DeleteStorageKeyData(
            storage_key_, type_,
            base::BindOnce(&StorageKeyDataDeleter::DidDeleteStorageKeyData,
                           weak_factory_.GetWeakPtr(), tracing_id));
      } else {
        ++skipped_clients_;
        --remaining_clients_;
      }
    }

    if (remaining_clients_ == 0)
      CallCompleted();
  }

  void Completed() override {
    if (error_count_ == 0) {
      // Only remove the entire storage key if we didn't skip any client types.
      if (skipped_clients_ == 0)
        manager()->DeleteStorageKeyFromDatabase(storage_key_, type_);
      std::move(callback_).Run(blink::mojom::QuotaStatusCode::kOk);
    } else {
      std::move(callback_).Run(
          blink::mojom::QuotaStatusCode::kErrorInvalidModification);
    }
    DeleteSoon();
  }

  void Aborted() override {
    std::move(callback_).Run(blink::mojom::QuotaStatusCode::kErrorAbort);
    DeleteSoon();
  }

 private:
  void DidDeleteStorageKeyData(int tracing_id,
                               blink::mojom::QuotaStatusCode status) {
    DCHECK_GT(remaining_clients_, 0U);
    TRACE_EVENT_NESTABLE_ASYNC_END0(
        "browsing_data", "QuotaManagerImpl::StorageKeyDataDeleter", tracing_id);

    if (status != blink::mojom::QuotaStatusCode::kOk)
      ++error_count_;

    if (--remaining_clients_ == 0)
      CallCompleted();
  }

  QuotaManagerImpl* manager() const {
    return static_cast<QuotaManagerImpl*>(observer());
  }

  const StorageKey storage_key_;
  const StorageType type_;
  const QuotaClientTypes quota_client_types_;
  int error_count_ = 0;
  size_t remaining_clients_ = 0;
  int skipped_clients_ = 0;
  StatusCallback callback_;

  base::WeakPtrFactory<StorageKeyDataDeleter> weak_factory_{this};
};

// Calls all QuotaClients for the bucket StorageType to delete bucket data.
// Keeps track of operations that have failed, and will only complete the
// task if all operations have succeeded. This is currently only for the default
// bucket. If a non-default bucket is to be evicted, it will immediately
// complete the task since non-default bucket usage is not being tracked by
// QuotaClients yet.
// TODO(crbug.com/1199417): Update to call QuotaClients to clear data for
// non-default buckets when QuotaClient is migrated to operate on buckets.
class QuotaManagerImpl::BucketDataDeleter : public QuotaTask {
 public:
  BucketDataDeleter(QuotaManagerImpl* manager,
                    const BucketLocator& bucket,
                    QuotaClientTypes quota_client_types,
                    bool is_eviction,
                    StatusCallback callback)
      : QuotaTask(manager),
        bucket_(bucket),
        quota_client_types_(std::move(quota_client_types)),
        is_eviction_(is_eviction),
        callback_(std::move(callback)) {}

 protected:
  void Run() override {
    DCHECK(manager()->client_types_.contains(bucket_.type));

    // If bucket is not the default bucket, skip calls to the QuotaClient since
    // they are not being tracked yet.
    // TODO(crbug.com/1199417): Update to call for all buckets once QuotaClient
    // is migrated to operate on buckets.
    if (!bucket_.is_default) {
      CallCompleted();
      return;
    }

    remaining_clients_ = manager()->client_types_[bucket_.type].size();

    for (const auto& client_and_type : manager()->client_types_[bucket_.type]) {
      mojom::QuotaClient* client = client_and_type.first;
      QuotaClientType client_type = client_and_type.second;
      if (quota_client_types_.contains(client_type)) {
        static int tracing_id = 0;
        std::ostringstream bucket_params;
        bucket_params << "storage_key: " << bucket_.storage_key.Serialize()
                      << ", is_default: " << bucket_.is_default
                      << ", id: " << bucket_.id;
        TRACE_EVENT_NESTABLE_ASYNC_BEGIN2(
            "browsing_data", "QuotaManagerImpl::BucketDataDeleter",
            ++tracing_id, "client_type", client_type, "bucket",
            bucket_params.str());
        client->DeleteStorageKeyData(
            bucket_.storage_key, bucket_.type,
            base::BindOnce(&BucketDataDeleter::DidDeleteBucketData,
                           weak_factory_.GetWeakPtr(), tracing_id));
      } else {
        ++skipped_clients_;
        --remaining_clients_;
      }
    }

    if (remaining_clients_ == 0)
      CallCompleted();
  }

  void Completed() override {
    if (error_count_ == 0) {
      // Only remove the bucket if we didn't skip any client types.
      if (skipped_clients_ == 0)
        manager()->DeleteBucketFromDatabase(bucket_.id, is_eviction_);
      std::move(callback_).Run(blink::mojom::QuotaStatusCode::kOk);
    } else {
      std::move(callback_).Run(
          blink::mojom::QuotaStatusCode::kErrorInvalidModification);
    }
    DeleteSoon();
  }

  void Aborted() override {
    std::move(callback_).Run(blink::mojom::QuotaStatusCode::kErrorAbort);
    DeleteSoon();
  }

 private:
  void DidDeleteBucketData(int tracing_id,
                           blink::mojom::QuotaStatusCode status) {
    DCHECK_GT(remaining_clients_, 0U);
    TRACE_EVENT_NESTABLE_ASYNC_END0(
        "browsing_data", "QuotaManagerImpl::BucketDataDeleter", tracing_id);

    if (status != blink::mojom::QuotaStatusCode::kOk)
      ++error_count_;

    if (--remaining_clients_ == 0)
      CallCompleted();
  }

  QuotaManagerImpl* manager() const {
    return static_cast<QuotaManagerImpl*>(observer());
  }

  const BucketLocator bucket_;
  const QuotaClientTypes quota_client_types_;
  int error_count_ = 0;
  size_t remaining_clients_ = 0;
  int skipped_clients_ = 0;
  const bool is_eviction_;
  StatusCallback callback_;

  base::WeakPtrFactory<BucketDataDeleter> weak_factory_{this};
};

class QuotaManagerImpl::HostDataDeleter : public QuotaTask {
 public:
  HostDataDeleter(QuotaManagerImpl* manager,
                  const std::string& host,
                  StorageType type,
                  QuotaClientTypes quota_client_types,
                  StatusCallback callback)
      : QuotaTask(manager),
        host_(host),
        type_(type),
        quota_client_types_(std::move(quota_client_types)),
        error_count_(0),
        remaining_clients_(0),
        remaining_deleters_(0),
        callback_(std::move(callback)) {}

 protected:
  void Run() override {
    DCHECK(manager()->client_types_.contains(type_));
    remaining_clients_ = manager()->client_types_[type_].size();

    for (const auto& client_and_type : manager()->client_types_[type_]) {
      client_and_type.first->GetStorageKeysForHost(
          type_, host_,
          base::BindOnce(&HostDataDeleter::DidGetStorageKeysForHost,
                         weak_factory_.GetWeakPtr()));
    }
  }

  void Completed() override {
    if (error_count_ == 0) {
      std::move(callback_).Run(blink::mojom::QuotaStatusCode::kOk);
    } else {
      std::move(callback_).Run(
          blink::mojom::QuotaStatusCode::kErrorInvalidModification);
    }
    DeleteSoon();
  }

  void Aborted() override {
    std::move(callback_).Run(blink::mojom::QuotaStatusCode::kErrorAbort);
    DeleteSoon();
  }

 private:
  void DidGetStorageKeysForHost(const std::vector<StorageKey>& storage_keys) {
    DCHECK_GT(remaining_clients_, 0U);

    storage_keys_.insert(storage_keys.begin(), storage_keys.end());

    if (--remaining_clients_ == 0) {
      if (!storage_keys_.empty())
        ScheduleStorageKeysDeletion();
      else
        CallCompleted();
    }
  }

  void ScheduleStorageKeysDeletion() {
    remaining_deleters_ = storage_keys_.size();
    for (const auto& storage_key : storage_keys_) {
      StorageKeyDataDeleter* deleter = new StorageKeyDataDeleter(
          manager(), storage_key, type_, std::move(quota_client_types_),
          base::BindOnce(&HostDataDeleter::DidDeleteStorageKeyData,
                         weak_factory_.GetWeakPtr()));
      deleter->Start();
    }
  }

  void DidDeleteStorageKeyData(blink::mojom::QuotaStatusCode status) {
    DCHECK_GT(remaining_deleters_, 0U);

    if (status != blink::mojom::QuotaStatusCode::kOk)
      ++error_count_;

    if (--remaining_deleters_ == 0)
      CallCompleted();
  }

  QuotaManagerImpl* manager() const {
    return static_cast<QuotaManagerImpl*>(observer());
  }

  const std::string host_;
  const StorageType type_;
  const QuotaClientTypes quota_client_types_;
  std::set<StorageKey> storage_keys_;
  int error_count_;
  size_t remaining_clients_;
  size_t remaining_deleters_;
  StatusCallback callback_;

  base::WeakPtrFactory<HostDataDeleter> weak_factory_{this};
};

class QuotaManagerImpl::StorageCleanupHelper : public QuotaTask {
 public:
  StorageCleanupHelper(QuotaManagerImpl* manager,
                       StorageType type,
                       QuotaClientTypes quota_client_types,
                       base::OnceClosure callback)
      : QuotaTask(manager),
        type_(type),
        quota_client_types_(std::move(quota_client_types)),
        callback_(std::move(callback)) {
    DCHECK(manager->client_types_.contains(type_));
  }

 protected:
  void Run() override {
    DCHECK(manager()->client_types_.contains(type_));
    base::RepeatingClosure barrier = base::BarrierClosure(
        manager()->client_types_[type_].size(),
        base::BindOnce(&StorageCleanupHelper::CallCompleted,
                       weak_factory_.GetWeakPtr()));

    // This may synchronously trigger |callback_| at the end of the for loop,
    // make sure we do nothing after this block.
    for (const auto& client_and_type : manager()->client_types_[type_]) {
      mojom::QuotaClient* client = client_and_type.first;
      QuotaClientType client_type = client_and_type.second;
      if (quota_client_types_.contains(client_type)) {
        client->PerformStorageCleanup(type_, barrier);
      } else {
        barrier.Run();
      }
    }
  }

  void Aborted() override {
    weak_factory_.InvalidateWeakPtrs();
    std::move(callback_).Run();
    DeleteSoon();
  }

  void Completed() override {
    weak_factory_.InvalidateWeakPtrs();
    std::move(callback_).Run();
    DeleteSoon();
  }

 private:
  QuotaManagerImpl* manager() const {
    return static_cast<QuotaManagerImpl*>(observer());
  }

  const StorageType type_;
  const QuotaClientTypes quota_client_types_;
  base::OnceClosure callback_;
  base::WeakPtrFactory<StorageCleanupHelper> weak_factory_{this};
};

// Gather storage key info table for quota-internals page.
//
// This class is granted ownership of itself when it is passed to
// DidDumpQuotaTable() via base::Owned(). When the closure for said function
// goes out of scope, the object is deleted.
// This class is not thread-safe because there can be a race when entries_ is
// modified.
class QuotaManagerImpl::DumpQuotaTableHelper {
 public:
  bool DumpQuotaTableOnDBThread(QuotaDatabase* database) {
    DCHECK(database);
    return database->DumpQuotaTable(base::BindRepeating(
        &DumpQuotaTableHelper::AppendEntry, base::Unretained(this)));
  }

  void DidDumpQuotaTable(const base::WeakPtr<QuotaManagerImpl>& manager,
                         DumpQuotaTableCallback callback,
                         bool success) {
    if (!manager) {
      // The operation was aborted.
      std::move(callback).Run(QuotaTableEntries());
      return;
    }
    manager->DidDatabaseWork(success);
    std::move(callback).Run(entries_);
  }

 private:
  bool AppendEntry(const QuotaTableEntry& entry) {
    entries_.push_back(entry);
    return true;
  }

  QuotaTableEntries entries_;
};

// Gather storage key info table for quota-internals page.
//
// This class is granted ownership of itself when it is passed to
// DidDumpQuotaTable() via base::Owned(). When the closure for said function
// goes out of scope, the object is deleted.
// This class is not thread-safe because there can be races when entries_ is
// modified.
class QuotaManagerImpl::DumpBucketTableHelper {
 public:
  bool DumpBucketTableOnDBThread(QuotaDatabase* database) {
    DCHECK(database);
    return database->DumpBucketTable(base::BindRepeating(
        &DumpBucketTableHelper::AppendEntry, base::Unretained(this)));
  }

  void DidDumpBucketTable(const base::WeakPtr<QuotaManagerImpl>& manager,
                          DumpBucketTableCallback callback,
                          bool success) {
    if (!manager) {
      // The operation was aborted.
      std::move(callback).Run(BucketTableEntries());
      return;
    }
    manager->DidDatabaseWork(success);
    std::move(callback).Run(entries_);
  }

 private:
  bool AppendEntry(const BucketTableEntry& entry) {
    entries_.push_back(entry);
    return true;
  }

  BucketTableEntries entries_;
};

// QuotaManagerImpl -----------------------------------------------------------

QuotaManagerImpl::QuotaManagerImpl(
    bool is_incognito,
    const base::FilePath& profile_path,
    scoped_refptr<base::SingleThreadTaskRunner> io_thread,
    base::RepeatingClosure quota_change_callback,
    scoped_refptr<SpecialStoragePolicy> special_storage_policy,
    const GetQuotaSettingsFunc& get_settings_function)
    : RefCountedDeleteOnSequence<QuotaManagerImpl>(io_thread),
      is_incognito_(is_incognito),
      profile_path_(profile_path),
      proxy_(base::MakeRefCounted<QuotaManagerProxy>(this, io_thread)),
      io_thread_(std::move(io_thread)),
      db_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN})),
      get_settings_function_(get_settings_function),
      quota_change_callback_(std::move(quota_change_callback)),
      special_storage_policy_(std::move(special_storage_policy)),
      get_volume_info_fn_(&QuotaManagerImpl::GetVolumeInfo) {
  DCHECK_EQ(settings_.refresh_interval, base::TimeDelta::Max());
  if (!get_settings_function.is_null()) {
    // Reset the interval to ensure we use the get_settings_function
    // the first times settings_ is needed.
    settings_.refresh_interval = base::TimeDelta();
    get_settings_task_runner_ = base::ThreadTaskRunnerHandle::Get();
  }
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

void QuotaManagerImpl::SetQuotaSettings(const QuotaSettings& settings) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  settings_ = settings;
  settings_timestamp_ = base::TimeTicks::Now();
}

void QuotaManagerImpl::GetOrCreateBucket(
    const StorageKey& storage_key,
    const std::string& bucket_name,
    base::OnceCallback<void(QuotaErrorOr<BucketInfo>)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EnsureDatabaseOpened();

  PostTaskAndReplyWithResultForDBThread(
      base::BindOnce(&GetOrCreateBucketOnDBThread, storage_key, bucket_name),
      base::BindOnce(&QuotaManagerImpl::DidGetBucket,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void QuotaManagerImpl::CreateBucketForTesting(
    const StorageKey& storage_key,
    const std::string& bucket_name,
    blink::mojom::StorageType storage_type,
    base::OnceCallback<void(QuotaErrorOr<BucketInfo>)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EnsureDatabaseOpened();

  PostTaskAndReplyWithResultForDBThread(
      base::BindOnce(&CreateBucketOnDBThread, storage_key, bucket_name,
                     storage_type),
      base::BindOnce(&QuotaManagerImpl::DidGetBucket,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void QuotaManagerImpl::GetBucket(
    const StorageKey& storage_key,
    const std::string& bucket_name,
    blink::mojom::StorageType type,
    base::OnceCallback<void(QuotaErrorOr<BucketInfo>)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EnsureDatabaseOpened();

  PostTaskAndReplyWithResultForDBThread(
      base::BindOnce(&GetBucketOnDBThread, storage_key, bucket_name, type),
      base::BindOnce(&QuotaManagerImpl::DidGetBucket,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void QuotaManagerImpl::GetStorageKeysForType(blink::mojom::StorageType type,
                                             GetStorageKeysCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EnsureDatabaseOpened();

  PostTaskAndReplyWithResultForDBThread(
      base::BindOnce(&GetStorageKeysForTypeOnDBThread, type),
      base::BindOnce(&QuotaManagerImpl::DidGetStorageKeys,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void QuotaManagerImpl::GetBucketsForType(
    blink::mojom::StorageType type,
    base::OnceCallback<void(QuotaErrorOr<std::set<BucketLocator>>)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EnsureDatabaseOpened();

  PostTaskAndReplyWithResultForDBThread(
      base::BindOnce(&GetBucketsForTypeOnDBThread, type),
      base::BindOnce(&QuotaManagerImpl::DidGetBuckets,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void QuotaManagerImpl::GetBucketsForHost(
    const std::string& host,
    blink::mojom::StorageType type,
    base::OnceCallback<void(QuotaErrorOr<std::set<BucketLocator>>)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EnsureDatabaseOpened();

  PostTaskAndReplyWithResultForDBThread(
      base::BindOnce(&GetBucketsForHostOnDBThread, host, type),
      base::BindOnce(&QuotaManagerImpl::DidGetBuckets,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void QuotaManagerImpl::GetBucketsForStorageKey(
    const StorageKey& storage_key,
    blink::mojom::StorageType type,
    base::OnceCallback<void(QuotaErrorOr<std::set<BucketLocator>>)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EnsureDatabaseOpened();

  PostTaskAndReplyWithResultForDBThread(
      base::BindOnce(&GetBucketsForStorageKeyOnDBThread, storage_key, type),
      base::BindOnce(&QuotaManagerImpl::DidGetBuckets,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void QuotaManagerImpl::GetUsageInfo(GetUsageInfoCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EnsureDatabaseOpened();
  GetUsageInfoTask* get_usage_info =
      new GetUsageInfoTask(this, std::move(callback));
  get_usage_info->Start();
}

void QuotaManagerImpl::GetUsageAndQuotaForWebApps(
    const StorageKey& storage_key,
    StorageType type,
    UsageAndQuotaCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  GetUsageAndQuotaWithBreakdown(
      storage_key, type,
      base::BindOnce(&DidGetUsageAndQuotaStripBreakdown, std::move(callback)));
}

void QuotaManagerImpl::GetUsageAndQuotaWithBreakdown(
    const StorageKey& storage_key,
    StorageType type,
    UsageAndQuotaWithBreakdownCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  GetUsageAndQuotaForDevtools(
      storage_key, type,
      base::BindOnce(&DidGetUsageAndQuotaStripOverride, std::move(callback)));
}

void QuotaManagerImpl::GetUsageAndQuotaForDevtools(
    const StorageKey& storage_key,
    StorageType type,
    UsageAndQuotaForDevtoolsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!IsSupportedType(type) ||
      (is_incognito_ && !IsSupportedIncognitoType(type))) {
    std::move(callback).Run(blink::mojom::QuotaStatusCode::kErrorNotSupported,
                            /*usage=*/0,
                            /*quota=*/0,
                            /*is_override_enabled=*/false,
                            /*usage_breakdown=*/nullptr);
    return;
  }
  EnsureDatabaseOpened();

  bool is_session_only = type == StorageType::kTemporary &&
                         special_storage_policy_ &&
                         special_storage_policy_->IsStorageSessionOnly(
                             storage_key.origin().GetURL());

  absl::optional<int64_t> quota_override =
      GetQuotaOverrideForStorageKey(storage_key);

  UsageAndQuotaInfoGatherer* helper = new UsageAndQuotaInfoGatherer(
      this, storage_key, type, IsStorageUnlimited(storage_key, type),
      is_session_only, is_incognito_, quota_override, std::move(callback));
  helper->Start();
}

void QuotaManagerImpl::GetUsageAndQuota(const StorageKey& storage_key,
                                        StorageType type,
                                        UsageAndQuotaCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (IsStorageUnlimited(storage_key, type)) {
    // TODO(michaeln): This seems like a non-obvious odd behavior, probably for
    // apps/extensions, but it would be good to eliminate this special case.
    std::move(callback).Run(blink::mojom::QuotaStatusCode::kOk, 0, kNoLimit);
    return;
  }

  if (!IsSupportedType(type) ||
      (is_incognito_ && !IsSupportedIncognitoType(type))) {
    std::move(callback).Run(
        /*status*/ blink::mojom::QuotaStatusCode::kErrorNotSupported,
        /*usage*/ 0,
        /*quota*/ 0);
    return;
  }
  EnsureDatabaseOpened();

  bool is_session_only = type == StorageType::kTemporary &&
                         special_storage_policy_ &&
                         special_storage_policy_->IsStorageSessionOnly(
                             storage_key.origin().GetURL());

  absl::optional<int64_t> quota_override =
      GetQuotaOverrideForStorageKey(storage_key);

  UsageAndQuotaInfoGatherer* helper = new UsageAndQuotaInfoGatherer(
      this, storage_key, type, IsStorageUnlimited(storage_key, type),
      is_session_only, is_incognito_, quota_override,
      base::BindOnce(&DidGetUsageAndQuotaStripOverride,
                     base::BindOnce(&DidGetUsageAndQuotaStripBreakdown,
                                    std::move(callback))));
  helper->Start();
}

void QuotaManagerImpl::NotifyWriteFailed(const StorageKey& storage_key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto age_of_disk_stats = base::TimeTicks::Now() -
                           std::get<0>(cached_disk_stats_for_storage_pressure_);

  // Avoid polling for free disk space if disk stats have been recently
  // queried.
  if (age_of_disk_stats < kStoragePressureCheckDiskStatsInterval) {
    int64_t total_space = std::get<1>(cached_disk_stats_for_storage_pressure_);
    int64_t available_space =
        std::get<2>(cached_disk_stats_for_storage_pressure_);
    MaybeRunStoragePressureCallback(storage_key, total_space, available_space);
  }

  GetStorageCapacity(
      base::BindOnce(&QuotaManagerImpl::MaybeRunStoragePressureCallback,
                     weak_factory_.GetWeakPtr(), storage_key));
}

void QuotaManagerImpl::SetUsageCacheEnabled(QuotaClientType client_id,
                                            const StorageKey& storage_key,
                                            StorageType type,
                                            bool enabled) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EnsureDatabaseOpened();
  DCHECK(GetUsageTracker(type));
  GetUsageTracker(type)->SetUsageCacheEnabled(client_id, storage_key, enabled);
}

void QuotaManagerImpl::DeleteBucketData(const BucketLocator& bucket,
                                        QuotaClientTypes quota_client_types,
                                        StatusCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DeleteBucketDataInternal(bucket, std::move(quota_client_types), false,
                           std::move(callback));
}

void QuotaManagerImpl::FindAndDeleteBucketData(const StorageKey& storage_key,
                                               const std::string& bucket_name,
                                               StatusCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EnsureDatabaseOpened();

  PostTaskAndReplyWithResultForDBThread(
      base::BindOnce(&GetBucketOnDBThread, storage_key, bucket_name,
                     StorageType::kTemporary),
      base::BindOnce(&QuotaManagerImpl::DidGetBucketForDeletion,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void QuotaManagerImpl::PerformStorageCleanup(
    StorageType type,
    QuotaClientTypes quota_client_types,
    base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  StorageCleanupHelper* deleter = new StorageCleanupHelper(
      this, type, std::move(quota_client_types), std::move(callback));
  deleter->Start();
}

void QuotaManagerImpl::DeleteHostData(const std::string& host,
                                      StorageType type,
                                      QuotaClientTypes quota_client_types,
                                      StatusCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EnsureDatabaseOpened();

  DCHECK(client_types_.contains(type));
  if (host.empty() || client_types_[type].empty()) {
    std::move(callback).Run(blink::mojom::QuotaStatusCode::kOk);
    return;
  }

  HostDataDeleter* deleter = new HostDataDeleter(
      this, host, type, std::move(quota_client_types), std::move(callback));
  deleter->Start();
}

void QuotaManagerImpl::GetPersistentHostQuota(const std::string& host,
                                              QuotaCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EnsureDatabaseOpened();
  if (host.empty()) {
    // This could happen if we are called on file:///.
    // TODO(kinuko) We may want to respect --allow-file-access-from-files
    // command line switch.
    std::move(callback).Run(blink::mojom::QuotaStatusCode::kOk, 0);
    return;
  }

  if (!persistent_host_quota_callbacks_.Add(host, std::move(callback)))
    return;

  int64_t* quota_ptr = new int64_t(0);
  PostTaskAndReplyWithResultForDBThread(
      FROM_HERE,
      base::BindOnce(&GetPersistentHostQuotaOnDBThread, host,
                     base::Unretained(quota_ptr)),
      base::BindOnce(&QuotaManagerImpl::DidGetPersistentHostQuota,
                     weak_factory_.GetWeakPtr(), host, base::Owned(quota_ptr)));
}

void QuotaManagerImpl::SetPersistentHostQuota(const std::string& host,
                                              int64_t new_quota,
                                              QuotaCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EnsureDatabaseOpened();
  if (host.empty()) {
    // This could happen if we are called on file:///.
    std::move(callback).Run(blink::mojom::QuotaStatusCode::kErrorNotSupported,
                            0);
    return;
  }

  if (new_quota < 0) {
    std::move(callback).Run(
        blink::mojom::QuotaStatusCode::kErrorInvalidModification, -1);
    return;
  }

  // Cap the requested size at the per-host quota limit.
  new_quota = std::min(new_quota, kPerHostPersistentQuotaLimit);

  if (db_disabled_) {
    std::move(callback).Run(blink::mojom::QuotaStatusCode::kErrorInvalidAccess,
                            -1);
    return;
  }

  int64_t* new_quota_ptr = new int64_t(new_quota);
  PostTaskAndReplyWithResultForDBThread(
      FROM_HERE,
      base::BindOnce(&SetPersistentHostQuotaOnDBThread, host,
                     base::Unretained(new_quota_ptr)),
      base::BindOnce(&QuotaManagerImpl::DidSetPersistentHostQuota,
                     weak_factory_.GetWeakPtr(), host, std::move(callback),
                     base::Owned(new_quota_ptr)));
}

void QuotaManagerImpl::GetGlobalUsage(StorageType type,
                                      GlobalUsageCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EnsureDatabaseOpened();
  DCHECK(GetUsageTracker(type));
  GetUsageTracker(type)->GetGlobalUsage(std::move(callback));
}

void QuotaManagerImpl::GetHostUsageWithBreakdown(
    const std::string& host,
    StorageType type,
    UsageWithBreakdownCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EnsureDatabaseOpened();
  DCHECK(GetUsageTracker(type));
  GetUsageTracker(type)->GetHostUsageWithBreakdown(host, std::move(callback));
}

std::map<std::string, std::string> QuotaManagerImpl::GetStatistics() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::map<std::string, std::string> statistics;
  if (temporary_storage_evictor_) {
    std::map<std::string, int64_t> stats;
    temporary_storage_evictor_->GetStatistics(&stats);
    for (const auto& storage_key_usage_pair : stats) {
      statistics[storage_key_usage_pair.first] =
          base::NumberToString(storage_key_usage_pair.second);
    }
  }
  return statistics;
}

bool QuotaManagerImpl::IsStorageUnlimited(const StorageKey& storage_key,
                                          StorageType type) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // For syncable storage we should always enforce quota (since the
  // quota must be capped by the server limit).
  if (type == StorageType::kSyncable)
    return false;
  if (type == StorageType::kQuotaNotManaged)
    return true;
  return special_storage_policy_.get() &&
         special_storage_policy_->IsStorageUnlimited(
             storage_key.origin().GetURL());
}

void QuotaManagerImpl::GetBucketsModifiedBetween(StorageType type,
                                                 base::Time begin,
                                                 base::Time end,
                                                 GetBucketsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EnsureDatabaseOpened();
  PostTaskAndReplyWithResultForDBThread(
      base::BindOnce(&GetModifiedBetweenOnDBThread, type, begin, end),
      base::BindOnce(&QuotaManagerImpl::DidGetModifiedBetween,
                     weak_factory_.GetWeakPtr(), std::move(callback), type));
}

bool QuotaManagerImpl::ResetUsageTracker(StorageType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(GetUsageTracker(type));
  if (GetUsageTracker(type)->IsWorking())
    return false;

  auto usage_tracker = std::make_unique<UsageTracker>(
      client_types_[type], type, special_storage_policy_.get());
  switch (type) {
    case StorageType::kTemporary:
      temporary_usage_tracker_ = std::move(usage_tracker);
      return true;
    case StorageType::kPersistent:
      persistent_usage_tracker_ = std::move(usage_tracker);
      return true;
    case StorageType::kSyncable:
      syncable_usage_tracker_ = std::move(usage_tracker);
      return true;
    default:
      NOTREACHED();
  }
  return true;
}

QuotaManagerImpl::~QuotaManagerImpl() {
  proxy_->InvalidateQuotaManagerImpl(base::PassKey<QuotaManagerImpl>());

  if (database_)
    db_runner_->DeleteSoon(FROM_HERE, database_.release());
}

QuotaManagerImpl::EvictionContext::EvictionContext() = default;
QuotaManagerImpl::EvictionContext::~EvictionContext() = default;

void QuotaManagerImpl::EnsureDatabaseOpened() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(io_thread_->BelongsToCurrentThread());
  if (database_) {
    // Already initialized.
    return;
  }

  // Use an empty path to open an in-memory only database for incognito.
  database_ = std::make_unique<QuotaDatabase>(
      is_incognito_ ? base::FilePath()
                    : profile_path_.AppendASCII(kDatabaseName));

  temporary_usage_tracker_ = std::make_unique<UsageTracker>(
      client_types_[StorageType::kTemporary], StorageType::kTemporary,
      special_storage_policy_.get());
  persistent_usage_tracker_ = std::make_unique<UsageTracker>(
      client_types_[StorageType::kPersistent], StorageType::kPersistent,
      special_storage_policy_.get());
  syncable_usage_tracker_ = std::make_unique<UsageTracker>(
      client_types_[StorageType::kSyncable], StorageType::kSyncable,
      special_storage_policy_.get());

  if (!is_incognito_) {
    histogram_timer_.Start(FROM_HERE,
                           base::Milliseconds(kReportHistogramInterval), this,
                           &QuotaManagerImpl::ReportHistogram);
  }

  base::PostTaskAndReplyWithResult(
      db_runner_.get(), FROM_HERE,
      base::BindOnce(&QuotaDatabase::IsBootstrappedForEviction,
                     base::Unretained(database_.get())),
      base::BindOnce(&QuotaManagerImpl::DidOpenDatabase,
                     weak_factory_.GetWeakPtr()));
}

void QuotaManagerImpl::DidOpenDatabase(bool is_database_bootstrapped) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  is_database_bootstrapped_for_eviction_ = is_database_bootstrapped;
  StartEviction();
}

void QuotaManagerImpl::BootstrapDatabaseForEviction(
    GetBucketCallback did_get_bucket_callback,
    int64_t usage,
    int64_t unlimited_usage) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // The usage cache should be fully populated now so we can
  // seed the database with storage keys we know about.
  std::set<StorageKey> storage_keys =
      temporary_usage_tracker_->GetCachedStorageKeys();
  PostTaskAndReplyWithResultForDBThread(
      FROM_HERE,
      base::BindOnce(&BootstrapDatabaseOnDBThread, std::move(storage_keys)),
      base::BindOnce(&QuotaManagerImpl::DidBootstrapDatabaseForEviction,
                     weak_factory_.GetWeakPtr(),
                     std::move(did_get_bucket_callback)));
}

void QuotaManagerImpl::DidBootstrapDatabaseForEviction(
    GetBucketCallback did_get_bucket_callback,
    bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  is_database_bootstrapped_for_eviction_ = success;
  DidDatabaseWork(success);
  GetLRUBucket(StorageType::kTemporary, std::move(did_get_bucket_callback));
}

void QuotaManagerImpl::RegisterClient(
    mojo::PendingRemote<mojom::QuotaClient> client,
    QuotaClientType client_type,
    const std::vector<blink::mojom::StorageType>& storage_types) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!database_.get())
      << "All clients must be registered before the database is initialized";

  clients_for_ownership_.emplace_back(std::move(client));
  mojom::QuotaClient* client_ptr = clients_for_ownership_.back().get();

  for (blink::mojom::StorageType storage_type : storage_types)
    client_types_[storage_type].insert({client_ptr, client_type});
}

UsageTracker* QuotaManagerImpl::GetUsageTracker(StorageType type) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  switch (type) {
    case StorageType::kTemporary:
      return temporary_usage_tracker_.get();
    case StorageType::kPersistent:
      return persistent_usage_tracker_.get();
    case StorageType::kSyncable:
      return syncable_usage_tracker_.get();
    case StorageType::kQuotaNotManaged:
      return nullptr;
    case StorageType::kUnknown:
      NOTREACHED();
  }
  return nullptr;
}

std::set<StorageKey> QuotaManagerImpl::GetCachedStorageKeys(StorageType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EnsureDatabaseOpened();
  DCHECK(GetUsageTracker(type));
  return GetUsageTracker(type)->GetCachedStorageKeys();
}

void QuotaManagerImpl::NotifyStorageAccessed(const StorageKey& storage_key,
                                             StorageType type,
                                             base::Time access_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EnsureDatabaseOpened();
  if (type == StorageType::kTemporary && is_getting_eviction_bucket_) {
    // Record the accessed storage keys while GetLRUBucket task is running
    // to filter out them from eviction.
    access_notified_storage_keys_.insert(storage_key);
  }

  if (db_disabled_)
    return;
  PostTaskAndReplyWithResultForDBThread(
      FROM_HERE,
      base::BindOnce(&UpdateAccessTimeOnDBThread, storage_key, type,
                     access_time),
      base::BindOnce(&QuotaManagerImpl::DidDatabaseWork,
                     weak_factory_.GetWeakPtr()));
}

void QuotaManagerImpl::NotifyBucketAccessed(BucketId bucket_id,
                                            base::Time access_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EnsureDatabaseOpened();
  if (is_getting_eviction_bucket_) {
    // Record the accessed buckets while GetLRUStorageKey task is running
    // to filter out them from eviction.
    access_notified_buckets_.insert(bucket_id);
  }

  if (db_disabled_)
    return;
  PostTaskAndReplyWithResultForDBThread(
      FROM_HERE,
      base::BindOnce(&UpdateBucketAccessTimeOnDBThread, bucket_id, access_time),
      base::BindOnce(&QuotaManagerImpl::DidDatabaseWork,
                     weak_factory_.GetWeakPtr()));
}

void QuotaManagerImpl::NotifyStorageModified(QuotaClientType client_id,
                                             const StorageKey& storage_key,
                                             StorageType type,
                                             int64_t delta,
                                             base::Time modification_time,
                                             base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EnsureDatabaseOpened();
  DCHECK(GetUsageTracker(type));
  GetUsageTracker(type)->UpdateUsageCache(client_id, storage_key, delta);

  if (callback)
    std::move(callback).Run();

  if (db_disabled_)
    return;

  PostTaskAndReplyWithResultForDBThread(
      FROM_HERE,
      base::BindOnce(&UpdateModifiedTimeOnDBThread, storage_key, type,
                     modification_time),
      base::BindOnce(&QuotaManagerImpl::DidDatabaseWork,
                     weak_factory_.GetWeakPtr()));
}

void QuotaManagerImpl::NotifyBucketModified(QuotaClientType client_id,
                                            BucketId bucket_id,
                                            int64_t delta,
                                            base::Time modification_time,
                                            base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EnsureDatabaseOpened();

  // TODO(crbug.com/1199417): Update bucket usage in UsageTracker once
  // QuotaClient & UsageTracker operate by bucket.

  if (callback)
    std::move(callback).Run();

  if (db_disabled_)
    return;

  PostTaskAndReplyWithResultForDBThread(
      FROM_HERE,
      base::BindOnce(&UpdateBucketModifiedTimeOnDBThread, bucket_id,
                     modification_time),
      base::BindOnce(&QuotaManagerImpl::DidDatabaseWork,
                     weak_factory_.GetWeakPtr()));
}

void QuotaManagerImpl::DumpQuotaTable(DumpQuotaTableCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DumpQuotaTableHelper* helper = new DumpQuotaTableHelper;
  PostTaskAndReplyWithResultForDBThread(
      FROM_HERE,
      base::BindOnce(&DumpQuotaTableHelper::DumpQuotaTableOnDBThread,
                     base::Unretained(helper)),
      base::BindOnce(&DumpQuotaTableHelper::DidDumpQuotaTable,
                     base::Owned(helper), weak_factory_.GetWeakPtr(),
                     std::move(callback)));
}

void QuotaManagerImpl::DumpBucketTable(DumpBucketTableCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DumpBucketTableHelper* helper = new DumpBucketTableHelper;
  PostTaskAndReplyWithResultForDBThread(
      FROM_HERE,
      base::BindOnce(&DumpBucketTableHelper::DumpBucketTableOnDBThread,
                     base::Unretained(helper)),
      base::BindOnce(&DumpBucketTableHelper::DidDumpBucketTable,
                     base::Owned(helper), weak_factory_.GetWeakPtr(),
                     std::move(callback)));
}

void QuotaManagerImpl::StartEviction() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!temporary_storage_evictor_.get());
  if (eviction_disabled_)
    return;
  temporary_storage_evictor_ = std::make_unique<QuotaTemporaryStorageEvictor>(
      this, kEvictionIntervalInMilliSeconds);
  temporary_storage_evictor_->Start();
}

void QuotaManagerImpl::DeleteStorageKeyFromDatabase(
    const StorageKey& storage_key,
    StorageType type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EnsureDatabaseOpened();
  if (db_disabled_)
    return;

  PostTaskAndReplyWithResultForDBThread(
      FROM_HERE,
      base::BindOnce(&DeleteStorageKeyInfoOnDBThread, storage_key, type),
      base::BindOnce(&QuotaManagerImpl::DidDatabaseWork,
                     weak_factory_.GetWeakPtr()));
}

void QuotaManagerImpl::DeleteBucketFromDatabase(BucketId bucket_id,
                                                bool is_eviction) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EnsureDatabaseOpened();
  if (db_disabled_)
    return;

  PostTaskAndReplyWithResultForDBThread(
      FROM_HERE,
      base::BindOnce(&DeleteBucketInfoOnDBThread, bucket_id, is_eviction),
      base::BindOnce(&QuotaManagerImpl::DidDatabaseWork,
                     weak_factory_.GetWeakPtr()));
}

void QuotaManagerImpl::DidBucketDataEvicted(
    blink::mojom::QuotaStatusCode status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(io_thread_->BelongsToCurrentThread());

  // We only try to evict buckets that are not in use, so basically deletion
  // attempt for eviction should not fail.  Let's record the bucket if we get
  // an error and exclude it from future eviction if the error happens
  // consistently (> kThresholdOfErrorsToBeDenylisted).
  if (status != blink::mojom::QuotaStatusCode::kOk)
    buckets_in_error_[eviction_context_.evicted_bucket.id]++;

  std::move(eviction_context_.evict_bucket_data_callback).Run(status);
}

void QuotaManagerImpl::DeleteBucketDataInternal(
    const BucketLocator& bucket,
    QuotaClientTypes quota_client_types,
    bool is_eviction,
    StatusCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EnsureDatabaseOpened();

  BucketDataDeleter* deleter =
      new BucketDataDeleter(this, bucket, std::move(quota_client_types),
                            is_eviction, std::move(callback));
  deleter->Start();
}

void QuotaManagerImpl::MaybeRunStoragePressureCallback(
    const StorageKey& storage_key,
    int64_t total_space,
    int64_t available_space) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(https://crbug.com/1059560): Figure out what 0 total_space means
  // and how to handle the storage pressure callback in these cases.
  if (total_space == 0)
    return;

  if (!storage_pressure_callback_) {
    // Quota will hold onto a storage pressure notification if no storage
    // pressure callback is set.
    storage_key_for_pending_storage_pressure_callback_ = std::move(storage_key);
    return;
  }

  if (available_space < kStoragePressureThresholdRatio * total_space) {
    storage_pressure_callback_.Run(std::move(storage_key));
  }
}

void QuotaManagerImpl::SimulateStoragePressure(const StorageKey& storage_key) {
  storage_pressure_callback_.Run(storage_key);
}

void QuotaManagerImpl::DetermineStoragePressure(int64_t total_space,
                                                int64_t free_space) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!base::FeatureList::IsEnabled(features::kStoragePressureEvent)) {
    return;
  }
  int64_t threshold_bytes =
      RandomizeByPercent(kGBytes, kThresholdRandomizationPercent);
  int64_t threshold = RandomizeByPercent(
      static_cast<int64_t>(total_space *
                           (kThresholdRandomizationPercent / 100.0)),
      kThresholdRandomizationPercent);
  threshold = std::min(threshold_bytes, threshold);
  if (free_space < threshold && !quota_change_callback_.is_null()) {
    quota_change_callback_.Run();
  }
}

void QuotaManagerImpl::SetStoragePressureCallback(
    base::RepeatingCallback<void(StorageKey)> storage_pressure_callback) {
  storage_pressure_callback_ = storage_pressure_callback;
  if (storage_key_for_pending_storage_pressure_callback_.has_value()) {
    storage_pressure_callback_.Run(
        std::move(storage_key_for_pending_storage_pressure_callback_.value()));
    storage_key_for_pending_storage_pressure_callback_ = absl::nullopt;
  }
}

int QuotaManagerImpl::GetOverrideHandleId() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return ++next_override_handle_id_;
}

void QuotaManagerImpl::OverrideQuotaForStorageKey(
    int handle_id,
    const StorageKey& storage_key,
    absl::optional<int64_t> quota_size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (quota_size.has_value()) {
    DCHECK_GE(next_override_handle_id_, handle_id);
    // Bracket notation is safe here because we want to construct a new
    // QuotaOverride in the case that one does not exist for storage key.
    devtools_overrides_[storage_key].active_override_session_ids.insert(
        handle_id);
    devtools_overrides_[storage_key].quota_size = quota_size.value();
  } else {
    devtools_overrides_.erase(storage_key);
  }
}

void QuotaManagerImpl::WithdrawOverridesForHandle(int handle_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<StorageKey> storage_keys_to_clear;
  for (auto& devtools_override : devtools_overrides_) {
    auto& quota_override = devtools_override.second;
    auto& storage_key = devtools_override.first;

    quota_override.active_override_session_ids.erase(handle_id);

    if (!quota_override.active_override_session_ids.size()) {
      storage_keys_to_clear.push_back(storage_key);
    }
  }

  for (auto& storage_key : storage_keys_to_clear) {
    devtools_overrides_.erase(storage_key);
  }
}

absl::optional<int64_t> QuotaManagerImpl::GetQuotaOverrideForStorageKey(
    const StorageKey& storage_key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!base::Contains(devtools_overrides_, storage_key)) {
    return absl::nullopt;
  }
  return devtools_overrides_[storage_key].quota_size;
}

void QuotaManagerImpl::SetQuotaChangeCallbackForTesting(
    base::RepeatingClosure storage_pressure_event_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  quota_change_callback_ = std::move(storage_pressure_event_callback);
}

void QuotaManagerImpl::ReportHistogram() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!is_incognito_);
  GetGlobalUsage(
      StorageType::kTemporary,
      base::BindOnce(&QuotaManagerImpl::DidGetTemporaryGlobalUsageForHistogram,
                     weak_factory_.GetWeakPtr()));
}

void QuotaManagerImpl::DidGetTemporaryGlobalUsageForHistogram(
    int64_t usage,
    int64_t unlimited_usage) {
  GetStorageCapacity(
      base::BindOnce(&QuotaManagerImpl::DidGetStorageCapacityForHistogram,
                     weak_factory_.GetWeakPtr(), usage));
}

void QuotaManagerImpl::DidGetStorageCapacityForHistogram(
    int64_t usage,
    int64_t total_space,
    int64_t available_space) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  UMA_HISTOGRAM_MBYTES("Quota.GlobalUsageOfTemporaryStorage", usage);
  if (total_space > 0) {
    UMA_HISTOGRAM_PERCENTAGE("Quota.PercentUsedForTemporaryStorage2",
                             static_cast<int>((usage * 100) / total_space));
    UMA_HISTOGRAM_MBYTES("Quota.AvailableDiskSpace2", available_space);
    UMA_HISTOGRAM_PERCENTAGE(
        "Quota.PercentDiskAvailable2",
        std::min(100, static_cast<int>((available_space * 100 / total_space))));
  }

  GetGlobalUsage(
      StorageType::kPersistent,
      base::BindOnce(&QuotaManagerImpl::DidGetPersistentGlobalUsageForHistogram,
                     weak_factory_.GetWeakPtr()));
}

void QuotaManagerImpl::DidGetPersistentGlobalUsageForHistogram(
    int64_t usage,
    int64_t unlimited_usage) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  UMA_HISTOGRAM_MBYTES("Quota.GlobalUsageOfPersistentStorage", usage);

  // We DumpBucketTable last to ensure the trackers caches are loaded.
  DumpBucketTable(
      base::BindOnce(&QuotaManagerImpl::DidDumpBucketTableForHistogram,
                     weak_factory_.GetWeakPtr()));
}

void QuotaManagerImpl::DidDumpBucketTableForHistogram(
    const BucketTableEntries& entries) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::map<StorageKey, int64_t> usage_map =
      GetUsageTracker(StorageType::kTemporary)->GetCachedStorageKeysUsage();
  base::Time now = base::Time::Now();
  for (const auto& info : entries) {
    if (info.type != StorageType::kTemporary)
      continue;

    // Ignore stale database entries. If there is no map entry, the storage
    // key's data has been deleted.
    auto it = usage_map.find(info.storage_key);
    if (it == usage_map.end() || it->second == 0)
      continue;

    base::TimeDelta age =
        now - std::max(info.last_accessed, info.last_modified);
    UMA_HISTOGRAM_COUNTS_1000("Quota.AgeOfOriginInDays", age.InDays());

    int64_t kilobytes = std::max(it->second / int64_t{1024}, int64_t{1});
    base::Histogram::FactoryGet(
        "Quota.AgeOfDataInDays", 1, 1000, 50,
        base::HistogramBase::kUmaTargetedHistogramFlag)->
            AddCount(age.InDays(),
                     base::saturated_cast<int>(kilobytes));
  }
}

std::set<BucketId> QuotaManagerImpl::GetEvictionBucketExceptions() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::set<BucketId> exceptions;
  for (const auto& p : buckets_in_error_) {
    if (p.second > QuotaManagerImpl::kThresholdOfErrorsToBeDenylisted)
      exceptions.insert(p.first);
  }

  return exceptions;
}

void QuotaManagerImpl::DidGetEvictionBucket(
    GetBucketCallback callback,
    const absl::optional<BucketLocator>& bucket) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Make sure the returned bucket has not been accessed since we posted the
  // eviction task.
  DCHECK(!bucket.has_value() ||
         !bucket->storage_key.origin().GetURL().is_empty());
  // TODO(crbug.com/1208141): Remove this evaluation for storage key once
  // QuotaClient is migrated to operate on buckets and NotifyStorageAccessed
  // no longer used.
  if (bucket.has_value() && bucket->is_default &&
      base::Contains(access_notified_storage_keys_, bucket->storage_key)) {
    std::move(callback).Run(absl::nullopt);
  } else if (bucket.has_value() &&
             base::Contains(access_notified_buckets_, bucket->id)) {
    std::move(callback).Run(absl::nullopt);
  } else {
    std::move(callback).Run(bucket);
  }
  access_notified_storage_keys_.clear();
  access_notified_buckets_.clear();

  is_getting_eviction_bucket_ = false;
}

void QuotaManagerImpl::GetEvictionBucket(StorageType type,
                                         int64_t global_quota,
                                         GetBucketCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EnsureDatabaseOpened();
  // This must not be called while there's an in-flight task.
  DCHECK(!is_getting_eviction_bucket_);
  is_getting_eviction_bucket_ = true;

  auto did_get_bucket_callback =
      base::BindOnce(&QuotaManagerImpl::DidGetEvictionBucket,
                     weak_factory_.GetWeakPtr(), std::move(callback));

  if (!is_database_bootstrapped_for_eviction_ && !eviction_disabled_) {
    // Once bootstrapped, GetLRUBucket will be called.
    GetGlobalUsage(
        StorageType::kTemporary,
        base::BindOnce(&QuotaManagerImpl::BootstrapDatabaseForEviction,
                       weak_factory_.GetWeakPtr(),
                       std::move(did_get_bucket_callback)));
    return;
  }

  GetLRUBucket(type, std::move(did_get_bucket_callback));
}

void QuotaManagerImpl::EvictBucketData(const BucketLocator& bucket,
                                       StatusCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(io_thread_->BelongsToCurrentThread());
  DCHECK_EQ(bucket.type, StorageType::kTemporary);

  eviction_context_.evicted_bucket = bucket;
  eviction_context_.evict_bucket_data_callback = std::move(callback);

  DeleteBucketDataInternal(
      bucket, AllQuotaClientTypes(), true,
      base::BindOnce(&QuotaManagerImpl::DidBucketDataEvicted,
                     weak_factory_.GetWeakPtr()));
}

void QuotaManagerImpl::GetEvictionRoundInfo(
    EvictionRoundInfoCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(io_thread_->BelongsToCurrentThread());
  EnsureDatabaseOpened();

  DCHECK(!eviction_helper_);
  eviction_helper_ = std::make_unique<EvictionRoundInfoHelper>(
      this, std::move(callback),
      base::BindOnce(&QuotaManagerImpl::DidGetEvictionRoundInfo,
                     weak_factory_.GetWeakPtr()));
  eviction_helper_->Run();
}

void QuotaManagerImpl::DidGetEvictionRoundInfo() {
  DCHECK(eviction_helper_);
  eviction_helper_.reset();
}

void QuotaManagerImpl::GetLRUBucket(StorageType type,
                                    GetBucketCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  EnsureDatabaseOpened();
  // This must not be called while there's an in-flight task.
  DCHECK(lru_bucket_callback_.is_null());
  lru_bucket_callback_ = std::move(callback);
  if (db_disabled_) {
    std::move(lru_bucket_callback_).Run(absl::nullopt);
    return;
  }

  PostTaskAndReplyWithResultForDBThread(
      base::BindOnce(&GetLRUBucketOnDBThread, type,
                     GetEvictionBucketExceptions(),
                     base::RetainedRef(special_storage_policy_)),
      base::BindOnce(&QuotaManagerImpl::DidGetLRUBucket,
                     weak_factory_.GetWeakPtr()));
}

void QuotaManagerImpl::DidGetPersistentHostQuota(const std::string& host,
                                                 const int64_t* quota,
                                                 bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DidDatabaseWork(success);
  persistent_host_quota_callbacks_.Run(
      host, blink::mojom::QuotaStatusCode::kOk,
      std::min(*quota, kPerHostPersistentQuotaLimit));
}

void QuotaManagerImpl::DidSetPersistentHostQuota(const std::string& host,
                                                 QuotaCallback callback,
                                                 const int64_t* new_quota,
                                                 bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DidDatabaseWork(success);
  std::move(callback).Run(
      success ? blink::mojom::QuotaStatusCode::kOk
              : blink::mojom::QuotaStatusCode::kErrorInvalidAccess,
      *new_quota);
}

void QuotaManagerImpl::DidGetLRUBucket(QuotaErrorOr<BucketLocator> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DidDatabaseWork(result.ok() || result.error() != QuotaError::kDatabaseError);

  if (result.ok()) {
    std::move(lru_bucket_callback_)
        .Run(absl::make_optional(std::move(result.value())));
  } else {
    std::move(lru_bucket_callback_).Run(absl::nullopt);
  }
}

namespace {
void DidGetSettingsThreadAdapter(base::TaskRunner* task_runner,
                                 OptionalQuotaSettingsCallback callback,
                                 absl::optional<QuotaSettings> settings) {
  task_runner->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(settings)));
}
}  // namespace

void QuotaManagerImpl::GetQuotaSettings(QuotaSettingsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (base::TimeTicks::Now() - settings_timestamp_ <
      settings_.refresh_interval) {
    std::move(callback).Run(settings_);
    return;
  }

  if (!settings_callbacks_.Add(std::move(callback)))
    return;

  // We invoke our clients GetQuotaSettingsFunc on the
  // UI thread and plumb the resulting value back to this thread.
  get_settings_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          get_settings_function_,
          base::BindOnce(&DidGetSettingsThreadAdapter,
                         base::RetainedRef(base::ThreadTaskRunnerHandle::Get()),
                         base::BindOnce(&QuotaManagerImpl::DidGetSettings,
                                        weak_factory_.GetWeakPtr()))));
}

void QuotaManagerImpl::DidGetSettings(absl::optional<QuotaSettings> settings) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!settings) {
    settings = settings_;
    settings->refresh_interval = base::Minutes(1);
  }
  SetQuotaSettings(*settings);
  settings_callbacks_.Run(*settings);
  UMA_HISTOGRAM_MBYTES("Quota.GlobalTemporaryPoolSize", settings->pool_size);
  LOG_IF(WARNING, settings->pool_size == 0)
      << "No storage quota provided in QuotaSettings.";
}

void QuotaManagerImpl::GetStorageCapacity(StorageCapacityCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!storage_capacity_callbacks_.Add(std::move(callback)))
    return;
  if (is_incognito_) {
    GetQuotaSettings(
        base::BindOnce(&QuotaManagerImpl::ContinueIncognitoGetStorageCapacity,
                       weak_factory_.GetWeakPtr()));
    return;
  }
  base::PostTaskAndReplyWithResult(
      db_runner_.get(), FROM_HERE,
      base::BindOnce(&QuotaManagerImpl::CallGetVolumeInfo, get_volume_info_fn_,
                     profile_path_),
      base::BindOnce(&QuotaManagerImpl::DidGetStorageCapacity,
                     weak_factory_.GetWeakPtr()));
}

void QuotaManagerImpl::ContinueIncognitoGetStorageCapacity(
    const QuotaSettings& settings) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  int64_t current_usage =
      GetUsageTracker(StorageType::kTemporary)->GetCachedUsage();
  current_usage += GetUsageTracker(StorageType::kPersistent)->GetCachedUsage();
  int64_t available_space =
      std::max(int64_t{0}, settings.pool_size - current_usage);
  DidGetStorageCapacity(std::make_tuple(settings.pool_size, available_space));
}

void QuotaManagerImpl::DidGetStorageCapacity(
    const std::tuple<int64_t, int64_t>& total_and_available) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  int64_t total_space = std::get<0>(total_and_available);
  int64_t available_space = std::get<1>(total_and_available);
  cached_disk_stats_for_storage_pressure_ =
      std::make_tuple(base::TimeTicks::Now(), total_space, available_space);
  storage_capacity_callbacks_.Run(total_space, available_space);
  DetermineStoragePressure(total_space, available_space);
}

void QuotaManagerImpl::DidDatabaseWork(bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  db_disabled_ = !success;
}

void QuotaManagerImpl::DidGetBucket(
    base::OnceCallback<void(QuotaErrorOr<BucketInfo>)> callback,
    QuotaErrorOr<BucketInfo> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DidDatabaseWork(result.ok() || result.error() != QuotaError::kDatabaseError);
  std::move(callback).Run(std::move(result));
}

void QuotaManagerImpl::DidGetBucketForDeletion(
    StatusCallback callback,
    QuotaErrorOr<BucketInfo> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DidDatabaseWork(result.ok() || result.error() != QuotaError::kDatabaseError);

  if (!result.ok()) {
    // Return QuotaStatusCode::kOk if bucket not found. No work needed.
    std::move(callback).Run(result.error() == QuotaError::kNotFound
                                ? blink::mojom::QuotaStatusCode::kOk
                                : blink::mojom::QuotaStatusCode::kUnknown);
    return;
  }

  BucketLocator bucket(result->id, result->storage_key, result->type,
                       result->name == kDefaultBucketName);
  DeleteBucketDataInternal(bucket, AllQuotaClientTypes(),
                           /*is_eviction=*/false, std::move(callback));
  return;
}

void QuotaManagerImpl::DidGetStorageKeys(
    GetStorageKeysCallback callback,
    QuotaErrorOr<std::set<StorageKey>> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DidDatabaseWork(result.ok() || result.error() != QuotaError::kDatabaseError);
  if (!result.ok()) {
    std::move(callback).Run(std::set<StorageKey>());
    return;
  }
  std::move(callback).Run(std::move(result.value()));
}

void QuotaManagerImpl::DidGetBuckets(
    base::OnceCallback<void(QuotaErrorOr<std::set<BucketLocator>>)> callback,
    QuotaErrorOr<std::set<BucketLocator>> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DidDatabaseWork(result.ok() || result.error() != QuotaError::kDatabaseError);
  std::move(callback).Run(std::move(result));
}

void QuotaManagerImpl::DidGetModifiedBetween(
    GetBucketsCallback callback,
    StorageType type,
    QuotaErrorOr<std::set<BucketLocator>> result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DidDatabaseWork(result.ok() || result.error() != QuotaError::kDatabaseError);
  if (!result.ok()) {
    std::move(callback).Run(std::set<BucketLocator>(), type);
    return;
  }
  std::move(callback).Run(result.value(), type);
}

void QuotaManagerImpl::PostTaskAndReplyWithResultForDBThread(
    const base::Location& from_here,
    base::OnceCallback<bool(QuotaDatabase*)> task,
    base::OnceCallback<void(bool)> reply) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Deleting manager will post another task to DB sequence to delete
  // |database_|, therefore we can be sure that database_ is alive when this
  // task runs.
  base::PostTaskAndReplyWithResult(
      db_runner_.get(), from_here,
      base::BindOnce(std::move(task), base::Unretained(database_.get())),
      std::move(reply));
}

template <typename ValueType>
void QuotaManagerImpl::PostTaskAndReplyWithResultForDBThread(
    base::OnceCallback<QuotaErrorOr<ValueType>(QuotaDatabase*)> task,
    base::OnceCallback<void(QuotaErrorOr<ValueType>)> reply,
    const base::Location& from_here) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Deleting manager will post another task to DB sequence to delete
  // |database_|, therefore we can be sure that database_ is alive when this
  // task runs.
  base::PostTaskAndReplyWithResult(
      db_runner_.get(), from_here,
      base::BindOnce(std::move(task), base::Unretained(database_.get())),
      std::move(reply));
}

// static
std::tuple<int64_t, int64_t> QuotaManagerImpl::CallGetVolumeInfo(
    GetVolumeInfoFn get_volume_info_fn,
    const base::FilePath& path) {
  if (!base::CreateDirectory(path)) {
    LOG(WARNING) << "Create directory failed for path" << path.value();
    return std::make_tuple<int64_t, int64_t>(0, 0);
  }
  int64_t total;
  int64_t available;
  std::tie(total, available) = get_volume_info_fn(path);
  if (total < 0 || available < 0) {
    LOG(WARNING) << "Unable to get volume info: " << path.value();
    return std::make_tuple<int64_t, int64_t>(0, 0);
  }
  UMA_HISTOGRAM_MBYTES("Quota.TotalDiskSpace", total);
  UMA_HISTOGRAM_MBYTES("Quota.AvailableDiskSpace", available);
  if (total > 0) {
    UMA_HISTOGRAM_PERCENTAGE("Quota.PercentDiskAvailable",
        std::min(100, static_cast<int>((available * 100) / total)));
  }
  return std::make_tuple(total, available);
}

// static
std::tuple<int64_t, int64_t> QuotaManagerImpl::GetVolumeInfo(
    const base::FilePath& path) {
  return std::make_tuple(base::SysInfo::AmountOfTotalDiskSpace(path),
                         base::SysInfo::AmountOfFreeDiskSpace(path));
}

}  // namespace storage
