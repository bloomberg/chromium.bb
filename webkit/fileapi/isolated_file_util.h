// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_ISOLATED_FILE_UTIL_H_
#define WEBKIT_FILEAPI_ISOLATED_FILE_UTIL_H_

#include "base/file_path.h"
#include "base/file_util_proxy.h"
#include "base/platform_file.h"
#include "webkit/fileapi/file_system_file_util.h"

namespace base {
class Time;
}

namespace fileapi {

class FileSystemOperationContext;
class IsolatedContext;

class IsolatedFileUtil : public FileSystemFileUtil {
 public:
  explicit IsolatedFileUtil(FileSystemFileUtil* underlying_file_util);
  virtual ~IsolatedFileUtil() {}

  // FileSystemFileUtil overrides.
  virtual base::PlatformFileError CreateOrOpen(
      FileSystemOperationContext* context,
      const FileSystemPath& path,
      int file_flags,
      base::PlatformFile* file_handle,
      bool* created) OVERRIDE;
  virtual base::PlatformFileError Close(
      FileSystemOperationContext* context,
      base::PlatformFile) OVERRIDE;
  virtual base::PlatformFileError EnsureFileExists(
      FileSystemOperationContext* context,
      const FileSystemPath& path, bool* created) OVERRIDE;
  virtual base::PlatformFileError CreateDirectory(
      FileSystemOperationContext* context,
      const FileSystemPath& path,
      bool exclusive,
      bool recursive) OVERRIDE;
  virtual base::PlatformFileError GetFileInfo(
      FileSystemOperationContext* context,
      const FileSystemPath& path,
      base::PlatformFileInfo* file_info,
      FilePath* platform_path) OVERRIDE;
  virtual AbstractFileEnumerator* CreateFileEnumerator(
      FileSystemOperationContext* context,
      const FileSystemPath& root_path,
      bool recursive) OVERRIDE;
  virtual base::PlatformFileError Touch(
      FileSystemOperationContext* context,
      const FileSystemPath& path,
      const base::Time& last_access_time,
      const base::Time& last_modified_time) OVERRIDE;
  virtual base::PlatformFileError Truncate(
      FileSystemOperationContext* context,
      const FileSystemPath& path,
      int64 length) OVERRIDE;
  virtual bool PathExists(
      FileSystemOperationContext* context,
      const FileSystemPath& path) OVERRIDE;
  virtual bool DirectoryExists(
      FileSystemOperationContext* context,
      const FileSystemPath& path) OVERRIDE;
  virtual bool IsDirectoryEmpty(
      FileSystemOperationContext* context,
      const FileSystemPath& path) OVERRIDE;
  virtual base::PlatformFileError CopyOrMoveFile(
      FileSystemOperationContext* context,
      const FileSystemPath& src_path,
      const FileSystemPath& dest_path,
      bool copy) OVERRIDE;
  virtual base::PlatformFileError CopyInForeignFile(
        FileSystemOperationContext* context,
        const FilePath& src_file_path,
        const FileSystemPath& dest_path) OVERRIDE;
  virtual base::PlatformFileError DeleteFile(
      FileSystemOperationContext* context,
      const FileSystemPath& path) OVERRIDE;
  virtual base::PlatformFileError DeleteSingleDirectory(
      FileSystemOperationContext* context,
      const FileSystemPath& path) OVERRIDE;

 private:
  // Returns false if the given |virtual_path| is not a valid path.
  bool GetPlatformPath(const FileSystemPath& virtual_path,
                       FilePath* platform_path) const;

  DISALLOW_COPY_AND_ASSIGN(IsolatedFileUtil);
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_ISOLATED_FILE_UTIL_H_
