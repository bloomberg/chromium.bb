// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/local_file_system_operation.h"

#include "base/bind.h"
#include "base/single_thread_task_runner.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "net/base/escape.h"
#include "net/url_request/url_request_context.h"
#include "webkit/blob/shareable_file_reference.h"
#include "webkit/fileapi/file_observers.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_file_util_proxy.h"
#include "webkit/fileapi/file_system_mount_point_provider.h"
#include "webkit/fileapi/file_system_task_runners.h"
#include "webkit/fileapi/file_system_types.h"
#include "webkit/fileapi/file_system_url.h"
#include "webkit/fileapi/file_system_util.h"
#include "webkit/fileapi/file_writer_delegate.h"
#include "webkit/fileapi/sandbox_file_stream_writer.h"
#include "webkit/quota/quota_manager.h"
#include "webkit/quota/quota_types.h"

using webkit_blob::ShareableFileReference;

namespace fileapi {

namespace {

bool IsMediaFileSystemType(FileSystemType type) {
  return type == kFileSystemTypeNativeMedia ||
      type == kFileSystemTypeDeviceMedia;
}

bool IsCrossOperationAllowed(FileSystemType src_type,
                             FileSystemType dest_type) {
  // If two types are supposed to run on different task runners we should not
  // allow cross FileUtil operations at this layer.
  return IsMediaFileSystemType(src_type) == IsMediaFileSystemType(dest_type);
}

}  // namespace

class LocalFileSystemOperation::ScopedUpdateNotifier {
 public:
  ScopedUpdateNotifier(FileSystemOperationContext* operation_context,
                       const FileSystemURL& url);
  ~ScopedUpdateNotifier();

