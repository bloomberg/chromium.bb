// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
#include "base/memory/ref_counted.h"
#include "base/message_loop_proxy.h"
#include "base/metrics/histogram.h"
#include "base/stl_util.h"
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

using base::ScopedCallbackFactory;

namespace quota {

namespace {

const char kEnableQuotaEviction[] = "enable-quota-eviction";
const int64 kMBytes = 1024 * 1024;
const int kMinutesInMilliSeconds = 60 * 1000;

// Returns the initial size of the temporary storage quota.
// (This just gives a default initial size; once its initial size is determined
// it won't automatically be adjusted.)
int64 GetInitialTemporaryStorageQuotaSize(const FilePath& path,
                                          bool is_incognito) {
  int64 free_space = base::SysInfo::AmountOfFreeDiskSpace(path);
  UMA_HISTOGRAM_MBYTES("Quota.FreeDiskSpaceForProfile", free_space);

  // Returns 0 (disables the temporary storage) if the available space is
  // less than the twice of the default quota size.
  if (free_space < QuotaManager::kTemporaryStorageQuotaDefaultSize * 2)
    return 0;

  if (is_incognito)
    return QuotaManager::kIncognitoDefaultTemporaryQuota;

  // Returns the default quota size while it is more than 5% of the
  // available space.
  if (free_space < QuotaManager::kTemporaryStorageQuotaDefaultSize * 20)
    return QuotaManager::kTemporaryStorageQuotaDefaultSize;

  // Returns the 5% of the available space while it does not exceed the
  // maximum quota size (1GB).
  if (free_space < QuotaManager::kTemporaryStorageQuotaMaxSize * 20)
    return free_space / 20;

  return QuotaManager::kTemporaryStorageQuotaMaxSize;
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

}  // anonymous namespace

// TODO(kinuko): We will need to have different sizes for different platforms
// (e.g. larger for desktop etc) and may want to have them in preferences.
const int64 QuotaManager::kTemporaryStorageQuotaDefaultSize = 50 * kMBytes;
const int64 QuotaManager::kTemporaryStorageQuotaMaxSize = 1 * 1024 * kMBytes;
const int64 QuotaManager::kIncognitoDefaultTemporaryQuota = 50 * kMBytes;

const int QuotaManager::kPerHostTemporaryPortion = 5;  // 20%

const char QuotaManager::kDatabaseName[] = "QuotaManager";

const int QuotaManager::kThresholdOfErrorsToBeBlacklisted = 3;

const int QuotaManager::kEvictionIntervalInMilliSeconds =
    30 * kMinutesInMilliSeconds;

const base::TimeDelta QuotaManager::kReportHistogramInterval =
    base::TimeDelta::FromMilliseconds(60 * 60 * 1000);  // 1 hour

// This class is for posting GetUsage/GetQuota tasks, gathering
// results and dispatching GetAndQuota callbacks.
// This class is self-destructed.
class QuotaManager::UsageAndQuotaDispatcherTask : public QuotaTask {
 public:
  typedef std::deque<GetUsageAndQuotaCallback*> CallbackList;

  static UsageAndQuotaDispatcherTask* Create(
      QuotaManager* manager, const std::string& host, StorageType type);

  // Returns true if it is the first call for this task; which means
  // the caller needs to call Start().
  bool AddCallback(GetUsageAndQuotaCallback* callback, bool unlimited) {
    if (unlimited)
      unlimited_callbacks_.push_back(callback);
    else
      callbacks_.push_back(callback);
    return (callbacks_.size() + unlimited_callbacks_.size() == 1);
  }

  void DidGetGlobalUsage(StorageType type, int64 usage,
                         int64 unlimited_usage) {
    DCHECK_EQ(type_, type);
    DCHECK_GE(usage, unlimited_usage);
    global_usage_ = usage;
    global_unlimited_usage_ = unlimited_usage;
    CheckCompleted();
  }

  void DidGetHostUsage(const std::string& host,
                       StorageType type,
                       int64 usage) {
    DCHECK_EQ(host_, host);
    DCHECK_EQ(type_, type);
    host_usage_ = usage;
    CheckCompleted();
  }

  void DidGetGlobalQuota(QuotaStatusCode status,
                         StorageType type,
                         int64 quota) {
    DCHECK_EQ(type_, type);
    quota_status_ = status;
    quota_ = quota;
    CheckCompleted();
  }

  void DidGetHostQuota(QuotaStatusCode status,
                       const std::string& host,
                       StorageType type,
                       int64 quota) {
    DCHECK_EQ(host_, host);
    DCHECK_EQ(type_, type);
    quota_status_ = status;
    quota_ = quota;
    CheckCompleted();
  }

