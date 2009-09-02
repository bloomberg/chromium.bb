// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/appcache/appcache_host.h"

#include "base/logging.h"
#include "webkit/appcache/appcache.h"
#include "webkit/appcache/appcache_group.h"
#include "webkit/appcache/appcache_interfaces.h"

namespace appcache {

AppCacheHost::AppCacheHost(int host_id, AppCacheFrontend* frontend)
  : host_id_(host_id),
    selected_cache_(NULL),
    group_(NULL),
    frontend_(frontend) {
}

AppCacheHost::~AppCacheHost() {
  if (selected_cache_)
    set_selected_cache(NULL);
  DCHECK(!group_);
}

void AppCacheHost::set_selected_cache(AppCache *cache) {
  if (selected_cache_)
    selected_cache_->UnassociateHost(this);

  selected_cache_ = cache;

  if (cache) {
    cache->AssociateHost(this);
    group_ = cache->owning_group();
  } else {
    group_ = NULL;
  }
}

}  // namespace appcache
