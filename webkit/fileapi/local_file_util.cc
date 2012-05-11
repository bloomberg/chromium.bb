// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/local_file_util.h"

#include "base/file_util_proxy.h"
#include "googleurl/src/gurl.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_mount_point_provider.h"
#include "webkit/fileapi/file_system_operation_context.h"
#include "webkit/fileapi/file_system_path.h"
#include "webkit/fileapi/file_system_types.h"
#include "webkit/fileapi/file_system_util.h"

namespace fileapi {

class LocalFileEnumerator : public FileSystemFileUtil::AbstractFileEnumerator {
 public:
  LocalFileEnumerator(const FilePath& platform_root_path,
                      const FilePath& virtual_root_path,
                      bool recursive,
                      file_util::FileEnumerator::FileType file_type)
      : file_enum_(platform_root_path, recursive, file_type),
        platform_root_path_(platform_root_path),
        virtual_root_path_(virtual_root_path) {
#if defined(OS_WIN)
    memset(&file_util_info_, 0, sizeof(file_util_info_));
#endif  // defined(OS_WIN)
  }

  ~LocalFileEnumerator() {}

  virtual FilePath Next() OVERRIDE;
  virtual int64 Size() OVERRIDE;
  virtual base::Time LastModifiedTime() OVERRIDE;
  virtual bool IsDirectory() OVERRIDE;
  virtual bool IsLink() OVERRIDE;

