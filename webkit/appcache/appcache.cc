// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/appcache/appcache.h"

#include "base/logging.h"
#include "webkit/appcache/appcache_group.h"
#include "webkit/appcache/appcache_host.h"
#include "webkit/appcache/appcache_service.h"

namespace appcache {

AppCache::~AppCache() {
  DCHECK(associated_hosts_.empty());
  DCHECK(!owning_group_);
  service_->RemoveCache(this);
}

void AppCache::UnassociateHost(AppCacheHost* host) {
  associated_hosts_.erase(host);

  // Inform group if this cache is no longer in use.
  if (associated_hosts_.empty()) {
    if (!owning_group_ || owning_group_->RemoveCache(this)) {
      owning_group_ = NULL;
      delete this;
    }
  }
}

void AppCache::AddEntry(const GURL& url, const AppCacheEntry& entry) {
  DCHECK(entries_.find(url) == entries_.end());
  entries_.insert(EntryMap::value_type(url, entry));
}

void AppCache::AddOrModifyEntry(const GURL& url, const AppCacheEntry& entry) {
  std::pair<EntryMap::iterator, bool> ret =
    entries_.insert(EntryMap::value_type(url, entry));

  // Entry already exists.  Merge the types of the new and existing entries.
  if (!ret.second)
    ret.first->second.add_types(entry.types());
}

AppCacheEntry* AppCache::GetEntry(const GURL& url) {
  EntryMap::iterator it = entries_.find(url);
  return (it != entries_.end()) ? &(it->second) : NULL;
}

}  // namespace appcache
