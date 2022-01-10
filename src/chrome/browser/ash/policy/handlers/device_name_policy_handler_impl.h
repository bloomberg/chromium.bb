// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_HANDLERS_DEVICE_NAME_POLICY_HANDLER_IMPL_H_
#define CHROME_BROWSER_ASH_POLICY_HANDLERS_DEVICE_NAME_POLICY_HANDLER_IMPL_H_

#include <memory>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/policy/handlers/device_name_policy_handler.h"
#include "chrome/browser/ash/settings/cros_settings.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_state_handler_observer.h"
#include "chromeos/system/statistics_provider.h"

namespace policy {

// This class observes the device setting |DeviceHostname|, and calls
// NetworkStateHandler::SetHostname() appropriately based on the value of that
// setting.
class DeviceNamePolicyHandlerImpl
    : public DeviceNamePolicyHandler,
      public chromeos::NetworkStateHandlerObserver {
 public:
  explicit DeviceNamePolicyHandlerImpl(ash::CrosSettings* cros_settings);

  DeviceNamePolicyHandlerImpl(const DeviceNamePolicyHandlerImpl&) = delete;
  DeviceNamePolicyHandlerImpl& operator=(const DeviceNamePolicyHandlerImpl&) =
      delete;

  ~DeviceNamePolicyHandlerImpl() override;

  // DeviceNamePolicyHandler:
  DeviceNamePolicy GetDeviceNamePolicy() const override;
  absl::optional<std::string> GetHostnameChosenByAdministrator() const override;

 private:
  friend class DeviceNamePolicyHandlerImplTest;

  DeviceNamePolicyHandlerImpl(
      ash::CrosSettings* cros_settings,
      chromeos::system::StatisticsProvider* statistics_provider,
      chromeos::NetworkStateHandler* handler);

  // NetworkStateHandlerObserver overrides
  void DefaultNetworkChanged(const chromeos::NetworkState* network) override;

  void OnDeviceHostnamePropertyChanged();

  void OnDeviceHostnamePropertyChangedAndMachineStatisticsLoaded();

  // Returns the device name policy to be used. If kPolicyHostnameChosenByAdmin
  // is returned, |hostname_template_out| is set to the template chosen by the
  // administrator; otherwise, |hostname_template_out| is left unchanged.
  DeviceNamePolicyHandler::DeviceNamePolicy ComputePolicy(
      std::string* hostname_template_out);

  // Generates the hostname according to the template chosen by the
  // administrator.
  std::string GenerateHostname(const std::string& hostname_template) const;

  // Sets new device name and policy if different from the current device name
  // and/or policy.
  void SetDeviceNamePolicy(DeviceNamePolicy policy, std::string& new_hostname);

  ash::CrosSettings* cros_settings_;
  chromeos::system::StatisticsProvider* statistics_provider_;
  chromeos::NetworkStateHandler* handler_;
  DeviceNamePolicy device_name_policy_;

  base::CallbackListSubscription template_policy_subscription_;
  base::CallbackListSubscription configurable_policy_subscription_;
  std::string hostname_;
  base::WeakPtrFactory<DeviceNamePolicyHandlerImpl> weak_factory_{this};
};

std::ostream& operator<<(
    std::ostream& stream,
    const DeviceNamePolicyHandlerImpl::DeviceNamePolicy& state);

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_HANDLERS_DEVICE_NAME_POLICY_HANDLER_IMPL_H_
