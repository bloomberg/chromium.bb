// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TETHER_HOST_SCAN_CACHE_H_
#define CHROMEOS_COMPONENTS_TETHER_HOST_SCAN_CACHE_H_

#include <string>
#include <unordered_set>

#include "base/macros.h"
#include "base/observer_list.h"
#include "chromeos/components/tether/host_scan_cache_entry.h"

namespace chromeos {

namespace tether {

// Caches host scan results.
class HostScanCache {
 public:
  HostScanCache();
  virtual ~HostScanCache();

  class Observer {
   public:
    virtual void OnCacheBecameEmpty() {}
  };

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Updates the cache to include this scan result. If no scan result for
  // |tether_network_guid| exists in the cache, a scan result will be added;
  // if a scan result is already present, it is updated with the new data
  // provided as parameters to this function.
  virtual void SetHostScanResult(const HostScanCacheEntry& entry) = 0;

  // Removes the scan result with GUID |tether_network_guid| from the cache, and
  // notifies observers if the cache becomes empty. If no scan result with that
  // GUID was present in the cache, this function is a no-op. Returns whether a
  // scan result was actually removed.
  bool RemoveHostScanResult(const std::string& tether_network_guid);

  // Returns whether an entry corresponding to GUId |tether_network_gui| exists
  // in the cache.
  virtual bool ExistsInCache(const std::string& tether_network_guid) = 0;

  // Returns a set of all Tether network GUIDs that are present in the cache.
  virtual std::unordered_set<std::string> GetTetherGuidsInCache() = 0;

  // Returns whether the scan result corresponding to |tether_network_guid|
  // requires first-time setup (i.e., user interaction) to allow tethering.
  virtual bool DoesHostRequireSetup(const std::string& tether_network_guid) = 0;

 protected:
  virtual bool RemoveHostScanResultImpl(
      const std::string& tether_network_guid) = 0;

 private:
  base::ObserverList<Observer>::Unchecked observer_list_;

  DISALLOW_COPY_AND_ASSIGN(HostScanCache);
};

}  // namespace tether

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TETHER_HOST_SCAN_CACHE_H_
