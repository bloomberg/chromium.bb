// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_MOCK_FILE_SYSTEM_CONTEXT_H_
#define WEBKIT_FILEAPI_MOCK_FILE_SYSTEM_CONTEXT_H_

#include "base/files/file_path.h"

namespace quota {
class QuotaManagerProxy;
class SpecialStoragePolicy;
}

namespace fileapi {

class FileSystemContext;

FileSystemContext* CreateFileSystemContextForTesting(
    quota::QuotaManagerProxy* quota_manager_proxy,
    const base::FilePath& base_path);

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_MOCK_FILE_SYSTEM_CONTEXT_H_
