// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_QUOTA_QUOTA_MANAGER_H_
#define STORAGE_BROWSER_QUOTA_QUOTA_MANAGER_H_

#include <stdint.h>

#include <list>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "base/stl_util.h"
#include "base/timer/timer.h"
#include "storage/browser/quota/quota_callbacks.h"
#include "storage/browser/quota/quota_client.h"
#include "storage/browser/quota/quota_client_type.h"
#include "storage/browser/quota/quota_database.h"
#include "storage/browser/quota/quota_settings.h"
#include "storage/browser/quota/quota_task.h"
#include "storage/browser/quota/special_storage_policy.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom-forward.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom-shared.h"

namespace base {
class SequencedTaskRunner;
class SingleThreadTaskRunner;
class TaskRunner;
}  // namespace base

namespace quota_internals {
class QuotaInternalsProxy;
}  // namespace quota_internals

namespace storage {

class QuotaManagerProxy;
class QuotaTemporaryStorageEvictor;
class UsageTracker;

// An interface called by QuotaTemporaryStorageEvictor. This is a grab bag of
// methods called by QuotaTemporaryStorageEvictor that need to be stubbed for
// testing.
class COMPONENT_EXPORT(STORAGE_BROWSER) QuotaEvictionHandler {
 public:
  using EvictionRoundInfoCallback =
      base::OnceCallback<void(blink::mojom::QuotaStatusCode status,
                              const QuotaSettings& settings,
                              int64_t available_space,
                              int64_t total_space,
                              int64_t global_usage,
                              bool global_usage_is_complete)>;

  // Called at the beginning of an eviction round to gather the info about
  // the current settings, capacity, and usage.
  virtual void GetEvictionRoundInfo(EvictionRoundInfoCallback callback) = 0;

  // Returns next origin to evict, or nullopt if there are no evictable
  // origins.
  virtual void GetEvictionOrigin(blink::mojom::StorageType type,
                                 int64_t global_quota,
                                 GetOriginCallback callback) = 0;

  // Called to evict an origin.
  virtual void EvictOriginData(const url::Origin& origin,
                               blink::mojom::StorageType type,
                               StatusCallback callback) = 0;

 protected:
  virtual ~QuotaEvictionHandler() = default;
};

struct UsageInfo {
  UsageInfo(std::string host, blink::mojom::StorageType type, int64_t usage)
      : host(std::move(host)), type(type), usage(usage) {}
  const std::string host;
  const blink::mojom::StorageType type;
  const int64_t usage;
};

// Entry point into the Quota System
//
// Each StoragePartition has exactly one QuotaManager instance, which
// coordinates quota across the Web platform features subject to quota.
// Each storage system interacts with quota via their own implementations of
// the QuotaClient interface.
//
// The class sets limits and defines the parameters of the systems heuristics.
// QuotaManager coordinates clients to orchestrate the collection of usage
// information, enforce quota limits, and evict stale data.
//
// The constructor and proxy() methods can be called on any thread. All other
// methods must be called on the IO thread.
class COMPONENT_EXPORT(STORAGE_BROWSER) QuotaManager
    : public QuotaTaskObserver,
      public QuotaEvictionHandler,
      public base::RefCountedDeleteOnSequence<QuotaManager> {
 public:
  using UsageAndQuotaCallback = base::OnceCallback<
      void(blink::mojom::QuotaStatusCode, int64_t usage, int64_t quota)>;

  using UsageAndQuotaWithBreakdownCallback =
      base::OnceCallback<void(blink::mojom::QuotaStatusCode,
                              int64_t usage,
                              int64_t quota,
                              blink::mojom::UsageBreakdownPtr usage_breakdown)>;

  static const int64_t kNoLimit;

  QuotaManager(bool is_incognito,
               const base::FilePath& profile_path,
               scoped_refptr<base::SingleThreadTaskRunner> io_thread,
               scoped_refptr<SpecialStoragePolicy> special_storage_policy,
               const GetQuotaSettingsFunc& get_settings_function);

  const QuotaSettings& settings() const { return settings_; }
  void SetQuotaSettings(const QuotaSettings& settings);

  // Returns a proxy object that can be used on any thread.
  QuotaManagerProxy* proxy() { return proxy_.get(); }

  // Called by clients or webapps. Returns usage per host.
  void GetUsageInfo(GetUsageInfoCallback callback);

  // Called by Web Apps.
  // This method is declared as virtual to allow test code to override it.
  virtual void GetUsageAndQuotaForWebApps(const url::Origin& origin,
                                          blink::mojom::StorageType type,
                                          UsageAndQuotaCallback callback);
  // Called by DevTools.
  // This method is declared as virtual to allow test code to override it.
  virtual void GetUsageAndQuotaWithBreakdown(
      const url::Origin& origin,
      blink::mojom::StorageType type,
      UsageAndQuotaWithBreakdownCallback callback);

  // Called by StorageClients.
  // This method is declared as virtual to allow test code to override it.
  //
  // For UnlimitedStorage origins, this version skips usage and quota handling
  // to avoid extra query cost.
  // Do not call this method for apps/user-facing code.
  virtual void GetUsageAndQuota(const url::Origin& origin,
                                blink::mojom::StorageType type,
                                UsageAndQuotaCallback callback);

  // Called by clients via proxy.
  // Client storage should call this method when storage is accessed.
  // Used to maintain LRU ordering.
  void NotifyStorageAccessed(const url::Origin& origin,
                             blink::mojom::StorageType type);

  // Called by clients via proxy.
  // Client storage must call this method whenever they have made any
  // modifications that change the amount of data stored in their storage.
  void NotifyStorageModified(QuotaClientType client_id,
                             const url::Origin& origin,
                             blink::mojom::StorageType type,
                             int64_t delta);

  // Called by clients via proxy.
  // This method is declared as virtual to allow test code to override it.
  //
  // Client storage must call this method whenever they run into disk
  // write errors. Used as a hint to determine if the storage partition is out
  // of space, and trigger actions if deemed appropriate.
  virtual void NotifyWriteFailed(const url::Origin& origin);

  // Used to avoid evicting origins with open pages.
  // A call to NotifyOriginInUse must be balanced by a later call
  // to NotifyOriginNoLongerInUse.
  void NotifyOriginInUse(const url::Origin& origin);
  void NotifyOriginNoLongerInUse(const url::Origin& origin);
  bool IsOriginInUse(const url::Origin& origin) const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return base::Contains(origins_in_use_, origin);
  }

