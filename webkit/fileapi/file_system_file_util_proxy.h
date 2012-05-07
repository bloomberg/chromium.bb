// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_FILE_SYSTEM_FILE_UTIL_PROXY_H_
#define WEBKIT_FILEAPI_FILE_SYSTEM_FILE_UTIL_PROXY_H_

#include <vector>

#include "base/callback.h"
#include "base/file_path.h"
#include "base/file_util_proxy.h"
#include "base/memory/ref_counted.h"
#include "base/platform_file.h"
#include "base/tracked_objects.h"

namespace fileapi {

using base::PlatformFile;
using base::PlatformFileError;
using base::PlatformFileInfo;

class FileSystemFileUtil;
class FileSystemOperationContext;
class FileSystemPath;

// This class provides asynchronous access to FileSystemFileUtil methods for
// FileSystem API operations.  This also implements cross-FileUtil copy/move
// operations on top of FileSystemFileUtil methods.
class FileSystemFileUtilProxy {
 public:
  // Some of the proxy routines are just wrapping around the FileUtilProxy's
  // relay methods, so we use the same types as FileUtilProxy for them.
  typedef base::FileUtilProxy::Entry Entry;
  typedef base::FileUtilProxy::CreateOrOpenCallback CreateOrOpenCallback;

  typedef base::Callback<void(PlatformFileError status)> StatusCallback;
  typedef base::Callback<void(PlatformFileError status,
                              bool created)> EnsureFileExistsCallback;
  typedef base::Callback<void(PlatformFileError status,
                              const PlatformFileInfo& info,
                              const FilePath& platform_path)>
      GetFileInfoCallback;
  typedef base::Callback<void(PlatformFileError,
                              const std::vector<Entry>&,
                              bool has_more)> ReadDirectoryCallback;

  // Deletes a file or a directory on the given context's file_task_runner.
  // It is an error to delete a non-empty directory with recursive=false.
  static bool Delete(
      FileSystemOperationContext* context,
      FileSystemFileUtil* file_util,
      const FileSystemPath& path,
      bool recursive,
      const StatusCallback& callback);

  // Creates or opens a file with the given flags by calling |file_util|'s
  // CreateOrOpen method on the given context's file_task_runner.
  static bool CreateOrOpen(
      FileSystemOperationContext* context,
      FileSystemFileUtil* file_util,
      const FileSystemPath& path,
      int file_flags,
      const CreateOrOpenCallback& callback);

  // Copies a file or a directory from |src_path| to |dest_path| by calling
  // FileSystemFileUtil's following methods on the given context's
  // file_task_runner.
  // - CopyOrMoveFile() for same-filesystem operations
  // - CopyInForeignFile() for (limited) cross-filesystem operations
  //
  // Error cases:
  // If destination's parent doesn't exist.
  // If source dir exists but destination path is an existing file.
  // If source file exists but destination path is an existing directory.
  // If source is a parent of destination.
  // If source doesn't exist.
  // If source and dest are the same path in the same filesystem.
  static bool Copy(
      FileSystemOperationContext* context,
      FileSystemFileUtil* src_util,
      FileSystemFileUtil* dest_util,
      const FileSystemPath& src_path,
      const FileSystemPath& dest_path,
      const StatusCallback& callback);

  // Moves a file or a directory from |src_path| to |dest_path| by calling
  // FileSystemFileUtil's following methods on the given context's
  // file_task_runner.
  // - CopyOrMoveFile() for same-filesystem operations
  // - CopyInForeignFile() for (limited) cross-filesystem operations
  //
  // This method returns an error on the same error cases with Copy.
  static bool Move(
      FileSystemOperationContext* context,
      FileSystemFileUtil* src_util,
      FileSystemFileUtil* dest_util,
      const FileSystemPath& src_path,
      const FileSystemPath& dest_path,
      const StatusCallback& callback);

  // Ensures that the given |path| exist by calling |file_util|'s
  // EnsureFileExists method on the given context's file_task_runner.
  static bool EnsureFileExists(
      FileSystemOperationContext* context,
      FileSystemFileUtil* file_util,
      const FileSystemPath& path,
      const EnsureFileExistsCallback& callback);

  // Creates directory at a given path by calling |file_util|'s
  // CreateDirectory method on the given context's file_task_runner.
  static bool CreateDirectory(
      FileSystemOperationContext* context,
      FileSystemFileUtil* file_util,
      const FileSystemPath& path,
      bool exclusive,
      bool recursive,
      const StatusCallback& callback);

  // Retrieves the information about a file by calling |file_util|'s
  // GetFileInfo method on the given context's file_task_runner.
  static bool GetFileInfo(
      FileSystemOperationContext* context,
      FileSystemFileUtil* file_util,
      const FileSystemPath& path,
      const GetFileInfoCallback& callback);

  // Reads the filenames in |path| by calling |file_util|'s
  // ReadDirectory method on the given context's file_task_runner.
  // TODO: this should support returning entries in multiple chunks.
  static bool ReadDirectory(
      FileSystemOperationContext* context,
      FileSystemFileUtil* file_util,
      const FileSystemPath& path,
      const ReadDirectoryCallback& callback);

  // Touches a file by calling |file_util|'s Touch method
  // on the given context's file_task_runner.
  static bool Touch(
      FileSystemOperationContext* context,
      FileSystemFileUtil* file_util,
      const FileSystemPath& path,
      const base::Time& last_access_time,
      const base::Time& last_modified_time,
      const StatusCallback& callback);

  // Truncates a file to the given length by calling |file_util|'s
  // Truncate method on the given context's file_task_runner.
  static bool Truncate(
      FileSystemOperationContext* context,
      FileSystemFileUtil* file_util,
      const FileSystemPath& path,
      int64 length,
      const StatusCallback& callback);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(FileSystemFileUtilProxy);
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_FILE_SYSTEM_FILE_UTIL_PROXY_H_