 private:
  file_util::FileEnumerator file_enum_;
  file_util::FileEnumerator::FindInfo file_util_info_;
  FilePath platform_root_path_;
  FilePath virtual_root_path_;
};

FilePath LocalFileEnumerator::Next() {
  FilePath next = file_enum_.Next();
  if (next.empty())
    return next;
  file_enum_.GetFindInfo(&file_util_info_);

  FilePath path;
  platform_root_path_.AppendRelativePath(next, &path);
  return virtual_root_path_.Append(path);
}

int64 LocalFileEnumerator::Size() {
  return file_util::FileEnumerator::GetFilesize(file_util_info_);
}

base::Time LocalFileEnumerator::LastModifiedTime() {
  return file_util::FileEnumerator::GetLastModifiedTime(file_util_info_);
}

bool LocalFileEnumerator::IsDirectory() {
  return file_util::FileEnumerator::IsDirectory(file_util_info_);
}

bool LocalFileEnumerator::IsLink() {
  return file_util::FileEnumerator::IsLink(file_util_info_);
}

LocalFileUtil::LocalFileUtil(FileSystemFileUtil* underlying_file_util)
    : FileSystemFileUtil(underlying_file_util) {
}

LocalFileUtil::~LocalFileUtil() {
}

PlatformFileError LocalFileUtil::CreateOrOpen(
    FileSystemOperationContext* context,
    const FileSystemPath& path, int file_flags,
    PlatformFile* file_handle, bool* created) {
  FileSystemPath local_path = GetLocalPath(context, path);
  if (local_path.internal_path().empty())
    return base::PLATFORM_FILE_ERROR_INVALID_OPERATION;
  return underlying_file_util()->CreateOrOpen(
      context, local_path, file_flags, file_handle, created);
}

PlatformFileError LocalFileUtil::EnsureFileExists(
    FileSystemOperationContext* context,
    const FileSystemPath& path,
    bool* created) {
  FileSystemPath local_path = GetLocalPath(context, path);
  if (local_path.internal_path().empty())
    return base::PLATFORM_FILE_ERROR_INVALID_OPERATION;
  return underlying_file_util()->EnsureFileExists(
      context, local_path, created);
}

PlatformFileError LocalFileUtil::CreateDirectory(
    FileSystemOperationContext* context,
    const FileSystemPath& path,
    bool exclusive,
    bool recursive) {
  FileSystemPath local_path = GetLocalPath(context, path);
  if (local_path.internal_path().empty())
    return base::PLATFORM_FILE_ERROR_INVALID_OPERATION;
  return underlying_file_util()->CreateDirectory(
      context, local_path, exclusive, recursive);
}

PlatformFileError LocalFileUtil::GetFileInfo(
    FileSystemOperationContext* context,
    const FileSystemPath& path,
    base::PlatformFileInfo* file_info,
    FilePath* platform_file_path) {
  FileSystemPath local_path = GetLocalPath(context, path);
  if (local_path.internal_path().empty())
    return base::PLATFORM_FILE_ERROR_INVALID_OPERATION;
  return underlying_file_util()->GetFileInfo(
      context, local_path, file_info, platform_file_path);
}

FileSystemFileUtil::AbstractFileEnumerator* LocalFileUtil::CreateFileEnumerator(
    FileSystemOperationContext* context,
    const FileSystemPath& root_path,
    bool recursive) {
  FileSystemPath local_path = GetLocalPath(context, root_path);
  if (local_path.internal_path().empty())
    return new EmptyFileEnumerator();
  return new LocalFileEnumerator(
      local_path.internal_path(), root_path.internal_path(), recursive,
      static_cast<file_util::FileEnumerator::FileType>(
          file_util::FileEnumerator::FILES |
          file_util::FileEnumerator::DIRECTORIES));
}

PlatformFileError LocalFileUtil::GetLocalFilePath(
    FileSystemOperationContext* context,
    const FileSystemPath& file_system_path,
    FilePath* local_file_path) {
  FileSystemPath local_path = GetLocalPath(context, file_system_path);
  if (local_path.internal_path().empty())
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;

  *local_file_path = local_path.internal_path();
  return base::PLATFORM_FILE_OK;
}

PlatformFileError LocalFileUtil::Touch(
    FileSystemOperationContext* context,
    const FileSystemPath& path,
    const base::Time& last_access_time,
    const base::Time& last_modified_time) {
  FileSystemPath local_path = GetLocalPath(context, path);
  if (local_path.internal_path().empty())
    return base::PLATFORM_FILE_ERROR_INVALID_OPERATION;
  return underlying_file_util()->Touch(
      context, local_path, last_access_time, last_modified_time);
}

PlatformFileError LocalFileUtil::Truncate(
    FileSystemOperationContext* context,
    const FileSystemPath& path,
    int64 length) {
  FileSystemPath local_path = GetLocalPath(context, path);
  if (local_path.internal_path().empty())
    return base::PLATFORM_FILE_ERROR_INVALID_OPERATION;
  return underlying_file_util()->Truncate(
      context, local_path, length);
}

bool LocalFileUtil::PathExists(
    FileSystemOperationContext* context,
    const FileSystemPath& path) {
  FileSystemPath local_path = GetLocalPath(context, path);
  if (local_path.internal_path().empty())
    return false;
  return underlying_file_util()->PathExists(
      context, local_path);
}

bool LocalFileUtil::DirectoryExists(
    FileSystemOperationContext* context,
    const FileSystemPath& path) {
  FileSystemPath local_path = GetLocalPath(context, path);
  if (local_path.internal_path().empty())
    return false;
  return underlying_file_util()->DirectoryExists(
      context, local_path);
}

bool LocalFileUtil::IsDirectoryEmpty(
    FileSystemOperationContext* context,
    const FileSystemPath& path) {
  FileSystemPath local_path = GetLocalPath(context, path);
  if (local_path.internal_path().empty())
    return true;
  return underlying_file_util()->IsDirectoryEmpty(
      context, local_path);
}

PlatformFileError LocalFileUtil::CopyOrMoveFile(
    FileSystemOperationContext* context,
    const FileSystemPath& src_path,
    const FileSystemPath& dest_path,
    bool copy) {
  // TODO(ericu): If they share a root URL, this could be optimized.
  FileSystemPath local_src_path = GetLocalPath(context, src_path);
  if (local_src_path.internal_path().empty())
    return base::PLATFORM_FILE_ERROR_INVALID_OPERATION;
  FileSystemPath local_dest_path = GetLocalPath(context, dest_path);
  if (local_dest_path.internal_path().empty())
    return base::PLATFORM_FILE_ERROR_INVALID_OPERATION;
  return underlying_file_util()->CopyOrMoveFile(
      context, local_src_path, local_dest_path, copy);
}

// TODO(dmikurube): Make it independent from CopyOrMoveFile.
PlatformFileError LocalFileUtil::CopyInForeignFile(
    FileSystemOperationContext* context,
    const FilePath& src_file_path,
    const FileSystemPath& dest_path) {
  if (src_file_path.empty())
    return base::PLATFORM_FILE_ERROR_INVALID_OPERATION;
  FileSystemPath local_dest_path = GetLocalPath(context, dest_path);
  if (local_dest_path.internal_path().empty())
    return base::PLATFORM_FILE_ERROR_INVALID_OPERATION;
  return underlying_file_util()->CopyInForeignFile(
      context, src_file_path, local_dest_path);
}

PlatformFileError LocalFileUtil::DeleteFile(
    FileSystemOperationContext* context,
    const FileSystemPath& path) {
  FileSystemPath local_path = GetLocalPath(context, path);
  if (local_path.internal_path().empty())
    return base::PLATFORM_FILE_ERROR_INVALID_OPERATION;
  return underlying_file_util()->DeleteFile(
      context, local_path);
}

PlatformFileError LocalFileUtil::DeleteSingleDirectory(
    FileSystemOperationContext* context,
    const FileSystemPath& path) {
  FileSystemPath local_path = GetLocalPath(context, path);
  if (local_path.internal_path().empty())
    return base::PLATFORM_FILE_ERROR_INVALID_OPERATION;
  return underlying_file_util()->DeleteSingleDirectory(
      context, local_path);
}

FileSystemPath LocalFileUtil::GetLocalPath(
    FileSystemOperationContext* context,
    const FileSystemPath& path) {
  FileSystemMountPointProvider* provider =
      context->file_system_context()->GetMountPointProvider(path.type());
  DCHECK(provider);
  FilePath root = provider->GetFileSystemRootPathOnFileThread(
      path.origin(), path.type(), path.internal_path(), false);
  if (root.empty())
    return FileSystemPath();
  return path.WithInternalPath(root.Append(path.internal_path()));
}

}  // namespace fileapi
