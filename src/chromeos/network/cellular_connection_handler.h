// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_CELLULAR_CONNECTION_HANDLER_H_
#define CHROMEOS_NETWORK_CELLULAR_CONNECTION_HANDLER_H_

#include <memory>

#include "base/containers/queue.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "chromeos/dbus/hermes/hermes_response_status.h"
#include "chromeos/network/cellular_inhibitor.h"
#include "chromeos/network/network_state_handler_observer.h"
#include "dbus/object_path.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace chromeos {

class CellularESimProfileHandler;
class CellularInhibitor;
class NetworkStateHandler;
class NetworkState;

// Prepares cellular networks for connection. Before we can connect to a
// cellular network, the network must be backed by Shill and must have its
// Connectable property set to true (meaning that it is the selected SIM
// profile in its slot).
//
// For pSIM networks, Chrome OS only supports a single physical SIM slot, so
// pSIM networks should always have their Connectable properties set to true as
// long as they are backed by Shill. Shill is expected to create a Service for
// each pSIM, so the only thing that needs to be done is to wait for that pSIM
// Service to be created by Shill.
//
// For eSIM networks, it is possible that there are multiple eSIM profiles on a
// single EUICC; in this case, Connectable == false refers to a disabled eSIM
// profile which must be enabled via Hermes before a connection can succeed. The
// steps for an eSIM network are:
//   (1) Check to see if the profile is already enabled; if so, return early.
//   (2) Inhibit cellular scans.
//   (3) Request installed profiles from Hermes.
//   (4) Enable the relevant profile.
//   (5) Uninhibit cellular scans.
//   (6) Wait until the associated NetworkState becomes connectable.
//
// Note that if this class receives multiple connection requests, it processes
// them in FIFO order.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) CellularConnectionHandler
    : public NetworkStateHandlerObserver {
 public:
  CellularConnectionHandler();
  CellularConnectionHandler(const CellularConnectionHandler&) = delete;
  CellularConnectionHandler& operator=(const CellularConnectionHandler&) =
      delete;
  ~CellularConnectionHandler() override;

  void Init(NetworkStateHandler* network_state_handler,
            CellularInhibitor* cellular_inhibitor,
            CellularESimProfileHandler* cellular_esim_profile_handler);

  // Success callback which receives the network's service path as a parameter.
  typedef base::OnceCallback<void(const std::string&)> SuccessCallback;

  // Error callback which receives the network's service path as the first
  // parameter and an error name as the second parameter. If no service path is
  // available (e.g., if no network with the given ICCID was found), an empty
  // string is passed as the first parameter.
  typedef base::OnceCallback<void(const std::string&, const std::string&)>
      ErrorCallback;

  // Prepares an existing network (i.e., one which has *not* just been
  // installed) for a connection. Upon success, the network will be backed by
  // Shill and will be connectable.
  void PrepareExistingCellularNetworkForConnection(
      const std::string& iccid,
      SuccessCallback success_callback,
      ErrorCallback error_callback);

  // Prepares a newly-installed eSIM profile for connection. This should be
  // called immediately after installation succeeds so that the profile is
  // enabled in Hermes. Upon success, the network will be backed by Shill and
  // will be connectable.
  void PrepareNewlyInstalledCellularNetworkForConnection(
      const dbus::ObjectPath& euicc_path,
      const dbus::ObjectPath& profile_path,
      std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock,
      SuccessCallback success_callback,
      ErrorCallback error_callback);

 private:
  friend class CellularConnectionHandlerTest;
  FRIEND_TEST_ALL_PREFIXES(CellularConnectionHandlerTest, NoService);
  FRIEND_TEST_ALL_PREFIXES(CellularConnectionHandlerTest,
                           ServiceAlreadyConnectable);
  FRIEND_TEST_ALL_PREFIXES(CellularConnectionHandlerTest, FailsInhibiting);
  FRIEND_TEST_ALL_PREFIXES(CellularConnectionHandlerTest, NoRelevantEuicc);
  FRIEND_TEST_ALL_PREFIXES(CellularConnectionHandlerTest,
                           FailsRequestingInstalledProfiles);
  FRIEND_TEST_ALL_PREFIXES(CellularConnectionHandlerTest,
                           TimeoutWaitingForConnectable_ESim);
  FRIEND_TEST_ALL_PREFIXES(CellularConnectionHandlerTest,
                           TimeoutWaitingForConnectable_PSim);
  FRIEND_TEST_ALL_PREFIXES(CellularConnectionHandlerTest, Success);
  FRIEND_TEST_ALL_PREFIXES(CellularConnectionHandlerTest,
                           Success_AlreadyEnabled);
  FRIEND_TEST_ALL_PREFIXES(CellularConnectionHandlerTest, ConnectToStub);
  FRIEND_TEST_ALL_PREFIXES(CellularConnectionHandlerTest, MultipleRequests);
  FRIEND_TEST_ALL_PREFIXES(CellularConnectionHandlerTest, NewProfile);

  struct ConnectionRequestMetadata {
    ConnectionRequestMetadata(const std::string& iccid,
                              SuccessCallback success_callback,
                              ErrorCallback error_callback);
    ConnectionRequestMetadata(
        const dbus::ObjectPath& euicc_path,
        const dbus::ObjectPath& profile_path,
        std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock,
        SuccessCallback success_callback,
        ErrorCallback error_callback);
    ~ConnectionRequestMetadata();

    absl::optional<std::string> iccid;
    absl::optional<dbus::ObjectPath> euicc_path;
    absl::optional<dbus::ObjectPath> profile_path;
    std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock;
    SuccessCallback success_callback;
    ErrorCallback error_callback;
  };

  enum class ConnectionState {
    kIdle,
    kCheckingServiceStatus,
    kInhibitingScans,
    kRequestingProfilesBeforeEnabling,
    kEnablingProfile,
    kWaitingForConnectable
  };
  friend std::ostream& operator<<(std::ostream& stream,
                                  const ConnectionState& step);

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class PrepareCellularConnectionResult {
    kSuccess = 0,
    kCouldNotFindNetworkWithIccid = 1,
    kInhibitFailed = 2,
    kCouldNotFindRelevantEuicc = 3,
    kRefreshProfilesFailed = 4,
    kCouldNotFindRelevantESimProfile = 5,
    kEnableProfileFailed = 6,
    kTimeoutWaitingForConnectable = 7,
    kMaxValue = kTimeoutWaitingForConnectable
  };
  static absl::optional<std::string> ResultToErrorString(
      PrepareCellularConnectionResult result);

  // NetworkStateHandlerObserver:
  void NetworkListChanged() override;
  void NetworkPropertiesUpdated(const NetworkState* network) override;
  void NetworkIdentifierTransitioned(const std::string& old_service_path,
                                     const std::string& new_service_path,
                                     const std::string& old_guid,
                                     const std::string& new_guid) override;

  void ProcessRequestQueue();
  void TransitionToConnectionState(ConnectionState state);

  // Invokes the success or error callback, depending on |result|.
  void CompleteConnectionAttempt(PrepareCellularConnectionResult result);

  const NetworkState* GetNetworkStateForCurrentOperation() const;
  absl::optional<dbus::ObjectPath> GetEuiccPathForCurrentOperation() const;
  absl::optional<dbus::ObjectPath> GetProfilePathForCurrentOperation() const;

  void CheckServiceStatus();
  void OnInhibitScanResult(
      std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock);
  void RequestInstalledProfiles();
  void OnRefreshProfileListResult(
      std::unique_ptr<CellularInhibitor::InhibitLock> inhibit_lock);
  void EnableProfile();
  void OnEnableCarrierProfileResult(HermesResponseStatus status);

  void UninhibitScans(
      const absl::optional<std::string>& error_before_uninhibit);
  void OnUninhibitScanResult(
      const absl::optional<std::string>& error_before_uninhibit,
      bool success);
  void HandleNetworkPropertiesUpdate();
  void CheckForConnectable();
  void OnWaitForConnectableTimeout();

  base::OneShotTimer timer_;

  NetworkStateHandler* network_state_handler_ = nullptr;
  CellularInhibitor* cellular_inhibitor_ = nullptr;
  CellularESimProfileHandler* cellular_esim_profile_handler_ = nullptr;

  ConnectionState state_ = ConnectionState::kIdle;
  base::queue<std::unique_ptr<ConnectionRequestMetadata>> request_queue_;

  base::WeakPtrFactory<CellularConnectionHandler> weak_ptr_factory_{this};
};

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove when this file is moved to ash.
namespace ash {
using ::chromeos::CellularConnectionHandler;
}  // namespace ash

#endif  // CHROMEOS_NETWORK_CELLULAR_CONNECTION_HANDLER_H_