 private:
  // Not owned; owned by the owner of this instance
  // (i.e. LocalFileSystemOperation).
  FileSystemOperationContext* operation_context_;
  FileSystemURL url_;
  DISALLOW_COPY_AND_ASSIGN(ScopedUpdateNotifier);
};

LocalFileSystemOperation::ScopedUpdateNotifier::ScopedUpdateNotifier(
    FileSystemOperationContext* operation_context,
    const FileSystemURL& url)
    : operation_context_(operation_context), url_(url) {
  operation_context_->update_observers()->Notify(
      &FileUpdateObserver::OnStartUpdate, MakeTuple(url_));
}

LocalFileSystemOperation::ScopedUpdateNotifier::~ScopedUpdateNotifier() {
  operation_context_->update_observers()->Notify(
      &FileUpdateObserver::OnEndUpdate, MakeTuple(url_));
}

LocalFileSystemOperation::~LocalFileSystemOperation() {
}

void LocalFileSystemOperation::CreateFile(const FileSystemURL& url,
                                          bool exclusive,
                                          const StatusCallback& callback) {
  DCHECK(SetPendingOperationType(kOperationCreateFile));

  base::PlatformFileError result = SetUp(url, &src_util_, SETUP_FOR_CREATE);
  if (result != base::PLATFORM_FILE_OK) {
    callback.Run(result);
    delete this;
    return;
  }

  GetUsageAndQuotaThenRunTask(
      url,
      base::Bind(&LocalFileSystemOperation::DoCreateFile,
                 base::Unretained(this), url, callback, exclusive),
      base::Bind(callback, base::PLATFORM_FILE_ERROR_FAILED));
}

void LocalFileSystemOperation::CreateDirectory(const FileSystemURL& url,
                                               bool exclusive,
                                               bool recursive,
                                               const StatusCallback& callback) {
  DCHECK(SetPendingOperationType(kOperationCreateDirectory));

  base::PlatformFileError result = SetUp(url, &src_util_, SETUP_FOR_CREATE);
  if (result != base::PLATFORM_FILE_OK) {
    callback.Run(result);
    delete this;
    return;
  }
  GetUsageAndQuotaThenRunTask(
      url,
      base::Bind(&LocalFileSystemOperation::DoCreateDirectory,
                 base::Unretained(this), url, callback, exclusive, recursive),
      base::Bind(callback, base::PLATFORM_FILE_ERROR_FAILED));
}

void LocalFileSystemOperation::Copy(const FileSystemURL& src_url,
                                    const FileSystemURL& dest_url,
                                    const StatusCallback& callback) {
  DCHECK(SetPendingOperationType(kOperationCopy));
  is_cross_operation_ = (src_url.type() != dest_url.type());

  base::PlatformFileError result = SetUp(src_url, &src_util_, SETUP_FOR_READ);
  if (result == base::PLATFORM_FILE_OK)
    result = SetUp(dest_url, &dest_util_, SETUP_FOR_CREATE);
  if (result == base::PLATFORM_FILE_OK) {
    if (!IsCrossOperationAllowed(src_url.type(), dest_url.type()))
      result = base::PLATFORM_FILE_ERROR_INVALID_OPERATION;
  }
  if (result != base::PLATFORM_FILE_OK) {
    callback.Run(result);
    delete this;
    return;
  }

  GetUsageAndQuotaThenRunTask(
      dest_url,
      base::Bind(&LocalFileSystemOperation::DoCopy,
                 base::Unretained(this), src_url, dest_url, callback),
      base::Bind(callback, base::PLATFORM_FILE_ERROR_FAILED));
}

void LocalFileSystemOperation::Move(const FileSystemURL& src_url,
                                    const FileSystemURL& dest_url,
                                    const StatusCallback& callback) {
  DCHECK(SetPendingOperationType(kOperationMove));
  is_cross_operation_ = (src_url.type() != dest_url.type());

  scoped_ptr<LocalFileSystemOperation> deleter(this);

  // Temporarily disables cross-filesystem move.
  // TODO(kinuko,tzik,kinaba): This special handling must be removed once
  // we support saner cross-filesystem operation.
  // (See http://crbug.com/130055)
  if (is_cross_operation_) {
    callback.Run(base::PLATFORM_FILE_ERROR_INVALID_OPERATION);
    return;
  }

  base::PlatformFileError result = SetUp(src_url, &src_util_, SETUP_FOR_WRITE);
  if (result == base::PLATFORM_FILE_OK)
    result = SetUp(dest_url, &dest_util_, SETUP_FOR_CREATE);
  if (result == base::PLATFORM_FILE_OK) {
    if (!IsCrossOperationAllowed(src_url.type(), dest_url.type()))
      result = base::PLATFORM_FILE_ERROR_INVALID_OPERATION;
  }
  if (result != base::PLATFORM_FILE_OK) {
    callback.Run(result);
    return;
  }

  GetUsageAndQuotaThenRunTask(
      dest_url,
      base::Bind(&LocalFileSystemOperation::DoMove,
                 base::Unretained(deleter.release()),
                 src_url, dest_url, callback),
      base::Bind(callback, base::PLATFORM_FILE_ERROR_FAILED));
}

void LocalFileSystemOperation::DirectoryExists(const FileSystemURL& url,
                                               const StatusCallback& callback) {
  DCHECK(SetPendingOperationType(kOperationDirectoryExists));

  base::PlatformFileError result = SetUp(url, &src_util_, SETUP_FOR_READ);
  if (result != base::PLATFORM_FILE_OK) {
    callback.Run(result);
    delete this;
    return;
  }

  FileSystemFileUtilProxy::GetFileInfo(
      operation_context_.get(), src_util_, url,
      base::Bind(&LocalFileSystemOperation::DidDirectoryExists,
                 base::Owned(this), callback));
}

void LocalFileSystemOperation::FileExists(const FileSystemURL& url,
                                          const StatusCallback& callback) {
  DCHECK(SetPendingOperationType(kOperationFileExists));

  base::PlatformFileError result = SetUp(url, &src_util_, SETUP_FOR_READ);
  if (result != base::PLATFORM_FILE_OK) {
    callback.Run(result);
    delete this;
    return;
  }

  FileSystemFileUtilProxy::GetFileInfo(
      operation_context_.get(), src_util_, url,
      base::Bind(&LocalFileSystemOperation::DidFileExists,
                 base::Owned(this), callback));
}

void LocalFileSystemOperation::GetMetadata(
    const FileSystemURL& url, const GetMetadataCallback& callback) {
  DCHECK(SetPendingOperationType(kOperationGetMetadata));

  base::PlatformFileError result = SetUp(url, &src_util_, SETUP_FOR_READ);
  if (result != base::PLATFORM_FILE_OK) {
    callback.Run(result, base::PlatformFileInfo(), FilePath());
    delete this;
    return;
  }

  FileSystemFileUtilProxy::GetFileInfo(
      operation_context_.get(), src_util_, url,
      base::Bind(&LocalFileSystemOperation::DidGetMetadata,
                 base::Owned(this), callback));
}

void LocalFileSystemOperation::ReadDirectory(
    const FileSystemURL& url, const ReadDirectoryCallback& callback) {
  DCHECK(SetPendingOperationType(kOperationReadDirectory));

  base::PlatformFileError result = SetUp(url, &src_util_, SETUP_FOR_READ);
  if (result != base::PLATFORM_FILE_OK) {
    callback.Run(result, std::vector<base::FileUtilProxy::Entry>(), false);
    delete this;
    return;
  }

  FileSystemFileUtilProxy::ReadDirectory(
      operation_context_.get(), src_util_, url,
      base::Bind(&LocalFileSystemOperation::DidReadDirectory,
                 base::Owned(this), callback));
}

void LocalFileSystemOperation::Remove(const FileSystemURL& url,
                                      bool recursive,
                                      const StatusCallback& callback) {
  DCHECK(SetPendingOperationType(kOperationRemove));

  base::PlatformFileError result = SetUp(url, &src_util_, SETUP_FOR_WRITE);
  if (result != base::PLATFORM_FILE_OK) {
    callback.Run(result);
    delete this;
    return;
  }

  FileSystemFileUtilProxy::Delete(
      operation_context_.get(), src_util_, url, recursive,
      base::Bind(&LocalFileSystemOperation::DidFinishFileOperation,
                 base::Owned(this), callback));
}

void LocalFileSystemOperation::Write(
    const net::URLRequestContext* url_request_context,
    const FileSystemURL& url,
    const GURL& blob_url,
    int64 offset,
    const WriteCallback& callback) {
  GetWriteClosure(url_request_context, url, blob_url, offset, callback).Run();
}

void LocalFileSystemOperation::Truncate(const FileSystemURL& url, int64 length,
                                        const StatusCallback& callback) {
  DCHECK(SetPendingOperationType(kOperationTruncate));

  base::PlatformFileError result = SetUp(url, &src_util_, SETUP_FOR_WRITE);
  if (result != base::PLATFORM_FILE_OK) {
    callback.Run(result);
    delete this;
    return;
  }
  GetUsageAndQuotaThenRunTask(
      url,
      base::Bind(&LocalFileSystemOperation::DoTruncate,
                 base::Unretained(this), url, callback, length),
      base::Bind(callback, base::PLATFORM_FILE_ERROR_FAILED));
}

void LocalFileSystemOperation::TouchFile(const FileSystemURL& url,
                                         const base::Time& last_access_time,
                                         const base::Time& last_modified_time,
                                         const StatusCallback& callback) {
  DCHECK(SetPendingOperationType(kOperationTouchFile));

  base::PlatformFileError result = SetUp(url, &src_util_, SETUP_FOR_WRITE);
  if (result != base::PLATFORM_FILE_OK) {
    callback.Run(result);
    delete this;
    return;
  }

  FileSystemFileUtilProxy::Touch(
      operation_context_.get(), src_util_, url,
      last_access_time, last_modified_time,
      base::Bind(&LocalFileSystemOperation::DidTouchFile,
                 base::Owned(this), callback));
}

void LocalFileSystemOperation::OpenFile(const FileSystemURL& url,
                                        int file_flags,
                                        base::ProcessHandle peer_handle,
                                        const OpenFileCallback& callback) {
  DCHECK(SetPendingOperationType(kOperationOpenFile));
  scoped_ptr<LocalFileSystemOperation> deleter(this);

  peer_handle_ = peer_handle;

  if (file_flags & (
      (base::PLATFORM_FILE_ENUMERATE | base::PLATFORM_FILE_TEMPORARY |
       base::PLATFORM_FILE_HIDDEN))) {
    callback.Run(base::PLATFORM_FILE_ERROR_FAILED,
                 base::PlatformFile(), base::ProcessHandle());
    return;
  }
  if (file_flags &
      (base::PLATFORM_FILE_CREATE | base::PLATFORM_FILE_OPEN_ALWAYS |
       base::PLATFORM_FILE_CREATE_ALWAYS | base::PLATFORM_FILE_OPEN_TRUNCATED |
       base::PLATFORM_FILE_WRITE | base::PLATFORM_FILE_EXCLUSIVE_WRITE |
       base::PLATFORM_FILE_DELETE_ON_CLOSE |
       base::PLATFORM_FILE_WRITE_ATTRIBUTES)) {
    base::PlatformFileError result = SetUp(url, &src_util_, SETUP_FOR_CREATE);
    if (result != base::PLATFORM_FILE_OK) {
      callback.Run(result, base::PlatformFile(), base::ProcessHandle());
      return;
    }
  } else {
    base::PlatformFileError result = SetUp(url, &src_util_, SETUP_FOR_READ);
    if (result != base::PLATFORM_FILE_OK) {
      callback.Run(result, base::PlatformFile(), base::ProcessHandle());
      return;
    }
  }
  GetUsageAndQuotaThenRunTask(
      url,
      base::Bind(&LocalFileSystemOperation::DoOpenFile,
                 base::Unretained(deleter.release()),
                 url, callback, file_flags),
      base::Bind(callback, base::PLATFORM_FILE_ERROR_FAILED,
                 base::kInvalidPlatformFileValue,
                 base::kNullProcessHandle));
}

void LocalFileSystemOperation::NotifyCloseFile(const FileSystemURL& url) {
  // No particular task to do. This method is for remote file systems that
  // need synchronization with remote server.
  delete this;
}

// We can only get here on a write or truncate that's not yet completed.
// We don't support cancelling any other operation at this time.
void LocalFileSystemOperation::Cancel(const StatusCallback& cancel_callback) {
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

LocalFileSystemOperation*
LocalFileSystemOperation::AsLocalFileSystemOperation() {
  return this;
}

void LocalFileSystemOperation::SyncGetPlatformPath(const FileSystemURL& url,
                                                   FilePath* platform_path) {
  DCHECK(SetPendingOperationType(kOperationGetLocalPath));

  base::PlatformFileError result = SetUp(url, &src_util_, SETUP_FOR_READ);
  if (result != base::PLATFORM_FILE_OK) {
    delete this;
    return;
  }

  src_util_->GetLocalFilePath(operation_context_.get(), url, platform_path);

  delete this;
}

void LocalFileSystemOperation::CreateSnapshotFile(
    const FileSystemURL& url,
    const SnapshotFileCallback& callback) {
  DCHECK(SetPendingOperationType(kOperationCreateSnapshotFile));

  base::PlatformFileError result = SetUp(url, &src_util_, SETUP_FOR_READ);
  if (result != base::PLATFORM_FILE_OK) {
    callback.Run(result, base::PlatformFileInfo(), FilePath(), NULL);
    delete this;
    return;
  }

  FileSystemFileUtilProxy::CreateSnapshotFile(
      operation_context_.get(), src_util_, url,
      base::Bind(&LocalFileSystemOperation::DidCreateSnapshotFile,
                 base::Owned(this), callback));
}

void LocalFileSystemOperation::CopyInForeignFile(
    const FilePath& src_local_disk_file_path,
    const FileSystemURL& dest_url,
    const StatusCallback& callback) {
  DCHECK(SetPendingOperationType(kOperationCopyInForeignFile));

  base::PlatformFileError result = SetUp(
      dest_url, &dest_util_, SETUP_FOR_CREATE);
  if (result != base::PLATFORM_FILE_OK) {
    callback.Run(result);
    delete this;
    return;
  }

  GetUsageAndQuotaThenRunTask(
      dest_url,
      base::Bind(&LocalFileSystemOperation::DoCopyInForeignFile,
                 base::Unretained(this), src_local_disk_file_path, dest_url,
                 callback),
      base::Bind(callback, base::PLATFORM_FILE_ERROR_FAILED));
}

LocalFileSystemOperation::LocalFileSystemOperation(
    FileSystemContext* file_system_context,
    scoped_ptr<FileSystemOperationContext> operation_context)
    : file_system_context_(file_system_context),
      operation_context_(operation_context.Pass()),
      src_util_(NULL),
      dest_util_(NULL),
      is_cross_operation_(false),
      peer_handle_(base::kNullProcessHandle),
      pending_operation_(kOperationNone),
      weak_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
  DCHECK(operation_context_.get());
}

void LocalFileSystemOperation::GetUsageAndQuotaThenRunTask(
    const FileSystemURL& url,
    const base::Closure& task,
    const base::Closure& error_callback) {
  quota::QuotaManagerProxy* quota_manager_proxy =
      file_system_context()->quota_manager_proxy();
  if (!quota_manager_proxy ||
      !file_system_context()->GetQuotaUtil(url.type())) {
    // If we don't have the quota manager or the requested filesystem type
    // does not support quota, we should be able to let it go.
    operation_context_->set_allowed_bytes_growth(kint64max);
    task.Run();
    return;
  }

  DCHECK(quota_manager_proxy);
  DCHECK(quota_manager_proxy->quota_manager());
  quota_manager_proxy->quota_manager()->GetUsageAndQuota(
      url.origin(),
      FileSystemTypeToQuotaStorageType(url.type()),
      base::Bind(&LocalFileSystemOperation::DidGetUsageAndQuotaAndRunTask,
                 weak_factory_.GetWeakPtr(), task, error_callback));
}

void LocalFileSystemOperation::DidGetUsageAndQuotaAndRunTask(
    const base::Closure& task,
    const base::Closure& error_callback,
    quota::QuotaStatusCode status,
    int64 usage, int64 quota) {
  if (status != quota::kQuotaStatusOk) {
    LOG(WARNING) << "Got unexpected quota error : " << status;
    error_callback.Run();
    return;
  }

  operation_context_->set_allowed_bytes_growth(quota - usage);
  task.Run();
}

base::Closure LocalFileSystemOperation::GetWriteClosure(
    const net::URLRequestContext* url_request_context,
    const FileSystemURL& url,
    const GURL& blob_url,
    int64 offset,
    const WriteCallback& callback) {
  DCHECK(SetPendingOperationType(kOperationWrite));

  base::PlatformFileError result = SetUp(url, &src_util_, SETUP_FOR_WRITE);
  if (result != base::PLATFORM_FILE_OK) {
    return base::Bind(&LocalFileSystemOperation::DidFailWrite,
                      base::Owned(this), callback, result);
  }

  FileSystemMountPointProvider* provider = file_system_context()->
      GetMountPointProvider(url.type());
  DCHECK(provider);
  scoped_ptr<FileStreamWriter> writer(provider->CreateFileStreamWriter(
          url, offset, file_system_context()));

  if (!writer.get()) {
    // Write is not supported.
    return base::Bind(&LocalFileSystemOperation::DidFailWrite,
                      base::Owned(this), callback,
                      base::PLATFORM_FILE_ERROR_SECURITY);
  }

  DCHECK(blob_url.is_valid());
  file_writer_delegate_.reset(new FileWriterDelegate(
      base::Bind(&LocalFileSystemOperation::DidWrite,
                 weak_factory_.GetWeakPtr(), url),
      writer.Pass()));

  write_callback_ = callback;
  scoped_ptr<net::URLRequest> blob_request(url_request_context->CreateRequest(
      blob_url, file_writer_delegate_.get()));

  return base::Bind(&FileWriterDelegate::Start,
                    base::Unretained(file_writer_delegate_.get()),
                    base::Passed(&blob_request));
}

void LocalFileSystemOperation::DidFailWrite(
    const WriteCallback& callback,
    base::PlatformFileError result) {
  callback.Run(result, 0, false);
}

void LocalFileSystemOperation::DoCreateFile(
    const FileSystemURL& url,
    const StatusCallback& callback,
    bool exclusive) {
  FileSystemFileUtilProxy::EnsureFileExists(
      operation_context_.get(),
      src_util_, url,
      base::Bind(
          exclusive ?
              &LocalFileSystemOperation::DidEnsureFileExistsExclusive :
              &LocalFileSystemOperation::DidEnsureFileExistsNonExclusive,
          base::Owned(this), callback));
}

void LocalFileSystemOperation::DoCreateDirectory(
    const FileSystemURL& url,
    const StatusCallback& callback,
    bool exclusive, bool recursive) {
  FileSystemFileUtilProxy::CreateDirectory(
      operation_context_.get(),
      src_util_, url, exclusive, recursive,
      base::Bind(&LocalFileSystemOperation::DidFinishFileOperation,
                 base::Owned(this), callback));
}

void LocalFileSystemOperation::DoCopy(const FileSystemURL& src_url,
                                      const FileSystemURL& dest_url,
                                      const StatusCallback& callback) {
  FileSystemFileUtilProxy::Copy(
      operation_context_.get(),
      src_util_, dest_util_,
      src_url, dest_url,
      base::Bind(&LocalFileSystemOperation::DidFinishFileOperation,
                 base::Owned(this), callback));
}

void LocalFileSystemOperation::DoCopyInForeignFile(
    const FilePath& src_local_disk_file_path,
    const FileSystemURL& dest_url,
    const StatusCallback& callback) {
  FileSystemFileUtilProxy::CopyInForeignFile(
      operation_context_.get(),
      dest_util_,
      src_local_disk_file_path, dest_url,
      base::Bind(&LocalFileSystemOperation::DidFinishFileOperation,
                 base::Owned(this), callback));
}

void LocalFileSystemOperation::DoMove(const FileSystemURL& src_url,
                                      const FileSystemURL& dest_url,
                                      const StatusCallback& callback) {
  FileSystemFileUtilProxy::Move(
      operation_context_.get(),
      src_util_, dest_util_,
      src_url, dest_url,
      base::Bind(&LocalFileSystemOperation::DidFinishFileOperation,
                 base::Owned(this), callback));
}

void LocalFileSystemOperation::DoTruncate(const FileSystemURL& url,
                                          const StatusCallback& callback,
                                          int64 length) {
  FileSystemFileUtilProxy::Truncate(
      operation_context_.get(), src_util_, url, length,
      base::Bind(&LocalFileSystemOperation::DidFinishFileOperation,
                 base::Owned(this), callback));
}

void LocalFileSystemOperation::DoOpenFile(const FileSystemURL& url,
                                          const OpenFileCallback& callback,
                                          int file_flags) {
  FileSystemFileUtilProxy::CreateOrOpen(
      operation_context_.get(), src_util_, url, file_flags,
      base::Bind(&LocalFileSystemOperation::DidOpenFile,
                 base::Owned(this), callback));
}

void LocalFileSystemOperation::DidEnsureFileExistsExclusive(
    const StatusCallback& callback,
    base::PlatformFileError rv, bool created) {
  if (rv == base::PLATFORM_FILE_OK && !created) {
    callback.Run(base::PLATFORM_FILE_ERROR_EXISTS);
  } else {
    DidFinishFileOperation(callback, rv);
  }
}

void LocalFileSystemOperation::DidEnsureFileExistsNonExclusive(
    const StatusCallback& callback,
    base::PlatformFileError rv, bool /* created */) {
  DidFinishFileOperation(callback, rv);
}

void LocalFileSystemOperation::DidFinishFileOperation(
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

void LocalFileSystemOperation::DidDirectoryExists(
    const StatusCallback& callback,
    base::PlatformFileError rv,
    const base::PlatformFileInfo& file_info,
    const FilePath& unused) {
  if (rv == base::PLATFORM_FILE_OK && !file_info.is_directory)
    rv = base::PLATFORM_FILE_ERROR_NOT_A_DIRECTORY;
  callback.Run(rv);
}

void LocalFileSystemOperation::DidFileExists(
    const StatusCallback& callback,
    base::PlatformFileError rv,
    const base::PlatformFileInfo& file_info,
    const FilePath& unused) {
  if (rv == base::PLATFORM_FILE_OK && file_info.is_directory)
    rv = base::PLATFORM_FILE_ERROR_NOT_A_FILE;
  callback.Run(rv);
}

void LocalFileSystemOperation::DidGetMetadata(
    const GetMetadataCallback& callback,
    base::PlatformFileError rv,
    const base::PlatformFileInfo& file_info,
    const FilePath& platform_path) {
  callback.Run(rv, file_info, platform_path);
}

void LocalFileSystemOperation::DidReadDirectory(
    const ReadDirectoryCallback& callback,
    base::PlatformFileError rv,
    const std::vector<base::FileUtilProxy::Entry>& entries,
    bool has_more) {
  callback.Run(rv, entries, has_more);
}

void LocalFileSystemOperation::DidWrite(
    const FileSystemURL& url,
    base::PlatformFileError rv,
    int64 bytes,
    FileWriterDelegate::WriteProgressStatus write_status) {
  if (write_callback_.is_null()) {
    // If cancelled, callback is already invoked and set to null in Cancel().
    // We must not call it twice. Just shut down this operation object.
    delete this;
    return;
  }

  const bool complete = (
      write_status != FileWriterDelegate::SUCCESS_IO_PENDING);
  if (complete && write_status != FileWriterDelegate::ERROR_WRITE_NOT_STARTED) {
    operation_context_->change_observers()->Notify(
        &FileChangeObserver::OnModifyFile, MakeTuple(url));
  }

  write_callback_.Run(rv, bytes, complete);
  if (complete || rv != base::PLATFORM_FILE_OK)
    delete this;
}

void LocalFileSystemOperation::DidTouchFile(const StatusCallback& callback,
                                            base::PlatformFileError rv) {
  callback.Run(rv);
}

void LocalFileSystemOperation::DidOpenFile(
    const OpenFileCallback& callback,
    base::PlatformFileError rv,
    base::PassPlatformFile file,
    bool unused) {
  if (rv == base::PLATFORM_FILE_OK)
    CHECK_NE(base::kNullProcessHandle, peer_handle_);
  callback.Run(rv, file.ReleaseValue(), peer_handle_);
}

void LocalFileSystemOperation::DidCreateSnapshotFile(
    const SnapshotFileCallback& callback,
    base::PlatformFileError result,
    const base::PlatformFileInfo& file_info,
    const FilePath& platform_path,
    FileSystemFileUtil::SnapshotFilePolicy snapshot_policy) {
  scoped_refptr<ShareableFileReference> file_ref;
  if (result == base::PLATFORM_FILE_OK &&
      snapshot_policy == FileSystemFileUtil::kSnapshotFileTemporary) {
    file_ref = ShareableFileReference::GetOrCreate(
        platform_path, ShareableFileReference::DELETE_ON_FINAL_RELEASE,
        file_system_context()->task_runners()->file_task_runner());
  }
  callback.Run(result, file_info, platform_path, file_ref);
}

base::PlatformFileError LocalFileSystemOperation::SetUp(
    const FileSystemURL& url,
    FileSystemFileUtil** file_util,
    SetUpMode mode) {
  if (!url.is_valid())
    return base::PLATFORM_FILE_ERROR_INVALID_URL;

  // Restricted file system is read-only.
  if (url.type() == fileapi::kFileSystemTypeRestrictedNativeLocal &&
      mode != SETUP_FOR_READ)
    return base::PLATFORM_FILE_ERROR_SECURITY;

  if (!file_system_context()->GetMountPointProvider(
          url.type())->IsAccessAllowed(url))
    return base::PLATFORM_FILE_ERROR_SECURITY;

  DCHECK(file_util);
  if (!*file_util)
    *file_util = file_system_context()->GetFileUtil(url.type());
  if (!*file_util)
    return base::PLATFORM_FILE_ERROR_SECURITY;

  if (mode == SETUP_FOR_READ) {
    // TODO(kinuko): This doesn't work well for cross-filesystem operation
    // in the current architecture since the operation context (thus the
    // observers) is configured for the destination URL while this method
    // could be called for both src and dest URL.
    if (!is_cross_operation_) {
      operation_context_->access_observers()->Notify(
          &FileAccessObserver::OnAccess, MakeTuple(url));
    }
    return base::PLATFORM_FILE_OK;
  }

  DCHECK(mode == SETUP_FOR_WRITE || mode == SETUP_FOR_CREATE);

  scoped_update_notifiers_.push_back(new ScopedUpdateNotifier(
      operation_context_.get(), url));

  // Any write access is disallowed on the root path.
  if (url.path().value().length() == 0 ||
      url.path().DirName().value() == url.path().value())
    return base::PLATFORM_FILE_ERROR_SECURITY;

  if (mode == SETUP_FOR_CREATE) {
    FileSystemMountPointProvider* provider = file_system_context()->
        GetMountPointProvider(url.type());

    // Check if the cracked file name looks good to create.
    if (provider->IsRestrictedFileName(VirtualPath::BaseName(url.path())))
      return base::PLATFORM_FILE_ERROR_SECURITY;
  }

  return base::PLATFORM_FILE_OK;
}

bool LocalFileSystemOperation::SetPendingOperationType(OperationType type) {
  if (pending_operation_ != kOperationNone)
    return false;
  pending_operation_ = type;
  return true;
}

}  // namespace fileapi
