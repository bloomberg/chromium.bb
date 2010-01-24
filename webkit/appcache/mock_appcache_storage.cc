// Copyright (c) 2010 The Chromium Authors. All rights reserved.
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
        id, make_scoped_refptr(GetOrCreateDelegateReference(delegate))));
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
        manifest_url,
        make_scoped_refptr(GetOrCreateDelegateReference(delegate))));
    return;
  }
  ProcessLoadOrCreateGroup(
      manifest_url, GetOrCreateDelegateReference(delegate));
}

void MockAppCacheStorage::StoreGroupAndNewestCache(
    AppCacheGroup* group, AppCache* newest_cache, Delegate* delegate) {
  DCHECK(group && delegate && newest_cache);

  // Always make this operation look async.
  ScheduleTask(method_factory_.NewRunnableMethod(
      &MockAppCacheStorage::ProcessStoreGroupAndNewestCache,
      make_scoped_refptr(group),
      make_scoped_refptr(newest_cache),
      make_scoped_refptr(GetOrCreateDelegateReference(delegate))));
}

void MockAppCacheStorage::FindResponseForMainRequest(
    const GURL& url, Delegate* delegate) {
  DCHECK(delegate);

  // Always make this operation look async.
  ScheduleTask(method_factory_.NewRunnableMethod(
      &MockAppCacheStorage::ProcessFindResponseForMainRequest,
      url,
      make_scoped_refptr(GetOrCreateDelegateReference(delegate))));
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

  GURL fallback_namespace_not_used;
  cache->FindResponseForRequest(
      url, found_entry, found_fallback_entry,
      &fallback_namespace_not_used, found_network_namespace);
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
}

void MockAppCacheStorage::MakeGroupObsolete(
    AppCacheGroup* group, Delegate* delegate) {
  DCHECK(group && delegate);

  // Always make this method look async.
  ScheduleTask(method_factory_.NewRunnableMethod(
      &MockAppCacheStorage::ProcessMakeGroupObsolete,
      make_scoped_refptr(group),
      make_scoped_refptr(GetOrCreateDelegateReference(delegate))));
}

AppCacheResponseReader* MockAppCacheStorage::CreateResponseReader(
    const GURL& manifest_url, int64 response_id) {
  return new AppCacheResponseReader(response_id, disk_cache());
}

AppCacheResponseWriter* MockAppCacheStorage::CreateResponseWriter(
    const GURL& manifest_url) {
  return new AppCacheResponseWriter(NewResponseId(), disk_cache());
}

void MockAppCacheStorage::DoomResponses(
    const GURL& manifest_url, const std::vector<int64>& response_ids) {
  DeleteResponses(manifest_url, response_ids);
}

void MockAppCacheStorage::DeleteResponses(
    const GURL& manifest_url, const std::vector<int64>& response_ids) {
  // We don't bother with actually removing responses from the disk-cache,
  // just keep track of which ids have been doomed or deleted
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
    group = new AppCacheGroup(service_, manifest_url, NewGroupId());

  if (delegate_ref->delegate)
    delegate_ref->delegate->OnGroupLoaded(group, manifest_url);
}

void MockAppCacheStorage::ProcessStoreGroupAndNewestCache(
    scoped_refptr<AppCacheGroup> group,
    scoped_refptr<AppCache> newest_cache,
    scoped_refptr<DelegateReference> delegate_ref) {
  Delegate* delegate = delegate_ref->delegate;
  if (simulate_store_group_and_newest_cache_failure_) {
    if (delegate)
      delegate->OnGroupAndNewestCacheStored(group, newest_cache, false);
    return;
  }

  AddStoredGroup(group);
  if (newest_cache != group->newest_complete_cache()) {
    newest_cache->set_complete(true);
    group->AddCache(newest_cache);
    AddStoredCache(newest_cache);

    // Copy the collection prior to removal, on final release
    // of a cache the group's collection will change.
    AppCacheGroup::Caches copy = group->old_caches();
    RemoveStoredCaches(copy);
  }

  if (delegate)
    delegate->OnGroupAndNewestCacheStored(group, newest_cache, true);
}

namespace {

struct FoundCandidate {
  AppCacheEntry entry;
  int64 cache_id;
  GURL manifest_url;
  bool is_cache_in_use;

  FoundCandidate() : cache_id(kNoCacheId), is_cache_in_use(false) {}
};

}  // namespace

