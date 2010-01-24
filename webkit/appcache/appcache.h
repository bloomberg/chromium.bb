// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_APPCACHE_APPCACHE_H_
#define WEBKIT_APPCACHE_APPCACHE_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/ref_counted.h"
#include "base/time.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest_prod.h"
#include "webkit/appcache/appcache_database.h"
#include "webkit/appcache/appcache_entry.h"
#include "webkit/appcache/manifest_parser.h"

namespace appcache {

class AppCacheGroup;
class AppCacheHost;
class AppCacheService;

// Set of cached resources for an application. A cache exists as long as a
// host is associated with it, the cache is in an appcache group or the
// cache is being created during an appcache upate.
class AppCache : public base::RefCounted<AppCache> {
 public:
  typedef std::map<GURL, AppCacheEntry> EntryMap;
  typedef std::set<AppCacheHost*> AppCacheHosts;

  AppCache(AppCacheService *service, int64 cache_id);

  int64 cache_id() const { return cache_id_; }

  AppCacheGroup* owning_group() const { return owning_group_; }

  bool is_complete() const { return is_complete_; }
  void set_complete(bool value) { is_complete_ = value; }

  AppCacheService* service() const { return service_; }

  // Adds a new entry. Entry must not already be in cache.
  void AddEntry(const GURL& url, const AppCacheEntry& entry);

  // Adds a new entry or modifies an existing entry by merging the types
  // of the new entry with the existing entry. Returns true if a new entry
  // is added, false if the flags are merged into an existing entry.
  bool AddOrModifyEntry(const GURL& url, const AppCacheEntry& entry);

  // Do not store the returned object as it could be deleted anytime.
  AppCacheEntry* GetEntry(const GURL& url);

  const EntryMap& entries() const { return entries_; }

  AppCacheHosts& associated_hosts() { return associated_hosts_; }

  bool IsNewerThan(AppCache* cache) const {
    if (update_time_ > cache->update_time_)
      return true;

    // Tie breaker. Newer caches have a larger cache ID.
    if (update_time_ == cache->update_time_)
      return cache_id_ > cache->cache_id_;

    return false;
  }

  base::TimeTicks update_time() const { return update_time_; }
  void set_update_time(base::TimeTicks ticks) { update_time_ = ticks; }

  // Initializes the cache with information in the manifest.
  // Do not use the manifest after this call.
  void InitializeWithManifest(Manifest* manifest);

  // Initializes the cache with the information in the database records.
  void InitializeWithDatabaseRecords(
      const AppCacheDatabase::CacheRecord& cache_record,
      const std::vector<AppCacheDatabase::EntryRecord>& entries,
      const std::vector<AppCacheDatabase::FallbackNameSpaceRecord>& fallbacks,
      const std::vector<AppCacheDatabase::OnlineWhiteListRecord>& whitelists);

  // Returns the database records to be stored in the AppCacheDatabase
  // to represent this cache.
  void ToDatabaseRecords(
      const AppCacheGroup* group,
      AppCacheDatabase::CacheRecord* cache_record,
      std::vector<AppCacheDatabase::EntryRecord>* entries,
      std::vector<AppCacheDatabase::FallbackNameSpaceRecord>* fallbacks,
      std::vector<AppCacheDatabase::OnlineWhiteListRecord>* whitelists);

  bool FindResponseForRequest(const GURL& url,
      AppCacheEntry* found_entry, AppCacheEntry* found_fallback_entry,
      GURL* found_fallback_namespace, bool* found_network_namespace);

 private:
  friend class AppCacheGroup;
  friend class AppCacheHost;
  friend class AppCacheStorageImplTest;
  friend class AppCacheUpdateJobTest;
  friend class base::RefCounted<AppCache>;

  ~AppCache();

  // Use AppCacheGroup::Add/RemoveCache() to manipulate owning group.
  void set_owning_group(AppCacheGroup* group) { owning_group_ = group; }

  // FindResponseForRequest helpers
  FallbackNamespace* FindFallbackNamespace(const GURL& url);
  bool IsInNetworkNamespace(const GURL& url);

  // Use AppCacheHost::AssociateCache() to manipulate host association.
  void AssociateHost(AppCacheHost* host) {
    associated_hosts_.insert(host);
  }
  void UnassociateHost(AppCacheHost* host);

  const int64 cache_id_;
  scoped_refptr<AppCacheGroup> owning_group_;
  AppCacheHosts associated_hosts_;

  EntryMap entries_;    // contains entries of all types

  std::vector<FallbackNamespace> fallback_namespaces_;
  std::vector<GURL> online_whitelist_namespaces_;
  bool online_whitelist_all_;

  bool is_complete_;

  // when this cache was last updated
  base::TimeTicks update_time_;

  // to notify service when cache is deleted
  AppCacheService* service_;

  FRIEND_TEST(AppCacheTest, InitializeWithManifest);
  DISALLOW_COPY_AND_ASSIGN(AppCache);
};

}  // namespace appcache

#endif  // WEBKIT_APPCACHE_APPCACHE_H_
