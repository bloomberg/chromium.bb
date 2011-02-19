// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/appcache/appcache_host.h"

#include "base/logging.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "webkit/appcache/appcache.h"
#include "webkit/appcache/appcache_backend_impl.h"
#include "webkit/appcache/appcache_request_handler.h"

namespace appcache {

namespace {

void FillCacheInfo(
    const AppCache* cache, Status status, AppCacheInfo* info) {
  DCHECK(cache);
  info->cache_id = cache->cache_id();
  info->status = status;
  info->is_complete = cache->is_complete();
  if (info->is_complete) {
    info->manifest_url = cache->owning_group()->manifest_url();
    info->last_update_time = cache->update_time();
    info->creation_time = cache->owning_group()->creation_time();
    info->size = cache->cache_size();
  }
}

}  // Anonymous namespace

AppCacheHost::AppCacheHost(int host_id, AppCacheFrontend* frontend,
                           AppCacheService* service)
    : host_id_(host_id), parent_host_id_(kNoHostId), parent_process_id_(0),
      pending_main_resource_cache_id_(kNoCacheId),
      pending_selected_cache_id_(kNoCacheId),
      frontend_(frontend), service_(service),
      pending_get_status_callback_(NULL), pending_start_update_callback_(NULL),
      pending_swap_cache_callback_(NULL), pending_callback_param_(NULL),
      main_resource_was_fallback_(false), main_resource_blocked_(false),
      associated_cache_info_pending_(false) {
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
         !pending_get_status_callback_ &&
         !is_selection_pending());

