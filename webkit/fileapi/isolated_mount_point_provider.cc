// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/isolated_mount_point_provider.h"

#include <string>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/message_loop_proxy.h"
#include "base/platform_file.h"
#include "base/sequenced_task_runner.h"
#include "webkit/blob/local_file_stream_reader.h"
#include "webkit/fileapi/async_file_util_adapter.h"
#include "webkit/fileapi/copy_or_move_file_validator.h"
#include "webkit/fileapi/file_system_callback_dispatcher.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_file_stream_reader.h"
#include "webkit/fileapi/file_system_operation_context.h"
#include "webkit/fileapi/file_system_task_runners.h"
#include "webkit/fileapi/file_system_types.h"
#include "webkit/fileapi/file_system_util.h"
#include "webkit/fileapi/isolated_context.h"
#include "webkit/fileapi/isolated_file_util.h"
#include "webkit/fileapi/local_file_stream_writer.h"
#include "webkit/fileapi/local_file_system_operation.h"
#include "webkit/fileapi/native_file_util.h"

namespace fileapi {

IsolatedMountPointProvider::IsolatedMountPointProvider()
    : isolated_file_util_(new AsyncFileUtilAdapter(new IsolatedFileUtil())),
      dragged_file_util_(new AsyncFileUtilAdapter(new DraggedFileUtil())) {
}

IsolatedMountPointProvider::~IsolatedMountPointProvider() {
}

bool IsolatedMountPointProvider::CanHandleType(FileSystemType type) const {
  switch (type) {
    case kFileSystemTypeIsolated:
    case kFileSystemTypeDragged:
      return true;
#if !defined(OS_CHROMEOS)
    case kFileSystemTypeNativeLocal:
    case kFileSystemTypeNativeForPlatformApp:
      return true;
#endif
    default:
      return false;
  }
}

void IsolatedMountPointProvider::ValidateFileSystemRoot(
    const GURL& origin_url,
    FileSystemType type,
    bool create,
    const ValidateFileSystemCallback& callback) {
  // We never allow opening a new isolated FileSystem via usual OpenFileSystem.
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, base::PLATFORM_FILE_ERROR_SECURITY));
}

base::FilePath IsolatedMountPointProvider::GetFileSystemRootPathOnFileThread(
    const FileSystemURL& url,
    bool create) {
  // This is not supposed to be used.
  NOTREACHED();
  return base::FilePath();
}

FileSystemFileUtil* IsolatedMountPointProvider::GetFileUtil(
    FileSystemType type) {
  switch (type) {
    case kFileSystemTypeNativeLocal:
      return isolated_file_util_->sync_file_util();
    case kFileSystemTypeDragged:
      return dragged_file_util_->sync_file_util();
    default:
      NOTREACHED();
  }
  return NULL;
}

AsyncFileUtil* IsolatedMountPointProvider::GetAsyncFileUtil(
    FileSystemType type) {
  switch (type) {
    case kFileSystemTypeNativeLocal:
      return isolated_file_util_.get();
    case kFileSystemTypeDragged:
      return dragged_file_util_.get();
    default:
      NOTREACHED();
  }
  return NULL;
}

CopyOrMoveFileValidatorFactory*
IsolatedMountPointProvider::GetCopyOrMoveFileValidatorFactory(
    FileSystemType type, base::PlatformFileError* error_code) {
  DCHECK(error_code);
  *error_code = base::PLATFORM_FILE_OK;
  return NULL;
}

void IsolatedMountPointProvider::InitializeCopyOrMoveFileValidatorFactory(
    FileSystemType type,
    scoped_ptr<CopyOrMoveFileValidatorFactory> factory) {
  DCHECK(!factory);
}

FilePermissionPolicy IsolatedMountPointProvider::GetPermissionPolicy(
    const FileSystemURL& url, int permissions) const {
  if (url.type() == kFileSystemTypeDragged && url.path().empty()) {
    // The root directory of the dragged filesystem must be always read-only.
    if (permissions & ~fileapi::kReadFilePermissions)
      return FILE_PERMISSION_ALWAYS_DENY;
  }
  // Access to isolated file systems should be checked using per-filesystem
  // access permission.
  return FILE_PERMISSION_USE_FILESYSTEM_PERMISSION;
}

FileSystemOperation* IsolatedMountPointProvider::CreateFileSystemOperation(
    const FileSystemURL& url,
    FileSystemContext* context,
    base::PlatformFileError* error_code) const {
  return new LocalFileSystemOperation(
      context, make_scoped_ptr(new FileSystemOperationContext(context)));
}

scoped_ptr<webkit_blob::FileStreamReader>
IsolatedMountPointProvider::CreateFileStreamReader(
    const FileSystemURL& url,
    int64 offset,
    const base::Time& expected_modification_time,
    FileSystemContext* context) const {
  return scoped_ptr<webkit_blob::FileStreamReader>(
      new webkit_blob::LocalFileStreamReader(
          context->task_runners()->file_task_runner(),
          url.path(), offset, expected_modification_time));
}

scoped_ptr<FileStreamWriter> IsolatedMountPointProvider::CreateFileStreamWriter(
    const FileSystemURL& url,
    int64 offset,
    FileSystemContext* context) const {
  return scoped_ptr<FileStreamWriter>(
      new LocalFileStreamWriter(url.path(), offset));
}

FileSystemQuotaUtil* IsolatedMountPointProvider::GetQuotaUtil() {
  // No quota support.
  return NULL;
}

void IsolatedMountPointProvider::DeleteFileSystem(
    const GURL& origin_url,
    FileSystemType type,
    FileSystemContext* context,
    const DeleteFileSystemCallback& callback) {
  NOTREACHED();
  callback.Run(base::PLATFORM_FILE_ERROR_INVALID_OPERATION);
}

}  // namespace fileapi
