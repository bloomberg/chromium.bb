// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_LOCAL_FILE_UTIL_H_
#define WEBKIT_FILEAPI_LOCAL_FILE_UTIL_H_

#include <vector>

#include "base/callback_forward.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/file_util_proxy.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/platform_file.h"
#include "webkit/fileapi/file_system_file_util.h"
#include "webkit/fileapi/file_system_types.h"

namespace base {
struct PlatformFileInfo;
class Time;
}

class GURL;

namespace fileapi {

using base::PlatformFile;
using base::PlatformFileError;

class FileSystemOperationContext;
class FileSystemPath;

// An instance of this class is created and owned by *MountPointProvider.
class LocalFileUtil : public FileSystemFileUtil {
 public:
  // |underlying_file_util| is owned by the instance.  It will be deleted by
  // the owner instance.  For example, it can be instanciated as follows:
  // FileSystemFileUtil* file_util = new LocalFileUtil(new NativeFileUtil());
  explicit LocalFileUtil(FileSystemFileUtil* underlying_file_util);
  virtual ~LocalFileUtil();

  virtual PlatformFileError CreateOrOpen(
      FileSystemOperationContext* context,
      const FileSystemPath& path,
      int file_flags,
      PlatformFile* file_handle,
      bool* created) OVERRIDE;
  virtual PlatformFileError EnsureFileExists(
      FileSystemOperationContext* context,
      const FileSystemPath& path, bool* created) OVERRIDE;
  virtual PlatformFileError CreateDirectory(
      FileSystemOperationContext* context,
      const FileSystemPath& path,
      bool exclusive,
      bool recursive) OVERRIDE;
  virtual PlatformFileError GetFileInfo(
      FileSystemOperationContext* context,
      const FileSystemPath& path,
      base::PlatformFileInfo* file_info,
      FilePath* platform_file) OVERRIDE;
  virtual AbstractFileEnumerator* CreateFileEnumerator(
      FileSystemOperationContext* context,
      const FileSystemPath& root_path,
      bool recursive) OVERRIDE;
  virtual PlatformFileError GetLocalFilePath(
      FileSystemOperationContext* context,
      const FileSystemPath& file_system_path,
      FilePath* local_file_path) OVERRIDE;
  virtual PlatformFileError Touch(
      FileSystemOperationContext* context,
      const FileSystemPath& path,
      const base::Time& last_access_time,
      const base::Time& last_modified_time) OVERRIDE;
  virtual PlatformFileError Truncate(
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
  virtual PlatformFileError CopyOrMoveFile(
      FileSystemOperationContext* context,
      const FileSystemPath& src_path,
      const FileSystemPath& dest_path,
      bool copy) OVERRIDE;
  virtual PlatformFileError CopyInForeignFile(
        FileSystemOperationContext* context,
        const FilePath& src_file_path,
        const FileSystemPath& dest_path) OVERRIDE;
  virtual PlatformFileError DeleteFile(
      FileSystemOperationContext* context,
      const FileSystemPath& path) OVERRIDE;
  virtual PlatformFileError DeleteSingleDirectory(
      FileSystemOperationContext* context,
      const FileSystemPath& path) OVERRIDE;

 private:
  // Given the filesystem path, produces a real, full local path for the
  // underlying filesystem (which is usually the native filesystem).
  FileSystemPath GetLocalPath(
      FileSystemOperationContext* context,
      const FileSystemPath& path);

  DISALLOW_COPY_AND_ASSIGN(LocalFileUtil);
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_LOCAL_FILE_UTIL_H_
