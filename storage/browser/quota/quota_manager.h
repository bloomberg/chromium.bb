// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_QUOTA_QUOTA_MANAGER_H_
#define STORAGE_BROWSER_QUOTA_QUOTA_MANAGER_H_

#include <stdint.h>

#include <deque>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/sequenced_task_runner_helpers.h"
#include "storage/browser/quota/quota_callbacks.h"
#include "storage/browser/quota/quota_client.h"
#include "storage/browser/quota/quota_database.h"
#include "storage/browser/quota/quota_settings.h"
#include "storage/browser/quota/quota_task.h"
#include "storage/browser/quota/special_storage_policy.h"
#include "storage/browser/quota/storage_observer.h"
#include "storage/browser/storage_browser_export.h"

namespace base {
class FilePath;
class SequencedTaskRunner;
class SingleThreadTaskRunner;
class TaskRunner;
}

namespace quota_internals {
class QuotaInternalsProxy;
}

namespace content {
class MockQuotaManager;
class MockStorageClient;
class QuotaManagerTest;
class StorageMonitorTest;

}

namespace storage {

class QuotaDatabase;
class QuotaManagerProxy;
class QuotaTemporaryStorageEvictor;
class StorageMonitor;
class UsageTracker;

struct QuotaManagerDeleter;

// An interface called by QuotaTemporaryStorageEvictor.
class STORAGE_EXPORT QuotaEvictionHandler {
 public:
  using EvictionRoundInfoCallback =
      base::Callback<void(QuotaStatusCode status,
                          const QuotaSettings& settings,
                          int64_t available_space,
                          int64_t total_space,
                          int64_t global_usage,
                          bool global_usage_is_complete)>;

  // Called at the beginning of an eviction round to gather the info about
  // the current settings, capacity, and usage.
  virtual void GetEvictionRoundInfo(
      const EvictionRoundInfoCallback& callback) = 0;

  // Returns next origin to evict.  It might return an empty GURL when there are
  // no evictable origins.
  virtual void GetEvictionOrigin(StorageType type,
                                 const std::set<GURL>& extra_exceptions,
                                 int64_t global_quota,
                                 const GetOriginCallback& callback) = 0;

  // Called to evict an origin.
  virtual void EvictOriginData(const GURL& origin,
                               StorageType type,
                               const StatusCallback& callback) = 0;

 protected:
  virtual ~QuotaEvictionHandler() {}
};

struct UsageInfo {
  UsageInfo(const std::string& host, StorageType type, int64_t usage)
      : host(host), type(type), usage(usage) {}
  std::string host;
  StorageType type;
  int64_t usage;
};

// The quota manager class.  This class is instantiated per profile and
// held by the profile.  With the exception of the constructor and the
// proxy() method, all methods should only be called on the IO thread.
class STORAGE_EXPORT QuotaManager
    : public QuotaTaskObserver,
      public QuotaEvictionHandler,
      public base::RefCountedThreadSafe<QuotaManager, QuotaManagerDeleter> {
 public:
  typedef base::Callback<
      void(QuotaStatusCode, int64_t /* usage */, int64_t /* quota */)>
      UsageAndQuotaCallback;

  static const int64_t kNoLimit;

  QuotaManager(
      bool is_incognito,
      const base::FilePath& profile_path,
      const scoped_refptr<base::SingleThreadTaskRunner>& io_thread,
      const scoped_refptr<base::SequencedTaskRunner>& db_thread,
      const scoped_refptr<SpecialStoragePolicy>& special_storage_policy,
      const GetQuotaSettingsFunc& get_settings_function);

  const QuotaSettings& settings() const { return settings_; }
  void SetQuotaSettings(const QuotaSettings& settings);

  // Returns a proxy object that can be used on any thread.
  QuotaManagerProxy* proxy() { return proxy_.get(); }

  // Called by clients or webapps. Returns usage per host.
  void GetUsageInfo(const GetUsageInfoCallback& callback);

  // Called by Web Apps.
  // This method is declared as virtual to allow test code to override it.
  virtual void GetUsageAndQuotaForWebApps(
      const GURL& origin,
      StorageType type,
      const UsageAndQuotaCallback& callback);

  // Called by StorageClients.
  // This method is declared as virtual to allow test code to override it.
  //
  // For UnlimitedStorage origins, this version skips usage and quota handling
  // to avoid extra query cost.
  // Do not call this method for apps/user-facing code.
  virtual void GetUsageAndQuota(const GURL& origin,
                                StorageType type,
                                const UsageAndQuotaCallback& callback);

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
                             int64_t delta);

