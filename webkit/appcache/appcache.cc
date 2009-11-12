// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "webkit/appcache/appcache.h"

#include "base/logging.h"
#include "base/string_util.h"
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

namespace {

bool SortByLength(
    const FallbackNamespace& lhs, const FallbackNamespace& rhs) {
  return lhs.first.spec().length() > rhs.first.spec().length();
}

}

void AppCache::InitializeWithManifest(Manifest* manifest) {
  DCHECK(manifest);
  fallback_namespaces_.swap(manifest->fallback_namespaces);
  online_whitelist_namespaces_.swap(manifest->online_whitelist_namespaces);
  online_whitelist_all_ = manifest->online_whitelist_all;

  // Sort the fallback namespaces by url string length, longest to shortest,
  // since longer matches trump when matching a url to a namespace.
  std::sort(fallback_namespaces_.begin(), fallback_namespaces_.end(),
            SortByLength);
}

bool AppCache::FindResponseForRequest(const GURL& url,
    AppCacheEntry* found_entry, AppCacheEntry* found_fallback_entry,
    GURL* found_fallback_namespace, bool* found_network_namespace) {
  AppCacheEntry* entry = GetEntry(url);
  if (entry) {
    *found_entry = *entry;
    return true;
  }

  FallbackNamespace* fallback_namespace = FindFallbackNamespace(url);
  if (fallback_namespace) {
    entry = GetEntry(fallback_namespace->second);
    DCHECK(entry);
    *found_fallback_entry = *entry;
    *found_fallback_namespace = fallback_namespace->first;
    return true;
  }

  *found_network_namespace = IsInNetworkNamespace(url);
  return *found_network_namespace;
}

FallbackNamespace* AppCache::FindFallbackNamespace(const GURL& url) {
  size_t count = fallback_namespaces_.size();
  for (size_t i = 0; i < count; ++i) {
    if (StartsWithASCII(
            url.spec(), fallback_namespaces_[i].first.spec(), true)) {
      return &fallback_namespaces_[i];
    }
  }
  return NULL;
}

bool AppCache::IsInNetworkNamespace(const GURL& url) {
  if (online_whitelist_all_)
    return true;

  // TODO(michaeln): There are certainly better 'prefix matching'
  // structures and algorithms that can be applied here and above.
  size_t count = online_whitelist_namespaces_.size();
  for (size_t i = 0; i < count; ++i) {
    if (StartsWithASCII(
            url.spec(), online_whitelist_namespaces_[i].spec(), true)) {
      return true;
    }
  }
  return false;
}

}  // namespace appcache
