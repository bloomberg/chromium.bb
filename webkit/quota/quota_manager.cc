// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/quota/quota_manager.h"

#include <algorithm>
#include <deque>
#include <set>

#include "base/callback.h"
#include "base/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop_proxy.h"
#include "base/stl_util-inl.h"
#include "base/sys_info.h"
#include "net/base/net_util.h"
#include "webkit/quota/quota_database.h"
#include "webkit/quota/quota_types.h"
#include "webkit/quota/usage_tracker.h"

using base::ScopedCallbackFactory;

namespace quota {

namespace {
// Returns the initial size of the temporary storage quota.
// (This just gives a default initial size; once its initial size is determined
// it won't automatically be adjusted.)
int64 GetInitialTemporaryStorageQuotaSize(const FilePath& path,
                                          bool is_incognito) {
  int64 free_space = base::SysInfo::AmountOfFreeDiskSpace(path);

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

const int64 MBytes = 1024 * 1024;
}  // anonymous namespace

// TODO(kinuko): We will need to have different sizes for different platforms
// (e.g. larger for desktop etc) and may want to have them in preferences.
const int64 QuotaManager::kTemporaryStorageQuotaDefaultSize = 50 * MBytes;
const int64 QuotaManager::kTemporaryStorageQuotaMaxSize = 1 * 1024 * MBytes;
const char QuotaManager::kDatabaseName[] = "QuotaManager";

const int64 QuotaManager::kIncognitoDefaultTemporaryQuota = 50 * MBytes;

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
  bool AddCallback(GetUsageAndQuotaCallback* callback) {
    callbacks_.push_back(callback);
    return (callbacks_.size() == 1);
  }

  void DidGetGlobalUsage(int64 usage) {
    global_usage_ = usage;
    CheckCompleted();
  }

  void DidGetHostUsage(const std::string& host_unused, int64 usage) {
    host_usage_ = usage;
    CheckCompleted();
  }

  void DidGetGlobalQuota(QuotaStatusCode status, int64 quota) {
    quota_status_ = status;
    quota_ = quota;
    CheckCompleted();
  }

