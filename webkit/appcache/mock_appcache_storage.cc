// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/appcache/mock_appcache_storage.h"

#include "base/logging.h"
#include "base/message_loop.h"
#include "base/ref_counted.h"
#include "base/stl_util-inl.h"
#include "webkit/appcache/appcache.h"
#include "webkit/appcache/appcache_entry.h"
#include "webkit/appcache/appcache_group.h"
#include "webkit/appcache/appcache_response.h"

// This is a quick and easy 'mock' implementation of the storage interface
// that doesn't put anything to disk.
//
// We simply add an extra reference to objects when they're put in storage,
// and remove the extra reference when they are removed from storage.
// Responses are never really removed from the in-memory disk cache.
// Delegate callbacks are made asyncly to appropiately mimic what will
// happen with a real disk-backed storage impl that involves IO on a
// background thread.

namespace appcache {

MockAppCacheStorage::MockAppCacheStorage(AppCacheService* service)
    : AppCacheStorage(service),
      ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)),
      simulate_make_group_obsolete_failure_(false),
      simulate_store_group_and_newest_cache_failure_(false),
      simulate_find_main_resource_(false),
      simulate_find_sub_resource_(false),
      simulated_found_cache_id_(kNoCacheId),
      simulated_found_network_namespace_(false) {
  last_cache_id_ = 0;
  last_entry_id_ = 0;
  last_group_id_ = 0;
  last_response_id_ = 0;
}

MockAppCacheStorage::~MockAppCacheStorage() {
  STLDeleteElements(&pending_tasks_);
}

void MockAppCacheStorage::LoadCache(int64 id, Delegate* delegate) {
  DCHECK(delegate);
  AppCache* cache = working_set_.GetCache(id);
  if (ShouldCacheLoadAppearAsync(cache)) {
    ScheduleTask(method_factory_.NewRunnableMethod(
        &MockAppCacheStorage::ProcessLoadCache,
        id, GetOrCreateDelegateReference(delegate)));
    return;
  }
  ProcessLoadCache(id, GetOrCreateDelegateReference(delegate));
}

void MockAppCacheStorage::LoadOrCreateGroup(
    const GURL& manifest_url, Delegate* delegate) {
  DCHECK(delegate);
  AppCacheGroup* group = working_set_.GetGroup(manifest_url);
  if (ShouldGroupLoadAppearAsync(group)) {
    ScheduleTask(method_factory_.NewRunnableMethod(
        &MockAppCacheStorage::ProcessLoadOrCreateGroup,
        manifest_url, GetOrCreateDelegateReference(delegate)));
    return;
  }
  ProcessLoadOrCreateGroup(
      manifest_url, GetOrCreateDelegateReference(delegate));
}

void MockAppCacheStorage::StoreGroupAndNewestCache(
    AppCacheGroup* group, Delegate* delegate) {
  DCHECK(group && delegate);
  DCHECK(group->newest_complete_cache());

  // Always make this operation look async.
  ScheduleTask(method_factory_.NewRunnableMethod(
      &MockAppCacheStorage::ProcessStoreGroupAndNewestCache,
      group, group->newest_complete_cache(),
      GetOrCreateDelegateReference(delegate)));
}

void MockAppCacheStorage::FindResponseForMainRequest(
    const GURL& url, Delegate* delegate) {
  DCHECK(delegate);

  // Always make this operation look async.
  ScheduleTask(method_factory_.NewRunnableMethod(
      &MockAppCacheStorage::ProcessFindResponseForMainRequest,
      url, GetOrCreateDelegateReference(delegate)));
}

void MockAppCacheStorage::FindResponseForSubRequest(
    AppCache* cache, const GURL& url,
    AppCacheEntry* found_entry, AppCacheEntry* found_fallback_entry,
    bool* found_network_namespace) {
  DCHECK(cache && cache->is_complete());

  // This layer of indirection is here to facilitate testing.
  if (simulate_find_sub_resource_) {
    *found_entry = simulated_found_entry_;
    *found_fallback_entry = simulated_found_fallback_entry_;
    *found_network_namespace = simulated_found_network_namespace_;
    simulate_find_sub_resource_ = false;
    return;
  }

  cache->FindResponseForRequest(url, found_entry, found_fallback_entry,
                                found_network_namespace);
}