 protected:
  UsageAndQuotaDispatcherTask(
      QuotaManager* manager,
      const std::string& host,
      StorageType type)
      : QuotaTask(manager),
        host_(host),
        type_(type),
        quota_(-1),
        global_usage_(-1),
        global_unlimited_usage_(-1),
        host_usage_(-1),
        quota_status_(kQuotaStatusUnknown),
        waiting_callbacks_(1),
        callback_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {}

  virtual ~UsageAndQuotaDispatcherTask() {
    STLDeleteContainerPointers(callbacks_.begin(), callbacks_.end());
    STLDeleteContainerPointers(unlimited_callbacks_.begin(),
                               unlimited_callbacks_.end());
  }

  // Subclasses must implement them.
  virtual void RunBody() = 0;
  virtual void DispatchCallbacks() = 0;

  virtual void Run() OVERRIDE {
    RunBody();
    // We initialize waiting_callbacks to 1 so that we won't run
    // the completion callback until here even some of the callbacks
    // are dispatched synchronously.
    CheckCompleted();
  }

  virtual void Aborted() OVERRIDE {
    CallCallbacksAndClear(&callbacks_, kQuotaErrorAbort, 0, 0);
    CallCallbacksAndClear(&unlimited_callbacks_, kQuotaErrorAbort, 0, 0);
    DeleteSoon();
  }

  virtual void Completed() OVERRIDE {
    DeleteSoon();
  }

  void CallCallbacksAndClear(
      CallbackList* callbacks, QuotaStatusCode status,
      int64 usage, int64 quota) {
    for (CallbackList::iterator iter = callbacks->begin();
         iter != callbacks->end(); ++iter) {
      (*iter)->Run(status, usage, quota);
      delete *iter;
    }
    callbacks->clear();
  }

  QuotaManager* manager() const {
    return static_cast<QuotaManager*>(observer());
  }

  std::string host() const { return host_; }
  StorageType type() const { return type_; }
  int64 quota() const { return quota_; }
  int64 global_usage() const { return global_usage_; }
  int64 global_unlimited_usage() const { return global_unlimited_usage_; }
  int64 host_usage() const { return host_usage_; }
  QuotaStatusCode quota_status() const { return quota_status_; }
  CallbackList& callbacks() { return callbacks_; }
  CallbackList& unlimited_callbacks() { return unlimited_callbacks_; }

  // Subclasses must call following methods to create a new 'waitable'
  // callback, which decrements waiting_callbacks when it is called.
  GlobalUsageCallback* NewWaitableGlobalUsageCallback() {
    ++waiting_callbacks_;
    return callback_factory_.NewCallback(
            &UsageAndQuotaDispatcherTask::DidGetGlobalUsage);
  }
  HostUsageCallback* NewWaitableHostUsageCallback() {
    ++waiting_callbacks_;
    return callback_factory_.NewCallback(
            &UsageAndQuotaDispatcherTask::DidGetHostUsage);
  }
  QuotaCallback* NewWaitableGlobalQuotaCallback() {
    ++waiting_callbacks_;
    return callback_factory_.NewCallback(
            &UsageAndQuotaDispatcherTask::DidGetGlobalQuota);
  }
  HostQuotaCallback* NewWaitableHostQuotaCallback() {
    ++waiting_callbacks_;
    return callback_factory_.NewCallback(
            &UsageAndQuotaDispatcherTask::DidGetHostQuota);
  }

 private:
  void CheckCompleted() {
    if (--waiting_callbacks_ <= 0) {
      DispatchCallbacks();
      DCHECK(callbacks_.empty());
      DCHECK(unlimited_callbacks_.empty());

      UsageAndQuotaDispatcherTaskMap& dispatcher_map =
          manager()->usage_and_quota_dispatchers_;
      DCHECK(dispatcher_map.find(std::make_pair(host_, type_)) !=
             dispatcher_map.end());
      dispatcher_map.erase(std::make_pair(host_, type_));
      CallCompleted();
    }
  }

  const std::string host_;
  const StorageType type_;
  int64 quota_;
  int64 global_usage_;
  int64 global_unlimited_usage_;
  int64 host_usage_;
  QuotaStatusCode quota_status_;
  CallbackList callbacks_;
  CallbackList unlimited_callbacks_;
  int waiting_callbacks_;
  ScopedCallbackFactory<UsageAndQuotaDispatcherTask> callback_factory_;

