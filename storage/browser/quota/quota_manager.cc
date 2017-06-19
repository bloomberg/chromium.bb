// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/quota/quota_manager.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <functional>
#include <limits>
#include <memory>
#include <utility>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/profiler/scoped_tracker.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/sys_info.h"
#include "base/task_runner_util.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "net/base/url_util.h"
#include "storage/browser/quota/client_usage_tracker.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/quota/quota_temporary_storage_evictor.h"
#include "storage/browser/quota/storage_monitor.h"
#include "storage/browser/quota/usage_tracker.h"
#include "storage/common/quota/quota_types.h"

namespace storage {

namespace {

const int64_t kMBytes = 1024 * 1024;
const int kMinutesInMilliSeconds = 60 * 1000;
const int64_t kReportHistogramInterval = 60 * 60 * 1000;  // 1 hour

#define UMA_HISTOGRAM_MBYTES(name, sample)                                     \
  UMA_HISTOGRAM_CUSTOM_COUNTS((name), static_cast<int>((sample) / kMBytes), 1, \
                              10 * 1024 * 1024 /* 10TB */, 100)

}  // namespace

const int64_t QuotaManager::kNoLimit = INT64_MAX;

// Cap size for per-host persistent quota determined by the histogram.
// This is a bit lax value because the histogram says nothing about per-host
// persistent storage usage and we determined by global persistent storage
// usage that is less than 10GB for almost all users.
const int64_t QuotaManager::kPerHostPersistentQuotaLimit = 10 * 1024 * kMBytes;

// Heuristics: assuming average cloud server allows a few Gigs storage
// on the server side and the storage needs to be shared for user data
// and by multiple apps.
int64_t QuotaManager::kSyncableStorageDefaultHostQuota = 500 * kMBytes;

const char QuotaManager::kDatabaseName[] = "QuotaManager";

const int QuotaManager::kThresholdOfErrorsToBeBlacklisted = 3;
const int QuotaManager::kEvictionIntervalInMilliSeconds =
    30 * kMinutesInMilliSeconds;

const char QuotaManager::kDaysBetweenRepeatedOriginEvictionsHistogram[] =
    "Quota.DaysBetweenRepeatedOriginEvictions";
const char QuotaManager::kEvictedOriginAccessedCountHistogram[] =
    "Quota.EvictedOriginAccessCount";
const char QuotaManager::kEvictedOriginDaysSinceAccessHistogram[] =
    "Quota.EvictedOriginDaysSinceAccess";

namespace {

bool IsSupportedType(StorageType type) {
  return type == kStorageTypeTemporary || type == kStorageTypePersistent ||
         type == kStorageTypeSyncable;
}

bool IsSupportedIncognitoType(StorageType type) {
  return type == kStorageTypeTemporary || type == kStorageTypePersistent;
}

void CountOriginType(const std::set<GURL>& origins,
                     SpecialStoragePolicy* policy,
                     size_t* protected_origins,
                     size_t* unlimited_origins) {
  DCHECK(protected_origins);
  DCHECK(unlimited_origins);
  *protected_origins = 0;
  *unlimited_origins = 0;
  if (!policy)
    return;
  for (std::set<GURL>::const_iterator itr = origins.begin();
       itr != origins.end();
       ++itr) {
    if (policy->IsStorageProtected(*itr))
      ++*protected_origins;
    if (policy->IsStorageUnlimited(*itr))
      ++*unlimited_origins;
  }
}

bool GetPersistentHostQuotaOnDBThread(const std::string& host,
                                      int64_t* quota,
                                      QuotaDatabase* database) {
  DCHECK(database);
  database->GetHostQuota(host, kStorageTypePersistent, quota);
  return true;
}

bool SetPersistentHostQuotaOnDBThread(const std::string& host,
                                      int64_t* new_quota,
                                      QuotaDatabase* database) {
  DCHECK(database);
  if (database->SetHostQuota(host, kStorageTypePersistent, *new_quota))
    return true;
  *new_quota = 0;
  return false;
}

bool GetLRUOriginOnDBThread(StorageType type,
                            const std::set<GURL>& exceptions,
                            SpecialStoragePolicy* policy,
                            GURL* url,
                            QuotaDatabase* database) {
  DCHECK(database);
  database->GetLRUOrigin(type, exceptions, policy, url);
  return true;
}

bool DeleteOriginInfoOnDBThread(const GURL& origin,
                                StorageType type,
                                bool is_eviction,
                                QuotaDatabase* database) {
  DCHECK(database);

  base::Time now = base::Time::Now();

  if (is_eviction) {
    QuotaDatabase::OriginInfoTableEntry entry;
    database->GetOriginInfo(origin, type, &entry);
    UMA_HISTOGRAM_COUNTS(QuotaManager::kEvictedOriginAccessedCountHistogram,
                         entry.used_count);
    UMA_HISTOGRAM_COUNTS_1000(
        QuotaManager::kEvictedOriginDaysSinceAccessHistogram,
        (now - entry.last_access_time).InDays());

  }

  if (!database->DeleteOriginInfo(origin, type))
    return false;

  // If the deletion is not due to an eviction, delete the entry in the eviction
  // table as well due to privacy concerns.
  if (!is_eviction)
    return database->DeleteOriginLastEvictionTime(origin, type);

  base::Time last_eviction_time;
  database->GetOriginLastEvictionTime(origin, type, &last_eviction_time);

  if (last_eviction_time != base::Time()) {
    UMA_HISTOGRAM_COUNTS_1000(
        QuotaManager::kDaysBetweenRepeatedOriginEvictionsHistogram,
        (now - last_eviction_time).InDays());
  }

  return database->SetOriginLastEvictionTime(origin, type, now);
}

bool BootstrapDatabaseOnDBThread(const std::set<GURL>* origins,
                                 QuotaDatabase* database) {
  DCHECK(database);
  if (database->IsOriginDatabaseBootstrapped())
    return true;

  // Register existing origins with 0 last time access.
  if (database->RegisterInitialOriginInfo(*origins, kStorageTypeTemporary)) {
    database->SetOriginDatabaseBootstrapped(true);
    return true;
  }
  return false;
}

bool UpdateAccessTimeOnDBThread(const GURL& origin,
                                StorageType type,
                                base::Time accessed_time,
                                QuotaDatabase* database) {
  DCHECK(database);
  return database->SetOriginLastAccessTime(origin, type, accessed_time);
}

bool UpdateModifiedTimeOnDBThread(const GURL& origin,
                                  StorageType type,
                                  base::Time modified_time,
                                  QuotaDatabase* database) {
  DCHECK(database);
  return database->SetOriginLastModifiedTime(origin, type, modified_time);
}

void DidGetUsageAndQuotaForWebApps(
    const QuotaManager::UsageAndQuotaCallback& callback,
    QuotaStatusCode status,
    int64_t usage,
    int64_t quota,
    base::flat_map<QuotaClient::ID, int64_t> usage_breakdown) {
  callback.Run(status, usage, quota);
}

}  // namespace

class QuotaManager::UsageAndQuotaHelper : public QuotaTask {
 public:
  UsageAndQuotaHelper(QuotaManager* manager,
                      const GURL& origin,
                      StorageType type,
                      bool is_unlimited,
                      bool is_session_only,
                      bool is_incognito,
                      const UsageAndQuotaWithBreakdownCallback& callback)
      : QuotaTask(manager),
        origin_(origin),
        callback_(callback),
        type_(type),
        is_unlimited_(is_unlimited),
        is_session_only_(is_session_only),
        is_incognito_(is_incognito),
        weak_factory_(this) {}

