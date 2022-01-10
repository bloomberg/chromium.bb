// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_NETWORK_CONFIG_PUBLIC_CPP_CROS_NETWORK_CONFIG_TEST_HELPER_H_
#define CHROMEOS_SERVICES_NETWORK_CONFIG_PUBLIC_CPP_CROS_NETWORK_CONFIG_TEST_HELPER_H_

#include <memory>

#include "chromeos/network/managed_network_configuration_handler.h"
#include "chromeos/network/network_state_test_helper.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom-forward.h"
#include "chromeos/services/network_config/public/mojom/network_types.mojom-forward.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {

class NetworkDeviceHandler;

namespace network_config {

class CrosNetworkConfig;

// Helper for tests which need a CrosNetworkConfig service interface.
class CrosNetworkConfigTestHelper {
 public:
  // Default constructor for unit tests.
  CrosNetworkConfigTestHelper();

  // Constructor for when a ManagedNetworkConfigurationHandler must be
  // separately initialized via Initialize(ManagedNetworkConfigurationHandler*).
  explicit CrosNetworkConfigTestHelper(bool initialize);

  CrosNetworkConfigTestHelper(const CrosNetworkConfigTestHelper&) = delete;
  CrosNetworkConfigTestHelper& operator=(const CrosNetworkConfigTestHelper&) =
      delete;

  ~CrosNetworkConfigTestHelper();

  mojom::NetworkStatePropertiesPtr CreateStandaloneNetworkProperties(
      const std::string& id,
      mojom::NetworkType type,
      mojom::ConnectionStateType connection_state,
      int signal_strength);

  NetworkStateTestHelper& network_state_helper() {
    return network_state_helper_;
  }

  NetworkDeviceHandler* network_device_handler() {
    return network_state_helper_.network_device_handler();
  }

  void Initialize(
      ManagedNetworkConfigurationHandler* network_configuration_handler);

 protected:
  NetworkStateTestHelper network_state_helper_{
      /*use_default_devices_and_services=*/false};
  std::unique_ptr<CrosNetworkConfig> cros_network_config_impl_;
};

}  // namespace network_config
}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove when moved to ash.
namespace ash {
namespace network_config {
using ::chromeos::network_config::CrosNetworkConfigTestHelper;
}  // namespace network_config
}  // namespace ash

#endif  // CHROMEOS_SERVICES_NETWORK_CONFIG_PUBLIC_CPP_CROS_NETWORK_CONFIG_TEST_HELPER_H_
