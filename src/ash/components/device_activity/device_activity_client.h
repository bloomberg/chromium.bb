// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_DEVICE_ACTIVITY_DEVICE_ACTIVITY_CLIENT_H_
#define ASH_COMPONENTS_DEVICE_ACTIVITY_DEVICE_ACTIVITY_CLIENT_H_

#include <memory>

#include "base/component_export.h"
#include "base/containers/queue.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "base/timer/timer.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_state_handler_observer.h"
#include "services/network/public/cpp/resource_request.h"
#include "third_party/private_membership/src/private_membership_rlwe_client.h"
#include "url/gurl.h"

namespace network {
class SimpleURLLoader;
class SharedURLLoaderFactory;
}  // namespace network

namespace ash {
namespace device_activity {

// Forward declaration from device_active_use_case.h
class DeviceActiveUseCase;

// Create a delegate which can be used to create fakes in unit tests.
// Fake via. delegate is required for creating deterministic unit tests.
class COMPONENT_EXPORT(ASH_DEVICE_ACTIVITY) PsmDelegate {
 public:
  virtual ~PsmDelegate() = default;
  virtual rlwe::StatusOr<
      std::unique_ptr<private_membership::rlwe::PrivateMembershipRlweClient>>
  CreatePsmClient(private_membership::rlwe::RlweUseCase use_case,
                  const std::vector<private_membership::rlwe::RlwePlaintextId>&
                      plaintext_ids) = 0;
};

// Observes the network for connected state to determine whether the device
// is active in a given window.
// State Transition flow:
// kIdle -> kCheckingMembershipOprf -> kCheckingMembershipQuery
// -> kIdle or (kCheckingIn -> kIdle)
//
// TODO(https://crbug.com/1302175): Move methods passing DeviceActiveUseCase* to
// methods of DeviceActiveUseCase class.
class COMPONENT_EXPORT(ASH_DEVICE_ACTIVITY) DeviceActivityClient
    : public chromeos::NetworkStateHandlerObserver {
 public:
  // Tracks the state the client is in, given the use case (i.e DAILY).
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class State {
    kUnknown = 0,  // Default value, typically we should never be in this state.
    kIdle = 1,     // Wait on network connection OR |report_timer_| to trigger.
    kCheckingMembershipOprf = 2,   // Phase 1 of the |CheckMembership| request.
    kCheckingMembershipQuery = 3,  // Phase 2 of the |CheckMembership| request.
    kCheckingIn = 4,               // |CheckIn| PSM device active request.
    kHealthCheck = 5,              // Query to perform server health check.
    kMaxValue = kHealthCheck,
  };

  // Categorize PSM response codes which will be used when bucketing UMA
  // histograms.
  //
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class PsmResponse {
    kUnknown = 0,  // Uncategorized response type returned.
    kSuccess = 1,  // Successfully completed PSM request.
    kError = 2,    // Error completing PSM request.
    kTimeout = 3,  // Timed out while completing PSM request.
    kMaxValue = kTimeout,
  };

  // Categorize device activity methods which will be used when bucketing UMA
  // histograms by number of calls to each method.
  // Enum listed keys map to methods in |DeviceActivityController| and
  // |DeviceActivityClient|.
  //
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class DeviceActivityMethod {
    kUnknown = 0,
    kDeviceActivityControllerConstructor = 1,
    kDeviceActivityControllerDestructor = 2,
    kDeviceActivityControllerStart = 3,
    kDeviceActivityControllerOnPsmDeviceActiveSecretFetched = 4,
    kDeviceActivityControllerOnMachineStatisticsLoaded = 5,
    kDeviceActivityClientConstructor = 6,
    kDeviceActivityClientDestructor = 7,
    kDeviceActivityClientOnNetworkOnline = 8,
    kDeviceActivityClientOnNetworkOffline = 9,
    kDeviceActivityClientReportUseCases = 10,
    kDeviceActivityClientCancelUseCases = 11,
    kDeviceActivityClientTransitionOutOfIdle = 12,
    kDeviceActivityClientTransitionToHealthCheck = 13,
    kDeviceActivityClientOnHealthCheckDone = 14,
    kDeviceActivityClientTransitionToCheckMembershipOprf = 15,
    kDeviceActivityClientOnCheckMembershipOprfDone = 16,
    kDeviceActivityClientTransitionToCheckMembershipQuery = 17,
    kDeviceActivityClientOnCheckMembershipQueryDone = 18,
    kDeviceActivityClientTransitionToCheckIn = 19,
    kDeviceActivityClientOnCheckInDone = 20,
    kDeviceActivityClientTransitionToIdle = 21,
    kMaxValue = kDeviceActivityClientTransitionToIdle,
  };

  // Records UMA histogram for number of times various methods are called in
  // device_activity/.
  static void RecordDeviceActivityMethodCalled(
      DeviceActivityMethod method_name);

  // Fires device active pings while the device network is connected.
  DeviceActivityClient(
      NetworkStateHandler* handler,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      std::unique_ptr<PsmDelegate> psm_delegate,
      std::unique_ptr<base::RepeatingTimer> report_timer,
      const std::string& fresnel_server_url,
      const std::string& api_key,
      std::vector<std::unique_ptr<DeviceActiveUseCase>> use_cases);
  DeviceActivityClient(const DeviceActivityClient&) = delete;
  DeviceActivityClient& operator=(const DeviceActivityClient&) = delete;
  ~DeviceActivityClient() override;

  // Returns pointer to |report_timer_|.
  base::RepeatingTimer* GetReportTimer();

  // NetworkStateHandlerObserver overridden method.
  void DefaultNetworkChanged(const NetworkState* network) override;

  State GetState() const;

  // Used for testing.
  std::vector<DeviceActiveUseCase*> GetUseCases() const;

 private:
  // Handles device network connecting successfully.
  void OnNetworkOnline();

  // Handle device network disconnecting successfully.
  void OnNetworkOffline();

  // Return Fresnel server network request endpoints determined by the |state_|.
  GURL GetFresnelURL() const;

  // Called when device network comes online or |report_timer_| executing.
  // Reports each use case in a sequenced order.
  void ReportUseCases();

  // Called when device network goes offline.
  // Since the network connection is severed, any pending network requests will
  // be cleaned up.
  // After calling this method: |state_| set to |kIdle|.
  void CancelUseCases();

  // Callback from |ReportUseCases()| handling whether a use case needs
  // to be reported for the time window.
  void TransitionOutOfIdle(DeviceActiveUseCase* current_use_case);

  // Send Health Check network request and update |state_|.
  // Before calling this method: |state_| is expected to be |kIdle|.
  // After calling this method: |state_| set to |kHealthCheck|.
  void TransitionToHealthCheck();

  // Callback from asynchronous method |TransitionToHealthCheck|.
  void OnHealthCheckDone(std::unique_ptr<std::string> response_body);

  // Send Oprf network request and update |state_|.
  // Before calling this method: |state_| is expected to be |kIdle|.
  // After calling this method:  |state_| set to |kCheckingMembershipOprf|.
  void TransitionToCheckMembershipOprf(DeviceActiveUseCase* current_use_case);

  // Callback from asynchronous method |TransitionToCheckMembershipOprf|.
  void OnCheckMembershipOprfDone(DeviceActiveUseCase* current_use_case,
                                 std::unique_ptr<std::string> response_body);

  // Send Query network request and update |state_|.
  // Before calling this method: |state_| is expected to be
  // |kCheckingMembershipOprf|.
  // After calling this method:  |state_| set to |kCheckingMembershipQuery|.
  void TransitionToCheckMembershipQuery(
      const private_membership::rlwe::PrivateMembershipRlweOprfResponse&
          oprf_response,
      DeviceActiveUseCase* current_use_case);

  // Callback from asynchronous method |TransitionToCheckMembershipQuery|.
  // Check in PSM id based on |response_body| from CheckMembershipQuery.
  void OnCheckMembershipQueryDone(DeviceActiveUseCase* current_use_case,
                                  std::unique_ptr<std::string> response_body);

  // Send Import network request and update |state_|.
  // Before calling this method: |state_| is expected to be either
  // |kCheckingMembershipQuery| or |kIdle|.
  // After calling this method:  |state_| set to |kCheckingIn|.
  void TransitionToCheckIn(DeviceActiveUseCase* current_use_case);

  // Callback from asynchronous method |TransitionToCheckIn|.
  void OnCheckInDone(DeviceActiveUseCase* current_use_case,
                     std::unique_ptr<std::string> response_body);

  // Updates |state_| to |kIdle| and resets state based member variables.
  void TransitionToIdle(DeviceActiveUseCase* current_use_case);

  // Tracks the current state of the DeviceActivityClient.
  State state_ = State::kIdle;

  // Keep track of whether the device is connected to the network.
  bool network_connected_ = false;

  // Time the device last transitioned out of idle state.
  base::Time last_transition_out_of_idle_time_;

  // Generated when entering new |state_| and reset when leaving |state_|.
  // This field is only used to determine total state duration, which is
  // reported to UMA via. histograms.
  base::ElapsedTimer state_timer_;

  // Tracks the visible networks and their properties.
  // |network_state_handler_| outlives the lifetime of this class.
  // |ChromeBrowserMainPartsAsh| initializes the network_state object as
  // part of the |dbus_services_|, before |DeviceActivityClient| is initialized.
  // Similarly, |DeviceActivityClient| is destructed before |dbus_services_|.
  NetworkStateHandler* const network_state_handler_;

  // Shared |url_loader_| object used to handle ongoing network requests.
  std::unique_ptr<network::SimpleURLLoader> url_loader_;

  // The URLLoaderFactory we use to issue network requests.
  // |url_loader_factory_| outlives |url_loader_|.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // Abstract class used to generate the |psm_rlwe_client_|.
  std::unique_ptr<PsmDelegate> psm_delegate_;

  // Tries reporting device actives every |kTimeToRepeat| from when this class
  // is initialized. Time of class initialization depends on when the device is
  // turned on (when |ChromeBrowserMainPartsAsh::PostBrowserStart| is run).
  std::unique_ptr<base::RepeatingTimer> report_timer_;

  // Base Fresnel server URL is set by |DeviceActivityClient| constructor.
  const std::string fresnel_base_url_;

  // API key used to authenticate with the Fresnel server. This key is read from
  // the chrome-internal repository and is not publicly exposed in Chromium.
  const std::string api_key_;

  // Vector of supported use cases containing the methods and metadata required
  // to counting device actives.
  const std::vector<std::unique_ptr<DeviceActiveUseCase>> use_cases_;

  // Contains the use cases to report active for.
  // The front of the queue represents the use case trying to be reported.
  // |ReportUseCases| initializes this field using the |use_cases_|.
  // |TransitionToIdle| pops from this field to report each pending use case.
  std::queue<DeviceActiveUseCase*> pending_use_cases_;

  // Automatically cancels callbacks when the referent of weakptr gets
  // destroyed.
  base::WeakPtrFactory<DeviceActivityClient> weak_factory_{this};
};

}  // namespace device_activity
}  // namespace ash

#endif  // ASH_COMPONENTS_DEVICE_ACTIVITY_DEVICE_ACTIVITY_CLIENT_H_