 protected:
  void Run() override {
    // Start the async process of gathering the info we need.
    // Gather 4 pieces of info before computing an answer:
    // settings, device_storage_capacity, host_usage, and host_quota.
    base::Closure barrier = base::BarrierClosure(
        4, base::Bind(&UsageAndQuotaHelper::OnBarrierComplete,
                      weak_factory_.GetWeakPtr()));

    std::string host = net::GetHostOrSpecFromURL(origin_);

    manager()->GetQuotaSettings(base::Bind(&UsageAndQuotaHelper::OnGotSettings,
                                           weak_factory_.GetWeakPtr(),
                                           barrier));
    manager()->GetStorageCapacity(
        base::Bind(&UsageAndQuotaHelper::OnGotCapacity,
                   weak_factory_.GetWeakPtr(), barrier));
    manager()->GetHostUsageWithBreakdown(
        host, type_,
        base::Bind(&UsageAndQuotaHelper::OnGotHostUsage,
                   weak_factory_.GetWeakPtr(), barrier));

    // Determine host_quota differently depending on type.
    if (is_unlimited_) {
      SetDesiredHostQuota(barrier, kQuotaStatusOk, kNoLimit);
    } else if (type_ == kStorageTypeSyncable) {
      SetDesiredHostQuota(barrier, kQuotaStatusOk,
                          kSyncableStorageDefaultHostQuota);
    } else if (type_ == kStorageTypePersistent) {
      manager()->GetPersistentHostQuota(
          host, base::Bind(&UsageAndQuotaHelper::SetDesiredHostQuota,
                           weak_factory_.GetWeakPtr(), barrier));
    } else {
      DCHECK_EQ(kStorageTypeTemporary, type_);
      // For temporary storage,  OnGotSettings will set the host quota.
    }
  }

  void Aborted() override {
    weak_factory_.InvalidateWeakPtrs();
    callback_.Run(kQuotaErrorAbort, 0, 0,
                  base::flat_map<QuotaClient::ID, int64_t>());
    DeleteSoon();
  }

  void Completed() override {
    weak_factory_.InvalidateWeakPtrs();

    // Constrain the desired |host_quota| to something that fits.
    // If available space is too low, cap usage at current levels.
    // If it's close to being too low, cap growth to avoid it getting too low.
    int64_t host_quota =
        std::min(desired_host_quota_,
                 host_usage_ +
                     std::max(INT64_C(0), available_space_ -
                                              settings_.must_remain_available));
    callback_.Run(kQuotaStatusOk, host_usage_, host_quota,
                  std::move(host_usage_breakdown_));
    if (type_ == kStorageTypeTemporary && !is_incognito_ && !is_unlimited_) {
      UMA_HISTOGRAM_MBYTES("Quota.QuotaForOrigin", host_quota);
      if (host_quota > 0) {
        UMA_HISTOGRAM_PERCENTAGE("Quota.PercentUsedByOrigin",
            std::min(100, static_cast<int>((host_usage_ * 100) / host_quota)));
      }
    }
    DeleteSoon();
  }

 private:
  QuotaManager* manager() const {
    return static_cast<QuotaManager*>(observer());
  }

  void OnGotSettings(const base::Closure& barrier_closure,
                     const QuotaSettings& settings) {
    settings_ = settings;
    barrier_closure.Run();
    if (type_ == kStorageTypeTemporary && !is_unlimited_) {
      int64_t host_quota = is_session_only_
                               ? settings.session_only_per_host_quota
                               : settings.per_host_quota;
      SetDesiredHostQuota(barrier_closure, kQuotaStatusOk, host_quota);
    }
  }

  void OnGotCapacity(const base::Closure& barrier_closure,
                     int64_t total_space,
                     int64_t available_space) {
    total_space_ = total_space;
    available_space_ = available_space;
    barrier_closure.Run();
  }

  void OnGotHostUsage(
      const base::Closure& barrier_closure,
      int64_t usage,
      base::flat_map<QuotaClient::ID, int64_t> usage_breakdown) {
    host_usage_ = usage;
    host_usage_breakdown_ = std::move(usage_breakdown);
    barrier_closure.Run();
  }

  void SetDesiredHostQuota(const base::Closure& barrier_closure,
                           QuotaStatusCode status,
                           int64_t quota) {
    desired_host_quota_ = quota;
    barrier_closure.Run();
  }

  void OnBarrierComplete() { CallCompleted(); }

  GURL origin_;
  QuotaManager::UsageAndQuotaWithBreakdownCallback callback_;
  StorageType type_;
  bool is_unlimited_;
  bool is_session_only_;
  bool is_incognito_;
  int64_t available_space_ = 0;
  int64_t total_space_ = 0;
  int64_t desired_host_quota_ = 0;
  int64_t host_usage_ = 0;
  base::flat_map<QuotaClient::ID, int64_t> host_usage_breakdown_;
  QuotaSettings settings_;
  base::WeakPtrFactory<UsageAndQuotaHelper> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(UsageAndQuotaHelper);
};

// Helper to asynchronously gather information needed at the start of an
// eviction round.
class QuotaManager::EvictionRoundInfoHelper : public QuotaTask {
 public:
  EvictionRoundInfoHelper(QuotaManager* manager,
                          const EvictionRoundInfoCallback& callback)
      : QuotaTask(manager), callback_(callback), weak_factory_(this) {}

 protected:
  void Run() override {
    // Gather 2 pieces of info before deciding if we need to get GlobalUsage:
    // settings and device_storage_capacity.
    base::Closure barrier = base::BarrierClosure(
        2, base::Bind(&EvictionRoundInfoHelper::OnBarrierComplete,
                      weak_factory_.GetWeakPtr()));

    manager()->GetQuotaSettings(
        base::Bind(&EvictionRoundInfoHelper::OnGotSettings,
                   weak_factory_.GetWeakPtr(), barrier));
    manager()->GetStorageCapacity(
        base::Bind(&EvictionRoundInfoHelper::OnGotCapacity,
                   weak_factory_.GetWeakPtr(), barrier));
  }

  void Aborted() override {
    weak_factory_.InvalidateWeakPtrs();
    callback_.Run(kQuotaErrorAbort, QuotaSettings(), 0, 0, 0, false);
    DeleteSoon();
  }

  void Completed() override {
    weak_factory_.InvalidateWeakPtrs();
    callback_.Run(kQuotaStatusOk, settings_,
                  available_space_, total_space_,
                  global_usage_, global_usage_is_complete_);
    DeleteSoon();
  }

 private:
  QuotaManager* manager() const {
    return static_cast<QuotaManager*>(observer());
  }

  void OnGotSettings(const base::Closure& barrier_closure,
                     const QuotaSettings& settings) {
    settings_ = settings;
    barrier_closure.Run();
  }

  void OnGotCapacity(const base::Closure& barrier_closure,
                     int64_t total_space,
                     int64_t available_space) {
    total_space_ = total_space;
    available_space_ = available_space;
    barrier_closure.Run();
  }

  void OnBarrierComplete() {
    // Avoid computing the full current_usage when there's no pressure.
    int64_t consumed_space = total_space_ - available_space_;
    if (consumed_space < settings_.pool_size &&
        available_space_ > settings_.should_remain_available) {
      DCHECK(!global_usage_is_complete_);
      global_usage_ =
          manager()->GetUsageTracker(kStorageTypeTemporary)->GetCachedUsage();
      CallCompleted();
      return;
    }
    manager()->GetGlobalUsage(
        kStorageTypeTemporary,
        base::Bind(&EvictionRoundInfoHelper::OnGotGlobalUsage,
                   weak_factory_.GetWeakPtr()));
  }

