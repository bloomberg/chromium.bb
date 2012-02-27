// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_CROSS_FILE_UTIL_HELPER_H_
#define WEBKIT_FILEAPI_CROSS_FILE_UTIL_HELPER_H_

#include "base/callback.h"
#include "base/platform_file.h"

namespace fileapi {

class FileSystemFileUtil;
class FileSystemOperationContext;
class FileSystemPath;

// A helper class for cross-FileUtil Copy/Move operations.
class CrossFileUtilHelper {
 public:
  enum Operation {
    OPERATION_COPY,
    OPERATION_MOVE
  };

  CrossFileUtilHelper(FileSystemOperationContext* context,
                      FileSystemFileUtil* src_util,
                      FileSystemFileUtil* dest_util,
                      const FileSystemPath& src_path,
                      const FileSystemPath& dest_path,
                      Operation operation);
  ~CrossFileUtilHelper();

  base::PlatformFileError DoWork();

 private:
  // Performs common pre-operation check and preparation.
  // This may delete the destination directory if it's empty.
  base::PlatformFileError PerformErrorCheckAndPreparationForMoveAndCopy();

  // This assumes that the root exists.
  bool ParentExists(const FileSystemPath& path, FileSystemFileUtil* file_util);

  // Performs recursive copy or move by calling CopyOrMoveFile for individual
  // files. Operations for recursive traversal are encapsulated in this method.
  // It assumes src_path and dest_path have passed
  // PerformErrorCheckAndPreparationForMoveAndCopy().
  base::PlatformFileError CopyOrMoveDirectory(
      const FileSystemPath& src_path,
      const FileSystemPath& dest_path);

  // Determines whether a simple same-filesystem move or copy can be done.  If
  // so, it delegates to CopyOrMoveFile.  Otherwise it looks up the true
  // platform path of the source file, delegates to CopyInForeignFile, and [for
  // move] calls DeleteFile on the source file.
  base::PlatformFileError CopyOrMoveFile(
      const FileSystemPath& src_path,
      const FileSystemPath& dest_path);

  FileSystemOperationContext* context_;
  FileSystemFileUtil* src_util_;  // Not owned.
  FileSystemFileUtil* dest_util_;  // Not owned.
  const FileSystemPath& src_root_path_;
  const FileSystemPath& dest_root_path_;
  Operation operation_;

  DISALLOW_COPY_AND_ASSIGN(CrossFileUtilHelper);
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_CROSS_FILE_UTIL_HELPER_H_
