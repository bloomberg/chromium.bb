// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/file_system_context.h"

#include "base/bind.h"
#include "base/stl_util.h"
#include "base/single_thread_task_runner.h"
#include "googleurl/src/gurl.h"
#include "webkit/fileapi/external_mount_points.h"
#include "webkit/fileapi/file_system_file_util.h"
#include "webkit/fileapi/file_system_operation.h"
#include "webkit/fileapi/file_system_options.h"
#include "webkit/fileapi/file_system_quota_client.h"
#include "webkit/fileapi/file_system_task_runners.h"
#include "webkit/fileapi/file_system_url.h"
#include "webkit/fileapi/file_system_util.h"
#include "webkit/fileapi/isolated_context.h"
#include "webkit/fileapi/isolated_mount_point_provider.h"
#include "webkit/fileapi/mount_points.h"
#include "webkit/fileapi/sandbox_mount_point_provider.h"
#include "webkit/fileapi/syncable/local_file_change_tracker.h"
#include "webkit/fileapi/syncable/local_file_sync_context.h"
#include "webkit/fileapi/syncable/syncable_file_system_util.h"
#include "webkit/fileapi/test_mount_point_provider.h"
#include "webkit/quota/quota_manager.h"
#include "webkit/quota/special_storage_policy.h"

#if defined(OS_CHROMEOS)
#include "webkit/chromeos/fileapi/cros_mount_point_provider.h"
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
    const base::FilePath& partition_path,
    const FileSystemOptions& options)
    : task_runners_(task_runners.Pass()),
      quota_manager_proxy_(quota_manager_proxy),
      sandbox_provider_(
          new SandboxMountPointProvider(
              quota_manager_proxy,
              task_runners_->file_task_runner(),
              partition_path,
              options)),
      isolated_provider_(new IsolatedMountPointProvider(partition_path)),
      external_mount_points_(external_mount_points),
      partition_path_(partition_path) {
  DCHECK(task_runners_.get());

  if (quota_manager_proxy) {
    quota_manager_proxy->RegisterClient(CreateQuotaClient(
            this, options.is_incognito()));
  }

#if defined(OS_CHROMEOS)
  DCHECK(external_mount_points);
  external_provider_.reset(
      new chromeos::CrosMountPointProvider(
          special_storage_policy,
          external_mount_points,
          ExternalMountPoints::GetSystemInstance()));
#endif

  if (external_mount_points)
    url_crackers_.push_back(external_mount_points);
  url_crackers_.push_back(ExternalMountPoints::GetSystemInstance());
  url_crackers_.push_back(IsolatedContext::GetInstance());
}