  DISALLOW_COPY_AND_ASSIGN(UsageAndQuotaDispatcherTask);
};

class QuotaManager::UsageAndQuotaDispatcherTaskForTemporary
    : public QuotaManager::UsageAndQuotaDispatcherTask {
 public:
  UsageAndQuotaDispatcherTaskForTemporary(
      QuotaManager* manager, const std::string& host)
      : UsageAndQuotaDispatcherTask(manager, host, kStorageTypeTemporary) {}

 protected:
  virtual void RunBody() OVERRIDE {
    manager()->temporary_usage_tracker_->GetGlobalUsage(
        NewWaitableGlobalUsageCallback());
    manager()->temporary_usage_tracker_->GetHostUsage(
        host(), NewWaitableHostUsageCallback());
    manager()->GetTemporaryGlobalQuota(NewWaitableGlobalQuotaCallback());
  }

  virtual void DispatchCallbacks() OVERRIDE {
    // Due to a mismatch of models between same-origin (required by stds) vs
    // per-host (as this info is viewed in the UI) vs the extension systems
    // notion of webextents (the basis of granting unlimited rights),
    // we can end up with both 'unlimited' and 'limited' callbacks to invoke.
    // Should be rare, but it can happen.

    CallCallbacksAndClear(&unlimited_callbacks(), quota_status(),
                          host_usage(), kint64max);

    if (!callbacks().empty()) {
      // Allow an individual host to utilize a fraction of the total
      // pool available for temp storage.
      int64 host_quota = quota() / kPerHostTemporaryPortion;

      // But if total temp usage is over-budget, stop letting new data in
      // until we reclaim space.
      DCHECK_GE(global_usage(), global_unlimited_usage());
      int64 limited_global_usage = global_usage() - global_unlimited_usage();
      if (limited_global_usage > quota())
        host_quota = std::min(host_quota, host_usage());

      CallCallbacksAndClear(&callbacks(), quota_status(),
                            host_usage(), host_quota);
    }
  }
};

class QuotaManager::UsageAndQuotaDispatcherTaskForPersistent
    : public QuotaManager::UsageAndQuotaDispatcherTask {
 public:
  UsageAndQuotaDispatcherTaskForPersistent(
      QuotaManager* manager, const std::string& host)
      : UsageAndQuotaDispatcherTask(manager, host, kStorageTypePersistent) {}

 protected:
  virtual void RunBody() OVERRIDE {
    manager()->persistent_usage_tracker_->GetHostUsage(
        host(), NewWaitableHostUsageCallback());
    manager()->GetPersistentHostQuota(
        host(), NewWaitableHostQuotaCallback());
  }

  virtual void DispatchCallbacks() OVERRIDE {
    CallCallbacksAndClear(&callbacks(), quota_status(),
                          host_usage(), quota());
    CallCallbacksAndClear(&unlimited_callbacks(), quota_status(),
                          host_usage(), kint64max);
  }
};

// static
QuotaManager::UsageAndQuotaDispatcherTask*
QuotaManager::UsageAndQuotaDispatcherTask::Create(
    QuotaManager* manager, const std::string& host, StorageType type) {
  switch (type) {
    case kStorageTypeTemporary:
      return new UsageAndQuotaDispatcherTaskForTemporary(
          manager, host);
    case kStorageTypePersistent:
      return new UsageAndQuotaDispatcherTaskForPersistent(
          manager, host);
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
                    StatusCallback* callback)
      : QuotaTask(manager),
        origin_(origin),
        type_(type),
        error_count_(0),
        remaining_clients_(-1),
        callback_(callback),
        callback_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {}

 protected:
  virtual void Run() OVERRIDE {
    error_count_ = 0;
    remaining_clients_ = manager()->clients_.size();
    for (QuotaClientList::iterator iter = manager()->clients_.begin();
         iter != manager()->clients_.end(); ++iter) {
      (*iter)->DeleteOriginData(origin_, type_, callback_factory_.NewCallback(
          &OriginDataDeleter::DidDeleteOriginData));
    }
  }

  virtual void Completed() OVERRIDE {
    if (error_count_ == 0) {
      manager()->DeleteOriginFromDatabase(origin_, type_);
      callback_->Run(kQuotaStatusOk);
    } else {
      callback_->Run(kQuotaErrorInvalidModification);
    }
    DeleteSoon();
  }

  virtual void Aborted() OVERRIDE {
    callback_->Run(kQuotaErrorAbort);
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
  int error_count_;
  int remaining_clients_;
  scoped_ptr<StatusCallback> callback_;

  ScopedCallbackFactory<OriginDataDeleter> callback_factory_;
  DISALLOW_COPY_AND_ASSIGN(OriginDataDeleter);
};

class QuotaManager::DatabaseTaskBase : public QuotaThreadTask {
 public:
  explicit DatabaseTaskBase(QuotaManager* manager)
      : QuotaThreadTask(manager, manager->db_thread_),
        manager_(manager),
        database_(manager->database_.get()),
        db_disabled_(false) {
    DCHECK(manager_);
    DCHECK(database_);
  }

 protected:
  virtual void DatabaseTaskCompleted() = 0;

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
  InitializeTask(
      QuotaManager* manager,
      const FilePath& profile_path,
      bool is_incognito)
      : DatabaseTaskBase(manager),
        profile_path_(profile_path),
        is_incognito_(is_incognito),
        need_initialize_origins_(false),
        temporary_storage_quota_(-1) {
  }

 protected:
  virtual void RunOnTargetThread() OVERRIDE {
    // Initializes the global temporary quota.
    if (!database()->GetGlobalQuota(
            kStorageTypeTemporary, &temporary_storage_quota_)) {
      // If the temporary storage quota size has not been initialized,
      // make up one and store it in the database.
      temporary_storage_quota_ = GetInitialTemporaryStorageQuotaSize(
          profile_path_, is_incognito_);
      UMA_HISTOGRAM_MBYTES("Quota.InitialTemporaryGlobalStorageQuota",
                           temporary_storage_quota_);
      if (!database()->SetGlobalQuota(
              kStorageTypeTemporary, temporary_storage_quota_)) {
        set_db_disabled(true);
      }
    }
    if (!db_disabled())
      need_initialize_origins_ = !database()->IsOriginDatabaseBootstrapped();
  }

  virtual void DatabaseTaskCompleted() OVERRIDE {
    manager()->need_initialize_origins_ = need_initialize_origins_;
    manager()->DidInitializeTemporaryGlobalQuota(temporary_storage_quota_);
    manager()->histogram_timer_.Start(FROM_HERE,
                                      QuotaManager::kReportHistogramInterval,
                                      manager(),
                                      &QuotaManager::ReportHistogram);
  }

 private:
  FilePath profile_path_;
  bool is_incognito_;
  bool need_initialize_origins_;
  int64 temporary_storage_quota_;
};

class QuotaManager::UpdateTemporaryGlobalQuotaTask
    : public QuotaManager::DatabaseTaskBase {
 public:
  UpdateTemporaryGlobalQuotaTask(
      QuotaManager* manager,
      int64 new_quota,
      QuotaCallback* callback)
      : DatabaseTaskBase(manager),
        new_quota_(new_quota),
        callback_(callback) {
    DCHECK_GE(new_quota, 0);
  }

 protected:
  virtual void RunOnTargetThread() OVERRIDE {
    if (!database()->SetGlobalQuota(kStorageTypeTemporary, new_quota_)) {
      set_db_disabled(true);
      new_quota_ = 0;
    }
  }

  virtual void DatabaseTaskCompleted() OVERRIDE {
    callback_->Run(db_disabled() ? kQuotaErrorInvalidAccess : kQuotaStatusOk,
                   kStorageTypeTemporary, new_quota_);
    if (!db_disabled()) {
      manager()->temporary_global_quota_ = new_quota_;
    }
  }

 private:
  int64 new_quota_;
  scoped_ptr<QuotaCallback> callback_;
};

class QuotaManager::GetPersistentHostQuotaTask
    : public QuotaManager::DatabaseTaskBase {
 public:
  GetPersistentHostQuotaTask(
      QuotaManager* manager,
      const std::string& host,
      HostQuotaCallback* callback)
      : DatabaseTaskBase(manager),
        host_(host),
        quota_(-1),
        callback_(callback) {
  }
 protected:
  virtual void RunOnTargetThread() OVERRIDE {
    if (!database()->GetHostQuota(host_, kStorageTypePersistent, &quota_))
      quota_ = 0;
  }
  virtual void DatabaseTaskCompleted() OVERRIDE {
    callback_->Run(kQuotaStatusOk,
                   host_, kStorageTypePersistent, quota_);
  }
 private:
  std::string host_;
  int64 quota_;
  scoped_ptr<HostQuotaCallback> callback_;
};

class QuotaManager::UpdatePersistentHostQuotaTask
    : public QuotaManager::DatabaseTaskBase {
 public:
  UpdatePersistentHostQuotaTask(
      QuotaManager* manager,
      const std::string& host,
      int new_quota,
      HostQuotaCallback* callback)
      : DatabaseTaskBase(manager),
        host_(host),
        new_quota_(new_quota),
        callback_(callback) {
    DCHECK_GE(new_quota_, 0);
  }
 protected:
  virtual void RunOnTargetThread() OVERRIDE {
    if (!database()->SetHostQuota(host_, kStorageTypePersistent, new_quota_)) {
      set_db_disabled(true);
      new_quota_ = 0;
    }
  }

  virtual void DatabaseTaskCompleted() OVERRIDE {
    callback_->Run(db_disabled() ? kQuotaErrorInvalidAccess : kQuotaStatusOk,
                   host_, kStorageTypePersistent, new_quota_);
  }

  virtual void Aborted() OVERRIDE {
    callback_.reset();
  }

 private:
  std::string host_;
  int64 new_quota_;
  scoped_ptr<HostQuotaCallback> callback_;
};

class QuotaManager::GetLRUOriginTask
    : public QuotaManager::DatabaseTaskBase {
 public:
  GetLRUOriginTask(
      QuotaManager* manager,
      StorageType type,
      const std::map<GURL, int>& origins_in_use,
      const std::map<GURL, int>& origins_in_error,
      GetLRUOriginCallback *callback)
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
  virtual void RunOnTargetThread() OVERRIDE {
    if (!database()->GetLRUOrigin(type_, exceptions_,
                                  special_storage_policy_, &url_))
      set_db_disabled(true);
  }

  virtual void DatabaseTaskCompleted() OVERRIDE {
    callback_->Run(url_);
  }

  virtual void Aborted() OVERRIDE {
    callback_.reset();
  }

 private:
  StorageType type_;
  std::set<GURL> exceptions_;
  scoped_ptr<GetLRUOriginCallback> callback_;
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
  virtual void RunOnTargetThread() OVERRIDE {
    if (!database()->DeleteOriginInfo(origin_, type_)) {
      set_db_disabled(true);
    }
  }
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
      scoped_refptr<base::MessageLoopProxy> db_message_loop,
      const FilePath& profile_path,
      AvailableSpaceCallback* callback)
      : QuotaThreadTask(manager, db_message_loop),
        profile_path_(profile_path),
        space_(-1),
        callback_(callback) {}
  virtual ~AvailableSpaceQueryTask() {}

 protected:
  virtual void RunOnTargetThread() OVERRIDE {
    space_ = base::SysInfo::AmountOfFreeDiskSpace(profile_path_);
  }

  virtual void Aborted() OVERRIDE {
    callback_->Run(kQuotaErrorAbort, -1);
  }

  virtual void Completed() OVERRIDE {
    callback_->Run(kQuotaStatusOk, space_);
  }

 private:
  FilePath profile_path_;
  int64 space_;
  scoped_ptr<AvailableSpaceCallback> callback_;
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
  virtual void RunOnTargetThread() OVERRIDE {
    if (!database()->SetOriginLastAccessTime(origin_, type_, accessed_time_)) {
      set_db_disabled(true);
    }
  }
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
  virtual void RunOnTargetThread() OVERRIDE {
    if (!database()->SetOriginLastModifiedTime(
            origin_, type_, modified_time_)) {
      set_db_disabled(true);
    }
  }
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
      GetOriginsCallback* callback)
      : DatabaseTaskBase(manager),
        type_(type),
        modified_since_(modified_since),
        callback_(callback) {}

