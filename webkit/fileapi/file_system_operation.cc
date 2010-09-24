// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/file_system_operation.h"

#include "googleurl/src/gurl.h"
#include "webkit/fileapi/file_system_callback_dispatcher.h"

namespace fileapi {

FileSystemOperation::FileSystemOperation(
    FileSystemCallbackDispatcher* dispatcher,
    scoped_refptr<base::MessageLoopProxy> proxy)
    : proxy_(proxy),
      dispatcher_(dispatcher),
      callback_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
  DCHECK(dispatcher);
#ifndef NDEBUG
  operation_pending_ = false;
#endif
}

FileSystemOperation::~FileSystemOperation() {
}

void FileSystemOperation::CreateFile(const FilePath& path,
                                     bool exclusive) {
#ifndef NDEBUG
  DCHECK(!operation_pending_);
  operation_pending_ = true;
#endif

  base::FileUtilProxy::CreateOrOpen(
    proxy_, path, base::PLATFORM_FILE_CREATE | base::PLATFORM_FILE_READ,
    callback_factory_.NewCallback(
        exclusive ? &FileSystemOperation::DidCreateFileExclusive
                  : &FileSystemOperation::DidCreateFileNonExclusive));
}

void FileSystemOperation::CreateDirectory(const FilePath& path,
                                          bool exclusive,
                                          bool recursive) {
#ifndef NDEBUG
  DCHECK(!operation_pending_);
  operation_pending_ = true;
#endif

  base::FileUtilProxy::CreateDirectory(
      proxy_, path, exclusive, recursive, callback_factory_.NewCallback(
          &FileSystemOperation::DidFinishFileOperation));
}

void FileSystemOperation::Copy(const FilePath& src_path,
                               const FilePath& dest_path) {
#ifndef NDEBUG
  DCHECK(!operation_pending_);
  operation_pending_ = true;
#endif

  base::FileUtilProxy::Copy(proxy_, src_path, dest_path,
      callback_factory_.NewCallback(
          &FileSystemOperation::DidFinishFileOperation));
}

void FileSystemOperation::Move(const FilePath& src_path,
                               const FilePath& dest_path) {
#ifndef NDEBUG
  DCHECK(!operation_pending_);
  operation_pending_ = true;
#endif

  base::FileUtilProxy::Move(proxy_, src_path, dest_path,
      callback_factory_.NewCallback(
          &FileSystemOperation::DidFinishFileOperation));
}

void FileSystemOperation::DirectoryExists(const FilePath& path) {
#ifndef NDEBUG
  DCHECK(!operation_pending_);
  operation_pending_ = true;
#endif

  base::FileUtilProxy::GetFileInfo(proxy_, path, callback_factory_.NewCallback(
      &FileSystemOperation::DidDirectoryExists));
}

void FileSystemOperation::FileExists(const FilePath& path) {
#ifndef NDEBUG
  DCHECK(!operation_pending_);
  operation_pending_ = true;
#endif

  base::FileUtilProxy::GetFileInfo(proxy_, path, callback_factory_.NewCallback(
      &FileSystemOperation::DidFileExists));
}

void FileSystemOperation::GetMetadata(const FilePath& path) {
#ifndef NDEBUG
  DCHECK(!operation_pending_);
  operation_pending_ = true;
#endif

  base::FileUtilProxy::GetFileInfo(proxy_, path, callback_factory_.NewCallback(
      &FileSystemOperation::DidGetMetadata));
}

void FileSystemOperation::ReadDirectory(const FilePath& path) {
#ifndef NDEBUG
  DCHECK(!operation_pending_);
  operation_pending_ = true;
#endif

  base::FileUtilProxy::ReadDirectory(proxy_, path,
      callback_factory_.NewCallback(
          &FileSystemOperation::DidReadDirectory));
}

void FileSystemOperation::Remove(const FilePath& path) {
#ifndef NDEBUG
  DCHECK(!operation_pending_);
  operation_pending_ = true;
#endif

  base::FileUtilProxy::Delete(proxy_, path, callback_factory_.NewCallback(
      &FileSystemOperation::DidFinishFileOperation));
}


