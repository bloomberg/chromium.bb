// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/appcache/appcache.h"

#include "base/logging.h"
#include "webkit/appcache/appcache_group.h"
#include "webkit/appcache/appcache_host.h"
#include "webkit/appcache/appcache_interfaces.h"
#include "webkit/appcache/appcache_service.h"
#include "webkit/appcache/appcache_storage.h"

namespace appcache {

AppCache::AppCache(AppCacheService *service, int64 cache_id)
    : cache_id_(cache_id),
      owning_group_(NULL),
      online_whitelist_all_(false),
      is_complete_(false),
      service_(service) {
  service_->storage()->working_set()->AddCache(this);
}

AppCache::~AppCache() {
  DCHECK(associated_hosts_.empty());
  if (owning_group_) {
    DCHECK(is_complete_);
    owning_group_->RemoveCache(this);
  }
  DCHECK(!owning_group_);
  service_->storage()->working_set()->RemoveCache(this);
}

void AppCache::UnassociateHost(AppCacheHost* host) {
  associated_hosts_.erase(host);
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

void AppCache::InitializeWithManifest(Manifest* manifest) {
  DCHECK(manifest);
  fallback_namespaces_.swap(manifest->fallback_namespaces);
  online_whitelist_namespaces_.swap(manifest->online_whitelist_namespaces);
  online_whitelist_all_ = manifest->online_whitelist_all;
}

}  // namespace appcache
