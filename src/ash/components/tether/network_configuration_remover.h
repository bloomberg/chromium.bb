// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_TETHER_NETWORK_CONFIGURATION_REMOVER_H_
#define ASH_COMPONENTS_TETHER_NETWORK_CONFIGURATION_REMOVER_H_

#include <string>

// TODO(https://crbug.com/1164001): move to forward declaration
#include "chromeos/network/managed_network_configuration_handler.h"

namespace ash {

namespace tether {

// Handles the removal of the configuration of a Wi-Fi network.
class NetworkConfigurationRemover {
 public:
  NetworkConfigurationRemover(ManagedNetworkConfigurationHandler*
                                  managed_network_configuration_handler);

  NetworkConfigurationRemover(const NetworkConfigurationRemover&) = delete;
  NetworkConfigurationRemover& operator=(const NetworkConfigurationRemover&) =
      delete;

  virtual ~NetworkConfigurationRemover();

  // Remove the network configuration of the Wi-Fi hotspot referenced by
  // |wifi_network_path|.
  virtual void RemoveNetworkConfigurationByPath(
      const std::string& wifi_network_path);

 private:
  friend class NetworkConfigurationRemoverTest;

  ManagedNetworkConfigurationHandler* managed_network_configuration_handler_;
};

}  // namespace tether

}  // namespace ash

#endif  // ASH_COMPONENTS_TETHER_NETWORK_CONFIGURATION_REMOVER_H_
