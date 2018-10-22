// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_MANAGER_H_

#include <map>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "chrome/browser/component_updater/cros_component_installer_chromeos.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/extensions/app_launch_params.h"
#include "chromeos/dbus/cicerone/cicerone_service.pb.h"
#include "chromeos/dbus/cicerone_client.h"
#include "chromeos/dbus/concierge/service.pb.h"
#include "chromeos/dbus/concierge_client.h"

class Profile;

namespace crostini {

// Result types for CrostiniManager::StartTerminaVmCallback etc.
enum class ConciergeClientResult {
  SUCCESS,
  DBUS_ERROR,
  UNPARSEABLE_RESPONSE,
  CREATE_DISK_IMAGE_FAILED,
  VM_START_FAILED,
  VM_STOP_FAILED,
  DESTROY_DISK_IMAGE_FAILED,
  LIST_VM_DISKS_FAILED,
  CLIENT_ERROR,
  DISK_TYPE_ERROR,
  CONTAINER_START_FAILED,
  LAUNCH_CONTAINER_APPLICATION_FAILED,
  INSTALL_LINUX_PACKAGE_FAILED,
  INSTALL_LINUX_PACKAGE_ALREADY_ACTIVE,
  SSHFS_MOUNT_ERROR,
  OFFLINE_WHEN_UPGRADE_REQUIRED,
  LOAD_COMPONENT_FAILED,
  UNKNOWN_ERROR,
};

enum class InstallLinuxPackageProgressStatus {
  SUCCEEDED,
  FAILED,
  DOWNLOADING,
  INSTALLING,
};

// Return type when getting app icons from within a container.
struct Icon {
  std::string desktop_file_id;

