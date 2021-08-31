// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/network_config/public/cpp/cros_network_config_test_helper.h"

#include "chromeos/network/network_device_handler.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/services/network_config/cros_network_config.h"
#include "chromeos/services/network_config/in_process_instance.h"

namespace chromeos {
namespace network_config {

CrosNetworkConfigTestHelper::CrosNetworkConfigTestHelper()
    : CrosNetworkConfigTestHelper(true) {}

CrosNetworkConfigTestHelper::CrosNetworkConfigTestHelper(bool initialize) {
  if (initialize)
    Initialize(/*network_configuration_handler=*/nullptr);
}

CrosNetworkConfigTestHelper::~CrosNetworkConfigTestHelper() {
  OverrideInProcessInstanceForTesting(nullptr);
}

network_config::mojom::NetworkStatePropertiesPtr
CrosNetworkConfigTestHelper::CreateStandaloneNetworkProperties(
    const std::string& id,
    mojom::NetworkType type,
    mojom::ConnectionStateType connection_state,
    int signal_strength) {
  using mojom::NetworkType;
  using mojom::NetworkTypeStateProperties;
  auto network = mojom::NetworkStateProperties::New();
  network->guid = id;
  network->name = id;
  network->type = type;
  network->connection_state = connection_state;
  switch (type) {
    case NetworkType::kAll:
    case NetworkType::kMobile:
    case NetworkType::kWireless:
      NOTREACHED();
      break;
    case NetworkType::kCellular: {
      auto cellular = mojom::CellularStateProperties::New();
      cellular->signal_strength = signal_strength;
      network->type_state =
          NetworkTypeStateProperties::NewCellular(std::move(cellular));
      break;
    }
    case NetworkType::kEthernet:
      break;
    case NetworkType::kTether: {
      auto tether = mojom::TetherStateProperties::New();
      tether->signal_strength = signal_strength;
      network->type_state =
          NetworkTypeStateProperties::NewTether(std::move(tether));
      break;
    }
    case NetworkType::kVPN:
      break;
    case NetworkType::kWiFi: {
      auto wifi = mojom::WiFiStateProperties::New();
      wifi->signal_strength = signal_strength;
      network->type_state =
          NetworkTypeStateProperties::NewWifi(std::move(wifi));
      break;
    }
  }
  return network;
}

void CrosNetworkConfigTestHelper::Initialize(
    ManagedNetworkConfigurationHandler* network_configuration_handler) {
  if (NetworkHandler::IsInitialized()) {
    cros_network_config_impl_ = std::make_unique<CrosNetworkConfig>();
  } else {
    cros_network_config_impl_ = std::make_unique<CrosNetworkConfig>(
        network_state_helper_.network_state_handler(),
        network_state_helper_.network_device_handler(),
        /*cellular_inhibitor=*/nullptr,
        /*cellular_esim_profile_handler=*/nullptr,
        network_configuration_handler,
        /*network_connection_handler=*/nullptr,
        /*network_certificate_handler=*/nullptr,
        /*network_profile_handler=*/nullptr);
  }
  OverrideInProcessInstanceForTesting(cros_network_config_impl_.get());
}

}  // namespace network_config
}  // namespace chromeos
