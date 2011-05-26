// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_LOCAL_FILE_SYSTEM_FILE_UTIL_H_
#define WEBKIT_FILEAPI_LOCAL_FILE_SYSTEM_FILE_UTIL_H_

#include <vector>

#include "base/callback.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/file_util_proxy.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/singleton.h"
#include "base/platform_file.h"
#include "base/tracked_objects.h"
#include "webkit/fileapi/file_system_file_util.h"
#include "webkit/fileapi/file_system_types.h"

namespace base {
struct PlatformFileInfo;
class MessageLoopProxy;
class Time;
}

class GURL;

namespace fileapi {

using base::PlatformFile;
using base::PlatformFileError;

class FileSystemOperationContext;

class LocalFileSystemFileUtil : public FileSystemFileUtil {
 public:
  static LocalFileSystemFileUtil* GetInstance();

  virtual PlatformFileError CreateOrOpen(
      FileSystemOperationContext* context,
      const FilePath& file_path,
      int file_flags,
      PlatformFile* file_handle,
      bool* created);

  virtual PlatformFileError EnsureFileExists(
      FileSystemOperationContext* context,
      const FilePath& file_path, bool* created);

  virtual PlatformFileError GetLocalFilePath(
      FileSystemOperationContext* context,
      const FilePath& virtual_file,
      FilePath* local_path);

  virtual PlatformFileError GetFileInfo(
      FileSystemOperationContext* context,
      const FilePath& file,
      base::PlatformFileInfo* file_info,
      FilePath* platform_file);

  virtual PlatformFileError ReadDirectory(
      FileSystemOperationContext* context,
      const FilePath& file_path,
      std::vector<base::FileUtilProxy::Entry>* entries);

  virtual PlatformFileError CreateDirectory(
      FileSystemOperationContext* context,
      const FilePath& file_path,
      bool exclusive,
      bool recursive);

  virtual PlatformFileError CopyOrMoveFile(
      FileSystemOperationContext* context,
      const FilePath& src_file_path,
      const FilePath& dest_file_path,
      bool copy);

  virtual PlatformFileError CopyInForeignFile(
        FileSystemOperationContext* context,
        const FilePath& src_file_path,
        const FilePath& dest_file_path);

  virtual PlatformFileError DeleteFile(
      FileSystemOperationContext* context,
      const FilePath& file_path);

  virtual PlatformFileError DeleteSingleDirectory(
      FileSystemOperationContext* context,
      const FilePath& file_path);

  virtual PlatformFileError Touch(
      FileSystemOperationContext* context,
      const FilePath& file_path,
      const base::Time& last_access_time,
      const base::Time& last_modified_time);

  virtual PlatformFileError Truncate(
      FileSystemOperationContext* context,
      const FilePath& path,
      int64 length);

  virtual bool PathExists(
      FileSystemOperationContext* context,
      const FilePath& file_path);

  virtual bool DirectoryExists(
      FileSystemOperationContext* context,
      const FilePath& file_path);

  virtual bool IsDirectoryEmpty(
      FileSystemOperationContext* context,
      const FilePath& file_path);

 protected:
  LocalFileSystemFileUtil() { }

  virtual AbstractFileEnumerator* CreateFileEnumerator(
      FileSystemOperationContext* context,
      const FilePath& root_path);

  friend struct DefaultSingletonTraits<LocalFileSystemFileUtil>;
  DISALLOW_COPY_AND_ASSIGN(LocalFileSystemFileUtil);

 private:
  // Given the filesystem's root URL and a virtual path, produces a real, full
  // local path.
  FilePath GetLocalPath(
      FileSystemOperationContext* context,
      const GURL& origin_url,
      FileSystemType type,
      const FilePath& virtual_path);
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_LOCAL_FILE_SYSTEM_FILE_UTIL_H_
