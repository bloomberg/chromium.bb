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
#include "base/ref_counted.h"
#include "base/task.h"
#include "googleurl/src/gurl.h"

class URLRequestContext;

namespace appcache {

class AppCache;
class AppCacheBackendImpl;
class AppCacheGroup;

// Class that manages the application cache service. Sends notifications
// to many frontends.  One instance per user-profile. Each instance has
// exclusive access to it's cache_directory on disk.
class AppCacheService {
 public:

  class LoadClient {
   public:
    virtual ~LoadClient() {}

    // If a load fails the object pointer will be NULL.
    virtual void CacheLoadedCallback(AppCache* cache, int64 cache_id) = 0;
    virtual void GroupLoadedCallback(AppCacheGroup* group,
                                     const GURL& manifest_url) = 0;
  };

  AppCacheService();
  virtual ~AppCacheService();

  void Initialize(const FilePath& cache_directory);

  // Context for use during cache updates, should only be accessed
  // on the IO thread. We do NOT add a reference to the request context,
  // it is the callers responsibility to ensure that the pointer
  // remains valid while set.
  URLRequestContext* request_context() const { return request_context_; }
  void set_request_context(URLRequestContext* context) {
    request_context_ = context;
  }

  // TODO(jennb): API to set service settings, like file paths for storage

  // Track which processes are using this appcache service.
  void RegisterBackend(AppCacheBackendImpl* backend_impl);
  void UnregisterBackend(AppCacheBackendImpl* backend_impl);
  AppCacheBackendImpl* GetBackend(int id) {
    BackendMap::iterator it = backends_.find(id);
    return (it != backends_.end()) ? it->second : NULL;
  }

  // Track what we have in our in-memory cache.
  void AddCache(AppCache* cache);
  void RemoveCache(AppCache* cache);
  void AddGroup(AppCacheGroup* group);
  void RemoveGroup(AppCacheGroup* group);
  AppCache* GetCache(int64 id) {
    CacheMap::iterator it = caches_.find(id);
    return (it != caches_.end()) ? it->second : NULL;
  }
  AppCacheGroup* GetGroup(const GURL& manifest_url) {
    GroupMap::iterator it = groups_.find(manifest_url);
    return (it != groups_.end()) ? it->second : NULL;
  }

  // Load caches and groups from storage. If the request object
  // is already in memory, the client is called immediately
  // without returning to the message loop.
  void LoadCache(int64 id, LoadClient* client);
  void LoadOrCreateGroup(const GURL& manifest_url,
                         LoadClient* client);

  // Cancels pending callbacks for this client.
  void CancelLoads(LoadClient* client);

  // Updates in memory and persistent storage.
  void MarkAsForeignEntry(const GURL& entry_url, int64 cache_id);

  // The service generates unique storage ids for different object types.
  int64 NewCacheId() { return ++last_cache_id_; }
  int64 NewGroupId() { return ++last_group_id_; }
  int64 NewEntryId() { return ++last_entry_id_; }
  int64 NewResponseId() { return ++last_response_id_; }

 private:
  // In-memory representation of stored appcache data. Represents a subset
  // of the appcache database currently in use.
  typedef base::hash_map<int64, AppCache*> CacheMap;
  typedef std::map<GURL, AppCacheGroup*> GroupMap;
  CacheMap caches_;
  GroupMap groups_;

  // The last storage id used for different object types.
  int64 last_cache_id_;
  int64 last_group_id_;
  int64 last_entry_id_;
  int64 last_response_id_;

  // Track current processes.  One 'backend' per child process.
  typedef std::map<int, AppCacheBackendImpl*> BackendMap;
  BackendMap backends_;

  // Where we save our data.
  FilePath cache_directory_;

  // Context for use during cache updates.
  URLRequestContext* request_context_;

  // TODO(michaeln): cache and group loading book keeping.
  // TODO(michaeln): database and response storage
  // TODO(jennb): service state: e.g. reached quota?
};

}  // namespace appcache

#endif  // WEBKIT_APPCACHE_APPCACHE_SERVICE_H_
