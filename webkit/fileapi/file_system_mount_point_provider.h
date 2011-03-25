// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_FILE_SYSTEM_MOUNT_POINT_PROVIDER_H_
#define WEBKIT_FILEAPI_FILE_SYSTEM_MOUNT_POINT_PROVIDER_H_

#include "base/file_path.h"
#include "googleurl/src/gurl.h"
#include "webkit/fileapi/file_system_path_manager.h"
#include "webkit/fileapi/file_system_types.h"

namespace fileapi {

// An interface to provide local filesystem paths.

class FileSystemMountPointProvider {
 public:
  virtual ~FileSystemMountPointProvider() {}

  // Checks if mount point access is allowed from |origin_url|.
  virtual bool IsAccessAllowed(const GURL& origin_url) = 0;

  // Retrieves the root path for the given |origin_url| and |type|, and
  // calls the given |callback| with the root path and name.
  // If |create| is true this also creates the directory if it doesn't exist.
  virtual void GetFileSystemRootPath(
      const GURL& origin_url,
      FileSystemType type,
      bool create,
      FileSystemPathManager::GetRootPathCallback* callback) = 0;

  // Like GetFileSystemRootPath, but synchronous, and can be called only while
  // running on the file thread.
  virtual FilePath GetFileSystemRootPathOnFileThread(
      const GURL& origin_url,
      FileSystemType type,
      const FilePath& virtual_path,
      bool create) = 0;

  // Checks if a given |name| contains any restricted names/chars in it.
  // Callable on any thread.
  virtual bool IsRestrictedFileName(const FilePath& filename) const = 0;
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_FILE_SYSTEM_MOUNT_POINT_PROVIDER_H_

