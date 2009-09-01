// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/appcache/appcache_service.h"

#include "base/logging.h"
#include "webkit/appcache/appcache.h"
#include "webkit/appcache/appcache_backend_impl.h"
#include "webkit/appcache/appcache_group.h"

namespace appcache {

AppCacheService::~AppCacheService() {
  DCHECK(backends_.empty());
  DCHECK(caches_.empty());
  DCHECK(groups_.empty());
}

void AppCacheService::Initialize(const FilePath& cache_directory) {
  // An empty cache directory indicates chrome incognito.
  cache_directory_ = cache_directory;
}

void AppCacheService::RegisterBackend(
    AppCacheBackendImpl* backend_impl) {
  DCHECK(backends_.find(backend_impl->process_id()) == backends_.end());
  backends_.insert(
      BackendMap::value_type(backend_impl->process_id(), backend_impl));
}

void AppCacheService::UnregisterBackend(
    AppCacheBackendImpl* backend_impl) {
  backends_.erase(backend_impl->process_id());
}

void AppCacheService::AddCache(AppCache* cache) {
  int64 cache_id = cache->cache_id();
  DCHECK(caches_.find(cache_id) == caches_.end());
  caches_.insert(CacheMap::value_type(cache_id, cache));
}

void AppCacheService::RemoveCache(AppCache* cache) {
  caches_.erase(cache->cache_id());
}

void AppCacheService::AddGroup(AppCacheGroup* group) {
  const GURL& url = group->manifest_url();
  DCHECK(groups_.find(url) == groups_.end());
  groups_.insert(GroupMap::value_type(url, group));
}

void AppCacheService::RemoveGroup(AppCacheGroup* group) {
  groups_.erase(group->manifest_url());
}

}  // namespace appcache
