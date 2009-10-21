// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/appcache/mock_appcache_storage.h"

#include "base/logging.h"
#include "base/ref_counted.h"
#include "webkit/appcache/appcache.h"
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
  if (cache->HasOneRef()) {
    // TODO(michaeln): make the load look async
  }
  delegate->OnCacheLoaded(cache, id);
}

void MockAppCacheStorage::LoadOrCreateGroup(
    const GURL& manifest_url, Delegate* delegate) {
  scoped_refptr<AppCacheGroup> group = working_set_.GetGroup(manifest_url);
  if (!group.get()) {
    group = new AppCacheGroup(service_, manifest_url);
    DCHECK(working_set_.GetGroup(manifest_url));
  }
  // TODO(michaeln): make the load look async if all of the groups
  // caches only have one ref
  delegate->OnGroupLoaded(group.get(), manifest_url);
}

void MockAppCacheStorage::StoreGroupAndNewestCache(
    AppCacheGroup* group, Delegate* delegate) {
  DCHECK(group->newest_complete_cache());

  // TODO(michaeln): write me
  // 'store' the group + newest
  // 'unstore' the old caches
  // figure out which responses can be doomed and doom them
  // OldRepsonses - NewestResponse == ToBeDoomed

  // TODO(michaeln): Make this appear async
  //AddStoredGroup(group);
  //AddStoredCache(group->newest_complete_cache());
  //
  //foreach(group->old_caches())
  //  RemoveStoredCache(old_cache);
  //std::set<int64> doomed_responses_ = responses from old caches
  //std::set<int64> needed_responses_ = responses from newest cache
  //foreach(needed_responses_)
  //  doomed_responses_.remove(needed_response_);
  //DoomResponses(group->manifest_url(), doomed_responses_);

  delegate->OnGroupAndNewestCacheStored(group, false);
}

void MockAppCacheStorage::FindResponseForMainRequest(
    const GURL& url, Delegate* delegate) {
  // TODO(michaeln): write me
  //
  //foreach(stored_group) {
  //  if (group->manifest_url()->origin() != url.GetOrigin())
  //    continue;
  //
    // look for an entry
    // look for a fallback namespace
    // look for a online namespace
  //}
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
  // TODO(michaeln): in real storage update in storage, and if this cache is
  // being loaded be sure to update the memory cache upon load completion.
}

void MockAppCacheStorage::MarkGroupAsObsolete(
    AppCacheGroup* group, Delegate* delegate) {
  // TODO(michaeln): write me
  // remove from working_set
  // remove from storage
  // doom things
  // group->set_obsolete(true);
  // TODO(michaeln): Make this appear async
}

AppCacheResponseReader* MockAppCacheStorage::CreateResponseReader(
    const GURL& origin, int64 response_id) {
  return new AppCacheResponseReader(response_id, disk_cache());
}

AppCacheResponseWriter* MockAppCacheStorage::CreateResponseWriter(
    const GURL& manifest_url) {
  return new AppCacheResponseWriter(NewResponseId(), disk_cache());
}

void MockAppCacheStorage::DoomResponses(
    const GURL& manifest_url, const std::vector<int64>& response_ids) {
  // We don't bother with deleting responses from mock storage.
}

void MockAppCacheStorage::AddStoredCache(AppCache* cache) {
//  cache->set_is_stored(true);
  int64 cache_id = cache->cache_id();
  if (stored_caches_.find(cache_id) == stored_caches_.end())
    stored_caches_.insert(StoredCacheMap::value_type(cache_id, cache));
}

void MockAppCacheStorage::RemoveStoredCache(AppCache* cache) {
//  cache->set_is_stored(false);
  stored_caches_.erase(cache->cache_id());
}

void MockAppCacheStorage::AddStoredGroup(AppCacheGroup* group) {
//  group->set_is_stored(true);
  const GURL& url = group->manifest_url();
  if (stored_groups_.find(url) == stored_groups_.end())
    stored_groups_.insert(StoredGroupMap::value_type(url, group));
}

void MockAppCacheStorage::RemoveStoredGroup(AppCacheGroup* group) {
//  group->set_is_stored(false);
  stored_groups_.erase(group->manifest_url());
}

}  // namespace appcache
