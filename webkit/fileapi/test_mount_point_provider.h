// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_TEST_MOUNT_POINT_PROVIDER_H_
#define WEBKIT_FILEAPI_TEST_MOUNT_POINT_PROVIDER_H_

#include "base/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "webkit/fileapi/file_system_mount_point_provider.h"
#include "webkit/fileapi/fileapi_export.h"
#include "webkit/fileapi/task_runner_bound_observer_list.h"

namespace base {
class SequencedTaskRunner;
}

namespace fileapi {

class LocalFileUtil;
class FileSystemQuotaUtil;

// This should be only used for testing.
// This mount point provider uses LocalFileUtil and stores data file
// under the given directory.
class FILEAPI_EXPORT_PRIVATE TestMountPointProvider
    : public FileSystemMountPointProvider {
 public:
  using FileSystemMountPointProvider::ValidateFileSystemCallback;
  using FileSystemMountPointProvider::DeleteFileSystemCallback;

  TestMountPointProvider(
      base::SequencedTaskRunner* task_runner,
      const FilePath& base_path);
  virtual ~TestMountPointProvider();

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

  const UpdateObserverList* GetUpdateObservers(FileSystemType type) const;

 private:
  class QuotaUtil;

  FilePath base_path_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  scoped_ptr<LocalFileUtil> local_file_util_;
  scoped_ptr<QuotaUtil> quota_util_;
  UpdateObserverList observers_;
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_TEST_MOUNT_POINT_PROVIDER_H_