 protected:
  virtual void RunOnTargetThread() OVERRIDE {
    if (!database()->GetOriginsModifiedSince(
            type_, &origins_, modified_since_)) {
      set_db_disabled(true);
    }
  }

  virtual void DatabaseTaskCompleted() OVERRIDE {
    callback_->Run(origins_, type_);
  }

  virtual void Aborted() OVERRIDE {
    callback_->Run(std::set<GURL>(), type_);
  }

 private:
  StorageType type_;
  base::Time modified_since_;
  std::set<GURL> origins_;
  scoped_ptr<GetOriginsCallback> callback_;
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
      Callback* callback)
      : DatabaseTaskBase(manager),
        callback_(callback) {
  }
 protected:
  virtual void RunOnTargetThread() OVERRIDE {
    if (!database()->DumpQuotaTable(
            new TableCallback(
                base::Bind(&self_type::AppendEntry, this))))
      set_db_disabled(true);
  }

  virtual void Aborted() OVERRIDE {
    callback_->Run(TableEntries());
  }

  virtual void DatabaseTaskCompleted() OVERRIDE {
    callback_->Run(entries_);
  }

 private:
  bool AppendEntry(const TableEntry& entry) {
    entries_.push_back(entry);
    return true;
  }

  scoped_ptr<Callback> callback_;
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
      Callback* callback)
      : DatabaseTaskBase(manager),
        callback_(callback) {
  }
 protected:
  virtual void RunOnTargetThread() OVERRIDE {
    if (!database()->DumpOriginInfoTable(
            new TableCallback(
                base::Bind(&self_type::AppendEntry, this))))
      set_db_disabled(true);
  }

  virtual void Aborted() OVERRIDE {
    callback_->Run(TableEntries());
  }

  virtual void DatabaseTaskCompleted() OVERRIDE {
    callback_->Run(entries_);
  }

 private:
  bool AppendEntry(const TableEntry& entry) {
    entries_.push_back(entry);
    return true;
  }

  scoped_ptr<Callback> callback_;
  TableEntries entries_;
};