  void OnGotGlobalUsage(int64_t usage, int64_t unlimited_usage) {
    global_usage_ = std::max(INT64_C(0), usage - unlimited_usage);
    global_usage_is_complete_ = true;
    if (total_space_ > 0) {
      UMA_HISTOGRAM_PERCENTAGE("Quota.PercentUsedForTemporaryStorage",
          std::min(100,
              static_cast<int>((global_usage_ * 100) / total_space_)));
    }
    CallCompleted();
  }

  EvictionRoundInfoCallback callback_;
  QuotaSettings settings_;
  int64_t available_space_ = 0;
  int64_t total_space_ = 0;
  int64_t global_usage_ = 0;
  bool global_usage_is_complete_ = false;
  base::WeakPtrFactory<EvictionRoundInfoHelper> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(EvictionRoundInfoHelper);
};

class QuotaManager::GetUsageInfoTask : public QuotaTask {
 public:
  GetUsageInfoTask(
      QuotaManager* manager,
      const GetUsageInfoCallback& callback)
      : QuotaTask(manager),
        callback_(callback),
        weak_factory_(this) {
  }

 protected:
  void Run() override {
    // crbug.com/349708
    TRACE_EVENT0("io", "QuotaManager::GetUsageInfoTask::Run");

    remaining_trackers_ = 3;
    // This will populate cached hosts and usage info.
    manager()->GetUsageTracker(kStorageTypeTemporary)->GetGlobalUsage(
        base::Bind(&GetUsageInfoTask::DidGetGlobalUsage,
                   weak_factory_.GetWeakPtr(),
                   kStorageTypeTemporary));
    manager()->GetUsageTracker(kStorageTypePersistent)->GetGlobalUsage(
        base::Bind(&GetUsageInfoTask::DidGetGlobalUsage,
                   weak_factory_.GetWeakPtr(),
                   kStorageTypePersistent));
    manager()->GetUsageTracker(kStorageTypeSyncable)->GetGlobalUsage(
        base::Bind(&GetUsageInfoTask::DidGetGlobalUsage,
                   weak_factory_.GetWeakPtr(),
                   kStorageTypeSyncable));
  }

  void Completed() override {
    // crbug.com/349708
    TRACE_EVENT0("io", "QuotaManager::GetUsageInfoTask::Completed");

    callback_.Run(entries_);
    DeleteSoon();
  }

  void Aborted() override {
    callback_.Run(UsageInfoEntries());
    DeleteSoon();
  }

 private:
  void AddEntries(StorageType type, UsageTracker* tracker) {
    std::map<std::string, int64_t> host_usage;
    tracker->GetCachedHostsUsage(&host_usage);
    for (std::map<std::string, int64_t>::const_iterator iter =
             host_usage.begin();
         iter != host_usage.end(); ++iter) {
      entries_.push_back(UsageInfo(iter->first, type, iter->second));
    }
    if (--remaining_trackers_ == 0)
      CallCompleted();
  }

  void DidGetGlobalUsage(StorageType type, int64_t, int64_t) {
    DCHECK(manager()->GetUsageTracker(type));
    AddEntries(type, manager()->GetUsageTracker(type));
  }

  QuotaManager* manager() const {
    return static_cast<QuotaManager*>(observer());
  }

  GetUsageInfoCallback callback_;
  UsageInfoEntries entries_;
  int remaining_trackers_;
  base::WeakPtrFactory<GetUsageInfoTask> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(GetUsageInfoTask);
};

class QuotaManager::OriginDataDeleter : public QuotaTask {
 public:
  OriginDataDeleter(QuotaManager* manager,
                    const GURL& origin,
                    StorageType type,
                    int quota_client_mask,
                    bool is_eviction,
                    const StatusCallback& callback)
      : QuotaTask(manager),
        origin_(origin),
        type_(type),
        quota_client_mask_(quota_client_mask),
        error_count_(0),
        remaining_clients_(-1),
        skipped_clients_(0),
        is_eviction_(is_eviction),
        callback_(callback),
        weak_factory_(this) {}

 protected:
  void Run() override {
    error_count_ = 0;
    remaining_clients_ = manager()->clients_.size();
    for (QuotaClientList::iterator iter = manager()->clients_.begin();
         iter != manager()->clients_.end(); ++iter) {
      if (quota_client_mask_ & (*iter)->id()) {
        (*iter)->DeleteOriginData(
            origin_, type_,
            base::Bind(&OriginDataDeleter::DidDeleteOriginData,
                       weak_factory_.GetWeakPtr()));
      } else {
        ++skipped_clients_;
        if (--remaining_clients_ == 0)
          CallCompleted();
      }
    }
  }

  void Completed() override {
    if (error_count_ == 0) {
      // crbug.com/349708
      TRACE_EVENT0("io", "QuotaManager::OriginDataDeleter::Completed Ok");

      // Only remove the entire origin if we didn't skip any client types.
      if (skipped_clients_ == 0)
        manager()->DeleteOriginFromDatabase(origin_, type_, is_eviction_);
      callback_.Run(kQuotaStatusOk);
    } else {
      // crbug.com/349708
      TRACE_EVENT0("io", "QuotaManager::OriginDataDeleter::Completed Error");

      callback_.Run(kQuotaErrorInvalidModification);
    }
    DeleteSoon();
  }

  void Aborted() override {
    callback_.Run(kQuotaErrorAbort);
    DeleteSoon();
  }

 private:
  void DidDeleteOriginData(QuotaStatusCode status) {
    DCHECK_GT(remaining_clients_, 0);

    if (status != kQuotaStatusOk)
      ++error_count_;

    if (--remaining_clients_ == 0)
      CallCompleted();
  }

  QuotaManager* manager() const {
    return static_cast<QuotaManager*>(observer());
  }

  GURL origin_;
  StorageType type_;
  int quota_client_mask_;
  int error_count_;
  int remaining_clients_;
  int skipped_clients_;
  bool is_eviction_;
  StatusCallback callback_;

  base::WeakPtrFactory<OriginDataDeleter> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(OriginDataDeleter);
};

class QuotaManager::HostDataDeleter : public QuotaTask {
 public:
  HostDataDeleter(QuotaManager* manager,
                  const std::string& host,
                  StorageType type,
                  int quota_client_mask,
                  const StatusCallback& callback)
      : QuotaTask(manager),
        host_(host),
        type_(type),
        quota_client_mask_(quota_client_mask),
        error_count_(0),
        remaining_clients_(-1),
        remaining_deleters_(-1),
        callback_(callback),
        weak_factory_(this) {}

 protected:
  void Run() override {
    error_count_ = 0;
    remaining_clients_ = manager()->clients_.size();
    for (QuotaClientList::iterator iter = manager()->clients_.begin();
         iter != manager()->clients_.end(); ++iter) {
      (*iter)->GetOriginsForHost(
          type_, host_,
          base::Bind(&HostDataDeleter::DidGetOriginsForHost,
                     weak_factory_.GetWeakPtr()));
    }
  }

  void Completed() override {
    if (error_count_ == 0) {
      // crbug.com/349708
      TRACE_EVENT0("io", "QuotaManager::HostDataDeleter::Completed Ok");

      callback_.Run(kQuotaStatusOk);
    } else {
      // crbug.com/349708
      TRACE_EVENT0("io", "QuotaManager::HostDataDeleter::Completed Error");

      callback_.Run(kQuotaErrorInvalidModification);
    }
    DeleteSoon();
  }

  void Aborted() override {
    callback_.Run(kQuotaErrorAbort);
    DeleteSoon();
  }

