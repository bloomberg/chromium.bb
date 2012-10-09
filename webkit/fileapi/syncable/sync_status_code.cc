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
    return SYNC_DATABASE_ERROR_UNKNOWN;
}

}  // namespace fileapi
