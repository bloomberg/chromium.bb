// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/file_system_operation.h"

#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "net/base/escape.h"
#include "net/url_request/url_request_context.h"
#include "webkit/fileapi/file_system_callback_dispatcher.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_file_util_proxy.h"
#include "webkit/fileapi/file_system_operation_context.h"
#include "webkit/fileapi/file_system_path_manager.h"
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

FileSystemOperation::FileSystemOperation(
    FileSystemCallbackDispatcher* dispatcher,
    scoped_refptr<base::MessageLoopProxy> proxy,
    FileSystemContext* file_system_context,
    FileSystemFileUtil* file_util)
    : proxy_(proxy),
      dispatcher_(dispatcher),
      file_system_operation_context_(file_system_context, file_util),
      callback_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
  DCHECK(dispatcher);
#ifndef NDEBUG
  pending_operation_ = kOperationNone;
#endif
}

FileSystemOperation::~FileSystemOperation() {
  if (file_writer_delegate_.get())
    FileSystemFileUtilProxy::Close(
        file_system_operation_context_,
        proxy_, file_writer_delegate_->file(), NULL);
}

void FileSystemOperation::OpenFileSystem(
    const GURL& origin_url, fileapi::FileSystemType type, bool create) {
#ifndef NDEBUG
  DCHECK(kOperationNone == pending_operation_);
  pending_operation_ = static_cast<FileSystemOperation::OperationType>(
      kOperationOpenFileSystem);
#endif

  DCHECK(file_system_context());
  file_system_operation_context_.set_src_origin_url(origin_url);
  file_system_operation_context_.set_src_type(type);
  // TODO(ericu): We don't really need to make this call if !create.
  // Also, in the future we won't need it either way, as long as we do all
  // permission+quota checks beforehand.  We only need it now because we have to
  // create an unpredictable directory name.  Without that, we could lazily
  // create the root later on the first filesystem write operation, and just
  // return GetFileSystemRootURI() here.
  file_system_context()->path_manager()->ValidateFileSystemRootAndGetURL(
      origin_url, type, create,
      callback_factory_.NewCallback(&FileSystemOperation::DidGetRootPath));
}

void FileSystemOperation::CreateFile(const GURL& path,
                                     bool exclusive) {
#ifndef NDEBUG
  DCHECK(kOperationNone == pending_operation_);
  pending_operation_ = kOperationCreateFile;
#endif
  GURL origin_url;
  FileSystemType type;
  FileSystemFileUtil* file_util;
  if (!VerifyFileSystemPathForWrite(path, true /* create */, &origin_url,
      &type, &src_virtual_path_, &file_util)) {
    delete this;
    return;
  }
  file_system_operation_context_.set_src_origin_url(origin_url);
  file_system_operation_context_.set_src_type(type);
  if (!file_system_operation_context_.src_file_util())
    file_system_operation_context_.set_src_file_util(file_util);
  exclusive_ = exclusive;

  GetUsageAndQuotaThenCallback(origin_url, callback_factory_.NewCallback(
      &FileSystemOperation::DelayedCreateFileForQuota));
}

void FileSystemOperation::DelayedCreateFileForQuota(
    quota::QuotaStatusCode status, int64 usage, int64 quota) {
  file_system_operation_context_.set_allowed_bytes_growth(quota - usage);

  quota_util_helper_.reset(new ScopedQuotaUtilHelper(
      file_system_context(),
      file_system_operation_context_.src_origin_url(),
      file_system_operation_context_.src_type()));

  FileSystemFileUtilProxy::EnsureFileExists(
      file_system_operation_context_,
      proxy_,
      src_virtual_path_,
      callback_factory_.NewCallback(
          exclusive_ ? &FileSystemOperation::DidEnsureFileExistsExclusive
                     : &FileSystemOperation::DidEnsureFileExistsNonExclusive));
}

