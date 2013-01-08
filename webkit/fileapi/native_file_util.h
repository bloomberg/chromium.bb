// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_NATIVE_FILE_UTIL_H_
#define WEBKIT_FILEAPI_NATIVE_FILE_UTIL_H_

#include "base/file_path.h"
#include "base/file_util_proxy.h"
#include "base/memory/scoped_ptr.h"
#include "base/platform_file.h"
#include "webkit/fileapi/file_system_file_util.h"
#include "webkit/storage/webkit_storage_export.h"

namespace base {
class Time;
}

namespace fileapi {

// TODO(dmikurube): Add unit tests for NativeFileUtil.
// This class handles accessing the OS native filesystem.
class WEBKIT_STORAGE_EXPORT_PRIVATE NativeFileUtil {
 public:
  static base::PlatformFileError CreateOrOpen(
      const FilePath& path,
      int file_flags,
      base::PlatformFile* file_handle,
      bool* created);
  static base::PlatformFileError Close(base::PlatformFile file);
  static base::PlatformFileError EnsureFileExists(const FilePath& path,
                                                  bool* created);
  static base::PlatformFileError CreateDirectory(const FilePath& path,
                                                 bool exclusive,
                                                 bool recursive);
  static base::PlatformFileError GetFileInfo(const FilePath& path,
                                             base::PlatformFileInfo* file_info);
  static scoped_ptr<FileSystemFileUtil::AbstractFileEnumerator>
      CreateFileEnumerator(const FilePath& root_path,
                           bool recursive);
  static base::PlatformFileError Touch(const FilePath& path,
                                       const base::Time& last_access_time,
                                       const base::Time& last_modified_time);
  static base::PlatformFileError Truncate(const FilePath& path, int64 length);
  static bool PathExists(const FilePath& path);
  static bool DirectoryExists(const FilePath& path);
  static bool IsDirectoryEmpty(const FilePath& path);
  static base::PlatformFileError CopyOrMoveFile(const FilePath& src_path,
                                                const FilePath& dest_path,
                                                bool copy);
  static base::PlatformFileError DeleteFile(const FilePath& path);
  static base::PlatformFileError DeleteSingleDirectory(const FilePath& path);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(NativeFileUtil);
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_NATIVE_FILE_UTIL_H_
