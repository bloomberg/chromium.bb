// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/handlers/device_name_policy_handler_impl.h"

#include "ash/components/settings/cros_settings_names.h"
#include "ash/components/settings/cros_settings_provider.h"
#include "ash/constants/ash_features.h"
#include "base/bind.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/handlers/device_name_policy_handler_name_generator.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chromeos/network/device_state.h"
#include "chromeos/tpm/install_attributes.h"

namespace policy {
namespace {

// By default, device name policy should be kPolicyHostnameNotConfigurable for
// managed devices and kNoPolicy for unmanaged devices.
DeviceNamePolicyHandler::DeviceNamePolicy ComputeInitialPolicy() {
  if (chromeos::InstallAttributes::Get()->IsEnterpriseManaged()) {
    // We assume that the device name is not configurable unless/until we know
    // about any policies that are set.
    return DeviceNamePolicyHandler::DeviceNamePolicy::
        kPolicyHostnameNotConfigurable;
  }

  return DeviceNamePolicyHandler::DeviceNamePolicy::kNoPolicy;
}

}  // namespace

DeviceNamePolicyHandlerImpl::DeviceNamePolicyHandlerImpl(
    ash::CrosSettings* cros_settings)
    : DeviceNamePolicyHandlerImpl(
          cros_settings,
          chromeos::system::StatisticsProvider::GetInstance(),
          chromeos::NetworkHandler::Get()->network_state_handler()) {}

DeviceNamePolicyHandlerImpl::DeviceNamePolicyHandlerImpl(
    ash::CrosSettings* cros_settings,
    chromeos::system::StatisticsProvider* statistics_provider,
    chromeos::NetworkStateHandler* handler)
    : cros_settings_(cros_settings),
      statistics_provider_(statistics_provider),
      handler_(handler),
      device_name_policy_(ComputeInitialPolicy()) {
  template_policy_subscription_ = cros_settings_->AddSettingsObserver(
      ash::kDeviceHostnameTemplate,
      base::BindRepeating(
          &DeviceNamePolicyHandlerImpl::OnDeviceHostnamePropertyChanged,
          weak_factory_.GetWeakPtr()));
  configurable_policy_subscription_ = cros_settings_->AddSettingsObserver(
      ash::kDeviceHostnameUserConfigurable,
      base::BindRepeating(
          &DeviceNamePolicyHandlerImpl::OnDeviceHostnamePropertyChanged,
          weak_factory_.GetWeakPtr()));
  chromeos::NetworkHandler::Get()->network_state_handler()->AddObserver(
      this, FROM_HERE);

  // Fire it once so we're sure we get an invocation on startup.
  OnDeviceHostnamePropertyChanged();
}

DeviceNamePolicyHandlerImpl::~DeviceNamePolicyHandlerImpl() {
  if (chromeos::NetworkHandler::IsInitialized()) {
    chromeos::NetworkHandler::Get()->network_state_handler()->RemoveObserver(
        this, FROM_HERE);
  }
}

DeviceNamePolicyHandler::DeviceNamePolicy
DeviceNamePolicyHandlerImpl::GetDeviceNamePolicy() const {
  return device_name_policy_;
}

absl::optional<std::string>
DeviceNamePolicyHandlerImpl::GetHostnameChosenByAdministrator() const {
  if (GetDeviceNamePolicy() == DeviceNamePolicy::kPolicyHostnameChosenByAdmin) {
    return hostname_;
  }
  return absl::nullopt;
}

void DeviceNamePolicyHandlerImpl::DefaultNetworkChanged(
    const chromeos::NetworkState* network) {
  OnDeviceHostnamePropertyChanged();
}

void DeviceNamePolicyHandlerImpl::OnDeviceHostnamePropertyChanged() {
  ash::CrosSettingsProvider::TrustedStatus status =
      cros_settings_->PrepareTrustedValues(base::BindOnce(
          &DeviceNamePolicyHandlerImpl::OnDeviceHostnamePropertyChanged,
          weak_factory_.GetWeakPtr()));
  if (status != ash::CrosSettingsProvider::TRUSTED)
    return;

  // Continue when machine statistics are loaded, to avoid blocking.
  statistics_provider_->ScheduleOnMachineStatisticsLoaded(base::BindOnce(
      &DeviceNamePolicyHandlerImpl::
          OnDeviceHostnamePropertyChangedAndMachineStatisticsLoaded,
      weak_factory_.GetWeakPtr()));
}

void DeviceNamePolicyHandlerImpl::
    OnDeviceHostnamePropertyChangedAndMachineStatisticsLoaded() {
  std::string hostname_template;
  DeviceNamePolicy policy = ComputePolicy(&hostname_template);

  std::string new_hostname;
  if (policy == DeviceNamePolicy::kPolicyHostnameChosenByAdmin) {
    new_hostname = GenerateHostname(hostname_template);
  }

  SetDeviceNamePolicy(policy, new_hostname);
}

DeviceNamePolicyHandler::DeviceNamePolicy
DeviceNamePolicyHandlerImpl::ComputePolicy(std::string* hostname_template_out) {
  if (cros_settings_->GetString(ash::kDeviceHostnameTemplate,
                                hostname_template_out)) {
    // Do not set an empty hostname (which would overwrite any custom hostname
    // set) if DeviceHostnameTemplate is not specified by policy.
    // No policy is set for administrator to choose hostname.
    return DeviceNamePolicy::kPolicyHostnameChosenByAdmin;
  }

  bool hostname_user_configurable;
  if (ash::features::IsHostnameSettingEnabled() &&
      cros_settings_->GetBoolean(ash::kDeviceHostnameUserConfigurable,
                                 &hostname_user_configurable)) {
    return hostname_user_configurable
               ? DeviceNamePolicy::kPolicyHostnameConfigurableByManagedUser
               : DeviceNamePolicy::kPolicyHostnameNotConfigurable;
  }

  // If no policies are set, device name policy should be
  // kPolicyHostnameNotConfigurable for managed devices and kNoPolicy for
  // unmanaged devices.
  if (chromeos::InstallAttributes::Get()->IsEnterpriseManaged())
    return DeviceNamePolicy::kPolicyHostnameNotConfigurable;

  return DeviceNamePolicy::kNoPolicy;
}

std::string DeviceNamePolicyHandlerImpl::GenerateHostname(
    const std::string& hostname_template) const {
  const std::string serial = chromeos::system::StatisticsProvider::GetInstance()
                                 ->GetEnterpriseMachineID();

  const std::string asset_id = g_browser_process->platform_part()
                                   ->browser_policy_connector_ash()
                                   ->GetDeviceAssetID();

  const std::string machine_name = g_browser_process->platform_part()
                                       ->browser_policy_connector_ash()
                                       ->GetMachineName();

  const std::string location = g_browser_process->platform_part()
                                   ->browser_policy_connector_ash()
                                   ->GetDeviceAnnotatedLocation();
  std::string mac = "MAC_unknown";
  const chromeos::NetworkState* network = handler_->DefaultNetwork();
  if (network) {
    const chromeos::DeviceState* device =
        handler_->GetDeviceState(network->device_path());
    if (device) {
      mac = device->mac_address();
      base::ReplaceSubstringsAfterOffset(&mac, 0, ":", "");
    }
  }

  return policy::FormatHostname(hostname_template, asset_id, serial, mac,
                                machine_name, location);
}

void DeviceNamePolicyHandlerImpl::SetDeviceNamePolicy(
    DeviceNamePolicy policy,
    std::string& new_hostname) {
  if (device_name_policy_ == policy && hostname_ == new_hostname)
    return;

  // If the hostname has changed, set it using NetworkStateHandler. Note that
  // this process is skipped when the hostname settings flag is enabled since
  // this is handled elsewhere. See https://crbug.com/126802.
  if (!ash::features::IsHostnameSettingEnabled() &&
      policy == DeviceNamePolicy::kPolicyHostnameChosenByAdmin &&
      hostname_ != new_hostname) {
    handler_->SetHostname(new_hostname);
  }

  device_name_policy_ = policy;
  hostname_ = new_hostname;
  NotifyHostnamePolicyChanged();
}

std::ostream& operator<<(
    std::ostream& stream,
    const DeviceNamePolicyHandlerImpl::DeviceNamePolicy& state) {
  switch (state) {
    case DeviceNamePolicyHandlerImpl::DeviceNamePolicy::kNoPolicy:
      stream << "[No policy]";
      break;
    case DeviceNamePolicyHandlerImpl::DeviceNamePolicy::
        kPolicyHostnameChosenByAdmin:
      stream << "[Admin chooses hostname template]";
      break;
    case DeviceNamePolicyHandlerImpl::DeviceNamePolicy::
        kPolicyHostnameConfigurableByManagedUser:
      stream << "[Managed user can choose hostname]";
      break;
    case DeviceNamePolicyHandlerImpl::DeviceNamePolicy::
        kPolicyHostnameNotConfigurable:
      stream << "[Managed user cannot choose hostname]";
      break;
  }
  return stream;
}

}  // namespace policy
