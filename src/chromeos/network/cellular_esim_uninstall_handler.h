// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_CELLULAR_ESIM_UNINSTALL_HANDLER_H_
#define CHROMEOS_NETWORK_CELLULAR_ESIM_UNINSTALL_HANDLER_H_

#include <memory>

#include "base/component_export.h"
#include "base/containers/circular_deque.h"
#include "base/containers/queue.h"
#include "base/gtest_prod_util.h"
#include "chromeos/dbus/hermes/hermes_response_status.h"
#include "chromeos/network/cellular_esim_profile_handler.h"
#include "chromeos/network/cellular_inhibitor.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_state_handler_observer.h"
#include "dbus/object_path.h"

namespace chromeos {

class CellularESimProfileHandler;
class CellularInhibitor;
class NetworkState;
class NetworkConfigurationHandler;
class NetworkConnectionHandler;

// Handles Uninstallation of an eSIM profile and it's corresponding network.
//
// Uninstalling and eSIM network involves interacting with both Shill and Hermes
// in the following sequence:
// 1. Disconnect Network with Shill
// 2. Inhibit Cellular Scans
// 3. Request Installed eSIM profiles from Hermes
// 4. Disable eSIM profile through Hermes
// 5. Uninstall eSIM profile in Hermes
// 6. Remove Shill configuration for Network
// 7. Uninhibit Cellular Scans
//
// Uninstallation requests are queued and run in order.
//
// Note: This class doesn't check and remove stale Shill eSIM services anymore
// because it might remove usable eSIM services incorrectly. This may cause some
// issues where some stale networks showing in UI because its Shill
// configuration doesn't get removed properly during the uninstallation.
// TODO(b/210726568)
class COMPONENT_EXPORT(CHROMEOS_NETWORK) CellularESimUninstallHandler
    : public NetworkStateHandlerObserver {
 public:
  CellularESimUninstallHandler();
  CellularESimUninstallHandler(const CellularESimUninstallHandler&) = delete;
  CellularESimUninstallHandler& operator=(const CellularESimUninstallHandler&) =
      delete;
  ~CellularESimUninstallHandler() override;

  void Init(CellularInhibitor* cellular_inhibitor,
            CellularESimProfileHandler* cellular_esim_profile_handler,
            NetworkConfigurationHandler* network_configuration_handler,
            NetworkConnectionHandler* network_connection_handler,
            NetworkStateHandler* network_state_handler);

  // Callback that returns true or false to indicate the success or failure of
  // an uninstallation request.
  using UninstallRequestCallback = base::OnceCallback<void(bool success)>;

  // Uninstalls an ESim profile and network with given |iccid| and
  // |esim_profile_path| that is installed in Euicc with the given
  // |euicc_path|.
  void UninstallESim(const std::string& iccid,
                     const dbus::ObjectPath& esim_profile_path,
                     const dbus::ObjectPath& euicc_path,
                     UninstallRequestCallback callback);

  // Resets memory ie. Removes all eSIM profiles on the Euicc with given
  // |euicc_path|.
  void ResetEuiccMemory(const dbus::ObjectPath& euicc_path,
                        UninstallRequestCallback callback);

 private:
  // NetworkStateHandlerObserver:
  void NetworkListChanged() override;
  void OnShuttingDown() override;

  friend class CellularESimUninstallHandlerTest;
  FRIEND_TEST_ALL_PREFIXES(CellularESimUninstallHandlerTest, Success);
  FRIEND_TEST_ALL_PREFIXES(CellularESimUninstallHandlerTest,
                           Success_AlreadyDisabled);
  FRIEND_TEST_ALL_PREFIXES(CellularESimUninstallHandlerTest, DisconnectFailure);
  FRIEND_TEST_ALL_PREFIXES(CellularESimUninstallHandlerTest, HermesFailure);
  FRIEND_TEST_ALL_PREFIXES(CellularESimUninstallHandlerTest, MultipleRequests);
  FRIEND_TEST_ALL_PREFIXES(CellularESimUninstallHandlerTest,
                           StubCellularNetwork);
  FRIEND_TEST_ALL_PREFIXES(CellularESimUninstallHandlerTest, ResetEuiccMemory);

  // Timeout when waiting for network list change after removing network
  // service. Service removal continues with next service.
  static const base::TimeDelta kNetworkListWaitTimeout;

  enum class UninstallState {
    kIdle,
    kCheckingNetworkState,
    kDisconnectingNetwork,
    kInhibitingShill,
    kRequestingInstalledProfiles,
    kDisablingProfile,
    kUninstallingProfile,
    kRemovingShillService,
    kWaitingForNetworkListUpdate,
  };
  friend std::ostream& operator<<(std::ostream& stream,
                                  const UninstallState& step);

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class UninstallESimResult {
    kSuccess = 0,
    kNetworkNotFound = 1,
    kDisconnectFailed = 2,
    kInhibitFailed = 3,
    kRefreshProfilesFailed = 4,
    kDisableProfileFailed = 5,
    kUninstallProfileFailed = 6,
    kRemoveServiceFailed = 7,
    kMaxValue = kRemoveServiceFailed
  };

  // Represents ESim uninstallation request parameters. Requests are queued and
  // processed one at a time. |esim_profile_path| and |euicc_path| are nullopt
  // for stale eSIM service removal requests. These requests skip directly to
  // Shill configuration removal.
  struct UninstallRequest {
    UninstallRequest(const absl::optional<std::string>& iccid,
                     const absl::optional<dbus::ObjectPath>& esim_profile_path,
                     const absl::optional<dbus::ObjectPath>& euicc_path,
                     bool reset_euicc,
                     UninstallRequestCallback callback);
    ~UninstallRequest();
    absl::optional<std::string> iccid;
    absl::optional<dbus::ObjectPath> esim_profile_path;
    absl::optional<dbus::ObjectPath> euicc_path;
    bool reset_euicc;
    UninstallRequestCallback callback;
    std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock;
  };
  friend std::ostream& operator<<(std::ostream& stream,
                                  const UninstallRequest& request);

  void ProcessPendingUninstallRequests();
  void TransitionToUninstallState(UninstallState next_state);
  void CompleteCurrentRequest(UninstallESimResult result);

  std::string GetIdForCurrentRequest() const;
  const NetworkState* GetNetworkStateForCurrentRequest() const;

  void CheckActiveNetworkState();

  void AttemptNetworkDisconnect(const NetworkState* network);
  void OnDisconnectSuccess();
  void OnDisconnectFailure(const std::string& error_name);

  void AttemptShillInhibit();
  void OnShillInhibit(
      std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock);

  void AttemptRequestInstalledProfiles();
  void OnRefreshProfileListResult(
      std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock);

  void AttemptDisableProfile();
  void OnDisableProfile(HermesResponseStatus status);

  void AttemptUninstallProfile();
  void OnUninstallProfile(HermesResponseStatus status);

  void AttemptRemoveShillService();
  void OnRemoveServiceSuccess();
  void OnRemoveServiceFailure(const std::string& error_name);
  void OnNetworkListWaitTimeout();

  absl::optional<dbus::ObjectPath> GetEnabledCellularESimProfilePath();
  NetworkStateHandler::NetworkStateList GetESimCellularNetworks() const;
  const NetworkState* GetNextResetServiceToRemove() const;

  UninstallState state_ = UninstallState::kIdle;
  base::circular_deque<std::unique_ptr<UninstallRequest>> uninstall_requests_;

  base::OneShotTimer network_list_wait_timer_;

  CellularInhibitor* cellular_inhibitor_ = nullptr;
  CellularESimProfileHandler* cellular_esim_profile_handler_ = nullptr;
  NetworkConfigurationHandler* network_configuration_handler_ = nullptr;
  NetworkConnectionHandler* network_connection_handler_ = nullptr;
  NetworkStateHandler* network_state_handler_ = nullptr;

  base::WeakPtrFactory<CellularESimUninstallHandler> weak_ptr_factory_{this};
};

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_CELLULAR_ESIM_UNINSTALL_HANDLER_H_
