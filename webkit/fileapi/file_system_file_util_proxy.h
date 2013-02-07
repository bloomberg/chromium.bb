// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_FILE_SYSTEM_FILE_UTIL_PROXY_H_
#define WEBKIT_FILEAPI_FILE_SYSTEM_FILE_UTIL_PROXY_H_

#include "base/callback.h"
#include "base/file_path.h"
#include "base/files/file_util_proxy.h"
#include "base/platform_file.h"
#include "webkit/fileapi/file_system_file_util.h"
#include "webkit/fileapi/file_system_operation.h"

namespace fileapi {

class FileSystemFileUtil;
class FileSystemOperationContext;
class FileSystemURL;

// This class provides asynchronous access to FileSystemFileUtil methods for
// FileSystem API operations.  This also implements cross-FileUtil copy/move
// operations on top of FileSystemFileUtil methods.
class FileSystemFileUtilProxy {
 public:
  // Some of the proxy routines are just wrapping around the FileUtilProxy's
  // relay methods, so we use the same types as FileUtilProxy for them.
  typedef base::FileUtilProxy::Entry Entry;
  typedef base::FileUtilProxy::CreateOrOpenCallback CreateOrOpenCallback;

  typedef base::Callback<void(base::PlatformFileError status)> StatusCallback;
  typedef base::Callback<void(base::PlatformFileError status,
                              bool created)> EnsureFileExistsCallback;
  typedef FileSystemOperation::GetMetadataCallback GetFileInfoCallback;
  typedef FileSystemOperation::ReadDirectoryCallback ReadDirectoryCallback;

  typedef base::Callback<
      void(base::PlatformFileError result,
           const base::PlatformFileInfo& file_info,
           const base::FilePath& platform_path,
           FileSystemFileUtil::SnapshotFilePolicy snapshot_policy)>
      SnapshotFileCallback;

  // Deletes a file on the given context's task_runner.
  static bool DeleteFile(
      FileSystemOperationContext* context,
      FileSystemFileUtil* file_util,
      const FileSystemURL& url,
      const StatusCallback& callback);

  // Deletes a directory on the given context's task_runner.
  static bool DeleteDirectory(
      FileSystemOperationContext* context,
      FileSystemFileUtil* file_util,
      const FileSystemURL& url,
      const StatusCallback& callback);

  // Creates or opens a file with the given flags by calling |file_util|'s
  // CreateOrOpen method on the given context's task_runner.
  static bool CreateOrOpen(
      FileSystemOperationContext* context,
      FileSystemFileUtil* file_util,
      const FileSystemURL& url,
      int file_flags,
      const CreateOrOpenCallback& callback);

  // Copies a local file or a directory from |src_url| to |dest_url|.
  static bool CopyFileLocal(
      FileSystemOperationContext* context,
      FileSystemFileUtil* file_util,
      const FileSystemURL& src_url,
      const FileSystemURL& dest_url,
      const StatusCallback& callback);

  // Moves a local file or a directory from |src_url| to |dest_url|.
  static bool MoveFileLocal(
      FileSystemOperationContext* context,
      FileSystemFileUtil* file_util,
      const FileSystemURL& src_url,
      const FileSystemURL& dest_url,
      const StatusCallback& callback);

  // Copies a file from local disk to the given filesystem destination.
  // Primarily used for the Syncable filesystem type (e.g. Drive).
  static bool CopyInForeignFile(
      FileSystemOperationContext* context,
      FileSystemFileUtil* file_util,
      const base::FilePath& src_local_disk_file_path,
      const FileSystemURL& dest_url,
      const StatusCallback& callback);

  // Ensures that the given |url| exist by calling |file_util|'s
  // EnsureFileExists method on the given context's task_runner.
  static bool EnsureFileExists(
      FileSystemOperationContext* context,
      FileSystemFileUtil* file_util,
      const FileSystemURL& url,
      const EnsureFileExistsCallback& callback);

  // Creates directory at a given url by calling |file_util|'s
  // CreateDirectory method on the given context's task_runner.
  static bool CreateDirectory(
      FileSystemOperationContext* context,
      FileSystemFileUtil* file_util,
      const FileSystemURL& url,
      bool exclusive,
      bool recursive,
      const StatusCallback& callback);

  // Retrieves the information about a file by calling |file_util|'s
  // GetFileInfo method on the given context's task_runner.
  static bool GetFileInfo(
      FileSystemOperationContext* context,
      FileSystemFileUtil* file_util,
      const FileSystemURL& url,
      const GetFileInfoCallback& callback);

  // Creates a snapshot file by calling |file_util|'s CreateSnapshotFile
  // method on the given context's task_runner.
  static bool CreateSnapshotFile(
      FileSystemOperationContext* context,
      FileSystemFileUtil* file_util,
      const FileSystemURL& url,
      const SnapshotFileCallback& callback);

  // Reads the filenames in |url| by calling |file_util|'s
  // ReadDirectory method on the given context's task_runner.
  // TODO(kinuko): this should support returning entries in multiple chunks.
  // (http://crbug.com/145908)
  static bool ReadDirectory(
      FileSystemOperationContext* context,
      FileSystemFileUtil* file_util,
      const FileSystemURL& url,
      const ReadDirectoryCallback& callback);

  // Touches a file by calling |file_util|'s Touch method
  // on the given context's task_runner.
  static bool Touch(
      FileSystemOperationContext* context,
      FileSystemFileUtil* file_util,
      const FileSystemURL& url,
      const base::Time& last_access_time,
      const base::Time& last_modified_time,
      const StatusCallback& callback);

  // Truncates a file to the given length by calling |file_util|'s
  // Truncate method on the given context's task_runner.
  static bool Truncate(
      FileSystemOperationContext* context,
      FileSystemFileUtil* file_util,
      const FileSystemURL& url,
      int64 length,
      const StatusCallback& callback);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(FileSystemFileUtilProxy);
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_FILE_SYSTEM_FILE_UTIL_PROXY_H_
