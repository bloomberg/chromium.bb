// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/file_system_file_util.h"

#include "base/file_util_proxy.h"

// This also removes the destination directory if it's non-empty and all other
// checks are passed (so that the copy/move correctly overwrites the
// destination).
// TODO(ericu, dmikurube): This method won't work with obfuscation and quota
// since all (file_util::) operations should consider obfuscation and quota.
// We will need to virtualize all these calls.  We should do that by making this
// method a non-virtual member of FileSystemFileUtil, but changing all of its
// file_util calls to be FileSystemFileUtil calls.  That way when we override
// them for obfuscation or quota, it'll just work.
static base::PlatformFileError PerformCommonCheckAndPreparationForMoveAndCopy(
    const FilePath& src_file_path,
    const FilePath& dest_file_path) {
  // Exits earlier if the source path does not exist.
  if (!file_util::PathExists(src_file_path))
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;

  // The parent of the |dest_file_path| does not exist.
  if (!file_util::DirectoryExists(dest_file_path.DirName()))
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;

  // It is an error to try to copy/move an entry into its child.
  if (src_file_path.IsParent(dest_file_path))
    return base::PLATFORM_FILE_ERROR_INVALID_OPERATION;

  // Now it is ok to return if the |dest_file_path| does not exist.
  if (!file_util::PathExists(dest_file_path))
    return base::PLATFORM_FILE_OK;

  // |src_file_path| exists and is a directory.
  // |dest_file_path| exists and is a file.
  bool src_is_directory = file_util::DirectoryExists(src_file_path);
  bool dest_is_directory = file_util::DirectoryExists(dest_file_path);
  if (src_is_directory && !dest_is_directory)
    return base::PLATFORM_FILE_ERROR_NOT_A_DIRECTORY;

  // |src_file_path| exists and is a file.
  // |dest_file_path| exists and is a directory.
  if (!src_is_directory && dest_is_directory)
    return base::PLATFORM_FILE_ERROR_NOT_A_FILE;

  // It is an error to copy/move an entry into the same path.
  if (src_file_path.value() == dest_file_path.value())
    return base::PLATFORM_FILE_ERROR_EXISTS;

  if (dest_is_directory) {
    // It is an error to copy/move an entry to a non-empty directory.
    // Otherwise the copy/move attempt must overwrite the destination, but
    // the file_util's Copy or Move method doesn't perform overwrite
    // on all platforms, so we delete the destination directory here.
    // TODO(kinuko): may be better to change the file_util::{Copy,Move}.
    if (!file_util::Delete(dest_file_path, false /* recursive */)) {
      if (!file_util::IsDirectoryEmpty(dest_file_path))
        return base::PLATFORM_FILE_ERROR_NOT_EMPTY;
      return base::PLATFORM_FILE_ERROR_FAILED;
    }
  }
  return base::PLATFORM_FILE_OK;
}