 private:
  void DidGetOriginsForHost(const std::set<GURL>& origins) {
    DCHECK_GT(remaining_clients_, 0);

    origins_.insert(origins.begin(), origins.end());

    if (--remaining_clients_ == 0) {
      if (!origins_.empty())
        ScheduleOriginsDeletion();
      else
        CallCompleted();
    }
  }

  void ScheduleOriginsDeletion() {
    remaining_deleters_ = origins_.size();
    for (std::set<GURL>::const_iterator p = origins_.begin();
         p != origins_.end();
         ++p) {
      OriginDataDeleter* deleter = new OriginDataDeleter(
          manager(), *p, type_, quota_client_mask_, false,
          base::Bind(&HostDataDeleter::DidDeleteOriginData,
                     weak_factory_.GetWeakPtr()));
      deleter->Start();
    }
  }

  void DidDeleteOriginData(QuotaStatusCode status) {
    DCHECK_GT(remaining_deleters_, 0);

    if (status != kQuotaStatusOk)
      ++error_count_;

    if (--remaining_deleters_ == 0)
      CallCompleted();
  }

  QuotaManager* manager() const {
    return static_cast<QuotaManager*>(observer());
  }

  std::string host_;
  StorageType type_;
  int quota_client_mask_;
  std::set<GURL> origins_;
  int error_count_;
  int remaining_clients_;
  int remaining_deleters_;
  StatusCallback callback_;

  base::WeakPtrFactory<HostDataDeleter> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(HostDataDeleter);
};

class QuotaManager::GetModifiedSinceHelper {
 public:
  bool GetModifiedSinceOnDBThread(StorageType type,
                                  base::Time modified_since,
                                  QuotaDatabase* database) {
    DCHECK(database);
    return database->GetOriginsModifiedSince(type, &origins_, modified_since);
  }

  void DidGetModifiedSince(const base::WeakPtr<QuotaManager>& manager,
                           const GetOriginsCallback& callback,
                           StorageType type,
                           bool success) {
    if (!manager) {
      // The operation was aborted.
      callback.Run(std::set<GURL>(), type);
      return;
    }
    manager->DidDatabaseWork(success);
    callback.Run(origins_, type);
  }

 private:
  std::set<GURL> origins_;
};

class QuotaManager::DumpQuotaTableHelper {
 public:
  bool DumpQuotaTableOnDBThread(QuotaDatabase* database) {
    DCHECK(database);
    return database->DumpQuotaTable(
        base::Bind(&DumpQuotaTableHelper::AppendEntry, base::Unretained(this)));
  }

  void DidDumpQuotaTable(const base::WeakPtr<QuotaManager>& manager,
                         const DumpQuotaTableCallback& callback,
                         bool success) {
    if (!manager) {
      // The operation was aborted.
      callback.Run(QuotaTableEntries());
      return;
    }
    manager->DidDatabaseWork(success);
    callback.Run(entries_);
  }

 private:
  bool AppendEntry(const QuotaTableEntry& entry) {
    entries_.push_back(entry);
    return true;
  }

  QuotaTableEntries entries_;
};

class QuotaManager::DumpOriginInfoTableHelper {
 public:
  bool DumpOriginInfoTableOnDBThread(QuotaDatabase* database) {
    DCHECK(database);
    return database->DumpOriginInfoTable(
        base::Bind(&DumpOriginInfoTableHelper::AppendEntry,
                   base::Unretained(this)));
  }

  void DidDumpOriginInfoTable(const base::WeakPtr<QuotaManager>& manager,
                              const DumpOriginInfoTableCallback& callback,
                              bool success) {
    if (!manager) {
      // The operation was aborted.
      callback.Run(OriginInfoTableEntries());
      return;
    }
    manager->DidDatabaseWork(success);
    callback.Run(entries_);
  }

 private:
  bool AppendEntry(const OriginInfoTableEntry& entry) {
    entries_.push_back(entry);
    return true;
  }

