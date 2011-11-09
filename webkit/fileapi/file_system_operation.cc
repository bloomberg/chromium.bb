// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/file_system_operation.h"

#include "base/bind.h"
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
    FileSystemContext* file_system_context)
    : proxy_(proxy),
      dispatcher_(dispatcher),
      operation_context_(file_system_context, NULL) {
#ifndef NDEBUG
  pending_operation_ = kOperationNone;
#endif
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

void FileSystemOperation::OpenFileSystem(
    const GURL& origin_url, fileapi::FileSystemType type, bool create) {
#ifndef NDEBUG
  DCHECK(kOperationNone == pending_operation_);
  pending_operation_ = static_cast<FileSystemOperation::OperationType>(
      kOperationOpenFileSystem);
#endif

  DCHECK(file_system_context());
  operation_context_.set_src_origin_url(origin_url);
  operation_context_.set_src_type(type);
  // TODO(ericu): We don't really need to make this call if !create.
  // Also, in the future we won't need it either way, as long as we do all
  // permission+quota checks beforehand.  We only need it now because we have to
  // create an unpredictable directory name.  Without that, we could lazily
  // create the root later on the first filesystem write operation, and just
  // return GetFileSystemRootURI() here.
  file_system_context()->path_manager()->ValidateFileSystemRootAndGetURL(
      origin_url, type, create,
      base::Bind(&FileSystemOperation::DidGetRootPath,
                 base::Owned(this)));
}

void FileSystemOperation::CreateFile(const GURL& path,
                                     bool exclusive) {
#ifndef NDEBUG
  DCHECK(kOperationNone == pending_operation_);
  pending_operation_ = kOperationCreateFile;
#endif
  if (!SetupSrcContextForWrite(path, true)) {
    delete this;
    return;
  }
  GetUsageAndQuotaThenCallback(
      operation_context_.src_origin_url(),
      base::Bind(&FileSystemOperation::DelayedCreateFileForQuota,
                 base::Unretained(this), exclusive));
}

void FileSystemOperation::DelayedCreateFileForQuota(
    bool exclusive,
    quota::QuotaStatusCode status, int64 usage, int64 quota) {
  operation_context_.set_allowed_bytes_growth(quota - usage);

  quota_util_helper_.reset(new ScopedQuotaUtilHelper(
      file_system_context(),
      operation_context_.src_origin_url(),
      operation_context_.src_type()));

  FileSystemFileUtilProxy::EnsureFileExists(
      operation_context_,
      proxy_,
      src_virtual_path_,
      base::Bind(
          exclusive ? &FileSystemOperation::DidEnsureFileExistsExclusive
                    : &FileSystemOperation::DidEnsureFileExistsNonExclusive,
          base::Owned(this)));
}

void FileSystemOperation::CreateDirectory(const GURL& path,
                                          bool exclusive,
                                          bool recursive) {
#ifndef NDEBUG
  DCHECK(kOperationNone == pending_operation_);
  pending_operation_ = kOperationCreateDirectory;
#endif
  if (!SetupSrcContextForWrite(path, true)) {
    delete this;
    return;
  }
  GetUsageAndQuotaThenCallback(
      operation_context_.src_origin_url(),
      base::Bind(&FileSystemOperation::DelayedCreateDirectoryForQuota,
                 base::Unretained(this), exclusive, recursive));
}

void FileSystemOperation::DelayedCreateDirectoryForQuota(
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
                 base::Owned(this)));
}

void FileSystemOperation::Copy(const GURL& src_path,
                               const GURL& dest_path) {
#ifndef NDEBUG
  DCHECK(kOperationNone == pending_operation_);
  pending_operation_ = kOperationCopy;
#endif
  if (!SetupSrcContextForRead(src_path) ||
      !SetupDestContextForWrite(dest_path, true)) {
    delete this;
    return;
  }

  GetUsageAndQuotaThenCallback(
      operation_context_.dest_origin_url(),
      base::Bind(&FileSystemOperation::DelayedCopyForQuota,
                 base::Unretained(this)));
}

void FileSystemOperation::DelayedCopyForQuota(quota::QuotaStatusCode status,
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
                 base::Owned(this)));
}

void FileSystemOperation::Move(const GURL& src_path,
                               const GURL& dest_path) {
#ifndef NDEBUG
  DCHECK(kOperationNone == pending_operation_);
  pending_operation_ = kOperationMove;
#endif
  if (!SetupSrcContextForWrite(src_path, false) ||
      !SetupDestContextForWrite(dest_path, true)) {
    delete this;
    return;
  }

  GetUsageAndQuotaThenCallback(
      operation_context_.dest_origin_url(),
      base::Bind(&FileSystemOperation::DelayedMoveForQuota,
                 base::Unretained(this)));
}