void FileSystemOperation::CreateDirectory(const GURL& path,
                                          bool exclusive,
                                          bool recursive) {
#ifndef NDEBUG
  DCHECK(kOperationNone == pending_operation_);
  pending_operation_ = kOperationCreateDirectory;
#endif
  GURL origin_url;
  FileSystemType type;
  FileSystemFileUtil* file_util;

  if (!VerifyFileSystemPathForWrite(path, true /* create */, &origin_url,
      &type, &src_virtual_path_, &file_util)) {
    delete this;
    return;
  }
  file_system_operation_context_.set_src_origin_url(origin_url);
  file_system_operation_context_.set_src_type(type);
  if (!file_system_operation_context_.src_file_util())
    file_system_operation_context_.set_src_file_util(file_util);
  exclusive_ = exclusive;
  recursive_ = recursive;

  GetUsageAndQuotaThenCallback(origin_url, callback_factory_.NewCallback(
      &FileSystemOperation::DelayedCreateDirectoryForQuota));
}

void FileSystemOperation::DelayedCreateDirectoryForQuota(
    quota::QuotaStatusCode status, int64 usage, int64 quota) {
  file_system_operation_context_.set_allowed_bytes_growth(quota - usage);

  quota_util_helper_.reset(new ScopedQuotaUtilHelper(
      file_system_context(),
      file_system_operation_context_.src_origin_url(),
      file_system_operation_context_.src_type()));

  FileSystemFileUtilProxy::CreateDirectory(
      file_system_operation_context_,
      proxy_,
      src_virtual_path_,
      exclusive_,
      recursive_,
      callback_factory_.NewCallback(
          &FileSystemOperation::DidFinishFileOperation));
}

void FileSystemOperation::Copy(const GURL& src_path,
                               const GURL& dest_path) {
#ifndef NDEBUG
  DCHECK(kOperationNone == pending_operation_);
  pending_operation_ = kOperationCopy;
#endif
  GURL src_origin_url;
  GURL dest_origin_url;
  FileSystemType src_type;
  FileSystemType dest_type;
  FileSystemFileUtil* src_file_util;
  FileSystemFileUtil* dest_file_util;

  if (!VerifyFileSystemPathForRead(src_path, &src_origin_url, &src_type,
        &src_virtual_path_, &src_file_util) ||
      !VerifyFileSystemPathForWrite(dest_path, true /* create */,
          &dest_origin_url, &dest_type, &dest_virtual_path_, &dest_file_util)) {
    delete this;
    return;
  }
  file_system_operation_context_.set_src_origin_url(src_origin_url);
  file_system_operation_context_.set_dest_origin_url(dest_origin_url);
  file_system_operation_context_.set_src_type(src_type);
  file_system_operation_context_.set_dest_type(dest_type);
  if (!file_system_operation_context_.src_file_util())
    file_system_operation_context_.set_src_file_util(src_file_util);
  if (!file_system_operation_context_.dest_file_util())
    file_system_operation_context_.set_dest_file_util(dest_file_util);

  GetUsageAndQuotaThenCallback(dest_origin_url, callback_factory_.NewCallback(
      &FileSystemOperation::DelayedCopyForQuota));
}

void FileSystemOperation::DelayedCopyForQuota(quota::QuotaStatusCode status,
                                              int64 usage, int64 quota) {
  file_system_operation_context_.set_allowed_bytes_growth(quota - usage);

  quota_util_helper_.reset(new ScopedQuotaUtilHelper(
      file_system_context(),
      file_system_operation_context_.dest_origin_url(),
      file_system_operation_context_.dest_type()));

  FileSystemFileUtilProxy::Copy(
      file_system_operation_context_,
      proxy_,
      src_virtual_path_,
      dest_virtual_path_,
      callback_factory_.NewCallback(
        &FileSystemOperation::DidFinishFileOperation));
}

