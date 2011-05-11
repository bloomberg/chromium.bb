// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_QUOTA_QUOTA_MANAGER_H_
#define WEBKIT_QUOTA_QUOTA_MANAGER_H_
#pragma once

#include <deque>
#include <list>
#include <map>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/file_path.h"
#include "base/memory/scoped_callback_factory.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/ref_counted.h"
#include "webkit/quota/quota_client.h"
#include "webkit/quota/quota_task.h"
#include "webkit/quota/quota_types.h"

class FilePath;

namespace quota {

class QuotaDatabase;
class UsageTracker;

struct QuotaManagerDeleter;
class QuotaManagerProxy;

// The quota manager class.  This class is instantiated per profile and
// held by the profile.  With the exception of the constructor and the
// proxy() method, all methods should only be called on the IO thread.
class QuotaManager : public QuotaTaskObserver,
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
               scoped_refptr<base::MessageLoopProxy> io_thread,
               scoped_refptr<base::MessageLoopProxy> db_thread);

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

  const static int64 kTemporaryStorageQuotaDefaultSize;
  const static int64 kTemporaryStorageQuotaMaxSize;
  const static char kDatabaseName[];

  const static int64 kIncognitoDefaultTemporaryQuota;

 private:
  class InitializeTask;
  class TemporaryGlobalQuotaUpdateTask;
  class PersistentHostQuotaUpdateTask;
  class PersistentHostQuotaQueryTask;

  class UsageAndQuotaDispatcherTask;
  class UsageAndQuotaDispatcherTaskForTemporary;
  class UsageAndQuotaDispatcherTaskForPersistent;

  class GlobalUsageQueryTask;
  class HostUsageQueryTask;

  typedef std::pair<std::string, StorageType> HostAndType;
  typedef std::map<HostAndType, UsageAndQuotaDispatcherTask*>
      UsageAndQuotaDispatcherTaskMap;

  friend struct QuotaManagerDeleter;
  friend class QuotaManagerProxy;

  // This initialization method is lazily called on the IO thread
  // when the first quota manager API is called.
  // Initialize must be called after all quota clients are added to the
  // manager by RegisterStorage.
  void LazyInitialize();

  // Called by clients via proxy.
  // Registers a quota client to the manager.
  // The client must remain valid until OnQuotaManagerDestored is called.
  void RegisterClient(QuotaClient* client);

  // Called by clients via proxy.
  // QuotaClients must call this method whenever they have made any
  // modifications that change the amount of data stored in their storage.
  void NotifyStorageModified(QuotaClient::ID client_id,
                             const GURL& origin,
                             StorageType type,
                             int64 delta);

  UsageTracker* GetUsageTracker(StorageType type) const;

  void DidGetTemporaryGlobalQuota(int64 quota);

  void DeleteOnCorrectThread() const;

  const bool is_incognito_;
  const FilePath profile_path_;

  scoped_refptr<QuotaManagerProxy> proxy_;
  bool db_initialized_;
  bool db_disabled_;
  scoped_refptr<base::MessageLoopProxy> io_thread_;
  scoped_refptr<base::MessageLoopProxy> db_thread_;
  mutable scoped_ptr<QuotaDatabase> database_;

  QuotaClientList clients_;

  scoped_ptr<UsageTracker> temporary_usage_tracker_;
  scoped_ptr<UsageTracker> persistent_usage_tracker_;

  UsageAndQuotaDispatcherTaskMap usage_and_quota_dispatchers_;

  int64 temporary_global_quota_;
  QuotaCallbackQueue temporary_global_quota_callbacks_;

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
  void GetUsageAndQuota(const GURL& origin,
                        StorageType type,
                        QuotaManager::GetUsageAndQuotaCallback* callback);
  void RegisterClient(QuotaClient* client);
  void NotifyStorageModified(QuotaClient::ID client_id,
                            const GURL& origin,
                            StorageType type,
                            int64 delta);
 private:
  friend class QuotaManager;
  friend class base::RefCountedThreadSafe<QuotaManagerProxy>;
  QuotaManagerProxy(QuotaManager* manager, base::MessageLoopProxy* io_thread);
  ~QuotaManagerProxy();

  QuotaManager* manager_;  // only accessed on the io thread
  scoped_refptr<base::MessageLoopProxy> io_thread_;

  DISALLOW_COPY_AND_ASSIGN(QuotaManagerProxy);
};


}  // namespace quota

#endif  // WEBKIT_QUOTA_QUOTA_MANAGER_H_
