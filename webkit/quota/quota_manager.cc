// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/quota/quota_manager.h"

#include <algorithm>
#include <deque>
#include <set>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/metrics/histogram.h"
#include "base/string_number_conversions.h"
#include "base/sys_info.h"
#include "base/task_runner_util.h"
#include "base/time.h"
#include "net/base/net_util.h"
#include "webkit/quota/quota_database.h"
#include "webkit/quota/quota_temporary_storage_evictor.h"
#include "webkit/quota/quota_types.h"
#include "webkit/quota/usage_tracker.h"

#define UMA_HISTOGRAM_MBYTES(name, sample)          \
  UMA_HISTOGRAM_CUSTOM_COUNTS(                      \
      (name), static_cast<int>((sample) / kMBytes), \
      1, 10 * 1024 * 1024 /* 10TB */, 100)

namespace quota {

namespace {

const int64 kMBytes = 1024 * 1024;
const int kMinutesInMilliSeconds = 60 * 1000;

const int64 kIncognitoDefaultTemporaryQuota = 50 * kMBytes;
const int64 kReportHistogramInterval = 60 * 60 * 1000;  // 1 hour
const double kTemporaryQuotaRatioToAvail = 0.5;  // 50%

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

bool SetTemporaryGlobalOverrideQuotaOnDBThread(int64* new_quota,
                                               QuotaDatabase* database) {
  DCHECK(database);
  if (!database->SetQuotaConfigValue(
          QuotaDatabase::kTemporaryQuotaOverrideKey, *new_quota)) {
    *new_quota = -1;
    return false;
  }
  return true;
}

bool GetPersistentHostQuotaOnDBThread(const std::string& host,
                                      int64* quota,
                                      QuotaDatabase* database) {
  DCHECK(database);
  database->GetHostQuota(host, kStorageTypePersistent, quota);
  return true;
}

bool SetPersistentHostQuotaOnDBThread(const std::string& host,
                                      int64* new_quota,
                                      QuotaDatabase* database) {
  DCHECK(database);
  if (database->SetHostQuota(host, kStorageTypePersistent, *new_quota))
    return true;
  *new_quota = 0;
  return false;
}

bool InitializeOnDBThread(int64* temporary_quota_override,
                          int64* desired_available_space,
                          QuotaDatabase* database) {
  DCHECK(database);
  database->GetQuotaConfigValue(QuotaDatabase::kTemporaryQuotaOverrideKey,
                                temporary_quota_override);
  database->GetQuotaConfigValue(QuotaDatabase::kDesiredAvailableSpaceKey,
                                desired_available_space);
  return true;
}

bool GetLRUOriginOnDBThread(StorageType type,
                            std::set<GURL>* exceptions,
                            SpecialStoragePolicy* policy,
                            GURL* url,
                            QuotaDatabase* database) {
  DCHECK(database);
  database->GetLRUOrigin(type, *exceptions, policy, url);
  return true;
}

bool DeleteOriginInfoOnDBThread(const GURL& origin,
                                StorageType type,
                                QuotaDatabase* database) {
  DCHECK(database);
  return database->DeleteOriginInfo(origin, type);
}

bool InitializeTemporaryOriginsInfoOnDBThread(const std::set<GURL>* origins,
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

}  // anonymous namespace

const int64 QuotaManager::kNoLimit = kint64max;

const int QuotaManager::kPerHostTemporaryPortion = 5;  // 20%

const char QuotaManager::kDatabaseName[] = "QuotaManager";

const int QuotaManager::kThresholdOfErrorsToBeBlacklisted = 3;

const int QuotaManager::kEvictionIntervalInMilliSeconds =
    30 * kMinutesInMilliSeconds;

// Heuristics: assuming average cloud server allows a few Gigs storage
// on the server side and the storage needs to be shared for user data
// and by multiple apps.
int64 QuotaManager::kSyncableStorageDefaultHostQuota = 500 * kMBytes;

// Callback translators.
void CallGetUsageAndQuotaCallback(
    const QuotaManager::GetUsageAndQuotaCallback& callback,
    bool unlimited,
    bool is_installed_app,
    QuotaStatusCode status,
    const QuotaAndUsage& quota_and_usage) {
  int64 usage;
  int64 quota;

  if (unlimited) {
    usage = quota_and_usage.unlimited_usage;
    quota = is_installed_app ? quota_and_usage.available_disk_space :
        QuotaManager::kNoLimit;
  } else {
    usage = quota_and_usage.usage;
    quota = quota_and_usage.quota;
  }

  callback.Run(status, usage, quota);
}

void CallQuotaCallback(
    const QuotaCallback& callback,
    StorageType type,
    QuotaStatusCode status,
    const QuotaAndUsage& quota_and_usage) {
  callback.Run(status, type, quota_and_usage.quota);
}

// This class is for posting GetUsage/GetQuota tasks, gathering
// results and dispatching GetAndQuota callbacks.
// This class is self-destructed.
class QuotaManager::UsageAndQuotaDispatcherTask : public QuotaTask {
 public:
  typedef UsageAndQuotaDispatcherCallback Callback;
  typedef std::deque<Callback> CallbackList;

  static UsageAndQuotaDispatcherTask* Create(
      QuotaManager* manager,
      bool global,
      const HostAndType& host_and_type);

  // Returns true if it is the first call for this task; which means
  // the caller needs to call Start().
  bool AddCallback(const Callback& callback) {
    callbacks_.push_back(callback);
    return (callbacks_.size() == 1);
  }

  void DidGetGlobalUsage(StorageType type, int64 usage, int64 unlimited_usage) {
    DCHECK_EQ(this->type(), type);
    DCHECK_GE(usage, unlimited_usage);
    if (quota_status_ == kQuotaStatusUnknown)
      quota_status_ = kQuotaStatusOk;
    global_usage_ = usage;
    global_unlimited_usage_ = unlimited_usage;
    CheckCompleted();
  }

