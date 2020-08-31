// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_EXPORT_IMPORT_H_
#define CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_EXPORT_IMPORT_H_

#include <map>
#include <memory>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/crostini/crostini_export_import_notification_controller.h"
#include "chrome/browser/chromeos/crostini/crostini_manager.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace content {
class WebContents;
}

namespace crostini {

enum class ExportContainerResult;
enum class ImportContainerResult;

enum class ExportImportType { EXPORT, IMPORT };

// ExportContainerResult and ImportContainerResult are used for UMA.  Adding new
// fields is OK, but do not delete or renumber.
enum class ExportContainerResult {
  kSuccess = 0,
  kFailed = 1,
  kFailedVmStopped = 2,
  kFailedVmStarted = 3,
  kMaxValue = kFailedVmStarted,
};

enum class ImportContainerResult {
  kSuccess = 0,
  kFailed = 1,
  kFailedVmStopped = 2,
  kFailedVmStarted = 3,
  kFailedArchitecture = 4,
  kFailedSpace = 5,
  kMaxValue = kFailedSpace,
};

// CrostiniExportImport is a keyed profile service to manage exporting and
// importing containers with crostini.  It manages a file dialog for selecting
// files and a notification to show the progress of export/import.
//
// TODO(crbug.com/932339): Ensure we have enough free space before doing
// backup or restore.
class CrostiniExportImport : public KeyedService,
                             public ui::SelectFileDialog::Listener,
                             public crostini::ExportContainerProgressObserver,
                             public crostini::ImportContainerProgressObserver {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Called immediately before operation begins with |in_progress|=true, and
    // again immediately after the operation completes with |in_progress|=false.
    virtual void OnCrostiniExportImportOperationStatusChanged(
        bool in_progress) = 0;
  };

  using OnceTrackerFactory = base::OnceCallback<std::unique_ptr<
      CrostiniExportImportStatusTracker>(ExportImportType, base::FilePath)>;

  struct OperationData {
    OperationData(ExportImportType type,
                  ContainerId id,
                  OnceTrackerFactory factory);
    ~OperationData();

    ExportImportType type;
    ContainerId container_id;
    OnceTrackerFactory tracker_factory;
  };

  static CrostiniExportImport* GetForProfile(Profile* profile);

  explicit CrostiniExportImport(Profile* profile);
  ~CrostiniExportImport() override;

  void AddObserver(Observer* observer) { observers_.AddObserver(observer); }

  void RemoveObserver(Observer* observer) {
    observers_.RemoveObserver(observer);
  }

  // KeyedService:
  void Shutdown() override;

  // Export the crostini container showing FileDialog.
  void ExportContainer(content::WebContents* web_contents);
  // Import the crostini container showing FileDialog.
  void ImportContainer(content::WebContents* web_contents);

  // Export |container_id| to |path| and invoke |callback| when complete.
  void ExportContainer(ContainerId container_id,
                       base::FilePath path,
                       CrostiniManager::CrostiniResultCallback callback);
  // Import |container_id| from |path| and invoke |callback| when complete.
  void ImportContainer(ContainerId container_id,
                       base::FilePath path,
                       CrostiniManager::CrostiniResultCallback callback);

  // Export |container_id| showing FileDialog, and using |tracker_factory| for
  // status tracking.
  void ExportContainer(ContainerId container_id,
                       content::WebContents* web_contents,
                       OnceTrackerFactory tracker_factory);
  // Import |container_id| showing FileDialog, and using |tracker_factory| for
  // status tracking.
  void ImportContainer(ContainerId container_id,
                       content::WebContents* web_contents,
                       OnceTrackerFactory tracker_factory);

  // Export |container| to |path| and invoke |tracker_factory| to create a
  // tracker for this operation.
  void ExportContainer(ContainerId container_id,
                       base::FilePath path,
                       OnceTrackerFactory tracker_factory);
  // Import |container| from |path| and invoke |tracker_factory| to create a
  // tracker for this operation.
  void ImportContainer(ContainerId container_id,
                       base::FilePath path,
                       OnceTrackerFactory tracker_factory);

  // Cancel currently running export/import.
  void CancelOperation(ExportImportType type, ContainerId id);

  // Whether an export or import is currently in progress.
  bool GetExportImportOperationStatus() const;

  // Returns the default location to export the container to.
  base::FilePath GetDefaultBackupPath() const;

  base::WeakPtr<CrostiniExportImportNotificationController>
  GetNotificationControllerForTesting(ContainerId container_id);