  if (main_resource_blocked_)
    frontend_->OnContentBlocked(host_id_,
                                blocked_manifest_url_);

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

void AppCacheHost::SelectCacheForWorker(int parent_process_id,
                                        int parent_host_id) {
  DCHECK(!pending_start_update_callback_ &&
         !pending_swap_cache_callback_ &&
         !pending_get_status_callback_ &&
         !is_selection_pending());

  parent_process_id_ = parent_process_id;
  parent_host_id_ = parent_host_id;
  FinishCacheSelection(NULL, NULL);
}

void AppCacheHost::SelectCacheForSharedWorker(int64 appcache_id) {
  DCHECK(!pending_start_update_callback_ &&
         !pending_swap_cache_callback_ &&
         !pending_get_status_callback_ &&
         !is_selection_pending());

  if (appcache_id != kNoCacheId) {
    LoadSelectedCache(appcache_id);
    return;
  }
  FinishCacheSelection(NULL, NULL);
}

// TODO(michaeln): change method name to MarkEntryAsForeign for consistency
void AppCacheHost::MarkAsForeignEntry(const GURL& document_url,
                                      int64 cache_document_was_loaded_from) {
  // The document url is not the resource url in the fallback case.
  service_->storage()->MarkEntryAsForeign(
      main_resource_was_fallback_ ? fallback_url_ : document_url,
      cache_document_was_loaded_from);
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
    if (!group->is_obsolete() && !group->is_being_deleted()) {
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

AppCacheHost* AppCacheHost::GetParentAppCacheHost() const {
  DCHECK(is_for_dedicated_worker());
  AppCacheBackendImpl* backend = service_->GetBackend(parent_process_id_);
  return backend ? backend->GetHost(parent_host_id_) : NULL;
}

AppCacheRequestHandler* AppCacheHost::CreateRequestHandler(
    net::URLRequest* request,
    ResourceType::Type resource_type) {
  if (is_for_dedicated_worker()) {
    AppCacheHost* parent_host = GetParentAppCacheHost();
    if (parent_host)
      return parent_host->CreateRequestHandler(request, resource_type);
    return NULL;
  }

  if (AppCacheRequestHandler::IsMainResourceType(resource_type))
    return new AppCacheRequestHandler(this, resource_type);

  if ((associated_cache() && associated_cache()->is_complete()) ||
      is_selection_pending()) {
    return new AppCacheRequestHandler(this, resource_type);
  }
  return NULL;
}

void AppCacheHost::GetResourceList(
    std::vector<AppCacheResourceInfo>* resource_infos) {
  if (associated_cache_.get() && associated_cache_->is_complete()) {
    for (AppCache::EntryMap::const_iterator it =
         associated_cache_->entries().begin();
         it != associated_cache_->entries().end(); ++it) {
      AppCacheResourceInfo info;
      info.url = it->first;
      info.is_master = it->second.IsMaster();
      info.is_manifest = it->second.IsManifest();
      info.is_fallback = it->second.IsFallback();
      info.is_foreign = it->second.IsForeign();
      info.is_explicit = it->second.IsExplicit();
      info.size = it->second.response_size();
      resource_infos->push_back(info);
    }
  }
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
    AppCacheGroup* owing_group = cache->owning_group();
    const char* kFormatString =
        "Document was loaded from Application Cache with manifest %s";
    frontend_->OnLogMessage(
        host_id_, LOG_INFO,
        base::StringPrintf(
            kFormatString, owing_group->manifest_url().spec().c_str()));
    AssociateCache(cache);
    if (!owing_group->is_obsolete() && !owing_group->is_being_deleted()) {
      owing_group->StartUpdateWithHost(this);
      ObserveGroupBeingUpdated(owing_group);
    }
  } else if (group && !group->is_being_deleted()) {
    // If document was loaded using HTTP GET or equivalent, and, there is a
    // manifest URL, and manifest URL has the same origin as document.
    // Invoke the application cache update process for manifest URL, with
    // the browsing context being navigated, and with document and the
    // resource from which document was loaded as the new master resourse.
    DCHECK(!group->is_obsolete());
    DCHECK(new_master_entry_url_.is_valid());
    const char* kFormatString = group->HasCache() ?
        "Adding master entry to Application Cache with manifest %s" :
        "Creating Application Cache with manifest %s";
    frontend_->OnLogMessage(
        host_id_, LOG_INFO,
        base::StringPrintf(kFormatString,
                           group->manifest_url().spec().c_str()));
    AssociateCache(NULL);  // The UpdateJob may produce one for us later.
    group->StartUpdateWithNewMasterEntry(this, new_master_entry_url_);
    ObserveGroupBeingUpdated(group);
  } else {
    // Otherwise, the Document is not associated with any application cache.
    new_master_entry_url_ = GURL();
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

  if (associated_cache_info_pending_ && associated_cache_.get() &&
      associated_cache_->is_complete()) {
    AppCacheInfo info;
    FillCacheInfo(associated_cache_.get(), GetStatus(), &info);
    associated_cache_info_pending_ = false;
    frontend_->OnCacheSelected(host_id_, info);
  }
}

void AppCacheHost::OnContentBlocked(AppCacheGroup* group) {
  frontend_->OnContentBlocked(host_id_, group->manifest_url());
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

void AppCacheHost::NotifyMainResourceFallback(const GURL& fallback_url) {
  main_resource_was_fallback_ = true;
  fallback_url_ = fallback_url;
}

void AppCacheHost::NotifyMainResourceBlocked(const GURL& manifest_url) {
  main_resource_blocked_ = true;
  blocked_manifest_url_ = manifest_url;
}

void AppCacheHost::AssociateCache(AppCache* cache) {
  if (associated_cache_.get()) {
    associated_cache_->UnassociateHost(this);
  }

  associated_cache_ = cache;
  SetSwappableCache(cache ? cache->owning_group() : NULL);
  associated_cache_info_pending_ = cache && !cache->is_complete();
  AppCacheInfo info;
  if (cache) {
    cache->AssociateHost(this);
    FillCacheInfo(cache, GetStatus(), &info);
  }
  frontend_->OnCacheSelected(host_id_, info);
}

}  // namespace appcache
