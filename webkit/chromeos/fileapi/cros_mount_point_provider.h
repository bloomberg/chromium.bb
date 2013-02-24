// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_CHROMEOS_FILEAPI_CROS_MOUNT_POINT_PROVIDER_H_
#define WEBKIT_CHROMEOS_FILEAPI_CROS_MOUNT_POINT_PROVIDER_H_

#include <map>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "webkit/fileapi/file_system_mount_point_provider.h"
#include "webkit/quota/special_storage_policy.h"
#include "webkit/storage/webkit_storage_export.h"

namespace fileapi {
class AsyncFileUtilAdapter;
class ExternalMountPoints;
class FileSystemFileUtil;
class FileSystemURL;
class IsolatedContext;
}

namespace chromeos {

class FileAccessPermissions;

// An interface to provide local filesystem paths.
class WEBKIT_STORAGE_EXPORT CrosMountPointProvider
    : public fileapi::ExternalFileSystemMountPointProvider {
 public:
  using fileapi::FileSystemMountPointProvider::ValidateFileSystemCallback;
  using fileapi::FileSystemMountPointProvider::DeleteFileSystemCallback;

  // CrosMountPointProvider will take an ownership of a |mount_points|
  // reference. On the other hand, |system_mount_points| will be kept as a raw
  // pointer and it should outlive CrosMountPointProvider instance.
  CrosMountPointProvider(
      scoped_refptr<quota::SpecialStoragePolicy> special_storage_policy,
      scoped_refptr<fileapi::ExternalMountPoints> mount_points,
      fileapi::ExternalMountPoints* system_mount_points);
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
  virtual base::FilePath GetFileSystemRootPathOnFileThread(
      const fileapi::FileSystemURL& url,
      bool create) OVERRIDE;
  virtual fileapi::FileSystemFileUtil* GetFileUtil(
      fileapi::FileSystemType type) OVERRIDE;
  virtual fileapi::AsyncFileUtil* GetAsyncFileUtil(
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
  virtual bool IsAccessAllowed(const fileapi::FileSystemURL& url)
      const OVERRIDE;
  virtual std::vector<base::FilePath> GetRootDirectories() const OVERRIDE;
  virtual void GrantFullAccessToExtension(
      const std::string& extension_id) OVERRIDE;
  virtual void GrantFileAccessToExtension(
      const std::string& extension_id,
      const base::FilePath& virtual_path) OVERRIDE;
  virtual void RevokeAccessForExtension(
      const std::string& extension_id) OVERRIDE;
  virtual bool GetVirtualPath(const base::FilePath& filesystem_path,
                              base::FilePath* virtual_path) OVERRIDE;

 private:
  fileapi::RemoteFileSystemProxyInterface* GetRemoteProxy(
      const std::string& mount_name) const;

  scoped_refptr<quota::SpecialStoragePolicy> special_storage_policy_;
  scoped_ptr<FileAccessPermissions> file_access_permissions_;
  scoped_ptr<fileapi::AsyncFileUtilAdapter> local_file_util_;

  // Mount points specific to the owning context.
  //
  // Add/Remove MountPoints will affect only these mount points.
  //
  // It is legal to have mount points with the same name as in
  // system_mount_points_. Also, mount point paths may overlap with mount point
  // paths in system_mount_points_. In both cases mount points in
  // |mount_points_| will have a priority.
  // E.g. if |mount_points_| map 'foo1' to '/foo/foo1' and
  // |file_system_mount_points_| map 'xxx' to '/foo/foo1/xxx', |GetVirtualPaths|
  // will resolve '/foo/foo1/xxx/yyy' as 'foo1/xxx/yyy' (i.e. the mapping from
  // |mount_points_| will be used).
  scoped_refptr<fileapi::ExternalMountPoints> mount_points_;

  // Globally visible mount points. System MountPonts instance should outlive
  // all CrosMountPointProvider instances, so raw pointer is safe.
  fileapi::ExternalMountPoints* system_mount_points_;

  DISALLOW_COPY_AND_ASSIGN(CrosMountPointProvider);
};

}  // namespace chromeos

#endif  // WEBKIT_CHROMEOS_FILEAPI_CROS_MOUNT_POINT_PROVIDER_H_
