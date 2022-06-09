// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_ENROLLMENT_AUTO_ENROLLMENT_CONTROLLER_H_
#define CHROME_BROWSER_ASH_LOGIN_ENROLLMENT_AUTO_ENROLLMENT_CONTROLLER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/policy/enrollment/auto_enrollment_client.h"
#include "chrome/browser/ash/policy/enrollment/private_membership/private_membership_rlwe_client.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chromeos/dbus/cryptohome/UserDataAuth.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
class CommandLine;
}

namespace ash {

class SystemClockSyncObservation;

// Drives the forced re-enrollment check (for historical reasons called
// auto-enrollment check), running an AutoEnrollmentClient if appropriate to
// make a decision.
class AutoEnrollmentController {
 public:
  using ProgressCallbackList =
      base::RepeatingCallbackList<void(policy::AutoEnrollmentState)>;

  // Parameter values for the kEnterpriseEnableForcedReEnrollment flag.
  static const char kForcedReEnrollmentAlways[];
  static const char kForcedReEnrollmentNever[];
  static const char kForcedReEnrollmentOfficialBuild[];

  // Parameter values for the kEnterpriseEnableInitialEnrollment flag.
  static const char kInitialEnrollmentAlways[];
  static const char kInitialEnrollmentNever[];
  static const char kInitialEnrollmentOfficialBuild[];

  // Requirement for forced re-enrollment check.
  enum class FRERequirement {
    // The device was setup (has kActivateDateKey) but doesn't have the
    // kCheckEnrollmentKey entry in VPD, or the VPD is corrupted.
    kRequired,
    // The device doesn't have kActivateDateKey, nor kCheckEnrollmentKey entry
    // while the serial number has been successfully read from VPD.
    kNotRequired,
    // FRE check explicitly required by the flag in VPD.
    kExplicitlyRequired,
    // FRE check to be skipped, explicitly stated by the flag in VPD.
    kExplicitlyNotRequired
  };

  // Requirement for initial state determination.
  enum class InitialStateDeterminationRequirement {
    // Initial state determination is not required.
    kNotRequired,
    // Initial state determination is required.
    kRequired,
    // It is not known whether initial state determination would be required
    // because the system clock is not synchronized.
    kUnknownDueToMissingSystemClockSync,
  };

  // Type of auto enrollment or state determination check.
  enum class AutoEnrollmentCheckType {
    kNone,
    // Forced Re-Enrollment check implicitly required because the device is new
    // or lost VPD state.
    kForcedReEnrollmentImplicitlyRequired,
    // Forced Re-Enrollment check explicitly required because the device was
    // previously enterprise-enrolled.
    kForcedReEnrollmentExplicitlyRequired,
    // Initial state determination.
    kInitialStateDetermination,
    // It is not known whether initial state determination would be required
    // because the system clock is not synchronized.
    kUnknownDueToMissingSystemClockSync,
  };

  // State of the system clock.
  enum class SystemClockSyncState {
    // This `AutoEnrollmentController` has not tried to wait for the system
    // clock sync state yet.
    kCanWaitForSync,
    // Currently waiting for the system clock to become synchronized.
    kWaitingForSync,
    // Waiting for the system clock to become synchronized timed out.
    kSyncFailed,
    // The system clock is synchronized
    kSynchronized
  };

  // Returns true if forced re-enrollment is enabled based on command-line flags
  // and official build status.
  static bool IsFREEnabled();

  // Returns true if initial enrollment is enabled based on command-line
  // flags and official build status.
  static bool IsInitialEnrollmentEnabled();

  // Returns true if any either FRE or initial enrollment are enabled.
  static bool IsEnabled();

  // Returns true if it is determined to use the fake PSM (private set
  // membership) RLWE client based on command-line flags.
  static bool ShouldUseFakePsmRlweClient();

  // Returns whether the FRE auto-enrollment check is required. When
  // kCheckEnrollmentKey VPD entry is present, it is explicitly stating whether
  // the forced re-enrollment is required or not. Otherwise, for backward
  // compatibility with devices upgrading from an older version of Chrome OS,
  // the kActivateDateKey VPD entry is queried. If it's missing, FRE is not
  // required. This enables factories to start full guest sessions for testing,
  // see http://crbug.com/397354 for more context. The requirement for the
  // machine serial number to be present is a sanity-check to ensure that the
  // VPD has actually been read successfully. If VPD read failed, the FRE check
  // is required.
  static FRERequirement GetFRERequirement();

