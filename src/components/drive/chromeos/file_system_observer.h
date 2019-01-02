// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DRIVE_CHROMEOS_FILE_SYSTEM_OBSERVER_H_
#define COMPONENTS_DRIVE_CHROMEOS_FILE_SYSTEM_OBSERVER_H_

#include <set>
#include <string>

#include "components/drive/chromeos/file_system/operation_delegate.h"
#include "components/drive/file_errors.h"

namespace base {
class FilePath;
}

namespace drive {

class FileChange;

// Interface for classes that need to observe events from classes implementing
// FileSystemInterface.
// All events are notified on UI thread.
class FileSystemObserver {
 public:
  // Triggered when a content of a directory has been changed.
  // |directory_path| is a virtual directory path (/drive/...) representing
  // changed directory.
  virtual void OnDirectoryChanged(const base::FilePath& directory_path) {}
  virtual void OnFileChanged(const FileChange& file_change) {}

  // Triggered when a specific drive error occurred.
  // |type| is a type of the error. |file_name| is a virtual path of the entry.
  virtual void OnDriveSyncError(file_system::DriveSyncErrorType type,
                                const base::FilePath& file_path) {}

  // Triggered when the list of team drives that the user has access to
  // changes. On first load all team drives will be passed via
  // |added_team_drive_ids|, subsequent calls will just be the delta additions
  // and removals.
  virtual void OnTeamDrivesUpdated(
      const std::set<std::string>& added_team_drive_ids,
      const std::set<std::string>& removed_team_drive_ids) {}

 protected:
  virtual ~FileSystemObserver() = default;
};

}  // namespace drive

#endif  // COMPONENTS_DRIVE_CHROMEOS_FILE_SYSTEM_OBSERVER_H_