void FileSystemOperation::DelayedMoveForQuota(quota::QuotaStatusCode status,
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
                 base::Owned(this)));
}

void FileSystemOperation::DirectoryExists(const GURL& path) {
#ifndef NDEBUG
  DCHECK(kOperationNone == pending_operation_);
  pending_operation_ = kOperationDirectoryExists;
#endif
  if (!SetupSrcContextForRead(path)) {
    delete this;
    return;
  }

  FileSystemFileUtilProxy::GetFileInfo(
      operation_context_, proxy_, src_virtual_path_,
      base::Bind(&FileSystemOperation::DidDirectoryExists, base::Owned(this)));
}

void FileSystemOperation::FileExists(const GURL& path) {
#ifndef NDEBUG
  DCHECK(kOperationNone == pending_operation_);
  pending_operation_ = kOperationFileExists;
#endif
  if (!SetupSrcContextForRead(path)) {
    delete this;
    return;
  }

  FileSystemFileUtilProxy::GetFileInfo(
      operation_context_, proxy_, src_virtual_path_,
      base::Bind(&FileSystemOperation::DidFileExists, base::Owned(this)));
}

void FileSystemOperation::GetMetadata(const GURL& path) {
#ifndef NDEBUG
  DCHECK(kOperationNone == pending_operation_);
  pending_operation_ = kOperationGetMetadata;
#endif
  if (!SetupSrcContextForRead(path)) {
    delete this;
    return;
  }

  FileSystemFileUtilProxy::GetFileInfo(
      operation_context_, proxy_, src_virtual_path_,
      base::Bind(&FileSystemOperation::DidGetMetadata, base::Owned(this)));
}

void FileSystemOperation::ReadDirectory(const GURL& path) {
#ifndef NDEBUG
  DCHECK(kOperationNone == pending_operation_);
  pending_operation_ = kOperationReadDirectory;
#endif
  if (!SetupSrcContextForRead(path)) {
    delete this;
    return;
  }

  FileSystemFileUtilProxy::ReadDirectory(
      operation_context_, proxy_, src_virtual_path_,
      base::Bind(&FileSystemOperation::DidReadDirectory, base::Owned(this)));
}

void FileSystemOperation::Remove(const GURL& path, bool recursive) {
#ifndef NDEBUG
  DCHECK(kOperationNone == pending_operation_);
  pending_operation_ = kOperationRemove;
#endif
  if (!SetupSrcContextForWrite(path, false)) {
    delete this;
    return;
  }

  base::FileUtilProxy::RelayFileTask(
      proxy_, FROM_HERE,
      base::Bind(&FileSystemFileUtil::Delete,
                 base::Unretained(operation_context_.src_file_util()),
                 &operation_context_,
                 src_virtual_path_, recursive),
      base::Bind(&FileSystemOperation::DidFinishFileOperation,
                 base::Owned(this)));
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
  if (!SetupSrcContextForWrite(path, true)) {
    delete this;
    return;
  }
  DCHECK(blob_url.is_valid());
  file_writer_delegate_.reset(new FileWriterDelegate(this, offset, proxy_));
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
                 &operation_context_,
                 src_virtual_path_, file_flags),
      base::Bind(&FileSystemFileUtil::Close,
                 base::Unretained(operation_context_.src_file_util()),
                 &operation_context_),
      base::Bind(&FileSystemOperation::OnFileOpenedForWrite,
                 base::Unretained(this)));
}

void FileSystemOperation::Truncate(const GURL& path, int64 length) {
#ifndef NDEBUG
  DCHECK(kOperationNone == pending_operation_);
  pending_operation_ = kOperationTruncate;
#endif
  if (!SetupSrcContextForWrite(path, false)) {
    delete this;
    return;
  }
  GetUsageAndQuotaThenCallback(
      operation_context_.src_origin_url(),
      base::Bind(&FileSystemOperation::DelayedTruncateForQuota,
                 base::Unretained(this), length));
}

void FileSystemOperation::DelayedTruncateForQuota(int64 length,
                                                  quota::QuotaStatusCode status,
                                                  int64 usage, int64 quota) {
  operation_context_.set_allowed_bytes_growth(quota - usage);

  quota_util_helper_.reset(new ScopedQuotaUtilHelper(
      file_system_context(),
      operation_context_.src_origin_url(),
      operation_context_.src_type()));

  base::FileUtilProxy::RelayFileTask(
      proxy_, FROM_HERE,
      base::Bind(&FileSystemFileUtil::Truncate,
                 base::Unretained(operation_context_.src_file_util()),
                 &operation_context_,
                 src_virtual_path_, length),
      base::Bind(&FileSystemOperation::DidFinishFileOperation,
                 base::Owned(this)));
}

