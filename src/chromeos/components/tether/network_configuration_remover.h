// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TETHER_NETWORK_CONFIGURATION_REMOVER_H_
#define CHROMEOS_COMPONENTS_TETHER_NETWORK_CONFIGURATION_REMOVER_H_

#include <string>

#include "base/macros.h"

namespace chromeos {

class ManagedNetworkConfigurationHandler;

namespace tether {

// Handles the removal of the configuration of a Wi-Fi network.
class NetworkConfigurationRemover {
 public:
  NetworkConfigurationRemover(ManagedNetworkConfigurationHandler*
                                  managed_network_configuration_handler);
  virtual ~NetworkConfigurationRemover();

  // Remove the network configuration of the Wi-Fi hotspot referenced by
  // |wifi_network_path|.
  virtual void RemoveNetworkConfigurationByPath(
      const std::string& wifi_network_path);

 private:
  friend class NetworkConfigurationRemoverTest;

  ManagedNetworkConfigurationHandler* managed_network_configuration_handler_;

  DISALLOW_COPY_AND_ASSIGN(NetworkConfigurationRemover);
};

}  // namespace tether

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TETHER_NETWORK_CONFIGURATION_REMOVER_H_