  void DidGetHostUsage(const std::string& host, StorageType type, int64 usage) {
    DCHECK_EQ(this->host(), host);
    DCHECK_EQ(this->type(), type);
    if (quota_status_ == kQuotaStatusUnknown)
      quota_status_ = kQuotaStatusOk;
    host_usage_ = usage;
    CheckCompleted();
  }

  void DidGetHostQuota(QuotaStatusCode status,
                       const std::string& host,
                       StorageType type,
                       int64 host_quota) {
    DCHECK_EQ(this->host(), host);
    DCHECK_EQ(this->type(), type);
    if (quota_status_ == kQuotaStatusUnknown || quota_status_ == kQuotaStatusOk)
      quota_status_ = status;
    host_quota_ = host_quota;
    CheckCompleted();
  }

  void DidGetAvailableSpace(QuotaStatusCode status, int64 space) {
    DCHECK_GE(space, 0);
    if (quota_status_ == kQuotaStatusUnknown || quota_status_ == kQuotaStatusOk)
      quota_status_ = status;
    available_space_ = space;
    CheckCompleted();
  }

  bool IsStartable() const {
    return !started_ && !callbacks_.empty();
  }

 protected:
  UsageAndQuotaDispatcherTask(
      QuotaManager* manager,
      const HostAndType& host_and_type)
      : QuotaTask(manager),
        host_and_type_(host_and_type),
        started_(false),
        host_quota_(-1),
        global_usage_(-1),
        global_unlimited_usage_(-1),
        host_usage_(-1),
        available_space_(-1),
        quota_status_(kQuotaStatusUnknown),
        waiting_callbacks_(1),
        weak_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {}

  virtual ~UsageAndQuotaDispatcherTask() {}

  // Subclasses must implement them.
  virtual void RunBody() = 0;
  virtual void DispatchCallbacks() = 0;

  virtual void Run() OVERRIDE {
    DCHECK(!started_);
    started_ = true;
    RunBody();
    // We initialize waiting_callbacks to 1 so that we won't run
    // the completion callback until here even some of the callbacks
    // are dispatched synchronously.
    CheckCompleted();
  }

  virtual void Aborted() OVERRIDE {
    CallCallbacksAndClear(kQuotaErrorAbort, 0, 0, 0, 0);
    DeleteSoon();
  }

  virtual void Completed() OVERRIDE {
    DeleteSoon();
  }

  void CallCallbacksAndClear(
      QuotaStatusCode status,
      int64 usage, int64 unlimited_usage, int64 quota,
      int64 available_space) {
    QuotaAndUsage qau = { usage, unlimited_usage, quota, available_space };
    for (CallbackList::iterator iter = callbacks_.begin();
         iter != callbacks_.end(); ++iter) {
      (*iter).Run(status, qau);
    }
    callbacks_.clear();
  }

  QuotaManager* manager() const {
    return static_cast<QuotaManager*>(observer());
  }

  std::string host() const { return host_and_type_.first; }
  virtual StorageType type() const { return host_and_type_.second; }
  int64 host_quota() const { return host_quota_; }
  int64 global_usage() const { return global_usage_; }
  int64 global_unlimited_usage() const { return global_unlimited_usage_; }
  int64 host_usage() const { return host_usage_; }
  int64 available_space() const { return available_space_; }
  QuotaStatusCode quota_status() const { return quota_status_; }
  CallbackList& callbacks() { return callbacks_; }

  // The main logic that determines the temporary global quota.
  int64 temporary_global_quota() const {
    DCHECK_EQ(type(), kStorageTypeTemporary);
    DCHECK(manager());
    DCHECK_GE(global_usage(), global_unlimited_usage());
    if (manager()->temporary_quota_override_ > 0) {
      // If the user has specified an explicit temporary quota, use the value.
      return manager()->temporary_quota_override_;
    }
    int64 limited_usage = global_usage() - global_unlimited_usage();
    int64 avail_space = available_space();
    if (avail_space < kint64max - limited_usage) {
      // We basically calculate the temporary quota by
      // [available_space + space_used_for_temp] * kTempQuotaRatio,
      // but make sure we'll have no overflow.
      avail_space += limited_usage;
    }
    return avail_space * kTemporaryQuotaRatioToAvail;
  }

  // Subclasses must call following methods to create a new 'waitable'
  // callback, which decrements waiting_callbacks when it is called.
  GlobalUsageCallback NewWaitableGlobalUsageCallback() {
    ++waiting_callbacks_;
    return base::Bind(&UsageAndQuotaDispatcherTask::DidGetGlobalUsage,
                      weak_factory_.GetWeakPtr());
  }
  HostUsageCallback NewWaitableHostUsageCallback() {
    ++waiting_callbacks_;
    return base::Bind(&UsageAndQuotaDispatcherTask::DidGetHostUsage,
                      weak_factory_.GetWeakPtr());
  }
  HostQuotaCallback NewWaitableHostQuotaCallback() {
    ++waiting_callbacks_;
    return base::Bind(&UsageAndQuotaDispatcherTask::DidGetHostQuota,
                      weak_factory_.GetWeakPtr());
  }
  AvailableSpaceCallback NewWaitableAvailableSpaceCallback() {
    ++waiting_callbacks_;
    return base::Bind(&UsageAndQuotaDispatcherTask::DidGetAvailableSpace,
                      weak_factory_.GetWeakPtr());
  }


 private:
  void CheckCompleted() {
    if (--waiting_callbacks_ <= 0) {
      DispatchCallbacks();
      DCHECK(callbacks_.empty());

      UsageAndQuotaDispatcherTaskMap& dispatcher_map =
          manager()->usage_and_quota_dispatchers_;
      DCHECK(dispatcher_map.find(host_and_type_) != dispatcher_map.end());
      dispatcher_map.erase(host_and_type_);
      CallCompleted();
    }
  }

