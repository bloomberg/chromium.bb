// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/browser/fileapi/file_system_context.h"

#include "base/bind.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/task_runner_util.h"
#include "url/gurl.h"
#include "webkit/browser/blob/file_stream_reader.h"
#include "webkit/browser/fileapi/copy_or_move_file_validator.h"
#include "webkit/browser/fileapi/external_mount_points.h"
#include "webkit/browser/fileapi/file_permission_policy.h"
#include "webkit/browser/fileapi/file_stream_writer.h"
#include "webkit/browser/fileapi/file_system_file_util.h"
#include "webkit/browser/fileapi/file_system_operation.h"
#include "webkit/browser/fileapi/file_system_operation_runner.h"
#include "webkit/browser/fileapi/file_system_options.h"
#include "webkit/browser/fileapi/file_system_quota_client.h"
#include "webkit/browser/fileapi/file_system_url.h"
#include "webkit/browser/fileapi/isolated_context.h"
#include "webkit/browser/fileapi/isolated_file_system_backend.h"
#include "webkit/browser/fileapi/mount_points.h"
#include "webkit/browser/fileapi/sandbox_file_system_backend.h"
#include "webkit/browser/fileapi/test_file_system_backend.h"
#include "webkit/browser/quota/quota_manager.h"
#include "webkit/browser/quota/special_storage_policy.h"
#include "webkit/common/fileapi/file_system_util.h"

using quota::QuotaClient;

