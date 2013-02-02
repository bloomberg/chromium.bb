// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_ASYNC_FILE_UTIL_H_
#define WEBKIT_FILEAPI_ASYNC_FILE_UTIL_H_

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/files/file_util_proxy.h"
#include "base/platform_file.h"
#include "webkit/fileapi/file_snapshot_policy.h"
#include "webkit/storage/webkit_storage_export.h"

namespace base {
class Time;
}

namespace fileapi {

class FileSystemOperationContext;
class FileSystemURL;

// An interface which provides filesystem-specific file operations for
// LocalFileSystemOperation.
//
// Each filesystem which needs to be dispatched from LocalFileSystemOperation
// must implement this interface or a synchronous version of interface:
// FileSystemFileUtil.
//
class WEBKIT_STORAGE_EXPORT AsyncFileUtil {
 public:
  typedef base::Callback<
      void(base::PlatformFileError result)> StatusCallback;

  typedef base::FileUtilProxy::CreateOrOpenCallback CreateOrOpenCallback;

  typedef base::Callback<
      void(base::PlatformFileError result,
           bool created)> EnsureFileExistsCallback;

  typedef base::Callback<
      void(base::PlatformFileError result,
           const base::PlatformFileInfo& file_info,
           const base::FilePath& platform_path)> GetFileInfoCallback;

  typedef base::FileUtilProxy::Entry Entry;
  typedef std::vector<base::FileUtilProxy::Entry> EntryList;
  typedef base::Callback<
      void(base::PlatformFileError result,
           const EntryList& file_list,
           bool has_more)> ReadDirectoryCallback;

  typedef base::Callback<
      void(base::PlatformFileError result,
           const base::PlatformFileInfo& file_info,
           const base::FilePath& platform_path,
           SnapshotFilePolicy policy)> CreateSnapshotFileCallback;

  AsyncFileUtil() {}
  virtual ~AsyncFileUtil() {}

  // Creates or opens a file with the given flags.
  // If PLATFORM_FILE_CREATE is set in |file_flags| it always tries to create
  // a new file at the given |url| and calls back with
  // PLATFORM_FILE_ERROR_FILE_EXISTS if the |url| already exists.
  //
  // LocalFileSystemOperation::OpenFile calls this.
  // This is used only by Pepper/NaCL File API.
  //
  // This returns false if it fails to post an async task.
  //
  virtual bool CreateOrOpen(
      FileSystemOperationContext* context,
      const FileSystemURL& url,
      int file_flags,
      const CreateOrOpenCallback& callback) = 0;

  // Ensures that the given |url| exist.  This creates a empty new file
  // at |url| if the |url| does not exist.
  //
  // LocalFileSystemOperation::CreateFile calls this.
  //
  // This returns false if it fails to post an async task.
  //
  // This reports following error code via |callback|:
  // - PLATFORM_FILE_OK and created==true if a file has not existed and
  //   is created at |url|.
  // - PLATFORM_FILE_OK and created==false if the file already exists.
  // - Other error code (with created=false) if a file hasn't existed yet
  //   and there was an error while creating a new file.
  //
  virtual bool EnsureFileExists(
      FileSystemOperationContext* context,
      const FileSystemURL& url,
      const EnsureFileExistsCallback& callback) = 0;

  // Creates directory at given url.
  //
  // LocalFileSystemOperation::CreateDirectory calls this.
  //
  // This returns false if it fails to post an async task.
  //
  // This reports following error code via |callback|:
  // - PLATFORM_FILE_ERROR_NOT_FOUND if the |url|'s parent directory
  //   does not exist and |recursive| is false.
  // - PLATFORM_FILE_ERROR_EXISTS if a directory already exists at |url|
  //   and |exclusive| is true.
  // - PLATFORM_FILE_ERROR_EXISTS if a file already exists at |url|
  //   (regardless of |exclusive| value).
  // - Other error code if it failed to create a directory.
  //
  virtual bool CreateDirectory(
      FileSystemOperationContext* context,
      const FileSystemURL& url,
      bool exclusive,
      bool recursive,
      const StatusCallback& callback) = 0;

