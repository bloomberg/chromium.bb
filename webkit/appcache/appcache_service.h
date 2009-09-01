// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_APPCACHE_APPCACHE_SERVICE_H_
#define WEBKIT_APPCACHE_APPCACHE_SERVICE_H_

#include <map>
#include <set>
#include <vector>

#include "base/hash_tables.h"
#include "base/file_path.h"
#include "googleurl/src/gurl.h"

namespace appcache {

class AppCache;
class AppCacheBackendImpl;
class AppCacheGroup;

// Class that manages the application cache service. Sends notifications
// to many frontends.  One instance per user-profile.
class AppCacheService {
 public:
  virtual ~AppCacheService();

  void Initialize(const FilePath& cache_directory);

  // TODO(jennb): API to set service settings, like file paths for storage

  // track which processes are using this appcache service
  void RegisterBackend(AppCacheBackendImpl* backend_impl);
  void UnregisterBackend(AppCacheBackendImpl* backend_impl);

  void AddCache(AppCache* cache);
  void RemoveCache(AppCache* cache);
  void AddGroup(AppCacheGroup* group);
  void RemoveGroup(AppCacheGroup* group);

  AppCacheBackendImpl* GetBackend(int id) {
    BackendMap::iterator it = backends_.find(id);
    return (it != backends_.end()) ? it->second : NULL;
  }

  AppCache* GetCache(int64 id) {
    CacheMap::iterator it = caches_.find(id);
    return (it != caches_.end()) ? it->second : NULL;
  }

  AppCacheGroup* GetGroup(const GURL& manifest_url) {
    GroupMap::iterator it = groups_.find(manifest_url);
    return (it != groups_.end()) ? it->second : NULL;
  }

 private:
  // In-memory representation of stored appcache data. Represents a subset
  // of the appcache database currently in use.
  typedef base::hash_map<int64, AppCache*> CacheMap;
  typedef std::map<GURL, AppCacheGroup*> GroupMap;
  CacheMap caches_;
  GroupMap groups_;

  // Track current processes.  One 'backend' per child process.
  typedef std::map<int, AppCacheBackendImpl*> BackendMap;
  BackendMap backends_;

  FilePath cache_directory_;
  // TODO(jennb): info about appcache storage
  // AppCacheDatabase db_;
  // DiskCache response_storage_;

  // TODO(jennb): service settings: e.g. max size of app cache?
  // TODO(jennb): service state: e.g. reached quota?
};

}  // namespace appcache

#endif  // WEBKIT_APPCACHE_APPCACHE_SERVICE_H_
