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
#include "webkit/fileapi/quota_file_util.h"

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
  return QuotaFileUtil::GetInstance()->CreateOrOpen(
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
  return QuotaFileUtil::GetInstance()->EnsureFileExists(
      context, local_path, created);
}

PlatformFileError LocalFileSystemFileUtil::GetLocalFilePath(
    FileSystemOperationContext* context,
    const FilePath& virtual_path,
    FilePath* local_path) {
  FilePath path =
      GetLocalPath(context, context->src_origin_url(), context->src_type(),
                   virtual_path);
  if (path.empty())
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;

  *local_path = path;
  return base::PLATFORM_FILE_OK;
}

PlatformFileError LocalFileSystemFileUtil::GetFileInfo(
    FileSystemOperationContext* context,
    const FilePath& file_path,
    base::PlatformFileInfo* file_info,
    FilePath* platform_file_path) {
  FilePath local_path =
      GetLocalPath(context, context->src_origin_url(), context->src_type(),
          file_path);
  if (local_path.empty())
    return base::PLATFORM_FILE_ERROR_INVALID_OPERATION;
  return QuotaFileUtil::GetInstance()->GetFileInfo(
      context, local_path, file_info, platform_file_path);
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
  return QuotaFileUtil::GetInstance()->ReadDirectory(
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
  return QuotaFileUtil::GetInstance()->CreateDirectory(
      context, local_path, exclusive, recursive);
}

PlatformFileError LocalFileSystemFileUtil::CopyOrMoveFile(
    FileSystemOperationContext* context,
    const FilePath& src_file_path,
    const FilePath& dest_file_path,
    bool copy) {
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
  return QuotaFileUtil::GetInstance()->CopyOrMoveFile(
      context, local_src_path, local_dest_path, copy);
}

PlatformFileError LocalFileSystemFileUtil::DeleteFile(
    FileSystemOperationContext* context,
    const FilePath& file_path) {
  FilePath local_path =
      GetLocalPath(context, context->src_origin_url(), context->src_type(),
          file_path);
  if (local_path.empty())
    return base::PLATFORM_FILE_ERROR_INVALID_OPERATION;
  return QuotaFileUtil::GetInstance()->DeleteFile(
      context, local_path);
}

PlatformFileError LocalFileSystemFileUtil::DeleteSingleDirectory(
    FileSystemOperationContext* context,
    const FilePath& file_path) {
  FilePath local_path =
      GetLocalPath(context, context->src_origin_url(), context->src_type(),
          file_path);
  if (local_path.empty())
    return base::PLATFORM_FILE_ERROR_INVALID_OPERATION;
  return QuotaFileUtil::GetInstance()->DeleteSingleDirectory(
      context, local_path);
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
  return QuotaFileUtil::GetInstance()->Touch(
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
  return QuotaFileUtil::GetInstance()->Truncate(
      context, local_path, length);
}

bool LocalFileSystemFileUtil::PathExists(
    FileSystemOperationContext* context,
    const FilePath& file_path) {
  FilePath local_path =
      GetLocalPath(context, context->src_origin_url(), context->src_type(),
          file_path);
  if (local_path.empty())
    return false;
  return QuotaFileUtil::GetInstance()->PathExists(
      context, local_path);
}

bool LocalFileSystemFileUtil::DirectoryExists(
    FileSystemOperationContext* context,
    const FilePath& file_path) {
  FilePath local_path =
      GetLocalPath(context, context->src_origin_url(), context->src_type(),
          file_path);
  if (local_path.empty())
    return false;
  return QuotaFileUtil::GetInstance()->DirectoryExists(
      context, local_path);
}

bool LocalFileSystemFileUtil::IsDirectoryEmpty(
    FileSystemOperationContext* context,
    const FilePath& file_path) {
  FilePath local_path =
      GetLocalPath(context, context->src_origin_url(), context->src_type(),
          file_path);
  if (local_path.empty())
    return true;
  return QuotaFileUtil::GetInstance()->IsDirectoryEmpty(
      context, local_path);
}

class LocalFileSystemFileEnumerator
    : public FileSystemFileUtil::AbstractFileEnumerator {
 public:
  LocalFileSystemFileEnumerator(const FilePath& platform_root_path,
                                const FilePath& virtual_root_path,
                                bool recursive,
                                file_util::FileEnumerator::FILE_TYPE file_type)
      : file_enum_(platform_root_path, recursive, file_type),
        platform_root_path_(platform_root_path),
        virtual_root_path_(virtual_root_path) {
  }

  ~LocalFileSystemFileEnumerator() {}

  virtual FilePath Next();
  virtual bool IsDirectory();

 private:
  file_util::FileEnumerator file_enum_;
  FilePath platform_root_path_;
  FilePath virtual_root_path_;
};

FilePath LocalFileSystemFileEnumerator::Next() {
  FilePath next = file_enum_.Next();
  if (next.empty())
    return next;

  FilePath path;
  platform_root_path_.AppendRelativePath(next, &path);
  return virtual_root_path_.Append(path);
}

bool LocalFileSystemFileEnumerator::IsDirectory() {
  file_util::FileEnumerator::FindInfo file_util_info;
  file_enum_.GetFindInfo(&file_util_info);
  return file_util::FileEnumerator::IsDirectory(file_util_info);
}

FileSystemFileUtil::AbstractFileEnumerator*
LocalFileSystemFileUtil::CreateFileEnumerator(
    FileSystemOperationContext* context,
    const FilePath& root_path) {
  FilePath local_path =
      GetLocalPath(context, context->src_origin_url(), context->src_type(),
          root_path);
  if (local_path.empty())
    return new EmptyFileEnumerator();
  return new LocalFileSystemFileEnumerator(
      local_path, root_path, true,
      static_cast<file_util::FileEnumerator::FILE_TYPE>(
          file_util::FileEnumerator::FILES |
          file_util::FileEnumerator::DIRECTORIES));
}

FilePath LocalFileSystemFileUtil::GetLocalPath(
    FileSystemOperationContext* context,
    const GURL& origin_url,
    FileSystemType type,
    const FilePath& virtual_path) {
  FilePath root = context->file_system_context()->path_manager()->
      ValidateFileSystemRootAndGetPathOnFileThread(origin_url, type,
          virtual_path, false);
  if (root.empty())
    return FilePath();
  return root.Append(virtual_path);
}

}  // namespace fileapi
