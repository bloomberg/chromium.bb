// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_ISOLATED_MOUNT_POINT_PROVIDER_H_
#define WEBKIT_FILEAPI_ISOLATED_MOUNT_POINT_PROVIDER_H_

#include "base/memory/scoped_ptr.h"
#include "webkit/fileapi/file_system_mount_point_provider.h"

namespace fileapi {

class AsyncFileUtilAdapter;

class IsolatedMountPointProvider : public FileSystemMountPointProvider {
 public:
  IsolatedMountPointProvider();
  virtual ~IsolatedMountPointProvider();

  // FileSystemMountPointProvider implementation.
  virtual bool CanHandleType(FileSystemType type) const OVERRIDE;
  virtual void ValidateFileSystemRoot(
      const GURL& origin_url,
      FileSystemType type,
      bool create,
      const ValidateFileSystemCallback& callback) OVERRIDE;
  virtual base::FilePath GetFileSystemRootPathOnFileThread(
      const FileSystemURL& url,
      bool create) OVERRIDE;
  virtual FileSystemFileUtil* GetFileUtil(FileSystemType type) OVERRIDE;
  virtual AsyncFileUtil* GetAsyncFileUtil(FileSystemType type) OVERRIDE;
  virtual CopyOrMoveFileValidatorFactory* GetCopyOrMoveFileValidatorFactory(
      FileSystemType type,
      base::PlatformFileError* error_code) OVERRIDE;
  virtual void InitializeCopyOrMoveFileValidatorFactory(
      FileSystemType type,
      scoped_ptr<CopyOrMoveFileValidatorFactory> factory) OVERRIDE;
  virtual FilePermissionPolicy GetPermissionPolicy(
      const FileSystemURL& url,
      int permissions) const OVERRIDE;
  virtual FileSystemOperation* CreateFileSystemOperation(
      const FileSystemURL& url,
      FileSystemContext* context,
      base::PlatformFileError* error_code) const OVERRIDE;
  virtual scoped_ptr<webkit_blob::FileStreamReader> CreateFileStreamReader(
      const FileSystemURL& url,
      int64 offset,
      const base::Time& expected_modification_time,
      FileSystemContext* context) const OVERRIDE;
  virtual scoped_ptr<FileStreamWriter> CreateFileStreamWriter(
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
  scoped_ptr<AsyncFileUtilAdapter> isolated_file_util_;
  scoped_ptr<AsyncFileUtilAdapter> dragged_file_util_;
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_ISOLATED_MOUNT_POINT_PROVIDER_H_