void FileSystemOperation::Move(const GURL& src_path,
                               const GURL& dest_path) {
#ifndef NDEBUG
  DCHECK(kOperationNone == pending_operation_);
  pending_operation_ = kOperationMove;
#endif
  GURL src_origin_url;
  GURL dest_origin_url;
  FileSystemType src_type;
  FileSystemType dest_type;
  FileSystemFileUtil* src_file_util;
  FileSystemFileUtil* dest_file_util;

  if (!VerifyFileSystemPathForWrite(src_path, false, &src_origin_url, &src_type,
          &src_virtual_path_, &src_file_util) ||
      !VerifyFileSystemPathForWrite(dest_path, true /* create */,
          &dest_origin_url, &dest_type, &dest_virtual_path_, &dest_file_util)) {
    delete this;
    return;
  }
  file_system_operation_context_.set_src_origin_url(src_origin_url);
  file_system_operation_context_.set_dest_origin_url(dest_origin_url);
  file_system_operation_context_.set_src_type(src_type);
  file_system_operation_context_.set_dest_type(dest_type);
  if (!file_system_operation_context_.src_file_util())
    file_system_operation_context_.set_src_file_util(src_file_util);
  if (!file_system_operation_context_.dest_file_util())
    file_system_operation_context_.set_dest_file_util(dest_file_util);

  GetUsageAndQuotaThenCallback(dest_origin_url, callback_factory_.NewCallback(
      &FileSystemOperation::DelayedMoveForQuota));
}

void FileSystemOperation::DelayedMoveForQuota(quota::QuotaStatusCode status,
                                              int64 usage, int64 quota) {
  file_system_operation_context_.set_allowed_bytes_growth(quota - usage);

  quota_util_helper_.reset(new ScopedQuotaUtilHelper(
      file_system_context(),
      file_system_operation_context_.dest_origin_url(),
      file_system_operation_context_.dest_type()));

  FileSystemFileUtilProxy::Move(
      file_system_operation_context_,
      proxy_,
      src_virtual_path_,
      dest_virtual_path_,
      callback_factory_.NewCallback(
        &FileSystemOperation::DidFinishFileOperation));
}

void FileSystemOperation::DirectoryExists(const GURL& path) {
#ifndef NDEBUG
  DCHECK(kOperationNone == pending_operation_);
  pending_operation_ = kOperationDirectoryExists;
#endif

  FilePath virtual_path;
  GURL origin_url;
  FileSystemType type;
  FileSystemFileUtil* file_util;
  if (!VerifyFileSystemPathForRead(path, &origin_url, &type, &virtual_path,
      &file_util)) {
    delete this;
    return;
  }
  file_system_operation_context_.set_src_origin_url(origin_url);
  file_system_operation_context_.set_src_type(type);
  if (!file_system_operation_context_.src_file_util())
    file_system_operation_context_.set_src_file_util(file_util);
  FileSystemFileUtilProxy::GetFileInfo(
      file_system_operation_context_,
      proxy_, virtual_path, callback_factory_.NewCallback(
          &FileSystemOperation::DidDirectoryExists));
}

void FileSystemOperation::FileExists(const GURL& path) {
#ifndef NDEBUG
  DCHECK(kOperationNone == pending_operation_);
  pending_operation_ = kOperationFileExists;
#endif

  FilePath virtual_path;
  GURL origin_url;
  FileSystemType type;
  FileSystemFileUtil* file_util;
  if (!VerifyFileSystemPathForRead(path, &origin_url, &type, &virtual_path,
      &file_util)) {
    delete this;
    return;
  }
  file_system_operation_context_.set_src_origin_url(origin_url);
  file_system_operation_context_.set_src_type(type);
  if (!file_system_operation_context_.src_file_util())
    file_system_operation_context_.set_src_file_util(file_util);
  FileSystemFileUtilProxy::GetFileInfo(
      file_system_operation_context_,
      proxy_, virtual_path, callback_factory_.NewCallback(
          &FileSystemOperation::DidFileExists));
}

void FileSystemOperation::GetMetadata(const GURL& path) {
#ifndef NDEBUG
  DCHECK(kOperationNone == pending_operation_);
  pending_operation_ = kOperationGetMetadata;
#endif

  FilePath virtual_path;
  GURL origin_url;
  FileSystemType type;
  FileSystemFileUtil* file_util;
  if (!VerifyFileSystemPathForRead(path, &origin_url, &type, &virtual_path,
      &file_util)) {
    delete this;
    return;
  }
  file_system_operation_context_.set_src_origin_url(origin_url);
  file_system_operation_context_.set_src_type(type);
  if (!file_system_operation_context_.src_file_util())
    file_system_operation_context_.set_src_file_util(file_util);
  FileSystemFileUtilProxy::GetFileInfo(
      file_system_operation_context_,
      proxy_, virtual_path, callback_factory_.NewCallback(
          &FileSystemOperation::DidGetMetadata));
}

