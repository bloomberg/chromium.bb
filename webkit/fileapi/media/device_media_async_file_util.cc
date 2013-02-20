// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/media/device_media_async_file_util.h"

#include "base/callback.h"
#include "base/file_util.h"
#include "base/single_thread_task_runner.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_operation_context.h"
#include "webkit/fileapi/file_system_task_runners.h"
#include "webkit/fileapi/file_system_url.h"
#include "webkit/fileapi/isolated_context.h"
#include "webkit/fileapi/media/filtering_file_enumerator.h"
#include "webkit/fileapi/media/media_path_filter.h"
#include "webkit/fileapi/media/mtp_device_async_delegate.h"
#include "webkit/fileapi/media/mtp_device_map_service.h"

namespace fileapi {

namespace {

const base::FilePath::CharType kDeviceMediaAsyncFileUtilTempDir[] =
    FILE_PATH_LITERAL("DeviceMediaFileSystem");

// Returns true if the current thread is IO thread.
bool IsOnIOThread(FileSystemOperationContext* context) {
  return context->file_system_context()->task_runners()->
      io_task_runner()->RunsTasksOnCurrentThread();
}

// Called on the IO thread.
MTPDeviceAsyncDelegate* GetMTPDeviceDelegate(
    FileSystemOperationContext* context) {
  DCHECK(IsOnIOThread(context));
  return MTPDeviceMapService::GetInstance()->GetMTPDeviceAsyncDelegate(
      context->mtp_device_delegate_url());
}

// Called on the blocking pool thread to create a snapshot file to hold the
// contents of |device_file_path|. The snapshot file is created in
// "profile_path/kDeviceMediaAsyncFileUtilTempDir" directory. If the snapshot
// file is created successfully, |snapshot_file_path| will be a non-empty file
// path. In case of failure, the |snapshot_file_path| will be an empty file
// path.
void CreateSnapshotFileOnBlockingPool(
    const base::FilePath& device_file_path,
    const base::FilePath& profile_path,
    base::FilePath* snapshot_file_path) {
  DCHECK(snapshot_file_path);
  base::FilePath isolated_media_file_system_dir_path =
      profile_path.Append(kDeviceMediaAsyncFileUtilTempDir);
  if (!file_util::CreateDirectory(isolated_media_file_system_dir_path) ||
      !file_util::CreateTemporaryFileInDir(isolated_media_file_system_dir_path,
                                           snapshot_file_path)) {
    LOG(WARNING) << "Could not create media snapshot file "
                 << isolated_media_file_system_dir_path.value();
    *snapshot_file_path = base::FilePath();
  }
}

}  // namespace

DeviceMediaAsyncFileUtil::~DeviceMediaAsyncFileUtil() {
}

// static
DeviceMediaAsyncFileUtil* DeviceMediaAsyncFileUtil::Create(
    const base::FilePath& profile_path) {
  return NULL;
}

bool DeviceMediaAsyncFileUtil::CreateOrOpen(
    FileSystemOperationContext* context,
    const FileSystemURL& url,
    int file_flags,
    const CreateOrOpenCallback& callback) {
  DCHECK(IsOnIOThread(context));
  NOTIMPLEMENTED();
  if (!callback.is_null()) {
    base::PlatformFile invalid_file = base::kInvalidPlatformFileValue;
    callback.Run(base::PLATFORM_FILE_ERROR_SECURITY,
                 base::PassPlatformFile(&invalid_file),
                 false);
  }
  return true;
}

bool DeviceMediaAsyncFileUtil::EnsureFileExists(
    FileSystemOperationContext* context,
    const FileSystemURL& url,
    const EnsureFileExistsCallback& callback) {
  DCHECK(IsOnIOThread(context));
  NOTIMPLEMENTED();
  if (!callback.is_null())
    callback.Run(base::PLATFORM_FILE_ERROR_SECURITY, false);
  return true;
}

bool DeviceMediaAsyncFileUtil::CreateDirectory(
    FileSystemOperationContext* context,
    const FileSystemURL& url,
    bool exclusive,
    bool recursive,
    const StatusCallback& callback) {
  DCHECK(IsOnIOThread(context));
  NOTIMPLEMENTED();
  if (!callback.is_null())
    callback.Run(base::PLATFORM_FILE_ERROR_SECURITY);
  return true;
}

bool DeviceMediaAsyncFileUtil::GetFileInfo(
    FileSystemOperationContext* context,
    const FileSystemURL& url,
    const GetFileInfoCallback& callback) {
  DCHECK(IsOnIOThread(context));
  MTPDeviceAsyncDelegate* delegate = GetMTPDeviceDelegate(context);
  if (!delegate) {
    OnGetFileInfoError(callback, url.path(),
                       base::PLATFORM_FILE_ERROR_NOT_FOUND);
    return true;
  }
  delegate->GetFileInfo(
      url.path(),
      base::Bind(&DeviceMediaAsyncFileUtil::OnDidGetFileInfo,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback,
                 url.path()),
      base::Bind(&DeviceMediaAsyncFileUtil::OnGetFileInfoError,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback,
                 url.path()));
  return true;
}

bool DeviceMediaAsyncFileUtil::ReadDirectory(
    FileSystemOperationContext* context,
    const FileSystemURL& url,
    const ReadDirectoryCallback& callback) {
  DCHECK(IsOnIOThread(context));
  MTPDeviceAsyncDelegate* delegate = GetMTPDeviceDelegate(context);
  if (!delegate) {
    OnReadDirectoryError(callback, base::PLATFORM_FILE_ERROR_NOT_FOUND);
    return true;
  }
  delegate->ReadDirectory(
      url.path(),
      base::Bind(&DeviceMediaAsyncFileUtil::OnDidReadDirectory,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback),
      base::Bind(&DeviceMediaAsyncFileUtil::OnReadDirectoryError,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback));
  return true;
}

bool DeviceMediaAsyncFileUtil::Touch(
    FileSystemOperationContext* context,
    const FileSystemURL& url,
    const base::Time& last_access_time,
    const base::Time& last_modified_time,
    const StatusCallback& callback) {
  DCHECK(IsOnIOThread(context));
  NOTIMPLEMENTED();
  if (!callback.is_null())
    callback.Run(base::PLATFORM_FILE_ERROR_SECURITY);
  return true;
}

bool DeviceMediaAsyncFileUtil::Truncate(
    FileSystemOperationContext* context,
    const FileSystemURL& url,
    int64 length,
    const StatusCallback& callback) {
  DCHECK(IsOnIOThread(context));
  NOTIMPLEMENTED();
  if (!callback.is_null())
    callback.Run(base::PLATFORM_FILE_ERROR_SECURITY);
  return true;
}

bool DeviceMediaAsyncFileUtil::CopyFileLocal(
    FileSystemOperationContext* context,
    const FileSystemURL& src_url,
    const FileSystemURL& dest_url,
    const StatusCallback& callback) {
  DCHECK(IsOnIOThread(context));
  NOTIMPLEMENTED();
  if (!callback.is_null())
    callback.Run(base::PLATFORM_FILE_ERROR_SECURITY);
  return true;
}

bool DeviceMediaAsyncFileUtil::MoveFileLocal(
    FileSystemOperationContext* context,
    const FileSystemURL& src_url,
    const FileSystemURL& dest_url,
    const StatusCallback& callback) {
  DCHECK(IsOnIOThread(context));
  NOTIMPLEMENTED();
  if (!callback.is_null())
    callback.Run(base::PLATFORM_FILE_ERROR_SECURITY);
  return true;
}

bool DeviceMediaAsyncFileUtil::CopyInForeignFile(
    FileSystemOperationContext* context,
    const base::FilePath& src_file_path,
    const FileSystemURL& dest_url,
    const StatusCallback& callback) {
  DCHECK(IsOnIOThread(context));
  NOTIMPLEMENTED();
  if (!callback.is_null())
    callback.Run(base::PLATFORM_FILE_ERROR_SECURITY);
  return true;
}

bool DeviceMediaAsyncFileUtil::DeleteFile(
    FileSystemOperationContext* context,
    const FileSystemURL& url,
    const StatusCallback& callback) {
  DCHECK(IsOnIOThread(context));
  NOTIMPLEMENTED();
  if (!callback.is_null())
    callback.Run(base::PLATFORM_FILE_ERROR_SECURITY);
  return true;
}

bool DeviceMediaAsyncFileUtil::DeleteDirectory(
    FileSystemOperationContext* context,
    const FileSystemURL& url,
    const StatusCallback& callback) {
  DCHECK(IsOnIOThread(context));
  NOTIMPLEMENTED();
  if (!callback.is_null())
    callback.Run(base::PLATFORM_FILE_ERROR_SECURITY);
  return true;
}

bool DeviceMediaAsyncFileUtil::CreateSnapshotFile(
    FileSystemOperationContext* context,
    const FileSystemURL& url,
    const CreateSnapshotFileCallback& callback) {
  DCHECK(IsOnIOThread(context));
  MTPDeviceAsyncDelegate* delegate = GetMTPDeviceDelegate(context);
  if (!delegate) {
    OnCreateSnapshotFileError(callback, base::PLATFORM_FILE_ERROR_NOT_FOUND);
    return true;
  }
  base::FilePath* snapshot_file_path = new base::FilePath;
  return context->file_system_context()->task_runners()->media_task_runner()->
      PostTaskAndReply(
          FROM_HERE,
          base::Bind(&CreateSnapshotFileOnBlockingPool,
                     url.path(),
                     profile_path_,
                     base::Unretained(snapshot_file_path)),
          base::Bind(&DeviceMediaAsyncFileUtil::OnSnapshotFileCreatedRunTask,
                     weak_ptr_factory_.GetWeakPtr(),
                     context,
                     callback,
                     url.path(),
                     base::Owned(snapshot_file_path)));
}

DeviceMediaAsyncFileUtil::DeviceMediaAsyncFileUtil(
    const base::FilePath& profile_path)
    : profile_path_(profile_path),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
}

