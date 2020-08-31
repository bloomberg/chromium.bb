// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PLUGIN_VM_PLUGIN_VM_INSTALLER_H_
#define CHROME_BROWSER_CHROMEOS_PLUGIN_VM_PLUGIN_VM_INSTALLER_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_license_checker.h"
#include "chromeos/dbus/concierge/concierge_service.pb.h"
#include "chromeos/dbus/concierge_client.h"
#include "chromeos/dbus/dlcservice/dlcservice_client.h"
#include "components/download/public/background_service/download_params.h"
#include "components/keyed_service/core/keyed_service.h"

namespace download {
class DownloadService;
struct CompletionInfo;
}  // namespace download

class Profile;

namespace plugin_vm {

class PluginVmDriveImageDownloadService;

// PluginVmInstaller is responsible for installing the PluginVm image,
// including downloading this image from url specified by the user policy,
// and importing the downloaded image archive using concierge D-Bus services.
//
// This class uses one of two different objects for handling file downloads. If
// the image is hosted on Drive, a PluginVmDriveImageDownloadService object is
// used due to the need for using the Drive API. In all other cases, the
// DownloadService class is used to make the request directly.
class PluginVmInstaller : public KeyedService,
                          public chromeos::ConciergeClient::DiskImageObserver {
 public:
  // FailureReasons values are logged to UMA and shown to users. Do not change
  // or re-use enum values.
  enum class FailureReason {
    // LOGIC_ERROR = 0,
    SIGNAL_NOT_CONNECTED = 1,
    OPERATION_IN_PROGRESS = 2,
    NOT_ALLOWED = 3,
    INVALID_IMAGE_URL = 4,
    UNEXPECTED_DISK_IMAGE_STATUS = 5,
    INVALID_DISK_IMAGE_STATUS_RESPONSE = 6,
    DOWNLOAD_FAILED_UNKNOWN = 7,
    DOWNLOAD_FAILED_NETWORK = 8,
    DOWNLOAD_FAILED_ABORTED = 9,
    HASH_MISMATCH = 10,
    DISPATCHER_NOT_AVAILABLE = 11,
    CONCIERGE_NOT_AVAILABLE = 12,
    COULD_NOT_OPEN_IMAGE = 13,
    INVALID_IMPORT_RESPONSE = 14,
    IMAGE_IMPORT_FAILED = 15,
    // DLC_DOWNLOAD_FAILED = 16,
    // DLC_DOWNLOAD_NOT_STARTED = 17,
    DLC_INTERNAL = 18,
    DLC_UNSUPPORTED = 19,
    DLC_BUSY = 20,
    DLC_NEED_REBOOT = 21,
    DLC_NEED_SPACE = 22,
    INSUFFICIENT_DISK_SPACE = 23,
    INVALID_LICENSE = 24,

    kMaxValue = INVALID_LICENSE,
  };

  enum class InstallingState {
    kInactive,
    kCheckingLicense,
    kCheckingDiskSpace,
    kPausedLowDiskSpace,
    kDownloadingDlc,
    kCheckingForExistingVm,
    kDownloadingImage,
    kImporting,
  };

  static constexpr int64_t kMinimumFreeDiskSpace = 16LL * 1024 * 1024 * 1024;
  static constexpr int64_t kRecommendedFreeDiskSpace =
      32LL * 1024 * 1024 * 1024;

  // Observer class for the PluginVm image related events.
  // TODO(timloh): Merge OnFooFailed functions as the failure reason is enough
  // to distinguish where we failed.
  class Observer {
   public:
    virtual ~Observer() = default;

    virtual void OnProgressUpdated(double fraction_complete) = 0;

    virtual void OnLicenseChecked() = 0;

    // If |low_disk_space| is true, the device doesn't have the recommended
    // amount of free disk space and the install will pause until Continue() or
    // Cancel() is called.
    virtual void OnCheckedDiskSpace(bool low_disk_space) = 0;

    virtual void OnDlcDownloadCompleted() = 0;

    // If |has_vm| is true, the install is done.
    virtual void OnExistingVmCheckCompleted(bool has_vm) = 0;

    virtual void OnDownloadProgressUpdated(uint64_t bytes_downloaded,
                                           int64_t content_length) = 0;
    virtual void OnDownloadCompleted() = 0;
    virtual void OnCreated() = 0;
    virtual void OnImported() = 0;
    virtual void OnError(FailureReason reason) = 0;

    virtual void OnCancelFinished() = 0;
  };

  explicit PluginVmInstaller(Profile* profile);

  // Returns true if installer is processing a PluginVm image at the moment.
  bool IsProcessing();

  // Start the installation. Progress updates will be sent to the observer.
  void Start();
  // Continue the installation if it was paused due to low disk space.
  void Continue();
  // Cancel the installation.
  void Cancel();

  void SetObserver(Observer* observer);
  void RemoveObserver();

  // Called by DlcserviceClient, are not supposed to be used by other classes.
  void OnDlcDownloadProgressUpdated(double progress);
  void OnDlcDownloadCompleted(
      const chromeos::DlcserviceClient::InstallResult& install_result);

  // Called by PluginVmImageDownloadClient, are not supposed to be used by other
  // classes.
  void OnDownloadStarted();
  void OnDownloadProgressUpdated(uint64_t bytes_downloaded,
                                 int64_t content_length);
  void OnDownloadCompleted(const download::CompletionInfo& info);
  void OnDownloadCancelled();
  void OnDownloadFailed(FailureReason reason);

  // ConciergeClient::DiskImageObserver:
  void OnDiskImageProgress(
      const vm_tools::concierge::DiskImageStatusResponse& signal) override;

  // Helper function that returns true in case downloaded PluginVm image
  // archive passes hash verification and false otherwise.
  // Public for testing purposes.
  bool VerifyDownload(const std::string& downloaded_archive_hash);

  void SetFreeDiskSpaceForTesting(int64_t bytes) {
    free_disk_space_for_testing_ = bytes;
  }
  void SetDownloadServiceForTesting(
      download::DownloadService* download_service);
  void SetDownloadedImageForTesting(const base::FilePath& downloaded_image);
  void SetDriveDownloadServiceForTesting(
      std::unique_ptr<PluginVmDriveImageDownloadService>
          drive_download_service);
  std::string GetCurrentDownloadGuidForTesting();

 private:
  void CheckLicense();
  void OnLicenseChecked(bool license_is_valid);
  void CheckDiskSpace();
  void OnAvailableDiskSpace(int64_t bytes);
  void StartDlcDownload();
  void OnUpdateVmState(bool default_vm_exists);
  void OnUpdateVmStateFailed();
  void StartDownload();
  void DetectImageType();
  void StartImport();

  void UpdateProgress(double state_progress);

  // Cancels the download of PluginVm image finishing the image processing.
  // Downloaded PluginVm image archive is being deleted.
  void CancelDownload();
  // Makes a call to concierge to cancel the import.
  void CancelImport();
  // Reset state and call observers.
  void CancelFinished();

  void InstallFailed(FailureReason reason);
  // Reset state, callers also need to call the appropriate observer functions.
  void InstallFinished();

  enum class State {
    kIdle,
    kInstalling,
    kCancelling,
  };

  Profile* profile_ = nullptr;
  Observer* observer_ = nullptr;
  download::DownloadService* download_service_ = nullptr;
  State state_ = State::kIdle;
  InstallingState installing_state_ = InstallingState::kInactive;
  std::string current_download_guid_;
  base::FilePath downloaded_image_;
  // Used to identify our running import with concierge:
  std::string current_import_command_uuid_;
  // -1 when is not yet determined.
  int64_t downloaded_image_size_ = -1;
  bool creating_new_vm_ = false;
  double progress_ = 0;
  std::unique_ptr<PluginVmDriveImageDownloadService> drive_download_service_;
  std::unique_ptr<PluginVmLicenseChecker> license_checker_;
  bool using_drive_download_service_ = false;

  // -1 indicates not set
  int64_t free_disk_space_for_testing_ = -1;

  ~PluginVmInstaller() override;

  // Get string representation of state for logging purposes.
  static std::string GetStateName(State state);
  static std::string GetInstallingStateName(InstallingState state);

  GURL GetPluginVmImageDownloadUrl();
  download::DownloadParams GetDownloadParams(const GURL& url);

  void OnStartDownload(const std::string& download_guid,
                       download::DownloadParams::StartResult start_result);

  // Callback when image type has been detected. This will make call to
  // concierge's ImportDiskImage.
  void OnImageTypeDetected();

  // Callback which is called once we know if concierge is available.
  void OnConciergeAvailable(bool success);

  // Ran as a blocking task preparing the FD for the ImportDiskImage call.
  base::Optional<base::ScopedFD> PrepareFD();

  // Callback when the FD is prepared. Makes the call to CreateDiskImage or
  // ImportDiskImage, depending on whether we are trying to create a new VM
  // from an ISO, or import prepared VM image.
  void OnFDPrepared(base::Optional<base::ScopedFD> maybeFd);

  // Callback for the concierge CreateDiskImage/ImportDiskImage calls.
  template <typename ReplyType>
  void OnImportDiskImage(base::Optional<ReplyType> reply);

  // After we get a signal that the import is finished successfully, we
  // make one final call to concierge's DiskImageStatus method to get a
  // final resolution.
  void RequestFinalStatus();

  // Callback for the final call to concierge's DiskImageStatus to
  // get the final result of the disk import operation. This moves
  // the installer to a finishing state, depending on the result of the
  // query. Called when the signal for the command indicates that we
  // are done with importing.
  void OnFinalDiskImageStatus(
      base::Optional<vm_tools::concierge::DiskImageStatusResponse> reply);

  // Finishes the processing of PluginVm image. If |failure_reason| has a value,
  // then the import has failed, otherwise it was successful.
  void OnImported(base::Optional<FailureReason> failure_reason);

  // Callback for the concierge CancelDiskImageOperation call.
  void OnImportDiskImageCancelled(
      base::Optional<vm_tools::concierge::CancelDiskImageResponse> reply);

  void RemoveTemporaryImageIfExists();
  void OnTemporaryImageRemoved(bool success);

  base::WeakPtrFactory<PluginVmInstaller> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PluginVmInstaller);
};

}  // namespace plugin_vm

#endif  // CHROME_BROWSER_CHROMEOS_PLUGIN_VM_PLUGIN_VM_INSTALLER_H_
