// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/database/vfs_backend.h"

#if defined(USE_SYSTEM_SQLITE)
#include <sqlite3.h>
#else
#include "third_party/sqlite/preprocessed/sqlite3.h"
#endif

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"

namespace webkit_database {

bool VfsBackend::OpenFileFlagsAreConsistent(int desired_flags) {
  const int file_type = desired_flags & 0x00007F00;
  const bool is_exclusive = (desired_flags & SQLITE_OPEN_EXCLUSIVE) != 0;
  const bool is_delete = (desired_flags & SQLITE_OPEN_DELETEONCLOSE) != 0;
  const bool is_create = (desired_flags & SQLITE_OPEN_CREATE) != 0;
  const bool is_read_only = (desired_flags & SQLITE_OPEN_READONLY) != 0;
  const bool is_read_write = (desired_flags & SQLITE_OPEN_READWRITE) != 0;

  // All files should be opened either read-write or read-only, but not both.
  if (is_read_only == is_read_write)
    return false;

  // If a new file is created, it must also be writable.
  if (is_create && !is_read_write)
    return false;

  // If we're accessing an existing file, we cannot give exclusive access, and
  // we can't delete it.
  if ((is_exclusive || is_delete) && !is_create)
    return false;

  // The main DB, main journal and master journal cannot be auto-deleted.
  if (is_delete && ((file_type == SQLITE_OPEN_MAIN_DB) ||
                    (file_type == SQLITE_OPEN_MAIN_JOURNAL) ||
                    (file_type == SQLITE_OPEN_MASTER_JOURNAL))) {
    return false;
  }

  // Make sure we're opening the DB directory or that a file type is set.
  return (file_type == SQLITE_OPEN_MAIN_DB) ||
         (file_type == SQLITE_OPEN_TEMP_DB) ||
         (file_type == SQLITE_OPEN_MAIN_JOURNAL) ||
         (file_type == SQLITE_OPEN_TEMP_JOURNAL) ||
         (file_type == SQLITE_OPEN_SUBJOURNAL) ||
         (file_type == SQLITE_OPEN_MASTER_JOURNAL) ||
         (file_type == SQLITE_OPEN_TRANSIENT_DB);
}

void VfsBackend::OpenFile(const FilePath& file_path,
                          int desired_flags,
                          base::ProcessHandle handle,
                          base::PlatformFile* target_handle,
                          base::PlatformFile* target_dir_handle) {
  DCHECK(!file_path.empty());

  // Verify the flags for consistency and create the database
  // directory if it doesn't exist.
  if (!OpenFileFlagsAreConsistent(desired_flags) ||
      !file_util::CreateDirectory(file_path.DirName()))
    return;

  int flags = 0;
  flags |= base::PLATFORM_FILE_READ;
  if (desired_flags & SQLITE_OPEN_READWRITE)
    flags |= base::PLATFORM_FILE_WRITE;

  if (!(desired_flags & SQLITE_OPEN_MAIN_DB)) {
    flags |= base::PLATFORM_FILE_EXCLUSIVE_READ |
             base::PLATFORM_FILE_EXCLUSIVE_WRITE;
  }

  flags |= ((desired_flags & SQLITE_OPEN_CREATE) ?
      base::PLATFORM_FILE_OPEN_ALWAYS : base::PLATFORM_FILE_OPEN);

  if (desired_flags & SQLITE_OPEN_EXCLUSIVE) {
    flags |= base::PLATFORM_FILE_EXCLUSIVE_READ |
             base::PLATFORM_FILE_EXCLUSIVE_WRITE;
  }

  if (desired_flags & SQLITE_OPEN_DELETEONCLOSE) {
    flags |= base::PLATFORM_FILE_TEMPORARY | base::PLATFORM_FILE_HIDDEN |
             base::PLATFORM_FILE_DELETE_ON_CLOSE;
  }

  // Try to open/create the DB file.
  base::PlatformFile file_handle =
      base::CreatePlatformFile(file_path.ToWStringHack(), flags, NULL);
  if (file_handle != base::kInvalidPlatformFileValue) {
#if defined(OS_WIN)
    // Duplicate the file handle.
    if (!DuplicateHandle(GetCurrentProcess(), file_handle,
                         handle, target_handle, 0, false,
                         DUPLICATE_CLOSE_SOURCE | DUPLICATE_SAME_ACCESS)) {
      // file_handle is closed whether or not DuplicateHandle succeeds.
      *target_handle = INVALID_HANDLE_VALUE;
    }
#elif defined(OS_POSIX)
    *target_handle = file_handle;

    int file_type = desired_flags & 0x00007F00;
    bool creating_new_file = (desired_flags & SQLITE_OPEN_CREATE);
    if (creating_new_file && ((file_type == SQLITE_OPEN_MASTER_JOURNAL) ||
                              (file_type == SQLITE_OPEN_MAIN_JOURNAL))) {
      // We return a handle to the containing directory because on POSIX
      // systems the VFS might want to fsync it after changing a file.
      // By returning it here, we avoid an extra IPC call.
      *target_dir_handle = base::CreatePlatformFile(
          file_path.DirName().ToWStringHack(),
          base::PLATFORM_FILE_OPEN | base::PLATFORM_FILE_READ, NULL);
      if (*target_dir_handle == base::kInvalidPlatformFileValue) {
        base::ClosePlatformFile(*target_handle);
        *target_handle = base::kInvalidPlatformFileValue;
      }
    }
#endif
  }
}

void VfsBackend::OpenTempFileInDirectory(
    const FilePath& dir_path,
    int desired_flags,
    base::ProcessHandle handle,
    base::PlatformFile* target_handle,
    base::PlatformFile* target_dir_handle) {
  // We should be able to delete temp files when they're closed
  // and create them as needed
  if (!(desired_flags & SQLITE_OPEN_DELETEONCLOSE) ||
      !(desired_flags & SQLITE_OPEN_CREATE)) {
    return;
  }

  // Get a unique temp file name in the database directory.
  FilePath temp_file_path;
  if (!file_util::CreateTemporaryFileInDir(dir_path, &temp_file_path))
    return;

  OpenFile(temp_file_path, desired_flags, handle,
           target_handle, target_dir_handle);
}

int VfsBackend::DeleteFile(const FilePath& file_path, bool sync_dir) {
  if (!file_util::PathExists(file_path))
    return SQLITE_OK;
  if (!file_util::Delete(file_path, false))
    return SQLITE_IOERR_DELETE;

  int error_code = SQLITE_OK;
#if defined(OS_POSIX)
  if (sync_dir) {
    base::PlatformFile dir_fd = base::CreatePlatformFile(
        file_path.DirName().ToWStringHack(), base::PLATFORM_FILE_READ, NULL);
    if (dir_fd == base::kInvalidPlatformFileValue) {
      error_code = SQLITE_CANTOPEN;
    } else {
      if (fsync(dir_fd))
        error_code = SQLITE_IOERR_DIR_FSYNC;
      base::ClosePlatformFile(dir_fd);
    }
  }
#endif
  return error_code;
}

uint32 VfsBackend::GetFileAttributes(const FilePath& file_path) {
#if defined(OS_WIN)
  uint32 attributes = ::GetFileAttributes(file_path.value().c_str());
#elif defined(OS_POSIX)
  uint32 attributes = 0;
  if (!access(file_path.value().c_str(), R_OK))
    attributes |= static_cast<uint32>(R_OK);
  if (!access(file_path.value().c_str(), W_OK))
    attributes |= static_cast<uint32>(W_OK);
  if (!attributes)
    attributes = -1;
#endif
  return attributes;
}

int64 VfsBackend::GetFileSize(const FilePath& file_path) {
  int64 size = 0;
  return (file_util::GetFileSize(file_path, &size) ? size : 0);
}

} // namespace webkit_database