void FileSystemOperation::Write(
    const FilePath&,
    const GURL&,
    int64) {
#ifndef NDEBUG
  DCHECK(!operation_pending_);
  operation_pending_ = true;
#endif
  NOTREACHED();
  // TODO(ericu):
  // Set up a loop that, via multiple callback invocations, reads from a
  // URLRequest wrapping blob_url, writes the bytes to the file, reports
  // progress events no more frequently than some set rate, and periodically
  // checks to see if it's been cancelled.
}

void FileSystemOperation::Truncate(const FilePath& path, int64 length) {
#ifndef NDEBUG
  DCHECK(!operation_pending_);
  operation_pending_ = true;
#endif
  // TODO(ericu):
  NOTREACHED();
}

void FileSystemOperation::Cancel() {
#ifndef NDEBUG
  DCHECK(operation_pending_);
#endif
  NOTREACHED();
  // TODO(ericu):
  // Make sure this was done on a FileSystemOperation used for a Write.
  // Then set a flag that ensures that the Write loop will exit without
  // reporting any more progress, with a failure notification.
}

void FileSystemOperation::DidCreateFileExclusive(
    base::PlatformFileError rv,
    base::PassPlatformFile file,
    bool created) {
  DidFinishFileOperation(rv);
}

void FileSystemOperation::DidCreateFileNonExclusive(
    base::PlatformFileError rv,
    base::PassPlatformFile file,
    bool created) {
  // Suppress the already exists error and report success.
  if (rv == base::PLATFORM_FILE_OK ||
      rv == base::PLATFORM_FILE_ERROR_EXISTS)
    dispatcher_->DidSucceed();
  else
    dispatcher_->DidFail(rv);
}

void FileSystemOperation::DidFinishFileOperation(
    base::PlatformFileError rv) {
  if (rv == base::PLATFORM_FILE_OK)
    dispatcher_->DidSucceed();
  else
    dispatcher_->DidFail(rv);
}

void FileSystemOperation::DidDirectoryExists(
    base::PlatformFileError rv,
    const base::PlatformFileInfo& file_info) {
  if (rv == base::PLATFORM_FILE_OK) {
    if (file_info.is_directory)
      dispatcher_->DidSucceed();
    else
      dispatcher_->DidFail(base::PLATFORM_FILE_ERROR_FAILED);
  } else
    dispatcher_->DidFail(rv);
}

void FileSystemOperation::DidFileExists(
    base::PlatformFileError rv,
    const base::PlatformFileInfo& file_info) {
  if (rv == base::PLATFORM_FILE_OK) {
    if (file_info.is_directory)
      dispatcher_->DidFail(base::PLATFORM_FILE_ERROR_FAILED);
    else
      dispatcher_->DidSucceed();
  } else
    dispatcher_->DidFail(rv);
}

void FileSystemOperation::DidGetMetadata(
    base::PlatformFileError rv,
    const base::PlatformFileInfo& file_info) {
  if (rv == base::PLATFORM_FILE_OK)
    dispatcher_->DidReadMetadata(file_info);
  else
    dispatcher_->DidFail(rv);
}

void FileSystemOperation::DidReadDirectory(
    base::PlatformFileError rv,
    const std::vector<base::file_util_proxy::Entry>& entries) {
  if (rv == base::PLATFORM_FILE_OK)
    dispatcher_->DidReadDirectory(entries, false /* has_more */);
  else
    dispatcher_->DidFail(rv);
}

void FileSystemOperation::DidWrite(
    base::PlatformFileError rv,
    int64 bytes,
    bool complete) {
  if (rv == base::PLATFORM_FILE_OK)
    /* dispatcher_->DidWrite(bytes, complete) TODO(ericu): Coming soon. */ {}
  else
    dispatcher_->DidFail(rv);
}

}  // namespace fileapi
