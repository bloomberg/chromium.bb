// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_APPCACHE_APPCACHE_H_
#define WEBKIT_APPCACHE_APPCACHE_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/time.h"
#include "googleurl/src/gurl.h"
#include "webkit/appcache/appcache_entry.h"
#include "webkit/appcache/manifest_parser.h"

namespace appcache {

class AppCacheGroup;
class AppCacheHost;
class AppCacheService;

// Set of cached resources for an application.
class AppCache {
 public:
   // TODO(jennb): need constructor to set cache_id and service
  ~AppCache();

  int64 cache_id() { return cache_id_; }

  AppCacheGroup* owning_group() { return owning_group_; }
  void set_owning_group(AppCacheGroup* group) { owning_group_ = group; }

  void AssociateHost(AppCacheHost* host) {
    associated_hosts_.insert(host);
  }

  // Cache may be deleted after host is unassociated.
  void UnassociateHost(AppCacheHost* host);

  bool is_complete() { return is_complete_; }
  void set_complete(bool value) { is_complete_ = value; }

  // Adds a new entry. Entry must not already be in cache.
  void AddEntry(const GURL& url, const AppCacheEntry& entry);

  // Adds a new entry or modifies an existing entry by merging the types
  // of the new entry with the existing entry.
  void AddOrModifyEntry(const GURL& url, const AppCacheEntry& entry);

  // Do not store the returned object as it could be deleted anytime.
  AppCacheEntry* GetEntry(const GURL& url);

  typedef std::map<GURL, AppCacheEntry> EntryMap;
  const EntryMap& entries() { return entries_; }

  bool IsNewerThan(AppCache* cache) {
    return update_time_ > cache->update_time_;
  }

 private:
  int64 cache_id_;
  AppCacheEntry* manifest_;  // also in entry map
  AppCacheGroup* owning_group_;
  std::set<AppCacheHost*> associated_hosts_;

  EntryMap entries_;    // contains entries of all types

  std::vector<FallbackNamespace> fallback_namespaces_;
  std::vector<GURL> online_whitelist_namespaces_;
  bool online_whitelist_all_;

  bool is_complete_;

  // when this cache was last updated
  base::TimeTicks update_time_;

  // to notify service when cache is deleted
  AppCacheService* service_;
};

}  // namespace appcache

#endif  // WEBKIT_APPCACHE_APPCACHE_H_