namespace fileapi {

FileSystemFileUtil* FileSystemFileUtil::GetInstance() {
  return Singleton<FileSystemFileUtil>::get();
}

PlatformFileError FileSystemFileUtil::CreateOrOpen(
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

PlatformFileError FileSystemFileUtil::Close(
    FileSystemOperationContext* unused,
    PlatformFile file_handle) {
  if (!base::ClosePlatformFile(file_handle))
    return base::PLATFORM_FILE_ERROR_FAILED;
  return base::PLATFORM_FILE_OK;
}

PlatformFileError FileSystemFileUtil::EnsureFileExists(
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
    *created = false;
    error_code = base::PLATFORM_FILE_OK;
  }
  if (handle != base::kInvalidPlatformFileValue)
    base::ClosePlatformFile(handle);
  return error_code;
}

PlatformFileError FileSystemFileUtil::GetFileInfo(
    FileSystemOperationContext* unused,
    const FilePath& file_path,
    base::PlatformFileInfo* file_info) {
  if (!file_util::PathExists(file_path))
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;
  if (!file_util::GetFileInfo(file_path, file_info))
    return base::PLATFORM_FILE_ERROR_FAILED;
  return base::PLATFORM_FILE_OK;
}

PlatformFileError FileSystemFileUtil::ReadDirectory(
    FileSystemOperationContext* unused,
    const FilePath& file_path,
    std::vector<base::FileUtilProxy::Entry>* entries) {
  // TODO(kkanetkar): Implement directory read in multiple chunks.
  if (!file_util::DirectoryExists(file_path))
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;

  file_util::FileEnumerator file_enum(
      file_path, false, static_cast<file_util::FileEnumerator::FILE_TYPE>(
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
    entries->push_back(entry);
  }
  return base::PLATFORM_FILE_OK;
}

PlatformFileError FileSystemFileUtil::CreateDirectory(
    FileSystemOperationContext* unused,
    const FilePath& file_path,
    bool exclusive) {
  bool path_exists = file_util::PathExists(file_path);
  if (!file_util::PathExists(file_path.DirName()))
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;
  // If parent dir of file doesn't exist.
  if (exclusive && path_exists)
    return base::PLATFORM_FILE_ERROR_EXISTS;
  // If file exists at the path.
  if (path_exists && !file_util::DirectoryExists(file_path))
    return base::PLATFORM_FILE_ERROR_EXISTS;
  if (!file_util::CreateDirectory(file_path))
    return base::PLATFORM_FILE_ERROR_FAILED;
  return base::PLATFORM_FILE_OK;
}

PlatformFileError FileSystemFileUtil::Copy(
    FileSystemOperationContext* unused,
    const FilePath& src_file_path,
    const FilePath& dest_file_path) {
  PlatformFileError error_code;
  error_code =
      PerformCommonCheckAndPreparationForMoveAndCopy(
          src_file_path, dest_file_path);
  if (error_code != base::PLATFORM_FILE_OK)
    return error_code;
  if (!file_util::CopyDirectory(src_file_path, dest_file_path,
      true /* recursive */))
    return base::PLATFORM_FILE_ERROR_FAILED;
  return base::PLATFORM_FILE_OK;
}

PlatformFileError FileSystemFileUtil::Move(
    FileSystemOperationContext* unused,
    const FilePath& src_file_path,
    const FilePath& dest_file_path) {
  PlatformFileError error_code;
  error_code =
      PerformCommonCheckAndPreparationForMoveAndCopy(
          src_file_path, dest_file_path);
  if (error_code != base::PLATFORM_FILE_OK)
    return error_code;
  if (!file_util::Move(src_file_path, dest_file_path))
    return base::PLATFORM_FILE_ERROR_FAILED;
  return base::PLATFORM_FILE_OK;
}

PlatformFileError FileSystemFileUtil::Delete(
    FileSystemOperationContext* unused,
    const FilePath& file_path,
    bool recursive) {
  if (!file_util::PathExists(file_path)) {
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;
  }
  if (!file_util::Delete(file_path, recursive)) {
    if (!recursive && !file_util::IsDirectoryEmpty(file_path)) {
      return base::PLATFORM_FILE_ERROR_NOT_EMPTY;
    }
    return base::PLATFORM_FILE_ERROR_FAILED;
  }
  return base::PLATFORM_FILE_OK;
}

PlatformFileError FileSystemFileUtil::Touch(
    FileSystemOperationContext* unused,
    const FilePath& file_path,
    const base::Time& last_access_time,
    const base::Time& last_modified_time) {
  if (!file_util::TouchFile(
          file_path, last_access_time, last_modified_time))
    return base::PLATFORM_FILE_ERROR_FAILED;
  return base::PLATFORM_FILE_OK;
}

PlatformFileError FileSystemFileUtil::Truncate(
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
  if (!base::TruncatePlatformFile(file, length))
    error_code = base::PLATFORM_FILE_ERROR_FAILED;
  base::ClosePlatformFile(file);
  return error_code;
}

}  // namespace fileapi
