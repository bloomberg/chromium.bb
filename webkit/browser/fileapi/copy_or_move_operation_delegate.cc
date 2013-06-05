// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/browser/fileapi/copy_or_move_operation_delegate.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "webkit/browser/fileapi/copy_or_move_file_validator.h"
#include "webkit/browser/fileapi/file_system_context.h"
#include "webkit/browser/fileapi/file_system_operation_runner.h"
#include "webkit/browser/fileapi/file_system_url.h"
#include "webkit/browser/fileapi/recursive_operation_delegate.h"
#include "webkit/common/blob/shareable_file_reference.h"
#include "webkit/common/fileapi/file_system_util.h"

namespace fileapi {

CopyOrMoveOperationDelegate::CopyOrMoveOperationDelegate(
    FileSystemContext* file_system_context,
    const FileSystemURL& src_root,
    const FileSystemURL& dest_root,
    OperationType operation_type,
    const StatusCallback& callback)
    : RecursiveOperationDelegate(file_system_context),
      src_root_(src_root),
      dest_root_(dest_root),
      operation_type_(operation_type),
      callback_(callback) {
  same_file_system_ = src_root_.IsInSameFileSystem(dest_root_);
}

CopyOrMoveOperationDelegate::~CopyOrMoveOperationDelegate() {
}

void CopyOrMoveOperationDelegate::Run() {
  // Not supported; this should never be called.
  NOTREACHED();
}

void CopyOrMoveOperationDelegate::RunRecursively() {
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
  CopyOrMoveFile(URLPair(src_root_, dest_root_),
                 base::Bind(&CopyOrMoveOperationDelegate::DidTryCopyOrMoveFile,
                            AsWeakPtr()));
}

void CopyOrMoveOperationDelegate::ProcessFile(const FileSystemURL& src_url,
                                         const StatusCallback& callback) {
  CopyOrMoveFile(URLPair(src_url, CreateDestURL(src_url)), callback);
}

void CopyOrMoveOperationDelegate::ProcessDirectory(const FileSystemURL& src_url,
                                              const StatusCallback& callback) {
  FileSystemURL dest_url = CreateDestURL(src_url);

  // If operation_type == Move we may need to record directories and
  // restore directory timestamps in the end, though it may have
  // negative performance impact.
  // See http://crbug.com/171284 for more details.
  operation_runner()->CreateDirectory(
      dest_url, false /* exclusive */, false /* recursive */, callback);
}

void CopyOrMoveOperationDelegate::DidTryCopyOrMoveFile(
    base::PlatformFileError error) {
  if (error == base::PLATFORM_FILE_OK ||
      error != base::PLATFORM_FILE_ERROR_NOT_A_FILE) {
    callback_.Run(error);
    return;
  }

  // The src_root_ looks to be a directory.
  // Try removing the dest_root_ to see if it exists and/or it is an
  // empty directory.
  operation_runner()->RemoveDirectory(
      dest_root_,
      base::Bind(&CopyOrMoveOperationDelegate::DidTryRemoveDestRoot,
                 AsWeakPtr()));
}

void CopyOrMoveOperationDelegate::DidTryRemoveDestRoot(
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
      src_root_, base::Bind(&CopyOrMoveOperationDelegate::DidFinishCopy,
                            AsWeakPtr(), src_root_, callback_));
}

void CopyOrMoveOperationDelegate::CopyOrMoveFile(const URLPair& url_pair,
                                            const StatusCallback& callback) {
  // Same filesystem case.
  if (same_file_system_) {
    if (operation_type_ == OPERATION_MOVE) {
      operation_runner()->MoveFileLocal(url_pair.src, url_pair.dest, callback);
    } else {
      operation_runner()->CopyFileLocal(url_pair.src, url_pair.dest, callback);
    }
    return;
  }

  // Cross filesystem case.
  // Perform CreateSnapshotFile, CopyInForeignFile and then calls
  // copy_callback which removes the source file if operation_type == MOVE.
  StatusCallback copy_callback =
      base::Bind(&CopyOrMoveOperationDelegate::DidFinishCopy, AsWeakPtr(),
                 url_pair.src, callback);
  operation_runner()->CreateSnapshotFile(
      url_pair.src,
      base::Bind(&CopyOrMoveOperationDelegate::DidCreateSnapshot, AsWeakPtr(),
                 url_pair, copy_callback));
}

void CopyOrMoveOperationDelegate::DidCreateSnapshot(
    const URLPair& url_pair,
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

  CopyOrMoveFileValidatorFactory* factory =
      file_system_context()->GetCopyOrMoveFileValidatorFactory(
          dest_root_.type(), &error);
  if (error != base::PLATFORM_FILE_OK) {
    callback.Run(error);
    return;
  }
  if (!factory) {
    DidValidateFile(url_pair.dest, callback, file_info, platform_path, error);
    return;
  }

  validator_.reset(
      factory->CreateCopyOrMoveFileValidator(url_pair.src, platform_path));
  validator_->StartValidation(
      base::Bind(&CopyOrMoveOperationDelegate::DidValidateFile, AsWeakPtr(),
                 url_pair.dest, callback, file_info, platform_path));
}

void CopyOrMoveOperationDelegate::DidValidateFile(
    const FileSystemURL& dest,
    const StatusCallback& callback,
    const base::PlatformFileInfo& file_info,
    const base::FilePath& platform_path,
    base::PlatformFileError error) {
  if (error != base::PLATFORM_FILE_OK) {
    callback.Run(error);
    return;
  }

  operation_runner()->CopyInForeignFile(platform_path, dest, callback);
}

void CopyOrMoveOperationDelegate::DidFinishCopy(
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
  operation_runner()->Remove(
      src, true /* recursive */,
      base::Bind(&CopyOrMoveOperationDelegate::DidRemoveSourceForMove,
                 AsWeakPtr(), callback));
}

void CopyOrMoveOperationDelegate::DidRemoveSourceForMove(
    const StatusCallback& callback,
    base::PlatformFileError error) {
  if (error == base::PLATFORM_FILE_ERROR_NOT_FOUND)
    error = base::PLATFORM_FILE_OK;
  callback.Run(error);
}

FileSystemURL CopyOrMoveOperationDelegate::CreateDestURL(
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

}  // namespace fileapi
