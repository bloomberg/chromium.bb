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
#include "base/synchronization/lock.h"
#include "webkit/fileapi/file_system_mount_point_provider.h"
#include "webkit/fileapi/local_file_util.h"
#include "webkit/quota/special_storage_policy.h"

namespace fileapi {
class FileSystemFileUtil;
}

namespace chromeos {

class FileAccessPermissions;

// An interface to provide local filesystem paths.
class CrosMountPointProvider
    : public fileapi::ExternalFileSystemMountPointProvider {
 public:
  // Mount point file system location enum.
  enum FileSystemLocation {
    // File system that is locally mounted by the underlying OS.
    LOCAL,
    // File system that is remotely hosted on the net.
    REMOTE,
  };

  typedef fileapi::FileSystemMountPointProvider::ValidateFileSystemCallback
      ValidateFileSystemCallback;

  CrosMountPointProvider(
      scoped_refptr<quota::SpecialStoragePolicy> special_storage_policy);
  virtual ~CrosMountPointProvider();

  // fileapi::FileSystemMountPointProvider overrides.
  virtual void ValidateFileSystemRoot(
      const GURL& origin_url,
      fileapi::FileSystemType type,
      bool create,
      const ValidateFileSystemCallback& callback) OVERRIDE;
  virtual FilePath GetFileSystemRootPathOnFileThread(
      const GURL& origin_url,
      fileapi::FileSystemType type,
      const FilePath& virtual_path,
      bool create) OVERRIDE;
  virtual bool IsAccessAllowed(
      const GURL& origin_url,
      fileapi::FileSystemType type,
      const FilePath& virtual_path) OVERRIDE;
  virtual bool IsRestrictedFileName(const FilePath& filename) const OVERRIDE;
  virtual std::vector<FilePath> GetRootDirectories() const OVERRIDE;
  virtual fileapi::FileSystemFileUtil* GetFileUtil() OVERRIDE;
  virtual FilePath GetPathForPermissionsCheck(const FilePath& virtual_path)
      const OVERRIDE;
  virtual fileapi::FileSystemOperationInterface* CreateFileSystemOperation(
      const GURL& origin_url,
      fileapi::FileSystemType file_system_type,
      const FilePath& virtual_path,
      fileapi::FileSystemContext* context) const OVERRIDE;
  virtual webkit_blob::FileReader* CreateFileReader(
    const GURL& path,
    int64 offset,
    fileapi::FileSystemContext* context) const OVERRIDE;
  virtual fileapi::FileSystemQuotaUtil* GetQuotaUtil() OVERRIDE;

  // fileapi::ExternalFileSystemMountPointProvider overrides.
  virtual void GrantFullAccessToExtension(
      const std::string& extension_id) OVERRIDE;
  virtual void GrantFileAccessToExtension(
      const std::string& extension_id, const FilePath& virtual_path) OVERRIDE;
  virtual void RevokeAccessForExtension(
      const std::string& extension_id) OVERRIDE;
  virtual bool HasMountPoint(const FilePath& mount_point) OVERRIDE;
  virtual void AddLocalMountPoint(const FilePath& mount_point) OVERRIDE;
  virtual void AddRemoteMountPoint(
      const FilePath& mount_point,
      fileapi::RemoteFileSystemProxyInterface* remote_proxy) OVERRIDE;
  virtual void RemoveMountPoint(const FilePath& mount_point) OVERRIDE;
  virtual bool GetVirtualPath(const FilePath& filesystem_path,
                              FilePath* virtual_path) OVERRIDE;

 private:
  class GetFileSystemRootPathTask;

  // Representation of a mount point exposed by this external mount point
  // provider.
  struct MountPoint {
    MountPoint(const FilePath& web_path,
               const FilePath& local_path,
               FileSystemLocation loc,
               fileapi::RemoteFileSystemProxyInterface* proxy);
    virtual ~MountPoint();
    // Virtual web path, relative to external root in filesytem URLs.
    // For example, in "filesystem://.../external/foo/bar/" this path would
    // map to "foo/bar/".
    const FilePath web_root_path;
    // Parent directory for the exposed file system path. For example,
    // mount point that exposes "/media/removable" would have this
    // root path as "/media".
    const FilePath local_root_path;
    // File system location.
    const FileSystemLocation location;
    // Remote file system proxy for remote mount points.
    scoped_refptr<fileapi::RemoteFileSystemProxyInterface> remote_proxy;
  };

  typedef std::map<std::string, MountPoint> MountPointMap;

  // Gives the real file system's |root_path| for given |virtual_path|. Returns
  // false when |virtual_path| cannot be mapped to the real file system.
  bool GetRootForVirtualPath(const FilePath& virtual_path, FilePath* root_path);
  // Returns mount point info for a given |virtual_path|, NULL if the path is
  // not part of the mounted file systems exposed through this provider.
  const MountPoint* GetMountPoint(const FilePath& virtual_path) const;

  base::Lock mount_point_map_lock_;
  MountPointMap mount_point_map_;
  scoped_refptr<quota::SpecialStoragePolicy> special_storage_policy_;
  scoped_ptr<FileAccessPermissions> file_access_permissions_;
  scoped_ptr<fileapi::LocalFileUtil> local_file_util_;
  DISALLOW_COPY_AND_ASSIGN(CrosMountPointProvider);
};

}  // namespace chromeos

#endif  // WEBKIT_CHROMEOS_FILEAPI_CROS_MOUNT_POINT_PROVIDER_H_