void FileSystemOperation::ReadDirectory(const GURL& path) {
#ifndef NDEBUG
  DCHECK(kOperationNone == pending_operation_);
  pending_operation_ = kOperationReadDirectory;
#endif

  FilePath virtual_path;
  GURL origin_url;
  FileSystemType type;
  FileSystemFileUtil* file_util;
  if (!VerifyFileSystemPathForRead(path, &origin_url, &type, &virtual_path,
      &file_util)) {
    delete this;
    return;
  }
  file_system_operation_context_.set_src_origin_url(origin_url);
  file_system_operation_context_.set_src_type(type);
  if (!file_system_operation_context_.src_file_util())
    file_system_operation_context_.set_src_file_util(file_util);
  FileSystemFileUtilProxy::ReadDirectory(
      file_system_operation_context_,
      proxy_, virtual_path, callback_factory_.NewCallback(
          &FileSystemOperation::DidReadDirectory));
}

void FileSystemOperation::Remove(const GURL& path, bool recursive) {
#ifndef NDEBUG
  DCHECK(kOperationNone == pending_operation_);
  pending_operation_ = kOperationRemove;
#endif

  FilePath virtual_path;
  GURL origin_url;
  FileSystemType type;
  FileSystemFileUtil* file_util;
  if (!VerifyFileSystemPathForWrite(path, false /* create */, &origin_url,
      &type, &virtual_path, &file_util)) {
    delete this;
    return;
  }
  file_system_operation_context_.set_src_origin_url(origin_url);
  file_system_operation_context_.set_src_type(type);
  if (!file_system_operation_context_.src_file_util())
    file_system_operation_context_.set_src_file_util(file_util);
  FileSystemFileUtilProxy::Delete(
      file_system_operation_context_,
      proxy_, virtual_path, recursive, callback_factory_.NewCallback(
          &FileSystemOperation::DidFinishFileOperation));
}

void FileSystemOperation::Write(
    scoped_refptr<net::URLRequestContext> url_request_context,
    const GURL& path,
    const GURL& blob_url,
    int64 offset) {
#ifndef NDEBUG
  DCHECK(kOperationNone == pending_operation_);
  pending_operation_ = kOperationWrite;
#endif
  GURL origin_url;
  FileSystemType type;
  FileSystemFileUtil* file_util;
  if (!VerifyFileSystemPathForWrite(path, true /* create */, &origin_url,
      &type, &src_virtual_path_, &file_util)) {
    delete this;
    return;
  }
  file_system_operation_context_.set_src_origin_url(origin_url);
  file_system_operation_context_.set_src_type(type);
  if (!file_system_operation_context_.src_file_util())
    file_system_operation_context_.set_src_file_util(file_util);
  DCHECK(blob_url.is_valid());
  file_writer_delegate_.reset(new FileWriterDelegate(this, offset, proxy_));
  blob_request_.reset(
      new net::URLRequest(blob_url, file_writer_delegate_.get()));
  blob_request_->set_context(url_request_context);

  GetUsageAndQuotaThenCallback(origin_url, callback_factory_.NewCallback(
      &FileSystemOperation::DelayedWriteForQuota));
}

void FileSystemOperation::DelayedWriteForQuota(quota::QuotaStatusCode status,
                                               int64 usage, int64 quota) {
  file_system_operation_context_.set_allowed_bytes_growth(quota - usage);

  quota_util_helper_.reset(new ScopedQuotaUtilHelper(
      file_system_context(),
      file_system_operation_context_.src_origin_url(),
      file_system_operation_context_.src_type()));

  FileSystemFileUtilProxy::CreateOrOpen(
      file_system_operation_context_,
      proxy_,
      src_virtual_path_,
      base::PLATFORM_FILE_OPEN | base::PLATFORM_FILE_WRITE |
          base::PLATFORM_FILE_ASYNC,
      callback_factory_.NewCallback(
          &FileSystemOperation::OnFileOpenedForWrite));
}