// QuotaManager ---------------------------------------------------------------

QuotaManager::QuotaManager(bool is_incognito,
                           const FilePath& profile_path,
                           base::MessageLoopProxy* io_thread,
                           base::MessageLoopProxy* db_thread,
                           SpecialStoragePolicy* special_storage_policy)
  : is_incognito_(is_incognito),
    profile_path_(profile_path),
    proxy_(new QuotaManagerProxy(
        ALLOW_THIS_IN_INITIALIZER_LIST(this), io_thread)),
    db_disabled_(false),
    eviction_disabled_(false),
    io_thread_(io_thread),
    db_thread_(db_thread),
    need_initialize_origins_(false),
    temporary_global_quota_(-1),
    special_storage_policy_(special_storage_policy),
    callback_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
}

QuotaManager::~QuotaManager() {
  DCHECK(io_thread_->BelongsToCurrentThread());
  proxy_->manager_ = NULL;
  std::for_each(clients_.begin(), clients_.end(),
                std::mem_fun(&QuotaClient::OnQuotaManagerDestroyed));
  if (database_.get())
    db_thread_->DeleteSoon(FROM_HERE, database_.release());
}

void QuotaManager::GetUsageAndQuota(
    const GURL& origin, StorageType type,
    GetUsageAndQuotaCallback* callback_ptr) {
  scoped_ptr<GetUsageAndQuotaCallback> callback(callback_ptr);
  LazyInitialize();

  if (type == kStorageTypeUnknown) {
    // Quota only supports temporary/persistent types.
    callback->Run(kQuotaErrorNotSupported, 0, 0);
    return;
  }

  // note: returns host usage and quota
  std::string host = net::GetHostOrSpecFromURL(origin);
  UsageAndQuotaDispatcherTaskMap::iterator found =
      usage_and_quota_dispatchers_.find(std::make_pair(host, type));
  if (found == usage_and_quota_dispatchers_.end()) {
    UsageAndQuotaDispatcherTask* dispatcher =
        UsageAndQuotaDispatcherTask::Create(this, host, type);
    found = usage_and_quota_dispatchers_.insert(
        std::make_pair(std::make_pair(host, type), dispatcher)).first;
  }
  if (found->second->AddCallback(
          callback.release(), IsStorageUnlimited(origin))) {
    found->second->Start();
  }
}

