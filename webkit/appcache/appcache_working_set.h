// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_APPCACHE_APPCACHE_WORKING_SET_H_
#define WEBKIT_APPCACHE_APPCACHE_WORKING_SET_H_

#include <map>

#include "base/hash_tables.h"
#include "googleurl/src/gurl.h"

namespace appcache {

class AppCache;
class AppCacheGroup;
class AppCacheResponseInfo;

// Represents the working set of appcache object instances
// currently in memory.
class AppCacheWorkingSet {
 public:
  ~AppCacheWorkingSet();

  void AddCache(AppCache* cache);
  void RemoveCache(AppCache* cache);
  AppCache* GetCache(int64 id) {
    CacheMap::iterator it = caches_.find(id);
    return (it != caches_.end()) ? it->second : NULL;
  }

  void AddGroup(AppCacheGroup* group);
  void RemoveGroup(AppCacheGroup* group);
  AppCacheGroup* GetGroup(const GURL& manifest_url) {
    GroupMap::iterator it = groups_.find(manifest_url);
    return (it != groups_.end()) ? it->second : NULL;
  }

  void AddResponseInfo(AppCacheResponseInfo* response_info);
  void RemoveResponseInfo(AppCacheResponseInfo* response_info);
  AppCacheResponseInfo* GetResponseInfo(int64 id) {
    ResponseInfoMap::iterator it = response_infos_.find(id);
    return (it != response_infos_.end()) ? it->second : NULL;
  }

 private:
  typedef base::hash_map<int64, AppCache*> CacheMap;
  typedef std::map<GURL, AppCacheGroup*> GroupMap;
  typedef base::hash_map<int64, AppCacheResponseInfo*> ResponseInfoMap;
  CacheMap caches_;
  GroupMap groups_;
  ResponseInfoMap response_infos_;
};

}  // namespace appcache

#endif  // WEBKIT_APPCACHE_APPCACHE_WORKING_SET_H_