void DeviceMediaAsyncFileUtil::OnDidGetFileInfo(
    const AsyncFileUtil::GetFileInfoCallback& callback,
    const base::FilePath& platform_path,
    const base::PlatformFileInfo& file_info) {
  if (!callback.is_null())
    callback.Run(base::PLATFORM_FILE_OK, file_info, platform_path);
}

void DeviceMediaAsyncFileUtil::OnGetFileInfoError(
    const AsyncFileUtil::GetFileInfoCallback& callback,
    const base::FilePath& platform_path,
    base::PlatformFileError error) {
  if (!callback.is_null())
    callback.Run(error, base::PlatformFileInfo(), platform_path);
}

void DeviceMediaAsyncFileUtil::OnDidReadDirectory(
    const AsyncFileUtil::ReadDirectoryCallback& callback,
    const AsyncFileUtil::EntryList& file_list,
    bool has_more) {
  if (!callback.is_null())
    callback.Run(base::PLATFORM_FILE_OK, file_list, has_more);
}

void DeviceMediaAsyncFileUtil::OnReadDirectoryError(
    const AsyncFileUtil::ReadDirectoryCallback& callback,
    base::PlatformFileError error) {
  if (!callback.is_null())
    callback.Run(error, AsyncFileUtil::EntryList(), false /*no more*/);
}

