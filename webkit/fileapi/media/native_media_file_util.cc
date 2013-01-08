// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/media/native_media_file_util.h"

#include "webkit/fileapi/file_system_operation_context.h"
#include "webkit/fileapi/media/media_path_filter.h"
#include "webkit/fileapi/media/filtering_file_enumerator.h"

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
  // TODO(tzik): Apply context()->media_path_filter() here when we support write
  // access.
  return base::PLATFORM_FILE_ERROR_SECURITY;
}

PlatformFileError NativeMediaFileUtil::EnsureFileExists(
    FileSystemOperationContext* context,
    const FileSystemURL& url, bool* created) {
  // TODO(tzik): Apply context()->media_path_filter() here when we support write
  // access.
  return base::PLATFORM_FILE_ERROR_SECURITY;
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
  // TODO(tzik): Apply context()->media_path_filter() here when we support write
  // access.
  return base::PLATFORM_FILE_ERROR_SECURITY;
}

PlatformFileError NativeMediaFileUtil::Truncate(
    FileSystemOperationContext* context,
    const FileSystemURL& url,
    int64 length) {
  // TODO(tzik): Apply context()->media_path_filter() here when we support write
  // access.
  return base::PLATFORM_FILE_ERROR_SECURITY;
}

bool NativeMediaFileUtil::IsDirectoryEmpty(
    FileSystemOperationContext* context,
    const FileSystemURL& url) {
  DCHECK(context);
  DCHECK(context->media_path_filter());

  scoped_ptr<AbstractFileEnumerator> enumerator(
      CreateFileEnumerator(context, url, false));
  FilePath path;
  while (!(path = enumerator->Next()).empty()) {
    if (enumerator->IsDirectory() ||
        context->media_path_filter()->Match(path))
      return false;
  }
  return true;
}

PlatformFileError NativeMediaFileUtil::CopyOrMoveFile(
    FileSystemOperationContext* context,
    const FileSystemURL& src_url,
    const FileSystemURL& dest_url,
    bool copy) {
  // TODO(tzik): Apply context()->media_path_filter() here when we support write
  // access.
  return base::PLATFORM_FILE_ERROR_SECURITY;
}

PlatformFileError NativeMediaFileUtil::CopyInForeignFile(
    FileSystemOperationContext* context,
    const FilePath& src_file_path,
    const FileSystemURL& dest_url) {
  // TODO(tzik): Apply context()->media_path_filter() here when we support write
  // access.
  return base::PLATFORM_FILE_ERROR_SECURITY;
}

PlatformFileError NativeMediaFileUtil::DeleteFile(
    FileSystemOperationContext* context,
    const FileSystemURL& url) {
  return base::PLATFORM_FILE_ERROR_SECURITY;
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
      context->media_path_filter()->Match(*platform_path))
    return base::PLATFORM_FILE_OK;
  return base::PLATFORM_FILE_ERROR_NOT_FOUND;
}

}  // namespace fileapi