  void DidGetHostQuota(QuotaStatusCode status,
                       const std::string& host_unused,
                       int64 quota) {
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
        host_usage_(-1),
        quota_status_(kQuotaStatusUnknown),
        callback_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {}

  virtual ~UsageAndQuotaDispatcherTask() {
    STLDeleteContainerPointers(callbacks_.begin(), callbacks_.end());
  }

  virtual bool IsCompleted() const = 0;

  virtual void DispatchCallback(GetUsageAndQuotaCallback* callback) = 0;

  virtual void Aborted() OVERRIDE {
    for (CallbackList::iterator iter = callbacks_.begin();
        iter != callbacks_.end();
        ++iter) {
      (*iter)->Run(kQuotaErrorAbort, 0, 0);
      delete *iter;
    }
    callbacks_.clear();
    DeleteSoon();
  }

  virtual void Completed() OVERRIDE {
    DeleteSoon();
  }
  QuotaManager* manager() const {
    return static_cast<QuotaManager*>(observer());
  }

  std::string host() const { return host_; }
  StorageType type() const { return type_; }
  int64 quota() const { return quota_; }
  int64 global_usage() const { return global_usage_; }
  int64 host_usage() const { return host_usage_; }
  QuotaStatusCode status() const { return quota_status_; }

  UsageCallback* NewGlobalUsageCallback() {
    return callback_factory_.NewCallback(
            &UsageAndQuotaDispatcherTask::DidGetGlobalUsage);
  }

  HostUsageCallback* NewHostUsageCallback() {
    return callback_factory_.NewCallback(
            &UsageAndQuotaDispatcherTask::DidGetHostUsage);
  }

  QuotaCallback* NewGlobalQuotaCallback() {
    return callback_factory_.NewCallback(
            &UsageAndQuotaDispatcherTask::DidGetGlobalQuota);
  }

  HostQuotaCallback* NewHostQuotaCallback() {
    return callback_factory_.NewCallback(
            &UsageAndQuotaDispatcherTask::DidGetHostQuota);
  }

 private:
  void CheckCompleted() {
    if (IsCompleted()) {
      // Dispatches callbacks.
      for (CallbackList::iterator iter = callbacks_.begin();
          iter != callbacks_.end();
          ++iter) {
        DispatchCallback(*iter);
        delete *iter;
      }
      callbacks_.clear();
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
  int64 host_usage_;
  QuotaStatusCode quota_status_;
  CallbackList callbacks_;
  ScopedCallbackFactory<UsageAndQuotaDispatcherTask> callback_factory_;

  DISALLOW_COPY_AND_ASSIGN(UsageAndQuotaDispatcherTask);
};

class QuotaManager::UsageAndQuotaDispatcherTaskForTemporary
    : public QuotaManager::UsageAndQuotaDispatcherTask {
 public:
  UsageAndQuotaDispatcherTaskForTemporary(
      QuotaManager* manager, const std::string host)
      : UsageAndQuotaDispatcherTask(manager, host, kStorageTypeTemporary) {}

 protected:
  virtual void Run() OVERRIDE {
    manager()->temporary_usage_tracker_->GetGlobalUsage(
        NewGlobalUsageCallback());
    manager()->temporary_usage_tracker_->GetHostUsage(
        host(), NewHostUsageCallback());
    manager()->GetTemporaryGlobalQuota(NewGlobalQuotaCallback());
  }

  virtual bool IsCompleted() const OVERRIDE {
    return (quota() >= 0 && global_usage() >= 0 && host_usage() >= 0);
  }

  virtual void DispatchCallback(GetUsageAndQuotaCallback* callback) OVERRIDE {
    // TODO(kinuko): For now it returns pessimistic quota.  Change this
    // to return {usage, quota - nonevictable_usage} once eviction is
    // supported.
    int64 other_usage = global_usage() - host_usage();
    callback->Run(status(), host_usage(), quota() - other_usage);
  }
};

class QuotaManager::UsageAndQuotaDispatcherTaskForPersistent
    : public QuotaManager::UsageAndQuotaDispatcherTask {
 public:
  UsageAndQuotaDispatcherTaskForPersistent(
      QuotaManager* manager, const std::string host)
      : UsageAndQuotaDispatcherTask(manager, host, kStorageTypePersistent) {}

 protected:
  virtual void Run() OVERRIDE {
    manager()->persistent_usage_tracker_->GetHostUsage(
        host(), NewHostUsageCallback());
    manager()->GetPersistentHostQuota(
        host(), NewHostQuotaCallback());
  }

  virtual bool IsCompleted() const OVERRIDE {
    return (quota() >= 0 && host_usage() >= 0);
  }

  virtual void DispatchCallback(GetUsageAndQuotaCallback* callback) OVERRIDE {
    callback->Run(status(), host_usage(), quota());
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

class QuotaManager::DatabaseTaskBase : public QuotaThreadTask {
 public:
  DatabaseTaskBase(
      QuotaManager* manager,
      QuotaDatabase* database,
      scoped_refptr<base::MessageLoopProxy> db_message_loop)
      : QuotaThreadTask(manager, db_message_loop),
        manager_(manager),
        database_(database),
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
      QuotaDatabase* database,
      scoped_refptr<base::MessageLoopProxy> db_message_loop,
      const FilePath& profile_path,
      bool is_incognito)
      : DatabaseTaskBase(manager, database, db_message_loop),
        profile_path_(profile_path),
        is_incognito_(is_incognito),
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
      if (!database()->SetGlobalQuota(
              kStorageTypeTemporary, temporary_storage_quota_)) {
        set_db_disabled(true);
      }
    }
  }

  virtual void DatabaseTaskCompleted() OVERRIDE {
    if (manager()->temporary_global_quota_ < 0)
      manager()->DidGetTemporaryGlobalQuota(temporary_storage_quota_);
  }

 private:
  FilePath profile_path_;
  bool is_incognito_;
  int64 temporary_storage_quota_;
};

class QuotaManager::TemporaryGlobalQuotaUpdateTask
    : public QuotaManager::DatabaseTaskBase {
 public:
  TemporaryGlobalQuotaUpdateTask(
      QuotaManager* manager,
      QuotaDatabase* database,
      scoped_refptr<base::MessageLoopProxy> db_message_loop,
      int64 new_quota,
      QuotaCallback* callback)
      : DatabaseTaskBase(manager, database, db_message_loop),
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
                   new_quota_);
  }

