// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_APPCACHE_MOCK_APPCACHE_STORAGE_H_
#define WEBKIT_APPCACHE_MOCK_APPCACHE_STORAGE_H_

#include <map>

#include "base/hash_tables.h"
#include "base/scoped_ptr.h"
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
  virtual void LoadCache(int64 id, Delegate* delegate);
  virtual void LoadOrCreateGroup(const GURL& manifest_url, Delegate* delegate);
  virtual void StoreGroupAndNewestCache(
      AppCacheGroup* group, Delegate* delegate);
  virtual void FindResponseForMainRequest(const GURL& url, Delegate* delegate);
  virtual void MarkEntryAsForeign(const GURL& entry_url, int64 cache_id);
  virtual void MarkGroupAsObsolete(AppCacheGroup* group, Delegate* delegate);
  virtual AppCacheResponseReader* CreateResponseReader(
      const GURL& manifest_url, int64 response_id);
  virtual AppCacheResponseWriter* CreateResponseWriter(const GURL& origin);
  virtual void DoomResponses(
      const GURL& manifest_url, const std::vector<int64>& response_ids);

 private:
  typedef base::hash_map<int64, scoped_refptr<AppCache> > StoredCacheMap;
  typedef std::map<GURL, scoped_refptr<AppCacheGroup> > StoredGroupMap;

  void AddStoredCache(AppCache* cache);
  void RemoveStoredCache(AppCache* cache);
  AppCache* GetStoredCache(int64 id) {
    StoredCacheMap::iterator it = stored_caches_.find(id);
    return (it != stored_caches_.end()) ? it->second : NULL;
  }

  void AddStoredGroup(AppCacheGroup* group);
  void RemoveStoredGroup(AppCacheGroup* group);
  AppCacheGroup* GetStoredGroup(const GURL& manifest_url) {
    StoredGroupMap::iterator it = stored_groups_.find(manifest_url);
    return (it != stored_groups_.end()) ? it->second : NULL;
  }

  // Lazily constructed in-memory disk cache.
  disk_cache::Backend* disk_cache() {
    if (!disk_cache_.get()) {
      const int kMaxCacheSize = 10 * 1024 * 1024;
      disk_cache_.reset(disk_cache::CreateInMemoryCacheBackend(kMaxCacheSize));
    }
    return disk_cache_.get();
  }

  StoredCacheMap stored_caches_;
  StoredGroupMap stored_groups_;
  scoped_ptr<disk_cache::Backend> disk_cache_;
};

}  // namespace appcache

#endif  // WEBKIT_APPCACHE_MOCK_APPCACHE_STORAGE_H_