void FileSystemOperation::TouchFile(const GURL& path,
                                    const base::Time& last_access_time,
                                    const base::Time& last_modified_time) {
#ifndef NDEBUG
  DCHECK(kOperationNone == pending_operation_);
  pending_operation_ = kOperationTouchFile;
#endif
  if (!SetupSrcContextForWrite(path, true)) {
    delete this;
    return;
  }

  base::FileUtilProxy::RelayFileTask(
      proxy_, FROM_HERE,
      base::Bind(&FileSystemFileUtil::Touch,
                 base::Unretained(operation_context_.src_file_util()),
                 &operation_context_,
                 src_virtual_path_, last_access_time, last_modified_time),
      base::Bind(&FileSystemOperation::DidTouchFile, base::Owned(this)));
}

void FileSystemOperation::OpenFile(const GURL& path,
                                   int file_flags,
                                   base::ProcessHandle peer_handle) {
#ifndef NDEBUG
  DCHECK(kOperationNone == pending_operation_);
  pending_operation_ = kOperationOpenFile;
#endif
  peer_handle_ = peer_handle;

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
    if (!SetupSrcContextForWrite(path, true)) {
      delete this;
      return;
    }
  } else {
    if (!SetupSrcContextForRead(path)) {
      delete this;
      return;
    }
  }
  GetUsageAndQuotaThenCallback(
      operation_context_.src_origin_url(),
      base::Bind(&FileSystemOperation::DelayedOpenFileForQuota,
                 base::Unretained(this), file_flags));
}

void FileSystemOperation::DelayedOpenFileForQuota(int file_flags,
                                                  quota::QuotaStatusCode status,
                                                  int64 usage, int64 quota) {
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
      base::Bind(&FileSystemOperation::DidOpenFile, base::Owned(this)));
}