namespace fileapi {

namespace {

QuotaClient* CreateQuotaClient(
    FileSystemContext* context,
    bool is_incognito) {
  return new FileSystemQuotaClient(context, is_incognito);
}

void DidOpenFileSystem(
    const FileSystemContext::OpenFileSystemCallback& callback,
    const GURL& filesystem_root,
    const std::string& filesystem_name,
    base::PlatformFileError error) {
  callback.Run(error, filesystem_name, filesystem_root);
}

}  // namespace

// static
int FileSystemContext::GetPermissionPolicy(FileSystemType type) {
  switch (type) {
    case kFileSystemTypeTemporary:
    case kFileSystemTypePersistent:
    case kFileSystemTypeSyncable:
      return FILE_PERMISSION_SANDBOX;

    case kFileSystemTypeDrive:
    case kFileSystemTypeNativeForPlatformApp:
    case kFileSystemTypeNativeLocal:
      return FILE_PERMISSION_USE_FILE_PERMISSION;

    case kFileSystemTypeRestrictedNativeLocal:
      return FILE_PERMISSION_READ_ONLY |
             FILE_PERMISSION_USE_FILE_PERMISSION;

    // Following types are only accessed via IsolatedFileSystem, and
    // don't have their own permission policies.
    case kFileSystemTypeDeviceMedia:
    case kFileSystemTypeDragged:
    case kFileSystemTypeForTransientFile:
    case kFileSystemTypeItunes:
    case kFileSystemTypeNativeMedia:
    case kFileSystemTypePicasa:
      return FILE_PERMISSION_ALWAYS_DENY;

    // Following types only appear as mount_type, and will not be
    // queried for their permission policies.
    case kFileSystemTypeIsolated:
    case kFileSystemTypeExternal:
      return FILE_PERMISSION_ALWAYS_DENY;

    // Following types should not be used to access files by FileAPI clients.
    case kFileSystemTypeTest:
    case kFileSystemTypeSyncableForInternalSync:
    case kFileSystemInternalTypeEnumEnd:
    case kFileSystemInternalTypeEnumStart:
    case kFileSystemTypeUnknown:
      return FILE_PERMISSION_ALWAYS_DENY;
  }
  NOTREACHED();
  return FILE_PERMISSION_ALWAYS_DENY;
}

FileSystemContext::FileSystemContext(
    base::SingleThreadTaskRunner* io_task_runner,
    base::SequencedTaskRunner* file_task_runner,
    ExternalMountPoints* external_mount_points,
    quota::SpecialStoragePolicy* special_storage_policy,
    quota::QuotaManagerProxy* quota_manager_proxy,
    ScopedVector<FileSystemBackend> additional_backends,
    const base::FilePath& partition_path,
    const FileSystemOptions& options)
    : io_task_runner_(io_task_runner),
      default_file_task_runner_(file_task_runner),
      quota_manager_proxy_(quota_manager_proxy),
      sandbox_context_(new SandboxContext(
          quota_manager_proxy,
          file_task_runner,
          partition_path,
          special_storage_policy,
          options)),
      sandbox_backend_(new SandboxFileSystemBackend(
          sandbox_context_.get())),
      isolated_backend_(new IsolatedFileSystemBackend()),
      additional_backends_(additional_backends.Pass()),
      external_mount_points_(external_mount_points),
      partition_path_(partition_path),
      operation_runner_(new FileSystemOperationRunner(this)) {
  if (quota_manager_proxy) {
    quota_manager_proxy->RegisterClient(CreateQuotaClient(
            this, options.is_incognito()));
  }

  RegisterBackend(sandbox_backend_.get());
  RegisterBackend(isolated_backend_.get());

  for (ScopedVector<FileSystemBackend>::const_iterator iter =
          additional_backends_.begin();
       iter != additional_backends_.end(); ++iter) {
    RegisterBackend(*iter);
  }

  sandbox_backend_->Initialize(this);
  isolated_backend_->Initialize(this);
  for (ScopedVector<FileSystemBackend>::const_iterator iter =
          additional_backends_.begin();
       iter != additional_backends_.end(); ++iter) {
    (*iter)->Initialize(this);
  }

  // Additional mount points must be added before regular system-wide
  // mount points.
  if (external_mount_points)
    url_crackers_.push_back(external_mount_points);
  url_crackers_.push_back(ExternalMountPoints::GetSystemInstance());
  url_crackers_.push_back(IsolatedContext::GetInstance());
}

bool FileSystemContext::DeleteDataForOriginOnFileThread(
    const GURL& origin_url) {
  DCHECK(default_file_task_runner()->RunsTasksOnCurrentThread());
  DCHECK(origin_url == origin_url.GetOrigin());

  bool success = true;
  for (FileSystemBackendMap::iterator iter = backend_map_.begin();
       iter != backend_map_.end();
       ++iter) {
    FileSystemBackend* backend = iter->second;
    if (!backend->GetQuotaUtil())
      continue;
    if (backend->GetQuotaUtil()->DeleteOriginDataOnFileThread(
            this, quota_manager_proxy(), origin_url, iter->first)
            != base::PLATFORM_FILE_OK) {
      // Continue the loop, but record the failure.
      success = false;
    }
  }

  return success;
}

void FileSystemContext::Shutdown() {
  if (!io_task_runner_->RunsTasksOnCurrentThread()) {
    io_task_runner_->PostTask(
        FROM_HERE, base::Bind(&FileSystemContext::Shutdown,
                              make_scoped_refptr(this)));
    return;
  }
  operation_runner_->Shutdown();
}

FileSystemQuotaUtil*
FileSystemContext::GetQuotaUtil(FileSystemType type) const {
  FileSystemBackend* backend = GetFileSystemBackend(type);
  if (!backend)
    return NULL;
  return backend->GetQuotaUtil();
}

AsyncFileUtil* FileSystemContext::GetAsyncFileUtil(
    FileSystemType type) const {
  FileSystemBackend* backend = GetFileSystemBackend(type);
  if (!backend)
    return NULL;
  return backend->GetAsyncFileUtil(type);
}

FileSystemFileUtil* FileSystemContext::GetFileUtil(
    FileSystemType type) const {
  FileSystemBackend* backend = GetFileSystemBackend(type);
  if (!backend)
    return NULL;
  return backend->GetFileUtil(type);
}

CopyOrMoveFileValidatorFactory*
FileSystemContext::GetCopyOrMoveFileValidatorFactory(
    FileSystemType type, base::PlatformFileError* error_code) const {
  DCHECK(error_code);
  *error_code = base::PLATFORM_FILE_OK;
  FileSystemBackend* backend = GetFileSystemBackend(type);
  if (!backend)
    return NULL;
  return backend->GetCopyOrMoveFileValidatorFactory(
      type, error_code);
}

FileSystemBackend* FileSystemContext::GetFileSystemBackend(
    FileSystemType type) const {
  FileSystemBackendMap::const_iterator found = backend_map_.find(type);
  if (found != backend_map_.end())
    return found->second;
  NOTREACHED() << "Unknown filesystem type: " << type;
  return NULL;
}

bool FileSystemContext::IsSandboxFileSystem(FileSystemType type) const {
  return GetQuotaUtil(type) != NULL;
}

const UpdateObserverList* FileSystemContext::GetUpdateObservers(
    FileSystemType type) const {
  FileSystemBackend* backend = GetFileSystemBackend(type);
  if (backend->GetQuotaUtil())
    return backend->GetQuotaUtil()->GetUpdateObservers(type);
  return NULL;
}

const AccessObserverList* FileSystemContext::GetAccessObservers(
    FileSystemType type) const {
  FileSystemBackend* backend = GetFileSystemBackend(type);
  if (backend->GetQuotaUtil())
    return backend->GetQuotaUtil()->GetAccessObservers(type);
  return NULL;
}

void FileSystemContext::GetFileSystemTypes(
    std::vector<FileSystemType>* types) const {
  types->clear();
  for (FileSystemBackendMap::const_iterator iter = backend_map_.begin();
       iter != backend_map_.end(); ++iter)
    types->push_back(iter->first);
}

ExternalFileSystemBackend*
FileSystemContext::external_backend() const {
  return static_cast<ExternalFileSystemBackend*>(
      GetFileSystemBackend(kFileSystemTypeExternal));
}

void FileSystemContext::OpenFileSystem(
    const GURL& origin_url,
    FileSystemType type,
    OpenFileSystemMode mode,
    const OpenFileSystemCallback& callback) {
  DCHECK(!callback.is_null());

  FileSystemBackend* backend = GetFileSystemBackend(type);
  if (!backend) {
    callback.Run(base::PLATFORM_FILE_ERROR_SECURITY, std::string(), GURL());
    return;
  }

  backend->OpenFileSystem(origin_url, type, mode,
                          base::Bind(&DidOpenFileSystem, callback));
}

void FileSystemContext::DeleteFileSystem(
    const GURL& origin_url,
    FileSystemType type,
    const DeleteFileSystemCallback& callback) {
  DCHECK(origin_url == origin_url.GetOrigin());
  FileSystemBackend* backend = GetFileSystemBackend(type);
  if (!backend) {
    callback.Run(base::PLATFORM_FILE_ERROR_SECURITY);
    return;
  }
  if (!backend->GetQuotaUtil()) {
    callback.Run(base::PLATFORM_FILE_ERROR_INVALID_OPERATION);
    return;
  }

  base::PostTaskAndReplyWithResult(
      default_file_task_runner(),
      FROM_HERE,
      // It is safe to pass Unretained(quota_util) since context owns it.
      base::Bind(&FileSystemQuotaUtil::DeleteOriginDataOnFileThread,
                 base::Unretained(backend->GetQuotaUtil()),
                 make_scoped_refptr(this),
                 base::Unretained(quota_manager_proxy()),
                 origin_url,
                 type),
      callback);
}

scoped_ptr<webkit_blob::FileStreamReader>
FileSystemContext::CreateFileStreamReader(
    const FileSystemURL& url,
    int64 offset,
    const base::Time& expected_modification_time) {
  if (!url.is_valid())
    return scoped_ptr<webkit_blob::FileStreamReader>();
  FileSystemBackend* backend = GetFileSystemBackend(url.type());
  if (!backend)
    return scoped_ptr<webkit_blob::FileStreamReader>();
  return backend->CreateFileStreamReader(
      url, offset, expected_modification_time, this);
}

scoped_ptr<FileStreamWriter> FileSystemContext::CreateFileStreamWriter(
    const FileSystemURL& url,
    int64 offset) {
  if (!url.is_valid())
    return scoped_ptr<FileStreamWriter>();
  FileSystemBackend* backend = GetFileSystemBackend(url.type());
  if (!backend)
    return scoped_ptr<FileStreamWriter>();
  return backend->CreateFileStreamWriter(url, offset, this);
}

scoped_ptr<FileSystemOperationRunner>
FileSystemContext::CreateFileSystemOperationRunner() {
  return make_scoped_ptr(new FileSystemOperationRunner(this));
}

FileSystemURL FileSystemContext::CrackURL(const GURL& url) const {
  return CrackFileSystemURL(FileSystemURL(url));
}

FileSystemURL FileSystemContext::CreateCrackedFileSystemURL(
    const GURL& origin,
    FileSystemType type,
    const base::FilePath& path) const {
  return CrackFileSystemURL(FileSystemURL(origin, type, path));
}

#if defined(OS_CHROMEOS) && defined(GOOGLE_CHROME_BUILD)
void FileSystemContext::EnableTemporaryFileSystemInIncognito() {
  sandbox_backend_->set_enable_temporary_file_system_in_incognito(true);
}
#endif

FileSystemContext::~FileSystemContext() {
}

void FileSystemContext::DeleteOnCorrectThread() const {
  if (!io_task_runner_->RunsTasksOnCurrentThread() &&
      io_task_runner_->DeleteSoon(FROM_HERE, this)) {
    return;
  }
  delete this;
}

FileSystemOperation* FileSystemContext::CreateFileSystemOperation(
    const FileSystemURL& url, base::PlatformFileError* error_code) {
  if (!url.is_valid()) {
    if (error_code)
      *error_code = base::PLATFORM_FILE_ERROR_INVALID_URL;
    return NULL;
  }

  FileSystemBackend* backend = GetFileSystemBackend(url.type());
  if (!backend) {
    if (error_code)
      *error_code = base::PLATFORM_FILE_ERROR_FAILED;
    return NULL;
  }

  base::PlatformFileError fs_error = base::PLATFORM_FILE_OK;
  FileSystemOperation* operation =
      backend->CreateFileSystemOperation(url, this, &fs_error);

  if (error_code)
    *error_code = fs_error;
  return operation;
}

FileSystemURL FileSystemContext::CrackFileSystemURL(
    const FileSystemURL& url) const {
  if (!url.is_valid())
    return FileSystemURL();

  // The returned value in case there is no crackers which can crack the url.
  // This is valid situation for non isolated/external file systems.
  FileSystemURL current = url;

  // File system may be mounted multiple times (e.g., an isolated filesystem on
  // top of an external filesystem). Hence cracking needs to be iterated.
  for (;;) {
    FileSystemURL cracked = current;
    for (size_t i = 0; i < url_crackers_.size(); ++i) {
      if (!url_crackers_[i]->HandlesFileSystemMountType(current.type()))
        continue;
      cracked = url_crackers_[i]->CrackFileSystemURL(current);
      if (cracked.is_valid())
        break;
    }
    if (cracked == current)
      break;
    current = cracked;
  }
  return current;
}

void FileSystemContext::RegisterBackend(
    FileSystemBackend* backend) {
  const FileSystemType mount_types[] = {
    kFileSystemTypeTemporary,
    kFileSystemTypePersistent,
    kFileSystemTypeIsolated,
    kFileSystemTypeExternal,
  };
  // Register file system backends for public mount types.
  for (size_t j = 0; j < ARRAYSIZE_UNSAFE(mount_types); ++j) {
    if (backend->CanHandleType(mount_types[j])) {
      const bool inserted = backend_map_.insert(
          std::make_pair(mount_types[j], backend)).second;
      DCHECK(inserted);
    }
  }
  // Register file system backends for internal types.
  for (int t = kFileSystemInternalTypeEnumStart + 1;
       t < kFileSystemInternalTypeEnumEnd; ++t) {
    FileSystemType type = static_cast<FileSystemType>(t);
    if (backend->CanHandleType(type)) {
      const bool inserted = backend_map_.insert(
          std::make_pair(type, backend)).second;
      DCHECK(inserted);
    }
  }
}

}  // namespace fileapi
