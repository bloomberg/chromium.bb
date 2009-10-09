// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/appcache/mock_appcache_storage.h"

#include "base/logging.h"
#include "base/ref_counted.h"
#include "webkit/appcache/appcache.h"
#include "webkit/appcache/appcache_backend_impl.h"
#include "webkit/appcache/appcache_entry.h"
#include "webkit/appcache/appcache_group.h"
#include "webkit/appcache/appcache_response.h"

namespace appcache {

MockAppCacheStorage::MockAppCacheStorage(AppCacheService* service)
    : AppCacheStorage(service) {
  last_cache_id_ = 0;
  last_entry_id_ = 0;
  last_group_id_ = 0;
  last_response_id_ = 0;
}

void MockAppCacheStorage::LoadCache(int64 id, Delegate* delegate) {
  AppCache* cache = working_set_.GetCache(id);
  delegate->OnCacheLoaded(cache, id);
}

void MockAppCacheStorage::LoadOrCreateGroup(
    const GURL& manifest_url, Delegate* delegate) {
  scoped_refptr<AppCacheGroup> group = working_set_.GetGroup(manifest_url);
  if (!group.get()) {
    group = new AppCacheGroup(service_, manifest_url);
    DCHECK(working_set_.GetGroup(manifest_url));
  }
  delegate->OnGroupLoaded(group.get(), manifest_url);
}

void MockAppCacheStorage::LoadResponseInfo(
    const GURL& manifest_url, int64 id, Delegate* delegate) {
  delegate->OnResponseInfoLoaded(working_set_.GetResponseInfo(id), id);
}

void MockAppCacheStorage::StoreGroupAndNewestCache(
    AppCacheGroup* group, Delegate* delegate) {
  // TODO(michaeln): write me
  delegate->OnGroupAndNewestCacheStored(group, false);
}

void MockAppCacheStorage::FindResponseForMainRequest(
    const GURL& url, Delegate* delegate) {
  // TODO(michaeln): write me
  delegate->OnMainResponseFound(
      url, kNoResponseId, false, kNoCacheId, GURL::EmptyGURL());
}

void MockAppCacheStorage::MarkEntryAsForeign(
    const GURL& entry_url, int64 cache_id) {
  // Update the working set.
  AppCache* cache = working_set_.GetCache(cache_id);
  if (cache) {
    AppCacheEntry* entry = cache->GetEntry(entry_url);
    DCHECK(entry);
    if (entry)
      entry->add_types(AppCacheEntry::FOREIGN);
  }
  // TODO(michaeln): actually update in storage, and if this cache is
  // being loaded be sure to update the memory cache upon load completion.
}

void MockAppCacheStorage::MarkGroupAsObsolete(
    AppCacheGroup* group, Delegate* delegate) {
  // TODO(michaeln): write me
}

void MockAppCacheStorage::CancelDelegateCallbacks(Delegate* delegate) {
  // TODO(michaeln): remove delegate from callback list
}

AppCacheResponseReader* MockAppCacheStorage::CreateResponseReader(
    const GURL& origin, int64 response_id) {
  return new AppCacheResponseReader(response_id, NULL);
  // TODO(michaeln): use a disk_cache
}

AppCacheResponseWriter* MockAppCacheStorage::CreateResponseWriter(
    const GURL& manifest_url) {
  return new AppCacheResponseWriter(NewResponseId(), NULL);
  // TODO(michaeln): use a disk_cache
}

void MockAppCacheStorage::DoomResponses(
    const GURL& manifest_url, const std::vector<int64>& response_ids) {
  // TODO(michaeln): write me
}

}  // namespace appcache
