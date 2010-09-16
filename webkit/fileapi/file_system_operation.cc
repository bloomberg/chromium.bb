// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/file_system_operation.h"

#include "webkit/fileapi/file_system_operation_client.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFileError.h"

namespace {
// Utility method for error conversions.
WebKit::WebFileError PlatformFileErrorToWebFileError(
    base::PlatformFileError rv) {
  switch (rv) {
    case base::PLATFORM_FILE_ERROR_NOT_FOUND:
        return WebKit::WebFileErrorNotFound;
    case base::PLATFORM_FILE_ERROR_INVALID_OPERATION:
    case base::PLATFORM_FILE_ERROR_EXISTS:
    case base::PLATFORM_FILE_ERROR_NOT_A_DIRECTORY:
         return WebKit::WebFileErrorInvalidModification;
    case base::PLATFORM_FILE_ERROR_ACCESS_DENIED:
        return WebKit::WebFileErrorNoModificationAllowed;
    default:
        return WebKit::WebFileErrorInvalidModification;
  }
}
}  // namespace

namespace fileapi {

FileSystemOperation::FileSystemOperation(
    FileSystemOperationClient* client,
    scoped_refptr<base::MessageLoopProxy> proxy)
    : proxy_(proxy),
      client_(client),
      callback_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
      operation_pending_(false) {
  DCHECK(client_);
}

void FileSystemOperation::CreateFile(const FilePath& path,
                                     bool exclusive) {
  DCHECK(!operation_pending_);
  operation_pending_ = true;
  base::FileUtilProxy::CreateOrOpen(
    proxy_, path, base::PLATFORM_FILE_CREATE | base::PLATFORM_FILE_READ,
    callback_factory_.NewCallback(
        exclusive ? &FileSystemOperation::DidCreateFileExclusive
                  : &FileSystemOperation::DidCreateFileNonExclusive));
}

void FileSystemOperation::CreateDirectory(const FilePath& path,
                                          bool exclusive) {
  DCHECK(!operation_pending_);
  operation_pending_ = true;
  base::FileUtilProxy::CreateDirectory(proxy_, path, exclusive,
      false /* recursive */, callback_factory_.NewCallback(
          &FileSystemOperation::DidFinishFileOperation));
}

void FileSystemOperation::Copy(const FilePath& src_path,
                               const FilePath& dest_path) {
  DCHECK(!operation_pending_);
  operation_pending_ = true;
  base::FileUtilProxy::Copy(proxy_, src_path, dest_path,
      callback_factory_.NewCallback(
          &FileSystemOperation::DidFinishFileOperation));
}

void FileSystemOperation::Move(const FilePath& src_path,
                               const FilePath& dest_path) {
  DCHECK(!operation_pending_);
  operation_pending_ = true;
  base::FileUtilProxy::Move(proxy_, src_path, dest_path,
      callback_factory_.NewCallback(
          &FileSystemOperation::DidFinishFileOperation));
}

void FileSystemOperation::DirectoryExists(const FilePath& path) {
  DCHECK(!operation_pending_);
  operation_pending_ = true;
  base::FileUtilProxy::GetFileInfo(proxy_, path, callback_factory_.NewCallback(
      &FileSystemOperation::DidDirectoryExists));
}

void FileSystemOperation::FileExists(const FilePath& path) {
  DCHECK(!operation_pending_);
  operation_pending_ = true;
  base::FileUtilProxy::GetFileInfo(proxy_, path, callback_factory_.NewCallback(
      &FileSystemOperation::DidFileExists));
}

void FileSystemOperation::GetMetadata(const FilePath& path) {
  DCHECK(!operation_pending_);
  operation_pending_ = true;
  base::FileUtilProxy::GetFileInfo(proxy_, path, callback_factory_.NewCallback(
      &FileSystemOperation::DidGetMetadata));
}

void FileSystemOperation::ReadDirectory(const FilePath& path) {
  DCHECK(!operation_pending_);
  operation_pending_ = true;
  base::FileUtilProxy::ReadDirectory(proxy_, path,
      callback_factory_.NewCallback(
          &FileSystemOperation::DidReadDirectory));
}

void FileSystemOperation::Remove(const FilePath& path) {
  DCHECK(!operation_pending_);
  operation_pending_ = true;
  base::FileUtilProxy::Delete(proxy_, path, callback_factory_.NewCallback(
      &FileSystemOperation::DidFinishFileOperation));
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
    client_->DidSucceed(this);
  else
    client_->DidFail(PlatformFileErrorToWebFileError(rv), this);
}

void FileSystemOperation::DidFinishFileOperation(
    base::PlatformFileError rv) {
  if (rv == base::PLATFORM_FILE_OK)
    client_->DidSucceed(this);
  else
    client_->DidFail(PlatformFileErrorToWebFileError(rv), this);
}

void FileSystemOperation::DidDirectoryExists(
    base::PlatformFileError rv,
    const base::PlatformFileInfo& file_info) {
  if (rv == base::PLATFORM_FILE_OK) {
    if (file_info.is_directory)
      client_->DidSucceed(this);
    else
      client_->DidFail(WebKit::WebFileErrorInvalidState,
                       this);
  } else {
    // Something else went wrong.
    client_->DidFail(PlatformFileErrorToWebFileError(rv), this);
  }
}

void FileSystemOperation::DidFileExists(
    base::PlatformFileError rv,
    const base::PlatformFileInfo& file_info) {
  if (rv == base::PLATFORM_FILE_OK) {
    if (file_info.is_directory)
      client_->DidFail(WebKit::WebFileErrorInvalidState,
                       this);
    else
      client_->DidSucceed(this);
  } else {
    // Something else went wrong.
    client_->DidFail(PlatformFileErrorToWebFileError(rv), this);
  }
}

void FileSystemOperation::DidGetMetadata(
    base::PlatformFileError rv,
    const base::PlatformFileInfo& file_info) {
  if (rv == base::PLATFORM_FILE_OK)
    client_->DidReadMetadata(file_info, this);
  else
    client_->DidFail(PlatformFileErrorToWebFileError(rv), this);
}

void FileSystemOperation::DidReadDirectory(
    base::PlatformFileError rv,
    const std::vector<base::file_util_proxy::Entry>& entries) {
  if (rv == base::PLATFORM_FILE_OK)
    client_->DidReadDirectory(
        entries, false /* has_more */ , this);
  else
    client_->DidFail(PlatformFileErrorToWebFileError(rv), this);
}

}  // namespace fileapi

