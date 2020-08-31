// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_INSTALLER_H_
#define CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_INSTALLER_H_

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/scoped_observer.h"
#include "chrome/browser/chromeos/crostini/ansible/ansible_management_service.h"
#include "chrome/browser/chromeos/crostini/crostini_installer_ui_delegate.h"
#include "chrome/browser/chromeos/crostini/crostini_manager.h"
#include "chrome/browser/chromeos/crostini/crostini_types.mojom-forward.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace base {
class RepeatingTimer;
}  // namespace base

namespace crostini {

class CrostiniInstaller : public KeyedService,
                          public CrostiniManager::RestartObserver,
                          public CrostiniInstallerUIDelegate,
                          public AnsibleManagementService::Observer {
 public:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  // When you add entries to this enum don't forget to update enums.xml
  enum class SetupResult {
    kNotStarted = 0,
    // kUserCancelled = 1,
    kSuccess = 2,
    kErrorLoadingTermina = 3,
    kErrorStartingConcierge = 4,
    kErrorCreatingDiskImage = 5,
    kErrorStartingTermina = 6,
    kErrorStartingContainer = 7,
    kErrorOffline = 8,
    kErrorFetchingSshKeys = 9,
    kErrorMountingContainer = 10,
    kErrorSettingUpContainer = 11,

    kUserCancelledStart = 12,
    kUserCancelledInstallImageLoader = 13,
    kUserCancelledStartConcierge = 14,
    kUserCancelledCreateDiskImage = 15,
    kUserCancelledStartTerminaVm = 16,
    kUserCancelledCreateContainer = 17,
    kUserCancelledStartContainer = 18,
    kUserCancelledSetupContainer = 19,
    kUserCancelledFetchSshKeys = 20,
    kUserCancelledMountContainer = 21,

    kErrorInsufficientDiskSpace = 22,

    kErrorConfiguringContainer = 23,
    kUserCancelledConfiguringContainer = 24,

    kErrorCreateContainer = 25,
    kErrorUnknown = 26,

    kMaxValue = kErrorUnknown,
  };

  static CrostiniInstaller* GetForProfile(Profile* profile);

  explicit CrostiniInstaller(Profile* profile);
  ~CrostiniInstaller() override;
  void Shutdown() override;

  void ShowDialog(CrostiniUISurface ui_surface);

  // CrostiniInstallerUIDelegate:
  void Install(CrostiniManager::RestartOptions options,
               ProgressCallback progress_callback,
               ResultCallback result_callback) override;
  void Cancel(base::OnceClosure callback) override;
  void CancelBeforeStart() override;

  // CrostiniManager::RestartObserver:
  void OnStageStarted(crostini::mojom::InstallerState stage) override;
  void OnComponentLoaded(crostini::CrostiniResult result) override;
  void OnConciergeStarted(bool success) override;
  void OnDiskImageCreated(bool success,
                          vm_tools::concierge::DiskImageStatus status,
                          int64_t disk_size_available) override;
  void OnVmStarted(bool success) override;
  void OnContainerDownloading(int32_t download_percent) override;
  void OnContainerCreated(crostini::CrostiniResult result) override;
  void OnContainerSetup(bool success) override;
  void OnContainerStarted(crostini::CrostiniResult result) override;
  void OnSshKeysFetched(bool success) override;
  void OnContainerMounted(bool success) override;

  // AnsibleManagementService::Observer:
  void OnAnsibleSoftwareConfigurationStarted() override;
  void OnAnsibleSoftwareConfigurationFinished(bool success) override;

  // Return true if internal state allows starting installation.
  bool CanInstall();

  void set_require_cleanup_for_testing(bool require_cleanup) {
    require_cleanup_ = require_cleanup;
  }
  void set_skip_launching_terminal_for_testing() {
    skip_launching_terminal_for_testing_ = true;
  }

 private:
  enum class State {
    IDLE,
    INSTALLING,
    ERROR,                    // Something unexpected happened.
    CANCEL_CLEANUP,           // Deleting a partial installation.
    CANCEL_ABORT_CHECK_DISK,  // Don't proceed after checking disk.
  };

  void RunProgressCallback();
  void UpdateState(State new_state);
  void UpdateInstallingState(
      crostini::mojom::InstallerState new_installing_state,
      bool run_callback = true);
  void HandleError(crostini::mojom::InstallerError error);
  void FinishCleanup(crostini::CrostiniResult result);
  void RecordSetupResult(SetupResult result);

  void OnCrostiniRestartFinished(crostini::CrostiniResult result);
  void OnAvailableDiskSpace(int64_t bytes);

  void OnCrostiniRemovedAfterConfigurationFailed(
      crostini::CrostiniResult result);

  Profile* profile_;

  State state_ = State::IDLE;
  crostini::mojom::InstallerState installing_state_;
  base::TimeTicks install_start_time_;
  base::Time installing_state_start_time_;
  base::RepeatingTimer state_progress_timer_;
  bool require_cleanup_;
  int64_t free_disk_space_;
  int32_t container_download_percent_;
  crostini::CrostiniManager::RestartId restart_id_ =
      crostini::CrostiniManager::kUninitializedRestartId;
  CrostiniManager::RestartOptions restart_options_;

  bool skip_launching_terminal_for_testing_ = false;

  ProgressCallback progress_callback_;
  ResultCallback result_callback_;
  base::OnceClosure cancel_callback_;

  ScopedObserver<AnsibleManagementService, AnsibleManagementService::Observer>
      ansible_management_service_observer_{this};

  base::WeakPtrFactory<CrostiniInstaller> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CrostiniInstaller);
};

}  // namespace crostini

#endif  // CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_INSTALLER_H_