  void SetUsageCacheEnabled(QuotaClientType client_id,
                            const url::Origin& origin,
                            blink::mojom::StorageType type,
                            bool enabled);

  // DeleteOriginData and DeleteHostData (surprisingly enough) delete data of a
  // particular blink::mojom::StorageType associated with either a specific
  // origin or set of origins. Each method additionally requires a
  // |quota_client_types| which specifies the types of QuotaClients to delete
  // from the origin. Pass in QuotaClientType::AllClients() to remove all
  // clients from the origin, regardless of type.
  virtual void DeleteOriginData(const url::Origin& origin,
                                blink::mojom::StorageType type,
                                QuotaClientTypes quota_client_types,
                                StatusCallback callback);
  void DeleteHostData(const std::string& host,
                      blink::mojom::StorageType type,
                      QuotaClientTypes quota_client_types,
                      StatusCallback callback);

  // Instructs each QuotaClient to remove possible traces of deleted
  // data on the disk.
  void PerformStorageCleanup(blink::mojom::StorageType type,
                             QuotaClientTypes quota_client_types,
                             base::OnceClosure callback);

  // Called by UI and internal modules.
  void GetPersistentHostQuota(const std::string& host, QuotaCallback callback);
  void SetPersistentHostQuota(const std::string& host,
                              int64_t new_quota,
                              QuotaCallback callback);
  void GetGlobalUsage(blink::mojom::StorageType type,
                      GlobalUsageCallback callback);
  void GetHostUsage(const std::string& host,
                    blink::mojom::StorageType type,
                    UsageCallback callback);
  void GetHostUsageWithBreakdown(const std::string& host,
                                 blink::mojom::StorageType type,
                                 UsageWithBreakdownCallback callback);

  std::map<std::string, std::string> GetStatistics();

  bool IsStorageUnlimited(const url::Origin& origin,
                          blink::mojom::StorageType type) const;

  virtual void GetOriginsModifiedSince(blink::mojom::StorageType type,
                                       base::Time modified_since,
                                       GetOriginsCallback callback);

  bool ResetUsageTracker(blink::mojom::StorageType type);

  // Called when StoragePartition is initialized if embedder has an
  // implementation of StorageNotificationService.
  void SetStoragePressureCallback(
      base::RepeatingCallback<void(url::Origin)> storage_pressure_callback);

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

  void DisableDatabaseForTesting() { db_disabled_ = true; }

 protected:
  ~QuotaManager() override;

 private:
  friend class base::DeleteHelper<QuotaManager>;
  friend class base::RefCountedDeleteOnSequence<QuotaManager>;
  friend class quota_internals::QuotaInternalsProxy;
  friend class MockQuotaManager;
  friend class MockQuotaClient;
  friend class QuotaManagerProxy;
  friend class QuotaManagerTest;
  friend class QuotaTemporaryStorageEvictor;

