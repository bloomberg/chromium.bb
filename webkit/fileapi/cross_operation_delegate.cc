// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/cross_operation_delegate.h"

#include "base/bind.h"
#include "webkit/blob/shareable_file_reference.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_operation_context.h"
#include "webkit/fileapi/file_system_util.h"
#include "webkit/fileapi/local_file_system_operation.h"

namespace fileapi {

CrossOperationDelegate::CrossOperationDelegate(
    LocalFileSystemOperation* src_root_operation,
    LocalFileSystemOperation* dest_root_operation,
    const FileSystemURL& src_root,
    const FileSystemURL& dest_root,
    OperationType operation_type,
    const StatusCallback& callback)
    : RecursiveOperationDelegate(src_root_operation),
      src_root_(src_root),
      dest_root_(dest_root),
      operation_type_(operation_type),
      callback_(callback),
      dest_root_operation_(dest_root_operation) {
  same_file_system_ = AreSameFileSystem(src_root_, dest_root_);
}

CrossOperationDelegate::~CrossOperationDelegate() {
}

void CrossOperationDelegate::Run() {
  // Not supported; this should never be called.
  NOTREACHED();
}

void CrossOperationDelegate::RunRecursively() {
  // Perform light-weight checks first.

  // It is an error to try to copy/move an entry into its child.
  if (same_file_system_ && src_root_.path().IsParent(dest_root_.path())) {
    callback_.Run(base::PLATFORM_FILE_ERROR_INVALID_OPERATION);
    return;
  }

  // It is an error to copy/move an entry into the same path.
  if (same_file_system_ && src_root_.path() == dest_root_.path()) {
    callback_.Run(base::PLATFORM_FILE_ERROR_EXISTS);
    return;
  }

  // First try to copy/move it as a file.
  CopyOrMoveFile(src_root_, dest_root_,
                 base::Bind(&CrossOperationDelegate::DidTryCopyOrMoveFile,
                            AsWeakPtr()));
}

void CrossOperationDelegate::ProcessFile(const FileSystemURL& src_url,
                                         const StatusCallback& callback) {
  CopyOrMoveFile(src_url, CreateDestURL(src_url), callback);
}

void CrossOperationDelegate::ProcessDirectory(const FileSystemURL& src_url,
                                              const StatusCallback& callback) {
  FileSystemURL dest_url = CreateDestURL(src_url);
  LocalFileSystemOperation* dest_operation = NewDestOperation(dest_url);
  if (!dest_operation) {
    return;
  }

  // If operation_type == Move we may need to record directories and
  // restore directory timestamps in the end, though it may have
  // negative performance impact.
  // See http://crbug.com/171284 for more details.
  dest_operation->CreateDirectory(
      dest_url, false /* exclusive */, false /* recursive */, callback);
}

void CrossOperationDelegate::DidTryCopyOrMoveFile(
    base::PlatformFileError error) {
  if (error == base::PLATFORM_FILE_OK ||
      error != base::PLATFORM_FILE_ERROR_NOT_A_FILE) {
    callback_.Run(error);
    return;
  }

  // The src_root_ looks to be a directory.
  // Try removing the dest_root_ to see if it exists and/or it is an
  // empty directory.
  LocalFileSystemOperation* dest_operation = NewDestOperation(dest_root_);
  if (!dest_operation)
    return;
  dest_operation->RemoveDirectory(
      dest_root_, base::Bind(&CrossOperationDelegate::DidTryRemoveDestRoot,
                             AsWeakPtr()));
}

void CrossOperationDelegate::DidTryRemoveDestRoot(
    base::PlatformFileError error) {
  if (error == base::PLATFORM_FILE_ERROR_NOT_A_DIRECTORY) {
    callback_.Run(base::PLATFORM_FILE_ERROR_INVALID_OPERATION);
    return;
  }
  if (error != base::PLATFORM_FILE_OK &&
      error != base::PLATFORM_FILE_ERROR_NOT_FOUND) {
    callback_.Run(error);
    return;
  }

  // Start to process the source directory recursively.
  // TODO(kinuko): This could be too expensive for same_file_system_==true
  // and operation==MOVE case, probably we can just rename the root directory.
  // http://crbug.com/172187
  StartRecursiveOperation(
      src_root_, base::Bind(&CrossOperationDelegate::DidFinishCopy,
                            AsWeakPtr(), src_root_, callback_));
}

void CrossOperationDelegate::CopyOrMoveFile(
    const FileSystemURL& src,
    const FileSystemURL& dest,
    const StatusCallback& callback) {
  LocalFileSystemOperation* src_operation = NewSourceOperation(src);
  if (!src_operation)
    return;

  // Same filesystem case.
  if (same_file_system_) {
    if (operation_type_ == OPERATION_MOVE)
      src_operation->MoveFileLocal(src, dest, callback);
    else
      src_operation->CopyFileLocal(src, dest, callback);
    return;
  }

  // Cross filesystem case.
  // Perform CreateSnapshotFile, CopyInForeignFile and then calls
  // copy_callback which removes the source file if operation_type == MOVE.
  StatusCallback copy_callback =
      base::Bind(&CrossOperationDelegate::DidFinishCopy, AsWeakPtr(),
                 src, callback);
  src_operation->CreateSnapshotFile(
      src, base::Bind(&CrossOperationDelegate::DidCreateSnapshot, AsWeakPtr(),
                      dest, copy_callback));
}

void CrossOperationDelegate::DidCreateSnapshot(
    const FileSystemURL& dest,
    const StatusCallback& callback,
    base::PlatformFileError error,
    const base::PlatformFileInfo& file_info,
    const base::FilePath& platform_path,
    const scoped_refptr<webkit_blob::ShareableFileReference>& file_ref) {
  if (error != base::PLATFORM_FILE_OK) {
    callback.Run(error);
    return;
  }
  current_file_ref_ = file_ref;

  // For now we assume CreateSnapshotFile always return a valid local file path.
  // TODO(kinuko): Otherwise create a FileStreamReader to perform a copy/move.
  DCHECK(!platform_path.empty());

  LocalFileSystemOperation* dest_operation = NewDestOperation(dest);
  if (!dest_operation)
    return;
  dest_operation->CopyInForeignFile(platform_path, dest, callback);
}

void CrossOperationDelegate::DidFinishCopy(
    const FileSystemURL& src,
    const StatusCallback& callback,
    base::PlatformFileError error) {
  if (error != base::PLATFORM_FILE_OK ||
      operation_type_ == OPERATION_COPY) {
    callback.Run(error);
    return;
  }

  DCHECK_EQ(OPERATION_MOVE, operation_type_);

  // Remove the source for finalizing move operation.
  LocalFileSystemOperation* src_operation = NewSourceOperation(src);
  if (!src_operation)
    return;
  src_operation->Remove(
      src, true /* recursive */,
      base::Bind(&CrossOperationDelegate::DidRemoveSourceForMove,
                 AsWeakPtr(), callback));
}

void CrossOperationDelegate::DidRemoveSourceForMove(
    const StatusCallback& callback,
    base::PlatformFileError error) {
  if (error == base::PLATFORM_FILE_ERROR_NOT_FOUND)
    error = base::PLATFORM_FILE_OK;
  callback.Run(error);
}

FileSystemURL CrossOperationDelegate::CreateDestURL(
    const FileSystemURL& src_url) const {
  DCHECK_EQ(src_root_.type(), src_url.type());
  DCHECK_EQ(src_root_.origin(), src_url.origin());

  base::FilePath relative = dest_root_.virtual_path();
  src_root_.virtual_path().AppendRelativePath(src_url.virtual_path(),
                                              &relative);
  return file_system_context()->CreateCrackedFileSystemURL(
      dest_root_.origin(),
      dest_root_.mount_type(),
      relative);
}

LocalFileSystemOperation* CrossOperationDelegate::NewSourceOperation(
    const FileSystemURL& url) {
  base::PlatformFileError error = base::PLATFORM_FILE_OK;
  LocalFileSystemOperation* operation =
      RecursiveOperationDelegate::NewOperation(url, &error);
  if (!operation) {
    DCHECK_NE(base::PLATFORM_FILE_OK, error);
    callback_.Run(error);
    return NULL;
  }
  return operation;
}

LocalFileSystemOperation* CrossOperationDelegate::NewDestOperation(
    const FileSystemURL& url) {
  if (same_file_system_)
    return NewSourceOperation(url);

  base::PlatformFileError error = base::PLATFORM_FILE_OK;
  FileSystemOperation* operation = file_system_context()->
      CreateFileSystemOperation(url, &error);
  if (!operation) {
    DCHECK_NE(base::PLATFORM_FILE_OK, error);
    callback_.Run(error);
    return NULL;
  }
  LocalFileSystemOperation* local_operation =
      operation->AsLocalFileSystemOperation();
  DCHECK(local_operation);

  // Let the new operation inherit from the root operation.
  local_operation->set_overriding_operation_context(
      dest_root_operation_->operation_context());
  return local_operation;
}

}  // namespace fileapi