  OriginInfoTableEntries entries_;
};

// QuotaManager ---------------------------------------------------------------

QuotaManager::QuotaManager(
    bool is_incognito,
    const base::FilePath& profile_path,
    const scoped_refptr<base::SingleThreadTaskRunner>& io_thread,
    const scoped_refptr<base::SequencedTaskRunner>& db_thread,
    const scoped_refptr<SpecialStoragePolicy>& special_storage_policy,
    const GetQuotaSettingsFunc& get_settings_function)
    : is_incognito_(is_incognito),
      profile_path_(profile_path),
      proxy_(new QuotaManagerProxy(this, io_thread)),
      db_disabled_(false),
      eviction_disabled_(false),
      io_thread_(io_thread),
      db_thread_(db_thread),
      get_settings_function_(get_settings_function),
      is_getting_eviction_origin_(false),
      special_storage_policy_(special_storage_policy),
      get_volume_info_fn_(&QuotaManager::GetVolumeInfo),
      storage_monitor_(new StorageMonitor(this)),
      weak_factory_(this) {
  DCHECK_EQ(settings_.refresh_interval, base::TimeDelta::Max());
  if (!get_settings_function.is_null()) {
    // Reset the interval to ensure we use the get_settings_function
    // the first times settings_ is needed.
    settings_.refresh_interval = base::TimeDelta();
    get_settings_task_runner_ = base::ThreadTaskRunnerHandle::Get();
  }
}

void QuotaManager::SetQuotaSettings(const QuotaSettings& settings) {
  settings_ = settings;
  settings_timestamp_ = base::TimeTicks::Now();
}

void QuotaManager::GetUsageInfo(const GetUsageInfoCallback& callback) {
  LazyInitialize();
  GetUsageInfoTask* get_usage_info = new GetUsageInfoTask(this, callback);
  get_usage_info->Start();
}

void QuotaManager::GetUsageAndQuotaForWebApps(
    const GURL& origin,
    StorageType type,
    const UsageAndQuotaCallback& callback) {
  GetUsageAndQuotaWithBreakdown(
      origin, type, base::Bind(&DidGetUsageAndQuotaForWebApps, callback));
}

void QuotaManager::GetUsageAndQuotaWithBreakdown(
    const GURL& origin,
    StorageType type,
    const UsageAndQuotaWithBreakdownCallback& callback) {
  DCHECK(origin == origin.GetOrigin());
  if (!IsSupportedType(type) ||
      (is_incognito_ && !IsSupportedIncognitoType(type))) {
    callback.Run(kQuotaErrorNotSupported, 0, 0,
                 base::flat_map<QuotaClient::ID, int64_t>());
    return;
  }
  LazyInitialize();

  bool is_session_only = type == kStorageTypeTemporary &&
                         special_storage_policy_ &&
                         special_storage_policy_->IsStorageSessionOnly(origin);
  UsageAndQuotaHelper* helper = new UsageAndQuotaHelper(
      this, origin, type, IsStorageUnlimited(origin, type), is_session_only,
      is_incognito_, callback);
  helper->Start();
}

void QuotaManager::GetUsageAndQuota(const GURL& origin,
                                    StorageType type,
                                    const UsageAndQuotaCallback& callback) {
  DCHECK(origin == origin.GetOrigin());

  if (IsStorageUnlimited(origin, type)) {
    // TODO(michaeln): This seems like a non-obvious odd behavior, probably for
    // apps/extensions, but it would be good to eliminate this special case.
    callback.Run(kQuotaStatusOk, 0, kNoLimit);
    return;
  }

  GetUsageAndQuotaForWebApps(origin, type, callback);
}

void QuotaManager::NotifyStorageAccessed(
    QuotaClient::ID client_id,
    const GURL& origin, StorageType type) {
  DCHECK(origin == origin.GetOrigin());
  NotifyStorageAccessedInternal(client_id, origin, type, base::Time::Now());
}

void QuotaManager::NotifyStorageModified(QuotaClient::ID client_id,
                                         const GURL& origin,
                                         StorageType type,
                                         int64_t delta) {
  DCHECK(origin == origin.GetOrigin());
  NotifyStorageModifiedInternal(client_id, origin, type, delta,
                                base::Time::Now());
}

void QuotaManager::NotifyOriginInUse(const GURL& origin) {
  DCHECK(io_thread_->BelongsToCurrentThread());
  origins_in_use_[origin]++;
}

void QuotaManager::NotifyOriginNoLongerInUse(const GURL& origin) {
  DCHECK(io_thread_->BelongsToCurrentThread());
  DCHECK(IsOriginInUse(origin));
  int& count = origins_in_use_[origin];
  if (--count == 0)
    origins_in_use_.erase(origin);
}

void QuotaManager::SetUsageCacheEnabled(QuotaClient::ID client_id,
                                        const GURL& origin,
                                        StorageType type,
                                        bool enabled) {
  LazyInitialize();
  DCHECK(GetUsageTracker(type));
  GetUsageTracker(type)->SetUsageCacheEnabled(client_id, origin, enabled);
}

void QuotaManager::DeleteOriginData(const GURL& origin,
                                    StorageType type,
                                    int quota_client_mask,
                                    const StatusCallback& callback) {
  DeleteOriginDataInternal(origin, type, quota_client_mask, false, callback);
}

void QuotaManager::DeleteHostData(const std::string& host,
                                  StorageType type,
                                  int quota_client_mask,
                                  const StatusCallback& callback) {
  LazyInitialize();
  if (host.empty() || clients_.empty()) {
    callback.Run(kQuotaStatusOk);
    return;
  }

  HostDataDeleter* deleter =
      new HostDataDeleter(this, host, type, quota_client_mask, callback);
  deleter->Start();
}

void QuotaManager::GetPersistentHostQuota(const std::string& host,
                                          const QuotaCallback& callback) {
  LazyInitialize();
  if (host.empty()) {
    // This could happen if we are called on file:///.
    // TODO(kinuko) We may want to respect --allow-file-access-from-files
    // command line switch.
    callback.Run(kQuotaStatusOk, 0);
    return;
  }

  if (!persistent_host_quota_callbacks_.Add(host, callback))
    return;

  int64_t* quota_ptr = new int64_t(0);
  PostTaskAndReplyWithResultForDBThread(
      FROM_HERE,
      base::Bind(&GetPersistentHostQuotaOnDBThread,
                 host,
                 base::Unretained(quota_ptr)),
      base::Bind(&QuotaManager::DidGetPersistentHostQuota,
                 weak_factory_.GetWeakPtr(),
                 host,
                 base::Owned(quota_ptr)));
}

void QuotaManager::SetPersistentHostQuota(const std::string& host,
                                          int64_t new_quota,
                                          const QuotaCallback& callback) {
  LazyInitialize();
  if (host.empty()) {
    // This could happen if we are called on file:///.
    callback.Run(kQuotaErrorNotSupported, 0);
    return;
  }

  if (new_quota < 0) {
    callback.Run(kQuotaErrorInvalidModification, -1);
    return;
  }

  // Cap the requested size at the per-host quota limit.
  new_quota = std::min(new_quota, kPerHostPersistentQuotaLimit);

  if (db_disabled_) {
    callback.Run(kQuotaErrorInvalidAccess, -1);
    return;
  }

  int64_t* new_quota_ptr = new int64_t(new_quota);
  PostTaskAndReplyWithResultForDBThread(
      FROM_HERE,
      base::Bind(&SetPersistentHostQuotaOnDBThread,
                 host,
                 base::Unretained(new_quota_ptr)),
      base::Bind(&QuotaManager::DidSetPersistentHostQuota,
                 weak_factory_.GetWeakPtr(),
                 host,
                 callback,
                 base::Owned(new_quota_ptr)));
}

void QuotaManager::GetGlobalUsage(StorageType type,
                                  const GlobalUsageCallback& callback) {
  LazyInitialize();
  DCHECK(GetUsageTracker(type));
  GetUsageTracker(type)->GetGlobalUsage(callback);
}

void QuotaManager::GetHostUsage(const std::string& host,
                                StorageType type,
                                const UsageCallback& callback) {
  LazyInitialize();
  DCHECK(GetUsageTracker(type));
  GetUsageTracker(type)->GetHostUsage(host, callback);
}

void QuotaManager::GetHostUsage(const std::string& host,
                                StorageType type,
                                QuotaClient::ID client_id,
                                const UsageCallback& callback) {
  LazyInitialize();
  DCHECK(GetUsageTracker(type));
  ClientUsageTracker* tracker =
      GetUsageTracker(type)->GetClientTracker(client_id);
  if (!tracker) {
    callback.Run(0);
    return;
  }
  tracker->GetHostUsage(host, callback);
}

void QuotaManager::GetHostUsageWithBreakdown(
    const std::string& host,
    StorageType type,
    const UsageWithBreakdownCallback& callback) {
  LazyInitialize();
  DCHECK(GetUsageTracker(type));
  GetUsageTracker(type)->GetHostUsageWithBreakdown(host, callback);
}

bool QuotaManager::IsTrackingHostUsage(StorageType type,
                                       QuotaClient::ID client_id) const {
  UsageTracker* tracker = GetUsageTracker(type);
  return tracker && tracker->GetClientTracker(client_id);
}

void QuotaManager::GetStatistics(
    std::map<std::string, std::string>* statistics) {
  DCHECK(statistics);
  if (temporary_storage_evictor_) {
    std::map<std::string, int64_t> stats;
    temporary_storage_evictor_->GetStatistics(&stats);
    for (std::map<std::string, int64_t>::iterator p = stats.begin();
         p != stats.end(); ++p) {
      (*statistics)[p->first] = base::Int64ToString(p->second);
    }
  }
}

bool QuotaManager::IsStorageUnlimited(const GURL& origin,
                                      StorageType type) const {
  // For syncable storage we should always enforce quota (since the
  // quota must be capped by the server limit).
  if (type == kStorageTypeSyncable)
    return false;
  if (type == kStorageTypeQuotaNotManaged)
    return true;
  return special_storage_policy_.get() &&
         special_storage_policy_->IsStorageUnlimited(origin);
}

void QuotaManager::GetOriginsModifiedSince(StorageType type,
                                           base::Time modified_since,
                                           const GetOriginsCallback& callback) {
  LazyInitialize();
  GetModifiedSinceHelper* helper = new GetModifiedSinceHelper;
  PostTaskAndReplyWithResultForDBThread(
      FROM_HERE,
      base::Bind(&GetModifiedSinceHelper::GetModifiedSinceOnDBThread,
                 base::Unretained(helper),
                 type,
                 modified_since),
      base::Bind(&GetModifiedSinceHelper::DidGetModifiedSince,
                 base::Owned(helper),
                 weak_factory_.GetWeakPtr(),
                 callback,
                 type));
}

bool QuotaManager::ResetUsageTracker(StorageType type) {
  DCHECK(GetUsageTracker(type));
  if (GetUsageTracker(type)->IsWorking())
    return false;
  switch (type) {
    case kStorageTypeTemporary:
      temporary_usage_tracker_.reset(new UsageTracker(
          clients_, kStorageTypeTemporary, special_storage_policy_.get(),
          storage_monitor_.get()));
      return true;
    case kStorageTypePersistent:
      persistent_usage_tracker_.reset(new UsageTracker(
          clients_, kStorageTypePersistent, special_storage_policy_.get(),
          storage_monitor_.get()));
      return true;
    case kStorageTypeSyncable:
      syncable_usage_tracker_.reset(new UsageTracker(
          clients_, kStorageTypeSyncable, special_storage_policy_.get(),
          storage_monitor_.get()));
      return true;
    default:
      NOTREACHED();
  }
  return true;
}

void QuotaManager::AddStorageObserver(
    StorageObserver* observer, const StorageObserver::MonitorParams& params) {
  DCHECK(observer);
  storage_monitor_->AddObserver(observer, params);
}

void QuotaManager::RemoveStorageObserver(StorageObserver* observer) {
  DCHECK(observer);
  storage_monitor_->RemoveObserver(observer);
}

QuotaManager::~QuotaManager() {
  proxy_->manager_ = NULL;
  for (auto* client : clients_)
    client->OnQuotaManagerDestroyed();
  if (database_)
    db_thread_->DeleteSoon(FROM_HERE, database_.release());
}

QuotaManager::EvictionContext::EvictionContext()
    : evicted_type(kStorageTypeUnknown) {
}

QuotaManager::EvictionContext::~EvictionContext() {
}

void QuotaManager::LazyInitialize() {
  DCHECK(io_thread_->BelongsToCurrentThread());
  if (database_) {
    // Already initialized.
    return;
  }

  // Use an empty path to open an in-memory only databse for incognito.
  database_.reset(new QuotaDatabase(is_incognito_ ? base::FilePath() :
      profile_path_.AppendASCII(kDatabaseName)));

  temporary_usage_tracker_.reset(new UsageTracker(
      clients_, kStorageTypeTemporary, special_storage_policy_.get(),
      storage_monitor_.get()));
  persistent_usage_tracker_.reset(new UsageTracker(
      clients_, kStorageTypePersistent, special_storage_policy_.get(),
      storage_monitor_.get()));
  syncable_usage_tracker_.reset(new UsageTracker(
      clients_, kStorageTypeSyncable, special_storage_policy_.get(),
      storage_monitor_.get()));

  if (!is_incognito_) {
    histogram_timer_.Start(
        FROM_HERE, base::TimeDelta::FromMilliseconds(kReportHistogramInterval),
        this, &QuotaManager::ReportHistogram);
  }

  base::PostTaskAndReplyWithResult(
      db_thread_.get(), FROM_HERE,
      base::Bind(&QuotaDatabase::IsOriginDatabaseBootstrapped,
                 base::Unretained(database_.get())),
      base::Bind(&QuotaManager::FinishLazyInitialize,
                 weak_factory_.GetWeakPtr()));
}

void QuotaManager::FinishLazyInitialize(bool is_database_bootstrapped) {
  is_database_bootstrapped_ = is_database_bootstrapped;
  StartEviction();
}

void QuotaManager::BootstrapDatabaseForEviction(
    const GetOriginCallback& did_get_origin_callback,
    int64_t usage,
    int64_t unlimited_usage) {
  // The usage cache should be fully populated now so we can
  // seed the database with origins we know about.
  std::set<GURL>* origins = new std::set<GURL>;
  temporary_usage_tracker_->GetCachedOrigins(origins);
  PostTaskAndReplyWithResultForDBThread(
      FROM_HERE, base::Bind(&BootstrapDatabaseOnDBThread, base::Owned(origins)),
      base::Bind(&QuotaManager::DidBootstrapDatabase,
                 weak_factory_.GetWeakPtr(), did_get_origin_callback));
}

void QuotaManager::DidBootstrapDatabase(
    const GetOriginCallback& did_get_origin_callback,
    bool success) {
  is_database_bootstrapped_ = success;
  DidDatabaseWork(success);
  GetLRUOrigin(kStorageTypeTemporary, did_get_origin_callback);
}

void QuotaManager::RegisterClient(QuotaClient* client) {
  DCHECK(!database_.get());
  clients_.push_back(client);
}

UsageTracker* QuotaManager::GetUsageTracker(StorageType type) const {
  switch (type) {
    case kStorageTypeTemporary:
      return temporary_usage_tracker_.get();
    case kStorageTypePersistent:
      return persistent_usage_tracker_.get();
    case kStorageTypeSyncable:
      return syncable_usage_tracker_.get();
    case kStorageTypeQuotaNotManaged:
      return NULL;
    case kStorageTypeUnknown:
      NOTREACHED();
  }
  return NULL;
}

void QuotaManager::GetCachedOrigins(
    StorageType type, std::set<GURL>* origins) {
  DCHECK(origins);
  LazyInitialize();
  DCHECK(GetUsageTracker(type));
  GetUsageTracker(type)->GetCachedOrigins(origins);
}

void QuotaManager::NotifyStorageAccessedInternal(
    QuotaClient::ID client_id,
    const GURL& origin, StorageType type,
    base::Time accessed_time) {
  LazyInitialize();
  if (type == kStorageTypeTemporary && is_getting_eviction_origin_) {
    // Record the accessed origins while GetLRUOrigin task is runing
    // to filter out them from eviction.
    access_notified_origins_.insert(origin);
  }

  if (db_disabled_)
    return;
  PostTaskAndReplyWithResultForDBThread(
      FROM_HERE,
      base::Bind(&UpdateAccessTimeOnDBThread, origin, type, accessed_time),
      base::Bind(&QuotaManager::DidDatabaseWork,
                 weak_factory_.GetWeakPtr()));
}

void QuotaManager::NotifyStorageModifiedInternal(QuotaClient::ID client_id,
                                                 const GURL& origin,
                                                 StorageType type,
                                                 int64_t delta,
                                                 base::Time modified_time) {
  LazyInitialize();
  DCHECK(GetUsageTracker(type));
  GetUsageTracker(type)->UpdateUsageCache(client_id, origin, delta);

  PostTaskAndReplyWithResultForDBThread(
      FROM_HERE,
      base::Bind(&UpdateModifiedTimeOnDBThread, origin, type, modified_time),
      base::Bind(&QuotaManager::DidDatabaseWork,
                 weak_factory_.GetWeakPtr()));
}

void QuotaManager::DumpQuotaTable(const DumpQuotaTableCallback& callback) {
  DumpQuotaTableHelper* helper = new DumpQuotaTableHelper;
  PostTaskAndReplyWithResultForDBThread(
      FROM_HERE,
      base::Bind(&DumpQuotaTableHelper::DumpQuotaTableOnDBThread,
                 base::Unretained(helper)),
      base::Bind(&DumpQuotaTableHelper::DidDumpQuotaTable,
                 base::Owned(helper),
                 weak_factory_.GetWeakPtr(),
                 callback));
}

void QuotaManager::DumpOriginInfoTable(
    const DumpOriginInfoTableCallback& callback) {
  DumpOriginInfoTableHelper* helper = new DumpOriginInfoTableHelper;
  PostTaskAndReplyWithResultForDBThread(
      FROM_HERE,
      base::Bind(&DumpOriginInfoTableHelper::DumpOriginInfoTableOnDBThread,
                 base::Unretained(helper)),
      base::Bind(&DumpOriginInfoTableHelper::DidDumpOriginInfoTable,
                 base::Owned(helper),
                 weak_factory_.GetWeakPtr(),
                 callback));
}

void QuotaManager::StartEviction() {
  DCHECK(!temporary_storage_evictor_.get());
  if (eviction_disabled_)
    return;
  temporary_storage_evictor_.reset(new QuotaTemporaryStorageEvictor(
      this, kEvictionIntervalInMilliSeconds));
  temporary_storage_evictor_->Start();
}

void QuotaManager::DeleteOriginFromDatabase(const GURL& origin,
                                            StorageType type,
                                            bool is_eviction) {
  LazyInitialize();
  if (db_disabled_)
    return;

  PostTaskAndReplyWithResultForDBThread(
      FROM_HERE,
      base::Bind(&DeleteOriginInfoOnDBThread, origin, type, is_eviction),
      base::Bind(&QuotaManager::DidDatabaseWork, weak_factory_.GetWeakPtr()));
}

void QuotaManager::DidOriginDataEvicted(QuotaStatusCode status) {
  DCHECK(io_thread_->BelongsToCurrentThread());

  // We only try evict origins that are not in use, so basically
  // deletion attempt for eviction should not fail.  Let's record
  // the origin if we get error and exclude it from future eviction
  // if the error happens consistently (> kThresholdOfErrorsToBeBlacklisted).
  if (status != kQuotaStatusOk)
    origins_in_error_[eviction_context_.evicted_origin]++;

  eviction_context_.evict_origin_data_callback.Run(status);
  eviction_context_.evict_origin_data_callback.Reset();
}

void QuotaManager::DeleteOriginDataInternal(const GURL& origin,
                                            StorageType type,
                                            int quota_client_mask,
                                            bool is_eviction,
                                            const StatusCallback& callback) {
  LazyInitialize();

  if (origin.is_empty() || clients_.empty()) {
    callback.Run(kQuotaStatusOk);
    return;
  }

  DCHECK(origin == origin.GetOrigin());
  OriginDataDeleter* deleter = new OriginDataDeleter(
      this, origin, type, quota_client_mask, is_eviction, callback);
  deleter->Start();
}

void QuotaManager::ReportHistogram() {
  DCHECK(!is_incognito_);
  GetGlobalUsage(kStorageTypeTemporary,
                 base::Bind(
                     &QuotaManager::DidGetTemporaryGlobalUsageForHistogram,
                     weak_factory_.GetWeakPtr()));
}

void QuotaManager::DidGetTemporaryGlobalUsageForHistogram(
    int64_t usage,
    int64_t unlimited_usage) {
  UMA_HISTOGRAM_MBYTES("Quota.GlobalUsageOfTemporaryStorage", usage);

  std::set<GURL> origins;
  GetCachedOrigins(kStorageTypeTemporary, &origins);

  size_t num_origins = origins.size();
  size_t protected_origins = 0;
  size_t unlimited_origins = 0;
  CountOriginType(origins,
                  special_storage_policy_.get(),
                  &protected_origins,
                  &unlimited_origins);
  UMA_HISTOGRAM_COUNTS("Quota.NumberOfTemporaryStorageOrigins",
                       num_origins);
  UMA_HISTOGRAM_COUNTS("Quota.NumberOfProtectedTemporaryStorageOrigins",
                       protected_origins);
  UMA_HISTOGRAM_COUNTS("Quota.NumberOfUnlimitedTemporaryStorageOrigins",
                       unlimited_origins);

  GetGlobalUsage(kStorageTypePersistent,
                 base::Bind(
                     &QuotaManager::DidGetPersistentGlobalUsageForHistogram,
                     weak_factory_.GetWeakPtr()));
}

void QuotaManager::DidGetPersistentGlobalUsageForHistogram(
    int64_t usage,
    int64_t unlimited_usage) {
  UMA_HISTOGRAM_MBYTES("Quota.GlobalUsageOfPersistentStorage", usage);

  std::set<GURL> origins;
  GetCachedOrigins(kStorageTypePersistent, &origins);

  size_t num_origins = origins.size();
  size_t protected_origins = 0;
  size_t unlimited_origins = 0;
  CountOriginType(origins,
                  special_storage_policy_.get(),
                  &protected_origins,
                  &unlimited_origins);
  UMA_HISTOGRAM_COUNTS("Quota.NumberOfPersistentStorageOrigins",
                       num_origins);
  UMA_HISTOGRAM_COUNTS("Quota.NumberOfProtectedPersistentStorageOrigins",
                       protected_origins);
  UMA_HISTOGRAM_COUNTS("Quota.NumberOfUnlimitedPersistentStorageOrigins",
                       unlimited_origins);

  // We DumpOriginInfoTable last to ensure the trackers caches are loaded.
  DumpOriginInfoTable(
      base::Bind(&QuotaManager::DidDumpOriginInfoTableForHistogram,
                 weak_factory_.GetWeakPtr()));
}

void QuotaManager::DidDumpOriginInfoTableForHistogram(
    const OriginInfoTableEntries& entries) {
  using UsageMap = std::map<GURL, int64_t>;
  UsageMap usage_map;
  GetUsageTracker(kStorageTypeTemporary)->GetCachedOriginsUsage(&usage_map);
  base::Time now = base::Time::Now();
  for (const auto& info : entries) {
    if (info.type != kStorageTypeTemporary)
      continue;

    // Ignore stale database entries. If there is no map entry, the origin's
    // data has been deleted.
    UsageMap::const_iterator found = usage_map.find(info.origin);
    if (found == usage_map.end() || found->second == 0)
      continue;

    base::TimeDelta age = now - std::max(info.last_access_time,
                                         info.last_modified_time);
    UMA_HISTOGRAM_COUNTS_1000("Quota.AgeOfOriginInDays", age.InDays());

    int64_t kilobytes = std::max(found->second / INT64_C(1024), INT64_C(1));
    base::Histogram::FactoryGet(
        "Quota.AgeOfDataInDays", 1, 1000, 50,
        base::HistogramBase::kUmaTargetedHistogramFlag)->
            AddCount(age.InDays(),
                     base::saturated_cast<int>(kilobytes));
  }
}

std::set<GURL> QuotaManager::GetEvictionOriginExceptions(
    const std::set<GURL>& extra_exceptions) {
  std::set<GURL> exceptions = extra_exceptions;
  for (const auto& p : origins_in_use_) {
    if (p.second > 0)
      exceptions.insert(p.first);
  }

  for (const auto& p : origins_in_error_) {
    if (p.second > QuotaManager::kThresholdOfErrorsToBeBlacklisted)
      exceptions.insert(p.first);
  }

  return exceptions;
}

void QuotaManager::DidGetEvictionOrigin(const GetOriginCallback& callback,
                                        const GURL& origin) {
  // Make sure the returned origin is (still) not in the origin_in_use_ set
  // and has not been accessed since we posted the task.
  if (base::ContainsKey(origins_in_use_, origin) ||
      base::ContainsKey(access_notified_origins_, origin)) {
    callback.Run(GURL());
  } else {
    callback.Run(origin);
  }
  access_notified_origins_.clear();

  is_getting_eviction_origin_ = false;
}

void QuotaManager::GetEvictionOrigin(StorageType type,
                                     const std::set<GURL>& extra_exceptions,
                                     int64_t global_quota,
                                     const GetOriginCallback& callback) {
  LazyInitialize();
  // This must not be called while there's an in-flight task.
  DCHECK(!is_getting_eviction_origin_);
  is_getting_eviction_origin_ = true;

  GetOriginCallback did_get_origin_callback =
      base::Bind(&QuotaManager::DidGetEvictionOrigin,
                 weak_factory_.GetWeakPtr(), callback);

  if (!is_database_bootstrapped_ && !eviction_disabled_) {
    // Once bootstrapped, GetLRUOrigin will be called.
    GetGlobalUsage(
        kStorageTypeTemporary,
        base::Bind(&QuotaManager::BootstrapDatabaseForEviction,
                   weak_factory_.GetWeakPtr(), did_get_origin_callback));
    return;
  }

  GetLRUOrigin(type, did_get_origin_callback);
}

void QuotaManager::EvictOriginData(const GURL& origin,
                                   StorageType type,
                                   const StatusCallback& callback) {
  DCHECK(io_thread_->BelongsToCurrentThread());
  DCHECK_EQ(type, kStorageTypeTemporary);

  eviction_context_.evicted_origin = origin;
  eviction_context_.evicted_type = type;
  eviction_context_.evict_origin_data_callback = callback;

  DeleteOriginDataInternal(origin, type, QuotaClient::kAllClientsMask, true,
                           base::Bind(&QuotaManager::DidOriginDataEvicted,
                                      weak_factory_.GetWeakPtr()));
}

void QuotaManager::GetEvictionRoundInfo(
    const EvictionRoundInfoCallback& callback) {
  DCHECK(io_thread_->BelongsToCurrentThread());
  LazyInitialize();
  EvictionRoundInfoHelper* helper = new EvictionRoundInfoHelper(this, callback);
  helper->Start();
}

void QuotaManager::GetLRUOrigin(StorageType type,
                                const GetOriginCallback& callback) {
  LazyInitialize();
  // This must not be called while there's an in-flight task.
  DCHECK(lru_origin_callback_.is_null());
  lru_origin_callback_ = callback;
  if (db_disabled_) {
    lru_origin_callback_.Run(GURL());
    lru_origin_callback_.Reset();
    return;
  }

  GURL* url = new GURL;
  PostTaskAndReplyWithResultForDBThread(
      FROM_HERE, base::Bind(&GetLRUOriginOnDBThread, type,
                            GetEvictionOriginExceptions(std::set<GURL>()),
                            base::RetainedRef(special_storage_policy_),
                            base::Unretained(url)),
      base::Bind(&QuotaManager::DidGetLRUOrigin, weak_factory_.GetWeakPtr(),
                 base::Owned(url)));
}

void QuotaManager::DidGetPersistentHostQuota(const std::string& host,
                                             const int64_t* quota,
                                             bool success) {
  DidDatabaseWork(success);
  persistent_host_quota_callbacks_.Run(
      host, kQuotaStatusOk, std::min(*quota, kPerHostPersistentQuotaLimit));
}

void QuotaManager::DidSetPersistentHostQuota(const std::string& host,
                                             const QuotaCallback& callback,
                                             const int64_t* new_quota,
                                             bool success) {
  DidDatabaseWork(success);
  callback.Run(success ? kQuotaStatusOk : kQuotaErrorInvalidAccess, *new_quota);
}

void QuotaManager::DidGetLRUOrigin(const GURL* origin,
                                   bool success) {
  DidDatabaseWork(success);

  lru_origin_callback_.Run(*origin);
  lru_origin_callback_.Reset();
}

namespace {
void DidGetSettingsThreadAdapter(base::TaskRunner* task_runner,
                                 const OptionalQuotaSettingsCallback& callback,
                                 base::Optional<QuotaSettings> settings) {
  task_runner->PostTask(FROM_HERE, base::Bind(callback, std::move(settings)));
}
}  // namespace

void QuotaManager::GetQuotaSettings(const QuotaSettingsCallback& callback) {
  if (base::TimeTicks::Now() - settings_timestamp_ <
      settings_.refresh_interval) {
    callback.Run(settings_);
    return;
  }

  if (!settings_callbacks_.Add(callback))
    return;

  // We invoke our clients GetQuotaSettingsFunc on the
  // UI thread and plumb the resulting value back to this thread.
  get_settings_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(
          get_settings_function_,
          base::Bind(
              &DidGetSettingsThreadAdapter,
              base::RetainedRef(base::ThreadTaskRunnerHandle::Get()),
              base::Bind(&QuotaManager::DidGetSettings,
                         weak_factory_.GetWeakPtr(), base::TimeTicks::Now()))));
}

