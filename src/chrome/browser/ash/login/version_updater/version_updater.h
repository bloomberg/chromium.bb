// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_VERSION_UPDATER_VERSION_UPDATER_H_
#define CHROME_BROWSER_ASH_LOGIN_VERSION_UPDATER_VERSION_UPDATER_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/login/screens/network_error.h"
#include "chrome/browser/ash/login/version_updater/update_time_estimator.h"
#include "chromeos/dbus/update_engine/update_engine_client.h"
#include "chromeos/network/portal_detector/network_portal_detector.h"
// TODO(https://crbug.com/1164001): move to forward declaration when migrated.
#include "chromeos/network/network_state.h"

namespace base {
class DefaultTickClock;
}

namespace ash {

// Tries to update system, interacting with UpdateEnglineClient and
// NetworkPortalDetector. Uses callbacks - methods of `delegate_`, which may
// interact with user, change UI etc.
class VersionUpdater : public UpdateEngineClient::Observer,
                       public NetworkPortalDetector::Observer {
 public:
  enum class Result {
    UPDATE_NOT_REQUIRED,
    UPDATE_ERROR,
    UPDATE_SKIPPED,
  };

  enum class State {
    STATE_IDLE = 0,
    STATE_FIRST_PORTAL_CHECK,
    STATE_REQUESTING_USER_PERMISSION,
    STATE_UPDATE,
    STATE_ERROR
  };

  // Stores information about current downloading process, update progress and
  // state.
  struct UpdateInfo {
    UpdateInfo();

    update_engine::StatusResult status;

    // Time left for an update to finish in seconds.
    base::TimeDelta total_time_left;
    // Progress of an update which is based on total time left expectation.
    int better_update_progress = 0;

    // Estimated time left for only downloading stage, in seconds.
    // TODO(crbug.com/1101317): Remove when better update is launched.
    int estimated_time_left_in_secs = 0;
    bool show_estimated_time_left = false;

    // True if VersionUpdater in such a state that progress is not available or
    // applicable (e.g. checking for updates)
    bool progress_unavailable = true;
    std::u16string progress_message = std::u16string();
    // Percent of update progress, between 0 and 100.
    int progress = 0;

    bool requires_permission_for_cellular = false;

    // Information about a pending update. Set if a user permission is required
    // to proceed with the update. The values have to be passed to the update
    // engine in SetUpdateOverCellularOneTimePermission method in order to
    // enable update over cellular network.
    int64_t update_size = 0;
    std::string update_version = std::string();

    // True if in the process of checking for update.
    bool is_checking_for_update = true;

    // Current state.
    State state = State::STATE_IDLE;
  };

  // Interface for callbacks that are called when corresponding events occur
  // during update process.
  class Delegate {
   public:
    // Called when update info changes
    virtual void UpdateInfoChanged(
        const VersionUpdater::UpdateInfo& update_info) = 0;
    // Reports update results.
    virtual void FinishExitUpdate(VersionUpdater::Result result) = 0;
    // Timer notification handler.
    virtual void OnWaitForRebootTimeElapsed() = 0;
    // Called before update check starts.
    virtual void PrepareForUpdateCheck() = 0;
    virtual void UpdateErrorMessage(
        const NetworkPortalDetector::CaptivePortalStatus status,
        const NetworkError::ErrorState& error_state,
        const std::string& network_name) = 0;
    virtual void ShowErrorMessage() = 0;
    virtual void DelayErrorMessage() = 0;
  };

  // Callback type for `GetEOLInfo`
  using EolInfoCallback =
      base::OnceCallback<void(const UpdateEngineClient::EolInfo& eol_info)>;

  explicit VersionUpdater(VersionUpdater::Delegate* delegate);

  VersionUpdater(const VersionUpdater&) = delete;
  VersionUpdater& operator=(const VersionUpdater&) = delete;

  ~VersionUpdater() override;

  // Resets `VersionUpdater` to initial state.
  void Init();

  // Starts network check. If success, starts update check.
  void StartNetworkCheck();
  void StartUpdateCheck();

  void RefreshTimeLeftEstimation();

  void SetUpdateOverCellularOneTimePermission();
  void RejectUpdateOverCellular();
  void RebootAfterUpdate();
  void StartExitUpdate(Result result);

  const UpdateInfo& update_info() { return update_info_; }

  // Has the device already reached its End Of Life (Auto Update Expiration) ?
  void GetEolInfo(EolInfoCallback callback);

  void set_tick_clock_for_testing(const base::TickClock* tick_clock) {
    tick_clock_ = tick_clock;
    time_estimator_.set_tick_clock_for_testing(tick_clock);
  }

  void set_wait_for_reboot_time_for_testing(
      base::TimeDelta wait_for_reboot_time) {
    wait_for_reboot_time_ = wait_for_reboot_time;
  }

  base::OneShotTimer* GetRebootTimerForTesting();
  void UpdateStatusChangedForTesting(const update_engine::StatusResult& status);

 private:
  void RequestUpdateCheck();

  void OnGetEolInfo(EolInfoCallback cb, UpdateEngineClient::EolInfo info);

  // UpdateEngineClient::Observer implementation:
  void UpdateStatusChanged(const update_engine::StatusResult& status) override;

  // NetworkPortalDetector::Observer implementation:
  void OnPortalDetectionCompleted(
      const NetworkState* network,
      const NetworkPortalDetector::CaptivePortalStatus status) override;

  void OnWaitForRebootTimeElapsed();

  void UpdateErrorMessage(
      const NetworkState* network,
      const NetworkPortalDetector::CaptivePortalStatus status);

  // Callback to UpdateEngineClient::SetUpdateOverCellularOneTimePermission
  // called in response to user confirming that the OS update can proceed
  // despite being over cellular charges.
  // `success`: whether the update engine accepted the user permission.
  void OnSetUpdateOverCellularOneTimePermission(bool success);

  // Callback for UpdateEngineClient::RequestUpdateCheck() called from
  // StartUpdateCheck().
  void OnUpdateCheckStarted(UpdateEngineClient::UpdateCheckResult result);

  // Pointer to delegate that owns this VersionUpdater instance.
  Delegate* delegate_;

  std::unique_ptr<base::RepeatingTimer> refresh_timer_;

  // Timer for the interval to wait for the reboot.
  // If reboot didn't happen - ask user to reboot manually.
  base::OneShotTimer reboot_timer_;
  // Time in seconds after which we decide that the device has not rebooted
  // automatically. If reboot didn't happen during this interval, ask user to
  // reboot device manually.
  base::TimeDelta wait_for_reboot_time_;

  // True if there was no notification from NetworkPortalDetector
  // about state for the default network.
  bool is_first_detection_notification_ = true;

  // Ignore fist IDLE status that is sent before VersionUpdater initiated check.
  bool ignore_idle_status_ = true;

  // Stores information about current downloading process, update progress and
  // state. It is sent to Delegate on each UpdateInfoChanged call, and also can
  // be obtained with corresponding getter.
  UpdateInfo update_info_;

  UpdateTimeEstimator time_estimator_;

  const base::TickClock* tick_clock_;

  base::WeakPtrFactory<VersionUpdater> weak_ptr_factory_{this};
};

}  // namespace ash

// TODO(https://crbug.com/1164001): remove when migration is finished.
namespace chromeos {
using ::ash::VersionUpdater;
}

#endif  // CHROME_BROWSER_ASH_LOGIN_VERSION_UPDATER_VERSION_UPDATER_H_
