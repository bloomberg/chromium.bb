// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/file_system_operation.h"

#include "base/bind.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "net/base/escape.h"
#include "net/url_request/url_request_context.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_file_util_proxy.h"
#include "webkit/fileapi/file_system_mount_point_provider.h"
#include "webkit/fileapi/file_system_operation_context.h"
#include "webkit/fileapi/file_system_quota_util.h"
#include "webkit/fileapi/file_system_types.h"
#include "webkit/fileapi/file_system_util.h"
#include "webkit/fileapi/file_writer_delegate.h"
#include "webkit/fileapi/local_file_util.h"
#include "webkit/fileapi/quota_file_util.h"
#include "webkit/quota/quota_types.h"

namespace fileapi {

class FileSystemOperation::ScopedQuotaUtilHelper {
 public:
  ScopedQuotaUtilHelper(FileSystemContext* context,
                        const GURL& origin_url,
                        FileSystemType type);
  ~ScopedQuotaUtilHelper();

 private:
  FileSystemQuotaUtil* quota_util_;
  const GURL& origin_url_;
  FileSystemType type_;
  DISALLOW_COPY_AND_ASSIGN(ScopedQuotaUtilHelper);
};

FileSystemOperation::ScopedQuotaUtilHelper::ScopedQuotaUtilHelper(
    FileSystemContext* context, const GURL& origin_url, FileSystemType type)
    : origin_url_(origin_url), type_(type) {
  DCHECK(context);
  DCHECK(type != kFileSystemTypeUnknown);
  quota_util_ = context->GetQuotaUtil(type_);
  if (quota_util_) {
    DCHECK(quota_util_->proxy());
    quota_util_->proxy()->StartUpdateOrigin(origin_url_, type_);
  }
}

FileSystemOperation::ScopedQuotaUtilHelper::~ScopedQuotaUtilHelper() {
  if (quota_util_) {
    DCHECK(quota_util_->proxy());
    quota_util_->proxy()->EndUpdateOrigin(origin_url_, type_);
  }
}

FileSystemOperation::~FileSystemOperation() {
  if (file_writer_delegate_.get()) {
    FileSystemOperationContext* c =
        new FileSystemOperationContext(operation_context_);
    base::FileUtilProxy::RelayClose(
        proxy_,
        base::Bind(&FileSystemFileUtil::Close,
                   base::Unretained(c->src_file_util()),
                   base::Owned(c)),
        file_writer_delegate_->file(),
        base::FileUtilProxy::StatusCallback());
  }
}

void FileSystemOperation::CreateFile(const GURL& path,
                                     bool exclusive,
                                     const StatusCallback& callback) {
  DCHECK(SetPendingOperationType(kOperationCreateFile));

  base::PlatformFileError result = SetupSrcContextForWrite(path, true);
  if (result != base::PLATFORM_FILE_OK) {
    callback.Run(result);
    delete this;
    return;
  }
  GetUsageAndQuotaThenCallback(
      operation_context_.src_origin_url(),
      base::Bind(&FileSystemOperation::DelayedCreateFileForQuota,
                 base::Unretained(this), callback, exclusive));
}

void FileSystemOperation::DelayedCreateFileForQuota(
    const StatusCallback& callback,
    bool exclusive,
    quota::QuotaStatusCode status, int64 usage, int64 quota) {
  operation_context_.set_allowed_bytes_growth(quota - usage);

  quota_util_helper_.reset(new ScopedQuotaUtilHelper(
      file_system_context(),
      operation_context_.src_origin_url(),
      operation_context_.src_type()));

  FileSystemFileUtilProxy::RelayEnsureFileExists(
      proxy_,
      base::Bind(&FileSystemFileUtil::EnsureFileExists,
                 base::Unretained(operation_context_.src_file_util()),
                 &operation_context_, src_virtual_path_),
      base::Bind(
          exclusive ? &FileSystemOperation::DidEnsureFileExistsExclusive
                    : &FileSystemOperation::DidEnsureFileExistsNonExclusive,
          base::Owned(this), callback));
}

void FileSystemOperation::CreateDirectory(const GURL& path,
                                          bool exclusive,
                                          bool recursive,
                                          const StatusCallback& callback) {
  DCHECK(SetPendingOperationType(kOperationCreateDirectory));

  base::PlatformFileError result = SetupSrcContextForWrite(path, true);
  if (result != base::PLATFORM_FILE_OK) {
    callback.Run(result);
    delete this;
    return;
  }
  GetUsageAndQuotaThenCallback(
      operation_context_.src_origin_url(),
      base::Bind(&FileSystemOperation::DelayedCreateDirectoryForQuota,
                 base::Unretained(this), callback, exclusive, recursive));
}

void FileSystemOperation::DelayedCreateDirectoryForQuota(
    const StatusCallback& callback,
    bool exclusive, bool recursive,
    quota::QuotaStatusCode status, int64 usage, int64 quota) {
  operation_context_.set_allowed_bytes_growth(quota - usage);

  quota_util_helper_.reset(new ScopedQuotaUtilHelper(
      file_system_context(),
      operation_context_.src_origin_url(),
      operation_context_.src_type()));

  base::FileUtilProxy::RelayFileTask(
      proxy_, FROM_HERE,
      base::Bind(&FileSystemFileUtil::CreateDirectory,
                 base::Unretained(operation_context_.src_file_util()),
                 &operation_context_,
                 src_virtual_path_, exclusive, recursive),
      base::Bind(&FileSystemOperation::DidFinishFileOperation,
                 base::Owned(this), callback));
}

void FileSystemOperation::Copy(const GURL& src_path,
                               const GURL& dest_path,
                               const StatusCallback& callback) {
  DCHECK(SetPendingOperationType(kOperationCopy));

  base::PlatformFileError result = SetupSrcContextForRead(src_path);
  if (result == base::PLATFORM_FILE_OK)
    result = SetupDestContextForWrite(dest_path, true);
  if (result != base::PLATFORM_FILE_OK) {
    callback.Run(result);
    delete this;
    return;
  }

  GetUsageAndQuotaThenCallback(
      operation_context_.dest_origin_url(),
      base::Bind(&FileSystemOperation::DelayedCopyForQuota,
                 base::Unretained(this), callback));
}

void FileSystemOperation::DelayedCopyForQuota(const StatusCallback& callback,
                                              quota::QuotaStatusCode status,
                                              int64 usage, int64 quota) {
  operation_context_.set_allowed_bytes_growth(quota - usage);

  quota_util_helper_.reset(new ScopedQuotaUtilHelper(
      file_system_context(),
      operation_context_.dest_origin_url(),
      operation_context_.dest_type()));

  base::FileUtilProxy::RelayFileTask(
      proxy_, FROM_HERE,
      base::Bind(&FileSystemFileUtil::Copy,
                 base::Unretained(operation_context_.src_file_util()),
                 &operation_context_,
                 src_virtual_path_, dest_virtual_path_),
      base::Bind(&FileSystemOperation::DidFinishFileOperation,
                 base::Owned(this), callback));
}

void FileSystemOperation::Move(const GURL& src_path,
                               const GURL& dest_path,
                               const StatusCallback& callback) {
  DCHECK(SetPendingOperationType(kOperationMove));

  base::PlatformFileError result = SetupSrcContextForWrite(src_path, false);
  if (result == base::PLATFORM_FILE_OK)
    result = SetupDestContextForWrite(dest_path, true);
  if (result != base::PLATFORM_FILE_OK) {
    callback.Run(result);
    delete this;
    return;
  }

  GetUsageAndQuotaThenCallback(
      operation_context_.dest_origin_url(),
      base::Bind(&FileSystemOperation::DelayedMoveForQuota,
                 base::Unretained(this), callback));
}

void FileSystemOperation::DelayedMoveForQuota(const StatusCallback& callback,
                                              quota::QuotaStatusCode status,
                                              int64 usage, int64 quota) {
  operation_context_.set_allowed_bytes_growth(quota - usage);

  quota_util_helper_.reset(new ScopedQuotaUtilHelper(
      file_system_context(),
      operation_context_.dest_origin_url(),
      operation_context_.dest_type()));

  base::FileUtilProxy::RelayFileTask(
      proxy_, FROM_HERE,
      base::Bind(&FileSystemFileUtil::Move,
                 base::Unretained(operation_context_.src_file_util()),
                 &operation_context_,
                 src_virtual_path_, dest_virtual_path_),
      base::Bind(&FileSystemOperation::DidFinishFileOperation,
                 base::Owned(this), callback));
}

void FileSystemOperation::DirectoryExists(const GURL& path,
                                          const StatusCallback& callback) {
  DCHECK(SetPendingOperationType(kOperationDirectoryExists));

  base::PlatformFileError result = SetupSrcContextForRead(path);
  if (result != base::PLATFORM_FILE_OK) {
    callback.Run(result);
    delete this;
    return;
  }

  FileSystemFileUtilProxy::RelayGetFileInfo(
      proxy_,
      base::Bind(&FileSystemFileUtil::GetFileInfo,
                 base::Unretained(operation_context_.src_file_util()),
                 &operation_context_, src_virtual_path_),
      base::Bind(&FileSystemOperation::DidDirectoryExists,
                 base::Owned(this), callback));
}

void FileSystemOperation::FileExists(const GURL& path,
                                     const StatusCallback& callback) {
  DCHECK(SetPendingOperationType(kOperationFileExists));

  base::PlatformFileError result = SetupSrcContextForRead(path);
  if (result != base::PLATFORM_FILE_OK) {
    callback.Run(result);
    delete this;
    return;
  }

  FileSystemFileUtilProxy::RelayGetFileInfo(
      proxy_,
      base::Bind(&FileSystemFileUtil::GetFileInfo,
                 base::Unretained(operation_context_.src_file_util()),
                 &operation_context_, src_virtual_path_),
      base::Bind(&FileSystemOperation::DidFileExists,
                 base::Owned(this), callback));
}

void FileSystemOperation::GetMetadata(const GURL& path,
                                      const GetMetadataCallback& callback) {
  DCHECK(SetPendingOperationType(kOperationGetMetadata));

  base::PlatformFileError result = SetupSrcContextForRead(path);
  if (result != base::PLATFORM_FILE_OK) {
    callback.Run(result, base::PlatformFileInfo(), FilePath());
    delete this;
    return;
  }

  FileSystemFileUtilProxy::RelayGetFileInfo(
      proxy_,
      base::Bind(&FileSystemFileUtil::GetFileInfo,
                 base::Unretained(operation_context_.src_file_util()),
                 &operation_context_, src_virtual_path_),
      base::Bind(&FileSystemOperation::DidGetMetadata,
                 base::Owned(this), callback));
}

void FileSystemOperation::ReadDirectory(const GURL& path,
                                        const ReadDirectoryCallback& callback) {
  DCHECK(SetPendingOperationType(kOperationReadDirectory));

  base::PlatformFileError result = SetupSrcContextForRead(path);
  if (result != base::PLATFORM_FILE_OK) {
    callback.Run(result, std::vector<base::FileUtilProxy::Entry>(), false);
    delete this;
    return;
  }

  FileSystemFileUtilProxy::RelayReadDirectory(
      proxy_,
      base::Bind(&FileSystemFileUtil::ReadDirectory,
                 base::Unretained(operation_context_.src_file_util()),
                 &operation_context_, src_virtual_path_),
      base::Bind(&FileSystemOperation::DidReadDirectory,
                 base::Owned(this), callback));
}

void FileSystemOperation::Remove(const GURL& path, bool recursive,
                                 const StatusCallback& callback) {
  DCHECK(SetPendingOperationType(kOperationRemove));

  base::PlatformFileError result = SetupSrcContextForWrite(path, false);
  if (result != base::PLATFORM_FILE_OK) {
    callback.Run(result);
    delete this;
    return;
  }

  base::FileUtilProxy::RelayFileTask(
      proxy_, FROM_HERE,
      base::Bind(&FileSystemFileUtil::Delete,
                 base::Unretained(operation_context_.src_file_util()),
                 &operation_context_, src_virtual_path_, recursive),
      base::Bind(&FileSystemOperation::DidFinishFileOperation,
                 base::Owned(this), callback));
}

void FileSystemOperation::Write(
    const net::URLRequestContext* url_request_context,
    const GURL& path,
    const GURL& blob_url,
    int64 offset,
    const WriteCallback& callback) {
  DCHECK(SetPendingOperationType(kOperationWrite));

  base::PlatformFileError result = SetupSrcContextForWrite(path, true);
  if (result != base::PLATFORM_FILE_OK) {
    callback.Run(result, 0, false);
    delete this;
    return;
  }
  DCHECK(blob_url.is_valid());
  file_writer_delegate_.reset(new FileWriterDelegate(this, offset, proxy_));
  set_write_callback(callback);
  blob_request_.reset(
      new net::URLRequest(blob_url, file_writer_delegate_.get()));
  blob_request_->set_context(url_request_context);

  GetUsageAndQuotaThenCallback(
      operation_context_.src_origin_url(),
      base::Bind(&FileSystemOperation::DelayedWriteForQuota,
                 base::Unretained(this)));
}

void FileSystemOperation::DelayedWriteForQuota(quota::QuotaStatusCode status,
                                               int64 usage, int64 quota) {
  operation_context_.set_allowed_bytes_growth(quota - usage);

  quota_util_helper_.reset(new ScopedQuotaUtilHelper(
      file_system_context(),
      operation_context_.src_origin_url(),
      operation_context_.src_type()));

  int file_flags = base::PLATFORM_FILE_OPEN |
                   base::PLATFORM_FILE_WRITE |
                   base::PLATFORM_FILE_ASYNC;

  base::FileUtilProxy::RelayCreateOrOpen(
      proxy_,
      base::Bind(&FileSystemFileUtil::CreateOrOpen,
                 base::Unretained(operation_context_.src_file_util()),
                 &operation_context_, src_virtual_path_, file_flags),
      base::Bind(&FileSystemFileUtil::Close,
                 base::Unretained(operation_context_.src_file_util()),
                 &operation_context_),
      base::Bind(&FileSystemOperation::OnFileOpenedForWrite,
                 base::Unretained(this)));
}

void FileSystemOperation::Truncate(const GURL& path, int64 length,
                                   const StatusCallback& callback) {
  DCHECK(SetPendingOperationType(kOperationTruncate));

  base::PlatformFileError result = SetupSrcContextForWrite(path, false);
  if (result != base::PLATFORM_FILE_OK) {
    callback.Run(result);
    delete this;
    return;
  }
  GetUsageAndQuotaThenCallback(
      operation_context_.src_origin_url(),
      base::Bind(&FileSystemOperation::DelayedTruncateForQuota,
                 base::Unretained(this), callback, length));
}

void FileSystemOperation::DelayedTruncateForQuota(
    const StatusCallback& callback,
    int64 length, quota::QuotaStatusCode status, int64 usage, int64 quota) {
  operation_context_.set_allowed_bytes_growth(quota - usage);

  quota_util_helper_.reset(new ScopedQuotaUtilHelper(
      file_system_context(),
      operation_context_.src_origin_url(),
      operation_context_.src_type()));

  base::FileUtilProxy::RelayFileTask(
      proxy_, FROM_HERE,
      base::Bind(&FileSystemFileUtil::Truncate,
                 base::Unretained(operation_context_.src_file_util()),
                 &operation_context_, src_virtual_path_, length),
      base::Bind(&FileSystemOperation::DidFinishFileOperation,
                 base::Owned(this), callback));
}

void FileSystemOperation::TouchFile(const GURL& path,
                                    const base::Time& last_access_time,
                                    const base::Time& last_modified_time,
                                    const StatusCallback& callback) {
  DCHECK(SetPendingOperationType(kOperationTouchFile));

  base::PlatformFileError result = SetupSrcContextForWrite(path, true);
  if (result != base::PLATFORM_FILE_OK) {
    callback.Run(result);
    delete this;
    return;
  }

  base::FileUtilProxy::RelayFileTask(
      proxy_, FROM_HERE,
      base::Bind(&FileSystemFileUtil::Touch,
                 base::Unretained(operation_context_.src_file_util()),
                 &operation_context_,
                 src_virtual_path_, last_access_time, last_modified_time),
      base::Bind(&FileSystemOperation::DidTouchFile,
                 base::Owned(this), callback));
}

void FileSystemOperation::OpenFile(const GURL& path,
                                   int file_flags,
                                   base::ProcessHandle peer_handle,
                                   const OpenFileCallback& callback) {
  DCHECK(SetPendingOperationType(kOperationOpenFile));

  peer_handle_ = peer_handle;

  if (file_flags & (
      (base::PLATFORM_FILE_ENUMERATE | base::PLATFORM_FILE_TEMPORARY |
       base::PLATFORM_FILE_HIDDEN))) {
    callback.Run(base::PLATFORM_FILE_ERROR_FAILED,
                 base::PlatformFile(), base::ProcessHandle());
    delete this;
    return;
  }
  if (file_flags &
      (base::PLATFORM_FILE_CREATE | base::PLATFORM_FILE_OPEN_ALWAYS |
       base::PLATFORM_FILE_CREATE_ALWAYS | base::PLATFORM_FILE_OPEN_TRUNCATED |
       base::PLATFORM_FILE_WRITE | base::PLATFORM_FILE_EXCLUSIVE_WRITE |
       base::PLATFORM_FILE_DELETE_ON_CLOSE |
       base::PLATFORM_FILE_WRITE_ATTRIBUTES)) {
    base::PlatformFileError result = SetupSrcContextForWrite(path, true);
    if (result != base::PLATFORM_FILE_OK) {
      callback.Run(result, base::PlatformFile(), base::ProcessHandle());
      delete this;
      return;
    }
  } else {
    base::PlatformFileError result = SetupSrcContextForRead(path);
    if (result != base::PLATFORM_FILE_OK) {
      callback.Run(result, base::PlatformFile(), base::ProcessHandle());
      delete this;
      return;
    }
  }
  GetUsageAndQuotaThenCallback(
      operation_context_.src_origin_url(),
      base::Bind(&FileSystemOperation::DelayedOpenFileForQuota,
                 base::Unretained(this), callback, file_flags));
}

void FileSystemOperation::DelayedOpenFileForQuota(
    const OpenFileCallback& callback,
    int file_flags, quota::QuotaStatusCode status, int64 usage, int64 quota) {
  operation_context_.set_allowed_bytes_growth(quota - usage);

  quota_util_helper_.reset(new ScopedQuotaUtilHelper(
      file_system_context(),
      operation_context_.src_origin_url(),
      operation_context_.src_type()));

  base::FileUtilProxy::RelayCreateOrOpen(
      proxy_,
      base::Bind(&FileSystemFileUtil::CreateOrOpen,
                 base::Unretained(operation_context_.src_file_util()),
                 &operation_context_,
                 src_virtual_path_, file_flags),
      base::Bind(&FileSystemFileUtil::Close,
                 base::Unretained(operation_context_.src_file_util()),
                 &operation_context_),
      base::Bind(&FileSystemOperation::DidOpenFile,
                 base::Owned(this), callback));
}

// We can only get here on a write or truncate that's not yet completed.
// We don't support cancelling any other operation at this time.
void FileSystemOperation::Cancel(const StatusCallback& cancel_callback) {
  if (file_writer_delegate_.get()) {
    DCHECK_EQ(kOperationWrite, pending_operation_);
    // Writes are done without proxying through FileUtilProxy after the initial
    // opening of the PlatformFile.  All state changes are done on this thread,
    // so we're guaranteed to be able to shut down atomically.  We do need to
    // check that the file has been opened [which means the blob_request_ has
    // been created], so we know how much we need to do.
    if (blob_request_.get())
      // This halts any calls to file_writer_delegate_ from blob_request_.
      blob_request_->Cancel();

    if (!write_callback_.is_null()) {
      // Notify the failure status to the ongoing operation's callback.
      write_callback_.Run(base::PLATFORM_FILE_ERROR_ABORT, 0, false);
      // Do not delete this FileSystemOperation object yet. As a result of
      // abort, this->DidWrite is called later and there we delete this object.
    }
    cancel_callback.Run(base::PLATFORM_FILE_OK);
    write_callback_.Reset();
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

FileSystemOperation* FileSystemOperation::AsFileSystemOperation() {
  return this;
}

void FileSystemOperation::SyncGetPlatformPath(const GURL& path,
                                              FilePath* platform_path) {
  DCHECK(SetPendingOperationType(kOperationGetLocalPath));

  base::PlatformFileError result = SetupSrcContextForRead(path);
  if (result != base::PLATFORM_FILE_OK) {
    delete this;
    return;
  }

  operation_context_.src_file_util()->GetLocalFilePath(
      &operation_context_, src_virtual_path_, platform_path);

  delete this;
}

FileSystemOperation::FileSystemOperation(
    scoped_refptr<base::MessageLoopProxy> proxy,
    FileSystemContext* file_system_context)
    : proxy_(proxy),
      operation_context_(file_system_context, NULL),
      peer_handle_(base::kNullProcessHandle),
      pending_operation_(kOperationNone) {
}

void FileSystemOperation::GetUsageAndQuotaThenCallback(
    const GURL& origin_url,
    const quota::QuotaManager::GetUsageAndQuotaCallback& callback) {
  quota::QuotaManagerProxy* quota_manager_proxy =
      file_system_context()->quota_manager_proxy();
  if (!quota_manager_proxy ||
      !file_system_context()->GetQuotaUtil(
          operation_context_.src_type())) {
    // If we don't have the quota manager or the requested filesystem type
    // does not support quota, we should be able to let it go.
    callback.Run(quota::kQuotaStatusOk, 0, kint64max);
    return;
  }
  DCHECK(quota_manager_proxy);
  DCHECK(quota_manager_proxy->quota_manager());
  quota_manager_proxy->quota_manager()->GetUsageAndQuota(
      operation_context_.src_origin_url(),
      FileSystemTypeToQuotaStorageType(
          operation_context_.src_type()),
      callback);
}

void FileSystemOperation::DidEnsureFileExistsExclusive(
    const StatusCallback& callback,
    base::PlatformFileError rv, bool created) {
  if (rv == base::PLATFORM_FILE_OK && !created) {
    callback.Run(base::PLATFORM_FILE_ERROR_EXISTS);
  } else {
    DidFinishFileOperation(callback, rv);
  }
}

void FileSystemOperation::DidEnsureFileExistsNonExclusive(
    const StatusCallback& callback,
    base::PlatformFileError rv, bool /* created */) {
  DidFinishFileOperation(callback, rv);
}

void FileSystemOperation::DidFinishFileOperation(
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

void FileSystemOperation::DidDirectoryExists(
    const StatusCallback& callback,
    base::PlatformFileError rv,
    const base::PlatformFileInfo& file_info,
    const FilePath& unused) {
  if (rv == base::PLATFORM_FILE_OK && !file_info.is_directory)
    rv = base::PLATFORM_FILE_ERROR_NOT_A_DIRECTORY;
  callback.Run(rv);
}

void FileSystemOperation::DidFileExists(
    const StatusCallback& callback,
    base::PlatformFileError rv,
    const base::PlatformFileInfo& file_info,
    const FilePath& unused) {
  if (rv == base::PLATFORM_FILE_OK && file_info.is_directory)
    rv = base::PLATFORM_FILE_ERROR_NOT_A_FILE;
  callback.Run(rv);
}

void FileSystemOperation::DidGetMetadata(
    const GetMetadataCallback& callback,
    base::PlatformFileError rv,
    const base::PlatformFileInfo& file_info,
    const FilePath& platform_path) {
  callback.Run(rv, file_info, platform_path);
}

void FileSystemOperation::DidReadDirectory(
    const ReadDirectoryCallback& callback,
    base::PlatformFileError rv,
    const std::vector<base::FileUtilProxy::Entry>& entries,
    bool has_more) {
  callback.Run(rv, entries, has_more);
}

void FileSystemOperation::DidWrite(
    base::PlatformFileError rv,
    int64 bytes,
    bool complete) {
  if (write_callback_.is_null()) {
    // If cancelled, callback is already invoked and set to null in Cancel().
    // We must not call it twice. Just shut down this operation object.
    delete this;
    return;
  }
  write_callback_.Run(rv, bytes, complete);
  if (complete || rv != base::PLATFORM_FILE_OK)
    delete this;
}

void FileSystemOperation::DidTouchFile(const StatusCallback& callback,
                                       base::PlatformFileError rv) {
  callback.Run(rv);
}

void FileSystemOperation::DidOpenFile(
    const OpenFileCallback& callback,
    base::PlatformFileError rv,
    base::PassPlatformFile file,
    bool unused) {
  if (rv == base::PLATFORM_FILE_OK)
    CHECK_NE(base::kNullProcessHandle, peer_handle_);
  callback.Run(rv, file.ReleaseValue(), peer_handle_);
}

void FileSystemOperation::OnFileOpenedForWrite(
    base::PlatformFileError rv,
    base::PassPlatformFile file,
    bool created) {
  if (rv != base::PLATFORM_FILE_OK) {
    if (!write_callback_.is_null())
      write_callback_.Run(rv, 0, false);
    delete this;
    return;
  }
  file_writer_delegate_->Start(file.ReleaseValue(), blob_request_.get());
}

base::PlatformFileError FileSystemOperation::VerifyFileSystemPathForRead(
    const GURL& path, GURL* origin_url, FileSystemType* type,
    FilePath* virtual_path, FileSystemFileUtil** file_util) {
  base::PlatformFileError rv =
      VerifyFileSystemPath(path, origin_url, type, virtual_path, file_util);
  if (rv != base::PLATFORM_FILE_OK)
    return rv;

  // We notify this read access whether the read access succeeds or not.
  // This must be ok since this is used to let the QM's eviction logic know
  // someone is interested in reading the origin data and therefore to indicate
  // that evicting this origin may not be a good idea.
  FileSystemQuotaUtil* quota_util = file_system_context()->GetQuotaUtil(*type);
  if (quota_util) {
    quota_util->NotifyOriginWasAccessedOnIOThread(
        file_system_context()->quota_manager_proxy(),
        *origin_url,
        *type);
  }

  return base::PLATFORM_FILE_OK;
}

base::PlatformFileError FileSystemOperation::VerifyFileSystemPathForWrite(
    const GURL& path, bool create, GURL* origin_url, FileSystemType* type,
    FilePath* virtual_path, FileSystemFileUtil** file_util) {
  base::PlatformFileError rv =
      VerifyFileSystemPath(path, origin_url, type, virtual_path, file_util);
  if (rv != base::PLATFORM_FILE_OK)
    return rv;

  // Any write access is disallowed on the root path.
  if (virtual_path->value().length() == 0 ||
      virtual_path->DirName().value() == virtual_path->value()) {
    return base::PLATFORM_FILE_ERROR_SECURITY;
  }
  if (create &&
      file_system_context()->GetMountPointProvider(*type)->IsRestrictedFileName(
          virtual_path->BaseName())) {
    return base::PLATFORM_FILE_ERROR_SECURITY;
  }

  return base::PLATFORM_FILE_OK;
}

base::PlatformFileError FileSystemOperation::VerifyFileSystemPath(
    const GURL& path, GURL* origin_url, FileSystemType* type,
    FilePath* virtual_path, FileSystemFileUtil** file_util) {
  DCHECK(file_system_context());

  if (!CrackFileSystemURL(path, origin_url, type, virtual_path)) {
    return base::PLATFORM_FILE_ERROR_INVALID_URL;
  }
  if (!file_system_context()->GetMountPointProvider(*type)->IsAccessAllowed(
      *origin_url, *type, *virtual_path)) {
    return base::PLATFORM_FILE_ERROR_SECURITY;
  }
  DCHECK(file_util);
  *file_util = file_system_context()->GetFileUtil(*type);
  DCHECK(*file_util);

  return base::PLATFORM_FILE_OK;
}

base::PlatformFileError FileSystemOperation::SetupSrcContextForRead(
    const GURL& path) {
  GURL origin_url;
  FileSystemType type;
  FileSystemFileUtil* file_util;
  base::PlatformFileError result = VerifyFileSystemPathForRead(
      path, &origin_url, &type, &src_virtual_path_, &file_util);
  operation_context_.set_src_origin_url(origin_url);
  operation_context_.set_src_type(type);
  if (!operation_context_.src_file_util())
    operation_context_.set_src_file_util(file_util);
  return result;
}

base::PlatformFileError FileSystemOperation::SetupSrcContextForWrite(
    const GURL& path,
    bool create) {
  GURL origin_url;
  FileSystemType type;
  FileSystemFileUtil* file_util = NULL;
  base::PlatformFileError result = VerifyFileSystemPathForWrite(
      path, create, &origin_url, &type, &src_virtual_path_, &file_util);
  operation_context_.set_src_origin_url(origin_url);
  operation_context_.set_src_type(type);
  if (!operation_context_.src_file_util())
    operation_context_.set_src_file_util(file_util);
  return result;
}

base::PlatformFileError FileSystemOperation::SetupDestContextForWrite(
    const GURL& path,
    bool create) {
  GURL origin_url;
  FileSystemType type;
  FileSystemFileUtil* file_util = NULL;
  base::PlatformFileError result = VerifyFileSystemPathForWrite(
      path, create, &origin_url, &type, &dest_virtual_path_, &file_util);
  operation_context_.set_dest_origin_url(origin_url);
  operation_context_.set_dest_type(type);
  if (!operation_context_.dest_file_util())
    operation_context_.set_dest_file_util(file_util);
  return result;
}

bool FileSystemOperation::SetPendingOperationType(OperationType type) {
  if (pending_operation_ != kOperationNone)
    return false;
  pending_operation_ = type;
  return true;
}

}  // namespace fileapi