void FileSystemOperation::Truncate(const GURL& path, int64 length) {
#ifndef NDEBUG
  DCHECK(kOperationNone == pending_operation_);
  pending_operation_ = kOperationTruncate;
#endif
  GURL origin_url;
  FileSystemType type;
  FileSystemFileUtil* file_util;
  if (!VerifyFileSystemPathForWrite(path, false /* create */, &origin_url,
      &type, &src_virtual_path_, &file_util)) {
    delete this;
    return;
  }
  file_system_operation_context_.set_src_origin_url(origin_url);
  file_system_operation_context_.set_src_type(type);
  if (!file_system_operation_context_.src_file_util())
    file_system_operation_context_.set_src_file_util(file_util);
  length_ = length;

  GetUsageAndQuotaThenCallback(origin_url, callback_factory_.NewCallback(
      &FileSystemOperation::DelayedTruncateForQuota));
}

void FileSystemOperation::DelayedTruncateForQuota(quota::QuotaStatusCode status,
                                                  int64 usage, int64 quota) {
  file_system_operation_context_.set_allowed_bytes_growth(quota - usage);

  quota_util_helper_.reset(new ScopedQuotaUtilHelper(
      file_system_context(),
      file_system_operation_context_.src_origin_url(),
      file_system_operation_context_.src_type()));

  FileSystemFileUtilProxy::Truncate(
      file_system_operation_context_,
      proxy_,
      src_virtual_path_,
      length_, callback_factory_.NewCallback(
          &FileSystemOperation::DidFinishFileOperation));
}

void FileSystemOperation::TouchFile(const GURL& path,
                                    const base::Time& last_access_time,
                                    const base::Time& last_modified_time) {
#ifndef NDEBUG
  DCHECK(kOperationNone == pending_operation_);
  pending_operation_ = kOperationTouchFile;
#endif

  FilePath virtual_path;
  GURL origin_url;
  FileSystemType type;
  FileSystemFileUtil* file_util;
  if (!VerifyFileSystemPathForWrite(path, true /* create */, &origin_url,
      &type, &virtual_path, &file_util)) {
    delete this;
    return;
  }
  file_system_operation_context_.set_src_origin_url(origin_url);
  file_system_operation_context_.set_src_type(type);
  if (!file_system_operation_context_.src_file_util())
    file_system_operation_context_.set_src_file_util(file_util);
  FileSystemFileUtilProxy::Touch(
      file_system_operation_context_,
      proxy_, virtual_path, last_access_time, last_modified_time,
      callback_factory_.NewCallback(&FileSystemOperation::DidTouchFile));
}

void FileSystemOperation::OpenFile(const GURL& path,
                                   int file_flags,
                                   base::ProcessHandle peer_handle) {
#ifndef NDEBUG
  DCHECK(kOperationNone == pending_operation_);
  pending_operation_ = kOperationOpenFile;
#endif

  peer_handle_ = peer_handle;
  GURL origin_url;
  FileSystemType type;
  FileSystemFileUtil* file_util;
  if (file_flags & (
      (base::PLATFORM_FILE_ENUMERATE | base::PLATFORM_FILE_TEMPORARY |
       base::PLATFORM_FILE_HIDDEN))) {
    delete this;
    return;
  }
  if (file_flags &
      (base::PLATFORM_FILE_CREATE | base::PLATFORM_FILE_OPEN_ALWAYS |
       base::PLATFORM_FILE_CREATE_ALWAYS | base::PLATFORM_FILE_OPEN_TRUNCATED |
       base::PLATFORM_FILE_WRITE | base::PLATFORM_FILE_EXCLUSIVE_WRITE |
       base::PLATFORM_FILE_DELETE_ON_CLOSE |
       base::PLATFORM_FILE_WRITE_ATTRIBUTES)) {
    if (!VerifyFileSystemPathForWrite(path, true /* create */, &origin_url,
        &type, &src_virtual_path_, &file_util)) {
      delete this;
      return;
    }
  } else {
    if (!VerifyFileSystemPathForRead(path, &origin_url, &type,
        &src_virtual_path_, &file_util)) {
      delete this;
      return;
    }
  }
  file_system_operation_context_.set_src_origin_url(origin_url);
  file_system_operation_context_.set_src_type(type);
  if (!file_system_operation_context_.src_file_util())
    file_system_operation_context_.set_src_file_util(file_util);
  file_flags_ = file_flags;

  GetUsageAndQuotaThenCallback(origin_url, callback_factory_.NewCallback(
      &FileSystemOperation::DelayedOpenFileForQuota));
}

