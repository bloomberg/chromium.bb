// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/chromeos/fileapi/remote_file_system_operation.h"

#include "base/bind.h"
#include "base/utf_string_conversions.h"
#include "base/platform_file.h"
#include "base/values.h"
#include "googleurl/src/gurl.h"
#include "net/url_request/url_request_context.h"
#include "webkit/chromeos/fileapi/remote_file_stream_writer.h"
#include "webkit/fileapi/file_system_callback_dispatcher.h"
#include "webkit/fileapi/file_system_url.h"
#include "webkit/fileapi/file_writer_delegate.h"

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
  remote_proxy_->GetFileInfo(url,
      base::Bind(&RemoteFileSystemOperation::DidGetMetadata,
                 base::Owned(this), callback));
}

void RemoteFileSystemOperation::DirectoryExists(const FileSystemURL& url,
    const StatusCallback& callback) {
  DCHECK(SetPendingOperationType(kOperationDirectoryExists));
  remote_proxy_->GetFileInfo(url,
      base::Bind(&RemoteFileSystemOperation::DidDirectoryExists,
                 base::Owned(this), callback));
}

void RemoteFileSystemOperation::FileExists(const FileSystemURL& url,
    const StatusCallback& callback) {
  DCHECK(SetPendingOperationType(kOperationFileExists));
  remote_proxy_->GetFileInfo(url,
      base::Bind(base::Bind(&RemoteFileSystemOperation::DidFileExists,
                            base::Owned(this), callback)));
}

void RemoteFileSystemOperation::ReadDirectory(const FileSystemURL& url,
    const ReadDirectoryCallback& callback) {
  DCHECK(SetPendingOperationType(kOperationReadDirectory));
  remote_proxy_->ReadDirectory(url,
      base::Bind(&RemoteFileSystemOperation::DidReadDirectory,
                 base::Owned(this), callback));
}

void RemoteFileSystemOperation::Remove(const FileSystemURL& url, bool recursive,
                                       const StatusCallback& callback) {
  DCHECK(SetPendingOperationType(kOperationRemove));
  remote_proxy_->Remove(url, recursive,
      base::Bind(&RemoteFileSystemOperation::DidFinishFileOperation,
                 base::Owned(this), callback));
}


void RemoteFileSystemOperation::CreateDirectory(
    const FileSystemURL& url, bool exclusive, bool recursive,
    const StatusCallback& callback) {
  DCHECK(SetPendingOperationType(kOperationCreateDirectory));
  remote_proxy_->CreateDirectory(url, exclusive, recursive,
      base::Bind(&RemoteFileSystemOperation::DidFinishFileOperation,
                 base::Owned(this), callback));
}

void RemoteFileSystemOperation::CreateFile(const FileSystemURL& url,
                                           bool exclusive,
                                           const StatusCallback& callback) {
  DCHECK(SetPendingOperationType(kOperationCreateFile));
  remote_proxy_->CreateFile(url, exclusive,
      base::Bind(&RemoteFileSystemOperation::DidFinishFileOperation,
                 base::Owned(this), callback));
}

void RemoteFileSystemOperation::Copy(const FileSystemURL& src_url,
                                     const FileSystemURL& dest_url,
                                     const StatusCallback& callback) {
  DCHECK(SetPendingOperationType(kOperationCopy));

  remote_proxy_->Copy(src_url, dest_url,
      base::Bind(&RemoteFileSystemOperation::DidFinishFileOperation,
                 base::Owned(this), callback));
}

void RemoteFileSystemOperation::Move(const FileSystemURL& src_url,
                                     const FileSystemURL& dest_url,
                                     const StatusCallback& callback) {
  DCHECK(SetPendingOperationType(kOperationMove));

  remote_proxy_->Move(src_url, dest_url,
      base::Bind(&RemoteFileSystemOperation::DidFinishFileOperation,
                 base::Owned(this), callback));
}

void RemoteFileSystemOperation::Write(
    const net::URLRequestContext* url_request_context,
    const FileSystemURL& url,
    const GURL& blob_url,
    int64 offset,
    const WriteCallback& callback) {
  DCHECK(SetPendingOperationType(kOperationWrite));
  DCHECK(write_callback_.is_null());

  write_callback_ = callback;
  file_writer_delegate_.reset(
      new fileapi::FileWriterDelegate(
          base::Bind(&RemoteFileSystemOperation::DidWrite,
                     // FileWriterDelegate is owned by |this|. So Unretained.
                     base::Unretained(this)),
          scoped_ptr<fileapi::FileStreamWriter>(
              new fileapi::RemoteFileStreamWriter(remote_proxy_,
                                                  url,
                                                  offset))));

  scoped_ptr<net::URLRequest> blob_request(url_request_context->CreateRequest(
      blob_url, file_writer_delegate_.get()));

  file_writer_delegate_->Start(blob_request.Pass());
}

void RemoteFileSystemOperation::Truncate(const FileSystemURL& url,
                                         int64 length,
                                         const StatusCallback& callback) {
  DCHECK(SetPendingOperationType(kOperationTruncate));

  remote_proxy_->Truncate(url, length,
      base::Bind(&RemoteFileSystemOperation::DidFinishFileOperation,
                 base::Owned(this), callback));
}

