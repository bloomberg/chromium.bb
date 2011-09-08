// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_APPCACHE_APPCACHE_STORAGE_IMPL_H_
#define WEBKIT_APPCACHE_APPCACHE_STORAGE_IMPL_H_

#include <deque>
#include <map>
#include <set>
#include <vector>

#include "base/file_path.h"
#include "base/message_loop_proxy.h"
#include "base/task.h"
#include "webkit/appcache/appcache_database.h"
#include "webkit/appcache/appcache_disk_cache.h"
#include "webkit/appcache/appcache_storage.h"

namespace appcache {

class AppCacheStorageImpl : public AppCacheStorage {
 public:
  explicit AppCacheStorageImpl(AppCacheService* service);
  virtual ~AppCacheStorageImpl();

  void Initialize(const FilePath& cache_directory,
                  base::MessageLoopProxy* cache_thread);
  void Disable();
  bool is_disabled() const { return is_disabled_; }

  // AppCacheStorage methods, see the base class for doc comments.
  virtual void GetAllInfo(Delegate* delegate);
  virtual void LoadCache(int64 id, Delegate* delegate);
  virtual void LoadOrCreateGroup(const GURL& manifest_url, Delegate* delegate);
  virtual void StoreGroupAndNewestCache(
      AppCacheGroup* group, AppCache* newest_cache, Delegate* delegate);
  virtual void FindResponseForMainRequest(
      const GURL& url, const GURL& preferred_manifest_url, Delegate* delegate);
  virtual void FindResponseForSubRequest(
      AppCache* cache, const GURL& url,
      AppCacheEntry* found_entry, AppCacheEntry* found_fallback_entry,
      bool* found_network_namespace);
  virtual void MarkEntryAsForeign(const GURL& entry_url, int64 cache_id);
  virtual void MakeGroupObsolete(AppCacheGroup* group, Delegate* delegate);
  virtual AppCacheResponseReader* CreateResponseReader(
      const GURL& manifest_url, int64 response_id);
  virtual AppCacheResponseWriter* CreateResponseWriter(
      const GURL& manifest_url);
  virtual void DoomResponses(
      const GURL& manifest_url, const std::vector<int64>& response_ids);
  virtual void DeleteResponses(
      const GURL& manifest_url, const std::vector<int64>& response_ids);
  virtual void PurgeMemory();

 private:
  friend class AppCacheStorageImplTest;

  // The AppCacheStorageImpl class methods and datamembers may only be
  // accessed on the IO thread. This class manufactures seperate DatabaseTasks
  // which access the DB on a seperate background thread.
  class DatabaseTask;
  class InitTask;
  class CloseConnectionTask;
  class DisableDatabaseTask;
  class GetAllInfoTask;
  class StoreOrLoadTask;
  class CacheLoadTask;
  class GroupLoadTask;
  class StoreGroupAndCacheTask;
  class FindMainResponseTask;
  class MarkEntryAsForeignTask;
  class MakeGroupObsoleteTask;
  class GetDeletableResponseIdsTask;
  class InsertDeletableResponseIdsTask;
  class DeleteDeletableResponseIdsTask;
  class UpdateGroupLastAccessTimeTask;

  typedef std::deque<DatabaseTask*> DatabaseTaskQueue;
  typedef std::map<int64, CacheLoadTask*> PendingCacheLoads;
  typedef std::map<GURL, GroupLoadTask*> PendingGroupLoads;
  typedef std::deque<std::pair<GURL, int64> > PendingForeignMarkings;
  typedef std::set<StoreGroupAndCacheTask*> PendingQuotaQueries;

  bool IsInitTaskComplete() {
    return last_cache_id_ != AppCacheStorage::kUnitializedId;
  }

  CacheLoadTask* GetPendingCacheLoadTask(int64 cache_id);
  GroupLoadTask* GetPendingGroupLoadTask(const GURL& manifest_url);
  void GetPendingForeignMarkingsForCache(
      int64 cache_id, std::vector<GURL>* urls);

  void ScheduleSimpleTask(Task* task);
  void RunOnePendingSimpleTask();

  void DelayedStartDeletingUnusedResponses();
  void StartDeletingResponses(const std::vector<int64>& response_ids);
  void ScheduleDeleteOneResponse();
  void DeleteOneResponse();

  void OnDeletedOneResponse(int rv);
  void OnDiskCacheInitialized(int rv);

  // Sometimes we can respond without having to query the database.
  bool FindResponseForMainRequestInGroup(
      AppCacheGroup* group,  const GURL& url, Delegate* delegate);
  void DeliverShortCircuitedFindMainResponse(
      const GURL& url,
      const AppCacheEntry& found_entry,
      scoped_refptr<AppCacheGroup> group,
      scoped_refptr<AppCache> newest_cache,
      scoped_refptr<DelegateReference> delegate_ref);

  void CallOnMainResponseFound(
      DelegateReferenceVector* delegates,
      const GURL& url, const AppCacheEntry& entry,
      const GURL& fallback_url, const AppCacheEntry& fallback_entry,
      int64 cache_id, const GURL& manifest_url);

  AppCacheDiskCache* disk_cache();

  // The directory in which we place files in the file system.
  FilePath cache_directory_;
  scoped_refptr<base::MessageLoopProxy> cache_thread_;
  bool is_incognito_;

  // Structures to keep track of DatabaseTasks that are in-flight.
  DatabaseTaskQueue scheduled_database_tasks_;
  PendingCacheLoads pending_cache_loads_;
  PendingGroupLoads pending_group_loads_;
  PendingForeignMarkings pending_foreign_markings_;
  PendingQuotaQueries pending_quota_queries_;

  // Structures to keep track of lazy response deletion.
  std::deque<int64> deletable_response_ids_;
  std::vector<int64> deleted_response_ids_;
  bool is_response_deletion_scheduled_;
  bool did_start_deleting_responses_;
  int64 last_deletable_response_rowid_;

  // AppCacheDiskCache async callbacks
  net::CompletionCallbackImpl<AppCacheStorageImpl> doom_callback_;
  net::CompletionCallbackImpl<AppCacheStorageImpl> init_callback_;

  // Created on the IO thread, but only used on the DB thread.
  AppCacheDatabase* database_;

  // Set if we discover a fatal error like a corrupt sql database or
  // disk cache and cannot continue.
  bool is_disabled_;

  // TODO(michaeln): use a disk_cache per group (manifest or group_id).
  scoped_ptr<AppCacheDiskCache> disk_cache_;

  // Used to short-circuit certain operations without having to schedule
  // any tasks on the background database thread.
  std::deque<Task*> pending_simple_tasks_;
  ScopedRunnableMethodFactory<AppCacheStorageImpl> method_factory_;

  friend class ChromeAppCacheServiceTest;
};

}  // namespace appcache

#endif  // WEBKIT_APPCACHE_APPCACHE_STORAGE_IMPL_H_