void FileSystemOperation::SyncGetPlatformPath(const GURL& path,
                                              FilePath* platform_path) {
#ifndef NDEBUG
  DCHECK(kOperationNone == pending_operation_);
  pending_operation_ = kOperationGetLocalPath;
#endif
  if (!SetupSrcContextForRead(path)) {
    delete this;
    return;
  }

  operation_context_.src_file_util()->GetLocalFilePath(
      &operation_context_, src_virtual_path_, platform_path);

  delete this;
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

    if (dispatcher_.get())
      dispatcher_->DidFail(base::PLATFORM_FILE_ERROR_ABORT);
    cancel_operation->dispatcher_->DidSucceed();
    dispatcher_.reset();
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

void FileSystemOperation::DidGetRootPath(
    bool success,
    const FilePath& path, const std::string& name) {
  DCHECK(success || path.empty());
  GURL result;
  if (!dispatcher_.get())
    return;
  // We ignore the path, and return a URL instead.  The point was just to verify
  // that we could create/find the path.
  if (success) {
    result = GetFileSystemRootURI(
        operation_context_.src_origin_url(),
        operation_context_.src_type());
  }
  dispatcher_->DidOpenFileSystem(name, result);
}

void FileSystemOperation::DidEnsureFileExistsExclusive(
    base::PlatformFileError rv, bool created) {
  if (rv == base::PLATFORM_FILE_OK && !created) {
    if (dispatcher_.get())
      dispatcher_->DidFail(base::PLATFORM_FILE_ERROR_EXISTS);
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

    if (dispatcher_.get())
      dispatcher_->DidFail(base::PLATFORM_FILE_ERROR_ABORT);
    cancel_operation_->dispatcher_->DidSucceed();
  } else if (dispatcher_.get()) {
    if (rv == base::PLATFORM_FILE_OK)
      dispatcher_->DidSucceed();
    else
      dispatcher_->DidFail(rv);
  }
}

void FileSystemOperation::DidDirectoryExists(
    base::PlatformFileError rv,
    const base::PlatformFileInfo& file_info,
    const FilePath& unused) {
  if (!dispatcher_.get())
    return;
  if (rv == base::PLATFORM_FILE_OK) {
    if (file_info.is_directory)
      dispatcher_->DidSucceed();
    else
      dispatcher_->DidFail(base::PLATFORM_FILE_ERROR_NOT_A_DIRECTORY);
  } else {
    dispatcher_->DidFail(rv);
  }
}

void FileSystemOperation::DidFileExists(
    base::PlatformFileError rv,
    const base::PlatformFileInfo& file_info,
    const FilePath& unused) {
  if (!dispatcher_.get())
    return;
  if (rv == base::PLATFORM_FILE_OK) {
    if (file_info.is_directory)
      dispatcher_->DidFail(base::PLATFORM_FILE_ERROR_NOT_A_FILE);
    else
      dispatcher_->DidSucceed();
  } else {
    dispatcher_->DidFail(rv);
  }
}

void FileSystemOperation::DidGetMetadata(
    base::PlatformFileError rv,
    const base::PlatformFileInfo& file_info,
    const FilePath& platform_path) {
  if (!dispatcher_.get())
    return;
  if (rv == base::PLATFORM_FILE_OK)
    dispatcher_->DidReadMetadata(file_info, platform_path);
  else
    dispatcher_->DidFail(rv);
}

void FileSystemOperation::DidReadDirectory(
    base::PlatformFileError rv,
    const std::vector<base::FileUtilProxy::Entry>& entries) {
  if (!dispatcher_.get())
    return;

  if (rv == base::PLATFORM_FILE_OK)
    dispatcher_->DidReadDirectory(entries, false /* has_more */);
  else
    dispatcher_->DidFail(rv);
}

void FileSystemOperation::DidWrite(
    base::PlatformFileError rv,
    int64 bytes,
    bool complete) {
  if (!dispatcher_.get()) {
    delete this;
    return;
  }
  if (rv == base::PLATFORM_FILE_OK)
    dispatcher_->DidWrite(bytes, complete);
  else
    dispatcher_->DidFail(rv);
  if (complete || rv != base::PLATFORM_FILE_OK)
    delete this;
}

void FileSystemOperation::DidTouchFile(base::PlatformFileError rv) {
  if (!dispatcher_.get())
    return;
  if (rv == base::PLATFORM_FILE_OK)
    dispatcher_->DidSucceed();
  else
    dispatcher_->DidFail(rv);
}

void FileSystemOperation::DidOpenFile(
    base::PlatformFileError rv,
    base::PassPlatformFile file,
    bool unused) {
  if (!dispatcher_.get())
    return;
  if (rv == base::PLATFORM_FILE_OK)
    dispatcher_->DidOpenFile(file.ReleaseValue(), peer_handle_);
  else
    dispatcher_->DidFail(rv);
}

void FileSystemOperation::OnFileOpenedForWrite(
    base::PlatformFileError rv,
    base::PassPlatformFile file,
    bool created) {
  if (base::PLATFORM_FILE_OK != rv || !dispatcher_.get()) {
    if (dispatcher_.get())
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
    *type = operation_context_.src_type();
    *origin_url = operation_context_.src_origin_url();
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
    *type = operation_context_.dest_type();
    *origin_url = operation_context_.dest_origin_url();
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

bool FileSystemOperation::SetupSrcContextForRead(const GURL& path) {
  GURL origin_url;
  FileSystemType type;
  FileSystemFileUtil* file_util;
  bool result = VerifyFileSystemPathForRead(
      path, &origin_url, &type, &src_virtual_path_, &file_util);
  operation_context_.set_src_origin_url(origin_url);
  operation_context_.set_src_type(type);
  if (!operation_context_.src_file_util())
    operation_context_.set_src_file_util(file_util);
  return result;
}

bool FileSystemOperation::SetupSrcContextForWrite(const GURL& path,
                                                  bool create) {
  GURL origin_url;
  FileSystemType type;
  FileSystemFileUtil* file_util;
  bool result = VerifyFileSystemPathForWrite(
      path, create, &origin_url, &type, &src_virtual_path_, &file_util);
  operation_context_.set_src_origin_url(origin_url);
  operation_context_.set_src_type(type);
  if (!operation_context_.src_file_util())
    operation_context_.set_src_file_util(file_util);
  return result;
}

bool FileSystemOperation::SetupDestContextForWrite(const GURL& path,
                                                   bool create) {
  GURL origin_url;
  FileSystemType type;
  FileSystemFileUtil* file_util;
  bool result = VerifyFileSystemPathForWrite(
      path, create, &origin_url, &type, &dest_virtual_path_, &file_util);
  operation_context_.set_dest_origin_url(origin_url);
  operation_context_.set_dest_type(type);
  if (!operation_context_.dest_file_util())
    operation_context_.set_dest_file_util(file_util);
  return result;
}

}  // namespace fileapi
