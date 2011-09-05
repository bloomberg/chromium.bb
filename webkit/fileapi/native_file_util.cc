// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/native_file_util.h"

#include <vector>

#include "base/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "webkit/fileapi/file_system_operation_context.h"

namespace fileapi {

class NativeFileEnumerator : public FileSystemFileUtil::AbstractFileEnumerator {
 public:
  NativeFileEnumerator(const FilePath& root_path,
                       bool recursive,
                       file_util::FileEnumerator::FileType file_type)
    : file_enum_(root_path, recursive, file_type) {
  }

  ~NativeFileEnumerator() {}

  virtual FilePath Next() OVERRIDE;
  virtual int64 Size() OVERRIDE;
  virtual bool IsDirectory() OVERRIDE;

 private:
  file_util::FileEnumerator file_enum_;
  file_util::FileEnumerator::FindInfo file_util_info_;
};

FilePath NativeFileEnumerator::Next() {
  FilePath rv = file_enum_.Next();
  if (!rv.empty())
    file_enum_.GetFindInfo(&file_util_info_);
  return rv;
}

int64 NativeFileEnumerator::Size() {
  return file_util::FileEnumerator::GetFilesize(file_util_info_);
}

bool NativeFileEnumerator::IsDirectory() {
  return file_util::FileEnumerator::IsDirectory(file_util_info_);
}

PlatformFileError NativeFileUtil::CreateOrOpen(
    FileSystemOperationContext* unused,
    const FilePath& file_path, int file_flags,
    PlatformFile* file_handle, bool* created) {
  if (!file_util::DirectoryExists(file_path.DirName())) {
    // If its parent does not exist, should return NOT_FOUND error.
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;
  }
  PlatformFileError error_code = base::PLATFORM_FILE_OK;
  *file_handle = base::CreatePlatformFile(file_path, file_flags,
                                          created, &error_code);
  return error_code;
}

PlatformFileError NativeFileUtil::Close(
    FileSystemOperationContext* unused,
    PlatformFile file_handle) {
  if (!base::ClosePlatformFile(file_handle))
    return base::PLATFORM_FILE_ERROR_FAILED;
  return base::PLATFORM_FILE_OK;
}

PlatformFileError NativeFileUtil::EnsureFileExists(
    FileSystemOperationContext* unused,
    const FilePath& file_path,
    bool* created) {
  if (!file_util::DirectoryExists(file_path.DirName()))
    // If its parent does not exist, should return NOT_FOUND error.
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;
  PlatformFileError error_code = base::PLATFORM_FILE_OK;
  // Tries to create the |file_path| exclusively.  This should fail
  // with base::PLATFORM_FILE_ERROR_EXISTS if the path already exists.
  PlatformFile handle = base::CreatePlatformFile(
      file_path,
      base::PLATFORM_FILE_CREATE | base::PLATFORM_FILE_READ,
      created, &error_code);
  if (error_code == base::PLATFORM_FILE_ERROR_EXISTS) {
    // Make sure created_ is false.
    if (created)
      *created = false;
    error_code = base::PLATFORM_FILE_OK;
  }
  if (handle != base::kInvalidPlatformFileValue)
    base::ClosePlatformFile(handle);
  return error_code;
}

PlatformFileError NativeFileUtil::CreateDirectory(
    FileSystemOperationContext* context,
    const FilePath& file_path,
    bool exclusive,
    bool recursive) {
  // If parent dir of file doesn't exist.
  if (!recursive && !file_util::PathExists(file_path.DirName()))
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;

  bool path_exists = file_util::PathExists(file_path);
  if (exclusive && path_exists)
    return base::PLATFORM_FILE_ERROR_EXISTS;

  // If file exists at the path.
  if (path_exists && !file_util::DirectoryExists(file_path))
    return base::PLATFORM_FILE_ERROR_EXISTS;

  if (!file_util::CreateDirectory(file_path))
    return base::PLATFORM_FILE_ERROR_FAILED;
  return base::PLATFORM_FILE_OK;
}

PlatformFileError NativeFileUtil::GetFileInfo(
    FileSystemOperationContext* unused,
    const FilePath& file_path,
    base::PlatformFileInfo* file_info,
    FilePath* platform_file_path) {
  if (!file_util::PathExists(file_path))
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;
  // TODO(rkc): Fix this hack once we have refactored file_util to handle
  // symlinks correctly.
  // http://code.google.com/p/chromium-os/issues/detail?id=15948
  if (file_util::IsLink(file_path))
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;
  if (!file_util::GetFileInfo(file_path, file_info))
    return base::PLATFORM_FILE_ERROR_FAILED;
  *platform_file_path = file_path;
  return base::PLATFORM_FILE_OK;
}

PlatformFileError NativeFileUtil::ReadDirectory(
    FileSystemOperationContext* unused,
    const FilePath& file_path,
    std::vector<base::FileUtilProxy::Entry>* entries) {
  // TODO(kkanetkar): Implement directory read in multiple chunks.
  if (!file_util::DirectoryExists(file_path))
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;

  file_util::FileEnumerator file_enum(
      file_path, false, static_cast<file_util::FileEnumerator::FileType>(
      file_util::FileEnumerator::FILES |
      file_util::FileEnumerator::DIRECTORIES));
  FilePath current;
  while (!(current = file_enum.Next()).empty()) {
    base::FileUtilProxy::Entry entry;
    file_util::FileEnumerator::FindInfo info;
    file_enum.GetFindInfo(&info);
    entry.is_directory = file_enum.IsDirectory(info);
    // This will just give the entry's name instead of entire path
    // if we use current.value().
    entry.name = file_util::FileEnumerator::GetFilename(info).value();
    entry.size = file_util::FileEnumerator::GetFilesize(info);
    entry.last_modified_time =
        file_util::FileEnumerator::GetLastModifiedTime(info);
    // TODO(rkc): Fix this also once we've refactored file_util
    // http://code.google.com/p/chromium-os/issues/detail?id=15948
    // This currently just prevents a file from showing up at all
    // if it's a link, hence preventing arbitary 'read' exploits.
    if (!file_util::IsLink(file_path.Append(entry.name)))
      entries->push_back(entry);
  }
  return base::PLATFORM_FILE_OK;
}

FileSystemFileUtil::AbstractFileEnumerator*
NativeFileUtil::CreateFileEnumerator(
    FileSystemOperationContext* unused,
    const FilePath& root_path) {
  return new NativeFileEnumerator(
      root_path, true, static_cast<file_util::FileEnumerator::FileType>(
      file_util::FileEnumerator::FILES |
      file_util::FileEnumerator::DIRECTORIES));
}

PlatformFileError NativeFileUtil::GetLocalFilePath(
    FileSystemOperationContext* unused,
    const FilePath& virtual_path,
    FilePath* local_path) {
  *local_path = virtual_path;
  return base::PLATFORM_FILE_OK;
}

PlatformFileError NativeFileUtil::Touch(
    FileSystemOperationContext* unused,
    const FilePath& file_path,
    const base::Time& last_access_time,
    const base::Time& last_modified_time) {
  if (!file_util::TouchFile(
          file_path, last_access_time, last_modified_time))
    return base::PLATFORM_FILE_ERROR_FAILED;
  return base::PLATFORM_FILE_OK;
}

PlatformFileError NativeFileUtil::Truncate(
    FileSystemOperationContext* unused,
    const FilePath& file_path,
    int64 length) {
  PlatformFileError error_code(base::PLATFORM_FILE_ERROR_FAILED);
  PlatformFile file =
      base::CreatePlatformFile(
          file_path,
          base::PLATFORM_FILE_OPEN | base::PLATFORM_FILE_WRITE,
          NULL,
          &error_code);
  if (error_code != base::PLATFORM_FILE_OK) {
    return error_code;
  }
  DCHECK_NE(base::kInvalidPlatformFileValue, file);
  if (!base::TruncatePlatformFile(file, length))
    error_code = base::PLATFORM_FILE_ERROR_FAILED;
  base::ClosePlatformFile(file);
  return error_code;
}

bool NativeFileUtil::PathExists(
    FileSystemOperationContext* unused,
    const FilePath& file_path) {
  return file_util::PathExists(file_path);
}

bool NativeFileUtil::DirectoryExists(
    FileSystemOperationContext* unused,
    const FilePath& file_path) {
  return file_util::DirectoryExists(file_path);
}

bool NativeFileUtil::IsDirectoryEmpty(
    FileSystemOperationContext* unused,
    const FilePath& file_path) {
  return file_util::IsDirectoryEmpty(file_path);
}

PlatformFileError NativeFileUtil::CopyOrMoveFile(
      FileSystemOperationContext* unused,
      const FilePath& src_file_path,
      const FilePath& dest_file_path,
      bool copy) {
  if (copy) {
    if (file_util::CopyFile(src_file_path, dest_file_path))
      return base::PLATFORM_FILE_OK;
  } else {
    DCHECK(!file_util::DirectoryExists(src_file_path));
    if (file_util::Move(src_file_path, dest_file_path))
      return base::PLATFORM_FILE_OK;
  }
  return base::PLATFORM_FILE_ERROR_FAILED;
}

PlatformFileError NativeFileUtil::CopyInForeignFile(
      FileSystemOperationContext* context,
      const FilePath& src_file_path,
      const FilePath& dest_file_path) {
  return CopyOrMoveFile(context, src_file_path, dest_file_path, true);
}

PlatformFileError NativeFileUtil::DeleteFile(
    FileSystemOperationContext* unused,
    const FilePath& file_path) {
  if (!file_util::PathExists(file_path))
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;
  if (file_util::DirectoryExists(file_path))
    return base::PLATFORM_FILE_ERROR_NOT_A_FILE;
  if (!file_util::Delete(file_path, false))
    return base::PLATFORM_FILE_ERROR_FAILED;
  return base::PLATFORM_FILE_OK;
}

PlatformFileError NativeFileUtil::DeleteSingleDirectory(
    FileSystemOperationContext* unused,
    const FilePath& file_path) {
  if (!file_util::PathExists(file_path))
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;
  if (!file_util::DirectoryExists(file_path)) {
    // TODO(dmikurube): Check if this error code is appropriate.
    return base::PLATFORM_FILE_ERROR_NOT_A_DIRECTORY;
  }
  if (!file_util::IsDirectoryEmpty(file_path)) {
    // TODO(dmikurube): Check if this error code is appropriate.
    return base::PLATFORM_FILE_ERROR_NOT_EMPTY;
  }
  if (!file_util::Delete(file_path, false))
    return base::PLATFORM_FILE_ERROR_FAILED;
  return base::PLATFORM_FILE_OK;
}

}  // namespace fileapi