  class EvictionRoundInfoHelper;
  class UsageAndQuotaInfoGatherer;
  class GetUsageInfoTask;
  class OriginDataDeleter;
  class HostDataDeleter;
  class GetModifiedSinceHelper;
  class DumpQuotaTableHelper;
  class DumpOriginInfoTableHelper;
  class StorageCleanupHelper;

  using QuotaTableEntry = QuotaDatabase::QuotaTableEntry;
  using OriginInfoTableEntry = QuotaDatabase::OriginInfoTableEntry;
  using QuotaTableEntries = std::vector<QuotaTableEntry>;
  using OriginInfoTableEntries = std::vector<OriginInfoTableEntry>;

  using QuotaSettingsCallback = base::OnceCallback<void(const QuotaSettings&)>;

  // Function pointer type used to store the function which returns
  // information about the volume containing the given FilePath.
  // The value returned is std::tuple<total_space, available_space>.
  using GetVolumeInfoFn =
      std::tuple<int64_t, int64_t> (*)(const base::FilePath&);

  using DumpQuotaTableCallback =
      base::OnceCallback<void(const QuotaTableEntries&)>;
  using DumpOriginInfoTableCallback =
      base::OnceCallback<void(const OriginInfoTableEntries&)>;

  // The values returned total_space, available_space.
  using StorageCapacityCallback = base::OnceCallback<void(int64_t, int64_t)>;

  struct EvictionContext {
    EvictionContext();
    ~EvictionContext();
    url::Origin evicted_origin;
    blink::mojom::StorageType evicted_type;
    StatusCallback evict_origin_data_callback;
  };

  // This initialization method is lazily called on the IO thread
  // when the first quota manager API is called.
  // Initialize must be called after all quota clients are added to the
  // manager by RegisterStorage.
  void LazyInitialize();
  void FinishLazyInitialize(bool is_database_bootstraped);
  void BootstrapDatabaseForEviction(GetOriginCallback did_get_origin_callback,
                                    int64_t unused_usage,
                                    int64_t unused_unlimited_usage);
  void DidBootstrapDatabase(GetOriginCallback did_get_origin_callback,
                            bool success);

  // Called by clients via proxy.
  // Registers a quota client to the manager.
  void RegisterClient(scoped_refptr<QuotaClient> client);

  UsageTracker* GetUsageTracker(blink::mojom::StorageType type) const;

  // Extract cached origins list from the usage tracker.
  // (Might return empty list if no origin is tracked by the tracker.)
  std::set<url::Origin> GetCachedOrigins(blink::mojom::StorageType type);

  // These internal methods are separately defined mainly for testing.
  void NotifyStorageAccessedInternal(const url::Origin& origin,
                                     blink::mojom::StorageType type,
                                     base::Time accessed_time);
  void NotifyStorageModifiedInternal(QuotaClientType client_id,
                                     const url::Origin& origin,
                                     blink::mojom::StorageType type,
                                     int64_t delta,
                                     base::Time modified_time);

  void DumpQuotaTable(DumpQuotaTableCallback callback);
  void DumpOriginInfoTable(DumpOriginInfoTableCallback callback);

  void DeleteOriginDataInternal(const url::Origin& origin,
                                blink::mojom::StorageType type,
                                QuotaClientTypes quota_client_types,
                                bool is_eviction,
                                StatusCallback callback);

  // Methods for eviction logic.
  void StartEviction();
  void DeleteOriginFromDatabase(const url::Origin& origin,
                                blink::mojom::StorageType type,
                                bool is_eviction);

  void DidOriginDataEvicted(blink::mojom::QuotaStatusCode status);

  void ReportHistogram();
  void DidGetTemporaryGlobalUsageForHistogram(int64_t usage,
                                              int64_t unlimited_usage);
  void DidGetStorageCapacityForHistogram(int64_t usage,
                                         int64_t total_space,
                                         int64_t available_space);
  void DidGetPersistentGlobalUsageForHistogram(int64_t usage,
                                               int64_t unlimited_usage);
  void DidDumpOriginInfoTableForHistogram(
      const OriginInfoTableEntries& entries);

  std::set<url::Origin> GetEvictionOriginExceptions();
  void DidGetEvictionOrigin(GetOriginCallback callback,
                            const base::Optional<url::Origin>& origin);

  // QuotaEvictionHandler.
  void GetEvictionOrigin(blink::mojom::StorageType type,
                         int64_t global_quota,
                         GetOriginCallback callback) override;
  void EvictOriginData(const url::Origin& origin,
                       blink::mojom::StorageType type,
                       StatusCallback callback) override;
  void GetEvictionRoundInfo(EvictionRoundInfoCallback callback) override;

