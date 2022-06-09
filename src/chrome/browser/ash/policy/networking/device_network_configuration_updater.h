// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_NETWORKING_DEVICE_NETWORK_CONFIGURATION_UPDATER_H_
#define CHROME_BROWSER_ASH_POLICY_NETWORKING_DEVICE_NETWORK_CONFIGURATION_UPDATER_H_

#include <memory>
#include <string>

#include "base/callback_list.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/policy/networking/network_configuration_updater.h"
#include "components/onc/onc_constants.h"
#include "net/cert/x509_certificate.h"

namespace ash {
class CrosSettings;
}  // namespace ash

namespace base {
class DictionaryValue;
class ListValue;
}  // namespace base

namespace chromeos {
class ManagedNetworkConfigurationHandler;
class NetworkDeviceHandler;
}  // namespace chromeos

namespace policy {

class PolicyService;

// Implements addtional special handling of ONC device policies, which requires
// listening for notifications from CrosSettings.
class DeviceNetworkConfigurationUpdater : public NetworkConfigurationUpdater {
 public:
  DeviceNetworkConfigurationUpdater(const DeviceNetworkConfigurationUpdater&) =
      delete;
  DeviceNetworkConfigurationUpdater& operator=(
      const DeviceNetworkConfigurationUpdater&) = delete;

  ~DeviceNetworkConfigurationUpdater() override;

  // Fetches the device's administrator-annotated asset ID.
  using DeviceAssetIDFetcher = base::RepeatingCallback<std::string()>;

  // Creates an updater that applies the ONC device policy from |policy_service|
  // once the policy service is completely initialized and on each policy
  // change. The argument objects passed as pointers must outlive the returned
  // updater. |device_assed_id_fetcher| should return the
  // administrator-annotated asset ID of the device and is used for variable
  // replacement. If a null callback is passed, the asset ID from device policy
  // will be used.
  static std::unique_ptr<DeviceNetworkConfigurationUpdater>
  CreateForDevicePolicy(
      PolicyService* policy_service,
      chromeos::ManagedNetworkConfigurationHandler* network_config_handler,
      chromeos::NetworkDeviceHandler* network_device_handler,
      ash::CrosSettings* cros_settings,
      const DeviceAssetIDFetcher& device_asset_id_fetcher);

 private:
  DeviceNetworkConfigurationUpdater(
      PolicyService* policy_service,
      chromeos::ManagedNetworkConfigurationHandler* network_config_handler,
      chromeos::NetworkDeviceHandler* network_device_handler,
      ash::CrosSettings* cros_settings,
      const DeviceAssetIDFetcher& device_asset_id_fetcher);

  // NetworkConfigurationUpdater:
  void Init() override;
  void ImportClientCertificates() override;
  void ApplyNetworkPolicy(
      base::ListValue* network_configs_onc,
      base::DictionaryValue* global_network_config) override;
  void OnDataRoamingSettingChanged();

  chromeos::NetworkDeviceHandler* network_device_handler_;
  ash::CrosSettings* cros_settings_;
  base::CallbackListSubscription data_roaming_setting_subscription_;

  // Returns the device's administrator-set asset id.
  DeviceAssetIDFetcher device_asset_id_fetcher_;

  base::WeakPtrFactory<DeviceNetworkConfigurationUpdater> weak_factory_{this};
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_NETWORKING_DEVICE_NETWORK_CONFIGURATION_UPDATER_H_
