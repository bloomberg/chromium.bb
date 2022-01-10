// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_SYNC_WIFI_PENDING_NETWORK_CONFIGURATION_TRACKER_IMPL_H_
#define CHROMEOS_COMPONENTS_SYNC_WIFI_PENDING_NETWORK_CONFIGURATION_TRACKER_IMPL_H_

#include "chromeos/components/sync_wifi/pending_network_configuration_tracker.h"
#include "components/prefs/pref_service.h"

class PrefRegistrySimple;
class PrefService;

namespace chromeos {

namespace sync_wifi {

// Keeps track of in flight updates to the local network stack and persists
// them to disk.
class PendingNetworkConfigurationTrackerImpl
    : public PendingNetworkConfigurationTracker {
 public:
  explicit PendingNetworkConfigurationTrackerImpl(PrefService* pref_service);

  PendingNetworkConfigurationTrackerImpl(
      const PendingNetworkConfigurationTrackerImpl&) = delete;
  PendingNetworkConfigurationTrackerImpl& operator=(
      const PendingNetworkConfigurationTrackerImpl&) = delete;

  ~PendingNetworkConfigurationTrackerImpl() override;

  // Registers preferences used by this class in the provided |registry|.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // sync_wifi::PendingNetworkConfigurationTracker::
  std::string TrackPendingUpdate(
      const NetworkIdentifier& id,
      const absl::optional<sync_pb::WifiConfigurationSpecifics>& specifics)
      override;
  void MarkComplete(const std::string& change_guid,
                    const NetworkIdentifier& id) override;
  void IncrementCompletedAttempts(const std::string& change_guid,
                                  const NetworkIdentifier& id) override;
  std::vector<PendingNetworkConfigurationUpdate> GetPendingUpdates() override;
  absl::optional<PendingNetworkConfigurationUpdate> GetPendingUpdate(
      const std::string& change_guid,
      const NetworkIdentifier& id) override;

 private:
  PrefService* pref_service_;
  base::Value dict_;
};

}  // namespace sync_wifi

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_SYNC_WIFI_PENDING_NETWORK_CONFIGURATION_TRACKER_IMPL_H_
