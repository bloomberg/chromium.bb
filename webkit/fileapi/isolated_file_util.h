// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_ISOLATED_FILE_UTIL_H_
#define WEBKIT_FILEAPI_ISOLATED_FILE_UTIL_H_

#include "base/file_path.h"
#include "base/file_util_proxy.h"
#include "base/platform_file.h"
#include "webkit/fileapi/fileapi_export.h"
#include "webkit/fileapi/file_system_file_util.h"

namespace base {
class Time;
}

namespace fileapi {

class FileSystemOperationContext;
class IsolatedContext;

class FILEAPI_EXPORT_PRIVATE IsolatedFileUtil : public FileSystemFileUtil {
 public:
  IsolatedFileUtil();
  virtual ~IsolatedFileUtil() {}

  // FileSystemFileUtil overrides.
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
      FilePath* platform_path) OVERRIDE;
  virtual AbstractFileEnumerator* CreateFileEnumerator(
      FileSystemOperationContext* context,
      const FileSystemURL& root_url,
      bool recursive) OVERRIDE;
  virtual PlatformFileError GetLocalFilePath(
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
  virtual bool PathExists(
      FileSystemOperationContext* context,
      const FileSystemURL& url) OVERRIDE;
  virtual bool DirectoryExists(
      FileSystemOperationContext* context,
      const FileSystemURL& url) OVERRIDE;
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

 private:
  // Returns false if the given |url| is not a valid path.
  bool GetPlatformPath(const FileSystemURL& url,
                       FilePath* platform_path) const;

  // Returns false if the given |url| is not a valid path, or
  // the file system is not writable.
  bool GetPlatformPathForWrite(const FileSystemURL& url,
                               FilePath* platform_path) const;

  DISALLOW_COPY_AND_ASSIGN(IsolatedFileUtil);
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_ISOLATED_FILE_UTIL_H_
