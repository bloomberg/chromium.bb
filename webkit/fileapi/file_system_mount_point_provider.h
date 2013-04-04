// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_FILE_SYSTEM_MOUNT_POINT_PROVIDER_H_
#define WEBKIT_FILEAPI_FILE_SYSTEM_MOUNT_POINT_PROVIDER_H_

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/platform_file.h"
#include "webkit/fileapi/file_permission_policy.h"
#include "webkit/fileapi/file_system_types.h"
#include "webkit/storage/webkit_storage_export.h"

namespace webkit_blob {
class FileStreamReader;
}

namespace fileapi {

class AsyncFileUtil;
class CopyOrMoveFileValidatorFactory;
class FileSystemURL;
class FileStreamWriter;
class FileSystemContext;
class FileSystemFileUtil;
class FileSystemOperation;
class FileSystemQuotaUtil;
class RemoteFileSystemProxyInterface;

// An interface to provide mount-point-specific path-related utilities
// and specialized FileSystemFileUtil instance.
class WEBKIT_STORAGE_EXPORT FileSystemMountPointProvider {
 public:
  // Callback for ValidateFileSystemRoot.
  typedef base::Callback<void(base::PlatformFileError error)>
      ValidateFileSystemCallback;
  typedef base::Callback<void(base::PlatformFileError error)>
      DeleteFileSystemCallback;
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
  // file system url on the file thread.
  // If |create| is true this may also create the root directory for
  // the filesystem if it doesn't exist.
  virtual base::FilePath GetFileSystemRootPathOnFileThread(
      const FileSystemURL& url,
      bool create) = 0;

  // Returns the specialized FileSystemFileUtil for this mount point.
  // It is ok to return NULL if the filesystem doesn't support synchronous
  // version of FileUtil.
  virtual FileSystemFileUtil* GetFileUtil(FileSystemType type) = 0;

  // Returns the specialized AsyncFileUtil for this mount point.
  virtual AsyncFileUtil* GetAsyncFileUtil(FileSystemType type) = 0;

  // Returns the specialized CopyOrMoveFileValidatorFactory for this mount
  // point and |type|.  If |error_code| is PLATFORM_FILE_OK and the result
  // is NULL, then no validator is required.
  virtual CopyOrMoveFileValidatorFactory* GetCopyOrMoveFileValidatorFactory(
      FileSystemType type, base::PlatformFileError* error_code) = 0;

  // Initialize the CopyOrMoveFileValidatorFactory. Invalid to call more than
  // once.
  virtual void InitializeCopyOrMoveFileValidatorFactory(
      FileSystemType type,
      scoped_ptr<CopyOrMoveFileValidatorFactory> factory) = 0;

  // Returns file permission policy we should apply for the given |url|.
  virtual FilePermissionPolicy GetPermissionPolicy(
      const FileSystemURL& url,
      int permissions) const = 0;

  // Returns a new instance of the specialized FileSystemOperation for this
  // mount point based on the given triplet of |origin_url|, |file_system_type|
  // and |virtual_path|. On failure to create a file system operation, set
  // |error_code| correspondingly.
  // This method is usually dispatched by
  // FileSystemContext::CreateFileSystemOperation.
  virtual FileSystemOperation* CreateFileSystemOperation(
      const FileSystemURL& url,
      FileSystemContext* context,
      base::PlatformFileError* error_code) const = 0;

  // Creates a new file stream reader for a given filesystem URL |url| with an
  // offset |offset|. |expected_modification_time| specifies the expected last
  // modification if the value is non-null, the reader will check the underlying
  // file's actual modification time to see if the file has been modified, and
  // if it does any succeeding read operations should fail with
  // ERR_UPLOAD_FILE_CHANGED error.
  // The returned object must be owned and managed by the caller.
  // This method itself does *not* check if the given path exists and is a
  // regular file.
  virtual webkit_blob::FileStreamReader* CreateFileStreamReader(
    const FileSystemURL& url,
    int64 offset,
    const base::Time& expected_modification_time,
    FileSystemContext* context) const = 0;

  // Creates a new file stream writer for a given filesystem URL |url| with an
  // offset |offset|.
  // The returned object must be owned and managed by the caller.
  // This method itself does *not* check if the given path exists and is a
  // regular file.
  virtual FileStreamWriter* CreateFileStreamWriter(
      const FileSystemURL& url,
      int64 offset,
      FileSystemContext* context) const = 0;

  // Returns the specialized FileSystemQuotaUtil for this mount point.
  // This could return NULL if this mount point does not support quota.
  virtual FileSystemQuotaUtil* GetQuotaUtil() = 0;

  // Deletes the filesystem for the given |origin_url| and |type|.
  virtual void DeleteFileSystem(
      const GURL& origin_url,
      FileSystemType type,
      FileSystemContext* context,
      const DeleteFileSystemCallback& callback) = 0;
};

// An interface to control external file system access permissions.
class ExternalFileSystemMountPointProvider
    : public FileSystemMountPointProvider {
 public:
  // Returns true if |url| is allowed to be accessed.
  // This is supposed to perform ExternalFileSystem-specific security
  // checks. This method is also likely to be called by
  // FileSystemMountPointProvider::GetPermissionPolicy as
  // GetPermissionPolicy is supposed to perform fileapi-generic security
  // checks (which likely need to include ExternalFileSystem-specific checks).
  virtual bool IsAccessAllowed(const fileapi::FileSystemURL& url) const = 0;
  // Returns the list of top level directories that are exposed by this
  // provider. This list is used to set appropriate child process file access
  // permissions.
  virtual std::vector<base::FilePath> GetRootDirectories() const = 0;
  // Grants access to all external file system from extension identified with
  // |extension_id|.
  virtual void GrantFullAccessToExtension(const std::string& extension_id) = 0;
  // Grants access to |virtual_path| from |origin_url|.
  virtual void GrantFileAccessToExtension(
      const std::string& extension_id,
      const base::FilePath& virtual_path) = 0;
  // Revokes file access from extension identified with |extension_id|.
  virtual void RevokeAccessForExtension(
        const std::string& extension_id) = 0;
  // Gets virtual path by known filesystem path. Returns false when filesystem
  // path is not exposed by this provider.
  virtual bool GetVirtualPath(const base::FilePath& file_system_path,
                              base::FilePath* virtual_path) = 0;
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_FILE_SYSTEM_MOUNT_POINT_PROVIDER_H_