bool FileSystemContext::DeleteDataForOriginOnFileThread(
    const GURL& origin_url) {
  DCHECK(task_runners_->file_task_runner()->RunsTasksOnCurrentThread());
  DCHECK(sandbox_provider());
  DCHECK(origin_url == origin_url.GetOrigin());

  // Delete temporary and persistent data.
  return
      (sandbox_provider()->DeleteOriginDataOnFileThread(
          this, quota_manager_proxy(), origin_url,
          kFileSystemTypeTemporary) ==
       base::PLATFORM_FILE_OK) &&
      (sandbox_provider()->DeleteOriginDataOnFileThread(
          this, quota_manager_proxy(), origin_url,
          kFileSystemTypePersistent) ==
       base::PLATFORM_FILE_OK) &&
      (sandbox_provider()->DeleteOriginDataOnFileThread(
          this, quota_manager_proxy(), origin_url,
          kFileSystemTypeSyncable) ==
       base::PLATFORM_FILE_OK);
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

FileSystemMountPointProvider* FileSystemContext::GetMountPointProvider(
    FileSystemType type) const {
  switch (type) {
    case kFileSystemTypeTemporary:
    case kFileSystemTypePersistent:
    case kFileSystemTypeSyncable:
      return sandbox_provider_.get();
    case kFileSystemTypeExternal:
    case kFileSystemTypeDrive:
    case kFileSystemTypeRestrictedNativeLocal:
      return external_provider_.get();
    case kFileSystemTypeIsolated:
    case kFileSystemTypeDragged:
    case kFileSystemTypeNativeMedia:
    case kFileSystemTypeDeviceMedia:
      return isolated_provider_.get();
    case kFileSystemTypeNativeLocal:
    case kFileSystemTypeNativeForPlatformApp:
#if defined(OS_CHROMEOS)
      return external_provider_.get();
#else
      return isolated_provider_.get();
#endif
    default:
      if (provider_map_.find(type) != provider_map_.end())
        return provider_map_.find(type)->second;
      // Fall through.
    case kFileSystemTypeUnknown:
      NOTREACHED();
      return NULL;
  }
}

const UpdateObserverList* FileSystemContext::GetUpdateObservers(
    FileSystemType type) const {
  // Currently update observer is only available in SandboxMountPointProvider
  // and TestMountPointProvider.
  // TODO(kinuko): Probably GetUpdateObservers() virtual method should be
  // added to FileSystemMountPointProvider interface and be called like
  // other GetFoo() methods do.
  if (SandboxMountPointProvider::CanHandleType(type))
    return sandbox_provider()->GetUpdateObservers(type);
  if (type != kFileSystemTypeTest)
    return NULL;
  FileSystemMountPointProvider* mount_point_provider =
      GetMountPointProvider(type);
  return static_cast<TestMountPointProvider*>(
      mount_point_provider)->GetUpdateObservers(type);
}

SandboxMountPointProvider*
FileSystemContext::sandbox_provider() const {
  return sandbox_provider_.get();
}

ExternalFileSystemMountPointProvider*
FileSystemContext::external_provider() const {
  return external_provider_.get();
}

void FileSystemContext::OpenFileSystem(
    const GURL& origin_url,
    FileSystemType type,
    bool create,
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

  mount_point_provider->ValidateFileSystemRoot(
      origin_url, type, create,
      base::Bind(&DidOpenFileSystem, callback, root_url, name));
}

void FileSystemContext::OpenSyncableFileSystem(
    const std::string& mount_name,
    const GURL& origin_url,
    FileSystemType type,
    bool create,
    const OpenFileSystemCallback& callback) {
  DCHECK(!callback.is_null());

  DCHECK(type == kFileSystemTypeSyncable);

  GURL root_url = sync_file_system::GetSyncableFileSystemRootURI(
      origin_url, mount_name);
  std::string name = GetFileSystemName(origin_url, kFileSystemTypeSyncable);

  FileSystemMountPointProvider* mount_point_provider =
      GetMountPointProvider(type);
  DCHECK(mount_point_provider);
  mount_point_provider->ValidateFileSystemRoot(
      origin_url, type, create,
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

webkit_blob::FileStreamReader* FileSystemContext::CreateFileStreamReader(
    const FileSystemURL& url,
    int64 offset,
    const base::Time& expected_modification_time) {
  if (!url.is_valid())
    return NULL;
  FileSystemMountPointProvider* mount_point_provider =
      GetMountPointProvider(url.type());
  if (!mount_point_provider)
    return NULL;
  return mount_point_provider->CreateFileStreamReader(
      url, offset, expected_modification_time, this);
}

void FileSystemContext::RegisterMountPointProvider(
    FileSystemType type,
    FileSystemMountPointProvider* provider) {
  DCHECK(provider);
  DCHECK(provider_map_.find(type) == provider_map_.end());
  provider_map_[type] = provider;
}

void FileSystemContext::SetLocalFileChangeTracker(
    scoped_ptr<sync_file_system::LocalFileChangeTracker> tracker) {
  DCHECK(!change_tracker_.get());
  DCHECK(tracker.get());
  change_tracker_ = tracker.Pass();
  sandbox_provider_->AddSyncableFileUpdateObserver(
      change_tracker_.get(),
      task_runners_->file_task_runner());
  sandbox_provider_->AddSyncableFileChangeObserver(
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

FileSystemContext::~FileSystemContext() {
  task_runners_->file_task_runner()->DeleteSoon(
      FROM_HERE, change_tracker_.release());
}

void FileSystemContext::DeleteOnCorrectThread() const {
  if (!task_runners_->io_task_runner()->RunsTasksOnCurrentThread() &&
      task_runners_->io_task_runner()->DeleteSoon(FROM_HERE, this)) {
    return;
  }
  STLDeleteContainerPairSecondPointers(provider_map_.begin(),
                                       provider_map_.end());
  delete this;
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

FileSystemFileUtil* FileSystemContext::GetFileUtil(
    FileSystemType type) const {
  FileSystemMountPointProvider* mount_point_provider =
      GetMountPointProvider(type);
  if (!mount_point_provider)
    return NULL;
  return mount_point_provider->GetFileUtil(type);
}

}  // namespace fileapi