  // Used to avoid evicting origins with open pages.
  // A call to NotifyOriginInUse must be balanced by a later call
  // to NotifyOriginNoLongerInUse.
  void NotifyOriginInUse(const GURL& origin);
  void NotifyOriginNoLongerInUse(const GURL& origin);
  bool IsOriginInUse(const GURL& origin) const {
    return origins_in_use_.find(origin) != origins_in_use_.end();
  }

  void SetUsageCacheEnabled(QuotaClient::ID client_id,
                            const GURL& origin,
                            StorageType type,
                            bool enabled);

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
  void GetPersistentHostQuota(const std::string& host,
                              const QuotaCallback& callback);
  void SetPersistentHostQuota(const std::string& host,
                              int64_t new_quota,
                              const QuotaCallback& callback);
  void GetGlobalUsage(StorageType type, const GlobalUsageCallback& callback);
  void GetHostUsage(const std::string& host, StorageType type,
                    const UsageCallback& callback);
  void GetHostUsage(const std::string& host, StorageType type,
                    QuotaClient::ID client_id,
                    const UsageCallback& callback);

  bool IsTrackingHostUsage(StorageType type, QuotaClient::ID client_id) const;

  void GetStatistics(std::map<std::string, std::string>* statistics);

  bool IsStorageUnlimited(const GURL& origin, StorageType type) const;

  virtual void GetOriginsModifiedSince(StorageType type,
                                       base::Time modified_since,
                                       const GetOriginsCallback& callback);

  bool ResetUsageTracker(StorageType type);

  // Used to register/deregister observers that wish to monitor storage events.
  void AddStorageObserver(StorageObserver* observer,
                          const StorageObserver::MonitorParams& params);
  void RemoveStorageObserver(StorageObserver* observer);
  void RemoveStorageObserverForFilter(StorageObserver* observer,
                                      const StorageObserver::Filter& filter);

  static const int64_t kPerHostPersistentQuotaLimit;
  static const char kDatabaseName[];
  static const int kThresholdOfErrorsToBeBlacklisted;
  static const int kEvictionIntervalInMilliSeconds;
  static const char kDaysBetweenRepeatedOriginEvictionsHistogram[];
  static const char kEvictedOriginAccessedCountHistogram[];
  static const char kEvictedOriginDaysSinceAccessHistogram[];

  // Kept non-const so that test code can change the value.
  // TODO(kinuko): Make this a real const value and add a proper way to set
  // the quota for syncable storage. (http://crbug.com/155488)
  static int64_t kSyncableStorageDefaultHostQuota;

 protected:
  ~QuotaManager() override;

 private:
  friend class base::DeleteHelper<QuotaManager>;
  friend class base::RefCountedThreadSafe<QuotaManager, QuotaManagerDeleter>;
  friend class content::QuotaManagerTest;
  friend class content::StorageMonitorTest;
  friend class content::MockQuotaManager;
  friend class content::MockStorageClient;
  friend class quota_internals::QuotaInternalsProxy;
  friend class QuotaManagerProxy;
  friend class QuotaTemporaryStorageEvictor;
  friend struct QuotaManagerDeleter;

  class EvictionRoundInfoHelper;
  class UsageAndQuotaHelper;
  class GetUsageInfoTask;
  class OriginDataDeleter;
  class HostDataDeleter;
  class GetModifiedSinceHelper;
  class DumpQuotaTableHelper;
  class DumpOriginInfoTableHelper;

