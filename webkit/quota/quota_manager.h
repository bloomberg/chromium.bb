// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_QUOTA_QUOTA_MANAGER_H_
#define WEBKIT_QUOTA_QUOTA_MANAGER_H_
#pragma once

#include <deque>
#include <list>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner_helpers.h"
#include "webkit/quota/quota_client.h"
#include "webkit/quota/quota_database.h"
#include "webkit/quota/quota_task.h"
#include "webkit/quota/quota_types.h"
#include "webkit/quota/special_storage_policy.h"

class FilePath;

namespace base {
class SequencedTaskRunner;
class SingleThreadTaskRunner;
}

namespace quota_internals {
class QuotaInternalsProxy;
}

namespace quota {

struct QuotaManagerDeleter;

class QuotaDatabase;
class QuotaManagerProxy;
class QuotaTemporaryStorageEvictor;
class UsageTracker;
class MockQuotaManager;

struct QuotaAndUsage {
  int64 usage;
  int64 unlimited_usage;
  int64 quota;
  int64 available_disk_space;
};

// An interface called by QuotaTemporaryStorageEvictor.
class QuotaEvictionHandler {
 public:
  typedef base::Callback<void(const GURL&)> GetLRUOriginCallback;
  typedef StatusCallback EvictOriginDataCallback;
  typedef base::Callback<void(QuotaStatusCode,
                              const QuotaAndUsage& quota_and_usage)>
      GetUsageAndQuotaForEvictionCallback;

  virtual ~QuotaEvictionHandler() {}

  // Returns the least recently used origin.  It might return empty
  // GURL when there are no evictable origins.
  virtual void GetLRUOrigin(
      StorageType type,
      const GetLRUOriginCallback& callback) = 0;

  virtual void EvictOriginData(
      const GURL& origin,
      StorageType type,
      const EvictOriginDataCallback& callback) = 0;