 private:
  int64 new_quota_;
  scoped_ptr<QuotaCallback> callback_;
};

class QuotaManager::PersistentHostQuotaQueryTask
    : public QuotaManager::DatabaseTaskBase {
 public:
  PersistentHostQuotaQueryTask(
      QuotaManager* manager,
      QuotaDatabase* database,
      scoped_refptr<base::MessageLoopProxy> db_message_loop,
      const std::string& host,
      HostQuotaCallback* callback)
      : DatabaseTaskBase(manager, database, db_message_loop),
        host_(host),
        quota_(-1),
        callback_(callback) {
    DCHECK(!host_.empty());
  }
 protected:
  virtual void RunOnTargetThread() OVERRIDE {
    if (!database()->GetHostQuota(host_, kStorageTypePersistent, &quota_))
      quota_ = 0;
  }
  virtual void DatabaseTaskCompleted() OVERRIDE {
    callback_->Run(kQuotaStatusOk, host_, quota_);
  }
 private:
  std::string host_;
  int64 quota_;
  scoped_ptr<HostQuotaCallback> callback_;
};

class QuotaManager::PersistentHostQuotaUpdateTask
    : public QuotaManager::DatabaseTaskBase {
 public:
  PersistentHostQuotaUpdateTask(
      QuotaManager* manager,
      QuotaDatabase* database,
      scoped_refptr<base::MessageLoopProxy> db_message_loop,
      const std::string& host,
      int new_quota,
      HostQuotaCallback* callback)
      : DatabaseTaskBase(manager, database, db_message_loop),
        host_(host),
        new_quota_(new_quota),
        callback_(callback) {
    DCHECK(!host_.empty());
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
                   host_, new_quota_);
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
      QuotaDatabase* database,
      scoped_refptr<base::MessageLoopProxy> db_message_loop,
      StorageType type,
      const std::map<GURL, int>& origins_in_use,
      GetLRUOriginCallback *callback)
      : DatabaseTaskBase(manager, database, db_message_loop),
        type_(type),
        callback_(callback) {
    for (std::map<GURL, int>::const_iterator p = origins_in_use.begin();
         p != origins_in_use.end();
         ++p) {
      if (p->second > 0)
        exceptions_.insert(p->first);
    }
  }

 protected:
  virtual void RunOnTargetThread() OVERRIDE {
    if (!database()->GetLRUOrigin(type_, exceptions_, &url_))
      set_db_disabled(true);
  }

  virtual void DatabaseTaskCompleted() OVERRIDE {
    callback_->Run(url_);
  }

 private:
  StorageType type_;
  std::set<GURL> exceptions_;
  scoped_ptr<GetLRUOriginCallback> callback_;
  GURL url_;
};

class QuotaManager::OriginDeletionDatabaseTask
    : public QuotaManager::DatabaseTaskBase {
 public:
  OriginDeletionDatabaseTask(
      QuotaManager* manager,
      QuotaDatabase* database,
      scoped_refptr<base::MessageLoopProxy> db_message_loop,
      const GURL& origin,
      StorageType type)
      : DatabaseTaskBase(manager, database, db_message_loop),
        origin_(origin),
        type_(type) {}

 protected:
  virtual void RunOnTargetThread() OVERRIDE {
    if (!database()->DeleteOriginLastAccessTime(origin_, type_)) {
      set_db_disabled(true);
    }
  }
  virtual void DatabaseTaskCompleted() OVERRIDE {}

 private:
  GURL origin_;
  StorageType type_;
};

// QuotaManager ---------------------------------------------------------------

QuotaManager::QuotaManager(bool is_incognito,
                           const FilePath& profile_path,
                           base::MessageLoopProxy* io_thread,
                           base::MessageLoopProxy* db_thread)
  : is_incognito_(is_incognito),
    profile_path_(profile_path),
    proxy_(new QuotaManagerProxy(
        ALLOW_THIS_IN_INITIALIZER_LIST(this), io_thread)),
    db_disabled_(false),
    io_thread_(io_thread),
    db_thread_(db_thread),
    temporary_global_quota_(-1),
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

  std::string host = net::GetHostOrSpecFromURL(origin);
  UsageAndQuotaDispatcherTaskMap::iterator found =
      usage_and_quota_dispatchers_.find(std::make_pair(host, type));
  if (found == usage_and_quota_dispatchers_.end()) {
    UsageAndQuotaDispatcherTask* dispatcher =
        UsageAndQuotaDispatcherTask::Create(this, host, type);
    found = usage_and_quota_dispatchers_.insert(
        std::make_pair(std::make_pair(host, type), dispatcher)).first;
  }
  if (found->second->AddCallback(callback.release()))
    found->second->Start();
}