  void GetLRUOrigin(blink::mojom::StorageType type, GetOriginCallback callback);

  void DidGetPersistentHostQuota(const std::string& host,
                                 const int64_t* quota,
                                 bool success);
  void DidSetPersistentHostQuota(const std::string& host,
                                 QuotaCallback callback,
                                 const int64_t* new_quota,
                                 bool success);
  void DidGetLRUOrigin(std::unique_ptr<base::Optional<url::Origin>> origin,
                       bool success);
  void GetQuotaSettings(QuotaSettingsCallback callback);
  void DidGetSettings(base::Optional<QuotaSettings> settings);
  void GetStorageCapacity(StorageCapacityCallback callback);
  void ContinueIncognitoGetStorageCapacity(const QuotaSettings& settings);
  void DidGetStorageCapacity(
      const std::tuple<int64_t, int64_t>& total_and_available);

  void DidDatabaseWork(bool success);

  void DeleteOnCorrectThread() const;

  void MaybeRunStoragePressureCallback(const url::Origin& origin,
                                       int64_t total_space,
                                       int64_t available_space);
  // Used from quota-internals page to test behavior of the storage pressure
  // callback.
  void SimulateStoragePressure(const url::Origin origin);

  void PostTaskAndReplyWithResultForDBThread(
      const base::Location& from_here,
      base::OnceCallback<bool(QuotaDatabase*)> task,
      base::OnceCallback<void(bool)> reply);

  static std::tuple<int64_t, int64_t> CallGetVolumeInfo(
      GetVolumeInfoFn get_volume_info_fn,
      const base::FilePath& path);
  static std::tuple<int64_t, int64_t> GetVolumeInfo(const base::FilePath& path);

  const bool is_incognito_;
  const base::FilePath profile_path_;

  // proxy_ can be accessed by any thread so it must be thread-safe
  const scoped_refptr<QuotaManagerProxy> proxy_;
  bool db_disabled_;
  bool eviction_disabled_;
  base::Optional<url::Origin> origin_for_pending_storage_pressure_callback_;
  scoped_refptr<base::SingleThreadTaskRunner> io_thread_;
  scoped_refptr<base::SequencedTaskRunner> db_runner_;
  mutable std::unique_ptr<QuotaDatabase> database_;
  bool is_database_bootstrapped_ = false;

  GetQuotaSettingsFunc get_settings_function_;
  scoped_refptr<base::TaskRunner> get_settings_task_runner_;
  base::RepeatingCallback<void(url::Origin)> storage_pressure_callback_;
  QuotaSettings settings_;
  base::TimeTicks settings_timestamp_;
  std::tuple<base::TimeTicks, int64_t, int64_t>
      cached_disk_stats_for_storage_pressure_;
  CallbackQueue<QuotaSettingsCallback, const QuotaSettings&>
      settings_callbacks_;
  CallbackQueue<StorageCapacityCallback, int64_t, int64_t>
      storage_capacity_callbacks_;

  GetOriginCallback lru_origin_callback_;
  std::set<url::Origin> access_notified_origins_;

  std::vector<scoped_refptr<QuotaClient>> clients_;

  std::unique_ptr<UsageTracker> temporary_usage_tracker_;
  std::unique_ptr<UsageTracker> persistent_usage_tracker_;
  std::unique_ptr<UsageTracker> syncable_usage_tracker_;
  // TODO(michaeln): Need a way to clear the cache, drop and
  // reinstantiate the trackers when they're not handling requests.

  std::unique_ptr<QuotaTemporaryStorageEvictor> temporary_storage_evictor_;
  EvictionContext eviction_context_;
  bool is_getting_eviction_origin_;

  CallbackQueueMap<QuotaCallback,
                   std::string,
                   blink::mojom::QuotaStatusCode,
                   int64_t>
      persistent_host_quota_callbacks_;

  // Map from origin to count.
  std::map<url::Origin, int> origins_in_use_;
  // Map from origin to error count.
  std::map<url::Origin, int> origins_in_error_;

  scoped_refptr<SpecialStoragePolicy> special_storage_policy_;

  base::RepeatingTimer histogram_timer_;

  // Pointer to the function used to get volume information. This is
  // overwritten by QuotaManagerTest in order to attain deterministic reported
  // values. The default value points to QuotaManager::GetVolumeInfo.
  GetVolumeInfoFn get_volume_info_fn_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<QuotaManager> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(QuotaManager);
};

}  // namespace storage

#endif  // STORAGE_BROWSER_QUOTA_QUOTA_MANAGER_H_