  virtual void GetUsageAndQuotaForEviction(
      const GetUsageAndQuotaForEvictionCallback& callback) = 0;
};

struct UsageInfo {
  UsageInfo(const std::string& host, StorageType type, int64 usage)
      : host(host),
        type(type),
        usage(usage) {}
  std::string host;
  StorageType type;
  int64 usage;
};

// The quota manager class.  This class is instantiated per profile and
// held by the profile.  With the exception of the constructor and the
// proxy() method, all methods should only be called on the IO thread.
class QuotaManager : public QuotaTaskObserver,
                     public QuotaEvictionHandler,
                     public base::RefCountedThreadSafe<
                         QuotaManager, QuotaManagerDeleter> {
 public:
  typedef base::Callback<void(QuotaStatusCode,
                              int64 /* usage */,
                              int64 /* quota */)>
      GetUsageAndQuotaCallback;

  QuotaManager(bool is_incognito,
               const FilePath& profile_path,
               base::SingleThreadTaskRunner* io_thread,
               base::SequencedTaskRunner* db_thread,
               SpecialStoragePolicy* special_storage_policy);

  // Returns a proxy object that can be used on any thread.
  QuotaManagerProxy* proxy() { return proxy_.get(); }

  // Called by clients or webapps. Returns usage per host.
  void GetUsageInfo(const GetUsageInfoCallback& callback);

  // Called by clients or webapps.
  // This method is declared as virtual to allow test code to override it.
  // note: returns host usage and quota
  virtual void GetUsageAndQuota(const GURL& origin,
                                StorageType type,
                                const GetUsageAndQuotaCallback& callback);

  // Called by clients via proxy.
  // Client storage should call this method when storage is accessed.
  // Used to maintain LRU ordering.
  void NotifyStorageAccessed(QuotaClient::ID client_id,
                             const GURL& origin,
                             StorageType type);

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

  // DeleteOriginData and DeleteHostData (surprisingly enough) delete data of a
  // particular StorageType associated with either a specific origin or set of
  // origins. Each method additionally requires a |quota_client_mask| which
  // specifies the types of QuotaClients to delete from the origin. This is
  // specified by the caller as a bitmask built from QuotaClient::IDs. Setting
  // the mask to QuotaClient::kAllClientsMask will remove all clients from the
  // origin, regardless of type.
  virtual void DeleteOriginData(const GURL& origin,
                                StorageType type,
                                int quota_client_mask,
                                const StatusCallback& callback);
  void DeleteHostData(const std::string& host,
                      StorageType type,
                      int quota_client_mask,
                      const StatusCallback& callback);

  // Called by UI and internal modules.
  void GetAvailableSpace(const AvailableSpaceCallback& callback);
  void GetTemporaryGlobalQuota(const QuotaCallback& callback);

  // Ok to call with NULL callback.
  void SetTemporaryGlobalOverrideQuota(int64 new_quota,
                                       const QuotaCallback& callback);

  void GetPersistentHostQuota(const std::string& host,
                              const HostQuotaCallback& callback);
  void SetPersistentHostQuota(const std::string& host,
                              int64 new_quota,
                              const HostQuotaCallback& callback);
  void GetGlobalUsage(StorageType type, const GlobalUsageCallback& callback);
  void GetHostUsage(const std::string& host, StorageType type,
                    const HostUsageCallback& callback);

  void GetStatistics(std::map<std::string, std::string>* statistics);

  bool IsStorageUnlimited(const GURL& origin) const {
    return special_storage_policy_.get() &&
           special_storage_policy_->IsStorageUnlimited(origin);
  }

  virtual void GetOriginsModifiedSince(StorageType type,
                                       base::Time modified_since,
                                       const GetOriginsCallback& callback);

  bool ResetUsageTracker(StorageType type);

  // Determines the portion of the temp pool that can be
  // utilized by a single host (ie. 5 for 20%).
  static const int kPerHostTemporaryPortion;

  static const char kDatabaseName[];

  static const int kThresholdOfErrorsToBeBlacklisted;

  static const int kEvictionIntervalInMilliSeconds;

 protected:
  virtual ~QuotaManager();

 private:
  friend class base::DeleteHelper<QuotaManager>;
  friend class MockQuotaManager;
  friend class MockStorageClient;
  friend class quota_internals::QuotaInternalsProxy;
  friend class QuotaManagerProxy;
  friend class QuotaManagerTest;
  friend class QuotaTemporaryStorageEvictor;
  friend struct QuotaManagerDeleter;

  class DatabaseTaskBase;
  class InitializeTask;
  class UpdateTemporaryQuotaOverrideTask;
  class GetPersistentHostQuotaTask;
  class UpdatePersistentHostQuotaTask;
  class GetLRUOriginTask;
  class DeleteOriginInfo;
  class InitializeTemporaryOriginsInfoTask;
  class UpdateAccessTimeTask;
  class UpdateModifiedTimeTask;
  class GetModifiedSinceTask;

  class GetUsageInfoTask;
  class UsageAndQuotaDispatcherTask;
  class UsageAndQuotaDispatcherTaskForTemporary;
  class UsageAndQuotaDispatcherTaskForPersistent;
  class UsageAndQuotaDispatcherTaskForTemporaryGlobal;

  class OriginDataDeleter;
  class HostDataDeleter;

  class AvailableSpaceQueryTask;
  class DumpQuotaTableTask;
  class DumpOriginInfoTableTask;

  typedef QuotaDatabase::QuotaTableEntry QuotaTableEntry;
  typedef QuotaDatabase::OriginInfoTableEntry OriginInfoTableEntry;
  typedef std::vector<QuotaTableEntry> QuotaTableEntries;
  typedef std::vector<OriginInfoTableEntry> OriginInfoTableEntries;

  typedef base::Callback<void(const QuotaTableEntries&)>
      DumpQuotaTableCallback;
  typedef base::Callback<void(const OriginInfoTableEntries&)>
      DumpOriginInfoTableCallback;

  struct EvictionContext {
    EvictionContext() : evicted_type(kStorageTypeUnknown) {}
    virtual ~EvictionContext() {}
    GURL evicted_origin;
    StorageType evicted_type;

    EvictOriginDataCallback evict_origin_data_callback;
  };

  typedef std::pair<std::string, StorageType> HostAndType;
  typedef std::map<HostAndType, UsageAndQuotaDispatcherTask*>
      UsageAndQuotaDispatcherTaskMap;

  typedef QuotaEvictionHandler::GetUsageAndQuotaForEvictionCallback
      UsageAndQuotaDispatcherCallback;

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

  // Extract cached origins list from the usage tracker.
  // (Might return empty list if no origin is tracked by the tracker.)
  void GetCachedOrigins(StorageType type, std::set<GURL>* origins);

  // These internal methods are separately defined mainly for testing.
  void NotifyStorageAccessedInternal(
      QuotaClient::ID client_id,
      const GURL& origin,
      StorageType type,
      base::Time accessed_time);
  void NotifyStorageModifiedInternal(
      QuotaClient::ID client_id,
      const GURL& origin,
      StorageType type,
      int64 delta,
      base::Time modified_time);

  // |origin| can be empty if |global| is true.
  void GetUsageAndQuotaInternal(
      const GURL& origin,
      StorageType type,
      bool global,
      const UsageAndQuotaDispatcherCallback& callback);

  void DumpQuotaTable(const DumpQuotaTableCallback& callback);
  void DumpOriginInfoTable(const DumpOriginInfoTableCallback& callback);

  // Methods for eviction logic.
  void StartEviction();
  void DeleteOriginFromDatabase(const GURL& origin, StorageType type);

  void DidOriginDataEvicted(QuotaStatusCode status);
  void DidGetGlobalUsageAndQuotaForEviction(QuotaStatusCode status,
                                            StorageType type,
                                            int64 usage,
                                            int64 unlimited_usage,
                                            int64 quota,
                                            int64 available_space);

  void ReportHistogram();
  void DidGetTemporaryGlobalUsageForHistogram(StorageType type,
                                              int64 usage,
                                              int64 unlimited_usage);
  void DidGetPersistentGlobalUsageForHistogram(StorageType type,
                                               int64 usage,
                                               int64 unlimited_usage);

  // QuotaEvictionHandler.
  virtual void GetLRUOrigin(
      StorageType type,
      const GetLRUOriginCallback& callback) OVERRIDE;
  virtual void EvictOriginData(
      const GURL& origin,
      StorageType type,
      const EvictOriginDataCallback& callback) OVERRIDE;
  virtual void GetUsageAndQuotaForEviction(
      const GetUsageAndQuotaForEvictionCallback& callback) OVERRIDE;

  void DidRunInitializeTask();
  void DidGetInitialTemporaryGlobalQuota(QuotaStatusCode status,
                                         StorageType type,
                                         int64 quota_unused);
  void DidGetDatabaseLRUOrigin(const GURL& origin);

  void DeleteOnCorrectThread() const;

  const bool is_incognito_;
  const FilePath profile_path_;

  scoped_refptr<QuotaManagerProxy> proxy_;
  bool db_disabled_;
  bool eviction_disabled_;
  scoped_refptr<base::SingleThreadTaskRunner> io_thread_;
  scoped_refptr<base::SequencedTaskRunner> db_thread_;
  mutable scoped_ptr<QuotaDatabase> database_;

  GetLRUOriginCallback lru_origin_callback_;
  std::set<GURL> access_notified_origins_;

  QuotaClientList clients_;

  scoped_ptr<UsageTracker> temporary_usage_tracker_;
  scoped_ptr<UsageTracker> persistent_usage_tracker_;
  // TODO(michaeln): Need a way to clear the cache, drop and
  // reinstantiate the trackers when they're not handling requests.

  scoped_ptr<QuotaTemporaryStorageEvictor> temporary_storage_evictor_;
  EvictionContext eviction_context_;

  UsageAndQuotaDispatcherTaskMap usage_and_quota_dispatchers_;

  bool temporary_quota_initialized_;
  int64 temporary_quota_override_;

  int64 desired_available_space_;

  // Map from origin to count.
  std::map<GURL, int> origins_in_use_;
  // Map from origin to error count.
  std::map<GURL, int> origins_in_error_;

  scoped_refptr<SpecialStoragePolicy> special_storage_policy_;

  base::WeakPtrFactory<QuotaManager> weak_factory_;
  base::RepeatingTimer<QuotaManager> histogram_timer_;

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

  QuotaManagerProxy(QuotaManager* manager,
                    base::SingleThreadTaskRunner* io_thread);
  virtual ~QuotaManagerProxy();

  QuotaManager* manager_;  // only accessed on the io thread
  scoped_refptr<base::SingleThreadTaskRunner> io_thread_;

  DISALLOW_COPY_AND_ASSIGN(QuotaManagerProxy);
};

}  // namespace quota

#endif  // WEBKIT_QUOTA_QUOTA_MANAGER_H_
