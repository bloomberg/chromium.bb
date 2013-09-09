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

class CopyOrMoveOperationDelegate::CopyOrMoveImpl {
 public:
  virtual ~CopyOrMoveImpl() {}
  virtual void Run(
      const CopyOrMoveOperationDelegate::StatusCallback& callback) = 0;
 protected:
  CopyOrMoveImpl() {}
  DISALLOW_COPY_AND_ASSIGN(CopyOrMoveImpl);
};

namespace {

// Copies a file on a (same) file system. Just delegate the operation to
// |operation_runner|.
class CopyOrMoveOnSameFileSystemImpl
    : public CopyOrMoveOperationDelegate::CopyOrMoveImpl {
 public:
  CopyOrMoveOnSameFileSystemImpl(
      FileSystemOperationRunner* operation_runner,
      CopyOrMoveOperationDelegate::OperationType operation_type,
      const FileSystemURL& src_url,
      const FileSystemURL& dest_url)
      : operation_runner_(operation_runner),
        operation_type_(operation_type),
        src_url_(src_url),
        dest_url_(dest_url) {
  }

  virtual void Run(
      const CopyOrMoveOperationDelegate::StatusCallback& callback) OVERRIDE {
    if (operation_type_ == CopyOrMoveOperationDelegate::OPERATION_MOVE) {
      operation_runner_->MoveFileLocal(src_url_, dest_url_, callback);
    } else {
      // TODO(hidehiko): Support progress callback.
      operation_runner_->CopyFileLocal(
          src_url_, dest_url_,
          FileSystemOperationRunner::CopyFileProgressCallback(), callback);
    }
  }

 private:
  FileSystemOperationRunner* operation_runner_;
  CopyOrMoveOperationDelegate::OperationType operation_type_;
  FileSystemURL src_url_;
  FileSystemURL dest_url_;
  DISALLOW_COPY_AND_ASSIGN(CopyOrMoveOnSameFileSystemImpl);
};

// Specifically for cross file system copy/move operation, this class creates
// a snapshot file, validates it if necessary, runs copying process,
// validates the created file, and removes source file for move (noop for
// copy).
class SnapshotCopyOrMoveImpl
    : public CopyOrMoveOperationDelegate::CopyOrMoveImpl {
 public:
  SnapshotCopyOrMoveImpl(
      FileSystemOperationRunner* operation_runner,
      CopyOrMoveOperationDelegate::OperationType operation_type,
      const FileSystemURL& src_url,
      const FileSystemURL& dest_url,
      CopyOrMoveFileValidatorFactory* validator_factory)
      : operation_runner_(operation_runner),
        operation_type_(operation_type),
        src_url_(src_url),
        dest_url_(dest_url),
        validator_factory_(validator_factory),
        weak_factory_(this) {
  }

  virtual void Run(
      const CopyOrMoveOperationDelegate::StatusCallback& callback) OVERRIDE {
    operation_runner_->CreateSnapshotFile(
        src_url_,
        base::Bind(&SnapshotCopyOrMoveImpl::RunAfterCreateSnapshot,
                   weak_factory_.GetWeakPtr(), callback));
  }

 private:
  void RunAfterCreateSnapshot(
      const CopyOrMoveOperationDelegate::StatusCallback& callback,
      base::PlatformFileError error,
      const base::PlatformFileInfo& file_info,
      const base::FilePath& platform_path,
      const scoped_refptr<webkit_blob::ShareableFileReference>& file_ref) {
    if (error != base::PLATFORM_FILE_OK) {
      callback.Run(error);
      return;
    }

    // For now we assume CreateSnapshotFile always return a valid local file
    // path.
    DCHECK(!platform_path.empty());

    if (!validator_factory_) {
      // No validation is needed.
      RunAfterPreWriteValidation(
          platform_path, file_ref, callback, base::PLATFORM_FILE_OK);
      return;
    }

    // Run pre write validation.
    PreWriteValidation(
        platform_path,
        base::Bind(&SnapshotCopyOrMoveImpl::RunAfterPreWriteValidation,
                   weak_factory_.GetWeakPtr(),
                   platform_path, file_ref, callback));
  }

  void RunAfterPreWriteValidation(
      const base::FilePath& platform_path,
      const scoped_refptr<webkit_blob::ShareableFileReference>& file_ref,
      const CopyOrMoveOperationDelegate::StatusCallback& callback,
      base::PlatformFileError error) {
    if (error != base::PLATFORM_FILE_OK) {
      callback.Run(error);
      return;
    }

    // |file_ref| is unused but necessary to keep the file alive until
    // CopyInForeignFile() is completed.
    operation_runner_->CopyInForeignFile(
        platform_path, dest_url_,
        base::Bind(&SnapshotCopyOrMoveImpl::RunAfterCopyInForeignFile,
                   weak_factory_.GetWeakPtr(), file_ref, callback));
  }

  void RunAfterCopyInForeignFile(
      const scoped_refptr<webkit_blob::ShareableFileReference>& file_ref,
      const CopyOrMoveOperationDelegate::StatusCallback& callback,
      base::PlatformFileError error) {
    if (error != base::PLATFORM_FILE_OK) {
      callback.Run(error);
      return;
    }

    // |validator_| is NULL when the destination filesystem does not do
    // validation.
    if (!validator_) {
      // No validation is needed.
      RunAfterPostWriteValidation(callback, base::PLATFORM_FILE_OK);
      return;
    }

    PostWriteValidation(
        base::Bind(&SnapshotCopyOrMoveImpl::RunAfterPostWriteValidation,
                   weak_factory_.GetWeakPtr(), callback));
  }

  void RunAfterPostWriteValidation(
      const CopyOrMoveOperationDelegate::StatusCallback& callback,
      base::PlatformFileError error) {
    if (error != base::PLATFORM_FILE_OK) {
      // Failed to validate. Remove the destination file.
      operation_runner_->Remove(
          dest_url_, true /* recursive */,
          base::Bind(&SnapshotCopyOrMoveImpl::DidRemoveDestForError,
                     weak_factory_.GetWeakPtr(), error, callback));
      return;
    }

    if (operation_type_ == CopyOrMoveOperationDelegate::OPERATION_COPY) {
      callback.Run(base::PLATFORM_FILE_OK);
      return;
    }

    DCHECK_EQ(CopyOrMoveOperationDelegate::OPERATION_MOVE, operation_type_);

    // Remove the source for finalizing move operation.
    operation_runner_->Remove(
        src_url_, true /* recursive */,
        base::Bind(&SnapshotCopyOrMoveImpl::RunAfterRemoveSourceForMove,
                   weak_factory_.GetWeakPtr(), callback));
  }

  void RunAfterRemoveSourceForMove(
      const CopyOrMoveOperationDelegate::StatusCallback& callback,
      base::PlatformFileError error) {
    if (error == base::PLATFORM_FILE_ERROR_NOT_FOUND)
      error = base::PLATFORM_FILE_OK;
    callback.Run(error);
  }

  void DidRemoveDestForError(
      base::PlatformFileError prior_error,
      const CopyOrMoveOperationDelegate::StatusCallback& callback,
      base::PlatformFileError error) {
    if (error != base::PLATFORM_FILE_OK) {
      VLOG(1) << "Error removing destination file after validation error: "
              << error;
    }
    callback.Run(prior_error);
  }

  // Runs pre-write validation.
  void PreWriteValidation(
      const base::FilePath& platform_path,
      const CopyOrMoveOperationDelegate::StatusCallback& callback) {
    DCHECK(validator_factory_);
    validator_.reset(
        validator_factory_->CreateCopyOrMoveFileValidator(
            src_url_, platform_path));
    validator_->StartPreWriteValidation(callback);
  }

  // Runs post-write validation.
  void PostWriteValidation(
      const CopyOrMoveOperationDelegate::StatusCallback& callback) {
    operation_runner_->CreateSnapshotFile(
        dest_url_,
        base::Bind(
            &SnapshotCopyOrMoveImpl::PostWriteValidationAfterCreateSnapshotFile,
            weak_factory_.GetWeakPtr(), callback));
  }

  void PostWriteValidationAfterCreateSnapshotFile(
      const CopyOrMoveOperationDelegate::StatusCallback& callback,
      base::PlatformFileError error,
      const base::PlatformFileInfo& file_info,
      const base::FilePath& platform_path,
      const scoped_refptr<webkit_blob::ShareableFileReference>& file_ref) {
    if (error != base::PLATFORM_FILE_OK) {
      callback.Run(error);
      return;
    }

    DCHECK(validator_);
    // Note: file_ref passed here to keep the file alive until after
    // the StartPostWriteValidation operation finishes.
    validator_->StartPostWriteValidation(
        platform_path,
        base::Bind(&SnapshotCopyOrMoveImpl::DidPostWriteValidation,
                   weak_factory_.GetWeakPtr(), file_ref, callback));
  }

  // |file_ref| is unused; it is passed here to make sure the reference is
  // alive until after post-write validation is complete.
  void DidPostWriteValidation(
      const scoped_refptr<webkit_blob::ShareableFileReference>& file_ref,
      const CopyOrMoveOperationDelegate::StatusCallback& callback,
      base::PlatformFileError error) {
    callback.Run(error);
  }

  FileSystemOperationRunner* operation_runner_;
  CopyOrMoveOperationDelegate::OperationType operation_type_;
  FileSystemURL src_url_;
  FileSystemURL dest_url_;
  CopyOrMoveFileValidatorFactory* validator_factory_;
  scoped_ptr<CopyOrMoveFileValidator> validator_;

  base::WeakPtrFactory<SnapshotCopyOrMoveImpl> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(SnapshotCopyOrMoveImpl);
};

}  // namespace


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
      callback_(callback),
      weak_factory_(this) {
  same_file_system_ = src_root_.IsInSameFileSystem(dest_root_);
}

