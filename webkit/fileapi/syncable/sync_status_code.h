// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_SYNCABLE_SYNC_STATUS_CODE_H_
#define WEBKIT_FILEAPI_SYNCABLE_SYNC_STATUS_CODE_H_

#include <string>

#include "base/platform_file.h"
#include "webkit/storage/webkit_storage_export.h"

namespace leveldb {
class Status;
}

namespace sync_file_system {

enum SyncStatusCode {
  SYNC_STATUS_OK = 0,
  SYNC_STATUS_UNKNOWN = -1000,

  // Generic error code which is not specifically related to a specific
  // submodule error code (yet).
  SYNC_STATUS_FAILED = -1001,

  // Basic ones that could be directly mapped to PlatformFileError.
  SYNC_FILE_ERROR_FAILED = -1,
  SYNC_FILE_ERROR_IN_USE = -2,
  SYNC_FILE_ERROR_EXISTS = -3,
  SYNC_FILE_ERROR_NOT_FOUND = -4,
  SYNC_FILE_ERROR_ACCESS_DENIED = -5,
  SYNC_FILE_ERROR_TOO_MANY_OPENED = -6,
  SYNC_FILE_ERROR_NO_MEMORY = -7,
  SYNC_FILE_ERROR_NO_SPACE = -8,
  SYNC_FILE_ERROR_NOT_A_DIRECTORY = -9,
  SYNC_FILE_ERROR_INVALID_OPERATION = -10,
  SYNC_FILE_ERROR_SECURITY = -11,
  SYNC_FILE_ERROR_ABORT = -12,
  SYNC_FILE_ERROR_NOT_A_FILE = -13,
  SYNC_FILE_ERROR_NOT_EMPTY = -14,
  SYNC_FILE_ERROR_INVALID_URL = -15,

  // Database related errors.
  SYNC_DATABASE_ERROR_NOT_FOUND = -16,
  SYNC_DATABASE_ERROR_CORRUPTION = -17,
  SYNC_DATABASE_ERROR_IO_ERROR = -18,
  SYNC_DATABASE_ERROR_FAILED = -19,

  // Sync specific status code.
  SYNC_STATUS_FILE_BUSY = -100,
  SYNC_STATUS_HAS_CONFLICT = -101,
  SYNC_STATUS_NO_CONFLICT = -102,
  SYNC_STATUS_ABORT = -103,
  SYNC_STATUS_NO_CHANGE_TO_SYNC = -104,
  SYNC_STATUS_RETRY = -105,
  SYNC_STATUS_NETWORK_ERROR = -106,
  SYNC_STATUS_AUTHENTICATION_FAILED = -107,
  SYNC_STATUS_NOT_INITIALIZED = -108,
  SYNC_STATUS_NOT_MODIFIED = -109,
  SYNC_STATUS_SYNC_DISABLED = -110,
};

WEBKIT_STORAGE_EXPORT std::string SyncStatusCodeToString(SyncStatusCode status);

WEBKIT_STORAGE_EXPORT SyncStatusCode LevelDBStatusToSyncStatusCode(
    const leveldb::Status& status);

WEBKIT_STORAGE_EXPORT SyncStatusCode PlatformFileErrorToSyncStatusCode(
    base::PlatformFileError file_error);

}  // namespace sync_file_system

#endif  // WEBKIT_FILEAPI_SYNCABLE_SYNC_STATUS_CODE_H_
