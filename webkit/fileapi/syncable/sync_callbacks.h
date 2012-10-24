// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_SYNCABLE_SYNC_CALLBACKS_H_
#define WEBKIT_FILEAPI_SYNCABLE_SYNC_CALLBACKS_H_

#include "base/callback_forward.h"
#include "webkit/fileapi/syncable/sync_status_code.h"

namespace fileapi {
class FileSystemURL;

typedef base::Callback<void(SyncStatusCode status)> StatusCallback;
typedef base::Callback<void(SyncStatusCode, const FileSystemURL& url)>
    SyncCompletionCallback;
}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_SYNCABLE_SYNC_CALLBACKS_H_