void MockAppCacheStorage::ProcessFindResponseForMainRequest(
    const GURL& url, scoped_refptr<DelegateReference> delegate_ref) {
  if (simulate_find_main_resource_) {
    simulate_find_main_resource_ = false;
    if (delegate_ref->delegate) {
      delegate_ref->delegate->OnMainResponseFound(
          url, simulated_found_entry_, simulated_found_fallback_entry_,
          simulated_found_cache_id_, simulated_found_manifest_url_);
    }
    return;
  }

  // This call has no persistent side effects, if the delegate has gone
  // away, we can just bail out early.
  if (!delegate_ref->delegate)
    return;

  // TODO(michaeln): The heuristics around choosing amoungst
  // multiple candidates is under specified, and just plain
  // not fully understood. Refine these over time. In particular,
  // * prefer candidates from newer caches
  // * take into account the cache associated with the document
  //   that initiated the navigation
  // * take into account the cache associated with the document
  //   currently residing in the frame being navigated
  FoundCandidate found_candidate;
  FoundCandidate found_fallback_candidate;
  GURL found_fallback_candidate_namespace;

  for (StoredGroupMap::const_iterator it = stored_groups_.begin();
       it != stored_groups_.end(); ++it) {
    AppCacheGroup* group = it->second.get();
    AppCache* cache = group->newest_complete_cache();
    if (group->is_obsolete() || !cache ||
        (url.GetOrigin() != group->manifest_url().GetOrigin())) {
      continue;
    }

    AppCacheEntry found_entry;
    AppCacheEntry found_fallback_entry;
    GURL found_fallback_namespace;
    bool ignore_found_network_namespace = false;
    bool found = cache->FindResponseForRequest(
                            url, &found_entry, &found_fallback_entry,
                            &found_fallback_namespace,
                            &ignore_found_network_namespace);

    // 6.11.1 Navigating across documents, Step 10.
    // Network namespacing doesn't apply to main resource loads,
    // and foreign entries are excluded.
    if (!found || ignore_found_network_namespace ||
        (found_entry.has_response_id() && found_entry.IsForeign()) ||
        (found_fallback_entry.has_response_id() &&
         found_fallback_entry.IsForeign())) {
      continue;
    }

    // We have a bias for hits from caches that are in use.
    bool is_in_use = IsCacheStored(cache) && !cache->HasOneRef();

    if (found_entry.has_response_id()) {
      found_candidate.entry = found_entry;
      found_candidate.cache_id = cache->cache_id();
      found_candidate.manifest_url = group->manifest_url();
      found_candidate.is_cache_in_use = is_in_use;
      if (is_in_use)
        break;  // We break out of the loop with this direct hit.
    } else {
      DCHECK(found_fallback_entry.has_response_id());

      bool take_new_candidate = true;

      // Does the newly found entry trump our current candidate?
      if (found_fallback_candidate.entry.has_response_id()) {
        // Longer namespace prefix matches win.
        size_t found_length =
            found_fallback_namespace.spec().length();
        size_t candidate_length =
            found_fallback_candidate_namespace.spec().length();

        if (found_length > candidate_length) {
          take_new_candidate = true;
        } else if (found_length == candidate_length &&
                   is_in_use && !found_fallback_candidate.is_cache_in_use) {
          take_new_candidate = true;
        } else {
          take_new_candidate = false;
        }
      }

      if (take_new_candidate) {
        found_fallback_candidate.entry = found_fallback_entry;
        found_fallback_candidate.cache_id = cache->cache_id();
        found_fallback_candidate.manifest_url = group->manifest_url();
        found_fallback_candidate.is_cache_in_use = is_in_use;
        found_fallback_candidate_namespace = found_fallback_namespace;
      }
    }
  }

  // Found a direct hit.
  if (found_candidate.entry.has_response_id()) {
    delegate_ref->delegate->OnMainResponseFound(
        url, found_candidate.entry, AppCacheEntry(),
        found_candidate.cache_id, found_candidate.manifest_url);
    return;
  }

  // Found a fallback namespace.
  if (found_fallback_candidate.entry.has_response_id()) {
    delegate_ref->delegate->OnMainResponseFound(
        url, AppCacheEntry(), found_fallback_candidate.entry,
        found_fallback_candidate.cache_id,
        found_fallback_candidate.manifest_url);
    return;
  }

  // Didn't find anything.
  delegate_ref->delegate->OnMainResponseFound(
      url, AppCacheEntry(), AppCacheEntry(), kNoCacheId, GURL());
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

  // Also remove from the working set, caches for an 'obsolete' group
  // may linger in use, but the group itself cannot be looked up by
  // 'manifest_url' in the working set any longer.
  working_set()->RemoveGroup(group);

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