  // Retrieves the information about a file.
  //
  // LocalFileSystemOperation::GetMetadata calls this.
  //
  // This returns false if it fails to post an async task.
  //
  // This reports following error code via |callback|:
  // - PLATFORM_FILE_ERROR_NOT_FOUND if the file doesn't exist.
  // - Other error code if there was an error while retrieving the file info.
  //
  virtual bool GetFileInfo(
      FileSystemOperationContext* context,
      const FileSystemURL& url,
      const GetFileInfoCallback& callback) = 0;

  // Reads contents of a directory at |path|.
  //
  // LocalFileSystemOperation::ReadDirectory calls this.
  //
  // This returns false if it fails to post an async task.
  //
  // This reports following error code via |callback|:
  // - PLATFORM_FILE_ERROR_NOT_FOUND if the target directory doesn't exist.
  // - PLATFORM_FILE_ERROR_NOT_A_DIRECTORY if an entry exists at |url| but
  //   is a file (not a directory).
  //
  virtual bool ReadDirectory(
      FileSystemOperationContext* context,
      const FileSystemURL& url,
      const ReadDirectoryCallback& callback) = 0;

  // Modifies timestamps of a file or directory at |url| with
  // |last_access_time| and |last_modified_time|. The function DOES NOT
  // create a file unlike 'touch' command on Linux.
  //
  // LocalFileSystemOperation::TouchFile calls this.
  // This is used only by Pepper/NaCL File API.
  //
  // This returns false if it fails to post an async task.
  virtual bool Touch(
      FileSystemOperationContext* context,
      const FileSystemURL& url,
      const base::Time& last_access_time,
      const base::Time& last_modified_time,
      const StatusCallback& callback) = 0;

  // Truncates a file at |path| to |length|. If |length| is larger than
  // the original file size, the file will be extended, and the extended
  // part is filled with null bytes.
  //
  // LocalFileSystemOperation::Truncate calls this.
  //
  // This returns false if it fails to post an async task.
  //
  // This reports following error code via |callback|:
  // - PLATFORM_FILE_ERROR_NOT_FOUND if the file doesn't exist.
  //
  virtual bool Truncate(
      FileSystemOperationContext* context,
      const FileSystemURL& url,
      int64 length,
      const StatusCallback& callback) = 0;

  // Copies a file from |src_url| to |dest_url|.
  // This must be called for files that belong to the same filesystem
  // (i.e. type() and origin() of the |src_url| and |dest_url| must match).
  //
  // LocalFileSystemOperation::Copy calls this for same-filesystem copy case.
  //
  // This returns false if it fails to post an async task.
  //
  // This reports following error code via |callback|:
  // - PLATFORM_FILE_ERROR_NOT_FOUND if |src_url|
  //   or the parent directory of |dest_url| does not exist.
  // - PLATFORM_FILE_ERROR_NOT_A_FILE if |src_url| exists but is not a file.
  // - PLATFORM_FILE_ERROR_INVALID_OPERATION if |dest_url| exists and
  //   is not a file.
  // - PLATFORM_FILE_ERROR_FAILED if |dest_url| does not exist and
  //   its parent path is a file.
  //
  virtual bool CopyFileLocal(
      FileSystemOperationContext* context,
      const FileSystemURL& src_url,
      const FileSystemURL& dest_url,
      const StatusCallback& callback) = 0;

