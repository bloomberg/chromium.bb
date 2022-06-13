// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSTINI_CROSTINI_MANAGER_H_
#define CHROME_BROWSER_ASH_CROSTINI_CROSTINI_MANAGER_H_

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/callback_helpers.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/unguessable_token.h"
#include "chrome/browser/ash/crostini/crostini_low_disk_notification.h"
#include "chrome/browser/ash/crostini/crostini_simple_types.h"
#include "chrome/browser/ash/crostini/crostini_types.mojom-forward.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/ash/crostini/termina_installer.h"
#include "chrome/browser/ash/vm_shutdown_observer.h"
#include "chrome/browser/ash/vm_starting_observer.h"
#include "chrome/browser/component_updater/cros_component_installer_chromeos.h"
#include "chrome/browser/ui/browser.h"
#include "chromeos/dbus/anomaly_detector/anomaly_detector.pb.h"
#include "chromeos/dbus/anomaly_detector/anomaly_detector_client.h"
#include "chromeos/dbus/cicerone/cicerone_client.h"
#include "chromeos/dbus/cicerone/cicerone_service.pb.h"
#include "chromeos/dbus/concierge/concierge_client.h"
#include "chromeos/dbus/concierge/concierge_service.pb.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_state_handler_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "services/device/public/mojom/usb_manager.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class Profile;

namespace guest_os {
class GuestOsStabilityMonitor;
}

namespace crostini {

extern const char kCrostiniStabilityHistogram[];

class CrostiniUpgradeAvailableNotification;
class CrostiniSshfs;

class LinuxPackageOperationProgressObserver {
 public:
  // A successfully started package install will continually fire progress
  // events until it returns a status of SUCCEEDED or FAILED. The
  // |progress_percent| field is given as a percentage of the given step,
  // DOWNLOADING or INSTALLING. If |status| is FAILED, the |error_message|
  // will contain output of the failing installation command.
  virtual void OnInstallLinuxPackageProgress(
      const ContainerId& container_id,
      InstallLinuxPackageProgressStatus status,
      int progress_percent,
      const std::string& error_message) = 0;

  // A successfully started package uninstall will continually fire progress
  // events until it returns a status of SUCCEEDED or FAILED.
  virtual void OnUninstallPackageProgress(const ContainerId& container_id,
                                          UninstallPackageProgressStatus status,
                                          int progress_percent) = 0;
};

class PendingAppListUpdatesObserver : public base::CheckedObserver {
 public:
  // Called whenever the kPendingAppListUpdatesMethod signal is sent.
  virtual void OnPendingAppListUpdates(const ContainerId& container_id,
                                       int count) = 0;
};

class ExportContainerProgressObserver {
 public:
  // DEPCRECATED. A successfully started container export will continually fire
  // progress events until the original callback from ExportLxdContainer is
  // invoked with a status of SUCCESS or CONTAINER_EXPORT_FAILED.
  virtual void OnExportContainerProgress(const ContainerId& container_id,
                                         ExportContainerProgressStatus status,
                                         int progress_percent,
                                         uint64_t progress_speed) = 0;

