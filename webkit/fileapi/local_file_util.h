// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
      const FilePath& file_path,
      int file_flags,
      PlatformFile* file_handle,
      bool* created) OVERRIDE;
  virtual PlatformFileError EnsureFileExists(
      FileSystemOperationContext* context,
      const FilePath& file_path, bool* created) OVERRIDE;
  virtual PlatformFileError CreateDirectory(
      FileSystemOperationContext* context,
      const FilePath& file_path,
      bool exclusive,
      bool recursive) OVERRIDE;
  virtual PlatformFileError GetFileInfo(
      FileSystemOperationContext* context,
      const FilePath& file,
      base::PlatformFileInfo* file_info,
      FilePath* platform_file) OVERRIDE;
  virtual PlatformFileError ReadDirectory(
      FileSystemOperationContext* context,
      const FilePath& file_path,
      std::vector<base::FileUtilProxy::Entry>* entries) OVERRIDE;
  virtual AbstractFileEnumerator* CreateFileEnumerator(
      FileSystemOperationContext* context,
      const FilePath& root_path) OVERRIDE;
  virtual PlatformFileError GetLocalFilePath(
      FileSystemOperationContext* context,
      const FilePath& virtual_file,
      FilePath* local_path) OVERRIDE;
  virtual PlatformFileError Touch(
      FileSystemOperationContext* context,
      const FilePath& file_path,
      const base::Time& last_access_time,
      const base::Time& last_modified_time) OVERRIDE;
  virtual PlatformFileError Truncate(
      FileSystemOperationContext* context,
      const FilePath& path,
      int64 length) OVERRIDE;
  virtual bool PathExists(
      FileSystemOperationContext* context,
      const FilePath& file_path) OVERRIDE;
  virtual bool DirectoryExists(
      FileSystemOperationContext* context,
      const FilePath& file_path) OVERRIDE;
  virtual bool IsDirectoryEmpty(
      FileSystemOperationContext* context,
      const FilePath& file_path) OVERRIDE;
  virtual PlatformFileError CopyOrMoveFile(
      FileSystemOperationContext* context,
      const FilePath& src_file_path,
      const FilePath& dest_file_path,
      bool copy) OVERRIDE;
  virtual PlatformFileError CopyInForeignFile(
        FileSystemOperationContext* context,
        const FilePath& src_file_path,
        const FilePath& dest_file_path) OVERRIDE;
  virtual PlatformFileError DeleteFile(
      FileSystemOperationContext* context,
      const FilePath& file_path) OVERRIDE;
  virtual PlatformFileError DeleteSingleDirectory(
      FileSystemOperationContext* context,
      const FilePath& file_path) OVERRIDE;

 private:
  // Given the filesystem's root URL and a virtual path, produces a real, full
  // local path.
  FilePath GetLocalPath(
      FileSystemOperationContext* context,
      const GURL& origin_url,
      FileSystemType type,
      const FilePath& virtual_path);

  DISALLOW_COPY_AND_ASSIGN(LocalFileUtil);
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_LOCAL_FILE_UTIL_H_
