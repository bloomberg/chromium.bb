// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_FILE_SYSTEM_MOUNT_POINT_PROVIDER_H_
#define WEBKIT_FILEAPI_FILE_SYSTEM_MOUNT_POINT_PROVIDER_H_

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/platform_file.h"
#include "webkit/fileapi/file_system_types.h"

class GURL;

namespace webkit_blob {
class FileReader;
}

namespace fileapi {

class FileSystemContext;
class FileSystemFileUtil;
class FileSystemOperationInterface;
class RemoteFileSystemProxyInterface;

// An interface to provide mount-point-specific path-related utilities
// and specialized FileSystemFileUtil instance.
class FileSystemMountPointProvider {
 public:
  // Callback for ValidateFileSystemRoot.
  typedef base::Callback<void(base::PlatformFileError error)>
      ValidateFileSystemCallback;
  virtual ~FileSystemMountPointProvider() {}

  // Validates the filesystem for the given |origin_url| and |type|.
  // This verifies if it is allowed to request (or create) the filesystem
  // and if it can access (or create) the root directory of the mount point.
  // If |create| is true this may also create the root directory for
  // the filesystem if it doesn't exist.
  virtual void ValidateFileSystemRoot(
      const GURL& origin_url,
      FileSystemType type,
      bool create,
      const ValidateFileSystemCallback& callback) = 0;

  // Retrieves the root path of the filesystem specified by the given
  // |origin_url| and |type| on the file thread.
  // If |create| is true this may also create the root directory for
  // the filesystem if it doesn't exist.
  virtual FilePath GetFileSystemRootPathOnFileThread(
      const GURL& origin_url,
      FileSystemType type,
      const FilePath& virtual_path,
      bool create) = 0;

  // Checks if access to |virtual_path| is allowed from |origin_url|.
  virtual bool IsAccessAllowed(const GURL& origin_url,
                               FileSystemType type,
                               const FilePath& virtual_path) = 0;

  // Checks if a given |name| contains any restricted names/chars in it.
  // Callable on any thread.
  virtual bool IsRestrictedFileName(const FilePath& filename) const = 0;

  // Returns the list of top level directories that are exposed by this
  // provider. This list is used to set appropriate child process file access
  // permissions.
  virtual std::vector<FilePath> GetRootDirectories() const = 0;

  // Returns the specialized FileSystemFileUtil for this mount point.
  virtual FileSystemFileUtil* GetFileUtil() = 0;

  // Returns file path we should use to check access permissions for
  // |virtual_path|.
  virtual FilePath GetPathForPermissionsCheck(const FilePath& virtual_path)
      const = 0;

  // Returns a new instance of the specialized FileSystemOperation for this
  // mount point based on the given triplet of |origin_url|, |file_system_type|
  // and |virtual_path|.
  // This method is usually dispatched by
  // FileSystemContext::CreateFileSystemOperation.
  virtual FileSystemOperationInterface* CreateFileSystemOperation(
      const GURL& origin_url,
      FileSystemType file_system_type,
      const FilePath& virtual_path,
      FileSystemContext* context) const = 0;

  // Creates a new file reader for a given filesystem URL |url| with a offset
  // |offset|.
  // The returned object must be owned and managed by the caller.
  // This method itself does *not* check if the given path exists and is a
  // regular file.
  virtual webkit_blob::FileReader* CreateFileReader(
    const GURL& url,
    int64 offset,
    FileSystemContext* context) const = 0;
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
  // Checks if a given |mount_point| already exists.
  virtual bool HasMountPoint(const FilePath& mount_point) = 0;
  // Adds a new local mount point.
  virtual void AddLocalMountPoint(const FilePath& mount_point) = 0;
  // Adds a new remote mount point.
  virtual void AddRemoteMountPoint(
      const FilePath& mount_point,
      RemoteFileSystemProxyInterface* remote_proxy) = 0;
  // Remove a mount point.
  virtual void RemoveMountPoint(const FilePath& mount_point) = 0;
  // Gets virtual path by known filesystem path. Returns false when filesystem
  // path is not exposed by this provider.
  virtual bool GetVirtualPath(const FilePath& file_system_path,
                              FilePath* virtual_path) = 0;
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_FILE_SYSTEM_MOUNT_POINT_PROVIDER_H_
