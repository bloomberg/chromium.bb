// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/browser/chromeos/fileapi/remote_file_system_operation.h"

#include "base/bind.h"
#include "base/platform_file.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "googleurl/src/gurl.h"
#include "net/url_request/url_request.h"
#include "webkit/browser/chromeos/fileapi/remote_file_stream_writer.h"
#include "webkit/browser/fileapi/file_system_url.h"
#include "webkit/browser/fileapi/file_writer_delegate.h"

using fileapi::FileSystemURL;

namespace chromeos {

RemoteFileSystemOperation::RemoteFileSystemOperation(
    scoped_refptr<fileapi::RemoteFileSystemProxyInterface> remote_proxy)
      : remote_proxy_(remote_proxy),
        pending_operation_(kOperationNone) {
}

RemoteFileSystemOperation::~RemoteFileSystemOperation() {
}

void RemoteFileSystemOperation::GetMetadata(const FileSystemURL& url,
    const GetMetadataCallback& callback) {
  DCHECK(SetPendingOperationType(kOperationGetMetadata));
  remote_proxy_->GetFileInfo(url, callback);
}

void RemoteFileSystemOperation::DirectoryExists(const FileSystemURL& url,
    const StatusCallback& callback) {
  DCHECK(SetPendingOperationType(kOperationDirectoryExists));
  remote_proxy_->GetFileInfo(url,
      base::Bind(&RemoteFileSystemOperation::DidDirectoryExists,
                 AsWeakPtr(), callback));
}

void RemoteFileSystemOperation::FileExists(const FileSystemURL& url,
    const StatusCallback& callback) {
  DCHECK(SetPendingOperationType(kOperationFileExists));
  remote_proxy_->GetFileInfo(url,
      base::Bind(base::Bind(&RemoteFileSystemOperation::DidFileExists,
                            AsWeakPtr(), callback)));
}

void RemoteFileSystemOperation::ReadDirectory(const FileSystemURL& url,
    const ReadDirectoryCallback& callback) {
  DCHECK(SetPendingOperationType(kOperationReadDirectory));
  remote_proxy_->ReadDirectory(url, callback);
}

void RemoteFileSystemOperation::Remove(const FileSystemURL& url, bool recursive,
                                       const StatusCallback& callback) {
  DCHECK(SetPendingOperationType(kOperationRemove));
  remote_proxy_->Remove(url, recursive,
      base::Bind(&RemoteFileSystemOperation::DidFinishFileOperation,
                 AsWeakPtr(), callback));
}


void RemoteFileSystemOperation::CreateDirectory(
    const FileSystemURL& url, bool exclusive, bool recursive,
    const StatusCallback& callback) {
  DCHECK(SetPendingOperationType(kOperationCreateDirectory));
  remote_proxy_->CreateDirectory(url, exclusive, recursive,
      base::Bind(&RemoteFileSystemOperation::DidFinishFileOperation,
                 AsWeakPtr(), callback));
}

void RemoteFileSystemOperation::CreateFile(const FileSystemURL& url,
                                           bool exclusive,
                                           const StatusCallback& callback) {
  DCHECK(SetPendingOperationType(kOperationCreateFile));
  remote_proxy_->CreateFile(url, exclusive,
      base::Bind(&RemoteFileSystemOperation::DidFinishFileOperation,
                 AsWeakPtr(), callback));
}

void RemoteFileSystemOperation::Copy(const FileSystemURL& src_url,
                                     const FileSystemURL& dest_url,
                                     const StatusCallback& callback) {
  DCHECK(SetPendingOperationType(kOperationCopy));

  remote_proxy_->Copy(src_url, dest_url,
      base::Bind(&RemoteFileSystemOperation::DidFinishFileOperation,
                 AsWeakPtr(), callback));
}

void RemoteFileSystemOperation::Move(const FileSystemURL& src_url,
                                     const FileSystemURL& dest_url,
                                     const StatusCallback& callback) {
  DCHECK(SetPendingOperationType(kOperationMove));

  remote_proxy_->Move(src_url, dest_url,
      base::Bind(&RemoteFileSystemOperation::DidFinishFileOperation,
                 AsWeakPtr(), callback));
}

void RemoteFileSystemOperation::Write(
    const FileSystemURL& url,
    scoped_ptr<fileapi::FileWriterDelegate> writer_delegate,
    scoped_ptr<net::URLRequest> blob_request,
    const WriteCallback& callback) {
  DCHECK(SetPendingOperationType(kOperationWrite));
  file_writer_delegate_ = writer_delegate.Pass();
  file_writer_delegate_->Start(
      blob_request.Pass(),
      base::Bind(&RemoteFileSystemOperation::DidWrite, AsWeakPtr(), callback));
}

void RemoteFileSystemOperation::Truncate(const FileSystemURL& url,
                                         int64 length,
                                         const StatusCallback& callback) {
  DCHECK(SetPendingOperationType(kOperationTruncate));

  remote_proxy_->Truncate(url, length,
      base::Bind(&RemoteFileSystemOperation::DidFinishFileOperation,
                 AsWeakPtr(), callback));
}

void RemoteFileSystemOperation::Cancel(const StatusCallback& cancel_callback) {
  DCHECK(cancel_callback_.is_null());
  cancel_callback_ = cancel_callback;

  if (file_writer_delegate_) {
    DCHECK_EQ(kOperationWrite, pending_operation_);
    // This will call DidWrite() with ABORT status code.
    file_writer_delegate_->Cancel();
  } else {
    // For truncate we have no way to cancel the inflight operation (for now).
    // Let it just run and dispatch cancel callback later.
    DCHECK_EQ(kOperationTruncate, pending_operation_);
  }
}

void RemoteFileSystemOperation::TouchFile(const FileSystemURL& url,
                                          const base::Time& last_access_time,
                                          const base::Time& last_modified_time,
                                          const StatusCallback& callback) {
  DCHECK(SetPendingOperationType(kOperationTouchFile));
  remote_proxy_->TouchFile(
      url,
      last_access_time,
      last_modified_time,
      base::Bind(&RemoteFileSystemOperation::DidFinishFileOperation,
                 AsWeakPtr(), callback));
}

void RemoteFileSystemOperation::OpenFile(const FileSystemURL& url,
                                         int file_flags,
                                         base::ProcessHandle peer_handle,
                                         const OpenFileCallback& callback) {
  DCHECK(SetPendingOperationType(kOperationOpenFile));
  remote_proxy_->OpenFile(
      url,
      file_flags,
      peer_handle,
      base::Bind(&RemoteFileSystemOperation::DidOpenFile,
                 AsWeakPtr(), url, callback));
}

fileapi::LocalFileSystemOperation*
RemoteFileSystemOperation::AsLocalFileSystemOperation() {
  NOTIMPLEMENTED();
  return NULL;
}

void RemoteFileSystemOperation::CreateSnapshotFile(
    const FileSystemURL& url,
    const SnapshotFileCallback& callback) {
  DCHECK(SetPendingOperationType(kOperationCreateSnapshotFile));
  remote_proxy_->CreateSnapshotFile(url, callback);
}

bool RemoteFileSystemOperation::SetPendingOperationType(OperationType type) {
  if (pending_operation_ != kOperationNone)
    return false;
  pending_operation_ = type;
  return true;
}

void RemoteFileSystemOperation::DidDirectoryExists(
    const StatusCallback& callback,
    base::PlatformFileError rv,
    const base::PlatformFileInfo& file_info) {
  if (rv == base::PLATFORM_FILE_OK && !file_info.is_directory)
    rv = base::PLATFORM_FILE_ERROR_NOT_A_DIRECTORY;
  callback.Run(rv);
}

void RemoteFileSystemOperation::DidFileExists(
    const StatusCallback& callback,
    base::PlatformFileError rv,
    const base::PlatformFileInfo& file_info) {
  if (rv == base::PLATFORM_FILE_OK && file_info.is_directory)
    rv = base::PLATFORM_FILE_ERROR_NOT_A_FILE;
  callback.Run(rv);
}

void RemoteFileSystemOperation::DidWrite(
    const WriteCallback& write_callback,
    base::PlatformFileError rv,
    int64 bytes,
    FileWriterDelegate::WriteProgressStatus write_status) {
  bool complete = (write_status != FileWriterDelegate::SUCCESS_IO_PENDING);
  StatusCallback cancel_callback = cancel_callback_;
  write_callback.Run(rv, bytes, complete);
  if (!cancel_callback.is_null())
    cancel_callback.Run(base::PLATFORM_FILE_OK);
}

void RemoteFileSystemOperation::DidFinishFileOperation(
    const StatusCallback& callback,
    base::PlatformFileError rv) {
  if (!cancel_callback_.is_null()) {
    DCHECK_EQ(kOperationTruncate, pending_operation_);

    StatusCallback cancel_callback = cancel_callback_;
    callback.Run(base::PLATFORM_FILE_ERROR_ABORT);
    cancel_callback.Run(base::PLATFORM_FILE_OK);
  } else {
    callback.Run(rv);
  }
}

void RemoteFileSystemOperation::DidOpenFile(
    const fileapi::FileSystemURL& url,
    const OpenFileCallback& callback,
    base::PlatformFileError result,
    base::PlatformFile file,
    base::ProcessHandle peer_handle) {
  callback.Run(
      result, file,
      base::Bind(&fileapi::RemoteFileSystemProxyInterface::NotifyCloseFile,
                 remote_proxy_, url),
      peer_handle);
}

}  // namespace chromeos
