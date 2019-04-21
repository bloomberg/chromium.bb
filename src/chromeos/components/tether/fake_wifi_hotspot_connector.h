// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TETHER_FAKE_WIFI_HOTSPOT_CONNECTOR_H_
#define CHROMEOS_COMPONENTS_TETHER_FAKE_WIFI_HOTSPOT_CONNECTOR_H_

#include <string>

#include "base/macros.h"
#include "chromeos/components/tether/wifi_hotspot_connector.h"

namespace chromeos {

namespace tether {

// Test double for WifiHotspotConnector.
class FakeWifiHotspotConnector : public WifiHotspotConnector {
 public:
  FakeWifiHotspotConnector(NetworkStateHandler* network_state_handler);
  ~FakeWifiHotspotConnector() override;

  // Pass an empty string for |wifi_guid| to signify a failed connection.
  void CallMostRecentCallback(const std::string& wifi_guid);

  std::string most_recent_ssid() { return most_recent_ssid_; }

  std::string most_recent_password() { return most_recent_password_; }

  std::string most_recent_tether_network_guid() {
    return most_recent_tether_network_guid_;
  }

  // WifiHotspotConnector:
  void ConnectToWifiHotspot(
      const std::string& ssid,
      const std::string& password,
      const std::string& tether_network_guid,
      const WifiHotspotConnector::WifiConnectionCallback& callback) override;

 private:
  std::string most_recent_ssid_;
  std::string most_recent_password_;
  std::string most_recent_tether_network_guid_;
  WifiHotspotConnector::WifiConnectionCallback most_recent_callback_;

  DISALLOW_COPY_AND_ASSIGN(FakeWifiHotspotConnector);
};

}  // namespace tether

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TETHER_FAKE_WIFI_HOTSPOT_CONNECTOR_H_
