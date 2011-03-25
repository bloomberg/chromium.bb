// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/local_file_system_file_util.h"

#include "base/file_util_proxy.h"
#include "googleurl/src/gurl.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_operation_context.h"
#include "webkit/fileapi/file_system_path_manager.h"
#include "webkit/fileapi/file_system_types.h"
#include "webkit/fileapi/file_system_util.h"

namespace fileapi {

LocalFileSystemFileUtil* LocalFileSystemFileUtil::GetInstance() {
  return Singleton<LocalFileSystemFileUtil>::get();
}

PlatformFileError LocalFileSystemFileUtil::CreateOrOpen(
    FileSystemOperationContext* context,
    const FilePath& file_path, int file_flags,
    PlatformFile* file_handle, bool* created) {
  FilePath local_path =
      GetLocalPath(context, context->src_origin_url(), context->src_type(),
          file_path);
  if (local_path.empty())
    return base::PLATFORM_FILE_ERROR_INVALID_OPERATION;
  return FileSystemFileUtil::GetInstance()->CreateOrOpen(
      context, local_path, file_flags, file_handle, created);
}

PlatformFileError LocalFileSystemFileUtil::EnsureFileExists(
    FileSystemOperationContext* context,
    const FilePath& file_path,
    bool* created) {
  FilePath local_path =
      GetLocalPath(context, context->src_origin_url(), context->src_type(),
          file_path);
  if (local_path.empty())
    return base::PLATFORM_FILE_ERROR_INVALID_OPERATION;
  return FileSystemFileUtil::GetInstance()->EnsureFileExists(
      context, local_path, created);
}

PlatformFileError LocalFileSystemFileUtil::GetFileInfo(
    FileSystemOperationContext* context,
    const FilePath& file_path,
    base::PlatformFileInfo* file_info) {
  FilePath local_path =
      GetLocalPath(context, context->src_origin_url(), context->src_type(),
          file_path);
  if (local_path.empty())
    return base::PLATFORM_FILE_ERROR_INVALID_OPERATION;
  return FileSystemFileUtil::GetInstance()->GetFileInfo(
      context, local_path, file_info);
}

PlatformFileError LocalFileSystemFileUtil::ReadDirectory(
    FileSystemOperationContext* context,
    const FilePath& file_path,
    std::vector<base::FileUtilProxy::Entry>* entries) {
  // TODO(kkanetkar): Implement directory read in multiple chunks.
  FilePath local_path =
      GetLocalPath(context, context->src_origin_url(), context->src_type(),
          file_path);
  if (local_path.empty())
    return base::PLATFORM_FILE_ERROR_INVALID_OPERATION;
  return FileSystemFileUtil::GetInstance()->ReadDirectory(
      context, local_path, entries);
}

PlatformFileError LocalFileSystemFileUtil::CreateDirectory(
    FileSystemOperationContext* context,
    const FilePath& file_path,
    bool exclusive,
    bool recursive) {
  FilePath local_path =
      GetLocalPath(context, context->src_origin_url(), context->src_type(),
          file_path);
  if (local_path.empty())
    return base::PLATFORM_FILE_ERROR_INVALID_OPERATION;
  return FileSystemFileUtil::GetInstance()->CreateDirectory(
      context, local_path, exclusive, recursive);
}

PlatformFileError LocalFileSystemFileUtil::Copy(
    FileSystemOperationContext* context,
    const FilePath& src_file_path,
    const FilePath& dest_file_path) {
  // TODO(ericu): If they share a root URL, this could be optimized.
  FilePath local_src_path =
      GetLocalPath(context, context->src_origin_url(), context->src_type(),
          src_file_path);
  if (local_src_path.empty())
    return base::PLATFORM_FILE_ERROR_INVALID_OPERATION;
  FilePath local_dest_path =
      GetLocalPath(context, context->dest_origin_url(), context->dest_type(),
          dest_file_path);
  if (local_dest_path.empty())
    return base::PLATFORM_FILE_ERROR_INVALID_OPERATION;
  return FileSystemFileUtil::GetInstance()->Copy(
      context, local_src_path, local_dest_path);
}

PlatformFileError LocalFileSystemFileUtil::Move(
    FileSystemOperationContext* context,
    const FilePath& src_file_path,
    const FilePath& dest_file_path) {
  // TODO(ericu): If they share a root URL, this could be optimized.
  FilePath local_src_path =
      GetLocalPath(context, context->src_origin_url(), context->src_type(),
          src_file_path);
  if (local_src_path.empty())
    return base::PLATFORM_FILE_ERROR_INVALID_OPERATION;
  FilePath local_dest_path =
      GetLocalPath(context, context->dest_origin_url(), context->dest_type(),
          dest_file_path);
  if (local_dest_path.empty())
    return base::PLATFORM_FILE_ERROR_INVALID_OPERATION;
  return FileSystemFileUtil::GetInstance()->Move(
      context, local_src_path, local_dest_path);
}

PlatformFileError LocalFileSystemFileUtil::Delete(
    FileSystemOperationContext* context,
    const FilePath& file_path,
    bool recursive) {
  FilePath local_path =
      GetLocalPath(context, context->src_origin_url(), context->src_type(),
          file_path);
  if (local_path.empty())
    return base::PLATFORM_FILE_ERROR_INVALID_OPERATION;
  return FileSystemFileUtil::GetInstance()->Delete(
      context, local_path, recursive);
}

PlatformFileError LocalFileSystemFileUtil::Touch(
    FileSystemOperationContext* context,
    const FilePath& file_path,
    const base::Time& last_access_time,
    const base::Time& last_modified_time) {
  FilePath local_path =
      GetLocalPath(context, context->src_origin_url(), context->src_type(),
          file_path);
  if (local_path.empty())
    return base::PLATFORM_FILE_ERROR_INVALID_OPERATION;
  return FileSystemFileUtil::GetInstance()->Touch(
      context, local_path, last_access_time, last_modified_time);
}

PlatformFileError LocalFileSystemFileUtil::Truncate(
    FileSystemOperationContext* context,
    const FilePath& file_path,
    int64 length) {
  FilePath local_path =
      GetLocalPath(context, context->src_origin_url(), context->src_type(),
          file_path);
  if (local_path.empty())
    return base::PLATFORM_FILE_ERROR_INVALID_OPERATION;
  return FileSystemFileUtil::GetInstance()->Truncate(
      context, local_path, length);
}

FilePath LocalFileSystemFileUtil::GetLocalPath(
    FileSystemOperationContext* context,
    const GURL& origin_url,
    FileSystemType type,
    const FilePath& virtual_path) {
  FilePath root = context->file_system_context()->path_manager()->
      GetFileSystemRootPathOnFileThread(origin_url, type, virtual_path, false);
  if (root.empty())
    return FilePath();
  return root.Append(virtual_path);
}

}  // namespace fileapi
