// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/device_wilco_dtc_configuration_handler.h"

#include "chrome/browser/chromeos/wilco_dtc_supportd/wilco_dtc_supportd_manager.h"
#include "components/policy/policy_constants.h"

namespace policy {

namespace {

chromeos::WilcoDtcSupportdManager* GetWilcoDtcSupportdManager() {
  chromeos::WilcoDtcSupportdManager* const wilco_dtc_supportd_manager =
      chromeos::WilcoDtcSupportdManager::Get();
  return wilco_dtc_supportd_manager;
}

}  // namespace

DeviceWilcoDtcConfigurationHandler::DeviceWilcoDtcConfigurationHandler(
    PolicyService* policy_service)
    : device_wilco_dtc_configuration_observer_(
          std::make_unique<DeviceCloudExternalDataPolicyObserver>(
              policy_service,
              key::kDeviceWilcoDtcConfiguration,
              this)) {}

DeviceWilcoDtcConfigurationHandler::~DeviceWilcoDtcConfigurationHandler() {}

void DeviceWilcoDtcConfigurationHandler::OnDeviceExternalDataCleared(
    const std::string& policy) {
  auto* wilco_manager = GetWilcoDtcSupportdManager();
  if (wilco_manager)
    wilco_manager->SetConfigurationData(nullptr);
}

void DeviceWilcoDtcConfigurationHandler::OnDeviceExternalDataFetched(
    const std::string& policy,
    std::unique_ptr<std::string> data,
    const base::FilePath& file_path) {
  auto* wilco_manager = GetWilcoDtcSupportdManager();
  if (wilco_manager)
    wilco_manager->SetConfigurationData(std::move(data));
}

void DeviceWilcoDtcConfigurationHandler::Shutdown() {
  device_wilco_dtc_configuration_observer_.reset();
}

}  // namespace policy
