  // Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_CHROMEOS_FILEAPI_CROS_MOUNT_POINT_PROVIDER_H_
#define WEBKIT_CHROMEOS_FILEAPI_CROS_MOUNT_POINT_PROVIDER_H_

#include <map>
#include <set>
#include <string>

#include "webkit/fileapi/file_system_mount_point_provider.h"
#include "webkit/quota/special_storage_policy.h"

namespace chromeos {

// An interface to provide local filesystem paths.
class CrosMountPointProvider
    : public fileapi::FileSystemMountPointProvider {
 public:
  CrosMountPointProvider(
      scoped_refptr<quota::SpecialStoragePolicy> special_storage_policy);
  virtual ~CrosMountPointProvider();

  // fileapi::FileSystemMountPointProvider overrides.
  virtual bool IsAccessAllowed(const GURL& origin_url) OVERRIDE;

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

 private:
  class GetFileSystemRootPathTask;
  typedef std::map<std::string, std::string> MountPointMap;

  static const char kLocalDirName[];
  static const char kLocalName[];
  FilePath virtual_root_path_;
  FilePath base_path_;
  std::string local_dir_name_;
  MountPointMap mount_point_map_;
  scoped_refptr<quota::SpecialStoragePolicy> special_storage_policy_;

  DISALLOW_COPY_AND_ASSIGN(CrosMountPointProvider);
};

}  // namespace chromeos

#endif  // WEBKIT_CHROMEOS_FILEAPI_CROS_MOUNT_POINT_PROVIDER_H_
