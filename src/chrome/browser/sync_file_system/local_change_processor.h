// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_LOCAL_CHANGE_PROCESSOR_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_LOCAL_CHANGE_PROCESSOR_H_

#include "base/callback_forward.h"
#include "base/macros.h"
#include "chrome/browser/sync_file_system/sync_callbacks.h"

namespace storage {
class FileSystemURL;
}

namespace sync_file_system {

class FileChange;

// Represents an interface to process one local change and applies
// it to the remote server.
// This interface is to be implemented/backed by RemoteSyncFileService.
class LocalChangeProcessor {
 public:
  LocalChangeProcessor() {}
  virtual ~LocalChangeProcessor() {}

  // This is called to apply the local |change|. If the change type is
  // ADD_OR_UPDATE for a file, |local_file_path| points to a local file
  // path that contains the latest file image whose file metadata is
  // |local_file_metadata|.
  // When SYNC_STATUS_HAS_CONFLICT is returned the implementation should
  // notify the backing RemoteFileSyncService of the existence of conflict
  // (as the remote service is supposed to maintain a list of conflict files).
  virtual void ApplyLocalChange(const FileChange& change,
                                const base::FilePath& local_file_path,
                                const SyncFileMetadata& local_file_metadata,
                                const storage::FileSystemURL& url,
                                const SyncStatusCallback& callback) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(LocalChangeProcessor);
};

}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_LOCAL_CHANGE_PROCESSOR_H_