  const std::string host_;
  const HostAndType host_and_type_;
  bool started_;
  int64 host_quota_;
  int64 global_usage_;
  int64 global_unlimited_usage_;
  int64 host_usage_;
  int64 available_space_;
  QuotaStatusCode quota_status_;
  CallbackList callbacks_;
  int waiting_callbacks_;
  base::WeakPtrFactory<UsageAndQuotaDispatcherTask> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(UsageAndQuotaDispatcherTask);
};

class QuotaManager::GetUsageInfoTask : public QuotaTask {
 private:
  typedef QuotaManager::GetUsageInfoTask self_type;

 public:
  GetUsageInfoTask(
      QuotaManager* manager,
      const GetUsageInfoCallback& callback)
      : QuotaTask(manager),
        callback_(callback),
        weak_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
  }

 protected:
  virtual void Run() OVERRIDE {
    remaining_trackers_ = 3;
    // This will populate cached hosts and usage info.
    manager()->GetUsageTracker(kStorageTypeTemporary)->GetGlobalUsage(
        base::Bind(&GetUsageInfoTask::DidGetGlobalUsage,
                   weak_factory_.GetWeakPtr()));
    manager()->GetUsageTracker(kStorageTypePersistent)->GetGlobalUsage(
        base::Bind(&GetUsageInfoTask::DidGetGlobalUsage,
                   weak_factory_.GetWeakPtr()));
    manager()->GetUsageTracker(kStorageTypeSyncable)->GetGlobalUsage(
        base::Bind(&GetUsageInfoTask::DidGetGlobalUsage,
                   weak_factory_.GetWeakPtr()));
  }

  virtual void Completed() OVERRIDE {
    callback_.Run(entries_);
    DeleteSoon();
  }

  virtual void Aborted() OVERRIDE {
    callback_.Run(UsageInfoEntries());
    DeleteSoon();
  }

 private:
  void AddEntries(StorageType type, UsageTracker* tracker) {
    std::map<std::string, int64> host_usage;
    tracker->GetCachedHostsUsage(&host_usage);
    for (std::map<std::string, int64>::const_iterator iter = host_usage.begin();
         iter != host_usage.end();
         ++iter) {
      entries_.push_back(UsageInfo(iter->first, type, iter->second));
    }
    if (--remaining_trackers_ == 0)
      CallCompleted();
  }

  void DidGetGlobalUsage(StorageType type, int64, int64) {
    AddEntries(type, manager()->GetUsageTracker(type));
  }

  QuotaManager* manager() const {
    return static_cast<QuotaManager*>(observer());
  }

  GetUsageInfoCallback callback_;
  UsageInfoEntries entries_;
  base::WeakPtrFactory<GetUsageInfoTask> weak_factory_;
  int remaining_trackers_;

  DISALLOW_COPY_AND_ASSIGN(GetUsageInfoTask);
};

class QuotaManager::UsageAndQuotaDispatcherTaskForTemporary
    : public QuotaManager::UsageAndQuotaDispatcherTask {
 public:
  UsageAndQuotaDispatcherTaskForTemporary(
      QuotaManager* manager, const HostAndType& host_and_type)
      : UsageAndQuotaDispatcherTask(manager, host_and_type) {}

 protected:
  virtual void RunBody() OVERRIDE {
    manager()->GetUsageTracker(type())->GetGlobalUsage(
        NewWaitableGlobalUsageCallback());
    manager()->GetUsageTracker(type())->GetHostUsage(
        host(), NewWaitableHostUsageCallback());
    manager()->GetAvailableSpace(NewWaitableAvailableSpaceCallback());
  }

  virtual void DispatchCallbacks() OVERRIDE {
    // Allow an individual host to utilize a fraction of the total
    // pool available for temp storage.
    int64 host_quota = temporary_global_quota() / kPerHostTemporaryPortion;

    // But if total temp usage is over-budget, stop letting new data in
    // until we reclaim space.
    DCHECK_GE(global_usage(), global_unlimited_usage());
    int64 limited_global_usage = global_usage() - global_unlimited_usage();
    if (limited_global_usage > temporary_global_quota())
      host_quota = std::min(host_quota, host_usage());

    CallCallbacksAndClear(quota_status(),
                          host_usage(), host_usage(), host_quota,
                          available_space());
  }
};

class QuotaManager::UsageAndQuotaDispatcherTaskForPersistent
    : public QuotaManager::UsageAndQuotaDispatcherTask {
 public:
  UsageAndQuotaDispatcherTaskForPersistent(
      QuotaManager* manager, const HostAndType& host_and_type)
      : UsageAndQuotaDispatcherTask(manager, host_and_type) {}

 protected:
  virtual void RunBody() OVERRIDE {
    manager()->GetUsageTracker(type())->GetHostUsage(
        host(), NewWaitableHostUsageCallback());
    manager()->GetPersistentHostQuota(
        host(), NewWaitableHostQuotaCallback());
    manager()->GetAvailableSpace(NewWaitableAvailableSpaceCallback());
  }

  virtual void DispatchCallbacks() OVERRIDE {
    CallCallbacksAndClear(quota_status(),
                          host_usage(), host_usage(), host_quota(),
                          available_space());
  }
};

class QuotaManager::UsageAndQuotaDispatcherTaskForSyncable
    : public QuotaManager::UsageAndQuotaDispatcherTask {
 public:
  UsageAndQuotaDispatcherTaskForSyncable(
      QuotaManager* manager, const HostAndType& host_and_type)
      : UsageAndQuotaDispatcherTask(manager, host_and_type) {}

 protected:
  virtual void RunBody() OVERRIDE {
    manager()->GetUsageTracker(type())->GetHostUsage(
        host(), NewWaitableHostUsageCallback());
  }

  virtual void DispatchCallbacks() OVERRIDE {
    // TODO(kinuko): We should reflect the backend's actual quota instead
    // of returning a fixed default value.
    CallCallbacksAndClear(quota_status(),
                          host_usage(), host_usage(),
                          kSyncableStorageDefaultHostQuota,
                          available_space());
  }
};

class QuotaManager::UsageAndQuotaDispatcherTaskForTemporaryGlobal
    : public QuotaManager::UsageAndQuotaDispatcherTask {
 public:
  UsageAndQuotaDispatcherTaskForTemporaryGlobal(
      QuotaManager* manager, const HostAndType& host_and_type)
      : UsageAndQuotaDispatcherTask(manager, host_and_type) {}

 protected:
  virtual void RunBody() OVERRIDE {
    manager()->GetUsageTracker(type())->GetGlobalUsage(
        NewWaitableGlobalUsageCallback());
    manager()->GetAvailableSpace(NewWaitableAvailableSpaceCallback());
  }

  virtual void DispatchCallbacks() OVERRIDE {
    CallCallbacksAndClear(quota_status(),
                          global_usage(), global_unlimited_usage(),
                          temporary_global_quota(),
                          available_space());
  }

  virtual StorageType type() const OVERRIDE { return kStorageTypeTemporary; }
};

// static
QuotaManager::UsageAndQuotaDispatcherTask*
QuotaManager::UsageAndQuotaDispatcherTask::Create(
    QuotaManager* manager, bool global,
    const QuotaManager::HostAndType& host_and_type) {
  if (global)
    return new UsageAndQuotaDispatcherTaskForTemporaryGlobal(
        manager, host_and_type);
  switch (host_and_type.second) {
    case kStorageTypeTemporary:
      return new UsageAndQuotaDispatcherTaskForTemporary(
          manager, host_and_type);
    case kStorageTypePersistent:
      return new UsageAndQuotaDispatcherTaskForPersistent(
          manager, host_and_type);
    case kStorageTypeSyncable:
      return new UsageAndQuotaDispatcherTaskForSyncable(
          manager, host_and_type);
    default:
      NOTREACHED();
  }
  return NULL;
}

class QuotaManager::OriginDataDeleter : public QuotaTask {
 public:
  OriginDataDeleter(QuotaManager* manager,
                    const GURL& origin,
                    StorageType type,
                    int quota_client_mask,
                    const StatusCallback& callback)
      : QuotaTask(manager),
        origin_(origin),
        type_(type),
        quota_client_mask_(quota_client_mask),
        error_count_(0),
        remaining_clients_(-1),
        skipped_clients_(0),
        callback_(callback),
        weak_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {}

