// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/appcache/appcache_host.h"

#include "base/logging.h"
#include "webkit/appcache/appcache.h"
#include "webkit/appcache/appcache_group.h"
#include "webkit/appcache/appcache_interfaces.h"
#include "webkit/appcache/appcache_request_handler.h"
#include "webkit/appcache/appcache_service.h"

namespace appcache {

AppCacheHost::AppCacheHost(int host_id, AppCacheFrontend* frontend,
                           AppCacheService* service)
    : host_id_(host_id), pending_selected_cache_id_(kNoCacheId),
      frontend_(frontend), service_(service),
      pending_get_status_callback_(NULL), pending_start_update_callback_(NULL),
      pending_swap_cache_callback_(NULL), pending_callback_param_(NULL) {
}

AppCacheHost::~AppCacheHost() {
  if (associated_cache_.get())
    associated_cache_->UnassociateHost(this);
  service_->CancelLoads(this);
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
    service_->CancelLoads(this);
    pending_selected_manifest_url_ = GURL::EmptyGURL();
    pending_selected_cache_id_ = kNoCacheId;
  } else if (associated_cache()) {
    AssociateCache(NULL);
  }
  new_master_entry_url_ = GURL::EmptyGURL();

  // 6.9.6 The application cache selection algorithm.
  // The algorithm is started here and continues in FinishCacheSelection,
  // after cache or group loading is complete.
  // Note: foriegn entries are detected on the client side and
  // MarkAsForeignEntry is called in that case, so that detection
  // step is skipped here.

  if (cache_document_was_loaded_from != kNoCacheId) {
    LoadCache(cache_document_was_loaded_from);
    return;
  }

  if (!manifest_url.is_empty() &&
      (manifest_url.GetOrigin() == document_url.GetOrigin())) {
    new_master_entry_url_ = document_url;
    LoadOrCreateGroup(manifest_url);
    return;
  }

  // TODO(michaeln): If there was a manifest URL, the user agent may report
  // to the user that it was ignored, to aid in application development.
  FinishCacheSelection(NULL, NULL);
}

void AppCacheHost::MarkAsForeignEntry(const GURL& document_url,
                                      int64 cache_document_was_loaded_from) {
  service_->MarkAsForeignEntry(document_url, cache_document_was_loaded_from);
  SelectCache(document_url, kNoCacheId, GURL::EmptyGURL());
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

  // TODO(michaeln): start an update if appropiate to do so
  pending_start_update_callback_->Run(
      false, pending_callback_param_);

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

  // TODO(michaeln): swap if we have a cache that can be swapped.
  pending_swap_cache_callback_->Run(
      false, pending_callback_param_);

  pending_swap_cache_callback_ = NULL;
  pending_callback_param_ = NULL;
}

AppCacheRequestHandler* AppCacheHost::CreateRequestHandler(
                                          URLRequest* request,
                                          bool is_main_request) {
  if (is_main_request)
    return new AppCacheRequestHandler(this);

  if (associated_cache() && associated_cache()->is_complete())
    return new AppCacheRequestHandler(associated_cache());

  return NULL;
}

Status AppCacheHost::GetStatus() {
  // TODO(michaeln): determine a real status value
  Status status = associated_cache() ? IDLE : UNCACHED;
  return status;
}

void AppCacheHost::LoadOrCreateGroup(const GURL& manifest_url) {
  DCHECK(manifest_url.is_valid());
  pending_selected_manifest_url_ = manifest_url;
  service_->LoadOrCreateGroup(manifest_url, this);
}

void AppCacheHost::GroupLoadedCallback(
    AppCacheGroup* group, const GURL& manifest_url) {
  DCHECK(manifest_url == pending_selected_manifest_url_);
  pending_selected_manifest_url_ = GURL::EmptyGURL();
  FinishCacheSelection(NULL, group);
}

void AppCacheHost::LoadCache(int64 cache_id) {
  DCHECK(cache_id != kNoCacheId);
  pending_selected_cache_id_ = cache_id;
  service_->LoadCache(cache_id, this);
}

void AppCacheHost::CacheLoadedCallback(AppCache* cache, int64 cache_id) {
  DCHECK(cache_id == pending_selected_cache_id_);
  pending_selected_cache_id_ = kNoCacheId;
  if (cache)
    FinishCacheSelection(cache, NULL);
  else
    FinishCacheSelection(NULL, NULL);
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
    cache->owning_group()->StartUpdateWithHost(this);

  } else if (group) {
    // If document was loaded using HTTP GET or equivalent, and, there is a
    // manifest URL, and manifest URL has the same origin as document.
    // Invoke the application cache update process for manifest URL, with
    // the browsing context being navigated, and with document and the
    // resource from which document was loaded as the new master resourse.
    DCHECK(new_master_entry_url_.is_valid());
    AssociateCache(NULL);  // The UpdateJob may produce one for us later.
    group->StartUpdateWithNewMasterEntry(this, new_master_entry_url_);

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
}

void AppCacheHost::AssociateCache(AppCache* cache) {
  if (associated_cache_.get()) {
    associated_cache_->UnassociateHost(this);
    group_ = NULL;
  }

  associated_cache_ = cache;

  if (cache) {
    cache->AssociateHost(this);
    group_ = cache->owning_group();
    frontend_->OnCacheSelected(host_id_, cache->cache_id(), GetStatus());
  } else {
    frontend_->OnCacheSelected(host_id_, kNoCacheId, UNCACHED);
  }
}

}  // namespace appcache
