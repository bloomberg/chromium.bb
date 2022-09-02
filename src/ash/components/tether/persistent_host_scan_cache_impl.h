// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_TETHER_PERSISTENT_HOST_SCAN_CACHE_IMPL_H_
#define ASH_COMPONENTS_TETHER_PERSISTENT_HOST_SCAN_CACHE_IMPL_H_

#include <string>
#include <unordered_map>
#include <unordered_set>

#include "ash/components/tether/persistent_host_scan_cache.h"

class PrefRegistrySimple;
class PrefService;

namespace ash {

namespace tether {

// HostScanCache implementation which stores scan results in persistent user
// prefs.
class PersistentHostScanCacheImpl : public PersistentHostScanCache {
 public:
  // Registers the prefs used by this class to the given |registry|.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  PersistentHostScanCacheImpl(PrefService* pref_service);

  PersistentHostScanCacheImpl(const PersistentHostScanCacheImpl&) = delete;
  PersistentHostScanCacheImpl& operator=(const PersistentHostScanCacheImpl&) =
      delete;

  ~PersistentHostScanCacheImpl() override;

  // HostScanCache:
  void SetHostScanResult(const HostScanCacheEntry& entry) override;
  bool ExistsInCache(const std::string& tether_network_guid) override;
  std::unordered_set<std::string> GetTetherGuidsInCache() override;
  bool DoesHostRequireSetup(const std::string& tether_network_guid) override;

  // PersistentHostScanCache:
  std::unordered_map<std::string, HostScanCacheEntry> GetStoredCacheEntries()
      override;

 protected:
  bool RemoveHostScanResultImpl(
      const std::string& tether_network_guid) override;

 private:
  void StoreCacheEntriesToPrefs(
      const std::unordered_map<std::string, HostScanCacheEntry>& entries);

  PrefService* pref_service_;
};

}  // namespace tether

}  // namespace ash

#endif  // ASH_COMPONENTS_TETHER_PERSISTENT_HOST_SCAN_CACHE_IMPL_H_
