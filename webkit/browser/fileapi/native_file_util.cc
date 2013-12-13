// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/browser/fileapi/native_file_util.h"

#include "base/file_util.h"
#include "base/files/file_enumerator.h"
#include "base/memory/scoped_ptr.h"
#include "net/base/file_stream.h"
#include "webkit/browser/fileapi/file_system_operation_context.h"
#include "webkit/browser/fileapi/file_system_url.h"

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

// Copies a file |from| to |to|, and ensure the written content is synced to
// the disk. This is essentially base::CopyFile followed by fsync().
bool CopyFileAndSync(const base::FilePath& from, const base::FilePath& to) {
  net::FileStream infile(NULL);
  if (infile.OpenSync(from,
          base::PLATFORM_FILE_OPEN | base:: PLATFORM_FILE_READ) < 0) {
    return false;
  }

  net::FileStream outfile(NULL);
  if (outfile.OpenSync(to,
          base::PLATFORM_FILE_CREATE_ALWAYS | base:: PLATFORM_FILE_WRITE) < 0) {
    return false;
  }

  const int kBufferSize = 32768;
  std::vector<char> buffer(kBufferSize);

  for (;;) {
    int bytes_read = infile.ReadSync(&buffer[0], kBufferSize);
    if (bytes_read < 0)
      return false;
    if (bytes_read == 0)
      break;
    for (int bytes_written = 0; bytes_written < bytes_read; ) {
      int bytes_written_partial = outfile.WriteSync(&buffer[bytes_written],
                                                    bytes_read - bytes_written);
      if (bytes_written_partial < 0)
        return false;
      bytes_written += bytes_written_partial;
    }
  }

  return outfile.FlushSync() >= 0;
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

NativeFileUtil::CopyOrMoveMode NativeFileUtil::CopyOrMoveModeForDestination(
    const FileSystemURL& dest_url, bool copy) {
  if (copy) {
    return dest_url.mount_option().copy_sync_option() == COPY_SYNC_OPTION_SYNC ?
        COPY_SYNC : COPY_NOSYNC;
  }
  return MOVE;
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

  if (!base::CreateDirectory(path))
    return base::PLATFORM_FILE_ERROR_FAILED;

  if (!SetPlatformSpecificDirectoryPermissions(path)) {
    // Since some file systems don't support permission setting, we do not treat
    // an error from the function as the failure of copying. Just log it.
    LOG(WARNING) << "Setting directory permission failed: "
        << path.AsUTF8Unsafe();
  }

  return base::PLATFORM_FILE_OK;
}

PlatformFileError NativeFileUtil::GetFileInfo(
    const base::FilePath& path,
    base::PlatformFileInfo* file_info) {
  if (!base::PathExists(path))
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;
  if (!base::GetFileInfo(path, file_info))
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
  if (!base::TouchFile(path, last_access_time, last_modified_time))
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
    FileSystemOperation::CopyOrMoveOption option,
    CopyOrMoveMode mode) {
  base::PlatformFileInfo info;
  base::PlatformFileError error = NativeFileUtil::GetFileInfo(src_path, &info);
  if (error != base::PLATFORM_FILE_OK)
    return error;
  if (info.is_directory)
    return base::PLATFORM_FILE_ERROR_NOT_A_FILE;
  base::Time last_modified = info.last_modified;

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

  switch (mode) {
    case COPY_NOSYNC:
      if (!base::CopyFile(src_path, dest_path))
        return base::PLATFORM_FILE_ERROR_FAILED;
      break;
    case COPY_SYNC:
      if (!CopyFileAndSync(src_path, dest_path))
        return base::PLATFORM_FILE_ERROR_FAILED;
      break;
    case MOVE:
      if (!base::Move(src_path, dest_path))
        return base::PLATFORM_FILE_ERROR_FAILED;
      break;
  }

  // Preserve the last modified time. Do not return error here even if
  // the setting is failed, because the copy itself is successfully done.
  if (option == FileSystemOperation::OPTION_PRESERVE_LAST_MODIFIED)
    base::TouchFile(dest_path, last_modified, last_modified);

  return base::PLATFORM_FILE_OK;
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
  if (!base::IsDirectoryEmpty(path))
    return base::PLATFORM_FILE_ERROR_NOT_EMPTY;
  if (!base::DeleteFile(path, false))
    return base::PLATFORM_FILE_ERROR_FAILED;
  return base::PLATFORM_FILE_OK;
}

}  // namespace fileapi