void RemoteFileSystemOperation::Cancel(const StatusCallback& cancel_callback) {
  if (file_writer_delegate_.get()) {
    DCHECK_EQ(kOperationWrite, pending_operation_);

    // Writes are done without proxying through FileUtilProxy after the initial
    // opening of the PlatformFile.  All state changes are done on this thread,
    // so we're guaranteed to be able to shut down atomically.
    const bool delete_now = file_writer_delegate_->Cancel();

    if (!write_callback_.is_null()) {
      // Notify the failure status to the ongoing operation's callback.
      write_callback_.Run(base::PLATFORM_FILE_ERROR_ABORT, 0, false);
    }
    cancel_callback.Run(base::PLATFORM_FILE_OK);
    write_callback_.Reset();

    if (delete_now) {
      delete this;
      return;
    }
  } else {
    DCHECK_EQ(kOperationTruncate, pending_operation_);
    // We're cancelling a truncate operation, but we can't actually stop it
    // since it's been proxied to another thread.  We need to save the
    // cancel_callback so that when the truncate returns, it can see that it's
    // been cancelled, report it, and report that the cancel has succeeded.
    DCHECK(cancel_callback_.is_null());
    cancel_callback_ = cancel_callback;
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
                 base::Owned(this), callback));
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
                 base::Owned(this), callback));
}

void RemoteFileSystemOperation::NotifyCloseFile(
    const fileapi::FileSystemURL& url) {
  DCHECK(SetPendingOperationType(kOperationCloseFile));
  remote_proxy_->NotifyCloseFile(url);
  // Unlike other operations, NotifyCloseFile does not require callback.
  // So it must be deleted here right now.
  delete this;
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
  remote_proxy_->CreateSnapshotFile(
      url,
      base::Bind(&RemoteFileSystemOperation::DidCreateSnapshotFile,
                 base::Owned(this), callback));
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
    const base::PlatformFileInfo& file_info,
    const base::FilePath& unused) {
  if (rv == base::PLATFORM_FILE_OK && !file_info.is_directory)
    rv = base::PLATFORM_FILE_ERROR_NOT_A_DIRECTORY;
  callback.Run(rv);
}

void RemoteFileSystemOperation::DidFileExists(
    const StatusCallback& callback,
    base::PlatformFileError rv,
    const base::PlatformFileInfo& file_info,
    const base::FilePath& unused) {
  if (rv == base::PLATFORM_FILE_OK && file_info.is_directory)
    rv = base::PLATFORM_FILE_ERROR_NOT_A_FILE;
  callback.Run(rv);
}

void RemoteFileSystemOperation::DidGetMetadata(
    const GetMetadataCallback& callback,
    base::PlatformFileError rv,
    const base::PlatformFileInfo& file_info,
    const base::FilePath& platform_path) {
  callback.Run(rv, file_info, platform_path);
}

void RemoteFileSystemOperation::DidReadDirectory(
    const ReadDirectoryCallback& callback,
    base::PlatformFileError rv,
    const std::vector<base::FileUtilProxy::Entry>& entries,
    bool has_more) {
  callback.Run(rv, entries, has_more /* has_more */);
}

void RemoteFileSystemOperation::DidWrite(
    base::PlatformFileError rv,
    int64 bytes,
    FileWriterDelegate::WriteProgressStatus write_status) {
  if (write_callback_.is_null()) {
    // If cancelled, callback is already invoked and set to null in Cancel().
    // We must not call it twice. Just shut down this operation object.
    delete this;
    return;
  }

  bool complete = (write_status != FileWriterDelegate::SUCCESS_IO_PENDING);
  write_callback_.Run(rv, bytes, complete);
  if (rv != base::PLATFORM_FILE_OK || complete) {
    // Other Did*'s doesn't have "delete this", because it is automatic since
    // they are base::Owned by the caller of the callback. For DidWrite, the
    // owner is file_writer_delegate_ which itself is owned by this Operation
    // object. Hence we need manual life time management here.
    // TODO(kinaba): think about refactoring FileWriterDelegate to be self
    // destructing, for avoiding the manual management.
    delete this;
  }
}

void RemoteFileSystemOperation::DidFinishFileOperation(
    const StatusCallback& callback,
    base::PlatformFileError rv) {
  if (!cancel_callback_.is_null()) {
    DCHECK_EQ(kOperationTruncate, pending_operation_);

    callback.Run(base::PLATFORM_FILE_ERROR_ABORT);
    cancel_callback_.Run(base::PLATFORM_FILE_OK);
    cancel_callback_.Reset();
  } else {
    callback.Run(rv);
  }
}

void RemoteFileSystemOperation::DidCreateSnapshotFile(
    const SnapshotFileCallback& callback,
    base::PlatformFileError result,
    const base::PlatformFileInfo& file_info,
    const base::FilePath& platform_path,
    const scoped_refptr<webkit_blob::ShareableFileReference>& file_ref) {
  callback.Run(result, file_info, platform_path, file_ref);
}

void RemoteFileSystemOperation::DidOpenFile(
    const OpenFileCallback& callback,
    base::PlatformFileError result,
    base::PlatformFile file,
    base::ProcessHandle peer_handle) {
  callback.Run(result, file, peer_handle);
}

}  // namespace chromeos