 private:
  FRIEND_TEST_ALL_PREFIXES(CrostiniExportImportTest,
                           TestDeprecatedExportSuccess);
  FRIEND_TEST_ALL_PREFIXES(CrostiniExportImportTest, TestExportSuccess);
  FRIEND_TEST_ALL_PREFIXES(CrostiniExportImportTest, TestExportFail);
  FRIEND_TEST_ALL_PREFIXES(CrostiniExportImportTest, TestExportCancelled);
  FRIEND_TEST_ALL_PREFIXES(CrostiniExportImportTest,
                           TestExportDoneBeforeCancelled);
  FRIEND_TEST_ALL_PREFIXES(CrostiniExportImportTest, TestImportSuccess);
  FRIEND_TEST_ALL_PREFIXES(CrostiniExportImportTest, TestImportFail);
  FRIEND_TEST_ALL_PREFIXES(CrostiniExportImportTest, TestImportCancelled);
  FRIEND_TEST_ALL_PREFIXES(CrostiniExportImportTest,
                           TestImportDoneBeforeCancelled);
  FRIEND_TEST_ALL_PREFIXES(CrostiniExportImportTest,
                           TestImportFailArchitecture);
  FRIEND_TEST_ALL_PREFIXES(CrostiniExportImportTest, TestImportFailSpace);

  OperationData* NewOperationData(ExportImportType type,
                                  ContainerId id,
                                  OnceTrackerFactory cb);
  OperationData* NewOperationData(ExportImportType type, ContainerId id);
  OperationData* NewOperationData(ExportImportType type);

  // ui::SelectFileDialog::Listener implementation.
  void FileSelected(const base::FilePath& path,
                    int index,
                    void* params) override;
  void FileSelectionCanceled(void* params) override;

  void Start(OperationData* params,
             base::FilePath path,
             CrostiniManager::CrostiniResultCallback callback);

  // DEPRECATED crostini::ExportContainerProgressObserver implementation.
  // TODO(juwa): delete this once the new version of tremplin has shipped.
  void OnExportContainerProgress(const ContainerId& container_id,
                                 crostini::ExportContainerProgressStatus status,
                                 int progress_percent,
                                 uint64_t progress_speed) override;

  // crostini::ExportContainerProgressObserver implementation.
  void OnExportContainerProgress(const ContainerId& container_id,
                                 const StreamingExportStatus& status) override;

  // crostini::ImportContainerProgressObserver implementation.
  void OnImportContainerProgress(const ContainerId& container_id,
                                 crostini::ImportContainerProgressStatus status,
                                 int progress_percent,
                                 uint64_t progress_speed,
                                 const std::string& architecture_device,
                                 const std::string& architecture_container,
                                 uint64_t available_space,
                                 uint64_t minimum_required_space) override;

  void ExportAfterSharing(const ContainerId& container_id,
                          CrostiniManager::CrostiniResultCallback callback,
                          const base::FilePath& container_path,
                          bool result,
                          const std::string& failure_reason);
  void OnExportComplete(const base::Time& start,
                        const ContainerId& container_id,
                        CrostiniManager::CrostiniResultCallback callback,
                        CrostiniResult result,
                        uint64_t container_size,
                        uint64_t compressed_size);

  void ImportAfterSharing(const ContainerId& container_id,
                          CrostiniManager::CrostiniResultCallback callback,
                          const base::FilePath& container_path,
                          bool result,
                          const std::string& failure_reason);
  void OnImportComplete(const base::Time& start,
                        const ContainerId& container_id,
                        CrostiniManager::CrostiniResultCallback callback,
                        CrostiniResult result);

  void OpenFileDialog(OperationData* params,
                      content::WebContents* web_contents);

  std::string GetUniqueNotificationId();

  using TrackerMap =
      std::map<ContainerId, std::unique_ptr<CrostiniExportImportStatusTracker>>;

  std::unique_ptr<CrostiniExportImportStatusTracker> RemoveTracker(
      TrackerMap::iterator it);

  Profile* profile_;
  scoped_refptr<ui::SelectFileDialog> select_folder_dialog_;
  TrackerMap status_trackers_;
  // |operation_data_storage_| persists the data required to complete an
  // operation while the file selection dialog is open/operation is in progress.
  std::unordered_map<OperationData*, std::unique_ptr<OperationData>>
      operation_data_storage_;
  // Trackers must have unique-per-profile identifiers.
  // A non-static member on a profile-keyed-service will suffice.
  int next_status_tracker_id_;
  base::ObserverList<Observer> observers_;
  // weak_ptr_factory_ should always be last member.
  base::WeakPtrFactory<CrostiniExportImport> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CrostiniExportImport);
};

}  // namespace crostini

#endif  // CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_EXPORT_IMPORT_H_