void QuotaManager::DidGetSettings(base::TimeTicks start_ticks,
                                  base::Optional<QuotaSettings> settings) {
  if (!settings) {
    settings = settings_;
    settings->refresh_interval = base::TimeDelta::FromMinutes(1);
  }
  SetQuotaSettings(*settings);
  settings_callbacks_.Run(*settings);
  UMA_HISTOGRAM_MBYTES("Quota.GlobalTemporaryPoolSize", settings->pool_size);
  UMA_HISTOGRAM_LONG_TIMES("Quota.TimeToGetSettings",
                           base::TimeTicks::Now() - start_ticks);
  LOG_IF(WARNING, settings->pool_size == 0)
      << "No storage quota provided in QuotaSettings.";
}

void QuotaManager::GetStorageCapacity(const StorageCapacityCallback& callback) {
  if (!storage_capacity_callbacks_.Add(callback))
    return;
  if (is_incognito_) {
    GetQuotaSettings(
        base::Bind(&QuotaManager::ContinueIncognitoGetStorageCapacity,
                   weak_factory_.GetWeakPtr()));
    return;
  }
  base::PostTaskAndReplyWithResult(
      db_thread_.get(), FROM_HERE,
      base::Bind(&QuotaManager::CallGetVolumeInfo, get_volume_info_fn_,
                 profile_path_),
      base::Bind(&QuotaManager::DidGetStorageCapacity,
                 weak_factory_.GetWeakPtr()));
}

