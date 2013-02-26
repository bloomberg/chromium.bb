// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/syncable/sync_status_code.h"

#include "base/logging.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"

namespace sync_file_system {

std::string SyncStatusCodeToString(SyncStatusCode status) {
  switch (status) {
    case SYNC_STATUS_OK:
      return "OK.";
    case SYNC_STATUS_UNKNOWN:
      return "Unknown sync status.";
    case SYNC_STATUS_FAILED:
      return "Failed.";

    // PlatformFile related errors.
    // TODO(nhiroki): add stringize function for PlatformFileError into base/.
    case SYNC_FILE_ERROR_FAILED:
      return "File operation failed.";
    case SYNC_FILE_ERROR_IN_USE:
      return "File currently in use.";
    case SYNC_FILE_ERROR_EXISTS:
      return "File already exists.";
    case SYNC_FILE_ERROR_NOT_FOUND:
      return "File not found.";
    case SYNC_FILE_ERROR_ACCESS_DENIED:
      return "File access denied.";
    case SYNC_FILE_ERROR_TOO_MANY_OPENED:
      return "Too many files open.";
    case SYNC_FILE_ERROR_NO_MEMORY:
      return "Out of memory.";
    case SYNC_FILE_ERROR_NO_SPACE:
      return "No space left on disk.";
    case SYNC_FILE_ERROR_NOT_A_DIRECTORY:
      return "Not a directory.";
    case SYNC_FILE_ERROR_INVALID_OPERATION:
      return "Invalid file operation.";
    case SYNC_FILE_ERROR_SECURITY:
      return "Security error.";
    case SYNC_FILE_ERROR_ABORT:
      return "File operation aborted.";
    case SYNC_FILE_ERROR_NOT_A_FILE:
      return "Not a file.";
    case SYNC_FILE_ERROR_NOT_EMPTY:
      return "File not empty.";
    case SYNC_FILE_ERROR_INVALID_URL:
      return "Invalid URL.";

    // Database related errors.
    case SYNC_DATABASE_ERROR_NOT_FOUND:
      return "Database not found.";
    case SYNC_DATABASE_ERROR_CORRUPTION:
      return "Database was corrupted.";
    case SYNC_DATABASE_ERROR_IO_ERROR:
      return "Database I/O error.";
    case SYNC_DATABASE_ERROR_FAILED:
      return "Database operation failed.";

    // Sync specific status code.
    case SYNC_STATUS_FILE_BUSY:
      return "Sync: file is busy.";
    case SYNC_STATUS_HAS_CONFLICT:
      return "Sync: file has conflict.";
    case SYNC_STATUS_NO_CONFLICT:
      return "Sync: file has no conflict.";
    case SYNC_STATUS_ABORT:
      return "Sync: operation aborted.";
    case SYNC_STATUS_NO_CHANGE_TO_SYNC:
      return "Sync: no change to synchronize.";
    case SYNC_STATUS_RETRY:
      return "Sync: retry to synchronize.";
    case SYNC_STATUS_NETWORK_ERROR:
      return "Sync: network error.";
    case SYNC_STATUS_AUTHENTICATION_FAILED:
      return "Sync: authentication failed.";
    case SYNC_STATUS_NOT_INITIALIZED:
      return "Sync: not initialized.";
    case SYNC_STATUS_NOT_MODIFIED:
      return "Sync: file not modified.";
    case SYNC_STATUS_SYNC_DISABLED:
      return "Sync: sync is disabled.";
  }
  NOTREACHED();
  return "Unknown error.";
}

SyncStatusCode LevelDBStatusToSyncStatusCode(const leveldb::Status& status) {
  if (status.ok())
    return SYNC_STATUS_OK;
  else if (status.IsNotFound())
    return SYNC_DATABASE_ERROR_NOT_FOUND;
  else if (status.IsCorruption())
    return SYNC_DATABASE_ERROR_CORRUPTION;
  else if (status.IsIOError())
    return SYNC_DATABASE_ERROR_IO_ERROR;
  else
    return SYNC_DATABASE_ERROR_FAILED;
}

SyncStatusCode PlatformFileErrorToSyncStatusCode(
    base::PlatformFileError file_error) {
  switch (file_error) {
    case base::PLATFORM_FILE_OK:
      return SYNC_STATUS_OK;
    case base::PLATFORM_FILE_ERROR_FAILED:
      return SYNC_FILE_ERROR_FAILED;
    case base::PLATFORM_FILE_ERROR_IN_USE:
      return SYNC_FILE_ERROR_IN_USE;
    case base::PLATFORM_FILE_ERROR_EXISTS:
      return SYNC_FILE_ERROR_EXISTS;
    case base::PLATFORM_FILE_ERROR_NOT_FOUND:
      return SYNC_FILE_ERROR_NOT_FOUND;
    case base::PLATFORM_FILE_ERROR_ACCESS_DENIED:
      return SYNC_FILE_ERROR_ACCESS_DENIED;
    case base::PLATFORM_FILE_ERROR_TOO_MANY_OPENED:
      return SYNC_FILE_ERROR_TOO_MANY_OPENED;
    case base::PLATFORM_FILE_ERROR_NO_MEMORY:
      return SYNC_FILE_ERROR_NO_MEMORY;
    case base::PLATFORM_FILE_ERROR_NO_SPACE:
      return SYNC_FILE_ERROR_NO_SPACE;
    case base::PLATFORM_FILE_ERROR_NOT_A_DIRECTORY:
      return SYNC_FILE_ERROR_NOT_A_DIRECTORY;
    case base::PLATFORM_FILE_ERROR_INVALID_OPERATION:
      return SYNC_FILE_ERROR_INVALID_OPERATION;
    case base::PLATFORM_FILE_ERROR_SECURITY:
      return SYNC_FILE_ERROR_SECURITY;
    case base::PLATFORM_FILE_ERROR_ABORT:
      return SYNC_FILE_ERROR_ABORT;
    case base::PLATFORM_FILE_ERROR_NOT_A_FILE:
      return SYNC_FILE_ERROR_NOT_A_FILE;
    case base::PLATFORM_FILE_ERROR_NOT_EMPTY:
      return SYNC_FILE_ERROR_NOT_EMPTY;
    case base::PLATFORM_FILE_ERROR_INVALID_URL:
      return SYNC_FILE_ERROR_INVALID_URL;
    default:
      return SYNC_FILE_ERROR_FAILED;
  }
}

}  // namespace sync_file_system