void FileSystemOperation::DelayedOpenFileForQuota(quota::QuotaStatusCode status,
                                                  int64 usage, int64 quota) {
  file_system_operation_context_.set_allowed_bytes_growth(quota - usage);

  quota_util_helper_.reset(new ScopedQuotaUtilHelper(
      file_system_context(),
      file_system_operation_context_.src_origin_url(),
      file_system_operation_context_.src_type()));

  FileSystemFileUtilProxy::CreateOrOpen(
      file_system_operation_context_,
      proxy_,
      src_virtual_path_,
      file_flags_,
      callback_factory_.NewCallback(
          &FileSystemOperation::DidOpenFile));
}

// We can only get here on a write or truncate that's not yet completed.
// We don't support cancelling any other operation at this time.
void FileSystemOperation::Cancel(FileSystemOperation* cancel_operation_ptr) {
  scoped_ptr<FileSystemOperation> cancel_operation(cancel_operation_ptr);
  if (file_writer_delegate_.get()) {
#ifndef NDEBUG
    DCHECK(kOperationWrite == pending_operation_);
#endif
    // Writes are done without proxying through FileUtilProxy after the initial
    // opening of the PlatformFile.  All state changes are done on this thread,
    // so we're guaranteed to be able to shut down atomically.  We do need to
    // check that the file has been opened [which means the blob_request_ has
    // been created], so we know how much we need to do.
    if (blob_request_.get())
      // This halts any calls to file_writer_delegate_ from blob_request_.
      blob_request_->Cancel();

    dispatcher_->DidFail(base::PLATFORM_FILE_ERROR_ABORT);
    cancel_operation->dispatcher_->DidSucceed();
    delete this;
  } else {
#ifndef NDEBUG
    DCHECK(kOperationTruncate == pending_operation_);
#endif
    // We're cancelling a truncate operation, but we can't actually stop it
    // since it's been proxied to another thread.  We need to save the
    // cancel_operation so that when the truncate returns, it can see that it's
    // been cancelled, report it, and report that the cancel has succeeded.
    DCHECK(!cancel_operation_.get());
    cancel_operation_.swap(cancel_operation);
  }
}

void FileSystemOperation::GetUsageAndQuotaThenCallback(
    const GURL& origin_url,
    quota::QuotaManager::GetUsageAndQuotaCallback* callback) {
  quota::QuotaManagerProxy* quota_manager_proxy =
      file_system_context()->quota_manager_proxy();
  if (!quota_manager_proxy ||
      !file_system_context()->GetQuotaUtil(
          file_system_operation_context_.src_type())) {
    // If we don't have the quota manager or the requested filesystem type
    // does not support quota, we should be able to let it go.
    callback->Run(quota::kQuotaStatusOk, 0, kint64max);
    delete callback;
    return;
  }
  DCHECK(quota_manager_proxy);
  DCHECK(quota_manager_proxy->quota_manager());
  quota_manager_proxy->quota_manager()->GetUsageAndQuota(
      file_system_operation_context_.src_origin_url(),
      FileSystemTypeToQuotaStorageType(
          file_system_operation_context_.src_type()),
      callback);
}

void FileSystemOperation::DidGetRootPath(
    bool success,
    const FilePath& path, const std::string& name) {
  DCHECK(success || path.empty());
  GURL result;
  // We ignore the path, and return a URL instead.  The point was just to verify
  // that we could create/find the path.
  if (success) {
    result = GetFileSystemRootURI(
        file_system_operation_context_.src_origin_url(),
        file_system_operation_context_.src_type());
  }
  dispatcher_->DidOpenFileSystem(name, result);
  delete this;
}