  typedef QuotaDatabase::QuotaTableEntry QuotaTableEntry;
  typedef QuotaDatabase::OriginInfoTableEntry OriginInfoTableEntry;
  typedef std::vector<QuotaTableEntry> QuotaTableEntries;
  typedef std::vector<OriginInfoTableEntry> OriginInfoTableEntries;

  using QuotaSettingsCallback = base::Callback<void(const QuotaSettings&)>;

  // Function pointer type used to store the function which returns
  // information about the volume containing the given FilePath.
  // The value returned is std::tuple<total_space, available_space>.
  using GetVolumeInfoFn =
      std::tuple<int64_t, int64_t> (*)(const base::FilePath&);

  typedef base::Callback<void(const QuotaTableEntries&)>
      DumpQuotaTableCallback;
  typedef base::Callback<void(const OriginInfoTableEntries&)>
      DumpOriginInfoTableCallback;

  typedef CallbackQueue<base::Closure> ClosureQueue;
  typedef CallbackQueueMap<QuotaCallback, std::string, QuotaStatusCode, int64_t>
      HostQuotaCallbackMap;
  using QuotaSettingsCallbackQueue =
      CallbackQueue<QuotaSettingsCallback, const QuotaSettings&>;

  // The values returned total_space, available_space.
  using StorageCapacityCallback = base::Callback<void(int64_t, int64_t)>;
  using StorageCapacityCallbackQueue =
      CallbackQueue<StorageCapacityCallback, int64_t, int64_t>;

  struct EvictionContext {
    EvictionContext();
    ~EvictionContext();
    GURL evicted_origin;
    StorageType evicted_type;
    StatusCallback evict_origin_data_callback;
  };

  // This initialization method is lazily called on the IO thread
  // when the first quota manager API is called.
  // Initialize must be called after all quota clients are added to the
  // manager by RegisterStorage.
  void LazyInitialize();
  void FinishLazyInitialize(bool is_database_bootstraped);
  void BootstrapDatabaseForEviction(
      const GetOriginCallback& did_get_origin_callback,
      int64_t unused_usage,
      int64_t unused_unlimited_usage);
  void DidBootstrapDatabase(const GetOriginCallback& did_get_origin_callback,
                            bool success);

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
  void NotifyStorageModifiedInternal(QuotaClient::ID client_id,
                                     const GURL& origin,
                                     StorageType type,
                                     int64_t delta,
                                     base::Time modified_time);

  void DumpQuotaTable(const DumpQuotaTableCallback& callback);
  void DumpOriginInfoTable(const DumpOriginInfoTableCallback& callback);

  void DeleteOriginDataInternal(const GURL& origin,
                                StorageType type,
                                int quota_client_mask,
                                bool is_eviction,
                                const StatusCallback& callback);

  // Methods for eviction logic.
  void StartEviction();
  void DeleteOriginFromDatabase(const GURL& origin,
                                StorageType type,
                                bool is_eviction);

  void DidOriginDataEvicted(QuotaStatusCode status);

  void ReportHistogram();
  void DidGetTemporaryGlobalUsageForHistogram(int64_t usage,
                                              int64_t unlimited_usage);
  void DidGetPersistentGlobalUsageForHistogram(int64_t usage,
                                               int64_t unlimited_usage);
  void DidDumpOriginInfoTableForHistogram(
      const OriginInfoTableEntries& entries);

  std::set<GURL> GetEvictionOriginExceptions(
      const std::set<GURL>& extra_exceptions);
  void DidGetEvictionOrigin(const GetOriginCallback& callback,
                            const GURL& origin);

  // QuotaEvictionHandler.
  void GetEvictionOrigin(StorageType type,
                         const std::set<GURL>& extra_exceptions,
                         int64_t global_quota,
                         const GetOriginCallback& callback) override;
  void EvictOriginData(const GURL& origin,
                       StorageType type,
                       const StatusCallback& callback) override;
  void GetEvictionRoundInfo(const EvictionRoundInfoCallback& callback) override;

