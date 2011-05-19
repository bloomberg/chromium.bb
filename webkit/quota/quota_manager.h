// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_QUOTA_QUOTA_MANAGER_H_
#define WEBKIT_QUOTA_QUOTA_MANAGER_H_
#pragma once

#include <deque>
#include <list>
#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_callback_factory.h"
#include "base/memory/scoped_ptr.h"
#include "webkit/quota/quota_client.h"
#include "webkit/quota/quota_task.h"
#include "webkit/quota/quota_types.h"

namespace base {
class MessageLoopProxy;
}
class FilePath;

namespace quota {

class QuotaDatabase;
class UsageTracker;

struct QuotaManagerDeleter;
class QuotaManagerProxy;

// An interface called by QuotaTemporaryStorageEvictor.
class QuotaEvictionHandler {
 public:
  typedef Callback1<GURL>::Type GetLRUOriginCallback;
  typedef Callback1<QuotaStatusCode>::Type EvictOriginDataCallback;
  typedef Callback4<QuotaStatusCode,
                    int64 /* usage */,
                    int64 /* quota */,
                    int64 /* physical_available */ >::Type
      GetUsageAndQuotaForEvictionCallback;

  virtual void GetLRUOrigin(
      StorageType type,
      GetLRUOriginCallback* callback) = 0;

  virtual void EvictOriginData(
      const GURL& origin,
      StorageType type,
      EvictOriginDataCallback* callback) = 0;

  virtual void GetUsageAndQuotaForEviction(
      GetUsageAndQuotaForEvictionCallback* callback) = 0;