CopyOrMoveOperationDelegate::~CopyOrMoveOperationDelegate() {
  STLDeleteElements(&running_copy_set_);
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
  CopyOrMoveFile(src_root_, dest_root_,
                 base::Bind(&CopyOrMoveOperationDelegate::DidTryCopyOrMoveFile,
                            weak_factory_.GetWeakPtr()));
}

void CopyOrMoveOperationDelegate::ProcessFile(const FileSystemURL& src_url,
                                         const StatusCallback& callback) {
  CopyOrMoveFile(src_url, CreateDestURL(src_url), callback);
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
                 weak_factory_.GetWeakPtr()));
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
      src_root_,
      base::Bind(&CopyOrMoveOperationDelegate::DidFinishRecursiveCopyDir,
                 weak_factory_.GetWeakPtr(), src_root_, callback_));
}

void CopyOrMoveOperationDelegate::DidFinishRecursiveCopyDir(
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
                 weak_factory_.GetWeakPtr(), callback));
}

void CopyOrMoveOperationDelegate::DidRemoveSourceForMove(
    const StatusCallback& callback,
    base::PlatformFileError error) {
  if (error == base::PLATFORM_FILE_ERROR_NOT_FOUND)
    error = base::PLATFORM_FILE_OK;
  callback.Run(error);
}