  // Icon file content in PNG format.
  std::string content;
};

class InstallLinuxPackageProgressObserver {
 public:
  // A successfully started package install will continually fire progress
  // events until it returns a status of SUCCEEDED or FAILED. The
  // |progress_percent| field is given as a percentage of the given step,
  // DOWNLOADING or INSTALLING. |failure_reason| is returned from the container
  // for a FAILED case, and not necessarily localized.
  virtual void OnInstallLinuxPackageProgress(
      const std::string& vm_name,
      const std::string& container_name,
      InstallLinuxPackageProgressStatus status,
      int progress_percent,
      const std::string& failure_reason) = 0;
};

// CrostiniManager is a singleton which is used to check arguments for
// ConciergeClient and CiceroneClient. ConciergeClient is dedicated to
// communication with the Concierge service, CiceroneClient is dedicated to
// communication with the Cicerone service and both should remain as thin as
// possible. The existence of Cicerone is abstracted behind this class and
// only the Concierge name is exposed outside of here.
class CrostiniManager : public chromeos::ConciergeClient::Observer,
                        public chromeos::CiceroneClient::Observer {
 public:
  using ConciergeClientCallback =
      base::OnceCallback<void(ConciergeClientResult result)>;
  using BoolCallback = base::OnceCallback<void(bool)>;
  using CrostiniResultCallback = ConciergeClientCallback;

  // The type of the callback for CrostiniManager::StartConcierge.
  using StartConciergeCallback = BoolCallback;
  // The type of the callback for CrostiniManager::StopConcierge.
  using StopConciergeCallback = BoolCallback;
  // The type of the callback for CrostiniManager::StartTerminaVm.
  using StartTerminaVmCallback = ConciergeClientCallback;
  // The type of the callback for CrostiniManager::CreateDiskImage.
  using CreateDiskImageCallback =
      base::OnceCallback<void(ConciergeClientResult result,
                              const base::FilePath& disk_path)>;
  // The type of the callback for CrostiniManager::DestroyDiskImage.
  using DestroyDiskImageCallback = ConciergeClientCallback;
  // The type of the callback for CrostiniManager::ListVmDisks.
  using ListVmDisksCallback =
      base::OnceCallback<void(ConciergeClientResult result,
                              int64_t total_size)>;
  // The type of the callback for CrostiniManager::StopVm.
  using StopVmCallback = ConciergeClientCallback;
  // The type of the callback for CrostiniManager::StartContainer.
  using StartContainerCallback = ConciergeClientCallback;
  // The type of the callback for CrostiniManager::ShutdownContainer.
  using ShutdownContainerCallback = base::OnceClosure;
  // The type of the callback for CrostiniManager::LaunchContainerApplication.
  using LaunchContainerApplicationCallback = ConciergeClientCallback;
  // The type of the callback for CrostiniManager::GetContainerAppIcons.
  using GetContainerAppIconsCallback =
      base::OnceCallback<void(ConciergeClientResult result,
                              const std::vector<Icon>& icons)>;
  // The type of the callback for CrostiniManager::InstallLinuxPackage.
  // |failure_reason| is returned from the container upon failure
  // (INSTALL_LINUX_PACKAGE_FAILED), and not necessarily localized.
  using InstallLinuxPackageCallback =
      base::OnceCallback<void(ConciergeClientResult result,
                              const std::string& failure_reason)>;
  // The type of the callback for CrostiniManager::GetContainerSshKeys.
  using GetContainerSshKeysCallback =
      base::OnceCallback<void(ConciergeClientResult result,
                              const std::string& container_public_key,
                              const std::string& host_private_key,
                              const std::string& hostname)>;
  // The type of the callback for CrostiniManager::RestartCrostini.
  using RestartCrostiniCallback = ConciergeClientCallback;
  // The type of the callback for CrostiniManager::RemoveCrostini.
  using RemoveCrostiniCallback = ConciergeClientCallback;

  // Observer class for the Crostini restart flow.
  class RestartObserver {
   public:
    virtual ~RestartObserver() {}
    virtual void OnComponentLoaded(ConciergeClientResult result) = 0;
    virtual void OnConciergeStarted(ConciergeClientResult result) = 0;
    virtual void OnDiskImageCreated(ConciergeClientResult result) = 0;
    virtual void OnVmStarted(ConciergeClientResult result) = 0;
    virtual void OnContainerStarted(ConciergeClientResult result) = 0;
    virtual void OnSshKeysFetched(ConciergeClientResult result) = 0;
  };

  // Checks if the cros-termina component is installed.
  bool IsCrosTerminaInstalled() const;

  // Generate the URL for Crostini terminal application.
  static GURL GenerateVshInCroshUrl(
      Profile* profile,
      const std::string& vm_name,
      const std::string& container_name,
      const std::vector<std::string>& terminal_args);

  // Generate AppLaunchParams for the Crostini terminal application.
  static AppLaunchParams GenerateTerminalAppLaunchParams(Profile* profile);

  // Upgrades cros-termina component if the current version is not compatible.
  void MaybeUpgradeCrostini(Profile* profile);

  // Installs the current version of cros-termina component. Attempts to apply
  // pending upgrades if a MaybeUpgradeCrostini failed.
  void InstallTerminaComponent(CrostiniResultCallback callback);

  // Starts the Concierge service. |callback| is called after the method call
  // finishes.
  void StartConcierge(StartConciergeCallback callback);

  // Stops the Concierge service. |callback| is called after the method call
  // finishes.
  void StopConcierge(StopConciergeCallback callback);

  // Checks the arguments for creating a new Termina VM disk image. Creates a
  // disk image for a Termina VM via ConciergeClient::CreateDiskImage.
  // |callback| is called if the arguments are bad, or after the method call
  // finishes.
  void CreateDiskImage(
      // The cryptohome id for the user's encrypted storage.
      const std::string& cryptohome_id,
      // The path to the disk image, including the name of
      // the image itself. The image name should match the
      // name of the VM that it will be used for.
      const base::FilePath& disk_path,
      // The storage location for the disk image
      vm_tools::concierge::StorageLocation storage_location,
      CreateDiskImageCallback callback);

  // Checks the arguments for destroying a named Termina VM disk image.
  // Removes the named Termina VM via ConciergeClient::DestroyDiskImage.
  // |callback| is called if the arguments are bad, or after the method call
  // finishes.
  void DestroyDiskImage(
      // The cryptohome id for the user's encrypted storage.
      const std::string& cryptohome_id,
      // The path to the disk image, including the name of
      // the image itself.
      const base::FilePath& disk_path,
      // The storage location of the disk image
      vm_tools::concierge::StorageLocation storage_location,
      DestroyDiskImageCallback callback);

  void ListVmDisks(
      // The cryptohome id for the user's encrypted storage.
      const std::string& cryptohome_id,
      ListVmDisksCallback callback);

  // Checks the arguments for starting a Termina VM. Starts a Termina VM via
  // ConciergeClient::StartTerminaVm. |callback| is called if the arguments
  // are bad, or after the method call finishes.
  void StartTerminaVm(std::string owner_id,
                      // The human-readable name to be assigned to this VM.
                      std::string name,
                      // Path to the disk image on the host.
                      const base::FilePath& disk_path,
                      StartTerminaVmCallback callback);

  // Checks the arguments for stopping a Termina VM. Stops the Termina VM via
  // ConciergeClient::StopVm. |callback| is called if the arguments are bad,
  // or after the method call finishes.
  void StopVm(Profile* profile, std::string name, StopVmCallback callback);

  // Checks the arguments for starting a Container inside an existing Termina
  // VM. Starts the container via ConciergeClient::StartContainer. |callback|
  // is called if the arguments are bad, or after the method call finishes.
  void StartContainer(std::string vm_name,
                      std::string container_name,
                      std::string container_username,
                      std::string cryptohome_id,
                      StartContainerCallback callback);

  // Asynchronously launches an app as specified by its desktop file id.
  // |callback| is called with SUCCESS when the relevant process is started
  // or LAUNCH_CONTAINER_APPLICATION_FAILED if there was an error somewhere.
  void LaunchContainerApplication(Profile* profile,
                                  std::string vm_name,
                                  std::string container_name,
                                  std::string desktop_file_id,
                                  const std::vector<std::string>& files,
                                  LaunchContainerApplicationCallback callback);

  // Asynchronously gets app icons as specified by their desktop file ids.
  // |callback| is called after the method call finishes.
  void GetContainerAppIcons(Profile* profile,
                            std::string vm_name,
                            std::string container_name,
                            std::vector<std::string> desktop_file_ids,
                            int icon_size,
                            int scale,
                            GetContainerAppIconsCallback callback);

  // Begin installation of a Linux Package inside the container. If the
  // installation is successfully started, further updates will be sent to
  // added InstallLinuxPackageProgressObservers.
  void InstallLinuxPackage(Profile* profile,
                           std::string vm_name,
                           std::string container_name,
                           std::string package_path,
                           InstallLinuxPackageCallback callback);

  // Asynchronously gets SSH server public key of container and trusted SSH
  // client private key which can be used to connect to the container.
  // |callback| is called after the method call finishes.
  void GetContainerSshKeys(std::string vm_name,
                           std::string container_name,
                           std::string cryptohome_id,
                           GetContainerSshKeysCallback callback);

  // Create the crosh-in-a-window that displays a shell in an container on a VM.
  Browser* CreateContainerTerminal(const AppLaunchParams& launch_params,
                                   const GURL& vsh_in_crosh_url);

  // Shows the already created crosh-in-a-window that displays a shell in an
  // already running container on a VM.
  void ShowContainerTerminal(const AppLaunchParams& launch_params,
                             const GURL& vsh_in_crosh_url,
                             Browser* browser);

  // Launches the crosh-in-a-window that displays a shell in an already running
  // container on a VM and passes |terminal_args| as parameters to that shell
  // which will cause them to be executed as program inside that shell.
  void LaunchContainerTerminal(Profile* profile,
                               const std::string& vm_name,
                               const std::string& container_name,
                               const std::vector<std::string>& terminal_args);

  using RestartId = int;
  static const RestartId kUninitializedRestartId = -1;
  // Runs all the steps required to restart the given crostini vm and container.
  // The optional |observer| tracks progress.
  RestartId RestartCrostini(Profile* profile,
                            std::string vm_name,
                            std::string container_name,
                            RestartCrostiniCallback callback,
                            RestartObserver* observer = nullptr);

  // Aborts a restart. A "next" restarter with the same <vm_name,
  // container_name> will run, if there is one.
  void AbortRestartCrostini(Profile* profile, RestartId restart_id);

  // Returns true if the Restart corresponding to |restart_id| is not yet
  // complete.
  bool IsRestartPending(Profile* profile, RestartId restart_id);

  // Adds a callback to receive notification of container shutdown.
  void AddShutdownContainerCallback(
      Profile* profile,
      std::string vm_name,
      std::string container_name,
      ShutdownContainerCallback shutdown_callback);

  // Add/remove observers for package install progress.
  void AddInstallLinuxPackageProgressObserver(
      Profile* profile,
      InstallLinuxPackageProgressObserver* observer);
  void RemoveInstallLinuxPackageProgressObserver(
      Profile* profile,
      InstallLinuxPackageProgressObserver* observer);

  // ConciergeClient::Observer:
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
  void OnLxdContainerCreated(
      const vm_tools::cicerone::LxdContainerCreatedSignal& signal) override;
  void OnLxdContainerDownloading(
      const vm_tools::cicerone::LxdContainerDownloadingSignal& signal) override;
  void OnTremplinStarted(
      const vm_tools::cicerone::TremplinStartedSignal& signal) override;

  void RemoveCrostini(Profile* profile,
                      std::string vm_name,
                      std::string container_name,
                      RemoveCrostiniCallback callback);

  // Returns the singleton instance of CrostiniManager.
  static CrostiniManager* GetInstance();

  bool IsVmRunning(Profile* profile, std::string vm_name);
  // Returns null if VM is not running.
  base::Optional<vm_tools::concierge::VmInfo> GetVmInfo(std::string vm_name);
  void AddRunningVmForTesting(std::string vm_name,
                              vm_tools::concierge::VmInfo vm_info);
  bool IsContainerRunning(Profile* profile,
                          std::string vm_name,
                          std::string container_name);

  // Clear the lists of running VMs and containers.
  // Can be called for testing to skip restart.
  void set_skip_restart_for_testing() { skip_restart_for_testing_ = true; }
  bool skip_restart_for_testing() { return skip_restart_for_testing_; }
  void set_component_manager_load_error_for_testing(
      component_updater::CrOSComponentManager::Error error) {
    component_manager_load_error_for_testing_ = error;
  }

  // If the Crostini reporting policy is set, save the last app launch
  // time window and the Termina version in prefs for asynchronous reporting.
  void UpdateLaunchMetricsForEnterpriseReporting(Profile* profile);

 private:
  friend struct base::DefaultSingletonTraits<CrostiniManager>;

  CrostiniManager();
  ~CrostiniManager() override;

  // Callback for ConciergeClient::CreateDiskImage. Called after the Concierge
  // service method finishes.
  void OnCreateDiskImage(
      CreateDiskImageCallback callback,
      base::Optional<vm_tools::concierge::CreateDiskImageResponse> reply);

  // Callback for ConciergeClient::DestroyDiskImage. Called after the Concierge
  // service method finishes.
  void OnDestroyDiskImage(
      DestroyDiskImageCallback callback,
      base::Optional<vm_tools::concierge::DestroyDiskImageResponse> reply);

  // Callback for ConciergeClient::ListVmDisks. Called after the Concierge
  // service method finishes.
  void OnListVmDisks(
      ListVmDisksCallback callback,
      base::Optional<vm_tools::concierge::ListVmDisksResponse> reply);

  // Callback for ConciergeClient::StartTerminaVm. Called after the Concierge
  // service method finishes.
  void OnStartTerminaVm(
      std::string owner_id,
      std::string vm_name,
      StartTerminaVmCallback callback,
      base::Optional<vm_tools::concierge::StartVmResponse> reply);

  // Callback for ConciergeClient::StopVm. Called after the Concierge
  // service method finishes.
  void OnStopVm(std::string owner_id,
                std::string vm_name,
                StopVmCallback callback,
                base::Optional<vm_tools::concierge::StopVmResponse> reply);

  // Callback for CrostiniManager::InstallCrostiniComponent. Must be called on
  // the UI thread.
  void OnInstallTerminaComponent(
      CrostiniResultCallback callback,
      bool is_update_checked,
      component_updater::CrOSComponentManager::Error error,
      const base::FilePath& result);

  // Callback for CrostiniClient::StartConcierge. Called after the
  // DebugDaemon service method finishes.
  void OnStartConcierge(StartConciergeCallback callback, bool success);

  // Callback for CrostiniClient::StopConcierge. Called after the
  // DebugDaemon service method finishes.
  void OnStopConcierge(StopConciergeCallback callback, bool success);

  // Callback for CrostiniManager::StartContainer. Called after the Concierge
  // service finishes.
  void OnStartContainer(
      std::string owner_id,
      std::string vm_name,
      std::string container_name,
      StartContainerCallback callback,
      base::Optional<vm_tools::concierge::StartContainerResponse> reply);

  // Callback for CrostiniManager::LaunchContainerApplication.
  void OnLaunchContainerApplication(
      LaunchContainerApplicationCallback callback,
      base::Optional<vm_tools::cicerone::LaunchContainerApplicationResponse>
          reply);

  // Callback for CrostiniManager::GetContainerAppIcons. Called after the
  // Concierge service finishes.
  void OnGetContainerAppIcons(
      GetContainerAppIconsCallback callback,
      base::Optional<vm_tools::cicerone::ContainerAppIconResponse> reply);

  // Callback for CrostiniManager::InstallLinuxPackage.
  void OnInstallLinuxPackage(
      InstallLinuxPackageCallback callback,
      base::Optional<vm_tools::cicerone::InstallLinuxPackageResponse> reply);

  // Callback for CrostiniManager::GetContainerSshKeys. Called after the
  // Concierge service finishes.
  void OnGetContainerSshKeys(
      GetContainerSshKeysCallback callback,
      base::Optional<vm_tools::concierge::ContainerSshKeysResponse> reply);

  // Helper for CrostiniManager::MaybeUpgradeCrostini. Separated because the
  // checking registration code may block.
  void MaybeUpgradeCrostiniAfterTerminaCheck(bool is_registered);

  // Helper for CrostiniManager::CreateDiskImage. Separated so it can be run
  // off the main thread.
  void CreateDiskImageAfterSizeCheck(
      vm_tools::concierge::CreateDiskImageRequest request,
      CreateDiskImageCallback callback,
      int64_t free_disk_size);

  bool skip_restart_for_testing_ = false;
  component_updater::CrOSComponentManager::Error
      component_manager_load_error_for_testing_ =
          component_updater::CrOSComponentManager::Error::NONE;

  bool is_cros_termina_registered_ = false;
  bool termina_update_check_needed_ = false;

  // Pending StartContainer callbacks are keyed by <owner_id, vm_name,
  // container_name> string tuples.
  std::multimap<std::tuple<std::string, std::string, std::string>,
                StartContainerCallback>
      start_container_callbacks_;

  // Pending ShutdownContainer callbacks are keyed by <owner_id, vm_name,
  // container_name> string tuples.
  std::multimap<std::tuple<std::string, std::string, std::string>,
                ShutdownContainerCallback>
      shutdown_container_callbacks_;

  // Running vms as <owner_id, vm_name> pairs.
  std::set<std::pair<std::string, std::string>> running_vms_;

  // Running containers as keyed by <owner_id, vm_name> string pairs.
  std::multimap<std::pair<std::string, std::string>, std::string>
      running_containers_;

  // Keyed by owner_id.
  std::multimap<std::string, InstallLinuxPackageProgressObserver*>
      install_linux_package_progress_observers_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<CrostiniManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CrostiniManager);
};

}  // namespace crostini

#endif  // CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_MANAGER_H_