 protected:
  virtual ~QuotaEvictionHandler() {}
};

// The quota manager class.  This class is instantiated per profile and
// held by the profile.  With the exception of the constructor and the
// proxy() method, all methods should only be called on the IO thread.
class QuotaManager : public QuotaTaskObserver,
                     public QuotaEvictionHandler,
                     public base::RefCountedThreadSafe<
                         QuotaManager, QuotaManagerDeleter> {
 public:
  typedef Callback3<QuotaStatusCode,
                    int64 /* usage */,
                    int64 /* quota */>::Type GetUsageAndQuotaCallback;
  typedef Callback2<QuotaStatusCode,
                    int64 /* granted_quota */>::Type RequestQuotaCallback;

  QuotaManager(bool is_incognito,
               const FilePath& profile_path,
               base::MessageLoopProxy* io_thread,
               base::MessageLoopProxy* db_thread);

  virtual ~QuotaManager();

  // Returns a proxy object that can be used on any thread.
  QuotaManagerProxy* proxy() { return proxy_.get(); }

  // Called by clients or webapps.
  // This method is declared as virtual to allow test code to override it.
  virtual void GetUsageAndQuota(const GURL& origin,
                                StorageType type,
                                GetUsageAndQuotaCallback* callback);

  // Called by webapps.
  void RequestQuota(const GURL& origin,
                    StorageType type,
                    int64 requested_size,
                    RequestQuotaCallback* callback);

  // Called by clients via proxy.
  // Client storage should call this method when storage is accessed.
  // Used to maintain LRU ordering.
  void NotifyStorageAccessed(QuotaClient::ID client_id,
                             const GURL& origin,
                             StorageType typea);

  // Called by clients via proxy.
  // Client storage must call this method whenever they have made any
  // modifications that change the amount of data stored in their storage.
  void NotifyStorageModified(QuotaClient::ID client_id,
                             const GURL& origin,
                             StorageType type,
                             int64 delta);

  // Used to avoid evicting origins with open pages.
  // A call to NotifyOriginInUse must be balanced by a later call
  // to NotifyOriginNoLongerInUse.
  void NotifyOriginInUse(const GURL& origin);
  void NotifyOriginNoLongerInUse(const GURL& origin);
  bool IsOriginInUse(const GURL& origin) const {
    return origins_in_use_.find(origin) != origins_in_use_.end();
  }

  void GetAvailableSpace(AvailableSpaceCallback* callback);

  // Called by UI and internal modules.
  void GetTemporaryGlobalQuota(QuotaCallback* callback);
  void SetTemporaryGlobalQuota(int64 new_quota, QuotaCallback* callback);
  void GetPersistentHostQuota(const std::string& host,
                              HostQuotaCallback* callback);
  void SetPersistentHostQuota(const std::string& host,
                              int64 new_quota,
                              HostQuotaCallback* callback);

  void GetGlobalUsage(StorageType type, UsageCallback* callback);
  void GetHostUsage(const std::string& host, StorageType type,
                    HostUsageCallback* callback);

  static const int64 kTemporaryStorageQuotaDefaultSize;
  static const int64 kTemporaryStorageQuotaMaxSize;
  static const char kDatabaseName[];

  static const int64 kIncognitoDefaultTemporaryQuota;

 private:
  class DatabaseTaskBase;
  class InitializeTask;
  class TemporaryGlobalQuotaUpdateTask;
  class PersistentHostQuotaUpdateTask;
  class PersistentHostQuotaQueryTask;
  class GetLRUOriginTask;
  class OriginDeletionDatabaseTask;

  class UsageAndQuotaDispatcherTask;
  class UsageAndQuotaDispatcherTaskForTemporary;
  class UsageAndQuotaDispatcherTaskForPersistent;

  class AvailableSpaceQueryTask;

  struct EvictionContext {
    EvictionContext()
        : num_eviction_requested_clients(0),
          num_evicted_clients(0),
          num_eviction_error(0),
          usage(0),
          quota(0) {}
    virtual ~EvictionContext() {}

    scoped_ptr<EvictOriginDataCallback> evict_origin_data_callback;
    int num_eviction_requested_clients;
    int num_evicted_clients;
    int num_eviction_error;

    scoped_ptr<GetUsageAndQuotaForEvictionCallback>
        get_usage_and_quota_callback;
    int64 usage;
    int64 quota;
  };

  typedef std::pair<std::string, StorageType> HostAndType;
  typedef std::map<HostAndType, UsageAndQuotaDispatcherTask*>
      UsageAndQuotaDispatcherTaskMap;

  friend struct QuotaManagerDeleter;
  friend class QuotaManagerProxy;
  friend class QuotaManagerTest;

  // This initialization method is lazily called on the IO thread
  // when the first quota manager API is called.
  // Initialize must be called after all quota clients are added to the
  // manager by RegisterStorage.
  void LazyInitialize();

  // Called by clients via proxy.
  // Registers a quota client to the manager.
  // The client must remain valid until OnQuotaManagerDestored is called.
  void RegisterClient(QuotaClient* client);

  UsageTracker* GetUsageTracker(StorageType type) const;

  void DidGetTemporaryGlobalQuota(int64 quota);

  // Methods for eviction logic.
  void DeleteOriginFromDatabase(const GURL& origin, StorageType type);

  void DidOriginDataEvicted(QuotaStatusCode status);
  void DidGetAvailableSpaceForEviction(
      QuotaStatusCode status,
      int64 available_space);
  void DidGetGlobalQuotaForEviction(
      QuotaStatusCode status,
      int64 quota);
  void DidGetGlobalUsageForEviction(int64 usage);

  virtual void GetLRUOrigin(
      StorageType type,
      GetLRUOriginCallback* callback) OVERRIDE;
  virtual void EvictOriginData(
      const GURL& origin,
      StorageType type,
      EvictOriginDataCallback* callback) OVERRIDE;
  virtual void GetUsageAndQuotaForEviction(
      GetUsageAndQuotaForEvictionCallback* callback) OVERRIDE;

  void DeleteOnCorrectThread() const;

  const bool is_incognito_;
  const FilePath profile_path_;

  scoped_refptr<QuotaManagerProxy> proxy_;
  bool db_disabled_;
  scoped_refptr<base::MessageLoopProxy> io_thread_;
  scoped_refptr<base::MessageLoopProxy> db_thread_;
  mutable scoped_ptr<QuotaDatabase> database_;

  QuotaClientList clients_;

  scoped_ptr<UsageTracker> temporary_usage_tracker_;
  scoped_ptr<UsageTracker> persistent_usage_tracker_;

  EvictionContext eviction_context_;

  UsageAndQuotaDispatcherTaskMap usage_and_quota_dispatchers_;

  int64 temporary_global_quota_;
  QuotaCallbackQueue temporary_global_quota_callbacks_;

  // Map from origin to count.
  std::map<GURL, int> origins_in_use_;

  base::ScopedCallbackFactory<QuotaManager> callback_factory_;

  DISALLOW_COPY_AND_ASSIGN(QuotaManager);
};

struct QuotaManagerDeleter {
  static void Destruct(const QuotaManager* manager) {
    manager->DeleteOnCorrectThread();
  }
};

// The proxy may be called and finally released on any thread.
class QuotaManagerProxy
    : public base::RefCountedThreadSafe<QuotaManagerProxy> {
 public:
  virtual void RegisterClient(QuotaClient* client);
  virtual void NotifyStorageAccessed(QuotaClient::ID client_id,
                                     const GURL& origin,
                                     StorageType type);
  virtual void NotifyStorageModified(QuotaClient::ID client_id,
                                     const GURL& origin,
                                     StorageType type,
                                     int64 delta);
  virtual void NotifyOriginInUse(const GURL& origin);
  virtual void NotifyOriginNoLongerInUse(const GURL& origin);

  // This method may only be called on the IO thread.
  // It may return NULL if the manager has already been deleted.
  QuotaManager* quota_manager() const;

 protected:
  friend class QuotaManager;
  friend class base::RefCountedThreadSafe<QuotaManagerProxy>;

  QuotaManagerProxy(QuotaManager* manager, base::MessageLoopProxy* io_thread);
  virtual ~QuotaManagerProxy();

  QuotaManager* manager_;  // only accessed on the io thread
  scoped_refptr<base::MessageLoopProxy> io_thread_;

  DISALLOW_COPY_AND_ASSIGN(QuotaManagerProxy);
};


}  // namespace quota

#endif  // WEBKIT_QUOTA_QUOTA_MANAGER_H_
