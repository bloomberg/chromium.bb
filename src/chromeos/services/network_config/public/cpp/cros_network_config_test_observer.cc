// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/network_config/public/cpp/cros_network_config_test_observer.h"

#include "chromeos/services/network_config/cros_network_config.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_test_helper.h"
#include "chromeos/services/network_config/public/mojom/constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

namespace chromeos {
namespace network_config {

CrosNetworkConfigTestObserver::CrosNetworkConfigTestObserver() = default;
CrosNetworkConfigTestObserver::~CrosNetworkConfigTestObserver() = default;

mojom::CrosNetworkConfigObserverPtr
CrosNetworkConfigTestObserver::GenerateInterfacePtr() {
  mojom::CrosNetworkConfigObserverPtr observer_ptr;
  binding().Bind(mojo::MakeRequest(&observer_ptr));
  return observer_ptr;
}

void CrosNetworkConfigTestObserver::OnActiveNetworksChanged(
    std::vector<mojom::NetworkStatePropertiesPtr> networks) {
  active_networks_changed_++;
}
void CrosNetworkConfigTestObserver::OnNetworkStateListChanged() {
  network_state_list_changed_++;
}
void CrosNetworkConfigTestObserver::OnDeviceStateListChanged() {
  device_state_list_changed_++;
}

void CrosNetworkConfigTestObserver::ResetNetworkChanges() {
  active_networks_changed_ = 0;
  network_state_list_changed_ = 0;
  device_state_list_changed_ = 0;
}

void CrosNetworkConfigTestObserver::FlushForTesting() {
  binding_.FlushForTesting();
}

}  // namespace network_config
}  // namespace chromeos