void QuotaManager::RequestQuota(
    const GURL& origin, StorageType type,
    int64 requested_size,
    RequestQuotaCallback* callback) {
  LazyInitialize();
  // TODO(kinuko): implement me.
  callback->Run(kQuotaErrorNotSupported, 0);
  delete callback;
}

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
    callback_->Run(kQuotaErrorAbort, space_);
  }

  virtual void Completed() OVERRIDE {
    callback_->Run(kQuotaStatusOk, space_);
  }

 private:
  FilePath profile_path_;
  int64 space_;
  scoped_ptr<AvailableSpaceCallback> callback_;
};


void QuotaManager::GetAvailableSpace(AvailableSpaceCallback* callback) {
  scoped_refptr<AvailableSpaceQueryTask> task(
      new AvailableSpaceQueryTask(this, db_thread_, profile_path_, callback));
  task->Start();
}

void QuotaManager::GetTemporaryGlobalQuota(QuotaCallback* callback) {
  LazyInitialize();
  if (temporary_global_quota_ >= 0) {
    // TODO(kinuko): The in-memory quota value should be periodically
    // updated not to exceed the current available space in the hard drive.
    callback->Run(kQuotaStatusOk, temporary_global_quota_);
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
    callback->Run(kQuotaErrorInvalidModification, -1);
    delete callback;
    return;
  }

  DidGetTemporaryGlobalQuota(new_quota);
  if (!db_disabled_) {
    scoped_refptr<TemporaryGlobalQuotaUpdateTask> task(
        new TemporaryGlobalQuotaUpdateTask(
            this, database_.get(), db_thread_, new_quota, callback));
    task->Start();
  } else {
    callback->Run(kQuotaErrorInvalidAccess, -1);
    delete callback;
  }
}

void QuotaManager::GetPersistentHostQuota(const std::string& host,
                                          HostQuotaCallback* callback) {
  LazyInitialize();
  scoped_refptr<PersistentHostQuotaQueryTask> task(
      new PersistentHostQuotaQueryTask(
          this, database_.get(), db_thread_, host, callback));
  task->Start();
}

void QuotaManager::SetPersistentHostQuota(const std::string& host,
                                          int64 new_quota,
                                          HostQuotaCallback* callback) {
  LazyInitialize();
  if (new_quota < 0) {
    callback->Run(kQuotaErrorInvalidModification, host, -1);
    delete callback;
    return;
  }

  if (!db_disabled_) {
    scoped_refptr<PersistentHostQuotaUpdateTask> task(
        new PersistentHostQuotaUpdateTask(
            this, database_.get(), db_thread_, host, new_quota, callback));
    task->Start();
  } else {
    callback->Run(kQuotaErrorInvalidAccess, host, -1);
    delete callback;
  }
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
      new UsageTracker(clients_, kStorageTypeTemporary));
  persistent_usage_tracker_.reset(
      new UsageTracker(clients_, kStorageTypePersistent));

  scoped_refptr<InitializeTask> task(
      new InitializeTask(this, database_.get(), db_thread_,
                         profile_path_, is_incognito_));
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
  // TODO(michaeln): write me
}

