// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_CELLULAR_POLICY_HANDLER_H_
#define CHROMEOS_NETWORK_CELLULAR_POLICY_HANDLER_H_

#include "base/component_export.h"
#include "base/containers/queue.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "net/base/backoff_entry.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace dbus {
class ObjectPath;
}  // namespace dbus

namespace chromeos {

class CellularESimInstaller;
class NetworkProfileHandler;
class ManagedNetworkConfigurationHandler;
enum class HermesResponseStatus;

// Handles provisioning eSIM profiles via policy.
//
// When installing policy eSIM profiles, the activation code is constructed from
// the SM-DP+ address in the policy configuration. Install requests are queued
// and installation is performed one by one. Install attempts are retried for
// fixed number of tries.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) CellularPolicyHandler {
 public:
  CellularPolicyHandler();
  CellularPolicyHandler(const CellularPolicyHandler&) = delete;
  CellularPolicyHandler& operator=(const CellularPolicyHandler&) = delete;
  ~CellularPolicyHandler();

  void Init(CellularESimInstaller* cellular_esim_installer,
            NetworkProfileHandler* network_profile_handler,
            ManagedNetworkConfigurationHandler*
                managed_network_configuration_handler);

  // Installs an eSIM profile and connects to its network from policy with
  // given |smdp_address|. The Shill service configuration will also be updated
  // to the policy guid and the new ICCID after installation completes. If
  // another eSIM profile is already under installation process, the current
  // request will wait until the previous one is completed. Each installation
  // will be retried for a fixed number of tries.
  void InstallESim(const std::string& smdp_address,
                   const base::DictionaryValue& onc_config);

 private:
  friend class CellularPolicyHandlerTest;

  // Represents policy eSIM install request parameters. Requests are queued and
  // processed one at a time. |smdp_address| represents the smdp address that
  // will be used to install the eSIM profile as activation code and
  // |onc_config| is the ONC configuration of the cellular policy.
  struct InstallPolicyESimRequest {
    InstallPolicyESimRequest(const std::string& smdp_address,
                             const base::DictionaryValue& onc_config);
    InstallPolicyESimRequest(const InstallPolicyESimRequest&) = delete;
    InstallPolicyESimRequest& operator=(const InstallPolicyESimRequest&) =
        delete;
    ~InstallPolicyESimRequest();

    const std::string smdp_address;
    std::unique_ptr<base::DictionaryValue> onc_config;
  };

  void ProcessRequests();
  void AttemptInstallESim();
  const std::string& GetCurrentSmdpAddress() const;
  std::string GetCurrentPolicyGuid() const;
  void OnESimProfileInstallAttemptComplete(
      HermesResponseStatus hermes_status,
      absl::optional<dbus::ObjectPath> profile_path,
      absl::optional<std::string> service_path);
  void PopRequestAndProcessNext();
  void InvalidateCurrentRequest();

  CellularESimInstaller* cellular_esim_installer_ = nullptr;
  NetworkProfileHandler* network_profile_handler_ = nullptr;
  ManagedNetworkConfigurationHandler* managed_network_configuration_handler_ =
      nullptr;

  bool is_installing_ = false;
  base::circular_deque<std::unique_ptr<InstallPolicyESimRequest>>
      remaining_install_requests_;
  base::OneShotTimer retry_timer_;

  // Provides us the backoff timers for AttemptInstallESim().
  net::BackoffEntry retry_backoff_;

  base::WeakPtrFactory<CellularPolicyHandler> weak_ptr_factory_{this};
};

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_CELLULAR_POLICY_HANDLER_H_