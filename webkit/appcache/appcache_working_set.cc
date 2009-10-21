// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/appcache/appcache_working_set.h"

#include "base/logging.h"
#include "webkit/appcache/appcache.h"
#include "webkit/appcache/appcache_group.h"
#include "webkit/appcache/appcache_response.h"

namespace appcache {

AppCacheWorkingSet::~AppCacheWorkingSet() {
  DCHECK(caches_.empty());
  DCHECK(groups_.empty());
}

void AppCacheWorkingSet::AddCache(AppCache* cache) {
  DCHECK(cache->cache_id() != kNoCacheId);
  int64 cache_id = cache->cache_id();
  DCHECK(caches_.find(cache_id) == caches_.end());
  caches_.insert(CacheMap::value_type(cache_id, cache));
}

void AppCacheWorkingSet::RemoveCache(AppCache* cache) {
  caches_.erase(cache->cache_id());
}

void AppCacheWorkingSet::AddGroup(AppCacheGroup* group) {
  const GURL& url = group->manifest_url();
  DCHECK(groups_.find(url) == groups_.end());
  groups_.insert(GroupMap::value_type(url, group));
}

void AppCacheWorkingSet::RemoveGroup(AppCacheGroup* group) {
  groups_.erase(group->manifest_url());
}

void AppCacheWorkingSet::AddResponseInfo(AppCacheResponseInfo* info) {
  DCHECK(info->response_id() != kNoResponseId);
  int64 response_id = info->response_id();
  DCHECK(response_infos_.find(response_id) == response_infos_.end());
  response_infos_.insert(ResponseInfoMap::value_type(response_id, info));
}

void AppCacheWorkingSet::RemoveResponseInfo(AppCacheResponseInfo* info) {
  response_infos_.erase(info->response_id());
}

}  // namespace