void CopyOrMoveOperationDelegate::CopyOrMoveFile(
    const FileSystemURL& src_url,
    const FileSystemURL& dest_url,
    const StatusCallback& callback) {
  CopyOrMoveImpl* impl = NULL;
  if (same_file_system_) {
    impl = new CopyOrMoveOnSameFileSystemImpl(
        operation_runner(), operation_type_, src_url, dest_url);
  } else {
    // Cross filesystem case.
    // TODO(hidehiko): Support stream based copy. crbug.com/279287.
    base::PlatformFileError error = base::PLATFORM_FILE_ERROR_FAILED;
    CopyOrMoveFileValidatorFactory* validator_factory =
        file_system_context()->GetCopyOrMoveFileValidatorFactory(
            dest_root_.type(), &error);
    if (error != base::PLATFORM_FILE_OK) {
      callback.Run(error);
      return;
    }

    impl = new SnapshotCopyOrMoveImpl(
        operation_runner(), operation_type_, src_url, dest_url,
        validator_factory);
  }

  // Register the running task.
  running_copy_set_.insert(impl);
  impl->Run(base::Bind(&CopyOrMoveOperationDelegate::DidCopyOrMoveFile,
                       weak_factory_.GetWeakPtr(), impl, callback));
}

void CopyOrMoveOperationDelegate::DidCopyOrMoveFile(
    CopyOrMoveImpl* impl,
    const StatusCallback& callback,
    base::PlatformFileError error) {
  running_copy_set_.erase(impl);
  delete impl;
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