void QuotaManager::GetAvailableSpace(AvailableSpaceCallback* callback) {
  scoped_refptr<AvailableSpaceQueryTask> task(
      new AvailableSpaceQueryTask(this, db_thread_, profile_path_, callback));
  task->Start();
}

void QuotaManager::GetTemporaryGlobalQuota(QuotaCallback* callback) {
  LazyInitialize();
  if (temporary_global_quota_ >= 0) {
    // TODO(kinuko): We may want to adjust the quota when the current
    // available space in the hard drive is getting tight.
    callback->Run(kQuotaStatusOk,
                  kStorageTypeTemporary, temporary_global_quota_);
    delete callback;
    return;
  }
  // They are called upon completion of InitializeTask.
  temporary_global_quota_callbacks_.Add(callback);
}

void QuotaManager::SetTemporaryGlobalQuota(int64 new_quota,
                                           QuotaCallback* callback) {
  LazyInitialize();
  if (new_quota < 0) {
    callback->Run(kQuotaErrorInvalidModification,
                  kStorageTypeTemporary, -1);
    delete callback;
    return;
  }

  if (!db_disabled_) {
    scoped_refptr<UpdateTemporaryGlobalQuotaTask> task(
        new UpdateTemporaryGlobalQuotaTask(this, new_quota, callback));
    task->Start();
  } else {
    callback->Run(kQuotaErrorInvalidAccess,
                  kStorageTypeTemporary, -1);
    delete callback;
  }
}

void QuotaManager::GetPersistentHostQuota(const std::string& host,
                                          HostQuotaCallback* callback_ptr) {
  scoped_ptr<HostQuotaCallback> callback(callback_ptr);
  LazyInitialize();
  if (host.empty()) {
    // This could happen if we are called on file:///.
    // TODO(kinuko) We may want to respect --allow-file-access-from-files
    // command line switch.
    callback->Run(kQuotaStatusOk, host, kStorageTypePersistent, 0);
    return;
  }
  scoped_refptr<GetPersistentHostQuotaTask> task(
      new GetPersistentHostQuotaTask(this, host, callback.release()));
  task->Start();
}

void QuotaManager::SetPersistentHostQuota(const std::string& host,
                                          int64 new_quota,
                                          HostQuotaCallback* callback_ptr) {
  scoped_ptr<HostQuotaCallback> callback(callback_ptr);
  LazyInitialize();
  if (host.empty()) {
    // This could happen if we are called on file:///.
    callback->Run(kQuotaErrorNotSupported, host, kStorageTypePersistent, 0);
    return;
  }
  if (new_quota < 0) {
    callback->Run(kQuotaErrorInvalidModification,
                  host, kStorageTypePersistent, -1);
    return;
  }

  if (!db_disabled_) {
    scoped_refptr<UpdatePersistentHostQuotaTask> task(
        new UpdatePersistentHostQuotaTask(
            this, host, new_quota, callback.release()));
    task->Start();
  } else {
    callback->Run(kQuotaErrorInvalidAccess,
                  host, kStorageTypePersistent, -1);
  }
}

void QuotaManager::GetGlobalUsage(
    StorageType type,
    GlobalUsageCallback* callback) {
  LazyInitialize();
  GetUsageTracker(type)->GetGlobalUsage(callback);
}