void QuotaManager::ContinueIncognitoGetStorageCapacity(
    const QuotaSettings& settings) {
  int64_t current_usage =
      GetUsageTracker(kStorageTypeTemporary)->GetCachedUsage();
  current_usage += GetUsageTracker(kStorageTypePersistent)->GetCachedUsage();
  int64_t available_space =
      std::max(INT64_C(0), settings.pool_size - current_usage);
  DidGetStorageCapacity(std::make_tuple(settings.pool_size, available_space));
}

void QuotaManager::DidGetStorageCapacity(
    const std::tuple<int64_t, int64_t>& total_and_available) {
  storage_capacity_callbacks_.Run(std::get<0>(total_and_available),
                                  std::get<1>(total_and_available));
}

void QuotaManager::DidDatabaseWork(bool success) {
  db_disabled_ = !success;
}

void QuotaManager::DeleteOnCorrectThread() const {
  if (!io_thread_->BelongsToCurrentThread() &&
      io_thread_->DeleteSoon(FROM_HERE, this)) {
    return;
  }
  delete this;
}

void QuotaManager::PostTaskAndReplyWithResultForDBThread(
    const tracked_objects::Location& from_here,
    base::Callback<bool(QuotaDatabase*)> task,
    base::Callback<void(bool)> reply) {
  // Deleting manager will post another task to DB thread to delete
  // |database_|, therefore we can be sure that database_ is alive when this
  // task runs.
  base::PostTaskAndReplyWithResult(
      db_thread_.get(), from_here,
      base::Bind(std::move(task), base::Unretained(database_.get())),
      std::move(reply));
}

// static
std::tuple<int64_t, int64_t> QuotaManager::CallGetVolumeInfo(
    GetVolumeInfoFn get_volume_info_fn,
    const base::FilePath& path) {
  // crbug.com/349708
  TRACE_EVENT0("io", "CallGetVolumeInfo");
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
std::tuple<int64_t, int64_t> QuotaManager::GetVolumeInfo(
    const base::FilePath& path) {
  return std::make_tuple(base::SysInfo::AmountOfTotalDiskSpace(path),
                         base::SysInfo::AmountOfFreeDiskSpace(path));
}

}  // namespace storage
