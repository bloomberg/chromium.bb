// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_BROWSER_CHROMEOS_FILEAPI_CROS_MOUNT_POINT_PROVIDER_H_
#define WEBKIT_BROWSER_CHROMEOS_FILEAPI_CROS_MOUNT_POINT_PROVIDER_H_

#include <map>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "webkit/browser/fileapi/file_system_mount_point_provider.h"
#include "webkit/browser/quota/special_storage_policy.h"
#include "webkit/browser/webkit_storage_browser_export.h"
#include "webkit/common/fileapi/file_system_types.h"

namespace fileapi {
class AsyncFileUtilAdapter;
class CopyOrMoveFileValidatorFactory;
class ExternalMountPoints;
class FileSystemFileUtil;
class FileSystemURL;
class IsolatedContext;
}

namespace chromeos {

class FileAccessPermissions;

// CrosMountPointProvider is a Chrome OS specific implementation of
// ExternalFileSystemMountPointProvider. This class is responsible for a
// number of things, including:
//
// - Add system mount points
// - Grant/revoke/check file access permissions
// - Create FileSystemOperation per file system type
// - Create FileStreamReader/Writer per file system type
//
// Chrome OS specific mount points:
//
// "Downloads" is a mount point for user's Downloads directory on the local
// disk, where downloaded files are stored by default.
//
// "archive" is a mount point for an archive file, such as a zip file. This
// mount point exposes contents of an archive file via cros_disks and AVFS
// <http://avf.sourceforge.net/>.
//
// "removable" is a mount point for removable media such as an SD card.
// Insertion and removal of removable media are handled by cros_disks.
//
// "oem" is a read-only mount point for a directory containing OEM data.
//
// "drive" is a mount point for Google Drive. Drive is integrated with the
// FileSystem API layer via drive::FileSystemProxy. This mount point is added
// by drive::DriveIntegrationService.
//
// These mount points are placed under the "external" namespace, and file
// system URLs for these mount points look like:
//
//   filesystem:<origin>/external/<mount_name>/...
//
class WEBKIT_STORAGE_BROWSER_EXPORT CrosMountPointProvider
    : public fileapi::ExternalFileSystemMountPointProvider {
 public:
  using fileapi::FileSystemMountPointProvider::OpenFileSystemCallback;
  using fileapi::FileSystemMountPointProvider::DeleteFileSystemCallback;

  // CrosMountPointProvider will take an ownership of a |mount_points|
  // reference. On the other hand, |system_mount_points| will be kept as a raw
  // pointer and it should outlive CrosMountPointProvider instance.
  CrosMountPointProvider(
      scoped_refptr<quota::SpecialStoragePolicy> special_storage_policy,
      scoped_refptr<fileapi::ExternalMountPoints> mount_points,
      fileapi::ExternalMountPoints* system_mount_points);
  virtual ~CrosMountPointProvider();

  // Adds system mount points, such as "archive", and "removable". This
  // function is no-op if these mount points are already present.
  void AddSystemMountPoints();

  // Returns true if CrosMountpointProvider can handle |url|, i.e. its
  // file system type matches with what this provider supports.
  // This could be called on any threads.
  static bool CanHandleURL(const fileapi::FileSystemURL& url);

  // fileapi::FileSystemMountPointProvider overrides.
  virtual bool CanHandleType(fileapi::FileSystemType type) const OVERRIDE;
  virtual void OpenFileSystem(
      const GURL& origin_url,
      fileapi::FileSystemType type,
      fileapi::OpenFileSystemMode mode,
      const OpenFileSystemCallback& callback) OVERRIDE;
  virtual fileapi::FileSystemFileUtil* GetFileUtil(
      fileapi::FileSystemType type) OVERRIDE;
  virtual fileapi::AsyncFileUtil* GetAsyncFileUtil(
      fileapi::FileSystemType type) OVERRIDE;
  virtual fileapi::CopyOrMoveFileValidatorFactory*
      GetCopyOrMoveFileValidatorFactory(
          fileapi::FileSystemType type,
          base::PlatformFileError* error_code) OVERRIDE;
  virtual fileapi::FilePermissionPolicy GetPermissionPolicy(
      const fileapi::FileSystemURL& url,
      int permissions) const OVERRIDE;
  virtual fileapi::FileSystemOperation* CreateFileSystemOperation(
      const fileapi::FileSystemURL& url,
      fileapi::FileSystemContext* context,
      base::PlatformFileError* error_code) const OVERRIDE;
  virtual scoped_ptr<webkit_blob::FileStreamReader> CreateFileStreamReader(
      const fileapi::FileSystemURL& path,
      int64 offset,
      const base::Time& expected_modification_time,
      fileapi::FileSystemContext* context) const OVERRIDE;
  virtual scoped_ptr<fileapi::FileStreamWriter> CreateFileStreamWriter(
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
  base::FilePath GetFileSystemRootPath(const fileapi::FileSystemURL& url) const;

  scoped_refptr<quota::SpecialStoragePolicy> special_storage_policy_;
  scoped_ptr<FileAccessPermissions> file_access_permissions_;
  scoped_ptr<fileapi::AsyncFileUtilAdapter> local_file_util_;

  // Mount points specific to the owning context (i.e. per-profile mount
  // points).
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

#endif  // WEBKIT_BROWSER_CHROMEOS_FILEAPI_CROS_MOUNT_POINT_PROVIDER_H_
