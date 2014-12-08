// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_COMMON_FILEAPI_FILE_SYSTEM_MOUNT_OPTION_H_
#define STORAGE_COMMON_FILEAPI_FILE_SYSTEM_MOUNT_OPTION_H_

namespace storage {

// Option for specifying if disk sync operation is wanted after copying.
enum CopySyncOption {
  // No syncing is required after an operation is completed.
  COPY_SYNC_OPTION_NO_SYNC,

  // Syncing is required in order to commit written data. Note, that syncing
  // is only invoked via FileStreamWriter::Flush() and via base::File::Flush()
  // for native files. Hence, syncing will not be performed for copying within
  // non-native file systems as well as for non-native copies performed with
  // snapshots.
  COPY_SYNC_OPTION_SYNC,
};

// Conveys options for a mounted file systems.
class FileSystemMountOption {
 public:
  // Constructs with the default options.
  FileSystemMountOption()
      : copy_sync_option_(COPY_SYNC_OPTION_NO_SYNC) {}

  // Constructs with the specified component.
  explicit FileSystemMountOption(CopySyncOption copy_sync_option)
      : copy_sync_option_(copy_sync_option) {}

  CopySyncOption copy_sync_option() const {
    return copy_sync_option_;
  }

 private:
  CopySyncOption copy_sync_option_;
};

}  // namespace storage

#endif  // STORAGE_COMMON_FILEAPI_FILE_SYSTEM_MOUNT_OPTION_H_
