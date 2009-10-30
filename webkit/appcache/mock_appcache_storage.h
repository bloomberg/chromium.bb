// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_APPCACHE_MOCK_APPCACHE_STORAGE_H_
#define WEBKIT_APPCACHE_MOCK_APPCACHE_STORAGE_H_

#include <deque>
#include <map>

#include "base/hash_tables.h"
#include "base/scoped_ptr.h"
#include "base/task.h"
#include "net/disk_cache/disk_cache.h"
#include "webkit/appcache/appcache.h"
#include "webkit/appcache/appcache_group.h"
#include "webkit/appcache/appcache_storage.h"

namespace appcache {

// For use in unit tests.
// Note: This class is also being used to bootstrap our development efforts.
// We can get layout tests up and running, and back fill with real storage
// somewhat in parallel.
class MockAppCacheStorage : public AppCacheStorage {
 public:
  explicit MockAppCacheStorage(AppCacheService* service);
  virtual ~MockAppCacheStorage();

  virtual void LoadCache(int64 id, Delegate* delegate);
  virtual void LoadOrCreateGroup(const GURL& manifest_url, Delegate* delegate);
  virtual void StoreGroupAndNewestCache(
      AppCacheGroup* group, Delegate* delegate);
  virtual void FindResponseForMainRequest(const GURL& url, Delegate* delegate);
  virtual void MarkEntryAsForeign(const GURL& entry_url, int64 cache_id);
  virtual void MakeGroupObsolete(AppCacheGroup* group, Delegate* delegate);
  virtual AppCacheResponseReader* CreateResponseReader(
      const GURL& manifest_url, int64 response_id);
  virtual AppCacheResponseWriter* CreateResponseWriter(const GURL& origin);
  virtual void DoomResponses(
      const GURL& manifest_url, const std::vector<int64>& response_ids);

 private:
  friend class AppCacheUpdateJobTest;

  typedef base::hash_map<int64, scoped_refptr<AppCache> > StoredCacheMap;
  typedef std::map<GURL, scoped_refptr<AppCacheGroup> > StoredGroupMap;
  typedef std::set<int64> DoomedResponseIds;

  void ProcessLoadCache(
      int64 id, scoped_refptr<DelegateReference> delegate_ref);
  void ProcessLoadOrCreateGroup(
      const GURL& manifest_url, scoped_refptr<DelegateReference> delegate_ref);
  void ProcessStoreGroupAndNewestCache(
      scoped_refptr<AppCacheGroup> group, scoped_refptr<AppCache> newest_cache,
      scoped_refptr<DelegateReference> delegate_ref);
  void ProcessMakeGroupObsolete(
      scoped_refptr<AppCacheGroup> group,
      scoped_refptr<DelegateReference> delegate_ref);
  void ProcessFindResponseForMainRequest(
      const GURL& url, scoped_refptr<DelegateReference> delegate_ref);

  void ScheduleTask(Task* task);
  void RunOnePendingTask();

  void AddStoredCache(AppCache* cache);
  void RemoveStoredCache(AppCache* cache);
  void RemoveStoredCaches(const AppCacheGroup::Caches& caches);
  bool IsCacheStored(const AppCache* cache) {
    return stored_caches_.find(cache->cache_id()) != stored_caches_.end();
  }

  void AddStoredGroup(AppCacheGroup* group);
  void RemoveStoredGroup(AppCacheGroup* group);
  bool IsGroupStored(const AppCacheGroup* group) {
    return stored_groups_.find(group->manifest_url()) != stored_groups_.end();
  }

  // These helpers determine when certain operations should complete
  // asynchronously vs synchronously to faithfully mimic, or mock,
  // the behavior of the real implemenation of the AppCacheStorage
  // interface.
  bool ShouldGroupLoadAppearAsync(const AppCacheGroup* group);
  bool ShouldCacheLoadAppearAsync(const AppCache* cache);

  // Lazily constructed in-memory disk cache.
  disk_cache::Backend* disk_cache() {
    if (!disk_cache_.get()) {
      const int kMaxCacheSize = 10 * 1024 * 1024;
      disk_cache_.reset(disk_cache::CreateInMemoryCacheBackend(kMaxCacheSize));
    }
    return disk_cache_.get();
  }

  // Simulate failures for testing.
  void SimulateMakeGroupObsoleteFailure() {
    simulate_make_group_obsolete_failure_ = true;
  }
  void SimulateStoreGroupAndNewestCacheFailure() {
    simulate_store_group_and_newest_cache_failure_ = true;
  }

  StoredCacheMap stored_caches_;
  StoredGroupMap stored_groups_;
  DoomedResponseIds doomed_response_ids_;
  scoped_ptr<disk_cache::Backend> disk_cache_;
  std::deque<Task*> pending_tasks_;
  ScopedRunnableMethodFactory<MockAppCacheStorage> method_factory_;

  bool simulate_make_group_obsolete_failure_;
  bool simulate_store_group_and_newest_cache_failure_;

  FRIEND_TEST(MockAppCacheStorageTest, CreateGroup);
  FRIEND_TEST(MockAppCacheStorageTest, LoadCache_FarHit);
  FRIEND_TEST(MockAppCacheStorageTest, LoadGroupAndCache_FarHit);
  FRIEND_TEST(MockAppCacheStorageTest, MakeGroupObsolete);
  FRIEND_TEST(MockAppCacheStorageTest, StoreNewGroup);
  FRIEND_TEST(MockAppCacheStorageTest, StoreExistingGroup);
  DISALLOW_COPY_AND_ASSIGN(MockAppCacheStorage);
};

}  // namespace appcache

#endif  // WEBKIT_APPCACHE_MOCK_APPCACHE_STORAGE_H_
