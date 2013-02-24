// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_MEDIA_DEVICE_MEDIA_ASYNC_FILE_UTIL_H_
#define WEBKIT_FILEAPI_MEDIA_DEVICE_MEDIA_ASYNC_FILE_UTIL_H_

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/platform_file.h"
#include "webkit/fileapi/async_file_util.h"
#include "webkit/storage/webkit_storage_export.h"

namespace base {
class Time;
}

namespace fileapi {

class FileSystemOperationContext;
class FileSystemURL;

class WEBKIT_STORAGE_EXPORT_PRIVATE DeviceMediaAsyncFileUtil
    : public AsyncFileUtil {
 public:
  virtual ~DeviceMediaAsyncFileUtil();

  // Returns an instance of DeviceMediaAsyncFileUtil. Returns NULL if
  // asynchronous operation is not supported. Callers own the returned
  // object.
  static DeviceMediaAsyncFileUtil* Create(const base::FilePath& profile_path);

  // AsyncFileUtil overrides.
  virtual bool CreateOrOpen(
      FileSystemOperationContext* context,
      const FileSystemURL& url,
      int file_flags,
      const CreateOrOpenCallback& callback) OVERRIDE;
  virtual bool EnsureFileExists(
      FileSystemOperationContext* context,
      const FileSystemURL& url,
      const EnsureFileExistsCallback& callback) OVERRIDE;
  virtual bool CreateDirectory(
      FileSystemOperationContext* context,
      const FileSystemURL& url,
      bool exclusive,
      bool recursive,
      const StatusCallback& callback) OVERRIDE;
  virtual bool GetFileInfo(
      FileSystemOperationContext* context,
      const FileSystemURL& url,
      const GetFileInfoCallback& callback) OVERRIDE;
  virtual bool ReadDirectory(
      FileSystemOperationContext* context,
      const FileSystemURL& url,
      const ReadDirectoryCallback& callback) OVERRIDE;
  virtual bool Touch(
      FileSystemOperationContext* context,
      const FileSystemURL& url,
      const base::Time& last_access_time,
      const base::Time& last_modified_time,
      const StatusCallback& callback) OVERRIDE;
  virtual bool Truncate(
      FileSystemOperationContext* context,
      const FileSystemURL& url,
      int64 length,
      const StatusCallback& callback) OVERRIDE;
  virtual bool CopyFileLocal(
      FileSystemOperationContext* context,
      const FileSystemURL& src_url,
      const FileSystemURL& dest_url,
      const StatusCallback& callback) OVERRIDE;
  virtual bool MoveFileLocal(
      FileSystemOperationContext* context,
      const FileSystemURL& src_url,
      const FileSystemURL& dest_url,
      const StatusCallback& callback) OVERRIDE;
  virtual bool CopyInForeignFile(
      FileSystemOperationContext* context,
      const base::FilePath& src_file_path,
      const FileSystemURL& dest_url,
      const StatusCallback& callback) OVERRIDE;
  virtual bool DeleteFile(
      FileSystemOperationContext* context,
      const FileSystemURL& url,
      const StatusCallback& callback) OVERRIDE;
  virtual bool DeleteDirectory(
      FileSystemOperationContext* context,
      const FileSystemURL& url,
      const StatusCallback& callback) OVERRIDE;
  virtual bool CreateSnapshotFile(
      FileSystemOperationContext* context,
      const FileSystemURL& url,
      const CreateSnapshotFileCallback& callback) OVERRIDE;

 private:
  // Use Create() to get an instance of DeviceMediaAsyncFileUtil.
  explicit DeviceMediaAsyncFileUtil(const base::FilePath& profile_path);

  // Called when GetFileInfo method call succeeds. |file_info|
  // contains the |platform_path| file details. |callback| is invoked
  // to complete the GetFileInfo request.
  void OnDidGetFileInfo(
      const AsyncFileUtil::GetFileInfoCallback& callback,
      const base::FilePath& platform_path,
      const base::PlatformFileInfo& file_info);

  // Called when GetFileInfo method call failed to get the details of file
  // specified by the |platform_path|. |callback| is invoked to notify the
  // caller about the platform file |error|.
  void OnGetFileInfoError(
      const AsyncFileUtil::GetFileInfoCallback& callback,
      const base::FilePath& platform_path,
      base::PlatformFileError error);

  // Called when ReadDirectory method call succeeds. |callback| is invoked to
  // complete the ReadDirectory request.
  //
  // If the contents of the given directory are reported in one batch, then
  // |file_list| will have the list of all files/directories in the given
  // directory and |has_more| will be false.
  //
  // If the contents of the given directory are reported in multiple chunks,
  // |file_list| will have only a subset of all contents (the subsets reported
  // in any two calls are disjoint), and |has_more| will be true, except for
  // the last chunk.
  void OnDidReadDirectory(
      const AsyncFileUtil::ReadDirectoryCallback& callback,
      const AsyncFileUtil::EntryList& file_list,
      bool has_more);

  // Called when ReadDirectory method call failed to enumerate the directory
  // objects. |callback| is invoked to notify the caller about the |error|
  // that occured while reading the directory objects.
  void OnReadDirectoryError(
      const AsyncFileUtil::ReadDirectoryCallback& callback,
      base::PlatformFileError error);

  // Called when the snapshot file specified by the |platform_path| is
  // successfully created. |file_info| contains the device media file details
  // for which the snapshot file is created. |callback| is invoked to complete
  // the CreateSnapshotFile request.
  void OnDidCreateSnapshotFile(
      const AsyncFileUtil::CreateSnapshotFileCallback& callback,
      const base::PlatformFileInfo& file_info,
      const base::FilePath& platform_path);

  // Called when CreateSnapshotFile method call fails. |callback| is invoked to
  // notify the caller about the |error|.
  void OnCreateSnapshotFileError(
      const AsyncFileUtil::CreateSnapshotFileCallback& callback,
      base::PlatformFileError error);

  // Called when the snapshot file specified by the |snapshot_file_path| is
  // created to hold the contents of the |device_file_path|. If the snapshot
  // file is successfully created, |snapshot_file_path| will be an non-empty
  // file path. In case of failure, |snapshot_file_path| will be an empty file
  // path. Forwards the CreateSnapshot request to the delegate to copy the
  // contents of |device_file_path| to |snapshot_file_path|.
  void OnSnapshotFileCreatedRunTask(
      FileSystemOperationContext* context,
      const AsyncFileUtil::CreateSnapshotFileCallback& callback,
      const base::FilePath& device_file_path,
      base::FilePath* snapshot_file_path);

  // Profile path.
  const base::FilePath profile_path_;

  // For callbacks that may run after destruction.
  base::WeakPtrFactory<DeviceMediaAsyncFileUtil> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DeviceMediaAsyncFileUtil);
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_MEDIA_DEVICE_MEDIA_ASYNC_FILE_UTIL_H_