void MockAppCacheStorage::MarkEntryAsForeign(
    const GURL& entry_url, int64 cache_id) {
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

void MockAppCacheStorage::MakeGroupObsolete(
    AppCacheGroup* group, Delegate* delegate) {
  DCHECK(group && delegate);

  // Always make this method look async.
  ScheduleTask(method_factory_.NewRunnableMethod(
      &MockAppCacheStorage::ProcessMakeGroupObsolete,
      group, GetOrCreateDelegateReference(delegate)));
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
  // We don't bother with actually removing responses from the disk-cache,
  // just keep track of which ids have been doomed.
  std::vector<int64>::const_iterator it = response_ids.begin();
  while (it != response_ids.end()) {
    doomed_response_ids_.insert(*it);
    ++it;
  }
}

void MockAppCacheStorage::ProcessLoadCache(
    int64 id, scoped_refptr<DelegateReference> delegate_ref) {
  AppCache* cache = working_set_.GetCache(id);
  if (delegate_ref->delegate)
    delegate_ref->delegate->OnCacheLoaded(cache, id);
}

void MockAppCacheStorage::ProcessLoadOrCreateGroup(
    const GURL& manifest_url, scoped_refptr<DelegateReference> delegate_ref) {
  scoped_refptr<AppCacheGroup> group = working_set_.GetGroup(manifest_url);

  // Newly created groups are not put in the stored_groups collection
  // until StoreGroupAndNewestCache is called.
  if (!group)
    group = new AppCacheGroup(service_, manifest_url);

  if (delegate_ref->delegate)
    delegate_ref->delegate->OnGroupLoaded(group, manifest_url);
}

void MockAppCacheStorage::ProcessStoreGroupAndNewestCache(
    scoped_refptr<AppCacheGroup> group,
    scoped_refptr<AppCache> newest_cache,
    scoped_refptr<DelegateReference> delegate_ref) {
  DCHECK(group->newest_complete_cache() == newest_cache.get());

  if (simulate_store_group_and_newest_cache_failure_) {
    if (delegate_ref->delegate)
      delegate_ref->delegate->OnGroupAndNewestCacheStored(group, false);
    return;
  }

  AddStoredGroup(group);
  AddStoredCache(group->newest_complete_cache());

  // Copy the collection prior to removal, on final release
  // of a cache the group's collection will change.
  AppCacheGroup::Caches copy = group->old_caches();
  RemoveStoredCaches(copy);

  if (delegate_ref->delegate)
    delegate_ref->delegate->OnGroupAndNewestCacheStored(group, true);

  // We don't bother with removing responses from 'mock' storage
  // TODO(michaeln): for 'real' storage...
  // std::set<int64> doomed_responses_ = responses from old caches
  // std::set<int64> needed_responses_ = responses from newest cache
  // foreach(needed_responses_)
  //  doomed_responses_.remove(needed_response_);
  // DoomResponses(group->manifest_url(), doomed_responses_);
}

void MockAppCacheStorage::ProcessFindResponseForMainRequest(
    const GURL& url, scoped_refptr<DelegateReference> delegate_ref) {
  // TODO(michaeln): write me when doing AppCacheRequestHandler
  // foreach(stored_group) {
  //   if (group->manifest_url()->origin() != url.GetOrigin())
  //     continue;
  //   look for an entry
  //   look for a fallback namespace
  //   look for a online namespace
  // }
  AppCacheEntry found_entry;
  AppCacheEntry found_fallback_entry;
  int64 found_cache_id = kNoCacheId;
  GURL found_manifest_url = GURL();
  if (simulate_find_main_resource_) {
    found_entry = simulated_found_entry_;
    found_fallback_entry = simulated_found_fallback_entry_;
    found_cache_id = simulated_found_cache_id_;
    found_manifest_url = simulated_found_manifest_url_;
    simulate_find_main_resource_ = false;
  }
  if (delegate_ref->delegate) {
    delegate_ref->delegate->OnMainResponseFound(
        url, found_entry, found_fallback_entry,
        found_cache_id, found_manifest_url);
  }
}

void MockAppCacheStorage::ProcessMakeGroupObsolete(
    scoped_refptr<AppCacheGroup> group,
    scoped_refptr<DelegateReference> delegate_ref) {
  if (simulate_make_group_obsolete_failure_) {
    if (delegate_ref->delegate)
      delegate_ref->delegate->OnGroupMadeObsolete(group, false);
    return;
  }

  RemoveStoredGroup(group);
  if (group->newest_complete_cache())
    RemoveStoredCache(group->newest_complete_cache());

  // Copy the collection prior to removal, on final release
  // of a cache the group's collection will change.
  AppCacheGroup::Caches copy = group->old_caches();
  RemoveStoredCaches(copy);

  group->set_obsolete(true);

  if (delegate_ref->delegate)
    delegate_ref->delegate->OnGroupMadeObsolete(group, true);
}

void MockAppCacheStorage::ScheduleTask(Task* task) {
  pending_tasks_.push_back(task);
  MessageLoop::current()->PostTask(FROM_HERE,
      method_factory_.NewRunnableMethod(
          &MockAppCacheStorage::RunOnePendingTask));
}

void MockAppCacheStorage::RunOnePendingTask() {
  DCHECK(!pending_tasks_.empty());
  Task* task = pending_tasks_.front();
  pending_tasks_.pop_front();
  task->Run();
  delete task;
}

void MockAppCacheStorage::AddStoredCache(AppCache* cache) {
  int64 cache_id = cache->cache_id();
  if (stored_caches_.find(cache_id) == stored_caches_.end())
    stored_caches_.insert(StoredCacheMap::value_type(cache_id, cache));
}

void MockAppCacheStorage::RemoveStoredCache(AppCache* cache) {
  // Do not remove from the working set, active caches are still usable
  // and may be looked up by id until they fall out of use.
  stored_caches_.erase(cache->cache_id());
}

void MockAppCacheStorage::RemoveStoredCaches(
    const AppCacheGroup::Caches& caches) {
  AppCacheGroup::Caches::const_iterator it = caches.begin();
  while (it != caches.end()) {
    RemoveStoredCache(*it);
    ++it;
  }
}

void MockAppCacheStorage::AddStoredGroup(AppCacheGroup* group) {
  const GURL& url = group->manifest_url();
  if (stored_groups_.find(url) == stored_groups_.end())
    stored_groups_.insert(StoredGroupMap::value_type(url, group));
}

void MockAppCacheStorage::RemoveStoredGroup(AppCacheGroup* group) {
  // Also remove from the working set, caches for an 'obsolete' group
  // may linger in use, but the group itself cannot be looked up by
  // 'manifest_url' in the working set any longer.
  working_set()->RemoveGroup(group);
  stored_groups_.erase(group->manifest_url());
}

bool MockAppCacheStorage::ShouldGroupLoadAppearAsync(
    const AppCacheGroup* group) {
  // We'll have to query the database to see if a group for the
  // manifest_url exists on disk. So return true for async.
  if (!group)
    return true;

  // Groups without a newest cache can't have been put to disk yet, so
  // we can synchronously return a reference we have in the working set.
  if (!group->newest_complete_cache())
    return false;

  // The LoadGroup interface implies also loading the newest cache, so
  // if loading the newest cache should appear async, so too must the
  // loading of this group.
  if (ShouldCacheLoadAppearAsync(group->newest_complete_cache()))
    return true;


  // If any of the old caches are "in use", then the group must also
  // be memory resident and not require async loading.
  const AppCacheGroup::Caches& old_caches = group->old_caches();
  AppCacheGroup::Caches::const_iterator it = old_caches.begin();
  while (it != old_caches.end()) {
    // "in use" caches don't require async loading
    if (!ShouldCacheLoadAppearAsync(*it))
      return false;
    ++it;
  }

  return true;
}

bool MockAppCacheStorage::ShouldCacheLoadAppearAsync(const AppCache* cache) {
  if (!cache)
    return true;

  // If the 'stored' ref is the only ref, real storage will have to load from
  // the database.
  return IsCacheStored(cache) && cache->HasOneRef();
}

}  // namespace appcache
