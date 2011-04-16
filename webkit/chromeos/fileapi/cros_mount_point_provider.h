  // Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_CHROMEOS_FILEAPI_CROS_MOUNT_POINT_PROVIDER_H_
#define WEBKIT_CHROMEOS_FILEAPI_CROS_MOUNT_POINT_PROVIDER_H_

#include <map>
#include <string>
#include <vector>

#include "base/file_path.h"
#include "webkit/fileapi/file_system_mount_point_provider.h"
#include "webkit/quota/special_storage_policy.h"

namespace chromeos {

class FileAccessPermissions;

// An interface to provide local filesystem paths.
class CrosMountPointProvider
    : public fileapi::ExternalFileSystemMountPointProvider {
 public:
  CrosMountPointProvider(
      scoped_refptr<quota::SpecialStoragePolicy> special_storage_policy);
  virtual ~CrosMountPointProvider();

  // fileapi::FileSystemMountPointProvider overrides.
  virtual bool IsAccessAllowed(const GURL& origin_url,
                               fileapi::FileSystemType type,
                               const FilePath& virtual_path) OVERRIDE;
  virtual void GetFileSystemRootPath(
      const GURL& origin_url,
      fileapi::FileSystemType type,
      bool create,
      fileapi::FileSystemPathManager::GetRootPathCallback* callback) OVERRIDE;
  virtual FilePath GetFileSystemRootPathOnFileThread(
      const GURL& origin_url,
      fileapi::FileSystemType type,
      const FilePath& virtual_path,
      bool create);
  virtual bool IsRestrictedFileName(const FilePath& filename) const OVERRIDE;
  virtual std::vector<FilePath> GetRootDirectories() const OVERRIDE;

  // fileapi::ExternalFileSystemMountPointProvider overrides.
  virtual void GrantFullAccessToExtension(
      const std::string& extension_id) OVERRIDE;
  virtual void GrantFileAccessToExtension(
      const std::string& extension_id, const FilePath& virtual_path) OVERRIDE;
  void RevokeAccessForExtension(const std::string& extension_id) OVERRIDE;

 private:
  class GetFileSystemRootPathTask;
  typedef std::map<std::string, FilePath> MountPointMap;

  MountPointMap mount_point_map_;
  scoped_refptr<quota::SpecialStoragePolicy> special_storage_policy_;
  scoped_ptr<FileAccessPermissions> file_access_permissions_;
  DISALLOW_COPY_AND_ASSIGN(CrosMountPointProvider);
};

}  // namespace chromeos

#endif  // WEBKIT_CHROMEOS_FILEAPI_CROS_MOUNT_POINT_PROVIDER_H_
