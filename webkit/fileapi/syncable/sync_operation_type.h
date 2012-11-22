// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_SYNCABLE_SYNC_OPERATION_TYPE_H_
#define WEBKIT_FILEAPI_SYNCABLE_SYNC_OPERATION_TYPE_H_

namespace fileapi {

enum SyncOperationType {
  // Indicates no operation has been made.
  SYNC_OPERATION_NONE,

  // Indicates a new file or directory has been added.
  SYNC_OPERATION_ADD,

  // Indicates an existing file or directory has been updated.
  SYNC_OPERATION_UPDATE,

  // Indicates a file or directory has been deleted.
  SYNC_OPERATION_DELETE,
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_SYNCABLE_SYNC_OPERATION_TYPE_H_
