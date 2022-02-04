// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_MOCK_MANAGED_NETWORK_CONFIGURATION_HANDLER_H_
#define CHROMEOS_NETWORK_MOCK_MANAGED_NETWORK_CONFIGURATION_HANDLER_H_

#include <string>

#include "base/component_export.h"
#include "base/values.h"
#include "chromeos/network/managed_network_configuration_handler.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class COMPONENT_EXPORT(CHROMEOS_NETWORK) MockManagedNetworkConfigurationHandler
    : public ManagedNetworkConfigurationHandler {
 public:
  MockManagedNetworkConfigurationHandler();

  MockManagedNetworkConfigurationHandler(
      const MockManagedNetworkConfigurationHandler&) = delete;
  MockManagedNetworkConfigurationHandler& operator=(
      const MockManagedNetworkConfigurationHandler&) = delete;

  virtual ~MockManagedNetworkConfigurationHandler();

  // ManagedNetworkConfigurationHandler overrides
  MOCK_METHOD1(AddObserver, void(NetworkPolicyObserver* observer));
  MOCK_METHOD1(RemoveObserver, void(NetworkPolicyObserver* observer));
  MOCK_METHOD3(GetProperties,
               void(const std::string& userhash,
                    const std::string& service_path,
                    network_handler::PropertiesCallback callback));
  MOCK_METHOD3(GetManagedProperties,
               void(const std::string& userhash,
                    const std::string& service_path,
                    network_handler::PropertiesCallback callback));
  MOCK_METHOD4(SetProperties,
               void(const std::string& service_path,
                    const base::DictionaryValue& user_settings,
                    base::OnceClosure callback,
                    network_handler::ErrorCallback error_callback));
  MOCK_CONST_METHOD4(CreateConfiguration,
                     void(const std::string& userhash,
                          const base::DictionaryValue& properties,
                          network_handler::ServiceResultCallback callback,
                          network_handler::ErrorCallback error_callback));
  MOCK_CONST_METHOD2(ConfigurePolicyNetwork,
                     void(const base::Value& shill_properties,
                          base::OnceClosure callback));
  MOCK_CONST_METHOD3(RemoveConfiguration,
                     void(const std::string& service_path,
                          base::OnceClosure callback,
                          network_handler::ErrorCallback error_callback));
  MOCK_CONST_METHOD3(RemoveConfigurationFromCurrentProfile,
                     void(const std::string& service_path,
                          base::OnceClosure callback,
                          network_handler::ErrorCallback error_callback));
  MOCK_METHOD4(SetPolicy,
               void(::onc::ONCSource onc_source,
                    const std::string& userhash,
                    const base::Value& network_configs_onc,
                    const base::Value& global_network_config));
  MOCK_CONST_METHOD0(IsAnyPolicyApplicationRunning, bool());
  MOCK_CONST_METHOD3(
      FindPolicyByGUID,
      const base::DictionaryValue*(const std::string userhash,
                                   const std::string& guid,
                                   ::onc::ONCSource* onc_source));
  MOCK_CONST_METHOD1(GetNetworkConfigsFromPolicy,
                     const GuidToPolicyMap*(const std::string& userhash));
  MOCK_CONST_METHOD1(GetGlobalConfigFromPolicy,
                     const base::DictionaryValue*(const std::string& userhash));
  MOCK_CONST_METHOD3(
      FindPolicyByGuidAndProfile,
      const base::DictionaryValue*(const std::string& guid,
                                   const std::string& profile_path,
                                   ::onc::ONCSource* onc_source));
  MOCK_CONST_METHOD2(IsNetworkConfiguredByPolicy,
                     bool(const std::string& guid,
                          const std::string& profile_path));
  MOCK_CONST_METHOD2(CanRemoveNetworkConfig,
                     bool(const std::string& guid,
                          const std::string& profile_path));
  MOCK_CONST_METHOD1(NotifyPolicyAppliedToNetwork,
                     void(const std::string& service_path));
  MOCK_METHOD1(OnCellularPoliciesApplied, void(const NetworkProfile& profile));
  MOCK_CONST_METHOD0(AllowOnlyPolicyCellularNetworks, bool());
  MOCK_CONST_METHOD0(AllowOnlyPolicyWiFiToConnect, bool());
  MOCK_CONST_METHOD0(AllowOnlyPolicyWiFiToConnectIfAvailable, bool());
  MOCK_CONST_METHOD0(AllowOnlyPolicyNetworksToAutoconnect, bool());
  MOCK_CONST_METHOD0(GetBlockedHexSSIDs, std::vector<std::string>());
};

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove when this file is moved to ash.
namespace ash {
using ::chromeos::MockManagedNetworkConfigurationHandler;
}  // namespace ash

#endif  // CHROMEOS_NETWORK_MOCK_MANAGED_NETWORK_CONFIGURATION_HANDLER_H_
