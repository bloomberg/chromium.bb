// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_TEST_MOUNT_POINT_PROVIDER_H_
#define WEBKIT_FILEAPI_TEST_MOUNT_POINT_PROVIDER_H_
#pragma once

#include "base/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "webkit/fileapi/file_system_mount_point_provider.h"

namespace base {
class SequencedTaskRunner;
}

namespace fileapi {

class FileSystemQuotaUtil;

// This should be only used for testing.
// This mount point provider uses LocalFileUtil and stores data file
// under the given directory.
class TestMountPointProvider : public FileSystemMountPointProvider {
 public:
  typedef FileSystemMountPointProvider::ValidateFileSystemCallback
      ValidateFileSystemCallback;

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
  virtual bool IsAccessAllowed(const GURL& origin_url,
                               FileSystemType type,
                               const FilePath& virtual_path) OVERRIDE;
  virtual bool IsRestrictedFileName(const FilePath& filename) const OVERRIDE;
  virtual std::vector<FilePath> GetRootDirectories() const OVERRIDE;
  virtual FileSystemFileUtil* GetFileUtil() OVERRIDE;
  virtual FilePath GetPathForPermissionsCheck(const FilePath& virtual_path)
      const OVERRIDE;
  virtual FileSystemOperationInterface* CreateFileSystemOperation(
      const GURL& origin_url,
      FileSystemType file_system_type,
      const FilePath& virtual_path,
      FileSystemContext* context) const OVERRIDE;
  virtual webkit_blob::FileReader* CreateFileReader(
    const GURL& url,
    int64 offset,
    FileSystemContext* context) const OVERRIDE;
  virtual FileSystemQuotaUtil* GetQuotaUtil() OVERRIDE;

 private:
  FilePath base_path_;
  scoped_ptr<FileSystemFileUtil> local_file_util_;
  scoped_ptr<FileSystemQuotaUtil> quota_util_;
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_TEST_MOUNT_POINT_PROVIDER_H_
