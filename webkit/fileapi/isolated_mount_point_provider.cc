// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/isolated_mount_point_provider.h"

#include <string>

#include "base/bind.h"
#include "base/file_path.h"
#include "base/logging.h"
#include "base/message_loop_proxy.h"
#include "base/platform_file.h"
#include "base/sequenced_task_runner.h"
#include "webkit/blob/local_file_stream_reader.h"
#include "webkit/fileapi/file_system_callback_dispatcher.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_file_stream_reader.h"
#include "webkit/fileapi/file_system_task_runners.h"
#include "webkit/fileapi/file_system_types.h"
#include "webkit/fileapi/file_system_util.h"
#include "webkit/fileapi/isolated_context.h"
#include "webkit/fileapi/isolated_file_util.h"
#include "webkit/fileapi/local_file_stream_writer.h"
#include "webkit/fileapi/local_file_system_operation.h"
#include "webkit/fileapi/media/media_file_system_config.h"
#include "webkit/fileapi/media/media_path_filter.h"
#include "webkit/fileapi/media/native_media_file_util.h"
#include "webkit/fileapi/native_file_util.h"

#if defined(SUPPORT_MEDIA_FILESYSTEM)
#include "webkit/fileapi/media/device_media_file_util.h"
#include "webkit/fileapi/media/media_device_map_service.h"
#endif

namespace fileapi {

namespace {

IsolatedContext* isolated_context() {
  return IsolatedContext::GetInstance();
}

}  // namespace

IsolatedMountPointProvider::IsolatedMountPointProvider(
    const FilePath& profile_path)
    : profile_path_(profile_path),
      media_path_filter_(new MediaPathFilter()),
      isolated_file_util_(new IsolatedFileUtil()),
      dragged_file_util_(new DraggedFileUtil()),
      native_media_file_util_(new NativeMediaFileUtil()) {
#if defined(SUPPORT_MEDIA_FILESYSTEM)
  // TODO(kmadhusu): Initialize |device_media_file_util_| in
  // initialization list.
  device_media_file_util_.reset(new DeviceMediaFileUtil(profile_path_));
#endif
}

IsolatedMountPointProvider::~IsolatedMountPointProvider() {
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

FilePath IsolatedMountPointProvider::GetFileSystemRootPathOnFileThread(
    const GURL& origin_url,
    FileSystemType type,
    const FilePath& virtual_path,
    bool create) {
  // This is not supposed to be used.
  NOTREACHED();
  return FilePath();
}

bool IsolatedMountPointProvider::IsAccessAllowed(const FileSystemURL& url) {
  return true;
}

bool IsolatedMountPointProvider::IsRestrictedFileName(
    const FilePath& filename) const {
  // TODO(kinuko): We need to check platform-specific restricted file names
  // before we actually start allowing file creation in isolated file systems.
  return false;
}

FileSystemFileUtil* IsolatedMountPointProvider::GetFileUtil(
    FileSystemType type) {
  switch (type) {
    case kFileSystemTypeNativeLocal:
      return isolated_file_util_.get();
    case kFileSystemTypeDragged:
      return dragged_file_util_.get();
    case kFileSystemTypeNativeMedia:
      return native_media_file_util_.get();
    case kFileSystemTypeDeviceMedia:
#if defined(SUPPORT_MEDIA_FILESYSTEM)
      return device_media_file_util_.get();
#endif

    default:
      NOTREACHED();
  }
  return NULL;
}

FilePath IsolatedMountPointProvider::GetPathForPermissionsCheck(
    const FilePath& virtual_path) const {
  // For isolated filesystems we only check per-filesystem permissions.
  return FilePath();
}

FileSystemOperation* IsolatedMountPointProvider::CreateFileSystemOperation(
    const FileSystemURL& url,
    FileSystemContext* context,
    base::PlatformFileError* error_code) const {
  scoped_ptr<FileSystemOperationContext> operation_context(
      new FileSystemOperationContext(context));
  if (url.type() == kFileSystemTypeNativeMedia) {
    operation_context->set_media_path_filter(media_path_filter_.get());
    operation_context->set_task_runner(
        context->task_runners()->media_task_runner());
  }

#if defined(SUPPORT_MEDIA_FILESYSTEM)
  if (url.type() == kFileSystemTypeDeviceMedia) {
    MediaDeviceMapService* map_service = MediaDeviceMapService::GetInstance();
    MediaDeviceDelegate* device_delegate =
        map_service->GetMediaDeviceDelegate(url.filesystem_id());
    if (!device_delegate) {
      if (error_code)
        *error_code = base::PLATFORM_FILE_ERROR_NOT_FOUND;
      return NULL;
    }
    operation_context->set_media_device_delegate(device_delegate);
    operation_context->set_task_runner(device_delegate->media_task_runner());
    operation_context->set_media_path_filter(media_path_filter_.get());
  }
#endif

  return new LocalFileSystemOperation(context, operation_context.Pass());
}

webkit_blob::FileStreamReader*
IsolatedMountPointProvider::CreateFileStreamReader(
    const FileSystemURL& url,
    int64 offset,
    FileSystemContext* context) const {
  return new webkit_blob::LocalFileStreamReader(
      context->task_runners()->file_task_runner(),
      url.path(), offset, base::Time());
}

FileStreamWriter* IsolatedMountPointProvider::CreateFileStreamWriter(
    const FileSystemURL& url,
    int64 offset,
    FileSystemContext* context) const {
  return new LocalFileStreamWriter(url.path(), offset);
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
