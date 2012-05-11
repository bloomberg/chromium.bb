// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/file_system_file_util.h"

#include <stack>

#include "base/memory/scoped_ptr.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_operation_context.h"

namespace fileapi {

FileSystemFileUtil::FileSystemFileUtil() {
}

FileSystemFileUtil::FileSystemFileUtil(FileSystemFileUtil* underlying_file_util)
    : underlying_file_util_(underlying_file_util) {
}

FileSystemFileUtil::~FileSystemFileUtil() {
}

PlatformFileError FileSystemFileUtil::CreateOrOpen(
    FileSystemOperationContext* context,
    const FileSystemPath& path, int file_flags,
    PlatformFile* file_handle, bool* created) {
  if (underlying_file_util_.get()) {
    return underlying_file_util_->CreateOrOpen(
        context, path, file_flags, file_handle, created);
  }
  NOTREACHED() << "Subclasses must provide implementation if they have no"
               << "underlying_file_util";
  return base::PLATFORM_FILE_ERROR_FAILED;
}

PlatformFileError FileSystemFileUtil::Close(
    FileSystemOperationContext* context,
    PlatformFile file_handle) {
  if (underlying_file_util_.get()) {
    return underlying_file_util_->Close(context, file_handle);
  }
  NOTREACHED() << "Subclasses must provide implementation if they have no"
               << "underlying_file_util";
  return base::PLATFORM_FILE_ERROR_FAILED;
}

PlatformFileError FileSystemFileUtil::EnsureFileExists(
    FileSystemOperationContext* context,
    const FileSystemPath& path,
    bool* created) {
  if (underlying_file_util_.get()) {
    return underlying_file_util_->EnsureFileExists(context, path, created);
  }
  NOTREACHED() << "Subclasses must provide implementation if they have no"
               << "underlying_file_util";
  return base::PLATFORM_FILE_ERROR_FAILED;
}

PlatformFileError FileSystemFileUtil::CreateDirectory(
    FileSystemOperationContext* context,
    const FileSystemPath& path,
    bool exclusive,
    bool recursive) {
  if (underlying_file_util_.get()) {
    return underlying_file_util_->CreateDirectory(
        context, path, exclusive, recursive);
  }
  NOTREACHED() << "Subclasses must provide implementation if they have no"
               << "underlying_file_util";
  return base::PLATFORM_FILE_ERROR_FAILED;
}

PlatformFileError FileSystemFileUtil::GetFileInfo(
    FileSystemOperationContext* context,
    const FileSystemPath& path,
    base::PlatformFileInfo* file_info,
    FilePath* platform_file_path) {
  if (underlying_file_util_.get()) {
    return underlying_file_util_->GetFileInfo(
        context, path, file_info, platform_file_path);
  }
  NOTREACHED() << "Subclasses must provide implementation if they have no"
               << "underlying_file_util";
  return base::PLATFORM_FILE_ERROR_FAILED;
}

FileSystemFileUtil::AbstractFileEnumerator*
FileSystemFileUtil::CreateFileEnumerator(
    FileSystemOperationContext* context,
    const FileSystemPath& root_path,
    bool recursive) {
  if (underlying_file_util_.get()) {
    return underlying_file_util_->CreateFileEnumerator(context, root_path,
                                                       recursive);
  }
  NOTREACHED() << "Subclasses must provide implementation if they have no"
               << "underlying_file_util";
  return NULL;
}

PlatformFileError FileSystemFileUtil::GetLocalFilePath(
    FileSystemOperationContext* context,
    const FileSystemPath& file_system_path,
    FilePath* local_file_path) {
  if (underlying_file_util_.get()) {
    return underlying_file_util_->GetLocalFilePath(
        context, file_system_path, local_file_path);
  }
  NOTREACHED() << "Subclasses must provide implementation if they have no"
               << "underlying_file_util";
  return base::PLATFORM_FILE_ERROR_FAILED;
}

PlatformFileError FileSystemFileUtil::Touch(
    FileSystemOperationContext* context,
    const FileSystemPath& path,
    const base::Time& last_access_time,
    const base::Time& last_modified_time) {
  if (underlying_file_util_.get()) {
    return underlying_file_util_->Touch(
        context, path, last_access_time, last_modified_time);
  }
  NOTREACHED() << "Subclasses must provide implementation if they have no"
               << "underlying_file_util";
  return base::PLATFORM_FILE_ERROR_FAILED;
}

PlatformFileError FileSystemFileUtil::Truncate(
    FileSystemOperationContext* context,
    const FileSystemPath& path,
    int64 length) {
  if (underlying_file_util_.get()) {
    return underlying_file_util_->Truncate(context, path, length);
  }
  NOTREACHED() << "Subclasses must provide implementation if they have no"
               << "underlying_file_util";
  return base::PLATFORM_FILE_ERROR_FAILED;
}


bool FileSystemFileUtil::PathExists(
    FileSystemOperationContext* context,
    const FileSystemPath& path) {
  if (underlying_file_util_.get()) {
    return underlying_file_util_->PathExists(context, path);
  }
  NOTREACHED() << "Subclasses must provide implementation if they have no"
               << "underlying_file_util";
  return false;
}

bool FileSystemFileUtil::DirectoryExists(
    FileSystemOperationContext* context,
    const FileSystemPath& path) {
  if (underlying_file_util_.get()) {
    return underlying_file_util_->DirectoryExists(context, path);
  }
  NOTREACHED() << "Subclasses must provide implementation if they have no"
               << "underlying_file_util";
  return false;
}

bool FileSystemFileUtil::IsDirectoryEmpty(
    FileSystemOperationContext* context,
    const FileSystemPath& path) {
  if (underlying_file_util_.get()) {
    return underlying_file_util_->IsDirectoryEmpty(context, path);
  }
  NOTREACHED() << "Subclasses must provide implementation if they have no"
               << "underlying_file_util";
  return false;
}

PlatformFileError FileSystemFileUtil::CopyOrMoveFile(
    FileSystemOperationContext* context,
    const FileSystemPath& src_path,
    const FileSystemPath& dest_path,
    bool copy) {
  if (underlying_file_util_.get()) {
    return underlying_file_util_->CopyOrMoveFile(
        context, src_path, dest_path, copy);
  }
  NOTREACHED() << "Subclasses must provide implementation if they have no"
               << "underlying_file_util";
  return base::PLATFORM_FILE_ERROR_FAILED;
}

PlatformFileError FileSystemFileUtil::CopyInForeignFile(
      FileSystemOperationContext* context,
      const FilePath& src_file_path,
      const FileSystemPath& dest_path) {
  if (underlying_file_util_.get()) {
    return underlying_file_util_->CopyInForeignFile(
        context, src_file_path, dest_path);
  }
  NOTREACHED() << "Subclasses must provide implementation if they have no"
               << "underlying_file_util";
  return base::PLATFORM_FILE_ERROR_FAILED;
}

PlatformFileError FileSystemFileUtil::DeleteFile(
    FileSystemOperationContext* context,
    const FileSystemPath& path) {
  if (underlying_file_util_.get()) {
    return underlying_file_util_->DeleteFile(context, path);
  }
  NOTREACHED() << "Subclasses must provide implementation if they have no"
               << "underlying_file_util";
  return base::PLATFORM_FILE_ERROR_FAILED;
}

PlatformFileError FileSystemFileUtil::DeleteSingleDirectory(
    FileSystemOperationContext* context,
    const FileSystemPath& path) {
  if (underlying_file_util_.get()) {
    return underlying_file_util_->DeleteSingleDirectory(context, path);
  }
  NOTREACHED() << "Subclasses must provide implementation if they have no"
               << "underlying_file_util";
  return base::PLATFORM_FILE_ERROR_FAILED;
}

}  // namespace fileapi
