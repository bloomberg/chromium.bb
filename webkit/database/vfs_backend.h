// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_DATABASE_VFS_BACKEND_H_
#define WEBKIT_DATABASE_VFS_BACKEND_H_

#include "base/platform_file.h"
#include "base/process.h"
#include "base/string16.h"

class FilePath;

namespace webkit_database {

class VfsBackend {
 public:
  static void OpenFile(const FilePath& file_path,
                       int desired_flags,
                       base::ProcessHandle handle,
                       base::PlatformFile* target_handle,
                       base::PlatformFile* target_dir_handle);
  static void OpenTempFileInDirectory(const FilePath& dir_path,
                                      int desired_flags,
                                      base::ProcessHandle handle,
                                      base::PlatformFile* target_handle,
                                      base::PlatformFile* target_dir_handle);

  static int DeleteFile(const FilePath& file_path, bool sync_dir);

  static uint32 GetFileAttributes(const FilePath& file_path);

  static int64 GetFileSize(const FilePath& file_path);

 private:
  static bool OpenFileFlagsAreConsistent(int desired_flags);
};

} // namespace webkit_database

#endif  // WEBKIT_DATABASE_VFS_BACKEND_H_