  // Returns the type of auto-enrollment check performed by this client. This
  // will be `AutoEnrollmentCheckType::kNone` before `Start()` has been called.
  AutoEnrollmentCheckType auto_enrollment_check_type() const {
    return auto_enrollment_check_type_;
  }

  AutoEnrollmentController();

  AutoEnrollmentController(const AutoEnrollmentController&) = delete;
  AutoEnrollmentController& operator=(const AutoEnrollmentController&) = delete;

  ~AutoEnrollmentController();

  // Starts the auto-enrollment check.  Safe to call multiple times: aborts in
  // case a check is currently running or a decision has already been made.
  void Start();

  // Retry checking.
  void Retry();

  // Registers a callback to invoke on state changes.
  base::CallbackListSubscription RegisterProgressCallback(
      const ProgressCallbackList::CallbackType& callback);

  policy::AutoEnrollmentState state() const { return state_; }

  // Sets the factory that will be used to create the `AutoEnrollmentClient`.
  // Ownership is not transferred when calling this - the caller must ensure
  // that the `Factory` pointed to by `auto_enrollment_client_factory` remains
  // valid while this `AutoEnrollmentController` is using it.
  // To use the default factory again, call with nullptr.
  void SetAutoEnrollmentClientFactoryForTesting(
      policy::AutoEnrollmentClient::Factory* auto_enrollment_client_factory);

 private:
  // Determines the FRE and Initial Enrollment requirement and starts initial
  // enrollment if necessary. If Initial Enrollment would be skipped and the
  // system clock has not been synchronized yet, triggers waiting for system
  // clock sync and will be called again when the system clock state is known.
  void StartWithSystemClockSyncState();

  // Returns whether the initial state determination is required.
  static InitialStateDeterminationRequirement
  GetInitialStateDeterminationRequirement(
      SystemClockSyncState system_clock_sync_state);

  // Determines the type of auto-enrollment check that should be done.
  // Returning AutoEnrollmentCheckType::kUnknownDueToMissingSystemClockSync
  // indicates that it is not known yet whether Initial Enrollment should be
  // done because the system clock has not been synchronized yet.
  // In this case, the caller is supposed to call this again after the system
  // clock has been synchronized.
  static AutoEnrollmentCheckType DetermineAutoEnrollmentCheckType(
      SystemClockSyncState system_clock_sync_state);

  // Returns true if the FRE check should be done according to command-line
  // switches and device state.
  static bool ShouldDoFRECheck(base::CommandLine* command_line,
                               FRERequirement fre_requirement);
  // Returns true if the Initial Enrollment check should be done according to
  // command-line switches and device state.
  bool ShouldDoInitialEnrollmentCheck();

  // Callback for the ownership status check.
  void OnOwnershipStatusCheckDone(
      DeviceSettingsService::OwnershipStatus status);

  // Starts the auto-enrollment client for forced re-enrollment.
  void StartClientForFRE(const std::vector<std::string>& state_keys);

  // Called when the system clock has been synchronized or a timeout has been
  // reached while waiting for the system clock sync.
  void OnSystemClockSyncResult(bool system_clock_synchronized);

  // Starts the auto-enrollment client for initial enrollment.
  void StartClientForInitialEnrollment();

  // Sets `state_` and notifies `progress_callbacks_`.
  void UpdateState(policy::AutoEnrollmentState state);

  // Clears everything that needs to be cleared at OOBE if
  // the device gets the response that forced re-enrollment is not required.
  // This currently removes firmware management parameters and sets
  // block_devmode=0 and check_enrollment=0 in RW_VPD by making asynchronous
  // calls to the respective D-Bus services.
  // The notifications have to be sent only after the FWMP and VPD is cleared,
  // because the user might try to switch to devmode. In this case, if
  // block_devmode is in FWMP and the clear operation didn't finish, the switch
  // would be denied. Also the safeguard timer has to be active until the FWMP
  // is cleared to avoid the risk of blocked flow.
  void StartCleanupForcedReEnrollment();