  // A successfully started container export will continually fire progress
  // events until the original callback from ExportLxdContainer is invoked with
  // a status of SUCCESS or CONTAINER_EXPORT_FAILED.
  virtual void OnExportContainerProgress(const ContainerId& container_id,
                                         const StreamingExportStatus&) = 0;
};

class ImportContainerProgressObserver {
 public:
  // A successfully started container import will continually fire progress
  // events until the original callback from ImportLxdContainer is invoked with
  // a status of SUCCESS or CONTAINER_IMPORT_FAILED[_*].
  virtual void OnImportContainerProgress(
      const ContainerId& container_id,
      ImportContainerProgressStatus status,
      int progress_percent,
      uint64_t progress_speed,
      const std::string& architecture_device,
      const std::string& architecture_container,
      uint64_t available_space,
      uint64_t minimum_required_space) = 0;
};

class UpgradeContainerProgressObserver {
 public:
  virtual void OnUpgradeContainerProgress(
      const ContainerId& container_id,
      UpgradeContainerProgressStatus status,
      const std::vector<std::string>& messages) = 0;
};

class CrostiniDialogStatusObserver : public base::CheckedObserver {
 public:
  // Called when a Crostini dialog (installer, upgrader, etc.) opens or
  // closes.
  virtual void OnCrostiniDialogStatusChanged(DialogType dialog_type,
                                             bool open) = 0;
};

class CrostiniContainerPropertiesObserver : public base::CheckedObserver {
 public:
  // Called when a container's OS release version changes.
  virtual void OnContainerOsReleaseChanged(const ContainerId& container_id,
                                           bool can_upgrade) = 0;
};

class ContainerStartedObserver : public base::CheckedObserver {
 public:
  // Called when the container has started.
  virtual void OnContainerStarted(const ContainerId& container_id) = 0;
};

class ContainerShutdownObserver : public base::CheckedObserver {
 public:
  // Called when the container has shutdown.
  virtual void OnContainerShutdown(const ContainerId& container_id) = 0;
};

class CrostiniFileChangeObserver : public base::CheckedObserver {
 public:
  // Called when a path registered via AddFileWatch() is changed.
  virtual void OnCrostiniFileChanged(const ContainerId& container_id,
                                     const base::FilePath& path) = 0;
};

// CrostiniManager is a singleton which is used to check arguments for
// ConciergeClient and CiceroneClient. ConciergeClient is dedicated to
// communication with the Concierge service, CiceroneClient is dedicated to
// communication with the Cicerone service and both should remain as thin as
// possible. The existence of Cicerone is abstracted behind this class and
// only the Concierge name is exposed outside of here.
class CrostiniManager : public KeyedService,
                        public chromeos::AnomalyDetectorClient::Observer,
                        public chromeos::ConciergeClient::VmObserver,
                        public chromeos::ConciergeClient::ContainerObserver,
                        public chromeos::CiceroneClient::Observer,
                        public chromeos::NetworkStateHandlerObserver,
                        public chromeos::PowerManagerClient::Observer {
 public:
  using CrostiniResultCallback =
      base::OnceCallback<void(CrostiniResult result)>;
  using ExportLxdContainerResultCallback =
      base::OnceCallback<void(CrostiniResult result,
                              uint64_t container_size,
                              uint64_t compressed_size)>;
  // Callback indicating success or failure
  using BoolCallback = base::OnceCallback<void(bool success)>;

  using RestartId = int;
  static const RestartId kUninitializedRestartId = -1;

  // Observer class for the Crostini restart flow.
  class RestartObserver {
   public:
    virtual ~RestartObserver() {}
    virtual void OnStageStarted(mojom::InstallerState stage) {}
    virtual void OnComponentLoaded(CrostiniResult result) {}
    virtual void OnDiskImageCreated(bool success,
                                    vm_tools::concierge::DiskImageStatus status,
                                    int64_t disk_size_bytes) {}
    virtual void OnVmStarted(bool success) {}
    virtual void OnLxdStarted(CrostiniResult result) {}
    virtual void OnContainerDownloading(int32_t download_percent) {}
    virtual void OnContainerCreated(CrostiniResult result) {}
    virtual void OnContainerSetup(bool success) {}
    virtual void OnContainerStarted(CrostiniResult result) {}
  };

  struct RestartOptions {
    bool start_vm_only = false;
    bool stop_after_lxd_available = false;
    // Paths to share with VM on startup.
    std::vector<base::FilePath> share_paths;
    // These four options only affect new containers.
    absl::optional<std::string> container_username;
    absl::optional<int64_t> disk_size_bytes;
    absl::optional<std::string> image_server_url;
    absl::optional<std::string> image_alias;

    RestartOptions();
    ~RestartOptions();
    // Add copy version if necessary.
    RestartOptions(RestartOptions&&);
    RestartOptions& operator=(RestartOptions&&);
  };

  static CrostiniManager* GetForProfile(Profile* profile);

  explicit CrostiniManager(Profile* profile);

  CrostiniManager(const CrostiniManager&) = delete;
  CrostiniManager& operator=(const CrostiniManager&) = delete;

  ~CrostiniManager() override;

  base::WeakPtr<CrostiniManager> GetWeakPtr();

  // Returns true if the /dev/kvm directory is present.
  static bool IsDevKvmPresent();

  // Upgrades cros-termina component if the current version is not
  // compatible. This is a no-op if chromeos::features::kCrostiniUseDlc is
  // enabled.
  void MaybeUpdateCrostini();

  // Installs termina using either component updater or the DLC service
  // depending on the value of chromeos::features::kCrostiniUseDlc
  void InstallTermina(CrostiniResultCallback callback, bool is_initial_install);

  // Try to cancel a previous InstallTermina call. This is done on a best-effort
  // basis, and we cannot signal if/when it succeeds, but once called the
  // callback passed to InstallTermina will never be run.
  void CancelInstallTermina();

  // Unloads and removes termina.
  void UninstallTermina(BoolCallback callback);

  // Checks the arguments for creating a new Termina VM disk image. Creates a
  // disk image for a Termina VM via ConciergeClient::CreateDiskImage.
  // |callback| is called if the arguments are bad, or after the method call
  // finishes.
  using CreateDiskImageCallback =
      base::OnceCallback<void(bool success,
                              vm_tools::concierge::DiskImageStatus,
                              const base::FilePath& disk_path)>;
  void CreateDiskImage(
      // The path to the disk image, including the name of
      // the image itself. The image name should match the
      // name of the VM that it will be used for.
      const std::string& vm_name,
      // The storage location for the disk image
      vm_tools::concierge::StorageLocation storage_location,
      // The logical size of the disk image, in bytes
      int64_t disk_size_bytes,
      CreateDiskImageCallback callback);

  // Checks the arguments for destroying a named Termina VM disk image.
  // Removes the named Termina VM via ConciergeClient::DestroyDiskImage.
  // |callback| is called if the arguments are bad, or after the method call
  // finishes.
  void DestroyDiskImage(
      // The path to the disk image, including the name of the image itself.
      const std::string& vm_name,
      BoolCallback callback);

  using ListVmDisksCallback =
      base::OnceCallback<void(CrostiniResult result, int64_t total_size)>;
  void ListVmDisks(ListVmDisksCallback callback);

  // Checks the arguments for starting a Termina VM. Starts a Termina VM via
  // ConciergeClient::StartTerminaVm. |callback| is called if the arguments
  // are bad, or after the method call finishes.
  void StartTerminaVm(
      // The human-readable name to be assigned to this VM.
      std::string name,
      // Path to the disk image on the host.
      const base::FilePath& disk_path,
      // The number of logical CPU cores that are currently disabled.
      size_t num_cores_disabled,
      BoolCallback callback);

  // Checks the arguments for stopping a Termina VM. Stops the Termina VM via
  // ConciergeClient::StopVm. |callback| is called if the arguments are bad,
  // or after the method call finishes.
  void StopVm(std::string name, CrostiniResultCallback callback);

  // Asynchronously retrieve the Termina VM kernel version using
  // concierge's GetVmEnterpriseReportingInfo method.
  using GetTerminaVmKernelVersionCallback = base::OnceCallback<void(
      const absl::optional<std::string>& maybe_kernel_version)>;
  void GetTerminaVmKernelVersion(GetTerminaVmKernelVersionCallback callback);

  // Wrapper for CiceroneClient::StartLxd with some extra parameter validation.
  // |callback| is called immediately if the arguments are bad, or after LXD has
  // been started.
  void StartLxd(std::string vm_name, CrostiniResultCallback callback);

  // Checks the arguments for creating an Lxd container via
  // CiceroneClient::CreateLxdContainer. |callback| is called immediately if the
  // arguments are bad, or once the container has been created.
  void CreateLxdContainer(ContainerId container_id,
                          absl::optional<std::string> opt_image_server_url,
                          absl::optional<std::string> opt_image_alias,
                          CrostiniResultCallback callback);

  // Checks the arguments for deleting an Lxd container via
  // CiceroneClient::DeleteLxdContainer. |callback| is called immediately if the
  // arguments are bad, or once the container has been deleted.
  void DeleteLxdContainer(ContainerId container_id, BoolCallback callback);

  // Checks the arguments for starting an Lxd container via
  // CiceroneClient::StartLxdContainer. |callback| is called immediately if the
  // arguments are bad, or once the container has been created.
  void StartLxdContainer(ContainerId container_id,
                         CrostiniResultCallback callback);

  // Checks the arguments for setting up an Lxd container user via
  // CiceroneClient::SetUpLxdContainerUser. |callback| is called immediately if
  // the arguments are bad, or once garcon has been started.
  void SetUpLxdContainerUser(ContainerId container_id,
                             std::string container_username,
                             BoolCallback callback);

  // Checks the arguments for exporting an Lxd container via
  // CiceroneClient::ExportLxdContainer. |callback| is called immediately if the
  // arguments are bad, or after the method call finishes.
  void ExportLxdContainer(ContainerId container_id,
                          base::FilePath export_path,
                          ExportLxdContainerResultCallback callback);

  // Checks the arguments for importing an Lxd container via
  // CiceroneClient::ImportLxdContainer. |callback| is called immediately if the
  // arguments are bad, or after the method call finishes.
  void ImportLxdContainer(ContainerId container_id,
                          base::FilePath import_path,
                          CrostiniResultCallback callback);

  // Checks the arguments for cancelling a Lxd container export via
  // CiceroneClient::CancelExportLxdContainer .
  void CancelExportLxdContainer(ContainerId key);

  // Checks the arguments for cancelling a Lxd container import via
  // CiceroneClient::CancelImportLxdContainer.
  void CancelImportLxdContainer(ContainerId key);

  // Checks the arguments for upgrading an existing container via
  // CiceroneClient::UpgradeContainer. An UpgradeProgressObserver should be used
  // to monitor further results.
  void UpgradeContainer(const ContainerId& key,
                        ContainerVersion target_version,
                        CrostiniResultCallback callback);

  // Checks the arguments for canceling the upgrade of an existing container via
  // CiceroneClient::CancelUpgradeContainer.
  void CancelUpgradeContainer(const ContainerId& key,
                              CrostiniResultCallback callback);

  // Asynchronously launches an app as specified by its desktop file id.
  void LaunchContainerApplication(const ContainerId& container_id,
                                  std::string desktop_file_id,
                                  const std::vector<std::string>& files,
                                  bool display_scaled,
                                  CrostiniSuccessCallback callback);

  // Asynchronously gets app icons as specified by their desktop file ids.
  // |callback| is called after the method call finishes.
  using GetContainerAppIconsCallback =
      base::OnceCallback<void(bool success, const std::vector<Icon>& icons)>;
  void GetContainerAppIcons(const ContainerId& container_id,
                            std::vector<std::string> desktop_file_ids,
                            int icon_size,
                            int scale,
                            GetContainerAppIconsCallback callback);

  // Asynchronously retrieve information about a Linux Package (.deb) inside the
  // container.
  using GetLinuxPackageInfoCallback =
      base::OnceCallback<void(const LinuxPackageInfo&)>;
  void GetLinuxPackageInfo(const ContainerId& container_id,
                           std::string package_path,
                           GetLinuxPackageInfoCallback callback);

  // Begin installation of a Linux Package inside the container. If the
  // installation is successfully started, further updates will be sent to
  // added LinuxPackageOperationProgressObservers.
  using InstallLinuxPackageCallback = CrostiniResultCallback;
  void InstallLinuxPackage(const ContainerId& container_id,
                           std::string package_path,
                           InstallLinuxPackageCallback callback);

  // Begin installation of a Linux Package inside the container. If the
  // installation is successfully started, further updates will be sent to
  // added LinuxPackageOperationProgressObservers. Uses a package_id, given
  // by "package_name;version;arch;data", to identify the package to install
  // from the APT repository.
  void InstallLinuxPackageFromApt(const ContainerId& container_id,
                                  std::string package_id,
                                  InstallLinuxPackageCallback callback);

  // Begin uninstallation of a Linux Package inside the container. The package
  // is identified by its associated .desktop file's ID; we don't use package_id
  // to avoid problems with stale package_ids (such as after upgrades). If the
  // uninstallation is successfully started, further updates will be sent to
  // added LinuxPackageOperationProgressObservers.
  void UninstallPackageOwningFile(const ContainerId& container_id,
                                  std::string desktop_file_id,
                                  CrostiniResultCallback callback);

  // Asynchronously gets SSH server public key of container and trusted SSH
  // client private key which can be used to connect to the container.
  // |callback| is called after the method call finishes.
  using GetContainerSshKeysCallback =
      base::OnceCallback<void(bool success,
                              const std::string& container_public_key,
                              const std::string& host_private_key,
                              const std::string& hostname)>;
  void GetContainerSshKeys(const ContainerId& container_id,
                           GetContainerSshKeysCallback callback);

  // Add a relative path to watch within the container homedir. Register as a
  // CrostiniFileChangeObserver to be notified when changes occur. Used by
  // FilesApp.
  void AddFileWatch(const ContainerId& container_id,
                    const base::FilePath& path,
                    BoolCallback callback);
  void RemoveFileWatch(const ContainerId& container_id,
                       const base::FilePath& path);
  void AddFileChangeObserver(CrostiniFileChangeObserver* observer);
  void RemoveFileChangeObserver(CrostiniFileChangeObserver* observer);

  // Lookup vsh session from pid. Used by terminal to open new tabs in cwd.
  using VshSessionCallback =
      base::OnceCallback<void(bool success,
                              const std::string& failure_reason,
                              int32_t container_shell_pid)>;
  void GetVshSession(const ContainerId& container_id,
                     int32_t host_vsh_pid,
                     VshSessionCallback callback);

  // Runs all the steps required to restart the given crostini vm and container.
  // The optional |observer| tracks progress. If provided, it must be alive
  // until the restart completes (i.e. when |callback| is called) or the restart
  // is aborted via |AbortRestartCrostini|.
  RestartId RestartCrostini(ContainerId container_id,
                            CrostiniResultCallback callback,
                            RestartObserver* observer = nullptr);

  RestartId RestartCrostiniWithOptions(ContainerId container_id,
                                       RestartOptions options,
                                       CrostiniResultCallback callback,
                                       RestartObserver* observer = nullptr);

  // Aborts a restart. A "next" restarter with the same ContainerId will run, if
  // there is one. |callback| will be called once the restart has finished
  // aborting
  void AbortRestartCrostini(RestartId restart_id, base::OnceClosure callback);

  // Returns true if the Restart corresponding to |restart_id| is not yet
  // complete.
  bool IsRestartPending(RestartId restart_id);

  // Adds a callback to receive notification of container shutdown.
  void AddShutdownContainerCallback(ContainerId container_id,
                                    base::OnceClosure shutdown_callback);

  // Adds a callback to receive uninstall notification.
  using RemoveCrostiniCallback = CrostiniResultCallback;
  void AddRemoveCrostiniCallback(RemoveCrostiniCallback remove_callback);

  // Add/remove observers for package install and uninstall progress.
  void AddLinuxPackageOperationProgressObserver(
      LinuxPackageOperationProgressObserver* observer);
  void RemoveLinuxPackageOperationProgressObserver(
      LinuxPackageOperationProgressObserver* observer);

  // Add/remove observers for pending app list updates.
  void AddPendingAppListUpdatesObserver(
      PendingAppListUpdatesObserver* observer);
  void RemovePendingAppListUpdatesObserver(
      PendingAppListUpdatesObserver* observer);

  // Add/remove observers for container export/import.
  void AddExportContainerProgressObserver(
      ExportContainerProgressObserver* observer);
  void RemoveExportContainerProgressObserver(
      ExportContainerProgressObserver* observer);
  void AddImportContainerProgressObserver(
      ImportContainerProgressObserver* observer);
  void RemoveImportContainerProgressObserver(
      ImportContainerProgressObserver* observer);

  // Add/remove observers for container upgrade
  void AddUpgradeContainerProgressObserver(
      UpgradeContainerProgressObserver* observer);
  void RemoveUpgradeContainerProgressObserver(
      UpgradeContainerProgressObserver* observer);

  // Add/remove vm shutdown observers.
  void AddVmShutdownObserver(ash::VmShutdownObserver* observer);
  void RemoveVmShutdownObserver(ash::VmShutdownObserver* observer);

  // Add/remove vm starting observers.
  void AddVmStartingObserver(ash::VmStartingObserver* observer);
  void RemoveVmStartingObserver(ash::VmStartingObserver* observer);

  // AnomalyDetectorClient::Observer:
  void OnGuestFileCorruption(
      const anomaly_detector::GuestFileCorruptionSignal& signal) override;

  // ConciergeClient::VmObserver:
  void OnVmStarted(const vm_tools::concierge::VmStartedSignal& signal) override;
  void OnVmStopped(const vm_tools::concierge::VmStoppedSignal& signal) override;

  // ConciergeClient::ContainerObserver:
  void OnContainerStartupFailed(
      const vm_tools::concierge::ContainerStartedSignal& signal) override;

  // CiceroneClient::Observer:
  void OnContainerStarted(
      const vm_tools::cicerone::ContainerStartedSignal& signal) override;
  void OnContainerShutdown(
      const vm_tools::cicerone::ContainerShutdownSignal& signal) override;
  void OnInstallLinuxPackageProgress(
      const vm_tools::cicerone::InstallLinuxPackageProgressSignal& signal)
      override;
  void OnUninstallPackageProgress(
      const vm_tools::cicerone::UninstallPackageProgressSignal& signal)
      override;
  void OnLxdContainerCreated(
      const vm_tools::cicerone::LxdContainerCreatedSignal& signal) override;
  void OnLxdContainerDeleted(
      const vm_tools::cicerone::LxdContainerDeletedSignal& signal) override;
  void OnLxdContainerDownloading(
      const vm_tools::cicerone::LxdContainerDownloadingSignal& signal) override;
  void OnTremplinStarted(
      const vm_tools::cicerone::TremplinStartedSignal& signal) override;
  void OnLxdContainerStarting(
      const vm_tools::cicerone::LxdContainerStartingSignal& signal) override;
  void OnExportLxdContainerProgress(
      const vm_tools::cicerone::ExportLxdContainerProgressSignal& signal)
      override;
  void OnImportLxdContainerProgress(
      const vm_tools::cicerone::ImportLxdContainerProgressSignal& signal)
      override;
  void OnPendingAppListUpdates(
      const vm_tools::cicerone::PendingAppListUpdatesSignal& signal) override;
  void OnApplyAnsiblePlaybookProgress(
      const vm_tools::cicerone::ApplyAnsiblePlaybookProgressSignal& signal)
      override;
  void OnUpgradeContainerProgress(
      const vm_tools::cicerone::UpgradeContainerProgressSignal& signal)
      override;
  void OnStartLxdProgress(
      const vm_tools::cicerone::StartLxdProgressSignal& signal) override;
  void OnFileWatchTriggered(
      const vm_tools::cicerone::FileWatchTriggeredSignal& signal) override;

  // chromeos::NetworkStateHandlerObserver overrides:
  void ActiveNetworksChanged(const std::vector<const chromeos::NetworkState*>&
                                 active_networks) override;

  // chromeos::PowerManagerClient::Observer overrides:
  void SuspendImminent(power_manager::SuspendImminent::Reason reason) override;
  void SuspendDone(base::TimeDelta sleep_duration) override;

  // Callback for |RemoveSshfsCrostiniVolume| called from |SuspendImminent| when
  // the device is allowed to suspend. Removes metadata associated with the
  // crostini sshfs mount and unblocks a pending suspend.
  void OnRemoveSshfsCrostiniVolume(
      base::UnguessableToken power_manager_suspend_token,
      bool result);

  void RemoveCrostini(std::string vm_name, RemoveCrostiniCallback callback);

  void UpdateVmState(std::string vm_name, VmState vm_state);
  bool IsVmRunning(std::string vm_name);
  // Returns null if VM is not running.
  absl::optional<VmInfo> GetVmInfo(std::string vm_name);
  void AddRunningVmForTesting(std::string vm_name);
  void AddStoppingVmForTesting(std::string vm_name);

  void SetContainerOsRelease(const ContainerId& container_id,
                             const vm_tools::cicerone::OsRelease& os_release);
  const vm_tools::cicerone::OsRelease* GetContainerOsRelease(
      const ContainerId& container_id) const;
  // Returns null if VM or container is not running.
  absl::optional<ContainerInfo> GetContainerInfo(
      const ContainerId& container_id);
  void AddRunningContainerForTesting(std::string vm_name, ContainerInfo info);

  // If the Crostini reporting policy is set, save the last app launch
  // time window and the Termina version in prefs for asynchronous reporting.
  void UpdateLaunchMetricsForEnterpriseReporting();

  // Clear the lists of running VMs and containers.
  // Can be called for testing to skip restart.
  void set_skip_restart_for_testing() { skip_restart_for_testing_ = true; }
  bool skip_restart_for_testing() { return skip_restart_for_testing_; }
  void set_component_manager_load_error_for_testing(
      component_updater::CrOSComponentManager::Error error) {
    component_manager_load_error_for_testing_ = error;
  }

  void SetCrostiniDialogStatus(DialogType dialog_type, bool open);
  // Returns true if the dialog is open.
  bool GetCrostiniDialogStatus(DialogType dialog_type) const;
  void AddCrostiniDialogStatusObserver(CrostiniDialogStatusObserver* observer);
  void RemoveCrostiniDialogStatusObserver(
      CrostiniDialogStatusObserver* observer);

  void AddCrostiniContainerPropertiesObserver(
      CrostiniContainerPropertiesObserver* observer);
  void RemoveCrostiniContainerPropertiesObserver(
      CrostiniContainerPropertiesObserver* observer);

  void AddContainerStartedObserver(ContainerStartedObserver* observer);
  void RemoveContainerStartedObserver(ContainerStartedObserver* observer);
  void AddContainerShutdownObserver(ContainerShutdownObserver* observer);
  void RemoveContainerShutdownObserver(ContainerShutdownObserver* observer);

  bool IsContainerUpgradeable(const ContainerId& container_id) const;
  bool ShouldPromptContainerUpgrade(const ContainerId& container_id) const;
  void UpgradePromptShown(const ContainerId& container_id);
  bool IsUncleanStartup() const;
  void SetUncleanStartupForTesting(bool is_unclean_startup);
  void RemoveUncleanSshfsMounts();
  void DeallocateForwardedPortsCallback(Profile* profile,
                                        const ContainerId& container_id);

  void CallRestarterStartLxdContainerFinishedForTesting(
      CrostiniManager::RestartId id,
      CrostiniResult result);
  void SetInstallTerminaNeverCompletesForTesting(bool never_completes) {
    install_termina_never_completes_ = never_completes;
  }

  // Mounts the user's Crostini home directory so it's accessible from the host.
  // Must be called from the UI thread, no-op if the home directory is already
  // mounted. If this is something running in the background set background to
  // true, if failures are user-visible set it to false. If you're setting
  // base::DoNothing as the callback then background should be true.
  void MountCrostiniFiles(ContainerId container_id,
                          CrostiniResultCallback callback,
                          bool background);

  void GetInstallLocation(base::OnceCallback<void(base::FilePath)> callback);

 private:
  class CrostiniRestarter;

  void RemoveDBusObservers();

  // Callback for ConciergeClient::CreateDiskImage. Called after the Concierge
  // service method finishes.
  void OnCreateDiskImage(
      CreateDiskImageCallback callback,
      absl::optional<vm_tools::concierge::CreateDiskImageResponse> response);

  // Callback for ConciergeClient::DestroyDiskImage. Called after the Concierge
  // service method finishes.
  void OnDestroyDiskImage(
      BoolCallback callback,
      absl::optional<vm_tools::concierge::DestroyDiskImageResponse> response);

  // Callback for ConciergeClient::ListVmDisks. Called after the Concierge
  // service method finishes.
  void OnListVmDisks(
      ListVmDisksCallback callback,
      absl::optional<vm_tools::concierge::ListVmDisksResponse> response);

  // Callback for ConciergeClient::StartTerminaVm. Called after the Concierge
  // service method finishes.  Updates running containers list then calls the
  // |callback| if the container has already been started, otherwise passes the
  // callback to OnStartTremplin.
  void OnStartTerminaVm(
      std::string vm_name,
      BoolCallback callback,
      absl::optional<vm_tools::concierge::StartVmResponse> response);

  // Callback for ConciergeClient::TremplinStartedSignal. Called after the
  // Tremplin service starts. Updates running containers list and then calls the
  // |callback| with true, indicating success.
  void OnStartTremplin(std::string vm_name, BoolCallback callback);

  // Callback for ConciergeClient::StopVm. Called after the Concierge
  // service method finishes.
  void OnStopVm(std::string vm_name,
                CrostiniResultCallback callback,
                absl::optional<vm_tools::concierge::StopVmResponse> response);

  // Callback for ConciergeClient::GetVmEnterpriseReportingInfo.
  // Currently used to report the Termina kernel version for enterprise
  // reporting.
  void OnGetTerminaVmKernelVersion(
      GetTerminaVmKernelVersionCallback callback,
      absl::optional<vm_tools::concierge::GetVmEnterpriseReportingInfoResponse>
          response);

  // Callback for CiceroneClient::StartLxd. May indicate that LXD is still being
  // started in which case we will wait for OnStartLxdProgress events.
  void OnStartLxd(
      std::string vm_name,
      CrostiniResultCallback callback,
      absl::optional<vm_tools::cicerone::StartLxdResponse> response);

  // Callback for CiceroneClient::CreateLxdContainer. May indicate the container
  // is still being created, in which case we will wait for an
  // OnLxdContainerCreated event.
  void OnCreateLxdContainer(
      const ContainerId& container_id,
      CrostiniResultCallback callback,
      absl::optional<vm_tools::cicerone::CreateLxdContainerResponse> response);

  // Callback for CiceroneClient::DeleteLxdContainer.
  void OnDeleteLxdContainer(
      const ContainerId& container_id,
      BoolCallback callback,
      absl::optional<vm_tools::cicerone::DeleteLxdContainerResponse> response);

  // Callback for CiceroneClient::StartLxdContainer.
  void OnStartLxdContainer(
      const ContainerId& container_id,
      CrostiniResultCallback callback,
      absl::optional<vm_tools::cicerone::StartLxdContainerResponse> response);

  // Callback for CiceroneClient::SetUpLxdContainerUser.
  void OnSetUpLxdContainerUser(
      const ContainerId& container_id,
      BoolCallback callback,
      absl::optional<vm_tools::cicerone::SetUpLxdContainerUserResponse>
          response);

  // Callback for CiceroneClient::ExportLxdContainer.
  void OnExportLxdContainer(
      const ContainerId& container_id,
      absl::optional<vm_tools::cicerone::ExportLxdContainerResponse> response);

  // Callback for CiceroneClient::ImportLxdContainer.
  void OnImportLxdContainer(
      const ContainerId& container_id,
      absl::optional<vm_tools::cicerone::ImportLxdContainerResponse> response);

  // Callback for CiceroneClient::CancelExportLxdContainer.
  void OnCancelExportLxdContainer(
      const ContainerId& key,
      absl::optional<vm_tools::cicerone::CancelExportLxdContainerResponse>
          response);

  // Callback for CiceroneClient::CancelImportLxdContainer.
  void OnCancelImportLxdContainer(
      const ContainerId& key,
      absl::optional<vm_tools::cicerone::CancelImportLxdContainerResponse>
          response);

  // Callback for CiceroneClient::UpgradeContainer.
  void OnUpgradeContainer(
      CrostiniResultCallback callback,
      absl::optional<vm_tools::cicerone::UpgradeContainerResponse> response);

  // Callback for CiceroneClient::CancelUpgradeContainer.
  void OnCancelUpgradeContainer(
      CrostiniResultCallback callback,
      absl::optional<vm_tools::cicerone::CancelUpgradeContainerResponse>
          response);

  // Callback for CrostiniManager::LaunchContainerApplication.
  void OnLaunchContainerApplication(
      CrostiniSuccessCallback callback,
      absl::optional<vm_tools::cicerone::LaunchContainerApplicationResponse>
          response);

  // Callback for CrostiniManager::GetContainerAppIcons. Called after the
  // Concierge service finishes.
  void OnGetContainerAppIcons(
      GetContainerAppIconsCallback callback,
      absl::optional<vm_tools::cicerone::ContainerAppIconResponse> response);

  // Callback for CrostiniManager::GetLinuxPackageInfo.
  void OnGetLinuxPackageInfo(
      GetLinuxPackageInfoCallback callback,
      absl::optional<vm_tools::cicerone::LinuxPackageInfoResponse> response);

  // Callback for CrostiniManager::InstallLinuxPackage.
  void OnInstallLinuxPackage(
      InstallLinuxPackageCallback callback,
      absl::optional<vm_tools::cicerone::InstallLinuxPackageResponse> response);

  // Callback for CrostiniManager::UninstallPackageOwningFile.
  void OnUninstallPackageOwningFile(
      CrostiniResultCallback callback,
      absl::optional<vm_tools::cicerone::UninstallPackageOwningFileResponse>
          response);

  // Callback for CrostiniManager::GetContainerSshKeys. Called after the
  // Concierge service finishes.
  void OnGetContainerSshKeys(
      GetContainerSshKeysCallback callback,
      absl::optional<vm_tools::concierge::ContainerSshKeysResponse> response);

  // Callback for AnsibleManagementService::ConfigureDefaultContainer
  void OnDefaultContainerConfigured(bool success);

  // Helper for CrostiniManager::MaybeUpdateCrostini. Makes blocking calls to
  // check for /dev/kvm.
  static void CheckPaths();

  // Helper for CrostiniManager::MaybeUpdateCrostini. Separated because the
  // checking component registration code may block.
  void MaybeUpdateCrostiniAfterChecks();

  void FinishRestart(CrostiniRestarter* restarter, CrostiniResult result);

  // Callback for CrostiniManager::AbortRestartCrostini
  void OnAbortRestartCrostini(RestartId restart_id, base::OnceClosure callback);

  // Callback for CrostiniManager::RemoveCrostini.
  void OnRemoveCrostini(CrostiniResult result);

  void OnVmStoppedCleanup(const std::string& vm_name);

  // Configure the container so that it can sideload apps into Arc++.
  void ConfigureForArcSideload();

  // Tries to query Concierge for the type of disk the named VM has then emits a
  // metric logging the type. Mostly happens async and best-effort.
  void EmitVmDiskTypeMetric(const std::string vm_name);

  Profile* profile_;
  std::string owner_id_;

  bool skip_restart_for_testing_ = false;
  component_updater::CrOSComponentManager::Error
      component_manager_load_error_for_testing_ =
          component_updater::CrOSComponentManager::Error::NONE;

  static bool is_dev_kvm_present_;

  // |is_unclean_startup_| is true when we detect Concierge still running at
  // session startup time, and the last session ended in a crash.
  bool is_unclean_startup_ = false;

  // Callbacks that are waiting on a signal
  std::multimap<ContainerId, CrostiniResultCallback> start_container_callbacks_;
  std::multimap<ContainerId, base::OnceClosure> shutdown_container_callbacks_;
  std::multimap<ContainerId, CrostiniResultCallback>
      create_lxd_container_callbacks_;
  std::multimap<ContainerId, BoolCallback> delete_lxd_container_callbacks_;
  std::map<ContainerId, ExportLxdContainerResultCallback>
      export_lxd_container_callbacks_;
  std::map<ContainerId, CrostiniResultCallback> import_lxd_container_callbacks_;

  // Callbacks to run after Tremplin is started, keyed by vm_name. These are
  // used if StartTerminaVm completes but we need to wait from Tremplin to
  // start.
  std::multimap<std::string, base::OnceClosure> tremplin_started_callbacks_;

  // Callbacks to run after LXD is started, keyed by vm_name. Used if StartLxd
  // completes but we need to wait for LXD to start.
  std::map<std::string, CrostiniResultCallback> start_lxd_callbacks_;

  std::map<std::string, VmInfo> running_vms_;

  // Running containers as keyed by vm name.
  std::multimap<std::string, ContainerInfo> running_containers_;

  // OsRelease protos keyed by ContainerId. We populate this map even if a
  // container fails to start normally.
  std::map<ContainerId, vm_tools::cicerone::OsRelease> container_os_releases_;
  std::set<ContainerId> container_upgrade_prompt_shown_;

  std::vector<RemoveCrostiniCallback> remove_crostini_callbacks_;

  base::ObserverList<LinuxPackageOperationProgressObserver>::Unchecked
      linux_package_operation_progress_observers_;

  base::ObserverList<PendingAppListUpdatesObserver>
      pending_app_list_updates_observers_;

  base::ObserverList<ExportContainerProgressObserver>::Unchecked
      export_container_progress_observers_;
  base::ObserverList<ImportContainerProgressObserver>::Unchecked
      import_container_progress_observers_;

  base::ObserverList<UpgradeContainerProgressObserver>::Unchecked
      upgrade_container_progress_observers_;

  base::ObserverList<ash::VmShutdownObserver> vm_shutdown_observers_;
  base::ObserverList<ash::VmStartingObserver> vm_starting_observers_;

  // Only one restarter flow is actually running for a given container, other
  // restarters will just have their callback called when the running restarter
  // completes.
  std::multimap<ContainerId, CrostiniManager::RestartId>
      restarters_by_container_;

  std::map<CrostiniManager::RestartId, std::unique_ptr<CrostiniRestarter>>
      restarters_by_id_;

  base::ObserverList<CrostiniDialogStatusObserver>
      crostini_dialog_status_observers_;
  base::ObserverList<CrostiniContainerPropertiesObserver>
      crostini_container_properties_observers_;

  base::ObserverList<ContainerStartedObserver> container_started_observers_;
  base::ObserverList<ContainerShutdownObserver> container_shutdown_observers_;

  base::ObserverList<CrostiniFileChangeObserver> file_change_observers_;

  // Contains the types of crostini dialogs currently open. It is generally
  // invalid to show more than one. e.g. uninstalling and installing are
  // mutually exclusive.
  base::flat_set<DialogType> open_crostini_dialogs_;

  bool dbus_observers_removed_ = false;

  base::Time time_of_last_disk_type_metric_;

  std::unique_ptr<guest_os::GuestOsStabilityMonitor>
      guest_os_stability_monitor_;

  std::unique_ptr<CrostiniLowDiskNotification> low_disk_notifier_;

  std::unique_ptr<CrostiniUpgradeAvailableNotification>
      upgrade_available_notification_;

  TerminaInstaller termina_installer_{};

  bool install_termina_never_completes_ = false;

  std::unique_ptr<CrostiniSshfs> crostini_sshfs_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<CrostiniManager> weak_ptr_factory_{this};
};

}  // namespace crostini

#endif  // CHROME_BROWSER_ASH_CROSTINI_CROSTINI_MANAGER_H_
