// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_SYNCABLE_SYNC_STATUS_CODE_H_
#define WEBKIT_FILEAPI_SYNCABLE_SYNC_STATUS_CODE_H_

namespace fileapi {

enum SyncStatusCode {
  SYNC_STATUS_OK = 0,

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
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_SYNCABLE_SYNC_STATUS_CODE_H_
