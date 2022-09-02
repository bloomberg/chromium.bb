// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_TETHER_FAKE_ACTIVE_HOST_H_
#define ASH_COMPONENTS_TETHER_FAKE_ACTIVE_HOST_H_

#include <string>

#include "ash/components/tether/active_host.h"
#include "base/callback.h"
#include "base/memory/weak_ptr.h"

namespace ash {

namespace tether {

// Test double for ActiveHost.
class FakeActiveHost : public ActiveHost {
 public:
  FakeActiveHost();

  FakeActiveHost(const FakeActiveHost&) = delete;
  FakeActiveHost& operator=(const FakeActiveHost&) = delete;

  ~FakeActiveHost() override;

  // ActiveHost:
  void SetActiveHostDisconnected() override;
  void SetActiveHostConnecting(const std::string& active_host_device_id,
                               const std::string& tether_network_guid) override;
  void SetActiveHostConnected(const std::string& active_host_device_id,
                              const std::string& tether_network_guid,
                              const std::string& wifi_network_guid) override;
  void GetActiveHost(
      ActiveHost::ActiveHostCallback active_host_callback) override;
  ActiveHostStatus GetActiveHostStatus() const override;
  std::string GetActiveHostDeviceId() const override;
  std::string GetTetherNetworkGuid() const override;
  std::string GetWifiNetworkGuid() const override;

 private:
  void SetActiveHost(ActiveHostStatus active_host_status,
                     const std::string& active_host_device_id,
                     const std::string& tether_network_guid,
                     const std::string& wifi_network_guid);

  ActiveHost::ActiveHostStatus active_host_status_;
  std::string active_host_device_id_;
  std::string tether_network_guid_;
  std::string wifi_network_guid_;
};

}  // namespace tether

}  // namespace ash

#endif  // ASH_COMPONENTS_TETHER_FAKE_ACTIVE_HOST_H_
