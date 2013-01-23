// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/media/native_media_file_util.h"

#include "webkit/fileapi/file_system_operation_context.h"
#include "webkit/fileapi/media/media_path_filter.h"
#include "webkit/fileapi/media/filtering_file_enumerator.h"
#include "webkit/fileapi/native_file_util.h"

using base::PlatformFile;
using base::PlatformFileError;
using base::PlatformFileInfo;

namespace fileapi {

NativeMediaFileUtil::NativeMediaFileUtil() {
}

PlatformFileError NativeMediaFileUtil::CreateOrOpen(
    FileSystemOperationContext* context,
    const FileSystemURL& url,
    int file_flags,
    PlatformFile* file_handle,
    bool* created) {
  // Only called by NaCl, which should not have access to media file systems.
  return base::PLATFORM_FILE_ERROR_SECURITY;
}

PlatformFileError NativeMediaFileUtil::EnsureFileExists(
    FileSystemOperationContext* context,
    const FileSystemURL& url, bool* created) {
  FilePath file_path;
  PlatformFileError error = GetFilteredLocalFilePath(context, url, &file_path);
  if (error != base::PLATFORM_FILE_OK)
    return error;
  return NativeFileUtil::EnsureFileExists(file_path, created);
}

scoped_ptr<FileSystemFileUtil::AbstractFileEnumerator>
NativeMediaFileUtil::CreateFileEnumerator(
    FileSystemOperationContext* context,
    const FileSystemURL& root_url,
    bool recursive) {
  DCHECK(context);
  return make_scoped_ptr(new FilteringFileEnumerator(
      IsolatedFileUtil::CreateFileEnumerator(context, root_url, recursive),
      context->media_path_filter()))
      .PassAs<FileSystemFileUtil::AbstractFileEnumerator>();
}

PlatformFileError NativeMediaFileUtil::Touch(
    FileSystemOperationContext* context,
    const FileSystemURL& url,
    const base::Time& last_access_time,
    const base::Time& last_modified_time) {
  FilePath file_path;
  PlatformFileError error = GetFilteredLocalFilePathForExistingFileOrDirectory(
      context,
      url,
      // Touch fails for non-existent paths and filtered paths.
      base::PLATFORM_FILE_ERROR_FAILED,
      &file_path);
  if (error != base::PLATFORM_FILE_OK)
    return error;
  return NativeFileUtil::Touch(file_path, last_access_time, last_modified_time);
}

PlatformFileError NativeMediaFileUtil::Truncate(
    FileSystemOperationContext* context,
    const FileSystemURL& url,
    int64 length) {
  FilePath file_path;
  PlatformFileError error = GetFilteredLocalFilePathForExistingFileOrDirectory(
      context,
      url,
      // Cannot truncate paths that do not exist, or are filtered.
      base::PLATFORM_FILE_ERROR_NOT_FOUND,
      &file_path);
  if (error != base::PLATFORM_FILE_OK)
    return error;
  return NativeFileUtil::Truncate(file_path, length);
}

PlatformFileError NativeMediaFileUtil::CopyOrMoveFile(
    FileSystemOperationContext* context,
    const FileSystemURL& src_url,
    const FileSystemURL& dest_url,
    bool copy) {
  FilePath src_file_path;
  PlatformFileError error =
      GetFilteredLocalFilePathForExistingFileOrDirectory(
          context, src_url,
          base::PLATFORM_FILE_ERROR_NOT_FOUND,
          &src_file_path);
  if (error != base::PLATFORM_FILE_OK)
    return error;
  if (NativeFileUtil::DirectoryExists(src_file_path))
    return base::PLATFORM_FILE_ERROR_NOT_A_FILE;

  FilePath dest_file_path;
  error = GetLocalFilePath(context, dest_url, &dest_file_path);
  if (error != base::PLATFORM_FILE_OK)
    return error;
  PlatformFileInfo file_info;
  error = NativeFileUtil::GetFileInfo(dest_file_path, &file_info);
  if (error != base::PLATFORM_FILE_OK &&
      error != base::PLATFORM_FILE_ERROR_NOT_FOUND)
    return error;
  if (error == base::PLATFORM_FILE_OK && file_info.is_directory)
    return base::PLATFORM_FILE_ERROR_INVALID_OPERATION;
  if (!context->media_path_filter()->Match(dest_file_path))
    return base::PLATFORM_FILE_ERROR_SECURITY;

  return NativeFileUtil::CopyOrMoveFile(src_file_path, dest_file_path, copy);
}

PlatformFileError NativeMediaFileUtil::CopyInForeignFile(
    FileSystemOperationContext* context,
    const FilePath& src_file_path,
    const FileSystemURL& dest_url) {
  if (src_file_path.empty())
    return base::PLATFORM_FILE_ERROR_INVALID_OPERATION;

  FilePath dest_file_path;
  PlatformFileError error =
      GetFilteredLocalFilePath(context, dest_url, &dest_file_path);
  if (error != base::PLATFORM_FILE_OK)
    return error;
  return NativeFileUtil::CopyOrMoveFile(src_file_path, dest_file_path, true);
}

PlatformFileError NativeMediaFileUtil::DeleteFile(
    FileSystemOperationContext* context,
    const FileSystemURL& url) {
  FilePath file_path;
  PlatformFileError error = GetLocalFilePath(context, url, &file_path);
  if (error != base::PLATFORM_FILE_OK)
    return error;
  PlatformFileInfo file_info;
  error = NativeFileUtil::GetFileInfo(file_path, &file_info);
  if (error != base::PLATFORM_FILE_OK)
    return error;
  if (file_info.is_directory)
    return base::PLATFORM_FILE_ERROR_NOT_A_FILE;
  if (!context->media_path_filter()->Match(file_path))
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;
  return NativeFileUtil::DeleteFile(file_path);
}

PlatformFileError NativeMediaFileUtil::GetFileInfo(
    FileSystemOperationContext* context,
    const FileSystemURL& url,
    PlatformFileInfo* file_info,
    FilePath* platform_path) {
  DCHECK(context);
  DCHECK(context->media_path_filter());
  DCHECK(file_info);
  DCHECK(platform_path);

  base::PlatformFileError error =
      IsolatedFileUtil::GetFileInfo(context, url, file_info, platform_path);
  if (error != base::PLATFORM_FILE_OK)
    return error;

  if (file_info->is_directory ||
      context->media_path_filter()->Match(*platform_path)) {
    return base::PLATFORM_FILE_OK;
  }
  return base::PLATFORM_FILE_ERROR_NOT_FOUND;
}

PlatformFileError NativeMediaFileUtil::GetFilteredLocalFilePath(
    FileSystemOperationContext* context,
    const FileSystemURL& file_system_url,
    FilePath* local_file_path) {
  FilePath file_path;
  PlatformFileError error =
      IsolatedFileUtil::GetLocalFilePath(context, file_system_url, &file_path);
  if (error != base::PLATFORM_FILE_OK)
    return error;
  if (!context->media_path_filter()->Match(file_path))
    return base::PLATFORM_FILE_ERROR_SECURITY;

  *local_file_path = file_path;
  return base::PLATFORM_FILE_OK;
}

PlatformFileError
NativeMediaFileUtil::GetFilteredLocalFilePathForExistingFileOrDirectory(
    FileSystemOperationContext* context,
    const FileSystemURL& file_system_url,
    PlatformFileError failure_error,
    FilePath* local_file_path) {
  FilePath file_path;
  PlatformFileError error =
      GetLocalFilePath(context, file_system_url, &file_path);
  if (error != base::PLATFORM_FILE_OK)
    return error;

  if (!file_util::PathExists(file_path))
    return failure_error;
  PlatformFileInfo file_info;
  if (!file_util::GetFileInfo(file_path, &file_info))
    return base::PLATFORM_FILE_ERROR_FAILED;

  if (!file_info.is_directory &&
      !context->media_path_filter()->Match(file_path)) {
    return failure_error;
  }

  *local_file_path = file_path;
  return base::PLATFORM_FILE_OK;
}

}  // namespace fileapi
