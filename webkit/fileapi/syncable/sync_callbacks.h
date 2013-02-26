// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_SYNCABLE_SYNC_CALLBACKS_H_
#define WEBKIT_FILEAPI_SYNCABLE_SYNC_CALLBACKS_H_

#include "base/callback_forward.h"
#include "webkit/fileapi/file_system_url.h"
#include "webkit/fileapi/syncable/sync_file_status.h"
#include "webkit/fileapi/syncable/sync_status_code.h"

namespace fileapi {
class FileSystemURL;
}

namespace sync_file_system {

class SyncFileMetadata;

typedef base::Callback<void(SyncStatusCode status)>
    SyncStatusCallback;

typedef base::Callback<void(SyncStatusCode status,
                            const fileapi::FileSystemURL& url)>
    SyncFileCallback;

typedef base::Callback<void(SyncStatusCode status,
                            const SyncFileMetadata& metadata)>
    SyncFileMetadataCallback;

typedef base::Callback<void(SyncStatusCode status,
                            const fileapi::FileSystemURLSet& urls)>
    SyncFileSetCallback;

typedef base::Callback<void(SyncStatusCode status,
                            SyncFileStatus sync_file_status)>
    SyncFileStatusCallback;

}  // namespace sync_file_system

#endif  // WEBKIT_FILEAPI_SYNCABLE_SYNC_CALLBACKS_H_
