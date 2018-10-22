// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_NQE_UI_NETWORK_QUALITY_ESTIMATOR_SERVICE_H_
#define CHROME_BROWSER_NET_NQE_UI_NETWORK_QUALITY_ESTIMATOR_SERVICE_H_

#include <stdint.h>

#include <map>
#include <memory>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/keyed_service/core/keyed_service.h"
#include "net/nqe/cached_network_quality.h"
#include "net/nqe/network_id.h"

class PrefRegistrySimple;
class Profile;

namespace net {
class NetworkQualitiesPrefsManager;
}

// UI service to manage storage of network quality prefs.
class UINetworkQualityEstimatorService : public KeyedService {
 public:
  explicit UINetworkQualityEstimatorService(Profile* profile);
  ~UINetworkQualityEstimatorService() override;

  // Registers the profile-specific network quality estimator prefs.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Clear the network quality estimator prefs.
  void ClearPrefs();

  // Reads the prefs from the disk, parses them into a map of NetworkIDs and
  // CachedNetworkQualities, and returns the map.
  std::map<net::nqe::internal::NetworkID,
           net::nqe::internal::CachedNetworkQuality>
  ForceReadPrefsForTesting() const;

 private:
  // KeyedService implementation:
  void Shutdown() override;

  // Prefs manager that is owned by this service. Created on the UI thread, but
  // used and deleted on the IO thread.
  std::unique_ptr<net::NetworkQualitiesPrefsManager> prefs_manager_;

  base::WeakPtrFactory<UINetworkQualityEstimatorService> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(UINetworkQualityEstimatorService);
};

#endif  // CHROME_BROWSER_NET_NQE_UI_NETWORK_QUALITY_ESTIMATOR_SERVICE_H_