void FileSystemOperation::DidEnsureFileExistsExclusive(
    base::PlatformFileError rv, bool created) {
  if (rv == base::PLATFORM_FILE_OK && !created) {
    dispatcher_->DidFail(base::PLATFORM_FILE_ERROR_EXISTS);
    delete this;
  } else {
    DidFinishFileOperation(rv);
  }
}

void FileSystemOperation::DidEnsureFileExistsNonExclusive(
    base::PlatformFileError rv, bool /* created */) {
  DidFinishFileOperation(rv);
}

void FileSystemOperation::DidFinishFileOperation(
    base::PlatformFileError rv) {
  if (cancel_operation_.get()) {
#ifndef NDEBUG
    DCHECK(kOperationTruncate == pending_operation_);
#endif

    dispatcher_->DidFail(base::PLATFORM_FILE_ERROR_ABORT);
    cancel_operation_->dispatcher_->DidSucceed();
  } else if (rv == base::PLATFORM_FILE_OK) {
    dispatcher_->DidSucceed();
  } else {
    dispatcher_->DidFail(rv);
  }
  delete this;
}

void FileSystemOperation::DidDirectoryExists(
    base::PlatformFileError rv,
    const base::PlatformFileInfo& file_info,
    const FilePath& unused) {
  if (rv == base::PLATFORM_FILE_OK) {
    if (file_info.is_directory)
      dispatcher_->DidSucceed();
    else
      dispatcher_->DidFail(base::PLATFORM_FILE_ERROR_NOT_A_DIRECTORY);
  } else {
    dispatcher_->DidFail(rv);
  }
  delete this;
}

void FileSystemOperation::DidFileExists(
    base::PlatformFileError rv,
    const base::PlatformFileInfo& file_info,
    const FilePath& unused) {
  if (rv == base::PLATFORM_FILE_OK) {
    if (file_info.is_directory)
      dispatcher_->DidFail(base::PLATFORM_FILE_ERROR_NOT_A_FILE);
    else
      dispatcher_->DidSucceed();
  } else {
    dispatcher_->DidFail(rv);
  }
  delete this;
}

void FileSystemOperation::DidGetMetadata(
    base::PlatformFileError rv,
    const base::PlatformFileInfo& file_info,
    const FilePath& platform_path) {
  if (rv == base::PLATFORM_FILE_OK)
    dispatcher_->DidReadMetadata(file_info, platform_path);
  else
    dispatcher_->DidFail(rv);
  delete this;
}

void FileSystemOperation::DidReadDirectory(
    base::PlatformFileError rv,
    const std::vector<base::FileUtilProxy::Entry>& entries) {

  if (rv == base::PLATFORM_FILE_OK)
    dispatcher_->DidReadDirectory(entries, false /* has_more */);
  else
    dispatcher_->DidFail(rv);
  delete this;
}

void FileSystemOperation::DidWrite(
    base::PlatformFileError rv,
    int64 bytes,
    bool complete) {
  if (rv == base::PLATFORM_FILE_OK)
    dispatcher_->DidWrite(bytes, complete);
  else
    dispatcher_->DidFail(rv);
  if (complete || rv != base::PLATFORM_FILE_OK)
    delete this;
}

void FileSystemOperation::DidTouchFile(base::PlatformFileError rv) {
  if (rv == base::PLATFORM_FILE_OK)
    dispatcher_->DidSucceed();
  else
    dispatcher_->DidFail(rv);
  delete this;
}

void FileSystemOperation::DidOpenFile(
    base::PlatformFileError rv,
    base::PassPlatformFile file,
    bool unused) {
  if (rv == base::PLATFORM_FILE_OK)
    dispatcher_->DidOpenFile(file.ReleaseValue(), peer_handle_);
  else
    dispatcher_->DidFail(rv);
  delete this;
}

void FileSystemOperation::OnFileOpenedForWrite(
    base::PlatformFileError rv,
    base::PassPlatformFile file,
    bool created) {
  if (base::PLATFORM_FILE_OK != rv) {
    dispatcher_->DidFail(rv);
    delete this;
    return;
  }
  file_writer_delegate_->Start(file.ReleaseValue(), blob_request_.get());
}

