// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_SYNCABLE_SYNC_DIRECTION_H_
#define WEBKIT_FILEAPI_SYNCABLE_SYNC_DIRECTION_H_

namespace sync_file_system {

enum SyncDirection {
  SYNC_DIRECTION_NONE,
  SYNC_DIRECTION_LOCAL_TO_REMOTE,
  SYNC_DIRECTION_REMOTE_TO_LOCAL,
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_SYNCABLE_SYNC_DIRECTION_H_
