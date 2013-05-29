// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_BROWSER_FILEAPI_ISOLATED_MOUNT_POINT_PROVIDER_H_
#define WEBKIT_BROWSER_FILEAPI_ISOLATED_MOUNT_POINT_PROVIDER_H_

#include "base/memory/scoped_ptr.h"
#include "webkit/browser/fileapi/file_system_mount_point_provider.h"

namespace fileapi {

class AsyncFileUtilAdapter;

class IsolatedMountPointProvider : public FileSystemMountPointProvider {
 public:
  IsolatedMountPointProvider();
  virtual ~IsolatedMountPointProvider();

  // FileSystemMountPointProvider implementation.
  virtual bool CanHandleType(FileSystemType type) const OVERRIDE;
  virtual void OpenFileSystem(
      const GURL& origin_url,
      FileSystemType type,
      OpenFileSystemMode mode,
      const OpenFileSystemCallback& callback) OVERRIDE;
  virtual FileSystemFileUtil* GetFileUtil(FileSystemType type) OVERRIDE;
  virtual AsyncFileUtil* GetAsyncFileUtil(FileSystemType type) OVERRIDE;
  virtual CopyOrMoveFileValidatorFactory* GetCopyOrMoveFileValidatorFactory(
      FileSystemType type,
      base::PlatformFileError* error_code) OVERRIDE;
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
  scoped_ptr<AsyncFileUtilAdapter> transient_file_util_;
};

}  // namespace fileapi

#endif  // WEBKIT_BROWSER_FILEAPI_ISOLATED_MOUNT_POINT_PROVIDER_H_
