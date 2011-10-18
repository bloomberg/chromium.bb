// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_FILE_SYSTEM_MOUNT_POINT_PROVIDER_H_
#define WEBKIT_FILEAPI_FILE_SYSTEM_MOUNT_POINT_PROVIDER_H_

#include <string>
#include <vector>

#include "base/file_path.h"
#include "googleurl/src/gurl.h"
#include "webkit/fileapi/file_system_path_manager.h"
#include "webkit/fileapi/file_system_types.h"

namespace fileapi {

// An interface to provide local filesystem paths.

class FileSystemMountPointProvider {
 public:
  virtual ~FileSystemMountPointProvider() {}

  // Checks if access to |virtual_path| is allowed from |origin_url|.
  virtual bool IsAccessAllowed(const GURL& origin_url,
                               FileSystemType type,
                               const FilePath& virtual_path) = 0;

  // Retrieves the root path for the given |origin_url| and |type|, and
  // calls the given |callback| with the root path and name.
  // If |create| is true this also creates the directory if it doesn't exist.
  virtual void ValidateFileSystemRootAndGetURL(
      const GURL& origin_url,
      FileSystemType type,
      bool create,
      FileSystemPathManager::GetRootPathCallback* callback) = 0;

  // Like GetFileSystemRootPath, but synchronous, and can be called only while
  // running on the file thread.
  virtual FilePath ValidateFileSystemRootAndGetPathOnFileThread(
      const GURL& origin_url,
      FileSystemType type,
      const FilePath& virtual_path,
      bool create) = 0;

  // Checks if a given |name| contains any restricted names/chars in it.
  // Callable on any thread.
  virtual bool IsRestrictedFileName(const FilePath& filename) const = 0;

  // Returns the list of top level directories that are exposed by this
  // provider. This list is used to set appropriate child process file access
  // permissions.
  virtual std::vector<FilePath> GetRootDirectories() const = 0;

  virtual FileSystemFileUtil* GetFileUtil() = 0;
};

// An interface to control external file system access permissions.
class ExternalFileSystemMountPointProvider
    : public FileSystemMountPointProvider {
 public:
  // Grant access to all external file system from extension identified with
  // |extension_id|.
  virtual void GrantFullAccessToExtension(const std::string& extension_id) = 0;
  // Grants access to |virtual_path| from |origin_url|.
  virtual void GrantFileAccessToExtension(
      const std::string& extension_id,
      const FilePath& virtual_path) = 0;
  // Revoke file access from extension identified with |extension_id|.
  virtual void RevokeAccessForExtension(
        const std::string& extension_id) = 0;
  // Adds a new mount point.
  virtual void AddMountPoint(FilePath mount_point) = 0;
  // Remove a mount point.
  virtual void RemoveMountPoint(FilePath mount_point) = 0;
  // Gets virtual path by known filesystem path. Returns false when filesystem
  // path is not exposed by this provider.
  virtual bool GetVirtualPath(const FilePath& filesystem_path,
                              FilePath* virtual_path) = 0;
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_FILE_SYSTEM_MOUNT_POINT_PROVIDER_H_
