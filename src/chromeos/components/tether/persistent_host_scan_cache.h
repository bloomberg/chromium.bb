// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TETHER_PERSISTENT_HOST_SCAN_CACHE_H_
#define CHROMEOS_COMPONENTS_TETHER_PERSISTENT_HOST_SCAN_CACHE_H_

#include "base/macros.h"
#include "chromeos/components/tether/host_scan_cache.h"

namespace chromeos {

namespace tether {

// HostScanCache implementation which stores scan results in persistent user
// prefs.
class PersistentHostScanCache : virtual public HostScanCache {
 public:
  PersistentHostScanCache() {}
  ~PersistentHostScanCache() override {}

  // Returns the cache entries that are currently stored in user prefs as a map
  // from Tether network GUID to entry.
  virtual std::unordered_map<std::string, HostScanCacheEntry>
  GetStoredCacheEntries() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(PersistentHostScanCache);
};

}  // namespace tether

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TETHER_PERSISTENT_HOST_SCAN_CACHE_H_
