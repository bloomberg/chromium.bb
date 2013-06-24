// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/browser/fileapi/file_system_context.h"

#include "base/bind.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "googleurl/src/gurl.h"
#include "webkit/browser/blob/file_stream_reader.h"
#include "webkit/browser/fileapi/copy_or_move_file_validator.h"
#include "webkit/browser/fileapi/external_mount_points.h"
#include "webkit/browser/fileapi/file_stream_writer.h"
#include "webkit/browser/fileapi/file_system_file_util.h"
#include "webkit/browser/fileapi/file_system_operation.h"
#include "webkit/browser/fileapi/file_system_operation_runner.h"
#include "webkit/browser/fileapi/file_system_options.h"
#include "webkit/browser/fileapi/file_system_quota_client.h"
#include "webkit/browser/fileapi/file_system_task_runners.h"
#include "webkit/browser/fileapi/file_system_url.h"
#include "webkit/browser/fileapi/isolated_context.h"
#include "webkit/browser/fileapi/isolated_mount_point_provider.h"
#include "webkit/browser/fileapi/mount_points.h"
#include "webkit/browser/fileapi/sandbox_mount_point_provider.h"
#include "webkit/browser/fileapi/syncable/local_file_change_tracker.h"
#include "webkit/browser/fileapi/syncable/local_file_sync_context.h"
#include "webkit/browser/fileapi/syncable/syncable_file_system_util.h"
#include "webkit/browser/fileapi/test_mount_point_provider.h"
#include "webkit/browser/quota/quota_manager.h"
#include "webkit/browser/quota/special_storage_policy.h"
#include "webkit/common/fileapi/file_system_util.h"

#if defined(OS_CHROMEOS)
#include "webkit/browser/chromeos/fileapi/cros_mount_point_provider.h"
#endif

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

