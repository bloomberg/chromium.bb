// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/media/device_media_file_util.h"

#include "base/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop_proxy.h"
#include "webkit/blob/shareable_file_reference.h"
#include "webkit/fileapi/file_system_operation_context.h"
#include "webkit/fileapi/file_system_url.h"
#include "webkit/fileapi/isolated_context.h"
#include "webkit/fileapi/media/media_device_interface_impl.h"
#include "webkit/fileapi/media/media_device_map_service.h"

using base::PlatformFileError;
using base::PlatformFileInfo;
using webkit_blob::ShareableFileReference;

namespace fileapi {

namespace {

const FilePath::CharType kDeviceMediaFileUtilTempDir[] =
    FILE_PATH_LITERAL("DeviceMediaFileSystem");

}  // namespace

DeviceMediaFileUtil::DeviceMediaFileUtil(const FilePath& profile_path)
    : profile_path_(profile_path) {
}

PlatformFileError DeviceMediaFileUtil::CreateOrOpen(
    FileSystemOperationContext* context,
    const FileSystemURL& url, int file_flags,
    PlatformFile* file_handle, bool* created) {
  return base::PLATFORM_FILE_ERROR_SECURITY;
}

PlatformFileError DeviceMediaFileUtil::Close(
    FileSystemOperationContext* context,
    PlatformFile file_handle) {
  // We don't allow open thus Close won't be called.
  return base::PLATFORM_FILE_ERROR_SECURITY;
}

PlatformFileError DeviceMediaFileUtil::EnsureFileExists(
    FileSystemOperationContext* context,
    const FileSystemURL& url,
    bool* created) {
  return base::PLATFORM_FILE_ERROR_SECURITY;
}

PlatformFileError DeviceMediaFileUtil::CreateDirectory(
    FileSystemOperationContext* context,
    const FileSystemURL& url,
    bool exclusive,
    bool recursive) {
  return base::PLATFORM_FILE_ERROR_SECURITY;
}

PlatformFileError DeviceMediaFileUtil::GetFileInfo(
    FileSystemOperationContext* context,
    const FileSystemURL& url,
    PlatformFileInfo* file_info,
    FilePath* platform_path) {
  if (!context->media_device())
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;
  return context->media_device()->GetFileInfo(url.path(), file_info);
}

FileSystemFileUtil::AbstractFileEnumerator*
DeviceMediaFileUtil::CreateFileEnumerator(
    FileSystemOperationContext* context,
    const FileSystemURL& url,
    bool recursive) {
  if (!context->media_device())
    return new FileSystemFileUtil::EmptyFileEnumerator();
  return context->media_device()->CreateFileEnumerator(url.path(), recursive);
}

PlatformFileError DeviceMediaFileUtil::GetLocalFilePath(
    FileSystemOperationContext* context,
    const FileSystemURL& file_system_url,
    FilePath* local_file_path) {
  return base::PLATFORM_FILE_ERROR_SECURITY;
}

PlatformFileError DeviceMediaFileUtil::Touch(
    FileSystemOperationContext* context,
    const FileSystemURL& url,
    const base::Time& last_access_time,
    const base::Time& last_modified_time) {
  if (!context->media_device())
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;
  return context->media_device()->Touch(url.path(), last_access_time,
                                        last_modified_time);
}

PlatformFileError DeviceMediaFileUtil::Truncate(
    FileSystemOperationContext* context,
    const FileSystemURL& url,
    int64 length) {
  return base::PLATFORM_FILE_ERROR_SECURITY;
}

bool DeviceMediaFileUtil::PathExists(
    FileSystemOperationContext* context,
    const FileSystemURL& url) {
  if (!context->media_device())
    return false;
  return context->media_device()->PathExists(url.path());
}

bool DeviceMediaFileUtil::DirectoryExists(
    FileSystemOperationContext* context,
    const FileSystemURL& url) {
  if (!context->media_device())
    return false;
  return context->media_device()->DirectoryExists(url.path());
}

bool DeviceMediaFileUtil::IsDirectoryEmpty(
    FileSystemOperationContext* context,
    const FileSystemURL& url) {
  if (!context->media_device())
    return false;
  return context->media_device()->IsDirectoryEmpty(url.path());
}

PlatformFileError DeviceMediaFileUtil::CopyOrMoveFile(
    FileSystemOperationContext* context,
    const FileSystemURL& src_url,
    const FileSystemURL& dest_url,
    bool copy) {
  return base::PLATFORM_FILE_ERROR_SECURITY;
}

PlatformFileError DeviceMediaFileUtil::CopyInForeignFile(
    FileSystemOperationContext* context,
    const FilePath& src_file_path,
    const FileSystemURL& dest_url) {
  return base::PLATFORM_FILE_ERROR_SECURITY;
}

PlatformFileError DeviceMediaFileUtil::DeleteFile(
    FileSystemOperationContext* context,
    const FileSystemURL& url) {
  return base::PLATFORM_FILE_ERROR_SECURITY;
}

PlatformFileError DeviceMediaFileUtil::DeleteSingleDirectory(
    FileSystemOperationContext* context,
    const FileSystemURL& url) {
  return base::PLATFORM_FILE_ERROR_SECURITY;
}

scoped_refptr<ShareableFileReference> DeviceMediaFileUtil::CreateSnapshotFile(
    FileSystemOperationContext* context,
    const FileSystemURL& url,
    base::PlatformFileError* result,
    base::PlatformFileInfo* file_info,
    FilePath* local_path) {
  DCHECK(result);
  DCHECK(file_info);
  DCHECK(local_path);

  scoped_refptr<ShareableFileReference> file_ref;
  if (!context->media_device()) {
    *result = base::PLATFORM_FILE_ERROR_NOT_FOUND;
    return file_ref;
  }

  *result = base::PLATFORM_FILE_ERROR_FAILED;

  // Create a temp file in "profile_path_/kDeviceMediaFileUtilTempDir".
  FilePath isolated_media_file_system_dir_path =
      profile_path_.Append(kDeviceMediaFileUtilTempDir);
  bool dir_exists = file_util::DirectoryExists(
      isolated_media_file_system_dir_path);
  if (!dir_exists) {
    if (!file_util::CreateDirectory(isolated_media_file_system_dir_path))
      return file_ref;
  }

  bool file_created = file_util::CreateTemporaryFileInDir(
      isolated_media_file_system_dir_path, local_path);
  if (!file_created)
    return file_ref;

  *result = context->media_device()->CreateSnapshotFile(url.path(), *local_path,
                                                        file_info);
  if (*result == base::PLATFORM_FILE_OK) {
    file_ref = ShareableFileReference::GetOrCreate(
        *local_path, ShareableFileReference::DELETE_ON_FINAL_RELEASE,
        context->file_task_runner());
  }
  return file_ref;
}

}  // namespace fileapi
