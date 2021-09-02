// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_FILEAPI_ARC_CONTENT_FILE_SYSTEM_SIZE_UTIL_H_
#define CHROME_BROWSER_ASH_ARC_FILEAPI_ARC_CONTENT_FILE_SYSTEM_SIZE_UTIL_H_

class GURL;

#include "base/callback.h"
#include "base/files/file.h"

namespace arc {

class ArcFileSystemOperationRunner;

constexpr int64_t kUnknownFileSize = -1;

using GetFileSizeFromOpenFileCallback =
    base::OnceCallback<void(base::File::Error error, int64_t file_size)>;

// Opens the file and gets the file size from the opened file descriptor.
// It will fail if the file descriptor returned by the open call is not a file.
// Must be run on the IO thread.
void GetFileSizeFromOpenFileOnIOThread(
    GURL content_url,
    GetFileSizeFromOpenFileCallback callback);
// Same as GetFileSizeFromOpenFileOnIOThread, but must be run on the UI thread.
void GetFileSizeFromOpenFileOnUIThread(
    GURL content_url,
    ArcFileSystemOperationRunner* runner,
    GetFileSizeFromOpenFileCallback callback);
}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_FILEAPI_ARC_CONTENT_FILE_SYSTEM_SIZE_UTIL_H_