FileSystemContext::FileSystemContext(
    scoped_ptr<FileSystemTaskRunners> task_runners,
    ExternalMountPoints* external_mount_points,
    quota::SpecialStoragePolicy* special_storage_policy,
    quota::QuotaManagerProxy* quota_manager_proxy,
    ScopedVector<FileSystemMountPointProvider> additional_providers,
    const base::FilePath& partition_path,
    const FileSystemOptions& options)
    : task_runners_(task_runners.Pass()),
      quota_manager_proxy_(quota_manager_proxy),
      sandbox_provider_(
          new SandboxMountPointProvider(
              quota_manager_proxy,
              task_runners_->file_task_runner(),
              partition_path,
              options,
              special_storage_policy)),
      isolated_provider_(new IsolatedMountPointProvider()),
      additional_providers_(additional_providers.Pass()),
      external_mount_points_(external_mount_points),
      partition_path_(partition_path),
      operation_runner_(new FileSystemOperationRunner(this)) {
  DCHECK(task_runners_.get());

  if (quota_manager_proxy) {
    quota_manager_proxy->RegisterClient(CreateQuotaClient(
            this, options.is_incognito()));
  }

  RegisterMountPointProvider(sandbox_provider_.get());
  RegisterMountPointProvider(isolated_provider_.get());

#if defined(OS_CHROMEOS)
  // TODO(kinuko): Move this out of webkit/fileapi layer.
  DCHECK(external_mount_points);
  chromeos::CrosMountPointProvider* cros_mount_provider =
      new chromeos::CrosMountPointProvider(
          special_storage_policy,
          external_mount_points,
          ExternalMountPoints::GetSystemInstance());
  cros_mount_provider->AddSystemMountPoints();
  external_provider_.reset(cros_mount_provider);
  RegisterMountPointProvider(external_provider_.get());
#endif

  for (ScopedVector<FileSystemMountPointProvider>::const_iterator iter =
          additional_providers_.begin();
       iter != additional_providers_.end(); ++iter) {
    RegisterMountPointProvider(*iter);
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
  DCHECK(task_runners_->file_task_runner()->RunsTasksOnCurrentThread());
  DCHECK(origin_url == origin_url.GetOrigin());

  bool success = true;
  for (MountPointProviderMap::iterator iter = provider_map_.begin();
       iter != provider_map_.end();
       ++iter) {
    FileSystemMountPointProvider* provider = iter->second;
    if (!provider->GetQuotaUtil())
      continue;
    if (provider->GetQuotaUtil()->DeleteOriginDataOnFileThread(
            this, quota_manager_proxy(), origin_url, iter->first)
            != base::PLATFORM_FILE_OK) {
      // Continue the loop, but record the failure.
      success = false;
    }
  }

  return success;
}

FileSystemQuotaUtil*
FileSystemContext::GetQuotaUtil(FileSystemType type) const {
  FileSystemMountPointProvider* mount_point_provider =
      GetMountPointProvider(type);
  if (!mount_point_provider)
    return NULL;
  return mount_point_provider->GetQuotaUtil();
}

AsyncFileUtil* FileSystemContext::GetAsyncFileUtil(
    FileSystemType type) const {
  FileSystemMountPointProvider* mount_point_provider =
      GetMountPointProvider(type);
  if (!mount_point_provider)
    return NULL;
  return mount_point_provider->GetAsyncFileUtil(type);
}

FileSystemFileUtil* FileSystemContext::GetFileUtil(
    FileSystemType type) const {
  FileSystemMountPointProvider* mount_point_provider =
      GetMountPointProvider(type);
  if (!mount_point_provider)
    return NULL;
  return mount_point_provider->GetFileUtil(type);
}

CopyOrMoveFileValidatorFactory*
FileSystemContext::GetCopyOrMoveFileValidatorFactory(
    FileSystemType type, base::PlatformFileError* error_code) const {
  DCHECK(error_code);
  *error_code = base::PLATFORM_FILE_OK;
  FileSystemMountPointProvider* mount_point_provider =
      GetMountPointProvider(type);
  if (!mount_point_provider)
    return NULL;
  return mount_point_provider->GetCopyOrMoveFileValidatorFactory(
      type, error_code);
}

FileSystemMountPointProvider* FileSystemContext::GetMountPointProvider(
    FileSystemType type) const {
  MountPointProviderMap::const_iterator found = provider_map_.find(type);
  if (found != provider_map_.end())
    return found->second;
  NOTREACHED() << "Unknown filesystem type: " << type;
  return NULL;
}

bool FileSystemContext::IsSandboxFileSystem(FileSystemType type) const {
  return GetQuotaUtil(type) != NULL;
}

const UpdateObserverList* FileSystemContext::GetUpdateObservers(
    FileSystemType type) const {
  // Currently update observer is only available in SandboxMountPointProvider
  // and TestMountPointProvider.
  // TODO(kinuko): Probably GetUpdateObservers() virtual method should be
  // added to FileSystemMountPointProvider interface and be called like
  // other GetFoo() methods do.
  if (sandbox_provider_->CanHandleType(type))
    return sandbox_provider_->GetUpdateObservers(type);
  if (type != kFileSystemTypeTest)
    return NULL;
  FileSystemMountPointProvider* mount_point_provider =
      GetMountPointProvider(type);
  return static_cast<TestMountPointProvider*>(
      mount_point_provider)->GetUpdateObservers(type);
}

const AccessObserverList* FileSystemContext::GetAccessObservers(
    FileSystemType type) const {
  // Currently access observer is only available in SandboxMountPointProvider.
  if (sandbox_provider_->CanHandleType(type))
    return sandbox_provider_->GetAccessObservers(type);
  return NULL;
}

void FileSystemContext::GetFileSystemTypes(
    std::vector<FileSystemType>* types) const {
  types->clear();
  for (MountPointProviderMap::const_iterator iter = provider_map_.begin();
       iter != provider_map_.end(); ++iter)
    types->push_back(iter->first);
}

ExternalFileSystemMountPointProvider*
FileSystemContext::external_provider() const {
  return external_provider_.get();
}

void FileSystemContext::OpenFileSystem(
    const GURL& origin_url,
    FileSystemType type,
    OpenFileSystemMode mode,
    const OpenFileSystemCallback& callback) {
  DCHECK(!callback.is_null());

  FileSystemMountPointProvider* mount_point_provider =
      GetMountPointProvider(type);
  if (!mount_point_provider) {
    callback.Run(base::PLATFORM_FILE_ERROR_SECURITY, std::string(), GURL());
    return;
  }

  GURL root_url = GetFileSystemRootURI(origin_url, type);
  std::string name = GetFileSystemName(origin_url, type);

  mount_point_provider->OpenFileSystem(
      origin_url, type, mode,
      base::Bind(&DidOpenFileSystem, callback, root_url, name));
}

void FileSystemContext::OpenSyncableFileSystem(
    const GURL& origin_url,
    FileSystemType type,
    OpenFileSystemMode mode,
    const OpenFileSystemCallback& callback) {
  DCHECK(!callback.is_null());

  DCHECK(type == kFileSystemTypeSyncable);

  GURL root_url = sync_file_system::GetSyncableFileSystemRootURI(origin_url);
  std::string name = GetFileSystemName(origin_url, kFileSystemTypeSyncable);

  FileSystemMountPointProvider* mount_point_provider =
      GetMountPointProvider(type);
  DCHECK(mount_point_provider);
  mount_point_provider->OpenFileSystem(
      origin_url, type, mode,
      base::Bind(&DidOpenFileSystem, callback, root_url, name));
}

void FileSystemContext::DeleteFileSystem(
    const GURL& origin_url,
    FileSystemType type,
    const DeleteFileSystemCallback& callback) {
  DCHECK(origin_url == origin_url.GetOrigin());
  FileSystemMountPointProvider* mount_point_provider =
      GetMountPointProvider(type);
  if (!mount_point_provider) {
    callback.Run(base::PLATFORM_FILE_ERROR_SECURITY);
    return;
  }

  mount_point_provider->DeleteFileSystem(origin_url, type, this, callback);
}

scoped_ptr<webkit_blob::FileStreamReader>
FileSystemContext::CreateFileStreamReader(
    const FileSystemURL& url,
    int64 offset,
    const base::Time& expected_modification_time) {
  if (!url.is_valid())
    return scoped_ptr<webkit_blob::FileStreamReader>();
  FileSystemMountPointProvider* mount_point_provider =
      GetMountPointProvider(url.type());
  if (!mount_point_provider)
    return scoped_ptr<webkit_blob::FileStreamReader>();
  return mount_point_provider->CreateFileStreamReader(
      url, offset, expected_modification_time, this);
}

scoped_ptr<FileStreamWriter> FileSystemContext::CreateFileStreamWriter(
    const FileSystemURL& url,
    int64 offset) {
  if (!url.is_valid())
    return scoped_ptr<FileStreamWriter>();
  FileSystemMountPointProvider* mount_point_provider =
      GetMountPointProvider(url.type());
  if (!mount_point_provider)
    return scoped_ptr<FileStreamWriter>();
  return mount_point_provider->CreateFileStreamWriter(url, offset, this);
}

void FileSystemContext::SetLocalFileChangeTracker(
    scoped_ptr<sync_file_system::LocalFileChangeTracker> tracker) {
  DCHECK(!change_tracker_.get());
  DCHECK(tracker.get());
  change_tracker_ = tracker.Pass();
  sandbox_provider_->AddFileUpdateObserver(
      kFileSystemTypeSyncable,
      change_tracker_.get(),
      task_runners_->file_task_runner());
  sandbox_provider_->AddFileChangeObserver(
      kFileSystemTypeSyncable,
      change_tracker_.get(),
      task_runners_->file_task_runner());
}

void FileSystemContext::set_sync_context(
    sync_file_system::LocalFileSyncContext* sync_context) {
  sync_context_ = sync_context;
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
  sandbox_provider_->set_enable_temporary_file_system_in_incognito(true);
}
#endif

FileSystemContext::~FileSystemContext() {
  task_runners_->file_task_runner()->DeleteSoon(
      FROM_HERE, change_tracker_.release());
}

void FileSystemContext::DeleteOnCorrectThread() const {
  if (!task_runners_->io_task_runner()->RunsTasksOnCurrentThread() &&
      task_runners_->io_task_runner()->DeleteSoon(FROM_HERE, this)) {
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

  FileSystemMountPointProvider* mount_point_provider =
      GetMountPointProvider(url.type());
  if (!mount_point_provider) {
    if (error_code)
      *error_code = base::PLATFORM_FILE_ERROR_FAILED;
    return NULL;
  }

  base::PlatformFileError fs_error = base::PLATFORM_FILE_OK;
  FileSystemOperation* operation =
      mount_point_provider->CreateFileSystemOperation(url, this, &fs_error);

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

void FileSystemContext::RegisterMountPointProvider(
    FileSystemMountPointProvider* provider) {
  const FileSystemType mount_types[] = {
    kFileSystemTypeTemporary,
    kFileSystemTypePersistent,
    kFileSystemTypeIsolated,
    kFileSystemTypeExternal,
  };
  // Register mount point providers for public mount types.
  for (size_t j = 0; j < ARRAYSIZE_UNSAFE(mount_types); ++j) {
    if (provider->CanHandleType(mount_types[j])) {
      const bool inserted = provider_map_.insert(
          std::make_pair(mount_types[j], provider)).second;
      DCHECK(inserted);
    }
  }
  // Register mount point providers for internal types.
  for (int t = kFileSystemInternalTypeEnumStart + 1;
       t < kFileSystemInternalTypeEnumEnd; ++t) {
    FileSystemType type = static_cast<FileSystemType>(t);
    if (provider->CanHandleType(type)) {
      const bool inserted = provider_map_.insert(
          std::make_pair(type, provider)).second;
      DCHECK(inserted);
    }
  }
}

}  // namespace fileapi
