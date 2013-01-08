// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_LOCAL_FILE_UTIL_H_
#define WEBKIT_FILEAPI_LOCAL_FILE_UTIL_H_

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/platform_file.h"
#include "webkit/fileapi/file_system_file_util.h"
#include "webkit/storage/webkit_storage_export.h"

namespace base {
class Time;
}

class FilePath;
class GURL;

namespace fileapi {

class FileSystemOperationContext;
class FileSystemURL;

// An instance of this class is created and owned by *MountPointProvider.
class WEBKIT_STORAGE_EXPORT_PRIVATE LocalFileUtil : public FileSystemFileUtil {
 public:
  LocalFileUtil();
  virtual ~LocalFileUtil();

  virtual base::PlatformFileError CreateOrOpen(
      FileSystemOperationContext* context,
      const FileSystemURL& url,
      int file_flags,
      base::PlatformFile* file_handle,
      bool* created) OVERRIDE;
  virtual base::PlatformFileError Close(
      FileSystemOperationContext* context,
      base::PlatformFile file) OVERRIDE;
  virtual base::PlatformFileError EnsureFileExists(
      FileSystemOperationContext* context,
      const FileSystemURL& url, bool* created) OVERRIDE;
  virtual base::PlatformFileError CreateDirectory(
      FileSystemOperationContext* context,
      const FileSystemURL& url,
      bool exclusive,
      bool recursive) OVERRIDE;
  virtual base::PlatformFileError GetFileInfo(
      FileSystemOperationContext* context,
      const FileSystemURL& url,
      base::PlatformFileInfo* file_info,
      FilePath* platform_file) OVERRIDE;
  virtual scoped_ptr<AbstractFileEnumerator> CreateFileEnumerator(
      FileSystemOperationContext* context,
      const FileSystemURL& root_url,
      bool recursive) OVERRIDE;
  virtual base::PlatformFileError GetLocalFilePath(
      FileSystemOperationContext* context,
      const FileSystemURL& file_system_url,
      FilePath* local_file_path) OVERRIDE;
  virtual base::PlatformFileError Touch(
      FileSystemOperationContext* context,
      const FileSystemURL& url,
      const base::Time& last_access_time,
      const base::Time& last_modified_time) OVERRIDE;
  virtual base::PlatformFileError Truncate(
      FileSystemOperationContext* context,
      const FileSystemURL& url,
      int64 length) OVERRIDE;
  virtual bool IsDirectoryEmpty(
      FileSystemOperationContext* context,
      const FileSystemURL& url) OVERRIDE;
  virtual base::PlatformFileError CopyOrMoveFile(
      FileSystemOperationContext* context,
      const FileSystemURL& src_url,
      const FileSystemURL& dest_url,
      bool copy) OVERRIDE;
  virtual base::PlatformFileError CopyInForeignFile(
      FileSystemOperationContext* context,
      const FilePath& src_file_path,
      const FileSystemURL& dest_url) OVERRIDE;
  virtual base::PlatformFileError DeleteFile(
      FileSystemOperationContext* context,
      const FileSystemURL& url) OVERRIDE;
  virtual base::PlatformFileError DeleteSingleDirectory(
      FileSystemOperationContext* context,
      const FileSystemURL& url) OVERRIDE;
  virtual base::PlatformFileError CreateSnapshotFile(
      FileSystemOperationContext* context,
      const FileSystemURL& url,
      base::PlatformFileInfo* file_info,
      FilePath* platform_path,
      SnapshotFilePolicy* snapshot_policy) OVERRIDE;

 private:
  // Given the filesystem url, produces a real, full local path for the
  // underlying filesystem (which is usually the native filesystem).
  FileSystemURL GetLocalPath(
      FileSystemOperationContext* context,
      const FileSystemURL& url);

  DISALLOW_COPY_AND_ASSIGN(LocalFileUtil);
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_LOCAL_FILE_UTIL_H_