void QuotaManager::GetHostUsage(const std::string& host, StorageType type,
                                HostUsageCallback* callback) {
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

void QuotaManager::GetOriginsModifiedSince(
    StorageType type,
    base::Time modified_since,
    GetOriginsCallback* callback) {
  LazyInitialize();
  make_scoped_refptr(new GetModifiedSinceTask(
      this, type, modified_since, callback))->Start();
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

  scoped_refptr<InitializeTask> task(
      new InitializeTask(this, profile_path_, is_incognito_));
  task->Start();
}

void QuotaManager::RegisterClient(QuotaClient* client) {
  DCHECK(io_thread_->BelongsToCurrentThread());
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
    const GURL& origin, StorageType type, StatusCallback* callback) {
  LazyInitialize();

  if (origin.is_empty() || clients_.empty()) {
    callback->Run(kQuotaStatusOk);
    delete callback;
    return;
  }

  OriginDataDeleter* deleter =
      new OriginDataDeleter(this, origin, type, callback);
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
  if (type == kStorageTypeTemporary && lru_origin_callback_.get()) {
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

void QuotaManager::DumpQuotaTable(DumpQuotaTableCallback* callback) {
  make_scoped_refptr(new DumpQuotaTableTask(this, callback))->Start();
}

void QuotaManager::DumpOriginInfoTable(
    DumpOriginInfoTableCallback* callback) {
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
    GetLRUOriginCallback* callback) {
  LazyInitialize();
  // This must not be called while there's an in-flight task.
  DCHECK(!lru_origin_callback_.get());
  lru_origin_callback_.reset(callback);
  if (db_disabled_) {
    lru_origin_callback_->Run(GURL());
    lru_origin_callback_.reset();
    return;
  }
  scoped_refptr<GetLRUOriginTask> task(new GetLRUOriginTask(
      this, type, origins_in_use_,
      origins_in_error_, callback_factory_.NewCallback(
          &QuotaManager::DidGetDatabaseLRUOrigin)));
  task->Start();
}

void QuotaManager::DidOriginDataEvicted(
    QuotaStatusCode status) {
  DCHECK(io_thread_->BelongsToCurrentThread());

  // We only try evict origins that are not in use, so basically
  // deletion attempt for eviction should not fail.  Let's record
  // the origin if we get error and exclude it from future eviction
  // if the error happens consistently (> kThresholdOfErrorsToBeBlacklisted).
  if (status != kQuotaStatusOk)
    origins_in_error_[eviction_context_.evicted_origin]++;

  eviction_context_.evict_origin_data_callback->Run(status);
  eviction_context_.evict_origin_data_callback.reset();
}

void QuotaManager::EvictOriginData(
    const GURL& origin,
    StorageType type,
    EvictOriginDataCallback* callback) {
  DCHECK(io_thread_->BelongsToCurrentThread());
  DCHECK_EQ(type, kStorageTypeTemporary);

  eviction_context_.evicted_origin = origin;
  eviction_context_.evicted_type = type;
  eviction_context_.evict_origin_data_callback.reset(callback);

  DeleteOriginData(origin, type, callback_factory_.NewCallback(
      &QuotaManager::DidOriginDataEvicted));
}

void QuotaManager::DidGetAvailableSpaceForEviction(
    QuotaStatusCode status,
    int64 available_space) {
  eviction_context_.get_usage_and_quota_callback->Run(status,
      eviction_context_.usage,
      eviction_context_.unlimited_usage,
      eviction_context_.quota, available_space);
  eviction_context_.get_usage_and_quota_callback.reset();
}

void QuotaManager::DidGetGlobalQuotaForEviction(
    QuotaStatusCode status,
    StorageType type,
    int64 quota) {
  DCHECK_EQ(type, kStorageTypeTemporary);
  if (status != kQuotaStatusOk) {
    eviction_context_.get_usage_and_quota_callback->Run(
        status, 0, 0, 0, 0);
    eviction_context_.get_usage_and_quota_callback.reset();
    return;
  }

  eviction_context_.quota = quota;
  GetAvailableSpace(callback_factory_.
      NewCallback(&QuotaManager::DidGetAvailableSpaceForEviction));
}

void QuotaManager::DidGetGlobalUsageForEviction(
    StorageType type, int64 usage, int64 unlimited_usage) {
  DCHECK_EQ(type, kStorageTypeTemporary);
  DCHECK_GE(usage, unlimited_usage);
  eviction_context_.usage = usage;
  eviction_context_.unlimited_usage = unlimited_usage;
  GetTemporaryGlobalQuota(callback_factory_.
      NewCallback(&QuotaManager::DidGetGlobalQuotaForEviction));
}

void QuotaManager::GetUsageAndQuotaForEviction(
    GetUsageAndQuotaForEvictionCallback* callback) {
  DCHECK(io_thread_->BelongsToCurrentThread());
  DCHECK(!eviction_context_.get_usage_and_quota_callback.get());

  eviction_context_.get_usage_and_quota_callback.reset(callback);
  // TODO(dmikurube): Make kStorageTypeTemporary an argument.
  GetGlobalUsage(kStorageTypeTemporary, callback_factory_.
      NewCallback(&QuotaManager::DidGetGlobalUsageForEviction));
}

void QuotaManager::StartEviction() {
  DCHECK(!temporary_storage_evictor_.get());
  temporary_storage_evictor_.reset(new QuotaTemporaryStorageEvictor(this,
      kEvictionIntervalInMilliSeconds));
  temporary_storage_evictor_->Start();
}

void QuotaManager::ReportHistogram() {
  GetGlobalUsage(kStorageTypeTemporary,
                 callback_factory_.NewCallback(
                     &QuotaManager::DidGetTemporaryGlobalUsageForHistogram));
  GetGlobalUsage(kStorageTypePersistent,
                 callback_factory_.NewCallback(
                     &QuotaManager::DidGetPersistentGlobalUsageForHistogram));
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

void QuotaManager::DidInitializeTemporaryGlobalQuota(int64 quota) {
  temporary_global_quota_ = quota;
  temporary_global_quota_callbacks_.Run(
      db_disabled_ ? kQuotaErrorInvalidAccess : kQuotaStatusOk,
      kStorageTypeTemporary, quota);

  if (db_disabled_ || eviction_disabled_)
    return;

  if (!need_initialize_origins_) {
    StartEviction();
    return;
  }

  // We seem to need to initialize the origin table in the database.
  // Kick the first GetGlobalUsage for temporary storage to cache a list
  // of origins that have data in temporary storage to register them
  // in the database.  (We'll need the global temporary usage anyway
  // for eviction later.)
  temporary_usage_tracker_->GetGlobalUsage(callback_factory_.NewCallback(
      &QuotaManager::DidRunInitialGetTemporaryGlobalUsage));
}

void QuotaManager::DidRunInitialGetTemporaryGlobalUsage(
    StorageType type, int64 usage_unused, int64 unlimited_usage_unused) {
  DCHECK_EQ(type, kStorageTypeTemporary);
  // This will call the StartEviction() when initial origin registration
  // is completed.
  scoped_refptr<InitializeTemporaryOriginsInfoTask> task(
      new InitializeTemporaryOriginsInfoTask(
          this, temporary_usage_tracker_.get()));
  task->Start();
}

void QuotaManager::DidGetDatabaseLRUOrigin(const GURL& origin) {
  // Make sure the returned origin is (still) not in the origin_in_use_ set
  // and has not been accessed since we posted the task.
  if (origins_in_use_.find(origin) != origins_in_use_.end() ||
      access_notified_origins_.find(origin) != access_notified_origins_.end())
    lru_origin_callback_->Run(GURL());
  else
    lru_origin_callback_->Run(origin);
  access_notified_origins_.clear();
  lru_origin_callback_.reset();
}

void QuotaManager::DeleteOnCorrectThread() const {
  if (!io_thread_->BelongsToCurrentThread()) {
    io_thread_->DeleteSoon(FROM_HERE, this);
    return;
  }
  delete this;
}

// QuotaManagerProxy ----------------------------------------------------------

void QuotaManagerProxy::RegisterClient(QuotaClient* client) {
  if (!io_thread_->BelongsToCurrentThread()) {
    io_thread_->PostTask(FROM_HERE, NewRunnableMethod(
        this, &QuotaManagerProxy::RegisterClient, client));
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
    io_thread_->PostTask(FROM_HERE, NewRunnableMethod(
        this, &QuotaManagerProxy::NotifyStorageAccessed,
        client_id, origin, type));
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
    io_thread_->PostTask(FROM_HERE, NewRunnableMethod(
        this, &QuotaManagerProxy::NotifyStorageModified,
        client_id, origin, type, delta));
    return;
  }
  if (manager_)
    manager_->NotifyStorageModified(client_id, origin, type, delta);
}

void QuotaManagerProxy::NotifyOriginInUse(
    const GURL& origin) {
  if (!io_thread_->BelongsToCurrentThread()) {
    io_thread_->PostTask(FROM_HERE, NewRunnableMethod(
        this, &QuotaManagerProxy::NotifyOriginInUse, origin));
    return;
  }
  if (manager_)
    manager_->NotifyOriginInUse(origin);
}

void QuotaManagerProxy::NotifyOriginNoLongerInUse(
    const GURL& origin) {
  if (!io_thread_->BelongsToCurrentThread()) {
    io_thread_->PostTask(FROM_HERE, NewRunnableMethod(
        this, &QuotaManagerProxy::NotifyOriginNoLongerInUse, origin));
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
    QuotaManager* manager, base::MessageLoopProxy* io_thread)
    : manager_(manager), io_thread_(io_thread) {
}

QuotaManagerProxy::~QuotaManagerProxy() {
}

}  // namespace quota
