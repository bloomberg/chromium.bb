// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_SYNCABLE_SYNC_FILE_METADATA_H_
#define WEBKIT_FILEAPI_SYNCABLE_SYNC_FILE_METADATA_H_

#include "base/basictypes.h"
#include "base/time.h"
#include "webkit/fileapi/file_system_url.h"
#include "webkit/fileapi/syncable/file_change.h"
#include "webkit/storage/webkit_storage_export.h"

namespace fileapi {

class WEBKIT_STORAGE_EXPORT SyncFileMetadata {
 public:
  SyncFileMetadata();
  SyncFileMetadata(sync_file_system::SyncFileType file_type,
                   int64 size,
                   const base::Time& last_modified);
  ~SyncFileMetadata();

  sync_file_system::SyncFileType file_type;
  int64 size;
  base::Time last_modified;

  bool operator==(const SyncFileMetadata& that) const;
};

class WEBKIT_STORAGE_EXPORT ConflictFileInfo {
 public:
  ConflictFileInfo();
  ~ConflictFileInfo();

  FileSystemURL url;
  SyncFileMetadata local_metadata;
  SyncFileMetadata remote_metadata;
};

struct WEBKIT_STORAGE_EXPORT LocalFileSyncInfo {
  LocalFileSyncInfo();
  ~LocalFileSyncInfo();

  FileSystemURL url;
  base::FilePath local_file_path;
  SyncFileMetadata metadata;
  sync_file_system::FileChangeList changes;
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_SYNCABLE_SYNC_FILE_METADATA_H_
