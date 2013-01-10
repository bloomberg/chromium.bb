// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_CHROMEOS_FILEAPI_CROS_MOUNT_POINT_PROVIDER_H_
#define WEBKIT_CHROMEOS_FILEAPI_CROS_MOUNT_POINT_PROVIDER_H_

#include <map>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "webkit/fileapi/file_system_mount_point_provider.h"
#include "webkit/quota/special_storage_policy.h"
#include "webkit/storage/webkit_storage_export.h"

namespace fileapi {
class FileSystemFileUtil;
class FileSystemURL;
class IsolatedContext;
class LocalFileUtil;
}

namespace chromeos {

class FileAccessPermissions;

// An interface to provide local filesystem paths.
class WEBKIT_STORAGE_EXPORT CrosMountPointProvider
    : public fileapi::ExternalFileSystemMountPointProvider {
 public:
  using fileapi::FileSystemMountPointProvider::ValidateFileSystemCallback;
  using fileapi::FileSystemMountPointProvider::DeleteFileSystemCallback;

  CrosMountPointProvider(
      scoped_refptr<quota::SpecialStoragePolicy> special_storage_policy);
  virtual ~CrosMountPointProvider();

  // Returns true if CrosMountpointProvider can handle |url|, i.e. its
  // file system type matches with what this provider supports.
  // This could be called on any threads.
  static bool CanHandleURL(const fileapi::FileSystemURL& url);

  // fileapi::FileSystemMountPointProvider overrides.
  virtual void ValidateFileSystemRoot(
      const GURL& origin_url,
      fileapi::FileSystemType type,
      bool create,
      const ValidateFileSystemCallback& callback) OVERRIDE;
  virtual FilePath GetFileSystemRootPathOnFileThread(
      const fileapi::FileSystemURL& url,
      bool create) OVERRIDE;
  virtual bool IsAccessAllowed(const fileapi::FileSystemURL& url) OVERRIDE;
  virtual bool IsRestrictedFileName(const FilePath& filename) const OVERRIDE;
  virtual fileapi::FileSystemFileUtil* GetFileUtil(
      fileapi::FileSystemType type) OVERRIDE;
  virtual fileapi::FilePermissionPolicy GetPermissionPolicy(
      const fileapi::FileSystemURL& url,
      int permissions) const OVERRIDE;
  virtual fileapi::FileSystemOperation* CreateFileSystemOperation(
      const fileapi::FileSystemURL& url,
      fileapi::FileSystemContext* context,
      base::PlatformFileError* error_code) const OVERRIDE;
  virtual webkit_blob::FileStreamReader* CreateFileStreamReader(
      const fileapi::FileSystemURL& path,
      int64 offset,
      const base::Time& expected_modification_time,
      fileapi::FileSystemContext* context) const OVERRIDE;
  virtual fileapi::FileStreamWriter* CreateFileStreamWriter(
      const fileapi::FileSystemURL& url,
      int64 offset,
      fileapi::FileSystemContext* context) const OVERRIDE;
  virtual fileapi::FileSystemQuotaUtil* GetQuotaUtil() OVERRIDE;
  virtual void DeleteFileSystem(
      const GURL& origin_url,
      fileapi::FileSystemType type,
      fileapi::FileSystemContext* context,
      const DeleteFileSystemCallback& callback) OVERRIDE;

  // fileapi::ExternalFileSystemMountPointProvider overrides.
  virtual std::vector<FilePath> GetRootDirectories() const OVERRIDE;
  virtual void GrantFullAccessToExtension(
      const std::string& extension_id) OVERRIDE;
  virtual void GrantFileAccessToExtension(
      const std::string& extension_id, const FilePath& virtual_path) OVERRIDE;
  virtual void RevokeAccessForExtension(
      const std::string& extension_id) OVERRIDE;
  virtual bool HasMountPoint(const FilePath& mount_point) OVERRIDE;
  virtual void AddLocalMountPoint(const FilePath& mount_point) OVERRIDE;
  virtual void AddRestrictedLocalMountPoint(
      const FilePath& mount_point) OVERRIDE;
  virtual void AddRemoteMountPoint(
      const FilePath& mount_point,
      fileapi::RemoteFileSystemProxyInterface* remote_proxy) OVERRIDE;
  virtual void RemoveMountPoint(const FilePath& mount_point) OVERRIDE;
  virtual bool GetVirtualPath(const FilePath& filesystem_path,
                              FilePath* virtual_path) OVERRIDE;

 private:
  typedef scoped_refptr<fileapi::RemoteFileSystemProxyInterface> RemoteProxy;
  typedef std::map<FilePath::StringType, RemoteProxy> RemoteProxyMap;

  fileapi::IsolatedContext* isolated_context() const;

  // Represents a map from mount point name to a remote proxy.
  RemoteProxyMap remote_proxy_map_;

  // Reverse map for GetVirtualPath.
  std::map<FilePath, FilePath> local_to_virtual_map_;

  mutable base::Lock mount_point_map_lock_;

  scoped_refptr<quota::SpecialStoragePolicy> special_storage_policy_;
  scoped_ptr<FileAccessPermissions> file_access_permissions_;
  scoped_ptr<fileapi::LocalFileUtil> local_file_util_;
  DISALLOW_COPY_AND_ASSIGN(CrosMountPointProvider);
};

}  // namespace chromeos

#endif  // WEBKIT_CHROMEOS_FILEAPI_CROS_MOUNT_POINT_PROVIDER_H_
