// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_APPCACHE_APPCACHE_STORAGE_IMPL_H_
#define WEBKIT_APPCACHE_APPCACHE_STORAGE_IMPL_H_

#include <deque>
#include <map>
#include <set>
#include <vector>

#include "base/file_path.h"
#include "net/disk_cache/disk_cache.h"
#include "webkit/appcache/appcache_database.h"
#include "webkit/appcache/appcache_storage.h"

namespace appcache {

class AppCacheStorageImpl : public AppCacheStorage {
 public:
  explicit AppCacheStorageImpl(AppCacheService* service);
  virtual ~AppCacheStorageImpl();

  void Initialize(const FilePath& cache_directory);

  // AppCacheStorage methods
  virtual void LoadCache(int64 id, Delegate* delegate);
  virtual void LoadOrCreateGroup(const GURL& manifest_url, Delegate* delegate);
  virtual void StoreGroupAndNewestCache(
      AppCacheGroup* group, AppCache* newest_cache, Delegate* delegate);
  virtual void FindResponseForMainRequest(const GURL& url, Delegate* delegate);
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

 private:
  friend class AppCacheStorageImplTest;

  // The AppCacheStorageImpl class methods and datamembers may only be
  // accessed on the IO thread. This class manufactures seperate DatabaseTasks
  // which access the DB on a seperate background thread.
  class DatabaseTask;
  class InitTask;
  class CloseConnectionTask;
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

  typedef std::deque<DatabaseTask*> DatabaseTaskQueue;
  typedef std::map<int64, CacheLoadTask*> PendingCacheLoads;
  typedef std::map<GURL, GroupLoadTask*> PendingGroupLoads;
  typedef std::deque<std::pair<GURL, int64> > PendingForeignMarkings;

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

  // Sometimes we can respond without having to query the database.
  void DeliverShortCircuitedFindMainResponse(
      const GURL& url, AppCacheEntry found_entry,
      scoped_refptr<AppCacheGroup> group, scoped_refptr<AppCache> newest_cache,
      scoped_refptr<DelegateReference> delegate_ref);

  disk_cache::Backend* disk_cache();

  // The directory in which we place files in the file system.
  FilePath cache_directory_;
  bool is_incognito_;

  // Structures to keep track of DatabaseTasks that are in-flight.
  DatabaseTaskQueue scheduled_database_tasks_;
  PendingCacheLoads pending_cache_loads_;
  PendingGroupLoads pending_group_loads_;
  PendingForeignMarkings pending_foreign_markings_;

  // Structures to keep track of lazy response deletion.
  std::deque<int64> deletable_response_ids_;
  std::vector<int64> deleted_response_ids_;
  bool is_response_deletion_scheduled_;
  bool did_start_deleting_responses_;
  int64 last_deletable_response_rowid_;

  // Created on the IO thread, but only used on the DB thread.
  AppCacheDatabase* database_;

  // TODO(michaeln): use a disk_cache per group (manifest or group_id).
  scoped_ptr<disk_cache::Backend> disk_cache_;

  // Used to short-circuit certain operations without having to schedule
  // any tasks on the background database thread.
  std::set<GURL> origins_with_groups_;
  std::deque<Task*> pending_simple_tasks_;
  ScopedRunnableMethodFactory<AppCacheStorageImpl> method_factory_;
};

}  // namespace appcache

#endif  // WEBKIT_APPCACHE_APPCACHE_STORAGE_IMPL_H_
