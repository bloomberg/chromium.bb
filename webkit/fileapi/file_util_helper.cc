// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/file_util_helper.h"

#include "webkit/fileapi/file_system_file_util.h"
#include "webkit/fileapi/file_system_operation_context.h"
#include "webkit/fileapi/file_system_path.h"

using base::PlatformFileError;

namespace fileapi {

base::PlatformFileError FileUtilHelper::Delete(
    FileSystemOperationContext* context,
    FileSystemFileUtil* file_util,
    const FileSystemPath& path,
    bool recursive) {
  if (file_util->DirectoryExists(context, path)) {
    if (!recursive)
      return file_util->DeleteSingleDirectory(context, path);
    else
      return DeleteDirectoryRecursive(context, file_util, path);
  } else {
    return file_util->DeleteFile(context, path);
  }
}

base::PlatformFileError FileUtilHelper::DeleteDirectoryRecursive(
    FileSystemOperationContext* context,
    FileSystemFileUtil* file_util,
    const FileSystemPath& path) {

  scoped_ptr<FileSystemFileUtil::AbstractFileEnumerator> file_enum(
      file_util->CreateFileEnumerator(context, path, true /* recursive */));
  FilePath file_path_each;
  std::stack<FilePath> directories;
  while (!(file_path_each = file_enum->Next()).empty()) {
    if (file_enum->IsDirectory()) {
      directories.push(file_path_each);
    } else {
      PlatformFileError error = file_util->DeleteFile(
          context, path.WithInternalPath(file_path_each));
      if (error != base::PLATFORM_FILE_ERROR_NOT_FOUND &&
          error != base::PLATFORM_FILE_OK)
        return error;
    }
  }

  while (!directories.empty()) {
    PlatformFileError error = file_util->DeleteSingleDirectory(
        context, path.WithInternalPath(directories.top()));
    if (error != base::PLATFORM_FILE_ERROR_NOT_FOUND &&
        error != base::PLATFORM_FILE_OK)
      return error;
    directories.pop();
  }
  return file_util->DeleteSingleDirectory(context, path);
}

}  // namespace fileapi
