// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_BROWSER_FILEAPI_MOCK_FILE_SYSTEM_CONTEXT_H_
#define WEBKIT_BROWSER_FILEAPI_MOCK_FILE_SYSTEM_CONTEXT_H_

#include "base/files/file_path.h"
#include "base/memory/scoped_vector.h"

namespace quota {
class QuotaManagerProxy;
class SpecialStoragePolicy;
}

namespace fileapi {

class FileSystemContext;
class FileSystemBackend;

FileSystemContext* CreateFileSystemContextForTesting(
    quota::QuotaManagerProxy* quota_manager_proxy,
    const base::FilePath& base_path);

// The caller is responsible for including TestFileSystemBackend in
// |additional_providers| if needed.
FileSystemContext* CreateFileSystemContextWithAdditionalProvidersForTesting(
    quota::QuotaManagerProxy* quota_manager_proxy,
    ScopedVector<FileSystemBackend> additional_providers,
    const base::FilePath& base_path);

}  // namespace fileapi

#endif  // WEBKIT_BROWSER_FILEAPI_MOCK_FILE_SYSTEM_CONTEXT_H_