 protected:
  virtual void Run() OVERRIDE {
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

  virtual void Completed() OVERRIDE {
    if (error_count_ == 0) {
      // Only remove the entire origin if we didn't skip any client types.
      if (skipped_clients_ == 0)
        manager()->DeleteOriginFromDatabase(origin_, type_);
      callback_.Run(kQuotaStatusOk);
    } else {
      callback_.Run(kQuotaErrorInvalidModification);
    }
    DeleteSoon();
  }

  virtual void Aborted() OVERRIDE {
    callback_.Run(kQuotaErrorAbort);
    DeleteSoon();
  }

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
        weak_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {}

 protected:
  virtual void Run() OVERRIDE {
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

  virtual void Completed() OVERRIDE {
    if (error_count_ == 0) {
      callback_.Run(kQuotaStatusOk);
    } else {
      callback_.Run(kQuotaErrorInvalidModification);
    }
    DeleteSoon();
  }

  virtual void Aborted() OVERRIDE {
    callback_.Run(kQuotaErrorAbort);
    DeleteSoon();
  }

  void DidGetOriginsForHost(const std::set<GURL>& origins, StorageType type) {
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
      OriginDataDeleter* deleter =
          new OriginDataDeleter(
              manager(), *p, type_, quota_client_mask_,
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

  void DidGetModifiedSince(QuotaManager* manager,
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
        new TableCallback(base::Bind(&DumpQuotaTableHelper::AppendEntry,
                                     base::Unretained(this))));
  }

  void DidDumpQuotaTable(QuotaManager* manager,
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
  typedef QuotaDatabase::QuotaTableCallback TableCallback;

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
        new TableCallback(base::Bind(&DumpOriginInfoTableHelper::AppendEntry,
                                     base::Unretained(this))));
  }

  void DidDumpOriginInfoTable(QuotaManager* manager,
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
  typedef QuotaDatabase::OriginInfoTableCallback TableCallback;

  bool AppendEntry(const OriginInfoTableEntry& entry) {
    entries_.push_back(entry);
    return true;
  }

  OriginInfoTableEntries entries_;
};

// QuotaManager ---------------------------------------------------------------

QuotaManager::QuotaManager(bool is_incognito,
                           const FilePath& profile_path,
                           base::SingleThreadTaskRunner* io_thread,
                           base::SequencedTaskRunner* db_thread,
                           SpecialStoragePolicy* special_storage_policy)
  : is_incognito_(is_incognito),
    profile_path_(profile_path),
    proxy_(new QuotaManagerProxy(
        ALLOW_THIS_IN_INITIALIZER_LIST(this), io_thread)),
    db_disabled_(false),
    eviction_disabled_(false),
    io_thread_(io_thread),
    db_thread_(db_thread),
    temporary_quota_initialized_(false),
    temporary_quota_override_(-1),
    desired_available_space_(-1),
    special_storage_policy_(special_storage_policy),
    weak_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
    get_disk_space_fn_(&base::SysInfo::AmountOfFreeDiskSpace) {
}

void QuotaManager::GetUsageInfo(const GetUsageInfoCallback& callback) {
  LazyInitialize();
  GetUsageInfoTask* get_usage_info = new GetUsageInfoTask(this, callback);
  get_usage_info->Start();
}

void QuotaManager::GetUsageAndQuota(
    const GURL& origin, StorageType type,
    const GetUsageAndQuotaCallback& callback) {
  GetUsageAndQuotaInternal(
      origin, type, false /* global */,
      base::Bind(&CallGetUsageAndQuotaCallback, callback,
                 IsStorageUnlimited(origin, type), IsInstalledApp(origin)));
}

void QuotaManager::NotifyStorageAccessed(
    QuotaClient::ID client_id,
    const GURL& origin, StorageType type) {
  NotifyStorageAccessedInternal(client_id, origin, type, base::Time::Now());
}

void QuotaManager::NotifyStorageModified(
    QuotaClient::ID client_id,
    const GURL& origin, StorageType type, int64 delta) {
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

void QuotaManager::DeleteOriginData(
    const GURL& origin, StorageType type, int quota_client_mask,
    const StatusCallback& callback) {
  LazyInitialize();

  if (origin.is_empty() || clients_.empty()) {
    callback.Run(kQuotaStatusOk);
    return;
  }

  OriginDataDeleter* deleter =
      new OriginDataDeleter(this, origin, type, quota_client_mask, callback);
  deleter->Start();
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

void QuotaManager::GetAvailableSpace(const AvailableSpaceCallback& callback) {
  if (is_incognito_) {
    callback.Run(kQuotaStatusOk, kIncognitoDefaultTemporaryQuota);
    return;
  }

  PostTaskAndReplyWithResult(
      db_thread_,
      FROM_HERE,
      base::Bind(get_disk_space_fn_, profile_path_),
      base::Bind(&QuotaManager::DidGetAvailableSpace,
                 weak_factory_.GetWeakPtr(),
                 callback));
}

void QuotaManager::GetTemporaryGlobalQuota(const QuotaCallback& callback) {
  if (temporary_quota_override_ > 0) {
    callback.Run(kQuotaStatusOk, kStorageTypeTemporary,
                 temporary_quota_override_);
    return;
  }
  GetUsageAndQuotaInternal(
      GURL(), kStorageTypeTemporary, true /* global */,
      base::Bind(&CallQuotaCallback, callback, kStorageTypeTemporary));
}

void QuotaManager::SetTemporaryGlobalOverrideQuota(
    int64 new_quota, const QuotaCallback& callback) {
  LazyInitialize();

  if (new_quota < 0) {
    if (!callback.is_null())
      callback.Run(kQuotaErrorInvalidModification,
                   kStorageTypeTemporary, -1);
    return;
  }

  if (db_disabled_) {
    if (callback.is_null())
      callback.Run(kQuotaErrorInvalidAccess,
                   kStorageTypeTemporary, -1);
    return;
  }

  int64* new_quota_ptr = new int64(new_quota);
  PostTaskAndReplyWithResultForDBThread(
      FROM_HERE,
      base::Bind(&SetTemporaryGlobalOverrideQuotaOnDBThread,
                 base::Unretained(new_quota_ptr)),
      base::Bind(&QuotaManager::DidSetTemporaryGlobalOverrideQuota,
                 weak_factory_.GetWeakPtr(),
                 callback,
                 base::Owned(new_quota_ptr)));
}

void QuotaManager::GetPersistentHostQuota(const std::string& host,
                                          const HostQuotaCallback& callback) {
  LazyInitialize();
  if (host.empty()) {
    // This could happen if we are called on file:///.
    // TODO(kinuko) We may want to respect --allow-file-access-from-files
    // command line switch.
    callback.Run(kQuotaStatusOk, host, kStorageTypePersistent, 0);
    return;
  }

  int64* quota_ptr = new int64(0);
  PostTaskAndReplyWithResultForDBThread(
      FROM_HERE,
      base::Bind(&GetPersistentHostQuotaOnDBThread,
                 host,
                 base::Unretained(quota_ptr)),
      base::Bind(&QuotaManager::DidGetPersistentHostQuota,
                 weak_factory_.GetWeakPtr(),
                 callback,
                 host,
                 base::Owned(quota_ptr)));
}

void QuotaManager::SetPersistentHostQuota(const std::string& host,
                                          int64 new_quota,
                                          const HostQuotaCallback& callback) {
  LazyInitialize();
  if (host.empty()) {
    // This could happen if we are called on file:///.
    callback.Run(kQuotaErrorNotSupported, host, kStorageTypePersistent, 0);
    return;
  }
  if (new_quota < 0) {
    callback.Run(kQuotaErrorInvalidModification,
                 host, kStorageTypePersistent, -1);
    return;
  }

  if (db_disabled_) {
    callback.Run(kQuotaErrorInvalidAccess,
                 host, kStorageTypePersistent, -1);
    return;
  }

  int64* new_quota_ptr = new int64(new_quota);
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
  GetUsageTracker(type)->GetGlobalUsage(callback);
}

void QuotaManager::GetHostUsage(const std::string& host,
                                StorageType type,
                                const HostUsageCallback& callback) {
  LazyInitialize();
  GetUsageTracker(type)->GetHostUsage(host, callback);
}

void QuotaManager::GetStatistics(
    std::map<std::string, std::string>* statistics) {
  DCHECK(statistics);
  if (temporary_storage_evictor_.get()) {
    std::map<std::string, int64> stats;
    temporary_storage_evictor_->GetStatistics(&stats);
    for (std::map<std::string, int64>::iterator p = stats.begin();
         p != stats.end();
         ++p)
      (*statistics)[p->first] = base::Int64ToString(p->second);
  }
}

bool QuotaManager::IsStorageUnlimited(const GURL& origin,
                                      StorageType type) const {
  // For syncable storage we should always enforce quota (since the
  // quota must be capped by the server limit).
  if (type == kStorageTypeSyncable)
    return false;
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
      temporary_usage_tracker_.reset(
          new UsageTracker(clients_, kStorageTypeTemporary,
                           special_storage_policy_));
      return true;
    case kStorageTypePersistent:
      persistent_usage_tracker_.reset(
          new UsageTracker(clients_, kStorageTypePersistent,
                           special_storage_policy_));
      return true;
    default:
      NOTREACHED();
  }
  return true;
}

QuotaManager::~QuotaManager() {
  proxy_->manager_ = NULL;
  std::for_each(clients_.begin(), clients_.end(),
                std::mem_fun(&QuotaClient::OnQuotaManagerDestroyed));
  if (database_.get())
    db_thread_->DeleteSoon(FROM_HERE, database_.release());
}

QuotaManager::EvictionContext::EvictionContext()
    : evicted_type(kStorageTypeUnknown) {
}

QuotaManager::EvictionContext::~EvictionContext() {
}

void QuotaManager::LazyInitialize() {
  DCHECK(io_thread_->BelongsToCurrentThread());
  if (database_.get()) {
    // Initialization seems to be done already.
    return;
  }

  // Use an empty path to open an in-memory only databse for incognito.
  database_.reset(new QuotaDatabase(is_incognito_ ? FilePath() :
      profile_path_.AppendASCII(kDatabaseName)));

  temporary_usage_tracker_.reset(
      new UsageTracker(clients_, kStorageTypeTemporary,
                       special_storage_policy_));
  persistent_usage_tracker_.reset(
      new UsageTracker(clients_, kStorageTypePersistent,
                       special_storage_policy_));
  syncable_usage_tracker_.reset(
      new UsageTracker(clients_, kStorageTypeSyncable,
                       special_storage_policy_));

  int64* temporary_quota_override = new int64(-1);
  int64* desired_available_space = new int64(-1);
  PostTaskAndReplyWithResultForDBThread(
      FROM_HERE,
      base::Bind(&InitializeOnDBThread,
                 base::Unretained(temporary_quota_override),
                 base::Unretained(desired_available_space)),
      base::Bind(&QuotaManager::DidInitialize,
                 weak_factory_.GetWeakPtr(),
                 base::Owned(temporary_quota_override),
                 base::Owned(desired_available_space)));
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
    default:
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
  if (type == kStorageTypeTemporary && !lru_origin_callback_.is_null()) {
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

void QuotaManager::NotifyStorageModifiedInternal(
    QuotaClient::ID client_id,
    const GURL& origin,
    StorageType type,
    int64 delta,
    base::Time modified_time) {
  LazyInitialize();
  GetUsageTracker(type)->UpdateUsageCache(client_id, origin, delta);

  PostTaskAndReplyWithResultForDBThread(
      FROM_HERE,
      base::Bind(&UpdateModifiedTimeOnDBThread, origin, type, modified_time),
      base::Bind(&QuotaManager::DidDatabaseWork,
                 weak_factory_.GetWeakPtr()));
}

void QuotaManager::GetUsageAndQuotaInternal(
    const GURL& origin, StorageType type, bool global,
    const UsageAndQuotaDispatcherCallback& callback) {
  LazyInitialize();

  StorageType requested_type = type;
  if (type == kStorageTypeUnknown) {
    // Quota only supports temporary/persistent types.
    callback.Run(kQuotaErrorNotSupported, QuotaAndUsage());
    return;
  }

  // Special internal type for querying global usage and quota.
  const int kStorageTypeTemporaryGlobal = kStorageTypeTemporary + 100;
  if (global) {
    DCHECK_EQ(kStorageTypeTemporary, type);
    type = static_cast<StorageType>(kStorageTypeTemporaryGlobal);
  }

  std::string host = net::GetHostOrSpecFromURL(origin);
  HostAndType host_and_type = std::make_pair(host, type);
  UsageAndQuotaDispatcherTaskMap::iterator found =
      usage_and_quota_dispatchers_.find(host_and_type);
  if (found == usage_and_quota_dispatchers_.end()) {
    UsageAndQuotaDispatcherTask* dispatcher =
        UsageAndQuotaDispatcherTask::Create(this, global, host_and_type);
    found = usage_and_quota_dispatchers_.insert(
        std::make_pair(host_and_type, dispatcher)).first;
  }
  // Start the dispatcher if it is the first one and temporary_quota_override
  // is already initialized iff the requested type is temporary.
  // (The first dispatcher task for temporary will be kicked in
  // DidInitialize if temporary_quota_initialized_ is false here.)
  if (found->second->AddCallback(callback) &&
      (requested_type != kStorageTypeTemporary ||
       temporary_quota_initialized_)) {
    found->second->Start();
  }
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
  temporary_storage_evictor_.reset(new QuotaTemporaryStorageEvictor(
      this, kEvictionIntervalInMilliSeconds));
  if (desired_available_space_ >= 0)
    temporary_storage_evictor_->set_min_available_disk_space_to_start_eviction(
        desired_available_space_);
  temporary_storage_evictor_->Start();
}

void QuotaManager::DeleteOriginFromDatabase(
    const GURL& origin, StorageType type) {
  LazyInitialize();
  if (db_disabled_)
    return;

  PostTaskAndReplyWithResultForDBThread(
      FROM_HERE,
      base::Bind(&DeleteOriginInfoOnDBThread, origin, type),
      base::Bind(&QuotaManager::DidDatabaseWork,
                 weak_factory_.GetWeakPtr()));
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

void QuotaManager::ReportHistogram() {
  GetGlobalUsage(kStorageTypeTemporary,
                 base::Bind(
                     &QuotaManager::DidGetTemporaryGlobalUsageForHistogram,
                     weak_factory_.GetWeakPtr()));
  GetGlobalUsage(kStorageTypePersistent,
                 base::Bind(
                     &QuotaManager::DidGetPersistentGlobalUsageForHistogram,
                     weak_factory_.GetWeakPtr()));
}

void QuotaManager::DidGetTemporaryGlobalUsageForHistogram(
    StorageType type,
    int64 usage,
    int64 unlimited_usage) {
  UMA_HISTOGRAM_MBYTES("Quota.GlobalUsageOfTemporaryStorage", usage);

  std::set<GURL> origins;
  GetCachedOrigins(type, &origins);

  size_t num_origins = origins.size();
  size_t protected_origins = 0;
  size_t unlimited_origins = 0;
  CountOriginType(origins, special_storage_policy_,
                  &protected_origins, &unlimited_origins);

  UMA_HISTOGRAM_COUNTS("Quota.NumberOfTemporaryStorageOrigins",
                       num_origins);
  UMA_HISTOGRAM_COUNTS("Quota.NumberOfProtectedTemporaryStorageOrigins",
                       protected_origins);
  UMA_HISTOGRAM_COUNTS("Quota.NumberOfUnlimitedTemporaryStorageOrigins",
                       unlimited_origins);
}

void QuotaManager::DidGetPersistentGlobalUsageForHistogram(
    StorageType type,
    int64 usage,
    int64 unlimited_usage) {
  UMA_HISTOGRAM_MBYTES("Quota.GlobalUsageOfPersistentStorage", usage);

  std::set<GURL> origins;
  GetCachedOrigins(type, &origins);

  size_t num_origins = origins.size();
  size_t protected_origins = 0;
  size_t unlimited_origins = 0;
  CountOriginType(origins, special_storage_policy_,
                  &protected_origins, &unlimited_origins);

  UMA_HISTOGRAM_COUNTS("Quota.NumberOfPersistentStorageOrigins",
                       num_origins);
  UMA_HISTOGRAM_COUNTS("Quota.NumberOfProtectedPersistentStorageOrigins",
                       protected_origins);
  UMA_HISTOGRAM_COUNTS("Quota.NumberOfUnlimitedPersistentStorageOrigins",
                       unlimited_origins);
}

void QuotaManager::GetLRUOrigin(
    StorageType type,
    const GetLRUOriginCallback& callback) {
  LazyInitialize();
  // This must not be called while there's an in-flight task.
  DCHECK(lru_origin_callback_.is_null());
  lru_origin_callback_ = callback;
  if (db_disabled_) {
    lru_origin_callback_.Run(GURL());
    lru_origin_callback_.Reset();
    return;
  }

  std::set<GURL>* exceptions = new std::set<GURL>;
  for (std::map<GURL, int>::const_iterator p = origins_in_use_.begin();
       p != origins_in_use_.end();
       ++p) {
    if (p->second > 0)
      exceptions->insert(p->first);
  }
  for (std::map<GURL, int>::const_iterator p = origins_in_error_.begin();
       p != origins_in_error_.end();
       ++p) {
    if (p->second > QuotaManager::kThresholdOfErrorsToBeBlacklisted)
      exceptions->insert(p->first);
  }

  GURL* url = new GURL;
  PostTaskAndReplyWithResultForDBThread(
      FROM_HERE,
      base::Bind(&GetLRUOriginOnDBThread,
                 type,
                 base::Owned(exceptions),
                 special_storage_policy_,
                 base::Unretained(url)),
      base::Bind(&QuotaManager::DidGetLRUOrigin,
                 weak_factory_.GetWeakPtr(),
                 base::Owned(url)));
}

void QuotaManager::EvictOriginData(
    const GURL& origin,
    StorageType type,
    const EvictOriginDataCallback& callback) {
  DCHECK(io_thread_->BelongsToCurrentThread());
  DCHECK_EQ(type, kStorageTypeTemporary);

  eviction_context_.evicted_origin = origin;
  eviction_context_.evicted_type = type;
  eviction_context_.evict_origin_data_callback = callback;

  DeleteOriginData(origin, type, QuotaClient::kAllClientsMask,
      base::Bind(&QuotaManager::DidOriginDataEvicted,
                 weak_factory_.GetWeakPtr()));
}

void QuotaManager::GetUsageAndQuotaForEviction(
    const GetUsageAndQuotaForEvictionCallback& callback) {
  DCHECK(io_thread_->BelongsToCurrentThread());
  GetUsageAndQuotaInternal(
      GURL(), kStorageTypeTemporary, true /* global */, callback);
}

void QuotaManager::DidSetTemporaryGlobalOverrideQuota(
    const QuotaCallback& callback,
    const int64* new_quota,
    bool success) {
  QuotaStatusCode status = kQuotaErrorInvalidAccess;
  DidDatabaseWork(success);
  if (success) {
    temporary_quota_override_ = *new_quota;
    status = kQuotaStatusOk;
  }

  if (callback.is_null())
    return;

  callback.Run(status, kStorageTypeTemporary, *new_quota);
}

void QuotaManager::DidGetPersistentHostQuota(const HostQuotaCallback& callback,
                                             const std::string& host,
                                             const int64* quota,
                                             bool success) {
  DidDatabaseWork(success);
  callback.Run(kQuotaStatusOk, host, kStorageTypePersistent, *quota);
}

void QuotaManager::DidSetPersistentHostQuota(const std::string& host,
                                             const HostQuotaCallback& callback,
                                             const int64* new_quota,
                                             bool success) {
  DidDatabaseWork(success);
  callback.Run(success ? kQuotaStatusOk : kQuotaErrorInvalidAccess,
               host, kStorageTypePersistent, *new_quota);
}

void QuotaManager::DidInitialize(int64* temporary_quota_override,
                                 int64* desired_available_space,
                                 bool success) {
  temporary_quota_override_ = *temporary_quota_override;
  desired_available_space_ = *desired_available_space;
  temporary_quota_initialized_ = true;
  DidDatabaseWork(success);

  histogram_timer_.Start(FROM_HERE,
                         base::TimeDelta::FromMilliseconds(
                             kReportHistogramInterval),
                         this, &QuotaManager::ReportHistogram);

  DCHECK(temporary_quota_initialized_);

  // Kick the tasks that have been waiting for the
  // temporary_quota_initialized_ to be initialized (if there're any).
  for (UsageAndQuotaDispatcherTaskMap::iterator iter =
           usage_and_quota_dispatchers_.begin();
       iter != usage_and_quota_dispatchers_.end(); ++iter) {
    if (iter->second->IsStartable())
      iter->second->Start();
  }

  // Kick the first GetTemporaryGlobalQuota. This internally fetches (and
  // caches) the usage of all origins and checks the available disk space.
  GetTemporaryGlobalQuota(
      base::Bind(&QuotaManager::DidGetInitialTemporaryGlobalQuota,
                 weak_factory_.GetWeakPtr()));
}

void QuotaManager::DidGetLRUOrigin(const GURL* origin,
                                   bool success) {
  DidDatabaseWork(success);
  // Make sure the returned origin is (still) not in the origin_in_use_ set
  // and has not been accessed since we posted the task.
  if (origins_in_use_.find(*origin) != origins_in_use_.end() ||
      access_notified_origins_.find(*origin) != access_notified_origins_.end())
    lru_origin_callback_.Run(GURL());
  else
    lru_origin_callback_.Run(*origin);
  access_notified_origins_.clear();
  lru_origin_callback_.Reset();
}

void QuotaManager::DidGetInitialTemporaryGlobalQuota(
    QuotaStatusCode status, StorageType type, int64 quota_unused) {
  DCHECK_EQ(type, kStorageTypeTemporary);

  if (eviction_disabled_)
    return;

  std::set<GURL>* origins = new std::set<GURL>;
  temporary_usage_tracker_->GetCachedOrigins(origins);
  // This will call the StartEviction() when initial origin registration
  // is completed.
  PostTaskAndReplyWithResultForDBThread(
      FROM_HERE,
      base::Bind(&InitializeTemporaryOriginsInfoOnDBThread,
                 base::Owned(origins)),
      base::Bind(&QuotaManager::DidInitializeTemporaryOriginsInfo,
                 weak_factory_.GetWeakPtr()));
}

void QuotaManager::DidInitializeTemporaryOriginsInfo(bool success) {
  DidDatabaseWork(success);
  if (success)
    StartEviction();
}

void QuotaManager::DidGetAvailableSpace(const AvailableSpaceCallback& callback,
                                        int64 space) {
  callback.Run(kQuotaStatusOk, space);
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
    const base::Callback<bool(QuotaDatabase*)>& task,
    const base::Callback<void(bool)>& reply) {
  // Deleting manager will post another task to DB thread to delete
  // |database_|, therefore we can be sure that database_ is alive when this
  // task runs.
  base::PostTaskAndReplyWithResult(
      db_thread_,
      from_here,
      base::Bind(task, base::Unretained(database_.get())),
      reply);
}

// QuotaManagerProxy ----------------------------------------------------------

void QuotaManagerProxy::RegisterClient(QuotaClient* client) {
  if (!io_thread_->BelongsToCurrentThread() &&
      io_thread_->PostTask(
          FROM_HERE,
          base::Bind(&QuotaManagerProxy::RegisterClient, this, client))) {
    return;
  }

  if (manager_)
    manager_->RegisterClient(client);
  else
    client->OnQuotaManagerDestroyed();
}

void QuotaManagerProxy::NotifyStorageAccessed(
    QuotaClient::ID client_id,
    const GURL& origin,
    StorageType type) {
  if (!io_thread_->BelongsToCurrentThread()) {
    io_thread_->PostTask(
        FROM_HERE,
        base::Bind(&QuotaManagerProxy::NotifyStorageAccessed, this, client_id,
                   origin, type));
    return;
  }

  if (manager_)
    manager_->NotifyStorageAccessed(client_id, origin, type);
}

void QuotaManagerProxy::NotifyStorageModified(
    QuotaClient::ID client_id,
    const GURL& origin,
    StorageType type,
    int64 delta) {
  if (!io_thread_->BelongsToCurrentThread()) {
    io_thread_->PostTask(
        FROM_HERE,
        base::Bind(&QuotaManagerProxy::NotifyStorageModified, this, client_id,
                   origin, type, delta));
    return;
  }

  if (manager_)
    manager_->NotifyStorageModified(client_id, origin, type, delta);
}

void QuotaManagerProxy::NotifyOriginInUse(
    const GURL& origin) {
  if (!io_thread_->BelongsToCurrentThread()) {
    io_thread_->PostTask(
        FROM_HERE,
        base::Bind(&QuotaManagerProxy::NotifyOriginInUse, this, origin));
    return;
  }

  if (manager_)
    manager_->NotifyOriginInUse(origin);
}

void QuotaManagerProxy::NotifyOriginNoLongerInUse(
    const GURL& origin) {
  if (!io_thread_->BelongsToCurrentThread()) {
    io_thread_->PostTask(
        FROM_HERE,
        base::Bind(&QuotaManagerProxy::NotifyOriginNoLongerInUse, this,
                   origin));
    return;
  }
  if (manager_)
    manager_->NotifyOriginNoLongerInUse(origin);
}

QuotaManager* QuotaManagerProxy::quota_manager() const {
  DCHECK(!io_thread_ || io_thread_->BelongsToCurrentThread());
  return manager_;
}

QuotaManagerProxy::QuotaManagerProxy(
    QuotaManager* manager, base::SingleThreadTaskRunner* io_thread)
    : manager_(manager), io_thread_(io_thread) {
}

QuotaManagerProxy::~QuotaManagerProxy() {
}

}  // namespace quota