  // Moves a local file from |src_url| to |dest_url|.
  // This must be called for files that belong to the same filesystem
  // (i.e. type() and origin() of the |src_url| and |dest_url| must match).
  //
  // LocalFileSystemOperation::Move calls this for same-filesystem move case.
  //
  // This returns false if it fails to post an async task.
  //
  // This reports following error code via |callback|:
  // - PLATFORM_FILE_ERROR_NOT_FOUND if |src_url|
  //   or the parent directory of |dest_url| does not exist.
  // - PLATFORM_FILE_ERROR_NOT_A_FILE if |src_url| exists but is not a file.
  // - PLATFORM_FILE_ERROR_INVALID_OPERATION if |dest_url| exists and
  //   is not a file.
  // - PLATFORM_FILE_ERROR_FAILED if |dest_url| does not exist and
  //   its parent path is a file.
  //
  virtual bool MoveFileLocal(
      FileSystemOperationContext* context,
      const FileSystemURL& src_url,
      const FileSystemURL& dest_url,
      const StatusCallback& callback) = 0;

  // Copies in a single file from a different filesystem.
  //
  // LocalFileSystemOperation::Copy or Move calls this for cross-filesystem
  // cases.
  //
  // This returns false if it fails to post an async task.
  //
  // This reports following error code via |callback|:
  // - PLATFORM_FILE_ERROR_NOT_FOUND if |src_file_path|
  //   or the parent directory of |dest_url| does not exist.
  // - PLATFORM_FILE_ERROR_INVALID_OPERATION if |dest_url| exists and
  //   is not a file.
  // - PLATFORM_FILE_ERROR_FAILED if |dest_url| does not exist and
  //   its parent path is a file.
  //
  virtual bool CopyInForeignFile(
        FileSystemOperationContext* context,
        const base::FilePath& src_file_path,
        const FileSystemURL& dest_url,
        const StatusCallback& callback) = 0;

  // Deletes a single file.
  //
  // LocalFileSystemOperation::RemoveFile calls this.
  //
  // This returns false if it fails to post an async task.
  //
  // This reports following error code via |callback|:
  // - PLATFORM_FILE_ERROR_NOT_FOUND if |url| does not exist.
  // - PLATFORM_FILE_ERROR_NOT_A_FILE if |url| is not a file.
  //
  virtual bool DeleteFile(
      FileSystemOperationContext* context,
      const FileSystemURL& url,
      const StatusCallback& callback) = 0;

  // Removes a single empty directory.
  //
  // LocalFileSystemOperation::RemoveDirectory calls this.
  //
  // This returns false if it fails to post an async task.
  //
  // This reports following error code via |callback|:
  // - PLATFORM_FILE_ERROR_NOT_FOUND if |url| does not exist.
  // - PLATFORM_FILE_ERROR_NOT_A_DIRECTORY if |url| is not a directory.
  // - PLATFORM_FILE_ERROR_NOT_EMPTY if |url| is not empty.
  //
  virtual bool DeleteDirectory(
      FileSystemOperationContext* context,
      const FileSystemURL& url,
      const StatusCallback& callback) = 0;

  // Creates a local snapshot file for a given |url| and returns the
  // metadata and platform path of the snapshot file via |callback|.
  // In regular filesystem cases the implementation may simply return
  // the metadata of the file itself (as well as GetMetadata does),
  // while in non-regular filesystem case the backend may create a
  // temporary snapshot file which holds the file data and return
  // the metadata of the temporary file.
  //
  // In the callback, it returns:
  // |file_info| is the metadata of the snapshot file created.
  // |platform_path| is the path to the snapshot file created.
  // |policy| should indicate the policy how the fileapi backend
  // should handle the returned file.
  //
  // LocalFileSystemOperation::CreateSnapshotFile calls this.
  //
  // This returns false if it fails to post an async task.
  //
  // This reports following error code via |callback|:
  // - PLATFORM_FILE_ERROR_NOT_FOUND if |url| does not exist.
  // - PLATFORM_FILE_ERROR_NOT_A_FILE if |url| exists but is a directory.
  //
  // The field values of |file_info| are undefined (implementation
  // dependent) in error cases, and the caller should always
  // check the return code.
  virtual bool CreateSnapshotFile(
      FileSystemOperationContext* context,
      const FileSystemURL& url,
      const CreateSnapshotFileCallback& callback) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(AsyncFileUtil);
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_ASYNC_FILE_UTIL_H_