  void GetLRUOrigin(StorageType type, const GetOriginCallback& callback);

  void DidGetPersistentHostQuota(const std::string& host,
                                 const int64_t* quota,
                                 bool success);
  void DidSetPersistentHostQuota(const std::string& host,
                                 const QuotaCallback& callback,
                                 const int64_t* new_quota,
                                 bool success);
  void DidGetLRUOrigin(const GURL* origin,
                       bool success);
  void GetQuotaSettings(const QuotaSettingsCallback& callback);
  void DidGetSettings(base::TimeTicks start_ticks,
                      base::Optional<QuotaSettings> settings);
  void GetStorageCapacity(const StorageCapacityCallback& callback);
  void ContinueIncognitoGetStorageCapacity(const QuotaSettings& settings);
  void DidGetStorageCapacity(
      const std::tuple<int64_t, int64_t>& total_and_available);

  void DidDatabaseWork(bool success);

  void DeleteOnCorrectThread() const;

  void PostTaskAndReplyWithResultForDBThread(
      const tracked_objects::Location& from_here,
      base::Callback<bool(QuotaDatabase*)> task,
      base::Callback<void(bool)> reply);

  static std::tuple<int64_t, int64_t> CallGetVolumeInfo(
      GetVolumeInfoFn get_volume_info_fn,
      const base::FilePath& path);
  static std::tuple<int64_t, int64_t> GetVolumeInfo(const base::FilePath& path);

  const bool is_incognito_;
  const base::FilePath profile_path_;

  scoped_refptr<QuotaManagerProxy> proxy_;
  bool db_disabled_;
  bool eviction_disabled_;
  scoped_refptr<base::SingleThreadTaskRunner> io_thread_;
  scoped_refptr<base::SequencedTaskRunner> db_thread_;
  mutable std::unique_ptr<QuotaDatabase> database_;
  bool is_database_bootstrapped_ = false;

  GetQuotaSettingsFunc get_settings_function_;
  scoped_refptr<base::TaskRunner> get_settings_task_runner_;
  QuotaSettings settings_;
  base::TimeTicks settings_timestamp_;
  QuotaSettingsCallbackQueue settings_callbacks_;
  StorageCapacityCallbackQueue storage_capacity_callbacks_;

  GetOriginCallback lru_origin_callback_;
  std::set<GURL> access_notified_origins_;

  QuotaClientList clients_;

  std::unique_ptr<UsageTracker> temporary_usage_tracker_;
  std::unique_ptr<UsageTracker> persistent_usage_tracker_;
  std::unique_ptr<UsageTracker> syncable_usage_tracker_;
  // TODO(michaeln): Need a way to clear the cache, drop and
  // reinstantiate the trackers when they're not handling requests.

  std::unique_ptr<QuotaTemporaryStorageEvictor> temporary_storage_evictor_;
  EvictionContext eviction_context_;
  bool is_getting_eviction_origin_;

  HostQuotaCallbackMap persistent_host_quota_callbacks_;

  // Map from origin to count.
  std::map<GURL, int> origins_in_use_;
  // Map from origin to error count.
  std::map<GURL, int> origins_in_error_;

  scoped_refptr<SpecialStoragePolicy> special_storage_policy_;

  base::RepeatingTimer histogram_timer_;

  // Pointer to the function used to get volume information. This is
  // overwritten by QuotaManagerTest in order to attain deterministic reported
  // values. The default value points to QuotaManager::GetVolumeInfo.
  GetVolumeInfoFn get_volume_info_fn_;

  std::unique_ptr<StorageMonitor> storage_monitor_;

  base::WeakPtrFactory<QuotaManager> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(QuotaManager);
};

struct QuotaManagerDeleter {
  static void Destruct(const QuotaManager* manager) {
    manager->DeleteOnCorrectThread();
  }
};

}  // namespace storage

#endif  // STORAGE_BROWSER_QUOTA_QUOTA_MANAGER_H_
