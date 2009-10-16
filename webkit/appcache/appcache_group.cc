// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/appcache/appcache_group.h"

#include <algorithm>

#include "base/logging.h"
#include "webkit/appcache/appcache.h"
#include "webkit/appcache/appcache_host.h"
#include "webkit/appcache/appcache_service.h"
#include "webkit/appcache/appcache_storage.h"
#include "webkit/appcache/appcache_update_job.h"

namespace appcache {

AppCacheGroup::AppCacheGroup(AppCacheService* service,
                             const GURL& manifest_url)
    : manifest_url_(manifest_url),
      update_status_(IDLE),
      is_obsolete_(false),
      newest_complete_cache_(NULL),
      update_job_(NULL),
      service_(service) {
  service_->storage()->working_set()->AddGroup(this);
}

AppCacheGroup::~AppCacheGroup() {
  DCHECK(old_caches_.empty());
  DCHECK(!newest_complete_cache_);

  if (update_job_)
    delete update_job_;
  DCHECK_EQ(IDLE, update_status_);

  service_->storage()->working_set()->RemoveGroup(this);
}

void AppCacheGroup::AddUpdateObserver(UpdateObserver* observer) {
  observers_.AddObserver(observer);
}

void AppCacheGroup::RemoveUpdateObserver(UpdateObserver* observer) {
  observers_.RemoveObserver(observer);
}

void AppCacheGroup::AddCache(AppCache* complete_cache) {
  DCHECK(complete_cache->is_complete());
  complete_cache->set_owning_group(this);

  if (!newest_complete_cache_) {
    newest_complete_cache_ = complete_cache;
    return;
  }

  if (complete_cache->IsNewerThan(newest_complete_cache_)) {
    old_caches_.push_back(newest_complete_cache_);
    newest_complete_cache_ = complete_cache;

    // Update hosts of older caches to add a reference to the newest cache.
    for (Caches::iterator it = old_caches_.begin();
         it != old_caches_.end(); ++it) {
      AppCache::AppCacheHosts& hosts = (*it)->associated_hosts();
      for (AppCache::AppCacheHosts::iterator host_it = hosts.begin();
           host_it != hosts.end(); ++host_it) {
        (*host_it)->SetSwappableCache(this);
      }
    }
  } else {
    old_caches_.push_back(complete_cache);
  }
}

void AppCacheGroup::RemoveCache(AppCache* cache) {
  DCHECK(cache->associated_hosts().empty());
  if (cache == newest_complete_cache_) {
    AppCache* cache = newest_complete_cache_;
    newest_complete_cache_ = NULL;
    cache->set_owning_group(NULL);  // may cause this group to be deleted
  } else {
    Caches::iterator it =
        std::find(old_caches_.begin(), old_caches_.end(), cache);
    if (it != old_caches_.end()) {
      AppCache* cache = *it;
      old_caches_.erase(it);
      cache->set_owning_group(NULL);  // may cause group to be deleted
    }
  }
}

void AppCacheGroup::StartUpdateWithNewMasterEntry(
    AppCacheHost* host, const GURL& new_master_resource) {
  if (!update_job_)
    update_job_ = new AppCacheUpdateJob(service_, this);

  update_job_->StartUpdate(host, new_master_resource);
}

void AppCacheGroup::SetUpdateStatus(UpdateStatus status) {
  if (status == update_status_)
    return;

  update_status_ = status;

  if (status != IDLE) {
    DCHECK(update_job_);
  } else {
    update_job_ = NULL;
    FOR_EACH_OBSERVER(UpdateObserver, observers_, OnUpdateComplete(this));
  }
}

}  // namespace appcache
