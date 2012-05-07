// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/quota/quota_manager.h"

#include <algorithm>
#include <deque>
#include <set>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/metrics/histogram.h"
#include "base/string_number_conversions.h"
#include "base/sys_info.h"
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

}  // anonymous namespace

const int QuotaManager::kPerHostTemporaryPortion = 5;  // 20%

const char QuotaManager::kDatabaseName[] = "QuotaManager";

const int QuotaManager::kThresholdOfErrorsToBeBlacklisted = 3;

const int QuotaManager::kEvictionIntervalInMilliSeconds =
    30 * kMinutesInMilliSeconds;

// Callback translators.
void CallGetUsageAndQuotaCallback(
    const QuotaManager::GetUsageAndQuotaCallback& callback,
    bool unlimited,
    QuotaStatusCode status,
    const QuotaAndUsage& quota_and_usage) {
  int64 usage =
      unlimited ? quota_and_usage.unlimited_usage : quota_and_usage.usage;
  int64 quota = unlimited ? kint64max : quota_and_usage.quota;
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
    remaining_trackers_ = 2;
    // This will populate cached hosts and usage info.
    manager()->GetUsageTracker(kStorageTypeTemporary)->GetGlobalUsage(
        base::Bind(&GetUsageInfoTask::DidGetGlobalUsage,
                   weak_factory_.GetWeakPtr()));
    manager()->GetUsageTracker(kStorageTypePersistent)->GetGlobalUsage(
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
    manager()->temporary_usage_tracker_->GetGlobalUsage(
        NewWaitableGlobalUsageCallback());
    manager()->temporary_usage_tracker_->GetHostUsage(
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
    manager()->persistent_usage_tracker_->GetHostUsage(
        host(), NewWaitableHostUsageCallback());
    manager()->GetPersistentHostQuota(
        host(), NewWaitableHostQuotaCallback());
  }

  virtual void DispatchCallbacks() OVERRIDE {
    CallCallbacksAndClear(quota_status(),
                          host_usage(), host_usage(), host_quota(),
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
    manager()->temporary_usage_tracker_->GetGlobalUsage(
        NewWaitableGlobalUsageCallback());
    manager()->GetAvailableSpace(NewWaitableAvailableSpaceCallback());
  }

  virtual void DispatchCallbacks() OVERRIDE {
    CallCallbacksAndClear(quota_status(),
                          global_usage(), global_unlimited_usage(),
                          temporary_global_quota(),
                          available_space());
  }

  virtual StorageType type() const { return kStorageTypeTemporary; }
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

class QuotaManager::DatabaseTaskBase : public QuotaThreadTask {
 public:
  explicit DatabaseTaskBase(QuotaManager* manager)
      : QuotaThreadTask(manager, manager->db_thread_),
        manager_(manager),
        database_(manager->database_.get()),
        db_disabled_(false) {
    DCHECK(database_);
  }

 protected:
  virtual ~DatabaseTaskBase() {}

  virtual void DatabaseTaskCompleted() = 0;

  // QuotaThreadTask:
  virtual void Completed() OVERRIDE {
    manager_->db_disabled_ = db_disabled_;
    DatabaseTaskCompleted();
  }

  bool db_disabled() const { return db_disabled_; }
  void set_db_disabled(bool db_disabled) {
    db_disabled_ = db_disabled;
  }

  QuotaManager* manager() const { return manager_; }
  QuotaDatabase* database() const { return database_; }

 private:
  QuotaManager* manager_;
  QuotaDatabase* database_;
  bool db_disabled_;
};

class QuotaManager::InitializeTask : public QuotaManager::DatabaseTaskBase {
 public:
  InitializeTask(QuotaManager* manager)
      : DatabaseTaskBase(manager),
        temporary_quota_override_(-1),
        desired_available_space_(-1) {
  }

 protected:
  virtual ~InitializeTask() {}

  // QuotaThreadTask:
  virtual void RunOnTargetThread() OVERRIDE {
    // See if we have overriding temporary quota configuration.
    database()->GetQuotaConfigValue(QuotaDatabase::kTemporaryQuotaOverrideKey,
                                    &temporary_quota_override_);
    database()->GetQuotaConfigValue(QuotaDatabase::kDesiredAvailableSpaceKey,
                                    &desired_available_space_);
  }

  // DatabaseTaskBase:
  virtual void DatabaseTaskCompleted() OVERRIDE {
    manager()->temporary_quota_override_ = temporary_quota_override_;
    manager()->desired_available_space_ = desired_available_space_;
    manager()->temporary_quota_initialized_ = true;
    manager()->DidRunInitializeTask();
  }

 private:
  int64 temporary_quota_override_;
  int64 desired_available_space_;
};

class QuotaManager::UpdateTemporaryQuotaOverrideTask
    : public QuotaManager::DatabaseTaskBase {
 public:
  UpdateTemporaryQuotaOverrideTask(
      QuotaManager* manager,
      int64 new_quota,
      const QuotaCallback& callback)
      : DatabaseTaskBase(manager),
        new_quota_(new_quota),
        callback_(callback) {}

 protected:
  virtual ~UpdateTemporaryQuotaOverrideTask() {}

  // QuotaThreadTask:
  virtual void RunOnTargetThread() OVERRIDE {
    if (!database()->SetQuotaConfigValue(
            QuotaDatabase::kTemporaryQuotaOverrideKey, new_quota_)) {
      set_db_disabled(true);
      new_quota_ = -1;
      return;
    }
  }

  // DatabaseTaskBase:
  virtual void DatabaseTaskCompleted() OVERRIDE {
    if (!db_disabled()) {
      manager()->temporary_quota_override_ = new_quota_;
      CallCallback(kQuotaStatusOk, kStorageTypeTemporary, new_quota_);
    } else {
      CallCallback(kQuotaErrorInvalidAccess, kStorageTypeTemporary, new_quota_);
    }
  }

 private:
  void CallCallback(QuotaStatusCode status, StorageType type, int64 quota) {
    if (!callback_.is_null()) {
      callback_.Run(status, type, quota);
      callback_.Reset();
    }
  }

  int64 new_quota_;
  QuotaCallback callback_;
};

class QuotaManager::GetPersistentHostQuotaTask
    : public QuotaManager::DatabaseTaskBase {
 public:
  GetPersistentHostQuotaTask(
      QuotaManager* manager,
      const std::string& host,
      const HostQuotaCallback& callback)
      : DatabaseTaskBase(manager),
        host_(host),
        quota_(-1),
        callback_(callback) {
  }

 protected:
  virtual ~GetPersistentHostQuotaTask() {}

  // QuotaThreadTask:
  virtual void RunOnTargetThread() OVERRIDE {
    if (!database()->GetHostQuota(host_, kStorageTypePersistent, &quota_))
      quota_ = 0;
  }

  // DatabaseTaskBase:
  virtual void DatabaseTaskCompleted() OVERRIDE {
    callback_.Run(kQuotaStatusOk,
                  host_, kStorageTypePersistent, quota_);
  }

 private:
  std::string host_;
  int64 quota_;
  HostQuotaCallback callback_;
};

class QuotaManager::UpdatePersistentHostQuotaTask
    : public QuotaManager::DatabaseTaskBase {
 public:
  UpdatePersistentHostQuotaTask(
      QuotaManager* manager,
      const std::string& host,
      int64 new_quota,
      const HostQuotaCallback& callback)
      : DatabaseTaskBase(manager),
        host_(host),
        new_quota_(new_quota),
        callback_(callback) {
    DCHECK_GE(new_quota_, 0);
  }

 protected:
  virtual ~UpdatePersistentHostQuotaTask() {}

  // QuotaThreadTask:
  virtual void RunOnTargetThread() OVERRIDE {
    if (!database()->SetHostQuota(host_, kStorageTypePersistent, new_quota_)) {
      set_db_disabled(true);
      new_quota_ = 0;
    }
  }

  virtual void Aborted() OVERRIDE {
    callback_.Reset();
  }

  // DatabaseTaskBase:
  virtual void DatabaseTaskCompleted() OVERRIDE {
    callback_.Run(db_disabled() ? kQuotaErrorInvalidAccess : kQuotaStatusOk,
                  host_, kStorageTypePersistent, new_quota_);
  }

 private:
  std::string host_;
  int64 new_quota_;
  HostQuotaCallback callback_;
};

class QuotaManager::GetLRUOriginTask
    : public QuotaManager::DatabaseTaskBase {
 public:
  GetLRUOriginTask(
      QuotaManager* manager,
      StorageType type,
      const std::map<GURL, int>& origins_in_use,
      const std::map<GURL, int>& origins_in_error,
      const GetLRUOriginCallback& callback)
      : DatabaseTaskBase(manager),
        type_(type),
        callback_(callback),
        special_storage_policy_(manager->special_storage_policy_) {
    for (std::map<GURL, int>::const_iterator p = origins_in_use.begin();
         p != origins_in_use.end();
         ++p) {
      if (p->second > 0)
        exceptions_.insert(p->first);
    }
    for (std::map<GURL, int>::const_iterator p = origins_in_error.begin();
         p != origins_in_error.end();
         ++p) {
      if (p->second > QuotaManager::kThresholdOfErrorsToBeBlacklisted)
        exceptions_.insert(p->first);
    }
  }

 protected:
  virtual ~GetLRUOriginTask() {}

  // QuotaThreadTask:
  virtual void RunOnTargetThread() OVERRIDE {
    database()->GetLRUOrigin(
        type_, exceptions_, special_storage_policy_, &url_);
  }

  virtual void Aborted() OVERRIDE {
    callback_.Reset();
  }

  // DatabaseTaskBase:
  virtual void DatabaseTaskCompleted() OVERRIDE {
    callback_.Run(url_);
  }

 private:
  StorageType type_;
  std::set<GURL> exceptions_;
  GetLRUOriginCallback callback_;
  scoped_refptr<SpecialStoragePolicy> special_storage_policy_;
  GURL url_;
};

class QuotaManager::DeleteOriginInfo
    : public QuotaManager::DatabaseTaskBase {
 public:
  DeleteOriginInfo(
      QuotaManager* manager,
      const GURL& origin,
      StorageType type)
      : DatabaseTaskBase(manager),
        origin_(origin),
        type_(type) {}

 protected:
  virtual ~DeleteOriginInfo() {}

  // QuotaThreadTask:
  virtual void RunOnTargetThread() OVERRIDE {
    if (!database()->DeleteOriginInfo(origin_, type_)) {
      set_db_disabled(true);
    }
  }

  // DatabaseTaskBase:
  virtual void DatabaseTaskCompleted() OVERRIDE {}

 private:
  GURL origin_;
  StorageType type_;
};

class QuotaManager::InitializeTemporaryOriginsInfoTask
    : public QuotaManager::DatabaseTaskBase {
 public:
  InitializeTemporaryOriginsInfoTask(
      QuotaManager* manager,
      UsageTracker* temporary_usage_tracker)
      : DatabaseTaskBase(manager),
        has_registered_origins_(false) {
    DCHECK(temporary_usage_tracker);
    temporary_usage_tracker->GetCachedOrigins(&origins_);
  }

 protected:
  virtual ~InitializeTemporaryOriginsInfoTask() {}

  // QuotaThreadTask:
  virtual void RunOnTargetThread() OVERRIDE {
    if (!database()->IsOriginDatabaseBootstrapped()) {
      // Register existing origins with 0 last time access.
      if (!database()->RegisterInitialOriginInfo(
              origins_, kStorageTypeTemporary)) {
        set_db_disabled(true);
      } else {
        has_registered_origins_ = true;
        database()->SetOriginDatabaseBootstrapped(true);
      }
    }
  }

  // DatabaseTaskBase:
  virtual void DatabaseTaskCompleted() OVERRIDE {
    if (has_registered_origins_)
      manager()->StartEviction();
  }

 private:
  std::set<GURL> origins_;
  bool has_registered_origins_;
};

class QuotaManager::AvailableSpaceQueryTask : public QuotaThreadTask {
 public:
  AvailableSpaceQueryTask(
      QuotaManager* manager,
      const AvailableSpaceCallback& callback)
      : QuotaThreadTask(manager, manager->db_thread_),
        profile_path_(manager->profile_path_),
        space_(-1),
        callback_(callback) {
  }

 protected:
  virtual ~AvailableSpaceQueryTask() {}

  // QuotaThreadTask:
  virtual void RunOnTargetThread() OVERRIDE {
    space_ = base::SysInfo::AmountOfFreeDiskSpace(profile_path_);
  }

  virtual void Aborted() OVERRIDE {
    callback_.Reset();
  }

  virtual void Completed() OVERRIDE {
    callback_.Run(kQuotaStatusOk, space_);
  }

 private:
  FilePath profile_path_;
  int64 space_;
  AvailableSpaceCallback callback_;
};

class QuotaManager::UpdateAccessTimeTask
    : public QuotaManager::DatabaseTaskBase {
 public:
  UpdateAccessTimeTask(
      QuotaManager* manager,
      const GURL& origin,
      StorageType type,
      base::Time accessed_time)
      : DatabaseTaskBase(manager),
        origin_(origin),
        type_(type),
        accessed_time_(accessed_time) {}

 protected:
  virtual ~UpdateAccessTimeTask() {}

  // QuotaThreadTask:
  virtual void RunOnTargetThread() OVERRIDE {
    if (!database()->SetOriginLastAccessTime(origin_, type_, accessed_time_)) {
      set_db_disabled(true);
    }
  }

  // DatabaseTaskBase:
  virtual void DatabaseTaskCompleted() OVERRIDE {}

 private:
  GURL origin_;
  StorageType type_;
  base::Time accessed_time_;
};

class QuotaManager::UpdateModifiedTimeTask
    : public QuotaManager::DatabaseTaskBase {
 public:
  UpdateModifiedTimeTask(
      QuotaManager* manager,
      const GURL& origin,
      StorageType type,
      base::Time modified_time)
      : DatabaseTaskBase(manager),
        origin_(origin),
        type_(type),
        modified_time_(modified_time) {}

 protected:
  virtual ~UpdateModifiedTimeTask() {}

  // QuotaThreadTask:
  virtual void RunOnTargetThread() OVERRIDE {
    if (!database()->SetOriginLastModifiedTime(
            origin_, type_, modified_time_)) {
      set_db_disabled(true);
    }
  }

  // DatabaseTaskBase:
  virtual void DatabaseTaskCompleted() OVERRIDE {}

 private:
  GURL origin_;
  StorageType type_;
  base::Time modified_time_;
};

class QuotaManager::GetModifiedSinceTask
    : public QuotaManager::DatabaseTaskBase {
 public:
  GetModifiedSinceTask(
      QuotaManager* manager,
      StorageType type,
      base::Time modified_since,
      GetOriginsCallback callback)
      : DatabaseTaskBase(manager),
        type_(type),
        modified_since_(modified_since),
        callback_(callback) {}

 protected:
  virtual ~GetModifiedSinceTask() {}

  // QuotaThreadTask:
  virtual void RunOnTargetThread() OVERRIDE {
    if (!database()->GetOriginsModifiedSince(
            type_, &origins_, modified_since_)) {
      set_db_disabled(true);
    }
  }

  virtual void Aborted() OVERRIDE {
    callback_.Run(std::set<GURL>(), type_);
  }

  // DatabaseTaskBase:
  virtual void DatabaseTaskCompleted() OVERRIDE {
    callback_.Run(origins_, type_);
  }

 private:
  StorageType type_;
  base::Time modified_since_;
  std::set<GURL> origins_;
  GetOriginsCallback callback_;
};

class QuotaManager::DumpQuotaTableTask
    : public QuotaManager::DatabaseTaskBase {
 private:
  typedef QuotaManager::DumpQuotaTableTask self_type;
  typedef QuotaManager::DumpQuotaTableCallback Callback;
  typedef QuotaManager::QuotaTableEntry TableEntry;
  typedef QuotaManager::QuotaTableEntries TableEntries;
  typedef QuotaDatabase::QuotaTableCallback TableCallback;

 public:
  DumpQuotaTableTask(
      QuotaManager* manager,
      const Callback& callback)
      : DatabaseTaskBase(manager),
        callback_(callback) {
  }

 protected:
  virtual ~DumpQuotaTableTask() {}

  // QuotaThreadTask:
  virtual void RunOnTargetThread() OVERRIDE {
    if (!database()->DumpQuotaTable(
            new TableCallback(
                base::Bind(&self_type::AppendEntry, this))))
      set_db_disabled(true);
  }

  virtual void Aborted() OVERRIDE {
    callback_.Run(TableEntries());
  }

  // DatabaseTaskBase:
  virtual void DatabaseTaskCompleted() OVERRIDE {
    callback_.Run(entries_);
  }

 private:
  bool AppendEntry(const TableEntry& entry) {
    entries_.push_back(entry);
    return true;
  }

  Callback callback_;
  TableEntries entries_;
};

class QuotaManager::DumpOriginInfoTableTask
    : public QuotaManager::DatabaseTaskBase {
 private:
  typedef QuotaManager::DumpOriginInfoTableTask self_type;
  typedef QuotaManager::DumpOriginInfoTableCallback Callback;
  typedef QuotaManager::OriginInfoTableEntry TableEntry;
  typedef QuotaManager::OriginInfoTableEntries TableEntries;
  typedef QuotaDatabase::OriginInfoTableCallback TableCallback;

 public:
  DumpOriginInfoTableTask(
      QuotaManager* manager,
      const Callback& callback)
      : DatabaseTaskBase(manager),
        callback_(callback) {
  }

 protected:
  virtual ~DumpOriginInfoTableTask() {}

  // QuotaThreadTask:
  virtual void RunOnTargetThread() OVERRIDE {
    if (!database()->DumpOriginInfoTable(
            new TableCallback(
                base::Bind(&self_type::AppendEntry, this))))
      set_db_disabled(true);
  }

  virtual void Aborted() OVERRIDE {
    callback_.Run(TableEntries());
  }

  // DatabaseTaskBase:
  virtual void DatabaseTaskCompleted() OVERRIDE {
    callback_.Run(entries_);
  }

 private:
  bool AppendEntry(const TableEntry& entry) {
    entries_.push_back(entry);
    return true;
  }

  Callback callback_;
  TableEntries entries_;
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
    weak_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
}

void QuotaManager::GetUsageInfo(const GetUsageInfoCallback& callback) {
  LazyInitialize();
  GetUsageInfoTask* get_usage_info = new GetUsageInfoTask(this, callback);
  get_usage_info->Start();
}

void QuotaManager::GetUsageAndQuota(
    const GURL& origin, StorageType type,
    const GetUsageAndQuotaCallback& callback) {
  GetUsageAndQuotaInternal(origin, type, false /* global */,
                           base::Bind(&CallGetUsageAndQuotaCallback,
                                      callback, IsStorageUnlimited(origin)));
}

void QuotaManager::GetAvailableSpace(const AvailableSpaceCallback& callback) {
  if (is_incognito_) {
    callback.Run(kQuotaStatusOk, kIncognitoDefaultTemporaryQuota);
    return;
  }
  make_scoped_refptr(new AvailableSpaceQueryTask(this, callback))->Start();
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

  make_scoped_refptr(new UpdateTemporaryQuotaOverrideTask(
      this, new_quota, callback))->Start();
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
  scoped_refptr<GetPersistentHostQuotaTask> task(
      new GetPersistentHostQuotaTask(this, host, callback));
  task->Start();
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

  if (!db_disabled_) {
    scoped_refptr<UpdatePersistentHostQuotaTask> task(
        new UpdatePersistentHostQuotaTask(
            this, host, new_quota, callback));
    task->Start();
  } else {
    callback.Run(kQuotaErrorInvalidAccess,
                  host, kStorageTypePersistent, -1);
  }
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

void QuotaManager::GetOriginsModifiedSince(StorageType type,
                                           base::Time modified_since,
                                           const GetOriginsCallback& callback) {
  LazyInitialize();
  make_scoped_refptr(new GetModifiedSinceTask(
      this, type, modified_since, callback))->Start();
}

QuotaManager::~QuotaManager() {
  proxy_->manager_ = NULL;
  std::for_each(clients_.begin(), clients_.end(),
                std::mem_fun(&QuotaClient::OnQuotaManagerDestroyed));
  if (database_.get())
    db_thread_->DeleteSoon(FROM_HERE, database_.release());
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

  make_scoped_refptr(new InitializeTask(this))->Start();
}

void QuotaManager::RegisterClient(QuotaClient* client) {
  DCHECK(!database_.get());
  clients_.push_back(client);
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

bool QuotaManager::ResetUsageTracker(StorageType type) {
  switch (type) {
    case kStorageTypeTemporary:
      if (temporary_usage_tracker_->IsWorking())
        return false;
      temporary_usage_tracker_.reset(
          new UsageTracker(clients_, kStorageTypeTemporary,
                           special_storage_policy_));
      return true;
    case kStorageTypePersistent:
      if (persistent_usage_tracker_->IsWorking())
        return false;
      persistent_usage_tracker_.reset(
          new UsageTracker(clients_, kStorageTypePersistent,
                           special_storage_policy_));
      return true;
    default:
      NOTREACHED();
  }
  return true;
}

UsageTracker* QuotaManager::GetUsageTracker(StorageType type) const {
  switch (type) {
    case kStorageTypeTemporary:
      return temporary_usage_tracker_.get();
    case kStorageTypePersistent:
      return persistent_usage_tracker_.get();
    default:
      NOTREACHED();
  }
  return NULL;
}

void QuotaManager::GetCachedOrigins(
    StorageType type, std::set<GURL>* origins) {
  DCHECK(origins);
  LazyInitialize();
  switch (type) {
    case kStorageTypeTemporary:
      DCHECK(temporary_usage_tracker_.get());
      temporary_usage_tracker_->GetCachedOrigins(origins);
      return;
    case kStorageTypePersistent:
      DCHECK(persistent_usage_tracker_.get());
      persistent_usage_tracker_->GetCachedOrigins(origins);
      return;
    default:
      NOTREACHED();
  }
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
  make_scoped_refptr(new UpdateAccessTimeTask(
      this, origin, type, accessed_time))->Start();
}

void QuotaManager::NotifyStorageModifiedInternal(
    QuotaClient::ID client_id,
    const GURL& origin,
    StorageType type,
    int64 delta,
    base::Time modified_time) {
  LazyInitialize();
  GetUsageTracker(type)->UpdateUsageCache(client_id, origin, delta);
  make_scoped_refptr(new UpdateModifiedTimeTask(
      this, origin, type, modified_time))->Start();
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
  // DidRunInitializeTask if temporary_quota_initialized_ is false here.)
  if (found->second->AddCallback(callback) &&
      (requested_type != kStorageTypeTemporary ||
       temporary_quota_initialized_)) {
    found->second->Start();
  }
}

void QuotaManager::DumpQuotaTable(const DumpQuotaTableCallback& callback) {
  make_scoped_refptr(new DumpQuotaTableTask(this, callback))->Start();
}

void QuotaManager::DumpOriginInfoTable(
    const DumpOriginInfoTableCallback& callback) {
  make_scoped_refptr(new DumpOriginInfoTableTask(this, callback))->Start();
}


void QuotaManager::DeleteOriginFromDatabase(
    const GURL& origin, StorageType type) {
  LazyInitialize();
  if (db_disabled_)
    return;
  scoped_refptr<DeleteOriginInfo> task =
      new DeleteOriginInfo(this, origin, type);
  task->Start();
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
  scoped_refptr<GetLRUOriginTask> task(new GetLRUOriginTask(
      this, type, origins_in_use_, origins_in_error_,
      base::Bind(&QuotaManager::DidGetDatabaseLRUOrigin,
                 weak_factory_.GetWeakPtr())));
  task->Start();
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

void QuotaManager::StartEviction() {
  DCHECK(!temporary_storage_evictor_.get());
  temporary_storage_evictor_.reset(new QuotaTemporaryStorageEvictor(
      this, kEvictionIntervalInMilliSeconds));
  if (desired_available_space_ >= 0)
    temporary_storage_evictor_->set_min_available_disk_space_to_start_eviction(
        desired_available_space_);
  temporary_storage_evictor_->Start();
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

void QuotaManager::DidRunInitializeTask() {
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

void QuotaManager::DidGetInitialTemporaryGlobalQuota(
    QuotaStatusCode status, StorageType type, int64 quota_unused) {
  DCHECK_EQ(type, kStorageTypeTemporary);

  if (eviction_disabled_)
    return;

  // This will call the StartEviction() when initial origin registration
  // is completed.
  make_scoped_refptr(new InitializeTemporaryOriginsInfoTask(
      this, temporary_usage_tracker_.get()))->Start();
}

void QuotaManager::DidGetDatabaseLRUOrigin(const GURL& origin) {
  // Make sure the returned origin is (still) not in the origin_in_use_ set
  // and has not been accessed since we posted the task.
  if (origins_in_use_.find(origin) != origins_in_use_.end() ||
      access_notified_origins_.find(origin) != access_notified_origins_.end())
    lru_origin_callback_.Run(GURL());
  else
    lru_origin_callback_.Run(origin);
  access_notified_origins_.clear();
  lru_origin_callback_.Reset();
}

void QuotaManager::DeleteOnCorrectThread() const {
  if (!io_thread_->BelongsToCurrentThread() && 
      io_thread_->DeleteSoon(FROM_HERE, this)) {
    return;
  }
  delete this;
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