  // Makes a D-Bus call to cryptohome to remove the firmware management
  // parameters (FWMP) from TPM. Stops the `safeguard_timer_` and notifies the
  // `progress_callbacks_` in case cryptohome does not become available and the
  // timer is still running.
  // `service_is_ready` indicates if the cryptohome D-Bus service is ready.
  void StartRemoveFirmwareManagementParameters(bool service_is_ready);

  // Callback for RemoveFirmwareManagementParameters(). If an error is received
  // here, it is logged only, without changing the flow after that, because
  // the FWMP is used only for newer devices.
  // This also starts the VPD clearing process.
  void OnFirmwareManagementParametersRemoved(
      absl::optional<user_data_auth::RemoveFirmwareManagementParametersReply>
          reply);

  // Makes a D-Bus call to session_manager to set block_devmode=0 and
  // check_enrollment=0 in RW_VPD. Stops the `safeguard_timer_` and notifies the
  // `progress_callbacks_` in case session manager does not become available
  // and the timer is still running.
  // `service_is_ready` indicates if the session manager D-Bus service is ready.
  void StartClearForcedReEnrollmentVpd(bool service_is_ready);

  // Callback for ClearForcedReEnrollmentVpd(). If an error is received
  // here, it is logged only, without changing the flow after that.
  // This also notifies the `progress_callbacks_` since the forced re-enrollment
  // cleanup is finished at this point.
  void OnForcedReEnrollmentVpdCleared(bool reply);

  // Handles timeout of the safeguard timer and stops waiting for a result.
  void Timeout();

  // Returns the factory that should be used to construct a new
  // `AutoEnrollmentClient`.
  policy::AutoEnrollmentClient::Factory* GetAutoEnrollmentClientFactory();

  // Unowned pointer. If not nullptr, this will be used to create the `client_`.
  // It can be set using `SetAutoEnrollmentClientFactoryForTesting`.
  policy::AutoEnrollmentClient::Factory*
      testing_auto_enrollment_client_factory_ = nullptr;

  // Constructs the PSM RLWE client. It will either create a fake or real
  // implementation of the client.
  // It is only used for PSM during creating the client for initial enrollment.
  std::unique_ptr<policy::PrivateMembershipRlweClient::Factory>
      psm_rlwe_client_factory_;

  policy::AutoEnrollmentState state_ = policy::AUTO_ENROLLMENT_STATE_IDLE;
  ProgressCallbackList progress_callbacks_;

  std::unique_ptr<policy::AutoEnrollmentClient> client_;

  // This timer acts as a belt-and-suspenders safety for the case where one of
  // the asynchronous steps required to make the auto-enrollment decision
  // doesn't come back. Even though in theory they should all terminate, better
  // safe than sorry: There are DBus interactions, an entire network stack etc.
  // - just too many moving pieces to be confident there are no bugs. If
  // something goes wrong, the timer will ensure that a decision gets made
  // eventually, which is crucial to not block OOBE forever. See
  // http://crbug.com/433634 for background.
  base::OneShotTimer safeguard_timer_;

  // Which type of auto-enrollment check is being performed by this
  // `AutoEnrollmentClient`.
  AutoEnrollmentCheckType auto_enrollment_check_type_ =
      AutoEnrollmentCheckType::kNone;

  // Utility for waiting until the system clock has been synchronized.
  std::unique_ptr<SystemClockSyncObservation> system_clock_sync_observation_;

  // Current system clock sync state. This is only modified in
  // `OnSystemClockSyncResult` after `system_clock_sync_wait_requested_` has
  // been set to true.
  SystemClockSyncState system_clock_sync_state_ =
      SystemClockSyncState::kCanWaitForSync;

  // Keeps track of number of tries to request state keys.
  int request_state_keys_tries_ = 0;

  // TODO(igorcov): Merge the two weak_ptr factories in one.
  base::WeakPtrFactory<AutoEnrollmentController> client_start_weak_factory_{
      this};
  base::WeakPtrFactory<AutoEnrollmentController> weak_ptr_factory_{this};
};

}  // namespace ash

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace chromeos {
using ::ash::AutoEnrollmentController;
}

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace ash {
using ::chromeos::AutoEnrollmentController;
}

#endif  // CHROME_BROWSER_ASH_LOGIN_ENROLLMENT_AUTO_ENROLLMENT_CONTROLLER_H_