void DeviceMediaAsyncFileUtil::OnDidCreateSnapshotFile(
    const AsyncFileUtil::CreateSnapshotFileCallback& callback,
    const base::PlatformFileInfo& file_info,
    const base::FilePath& platform_path) {
  if (!callback.is_null())
    callback.Run(base::PLATFORM_FILE_OK, file_info, platform_path,
                 kSnapshotFileTemporary);
}

void DeviceMediaAsyncFileUtil::OnCreateSnapshotFileError(
    const AsyncFileUtil::CreateSnapshotFileCallback& callback,
    base::PlatformFileError error) {
  if (!callback.is_null())
    callback.Run(error, base::PlatformFileInfo(), base::FilePath(),
                 kSnapshotFileTemporary);
}

void DeviceMediaAsyncFileUtil::OnSnapshotFileCreatedRunTask(
    FileSystemOperationContext* context,
    const AsyncFileUtil::CreateSnapshotFileCallback& callback,
    const base::FilePath& device_file_path,
    base::FilePath* snapshot_file_path) {
  DCHECK(IsOnIOThread(context));
  if (!snapshot_file_path || snapshot_file_path->empty()) {
    OnCreateSnapshotFileError(callback, base::PLATFORM_FILE_ERROR_FAILED);
    return;
  }
  MTPDeviceAsyncDelegate* delegate = GetMTPDeviceDelegate(context);
  if (!delegate) {
    OnCreateSnapshotFileError(callback, base::PLATFORM_FILE_ERROR_NOT_FOUND);
    return;
  }
  delegate->CreateSnapshotFile(
      device_file_path,
      *snapshot_file_path,
      base::Bind(&DeviceMediaAsyncFileUtil::OnDidCreateSnapshotFile,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback),
      base::Bind(&DeviceMediaAsyncFileUtil::OnCreateSnapshotFileError,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback));
}

}  // namespace fileapi
