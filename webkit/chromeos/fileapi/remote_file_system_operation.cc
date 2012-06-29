// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/chromeos/fileapi/remote_file_system_operation.h"

#include "base/bind.h"
#include "base/utf_string_conversions.h"
#include "base/platform_file.h"
#include "base/values.h"
#include "googleurl/src/gurl.h"
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

  file_writer_delegate_.reset(
      new fileapi::FileWriterDelegate(
          base::Bind(&RemoteFileSystemOperation::DidWrite,
                     // FileWriterDelegate is owned by |this|. So Unretained.
                     base::Unretained(this),
                     callback),
          scoped_ptr<fileapi::FileStreamWriter>(
              new fileapi::RemoteFileStreamWriter(remote_proxy_,
                                                  url,
                                                  offset))));

  scoped_ptr<net::URLRequest> blob_request(new net::URLRequest(
      blob_url, file_writer_delegate_.get(), url_request_context));

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
  // TODO(kinaba): crbug.com/132403. implement.
  NOTIMPLEMENTED();
}

void RemoteFileSystemOperation::TouchFile(const FileSystemURL& url,
                                          const base::Time& last_access_time,
                                          const base::Time& last_modified_time,
                                          const StatusCallback& callback) {
  NOTIMPLEMENTED();
}

void RemoteFileSystemOperation::OpenFile(const FileSystemURL& url,
                                         int file_flags,
                                         base::ProcessHandle peer_handle,
                                         const OpenFileCallback& callback) {
  // TODO(zelidrag): Implement file write operations.
  if ((file_flags & base::PLATFORM_FILE_CREATE) ||
      (file_flags & base::PLATFORM_FILE_WRITE) ||
      (file_flags & base::PLATFORM_FILE_EXCLUSIVE_WRITE) ||
      (file_flags & base::PLATFORM_FILE_CREATE_ALWAYS) ||
      (file_flags & base::PLATFORM_FILE_OPEN_TRUNCATED) ||
      (file_flags & base::PLATFORM_FILE_DELETE_ON_CLOSE)) {
    NOTIMPLEMENTED() << "File write operations not supported " << url.spec();
    return;
  }
  DCHECK(SetPendingOperationType(kOperationOpenFile));
  remote_proxy_->OpenFile(
      url,
      file_flags,
      peer_handle,
      base::Bind(&RemoteFileSystemOperation::DidOpenFile,
                 base::Owned(this), callback));
}

fileapi::FileSystemOperation*
RemoteFileSystemOperation::AsFileSystemOperation() {
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
    const FilePath& unused) {
  if (rv == base::PLATFORM_FILE_OK && !file_info.is_directory)
    rv = base::PLATFORM_FILE_ERROR_NOT_A_DIRECTORY;
  callback.Run(rv);
}

void RemoteFileSystemOperation::DidFileExists(
    const StatusCallback& callback,
    base::PlatformFileError rv,
    const base::PlatformFileInfo& file_info,
    const FilePath& unused) {
  if (rv == base::PLATFORM_FILE_OK && file_info.is_directory)
    rv = base::PLATFORM_FILE_ERROR_NOT_A_FILE;
  callback.Run(rv);
}

void RemoteFileSystemOperation::DidGetMetadata(
    const GetMetadataCallback& callback,
    base::PlatformFileError rv,
    const base::PlatformFileInfo& file_info,
    const FilePath& platform_path) {
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
    const WriteCallback& callback,
    base::PlatformFileError rv,
    int64 bytes,
    bool complete) {
  callback.Run(rv, bytes, complete);
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
  callback.Run(rv);
}

void RemoteFileSystemOperation::DidCreateSnapshotFile(
    const SnapshotFileCallback& callback,
    base::PlatformFileError result,
    const base::PlatformFileInfo& file_info,
    const FilePath& platform_path,
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
