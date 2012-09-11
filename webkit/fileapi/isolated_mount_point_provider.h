// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_ISOLATED_MOUNT_POINT_PROVIDER_H_
#define WEBKIT_FILEAPI_ISOLATED_MOUNT_POINT_PROVIDER_H_

#include "base/memory/scoped_ptr.h"
#include "webkit/fileapi/file_system_mount_point_provider.h"
#include "webkit/fileapi/media/media_file_system_config.h"

namespace fileapi {

class DraggedFileUtil;
class IsolatedContext;
class IsolatedFileUtil;
class MediaPathFilter;
class NativeMediaFileUtil;

#if defined(SUPPORT_MEDIA_FILESYSTEM)
class DeviceMediaFileUtil;
#endif

class IsolatedMountPointProvider : public FileSystemMountPointProvider {
 public:
  using FileSystemMountPointProvider::ValidateFileSystemCallback;
  using FileSystemMountPointProvider::DeleteFileSystemCallback;

  explicit IsolatedMountPointProvider(const FilePath& profile_path);
  virtual ~IsolatedMountPointProvider();

  // FileSystemMountPointProvider implementation.
  virtual void ValidateFileSystemRoot(
      const GURL& origin_url,
      FileSystemType type,
      bool create,
      const ValidateFileSystemCallback& callback) OVERRIDE;
  virtual FilePath GetFileSystemRootPathOnFileThread(
      const GURL& origin_url,
      FileSystemType type,
      const FilePath& virtual_path,
      bool create) OVERRIDE;
  virtual bool IsAccessAllowed(const FileSystemURL& url) OVERRIDE;
  virtual bool IsRestrictedFileName(const FilePath& filename) const OVERRIDE;
  virtual FileSystemFileUtil* GetFileUtil(FileSystemType type) OVERRIDE;
  virtual FilePath GetPathForPermissionsCheck(const FilePath& virtual_path)
      const OVERRIDE;
  virtual FileSystemOperation* CreateFileSystemOperation(
      const FileSystemURL& url,
      FileSystemContext* context,
      base::PlatformFileError* error_code) const OVERRIDE;
  virtual webkit_blob::FileStreamReader* CreateFileStreamReader(
      const FileSystemURL& url,
      int64 offset,
      FileSystemContext* context) const OVERRIDE;
  virtual FileStreamWriter* CreateFileStreamWriter(
      const FileSystemURL& url,
      int64 offset,
      FileSystemContext* context) const OVERRIDE;
  virtual FileSystemQuotaUtil* GetQuotaUtil() OVERRIDE;
  virtual void DeleteFileSystem(
      const GURL& origin_url,
      FileSystemType type,
      FileSystemContext* context,
      const DeleteFileSystemCallback& callback) OVERRIDE;

 private:
  // Store the profile path. We need this to create temporary snapshot files.
  const FilePath profile_path_;

  scoped_ptr<MediaPathFilter> media_path_filter_;

  scoped_ptr<IsolatedFileUtil> isolated_file_util_;
  scoped_ptr<DraggedFileUtil> dragged_file_util_;
  scoped_ptr<NativeMediaFileUtil> native_media_file_util_;

#if defined(SUPPORT_MEDIA_FILESYSTEM)
  scoped_ptr<DeviceMediaFileUtil> device_media_file_util_;
#endif
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_ISOLATED_MOUNT_POINT_PROVIDER_H_
