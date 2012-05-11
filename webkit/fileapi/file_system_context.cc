// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/file_system_context.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "base/stl_util.h"
#include "base/single_thread_task_runner.h"
#include "googleurl/src/gurl.h"
#include "webkit/fileapi/file_system_file_util.h"
#include "webkit/fileapi/file_system_operation_interface.h"
#include "webkit/fileapi/file_system_options.h"
#include "webkit/fileapi/file_system_quota_client.h"
#include "webkit/fileapi/file_system_util.h"
#include "webkit/fileapi/isolated_mount_point_provider.h"
#include "webkit/fileapi/sandbox_mount_point_provider.h"
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

void DidOpenFileSystem(FileSystemContext::OpenFileSystemCallback callback,
                       const GURL& filesystem_root,
                       const std::string& filesystem_name,
                       base::PlatformFileError error) {
  callback.Run(error, filesystem_name, filesystem_root);
}

}  // anonymous namespace

FileSystemContext::FileSystemContext(
    base::SequencedTaskRunner* file_task_runner,
    base::SingleThreadTaskRunner* io_task_runner,
    quota::SpecialStoragePolicy* special_storage_policy,
    quota::QuotaManagerProxy* quota_manager_proxy,
    const FilePath& profile_path,
    const FileSystemOptions& options)
    : file_task_runner_(file_task_runner),
      io_task_runner_(io_task_runner),
      quota_manager_proxy_(quota_manager_proxy),
      sandbox_provider_(
          new SandboxMountPointProvider(
              file_task_runner,
              profile_path,
              options)),
      isolated_provider_(new IsolatedMountPointProvider) {
  if (quota_manager_proxy) {
    quota_manager_proxy->RegisterClient(CreateQuotaClient(
            this, options.is_incognito()));
  }
#if defined(OS_CHROMEOS)
  external_provider_.reset(
      new chromeos::CrosMountPointProvider(special_storage_policy));
#endif
}

bool FileSystemContext::DeleteDataForOriginOnFileThread(
    const GURL& origin_url) {
  DCHECK(file_task_runner_->RunsTasksOnCurrentThread());
  DCHECK(sandbox_provider());

  // Delete temporary and persistent data.
  return
      sandbox_provider()->DeleteOriginDataOnFileThread(
          quota_manager_proxy(), origin_url, kFileSystemTypeTemporary) &&
      sandbox_provider()->DeleteOriginDataOnFileThread(
          quota_manager_proxy(), origin_url, kFileSystemTypePersistent);
}

bool FileSystemContext::DeleteDataForOriginAndTypeOnFileThread(
    const GURL& origin_url, FileSystemType type) {
  DCHECK(file_task_runner_->RunsTasksOnCurrentThread());
  if (type == fileapi::kFileSystemTypeTemporary ||
      type == fileapi::kFileSystemTypePersistent) {
    DCHECK(sandbox_provider());
    return sandbox_provider()->DeleteOriginDataOnFileThread(
        quota_manager_proxy(), origin_url, type);
  }
  return false;
}

FileSystemQuotaUtil*
FileSystemContext::GetQuotaUtil(FileSystemType type) const {
  FileSystemMountPointProvider* mount_point_provider =
      GetMountPointProvider(type);
  if (!mount_point_provider)
    return NULL;
  return mount_point_provider->GetQuotaUtil();
}

FileSystemFileUtil* FileSystemContext::GetFileUtil(
    FileSystemType type) const {
  FileSystemMountPointProvider* mount_point_provider =
      GetMountPointProvider(type);
  if (!mount_point_provider)
    return NULL;
  return mount_point_provider->GetFileUtil();
}

FileSystemMountPointProvider* FileSystemContext::GetMountPointProvider(
    FileSystemType type) const {
  switch (type) {
    case kFileSystemTypeTemporary:
    case kFileSystemTypePersistent:
      return sandbox_provider_.get();
    case kFileSystemTypeExternal:
      return external_provider_.get();
    case kFileSystemTypeIsolated:
      return isolated_provider_.get();
    default:
      if (provider_map_.find(type) != provider_map_.end())
        return provider_map_.find(type)->second;
      // Fall through.
    case kFileSystemTypeUnknown:
      NOTREACHED();
      return NULL;
  }
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
    OpenFileSystemCallback callback) {
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

FileSystemOperationInterface* FileSystemContext::CreateFileSystemOperation(
    const GURL& url) {
  GURL origin_url;
  FileSystemType file_system_type = kFileSystemTypeUnknown;
  FilePath file_path;
  if (!CrackFileSystemURL(url, &origin_url, &file_system_type, &file_path))
    return NULL;
  FileSystemMountPointProvider* mount_point_provider =
      GetMountPointProvider(file_system_type);
  if (!mount_point_provider)
    return NULL;
  return mount_point_provider->CreateFileSystemOperation(
      origin_url, file_system_type, file_path, this);
}

webkit_blob::FileReader* FileSystemContext::CreateFileReader(
    const GURL& url,
    int64 offset) {
  GURL origin_url;
  FileSystemType file_system_type = kFileSystemTypeUnknown;
  FilePath file_path;
  if (!CrackFileSystemURL(url, &origin_url, &file_system_type, &file_path))
    return NULL;
  FileSystemMountPointProvider* mount_point_provider =
      GetMountPointProvider(file_system_type);
  if (!mount_point_provider)
    return NULL;
  return mount_point_provider->CreateFileReader(url, offset, this);
}

void FileSystemContext::RegisterMountPointProvider(
    FileSystemType type,
    FileSystemMountPointProvider* provider) {
  DCHECK(provider);
  DCHECK(provider_map_.find(type) == provider_map_.end());
  provider_map_[type] = provider;
}

FileSystemContext::~FileSystemContext() {}

void FileSystemContext::DeleteOnCorrectThread() const {
  if (!io_task_runner_->RunsTasksOnCurrentThread() &&
      io_task_runner_->DeleteSoon(FROM_HERE, this)) {
    return;
  }
  STLDeleteContainerPairSecondPointers(provider_map_.begin(),
                                       provider_map_.end());
  delete this;
}

}  // namespace fileapi
