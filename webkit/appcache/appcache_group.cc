// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/appcache/appcache_group.h"

#include <algorithm>

#include "base/logging.h"
#include "webkit/appcache/appcache.h"
#include "webkit/appcache/appcache_service.h"

namespace appcache {

AppCacheGroup::AppCacheGroup(AppCacheService* service,
                             const GURL& manifest_url)
  : manifest_url_(manifest_url),
    update_status_(IDLE),
    is_obsolete_(false),
    newest_complete_cache_(NULL),
    service_(service) {
  service_->AddGroup(this);
}

AppCacheGroup::~AppCacheGroup() {
  DCHECK(old_caches_.empty());

  // Newest complete cache might never have been associated with a host
  // and thus would not be cleaned up by the backend impl during shutdown.
  if (newest_complete_cache_) {
    delete newest_complete_cache_;
  }

  service_->RemoveGroup(this);
}

void AppCacheGroup::AddCache(AppCache* complete_cache) {
  if (!newest_complete_cache_) {
    newest_complete_cache_ = complete_cache;
    return;
  }

  if (complete_cache->IsNewerThan(newest_complete_cache_)) {
    old_caches_.push_back(newest_complete_cache_);
    newest_complete_cache_ = complete_cache;
  } else {
    old_caches_.push_back(complete_cache);
  }
}

bool AppCacheGroup::RemoveCache(AppCache* cache) {
  if (cache == newest_complete_cache_) {

    // Cannot remove newest cache if there are older caches as those may
    // eventually be swapped to the newest cache.
    if (!old_caches_.empty())
      return false;

    newest_complete_cache_ = NULL;  // cache will be deleted by caller
  } else {
    // Unused old cache can always be removed.
    std::vector<AppCache*>::iterator it =
      std::find(old_caches_.begin(), old_caches_.end(), cache);
    if (it != old_caches_.end())
      old_caches_.erase(it);        // cache will be deleted by caller
  }

  if (old_caches_.empty() && !newest_complete_cache_) {
    delete this;
  }

  return true;
}

}  // namespace appcache
