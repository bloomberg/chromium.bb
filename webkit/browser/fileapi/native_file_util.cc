// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/browser/fileapi/native_file_util.h"

#include "base/file_util.h"
#include "base/files/file_enumerator.h"
#include "base/memory/scoped_ptr.h"
#include "webkit/browser/fileapi/file_system_operation_context.h"

namespace fileapi {

namespace {

// Sets permissions on directory at |dir_path| based on the target platform.
// Returns true on success, or false otherwise.
//
// TODO(benchan): Find a better place outside webkit to host this function.
bool SetPlatformSpecificDirectoryPermissions(const base::FilePath& dir_path) {
#if defined(OS_CHROMEOS)
    // System daemons on Chrome OS may run as a user different than the Chrome
    // process but need to access files under the directories created here.
    // Because of that, grant the execute permission on the created directory
    // to group and other users.
    if (HANDLE_EINTR(chmod(dir_path.value().c_str(),
                           S_IRWXU | S_IXGRP | S_IXOTH)) != 0) {
      return false;
    }
#endif
    // Keep the directory permissions unchanged on non-Chrome OS platforms.
    return true;
}

}  // namespace

using base::PlatformFile;
using base::PlatformFileError;

class NativeFileEnumerator : public FileSystemFileUtil::AbstractFileEnumerator {
 public:
  NativeFileEnumerator(const base::FilePath& root_path,
                       bool recursive,
                       int file_type)
    : file_enum_(root_path, recursive, file_type) {
  }

  virtual ~NativeFileEnumerator() {}

  virtual base::FilePath Next() OVERRIDE;
  virtual int64 Size() OVERRIDE;
  virtual base::Time LastModifiedTime() OVERRIDE;
  virtual bool IsDirectory() OVERRIDE;