void QuotaManager::NotifyStorageModified(
    QuotaClient::ID client_id,
    const GURL& origin, StorageType type, int64 delta) {
  LazyInitialize();
  GetUsageTracker(type)->UpdateUsageCache(client_id, origin, delta);
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

void QuotaManager::DidGetTemporaryGlobalQuota(int64 quota) {
  temporary_global_quota_ = quota;
  temporary_global_quota_callbacks_.Run(
      db_disabled_ ? kQuotaErrorInvalidAccess : kQuotaStatusOk,
      quota);
}

// TODO(kinuko): add test for this.
void QuotaManager::DeleteOriginFromDatabase(
    const GURL& origin, StorageType type) {
  LazyInitialize();
  if (db_disabled_)
    return;
  scoped_refptr<OriginDeletionDatabaseTask> task =
      new OriginDeletionDatabaseTask(
          this, database_.get(), db_thread_, origin, type);
  task->Start();
}

void QuotaManager::GetLRUOrigin(
    StorageType type,
    GetLRUOriginCallback* callback) {
  LazyInitialize();
  if (db_disabled_) {
    callback->Run(GURL());
    delete callback;
    return;
  }
  scoped_refptr<GetLRUOriginTask> task(new GetLRUOriginTask(
      this, database_.get(), db_thread_, type, origins_in_use_, callback));
  task->Start();
}

void QuotaManager::DidOriginDataEvicted(
    QuotaStatusCode status) {
  DCHECK(io_thread_->BelongsToCurrentThread());

  if (status != kQuotaStatusOk) {
    ++eviction_context_.num_eviction_error;
    // TODO(dmikurube): The origin with some error should have lower priority in
    // the next eviction?
  }

  ++eviction_context_.num_evicted_clients;
  DCHECK(eviction_context_.num_evicted_clients <=
      eviction_context_.num_eviction_requested_clients);
  if (eviction_context_.num_evicted_clients ==
      eviction_context_.num_eviction_requested_clients) {
    eviction_context_.num_eviction_requested_clients = 0;
    eviction_context_.num_evicted_clients = 0;

    // TODO(dmikurube): Call DeleteOriginFromDatabase here.
    eviction_context_.evict_origin_data_callback->Run(kQuotaStatusOk);
    eviction_context_.evict_origin_data_callback.reset();
  }
}

void QuotaManager::EvictOriginData(
    const GURL& origin,
    StorageType type,
    EvictOriginDataCallback* callback) {
  DCHECK(io_thread_->BelongsToCurrentThread());
  DCHECK(database_.get());
  DCHECK(eviction_context_.num_eviction_requested_clients == 0);
  DCHECK(type == kStorageTypeTemporary);

  int num_clients = clients_.size();

  if (origin.is_empty() || num_clients == 0) {
    callback->Run(kQuotaStatusOk);
    delete callback;
    return;
  }

  eviction_context_.num_eviction_requested_clients = num_clients;
  eviction_context_.num_evicted_clients = 0;

  eviction_context_.evict_origin_data_callback.reset(callback);
  for (QuotaClientList::iterator p = clients_.begin();
       p != clients_.end();
       ++p) {
    (*p)->DeleteOriginData(origin, type, callback_factory_.
        NewCallback(&QuotaManager::DidOriginDataEvicted));
  }
}

void QuotaManager::DidGetAvailableSpaceForEviction(
    QuotaStatusCode status,
    int64 available_space) {
  eviction_context_.get_usage_and_quota_callback->Run(status,
      eviction_context_.usage, eviction_context_.quota, available_space);
  eviction_context_.get_usage_and_quota_callback.reset();
}

void QuotaManager::DidGetGlobalQuotaForEviction(
    QuotaStatusCode status,
    int64 quota) {
  if (status != kQuotaStatusOk) {
    eviction_context_.get_usage_and_quota_callback->Run(status,
        eviction_context_.usage, quota, 0);
    eviction_context_.get_usage_and_quota_callback.reset();
    return;
  }

  eviction_context_.quota = quota;
  GetAvailableSpace(callback_factory_.
      NewCallback(&QuotaManager::DidGetAvailableSpaceForEviction));
}

void QuotaManager::DidGetGlobalUsageForEviction(int64 usage) {
  eviction_context_.usage = usage;
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

void QuotaManager::DeleteOnCorrectThread() const {
  if (!io_thread_->BelongsToCurrentThread()) {
    io_thread_->DeleteSoon(FROM_HERE, this);
    return;
  }
  delete this;
}

void QuotaManager::GetGlobalUsage(
    StorageType type,
    UsageCallback* callback) {
  LazyInitialize();
  GetUsageTracker(type)->GetGlobalUsage(callback);
}

void QuotaManager::GetHostUsage(const std::string& host, StorageType type,
                                HostUsageCallback* callback) {
  LazyInitialize();
  GetUsageTracker(type)->GetHostUsage(host, callback);
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
