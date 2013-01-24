// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/syncable/syncable_file_system_operation.h"

#include "base/logging.h"
#include "webkit/blob/shareable_file_reference.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_url.h"
#include "webkit/fileapi/local_file_system_operation.h"
#include "webkit/fileapi/sandbox_mount_point_provider.h"
#include "webkit/fileapi/syncable/local_file_sync_context.h"
#include "webkit/fileapi/syncable/syncable_file_operation_runner.h"

namespace fileapi {

namespace {

void WriteCallbackAdapter(
    const SyncableFileSystemOperation::WriteCallback& callback,
    base::PlatformFileError status) {
  callback.Run(status, 0, true);
}

}  // namespace

class SyncableFileSystemOperation::QueueableTask
    : public SyncableFileOperationRunner::Task {
 public:
  QueueableTask(SyncableFileSystemOperation* operation,
                const base::Closure& task)
      : operation_(operation), task_(task) {}

  virtual ~QueueableTask() {
    DCHECK(!operation_);
  }

  virtual void Run() OVERRIDE {
    DCHECK(!task_.is_null());
    task_.Run();
    operation_ = NULL;
  }

  virtual void Cancel() OVERRIDE {
    DCHECK(!task_.is_null());
    DCHECK(operation_);
    operation_->OnCancelled();
    task_.Reset();  // This will delete operation_.
    operation_ = NULL;
  }

  virtual std::vector<FileSystemURL>& target_paths() const OVERRIDE {
    DCHECK(operation_);
    return operation_->target_paths_;
  }

 private:
  SyncableFileSystemOperation* operation_;
  base::Closure task_;
  DISALLOW_COPY_AND_ASSIGN(QueueableTask);
};

SyncableFileSystemOperation::~SyncableFileSystemOperation() {}

void SyncableFileSystemOperation::CreateFile(
    const FileSystemURL& url,
    bool exclusive,
    const StatusCallback& callback) {
  DCHECK(CalledOnValidThread());
  if (!operation_runner_) {
    AbortOperation(callback, base::PLATFORM_FILE_ERROR_NOT_FOUND);
    return;
  }
  DCHECK(operation_runner_.get());
  target_paths_.push_back(url);
  completion_callback_ = callback;
  scoped_ptr<SyncableFileOperationRunner::Task> task(new QueueableTask(
      this,
      base::Bind(&FileSystemOperation::CreateFile,
                 base::Unretained(file_system_operation_),
                 url, exclusive,
                 base::Bind(&self::DidFinish, base::Owned(this)))));
  operation_runner_->PostOperationTask(task.Pass());
}

void SyncableFileSystemOperation::CreateDirectory(
    const FileSystemURL& url,
    bool exclusive,
    bool recursive,
    const StatusCallback& callback) {
  DCHECK(CalledOnValidThread());
  if (!operation_runner_) {
    AbortOperation(callback, base::PLATFORM_FILE_ERROR_NOT_FOUND);
    return;
  }
  if (!is_directory_operation_enabled_) {
    AbortOperation(callback, base::PLATFORM_FILE_ERROR_INVALID_OPERATION);
    return;
  }
  DCHECK(operation_runner_.get());
  target_paths_.push_back(url);
  completion_callback_ = callback;
  scoped_ptr<SyncableFileOperationRunner::Task> task(new QueueableTask(
      this,
      base::Bind(&FileSystemOperation::CreateDirectory,
                 base::Unretained(file_system_operation_),
                 url, exclusive, recursive,
                 base::Bind(&self::DidFinish, base::Owned(this)))));
  operation_runner_->PostOperationTask(task.Pass());
}

void SyncableFileSystemOperation::Copy(
    const FileSystemURL& src_url,
    const FileSystemURL& dest_url,
    const StatusCallback& callback) {
  DCHECK(CalledOnValidThread());
  if (!operation_runner_) {
    AbortOperation(callback, base::PLATFORM_FILE_ERROR_NOT_FOUND);
    return;
  }
  DCHECK(operation_runner_.get());
  target_paths_.push_back(dest_url);
  completion_callback_ = callback;
  scoped_ptr<SyncableFileOperationRunner::Task> task(new QueueableTask(
      this,
      base::Bind(&FileSystemOperation::Copy,
                 base::Unretained(file_system_operation_),
                 src_url, dest_url,
                 base::Bind(&self::DidFinish, base::Owned(this)))));
  operation_runner_->PostOperationTask(task.Pass());
}

void SyncableFileSystemOperation::Move(
    const FileSystemURL& src_url,
    const FileSystemURL& dest_url,
    const StatusCallback& callback) {
  DCHECK(CalledOnValidThread());
  if (!operation_runner_) {
    AbortOperation(callback, base::PLATFORM_FILE_ERROR_NOT_FOUND);
    return;
  }
  DCHECK(operation_runner_.get());
  target_paths_.push_back(src_url);
  target_paths_.push_back(dest_url);
  completion_callback_ = callback;
  scoped_ptr<SyncableFileOperationRunner::Task> task(new QueueableTask(
      this,
      base::Bind(&FileSystemOperation::Move,
                 base::Unretained(file_system_operation_),
                 src_url, dest_url,
                 base::Bind(&self::DidFinish, base::Owned(this)))));
  operation_runner_->PostOperationTask(task.Pass());
}

void SyncableFileSystemOperation::DirectoryExists(
    const FileSystemURL& url,
    const StatusCallback& callback) {
  DCHECK(CalledOnValidThread());
  if (!operation_runner_) {
    AbortOperation(callback, base::PLATFORM_FILE_ERROR_NOT_FOUND);
    return;
  }
  file_system_operation_->DirectoryExists(url, callback);
  delete this;
}

void SyncableFileSystemOperation::FileExists(
    const FileSystemURL& url,
    const StatusCallback& callback) {
  DCHECK(CalledOnValidThread());
  if (!operation_runner_) {
    AbortOperation(callback, base::PLATFORM_FILE_ERROR_NOT_FOUND);
    return;
  }
  file_system_operation_->FileExists(url, callback);
  delete this;
}

void SyncableFileSystemOperation::GetMetadata(
    const FileSystemURL& url,
    const GetMetadataCallback& callback) {
  DCHECK(CalledOnValidThread());
  if (!operation_runner_) {
    callback.Run(base::PLATFORM_FILE_ERROR_NOT_FOUND,
                 base::PlatformFileInfo(), FilePath());
    delete file_system_operation_;
    delete this;
    return;
  }
  file_system_operation_->GetMetadata(url, callback);
  delete this;
}

void SyncableFileSystemOperation::ReadDirectory(
    const FileSystemURL& url,
    const ReadDirectoryCallback& callback) {
  DCHECK(CalledOnValidThread());
  if (!operation_runner_) {
    callback.Run(base::PLATFORM_FILE_ERROR_NOT_FOUND, FileEntryList(), false);
    delete file_system_operation_;
    delete this;
    return;
  }
  // This is a read operation and there'd be no hard to let it go even if
  // directory operation is disabled. (And we should allow this if it's made
  // on the root directory)
  file_system_operation_->ReadDirectory(url, callback);
  delete this;
}

void SyncableFileSystemOperation::Remove(
    const FileSystemURL& url, bool recursive,
    const StatusCallback& callback) {
  DCHECK(CalledOnValidThread());
  if (!operation_runner_) {
    AbortOperation(callback, base::PLATFORM_FILE_ERROR_NOT_FOUND);
    return;
  }
  DCHECK(operation_runner_.get());
  target_paths_.push_back(url);
  completion_callback_ = callback;
  scoped_ptr<SyncableFileOperationRunner::Task> task(new QueueableTask(
      this,
      base::Bind(&FileSystemOperation::Remove,
                 base::Unretained(file_system_operation_),
                 url, recursive,
                 base::Bind(&self::DidFinish, base::Owned(this)))));
  operation_runner_->PostOperationTask(task.Pass());
}

void SyncableFileSystemOperation::Write(
    const net::URLRequestContext* url_request_context,
    const FileSystemURL& url,
    const GURL& blob_url,
    int64 offset,
    const WriteCallback& callback) {
  DCHECK(CalledOnValidThread());
  if (!operation_runner_) {
    callback.Run(base::PLATFORM_FILE_ERROR_NOT_FOUND, 0, true);
    delete file_system_operation_;
    delete this;
    return;
  }
  DCHECK(operation_runner_.get());
  target_paths_.push_back(url);
  completion_callback_ = base::Bind(&WriteCallbackAdapter, callback);
  scoped_ptr<SyncableFileOperationRunner::Task> task(new QueueableTask(
      this,
      file_system_operation_->GetWriteClosure(
          url_request_context, url, blob_url, offset,
          base::Bind(&self::DidWrite, base::Owned(this), callback))));
  operation_runner_->PostOperationTask(task.Pass());
}

void SyncableFileSystemOperation::Truncate(
    const FileSystemURL& url, int64 length,
    const StatusCallback& callback) {
  DCHECK(CalledOnValidThread());
  if (!operation_runner_) {
    AbortOperation(callback, base::PLATFORM_FILE_ERROR_NOT_FOUND);
    return;
  }
  DCHECK(operation_runner_.get());
  target_paths_.push_back(url);
  completion_callback_ = callback;
  scoped_ptr<SyncableFileOperationRunner::Task> task(new QueueableTask(
      this,
      base::Bind(&FileSystemOperation::Truncate,
                 base::Unretained(file_system_operation_),
                 url, length,
                 base::Bind(&self::DidFinish, base::Owned(this)))));
  operation_runner_->PostOperationTask(task.Pass());
}

void SyncableFileSystemOperation::TouchFile(
    const FileSystemURL& url,
    const base::Time& last_access_time,
    const base::Time& last_modified_time,
    const StatusCallback& callback) {
  DCHECK(CalledOnValidThread());
  if (!operation_runner_) {
    AbortOperation(callback, base::PLATFORM_FILE_ERROR_NOT_FOUND);
    return;
  }
  file_system_operation_->TouchFile(url, last_access_time,
                                    last_modified_time, callback);
  delete this;
}

void SyncableFileSystemOperation::OpenFile(
    const FileSystemURL& url,
    int file_flags,
    base::ProcessHandle peer_handle,
    const OpenFileCallback& callback) {
  NOTREACHED();
  delete this;
}

void SyncableFileSystemOperation::NotifyCloseFile(
    const FileSystemURL& url) {
  NOTREACHED();
  delete this;
}

void SyncableFileSystemOperation::Cancel(
    const StatusCallback& cancel_callback) {
  DCHECK(CalledOnValidThread());
  DCHECK(file_system_operation_);
  completion_callback_ = cancel_callback;
  file_system_operation_->Cancel(
      base::Bind(&self::DidFinish, base::Owned(this)));
}

LocalFileSystemOperation*
SyncableFileSystemOperation::AsLocalFileSystemOperation() {
  return file_system_operation_;
}

void SyncableFileSystemOperation::CreateSnapshotFile(
    const FileSystemURL& path,
    const SnapshotFileCallback& callback) {
  DCHECK(CalledOnValidThread());
  if (!operation_runner_) {
    callback.Run(base::PLATFORM_FILE_ERROR_NOT_FOUND,
                 base::PlatformFileInfo(), FilePath(), NULL);
    delete file_system_operation_;
    delete this;
    return;
  }
  file_system_operation_->CreateSnapshotFile(path, callback);
  delete this;
}

SyncableFileSystemOperation::SyncableFileSystemOperation(
    FileSystemContext* file_system_context,
    FileSystemOperation* file_system_operation) {
  DCHECK(file_system_context);
  DCHECK(file_system_operation);
  file_system_operation_ = file_system_operation->AsLocalFileSystemOperation();
  DCHECK(file_system_operation_);
  if (!file_system_context->sync_context()) {
    // Syncable FileSystem is opened in a file system context which doesn't
    // support (or is not initialized for) the API.
    // Returning here to leave operation_runner_ as NULL.
    return;
  }
  operation_runner_ = file_system_context->sync_context()->operation_runner();
  is_directory_operation_enabled_ = file_system_context->sandbox_provider()->
      is_sync_directory_operation_enabled();
}

void SyncableFileSystemOperation::DidFinish(base::PlatformFileError status) {
  DCHECK(CalledOnValidThread());
  DCHECK(!completion_callback_.is_null());
  if (operation_runner_.get())
    operation_runner_->OnOperationCompleted(target_paths_);
  completion_callback_.Run(status);
}

void SyncableFileSystemOperation::DidWrite(
    const WriteCallback& callback,
    base::PlatformFileError result,
    int64 bytes,
    bool complete) {
  DCHECK(CalledOnValidThread());
  if (!complete) {
    callback.Run(result, bytes, complete);
    return;
  }
  if (operation_runner_.get())
    operation_runner_->OnOperationCompleted(target_paths_);
  callback.Run(result, bytes, complete);
}

void SyncableFileSystemOperation::OnCancelled() {
  DCHECK(!completion_callback_.is_null());
  completion_callback_.Run(base::PLATFORM_FILE_ERROR_ABORT);
  delete file_system_operation_;
}

void SyncableFileSystemOperation::AbortOperation(
    const StatusCallback& callback,
    base::PlatformFileError error) {
  callback.Run(error);
  delete file_system_operation_;
  delete this;
}

}  // namespace fileapi
