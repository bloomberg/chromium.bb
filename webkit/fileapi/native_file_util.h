// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_NATIVE_FILE_UTIL_H_
#define WEBKIT_FILEAPI_NATIVE_FILE_UTIL_H_

#include "base/file_path.h"
#include "base/file_util_proxy.h"
#include "base/platform_file.h"
#include "webkit/fileapi/file_system_file_util.h"

namespace base {
class Time;
}

namespace fileapi {

using base::PlatformFile;
using base::PlatformFileError;
class FileSystemOperationContext;

// TODO(dmikurube): Add unit tests for NativeFileUtil.
// This class handles accessing the OS native filesystem.
class NativeFileUtil : public FileSystemFileUtil {
 public:
  NativeFileUtil() {}
  virtual ~NativeFileUtil() {}

  virtual PlatformFileError CreateOrOpen(
      FileSystemOperationContext* unused,
      const FileSystemPath& path,
      int file_flags,
      PlatformFile* file_handle,
      bool* created) OVERRIDE;
  virtual PlatformFileError Close(
      FileSystemOperationContext* unused,
      PlatformFile) OVERRIDE;
  virtual PlatformFileError EnsureFileExists(
      FileSystemOperationContext* unused,
      const FileSystemPath& path, bool* created) OVERRIDE;
  virtual PlatformFileError CreateDirectory(
      FileSystemOperationContext* context,
      const FileSystemPath& path,
      bool exclusive,
      bool recursive) OVERRIDE;
  virtual PlatformFileError GetFileInfo(
      FileSystemOperationContext* unused,
      const FileSystemPath& path,
      base::PlatformFileInfo* file_info,
      FilePath* platform_file_path) OVERRIDE;
  virtual AbstractFileEnumerator* CreateFileEnumerator(
      FileSystemOperationContext* unused,
      const FileSystemPath& root_path,
      bool recursive) OVERRIDE;
  virtual PlatformFileError GetLocalFilePath(
      FileSystemOperationContext* unused,
      const FileSystemPath& file_system_path,
      FilePath* local_file_path) OVERRIDE;
  virtual PlatformFileError Touch(
      FileSystemOperationContext* unused,
      const FileSystemPath& path,
      const base::Time& last_access_time,
      const base::Time& last_modified_time) OVERRIDE;
  virtual PlatformFileError Truncate(
      FileSystemOperationContext* unused,
      const FileSystemPath& path,
      int64 length) OVERRIDE;
  virtual bool PathExists(
      FileSystemOperationContext* unused,
      const FileSystemPath& path) OVERRIDE;
  virtual bool DirectoryExists(
      FileSystemOperationContext* unused,
      const FileSystemPath& path) OVERRIDE;
  virtual bool IsDirectoryEmpty(
      FileSystemOperationContext* unused,
      const FileSystemPath& path) OVERRIDE;
  virtual PlatformFileError CopyOrMoveFile(
      FileSystemOperationContext* unused,
      const FileSystemPath& src_path,
      const FileSystemPath& dest_path,
      bool copy) OVERRIDE;
  virtual PlatformFileError CopyInForeignFile(
        FileSystemOperationContext* unused,
        const FilePath& src_file_path,
        const FileSystemPath& dest_path) OVERRIDE;
  virtual PlatformFileError DeleteFile(
      FileSystemOperationContext* unused,
      const FileSystemPath& path) OVERRIDE;
  virtual PlatformFileError DeleteSingleDirectory(
      FileSystemOperationContext* unused,
      const FileSystemPath& path) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(NativeFileUtil);
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_NATIVE_FILE_UTIL_H_