 private:
  base::FileEnumerator file_enum_;
  base::FileEnumerator::FileInfo file_util_info_;
};

base::FilePath NativeFileEnumerator::Next() {
  base::FilePath rv = file_enum_.Next();
  if (!rv.empty())
    file_util_info_ = file_enum_.GetInfo();
  return rv;
}

int64 NativeFileEnumerator::Size() {
  return file_util_info_.GetSize();
}

base::Time NativeFileEnumerator::LastModifiedTime() {
  return file_util_info_.GetLastModifiedTime();
}

bool NativeFileEnumerator::IsDirectory() {
  return file_util_info_.IsDirectory();
}

PlatformFileError NativeFileUtil::CreateOrOpen(
    const base::FilePath& path, int file_flags,
    PlatformFile* file_handle, bool* created) {
  if (!base::DirectoryExists(path.DirName())) {
    // If its parent does not exist, should return NOT_FOUND error.
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;
  }
  if (base::DirectoryExists(path))
    return base::PLATFORM_FILE_ERROR_NOT_A_FILE;
  PlatformFileError error_code = base::PLATFORM_FILE_OK;
  *file_handle = base::CreatePlatformFile(path, file_flags,
                                          created, &error_code);
  return error_code;
}

PlatformFileError NativeFileUtil::Close(PlatformFile file_handle) {
  if (!base::ClosePlatformFile(file_handle))
    return base::PLATFORM_FILE_ERROR_FAILED;
  return base::PLATFORM_FILE_OK;
}

PlatformFileError NativeFileUtil::EnsureFileExists(
    const base::FilePath& path,
    bool* created) {
  if (!base::DirectoryExists(path.DirName()))
    // If its parent does not exist, should return NOT_FOUND error.
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;
  PlatformFileError error_code = base::PLATFORM_FILE_OK;
  // Tries to create the |path| exclusively.  This should fail
  // with base::PLATFORM_FILE_ERROR_EXISTS if the path already exists.
  PlatformFile handle = base::CreatePlatformFile(
      path,
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
    const base::FilePath& path,
    bool exclusive,
    bool recursive) {
  // If parent dir of file doesn't exist.
  if (!recursive && !base::PathExists(path.DirName()))
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;

  bool path_exists = base::PathExists(path);
  if (exclusive && path_exists)
    return base::PLATFORM_FILE_ERROR_EXISTS;

  // If file exists at the path.
  if (path_exists && !base::DirectoryExists(path))
    return base::PLATFORM_FILE_ERROR_EXISTS;

  if (!file_util::CreateDirectory(path))
    return base::PLATFORM_FILE_ERROR_FAILED;

  if (!SetPlatformSpecificDirectoryPermissions(path))
    return base::PLATFORM_FILE_ERROR_FAILED;

  return base::PLATFORM_FILE_OK;
}

PlatformFileError NativeFileUtil::GetFileInfo(
    const base::FilePath& path,
    base::PlatformFileInfo* file_info) {
  if (!base::PathExists(path))
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;
  if (!file_util::GetFileInfo(path, file_info))
    return base::PLATFORM_FILE_ERROR_FAILED;
  return base::PLATFORM_FILE_OK;
}

scoped_ptr<FileSystemFileUtil::AbstractFileEnumerator>
    NativeFileUtil::CreateFileEnumerator(const base::FilePath& root_path,
                                         bool recursive) {
  return make_scoped_ptr(new NativeFileEnumerator(
      root_path, recursive,
      base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES))
      .PassAs<FileSystemFileUtil::AbstractFileEnumerator>();
}

PlatformFileError NativeFileUtil::Touch(
    const base::FilePath& path,
    const base::Time& last_access_time,
    const base::Time& last_modified_time) {
  if (!file_util::TouchFile(
          path, last_access_time, last_modified_time))
    return base::PLATFORM_FILE_ERROR_FAILED;
  return base::PLATFORM_FILE_OK;
}

PlatformFileError NativeFileUtil::Truncate(
    const base::FilePath& path, int64 length) {
  PlatformFileError error_code(base::PLATFORM_FILE_ERROR_FAILED);
  PlatformFile file =
      base::CreatePlatformFile(
          path,
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

bool NativeFileUtil::PathExists(const base::FilePath& path) {
  return base::PathExists(path);
}

bool NativeFileUtil::DirectoryExists(const base::FilePath& path) {
  return base::DirectoryExists(path);
}

PlatformFileError NativeFileUtil::CopyOrMoveFile(
    const base::FilePath& src_path,
    const base::FilePath& dest_path,
    bool copy) {
  base::PlatformFileInfo info;
  base::PlatformFileError error = NativeFileUtil::GetFileInfo(src_path, &info);
  if (error != base::PLATFORM_FILE_OK)
    return error;
  if (info.is_directory)
    return base::PLATFORM_FILE_ERROR_NOT_A_FILE;

  error = NativeFileUtil::GetFileInfo(dest_path, &info);
  if (error != base::PLATFORM_FILE_OK &&
      error != base::PLATFORM_FILE_ERROR_NOT_FOUND)
    return error;
  if (info.is_directory)
    return base::PLATFORM_FILE_ERROR_INVALID_OPERATION;
  if (error == base::PLATFORM_FILE_ERROR_NOT_FOUND) {
    error = NativeFileUtil::GetFileInfo(dest_path.DirName(), &info);
    if (error != base::PLATFORM_FILE_OK)
      return error;
    if (!info.is_directory)
      return base::PLATFORM_FILE_ERROR_NOT_FOUND;
  }

  if (copy) {
    if (base::CopyFile(src_path, dest_path))
      return base::PLATFORM_FILE_OK;
  } else {
    if (base::Move(src_path, dest_path))
      return base::PLATFORM_FILE_OK;
  }
  return base::PLATFORM_FILE_ERROR_FAILED;
}

PlatformFileError NativeFileUtil::DeleteFile(const base::FilePath& path) {
  if (!base::PathExists(path))
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;
  if (base::DirectoryExists(path))
    return base::PLATFORM_FILE_ERROR_NOT_A_FILE;
  if (!base::DeleteFile(path, false))
    return base::PLATFORM_FILE_ERROR_FAILED;
  return base::PLATFORM_FILE_OK;
}

PlatformFileError NativeFileUtil::DeleteDirectory(const base::FilePath& path) {
  if (!base::PathExists(path))
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;
  if (!base::DirectoryExists(path))
    return base::PLATFORM_FILE_ERROR_NOT_A_DIRECTORY;
  if (!file_util::IsDirectoryEmpty(path))
    return base::PLATFORM_FILE_ERROR_NOT_EMPTY;
  if (!base::DeleteFile(path, false))
    return base::PLATFORM_FILE_ERROR_FAILED;
  return base::PLATFORM_FILE_OK;
}

}  // namespace fileapi