bool FileSystemOperation::VerifyFileSystemPathForRead(
    const GURL& path, GURL* origin_url, FileSystemType* type,
    FilePath* virtual_path, FileSystemFileUtil** file_util) {

  // If we have no context, we just allow any operations, for testing.
  // TODO(ericu): Revisit this hack for security.
  if (!file_system_context()) {
#ifdef OS_WIN
    // On Windows, the path will look like /C:/foo/bar; we need to remove the
    // leading slash to make it valid.  But if it's empty, we shouldn't do
    // anything.
    std::string temp = net::UnescapeURLComponent(path.path(),
        UnescapeRule::SPACES | UnescapeRule::URL_SPECIAL_CHARS);
    if (temp.size())
      temp = temp.substr(1);
    *virtual_path = FilePath(UTF8ToWide(temp)).NormalizeWindowsPathSeparators();
#else
    *virtual_path = FilePath(path.path());
#endif
    *type = file_system_operation_context_.src_type();
    *origin_url = file_system_operation_context_.src_origin_url();
    *file_util = NULL;
    return true;
  }

  // We may want do more checks, but for now it just checks if the given
  // URL is valid.
  if (!CrackFileSystemURL(path, origin_url, type, virtual_path)) {
    dispatcher_->DidFail(base::PLATFORM_FILE_ERROR_INVALID_URL);
    return false;
  }
  if (!file_system_context()->path_manager()->IsAccessAllowed(
      *origin_url, *type, *virtual_path)) {
    dispatcher_->DidFail(base::PLATFORM_FILE_ERROR_SECURITY);
    return false;
  }
  DCHECK(file_util);
  *file_util = file_system_context()->path_manager()->GetFileUtil(*type);
  DCHECK(*file_util);

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

  return true;
}

bool FileSystemOperation::VerifyFileSystemPathForWrite(
    const GURL& path, bool create, GURL* origin_url, FileSystemType* type,
    FilePath* virtual_path, FileSystemFileUtil** file_util) {

  // If we have no context, we just allow any operations, for testing.
  // TODO(ericu): Revisit this hack for security.
  if (!file_system_context()) {
#ifdef OS_WIN
    // On Windows, the path will look like /C:/foo/bar; we need to remove the
    // leading slash to make it valid.  But if it's empty, we shouldn't do
    // anything.
    std::string temp = net::UnescapeURLComponent(path.path(),
        UnescapeRule::SPACES | UnescapeRule::URL_SPECIAL_CHARS);
    if (temp.size())
      temp = temp.substr(1);
    *virtual_path = FilePath(UTF8ToWide(temp)).NormalizeWindowsPathSeparators();
#else
    *virtual_path = FilePath(path.path());
#endif
    *type = file_system_operation_context_.dest_type();
    *origin_url = file_system_operation_context_.dest_origin_url();
    *file_util = NULL;
    return true;
  }

  if (!CrackFileSystemURL(path, origin_url, type, virtual_path)) {
    dispatcher_->DidFail(base::PLATFORM_FILE_ERROR_INVALID_URL);
    return false;
  }
  if (!file_system_context()->path_manager()->IsAccessAllowed(
      *origin_url, *type, *virtual_path)) {
    dispatcher_->DidFail(base::PLATFORM_FILE_ERROR_SECURITY);
    return false;
  }
  // Any write access is disallowed on the root path.
  if (virtual_path->value().length() == 0 ||
      virtual_path->DirName().value() == virtual_path->value()) {
    dispatcher_->DidFail(base::PLATFORM_FILE_ERROR_SECURITY);
    return false;
  }
  if (create && file_system_context()->path_manager()->IsRestrictedFileName(
          *type, virtual_path->BaseName())) {
    dispatcher_->DidFail(base::PLATFORM_FILE_ERROR_SECURITY);
    return false;
  }
  DCHECK(file_util);
  *file_util = file_system_context()->path_manager()->GetFileUtil(*type);
  DCHECK(*file_util);

  return true;
}

}  // namespace fileapi
