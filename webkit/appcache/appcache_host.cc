// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/appcache/appcache_host.h"

#include "base/logging.h"
#include "webkit/appcache/appcache_request_handler.h"

namespace appcache {

AppCacheHost::AppCacheHost(int host_id, AppCacheFrontend* frontend,
                           AppCacheService* service)
    : host_id_(host_id), pending_main_resource_cache_id_(kNoCacheId),
      pending_selected_cache_id_(kNoCacheId),
      frontend_(frontend), service_(service),
      pending_get_status_callback_(NULL), pending_start_update_callback_(NULL),
      pending_swap_cache_callback_(NULL), pending_callback_param_(NULL) {
}

AppCacheHost::~AppCacheHost() {
  FOR_EACH_OBSERVER(Observer, observers_, OnDestructionImminent(this));
  if (associated_cache_.get())
    associated_cache_->UnassociateHost(this);
  if (group_being_updated_.get())
    group_being_updated_->RemoveUpdateObserver(this);
  service_->storage()->CancelDelegateCallbacks(this);
}

void AppCacheHost::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void AppCacheHost::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void AppCacheHost::SelectCache(const GURL& document_url,
                               const int64 cache_document_was_loaded_from,
                               const GURL& manifest_url) {
  DCHECK(!pending_start_update_callback_ &&
         !pending_swap_cache_callback_ &&
         !pending_get_status_callback_);

  // First we handle an unusual case of SelectCache being called a second
  // time. Generally this shouldn't happen, but with bad content I think
  // this can occur... <html manifest=foo> <html manifest=bar></html></html>
  // We handle this by killing whatever loading we have initiated, and by
  // unassociating any hosts we currently have associated... and starting
  // anew with the inputs to this SelectCache call.
  // TODO(michaeln): at some point determine what behavior the algorithms
  // described in the HTML5 draft produce and have our impl produce those
  // results (or suggest changes to the algorihtms described in the spec
  // if the resulting behavior is just too insane).
  if (is_selection_pending()) {
    service_->storage()->CancelDelegateCallbacks(this);
    pending_selected_manifest_url_ = GURL();
    pending_selected_cache_id_ = kNoCacheId;
  } else if (associated_cache()) {
    AssociateCache(NULL);
  }
  new_master_entry_url_ = GURL();

  // 6.9.6 The application cache selection algorithm.
  // The algorithm is started here and continues in FinishCacheSelection,
  // after cache or group loading is complete.
  // Note: Foreign entries are detected on the client side and
  // MarkAsForeignEntry is called in that case, so that detection
  // step is skipped here. See WebApplicationCacheHostImpl.cc

  if (cache_document_was_loaded_from != kNoCacheId) {
    LoadSelectedCache(cache_document_was_loaded_from);
    return;
  }

  if (!manifest_url.is_empty() &&
      (manifest_url.GetOrigin() == document_url.GetOrigin())) {
    // Note: The client detects if the document was not loaded using HTTP GET
    // and invokes SelectCache without a manifest url, so that detection step
    // is also skipped here. See WebApplicationCacheHostImpl.cc
    new_master_entry_url_ = document_url;
    LoadOrCreateGroup(manifest_url);
    return;
  }

  // TODO(michaeln): If there was a manifest URL, the user agent may report
  // to the user that it was ignored, to aid in application development.
  FinishCacheSelection(NULL, NULL);
}

// TODO(michaeln): change method name to MarkEntryAsForeign for consistency
void AppCacheHost::MarkAsForeignEntry(const GURL& document_url,
                                      int64 cache_document_was_loaded_from) {
  service_->storage()->MarkEntryAsForeign(
      document_url, cache_document_was_loaded_from);
  SelectCache(document_url, kNoCacheId, GURL());
}

void AppCacheHost::GetStatusWithCallback(GetStatusCallback* callback,
                                         void* callback_param) {
  DCHECK(!pending_start_update_callback_ &&
         !pending_swap_cache_callback_ &&
         !pending_get_status_callback_);

  pending_get_status_callback_ = callback;
  pending_callback_param_ = callback_param;
  if (is_selection_pending())
    return;

  DoPendingGetStatus();
}

void AppCacheHost::DoPendingGetStatus() {
  DCHECK(pending_get_status_callback_);

  pending_get_status_callback_->Run(
      GetStatus(), pending_callback_param_);

  pending_get_status_callback_ = NULL;
  pending_callback_param_ = NULL;
}

void AppCacheHost::StartUpdateWithCallback(StartUpdateCallback* callback,
                                           void* callback_param) {
  DCHECK(!pending_start_update_callback_ &&
         !pending_swap_cache_callback_ &&
         !pending_get_status_callback_);

  pending_start_update_callback_ = callback;
  pending_callback_param_ = callback_param;
  if (is_selection_pending())
    return;

  DoPendingStartUpdate();
}

void AppCacheHost::DoPendingStartUpdate() {
  DCHECK(pending_start_update_callback_);

  // 6.9.8 Application cache API
  bool success = false;
  if (associated_cache_ && associated_cache_->owning_group()) {
    AppCacheGroup* group = associated_cache_->owning_group();
    if (!group->is_obsolete()) {
      success = true;
      group->StartUpdate();
    }
  }

  pending_start_update_callback_->Run(
      success, pending_callback_param_);

  pending_start_update_callback_ = NULL;
  pending_callback_param_ = NULL;
}

void AppCacheHost::SwapCacheWithCallback(SwapCacheCallback* callback,
                                         void* callback_param) {
  DCHECK(!pending_start_update_callback_ &&
         !pending_swap_cache_callback_ &&
         !pending_get_status_callback_);

  pending_swap_cache_callback_ = callback;
  pending_callback_param_ = callback_param;
  if (is_selection_pending())
    return;

  DoPendingSwapCache();
}

void AppCacheHost::DoPendingSwapCache() {
  DCHECK(pending_swap_cache_callback_);

  // 6.9.8 Application cache API
  bool success = false;
  if (associated_cache_ && associated_cache_->owning_group()) {
    if (associated_cache_->owning_group()->is_obsolete()) {
      success = true;
      AssociateCache(NULL);
    } else if (swappable_cache_) {
      DCHECK(swappable_cache_.get() ==
             swappable_cache_->owning_group()->newest_complete_cache());
      success = true;
      AssociateCache(swappable_cache_);
    }
  }

  pending_swap_cache_callback_->Run(
      success, pending_callback_param_);

  pending_swap_cache_callback_ = NULL;
  pending_callback_param_ = NULL;
}

AppCacheRequestHandler* AppCacheHost::CreateRequestHandler(
                                          URLRequest* request,
                                          bool is_main_request) {
  if (is_main_request)
    return new AppCacheRequestHandler(this, true);

  if ((associated_cache() && associated_cache()->is_complete()) ||
      is_selection_pending()) {
    return new AppCacheRequestHandler(this, false);
  }
  return NULL;
}

Status AppCacheHost::GetStatus() {
  // 6.9.8 Application cache API
  AppCache* cache = associated_cache();
  if (!cache)
    return UNCACHED;

  // A cache without an owning group represents the cache being constructed
  // during the application cache update process.
  if (!cache->owning_group())
    return DOWNLOADING;

  if (cache->owning_group()->is_obsolete())
    return OBSOLETE;
  if (cache->owning_group()->update_status() == AppCacheGroup::CHECKING)
    return CHECKING;
  if (cache->owning_group()->update_status() == AppCacheGroup::DOWNLOADING)
    return DOWNLOADING;
  if (swappable_cache_)
    return UPDATE_READY;
  return IDLE;
}

void AppCacheHost::LoadOrCreateGroup(const GURL& manifest_url) {
  DCHECK(manifest_url.is_valid());
  pending_selected_manifest_url_ = manifest_url;
  service_->storage()->LoadOrCreateGroup(manifest_url, this);
}

void AppCacheHost::OnGroupLoaded(AppCacheGroup* group,
                                const GURL& manifest_url) {
  DCHECK(manifest_url == pending_selected_manifest_url_);
  pending_selected_manifest_url_ = GURL();
  FinishCacheSelection(NULL, group);
}

void AppCacheHost::LoadSelectedCache(int64 cache_id) {
  DCHECK(cache_id != kNoCacheId);
  pending_selected_cache_id_ = cache_id;
  service_->storage()->LoadCache(cache_id, this);
}

void AppCacheHost::OnCacheLoaded(AppCache* cache, int64 cache_id) {
  if (cache_id == pending_main_resource_cache_id_) {
    pending_main_resource_cache_id_ = kNoCacheId;
    main_resource_cache_ = cache;
  } else if (cache_id == pending_selected_cache_id_) {
    pending_selected_cache_id_ = kNoCacheId;
    FinishCacheSelection(cache, NULL);
  }
}

void AppCacheHost::FinishCacheSelection(
    AppCache *cache, AppCacheGroup* group) {
  DCHECK(!associated_cache());

  // 6.9.6 The application cache selection algorithm
  if (cache) {
    // If document was loaded from an application cache, Associate document
    // with the application cache from which it was loaded. Invoke the
    // application cache update process for that cache and with the browsing
    // context being navigated.
    DCHECK(cache->owning_group());
    DCHECK(new_master_entry_url_.is_empty());
    AssociateCache(cache);
    if (!cache->owning_group()->is_obsolete()) {
      cache->owning_group()->StartUpdateWithHost(this);
      ObserveGroupBeingUpdated(cache->owning_group());
    }
  } else if (group) {
    // If document was loaded using HTTP GET or equivalent, and, there is a
    // manifest URL, and manifest URL has the same origin as document.
    // Invoke the application cache update process for manifest URL, with
    // the browsing context being navigated, and with document and the
    // resource from which document was loaded as the new master resourse.
    DCHECK(!group->is_obsolete());
    DCHECK(new_master_entry_url_.is_valid());
    AssociateCache(NULL);  // The UpdateJob may produce one for us later.
    group->StartUpdateWithNewMasterEntry(this, new_master_entry_url_);
    ObserveGroupBeingUpdated(group);
  } else {
    // Otherwise, the Document is not associated with any application cache.
    AssociateCache(NULL);
  }

  // Respond to pending callbacks now that we have a selection.
  if (pending_get_status_callback_)
    DoPendingGetStatus();
  else if (pending_start_update_callback_)
    DoPendingStartUpdate();
  else if (pending_swap_cache_callback_)
    DoPendingSwapCache();

  FOR_EACH_OBSERVER(Observer, observers_, OnCacheSelectionComplete(this));
}

void AppCacheHost::ObserveGroupBeingUpdated(AppCacheGroup* group) {
  DCHECK(!group_being_updated_);
  group_being_updated_ = group;
  newest_cache_of_group_being_updated_ = group->newest_complete_cache();
  group->AddUpdateObserver(this);
}

void AppCacheHost::OnUpdateComplete(AppCacheGroup* group) {
  DCHECK_EQ(group, group_being_updated_);
  group->RemoveUpdateObserver(this);

  // Add a reference to the newest complete cache.
  SetSwappableCache(group);

  group_being_updated_ = NULL;
  newest_cache_of_group_being_updated_ = NULL;
}

void AppCacheHost::SetSwappableCache(AppCacheGroup* group) {
  if (!group) {
    swappable_cache_ = NULL;
  } else {
    AppCache* new_cache = group->newest_complete_cache();
    if (new_cache != associated_cache_)
      swappable_cache_ = new_cache;
    else
      swappable_cache_ = NULL;
  }
}

void AppCacheHost::LoadMainResourceCache(int64 cache_id) {
  DCHECK(cache_id != kNoCacheId);
  if (pending_main_resource_cache_id_ == cache_id ||
      (main_resource_cache_ && main_resource_cache_->cache_id() == cache_id)) {
    return;
  }
  pending_main_resource_cache_id_ = cache_id;
  service_->storage()->LoadCache(cache_id, this);
}

void AppCacheHost::AssociateCache(AppCache* cache) {
  if (associated_cache_.get()) {
    associated_cache_->UnassociateHost(this);
  }

  associated_cache_ = cache;
  SetSwappableCache(cache ? cache->owning_group() : NULL);

  if (cache) {
    cache->AssociateHost(this);
    frontend_->OnCacheSelected(host_id_, cache->cache_id(), GetStatus());
  } else {
    frontend_->OnCacheSelected(host_id_, kNoCacheId, UNCACHED);
  }
}

}  // namespace appcache
