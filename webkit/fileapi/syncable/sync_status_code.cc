// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/syncable/sync_status_code.h"

#include "third_party/leveldatabase/src/include/leveldb/db.h"

namespace fileapi {

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

}  // namespace fileapi
