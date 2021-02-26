// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_HOLDING_SPACE_HOLDING_SPACE_FILE_SYSTEM_DELEGATE_H_
#define CHROME_BROWSER_UI_ASH_HOLDING_SPACE_HOLDING_SPACE_FILE_SYSTEM_DELEGATE_H_

#include <memory>

#include "base/callback.h"
#include "base/scoped_observer.h"
#include "base/timer/timer.h"
#include "chrome/browser/chromeos/file_manager/volume_manager.h"
#include "chrome/browser/chromeos/file_manager/volume_manager_observer.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_keyed_service_delegate.h"
#include "chrome/browser/ui/ash/holding_space/holding_space_util.h"

namespace base {
class FilePath;
}  // namespace base

namespace ash {

// A delegate of `HoldingSpaceKeyedService` tasked with verifying validity of
// files backing holding space items. The delegate:
// *  Finalizes partially initialized items loaded from persistent storage once
//    the validity of the backing file path was verified.
// *  Monitors the file system for removal of files backing holding space
//    items.
class HoldingSpaceFileSystemDelegate : public HoldingSpaceKeyedServiceDelegate,
                                       file_manager::VolumeManagerObserver {
 public:
  HoldingSpaceFileSystemDelegate(Profile* profile, HoldingSpaceModel* model);
  HoldingSpaceFileSystemDelegate(const HoldingSpaceFileSystemDelegate&) =
      delete;
  HoldingSpaceFileSystemDelegate& operator=(
      const HoldingSpaceFileSystemDelegate&) = delete;
  ~HoldingSpaceFileSystemDelegate() override;

 private:
  class FileSystemWatcher;

  // HoldingSpaceKeyedServiceDelegate:
  void Init() override;
  void Shutdown() override;
  void OnHoldingSpaceItemAdded(const HoldingSpaceItem* item) override;
  void OnHoldingSpaceItemRemoved(const HoldingSpaceItem* item) override;
  void OnHoldingSpaceItemFinalized(const HoldingSpaceItem* item) override;

  // file_manager::VolumeManagerObserver:
  void OnVolumeMounted(chromeos::MountError error_code,
                       const file_manager::Volume& volume) override;
  void OnVolumeUnmounted(chromeos::MountError error_code,
                         const file_manager::Volume& volume) override;

  // Invoked when the specified `file_path` has changed.
  void OnFilePathChanged(const base::FilePath& file_path, bool error);

  // Adds file path validity requirement to `pending_file_path_validity_checks_`
  // and schedules a path validity check task (if another task is not already
  // scheduled).
  void ScheduleFilePathValidityCheck(
      holding_space_util::FilePathWithValidityRequirement requirement);

  // Runs validity checks for file paths in
  // `pending_file_path_validity_checks_`. The checks are performed
  // asynchronously (with callback `OnFilePathValidityChecksComplete()`).
  void RunPendingFilePathValidityChecks();

  // Callback for a batch of file path validity checks - it updates the model
  // depending on the determined file path state.
  void OnFilePathValidityChecksComplete(
      std::vector<base::FilePath> valid_paths,
      std::vector<base::FilePath> invalid_paths);

  // Adds/removes a watch for the specified `file_path`.
  void AddWatch(const base::FilePath& file_path);
  void RemoveWatch(const base::FilePath& file_path);

  // Clears all non-finalized items from holding space model - runs with a delay
  // after profile initialization to clean up items from volumes that have not
  // been mounted during startup.
  void ClearNonFinalizedItems();

  // The `file_system_watcher_` is tasked with watching the file system for
  // changes on behalf of the delegate. It does so on a non-UI sequence. As
  // such, all communication with `file_system_watcher_` must be posted via the
  // `file_system_watcher_runner_`. In return, the `file_system_watcher_` will
  // post its responses back onto the UI thread.
  std::unique_ptr<FileSystemWatcher> file_system_watcher_;
  scoped_refptr<base::SequencedTaskRunner> file_system_watcher_runner_;

  // List of file path validity checks that need to be run.
  holding_space_util::FilePathsWithValidityRequirements
      pending_file_path_validity_checks_;

  // Whether a task to run validity checks in
  // `pending_file_path_validity_checks_` is scheduled.
  bool file_path_validity_checks_scheduled_ = false;

  // A timer to run clean-up task for items that have not been finalized within
  // a reasonable amount of time from start-up. (E.g. if the volume they belong
  // to has not been yet mounted).
  base::OneShotTimer clear_non_finalized_items_timer_;

  ScopedObserver<file_manager::VolumeManager,
                 file_manager::VolumeManagerObserver>
      volume_manager_observer_{this};

  base::WeakPtrFactory<HoldingSpaceFileSystemDelegate> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_HOLDING_SPACE_HOLDING_SPACE_FILE_SYSTEM_DELEGATE_H_
