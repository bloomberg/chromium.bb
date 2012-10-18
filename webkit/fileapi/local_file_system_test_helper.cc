// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/local_file_system_test_helper.h"

#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "googleurl/src/gurl.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_operation_context.h"
#include "webkit/fileapi/file_system_task_runners.h"
#include "webkit/fileapi/file_system_usage_cache.h"
#include "webkit/fileapi/file_system_util.h"
#include "webkit/fileapi/file_util_helper.h"
#include "webkit/fileapi/local_file_system_operation.h"
#include "webkit/fileapi/mock_file_system_options.h"
#include "webkit/fileapi/sandbox_mount_point_provider.h"
#include "webkit/fileapi/test_mount_point_provider.h"
#include "webkit/quota/mock_special_storage_policy.h"

namespace fileapi {

LocalFileSystemTestOriginHelper::LocalFileSystemTestOriginHelper(
    const GURL& origin, FileSystemType type)
    : origin_(origin), type_(type), file_util_(NULL) {
}

LocalFileSystemTestOriginHelper::LocalFileSystemTestOriginHelper()
    : origin_(GURL("http://foo.com")),
      type_(kFileSystemTypeTemporary),
      file_util_(NULL) {
}

LocalFileSystemTestOriginHelper::~LocalFileSystemTestOriginHelper() {
}

void LocalFileSystemTestOriginHelper::SetUp(
    const FilePath& base_dir, FileSystemFileUtil* file_util) {
  SetUp(base_dir, false, NULL, file_util);
}

void LocalFileSystemTestOriginHelper::SetUp(
    FileSystemContext* file_system_context, FileSystemFileUtil* file_util) {
  file_util_ = file_util;
  file_system_context_ = file_system_context;
  if (!file_util_)
    file_util_ = file_system_context->GetFileUtil(type_);
  DCHECK(file_util_);

  // Prepare the origin's root directory.
  file_system_context_->GetMountPointProvider(type_)->
      GetFileSystemRootPathOnFileThread(
          origin_, type_, FilePath(), true /* create */);

  // Initialize the usage cache file.
  FilePath usage_cache_path = GetUsageCachePath();
  if (!usage_cache_path.empty())
    FileSystemUsageCache::UpdateUsage(usage_cache_path, 0);
}

void LocalFileSystemTestOriginHelper::SetUp(
    const FilePath& base_dir,
    bool unlimited_quota,
    quota::QuotaManagerProxy* quota_manager_proxy,
    FileSystemFileUtil* file_util) {
  scoped_refptr<quota::MockSpecialStoragePolicy> special_storage_policy =
      new quota::MockSpecialStoragePolicy;
  special_storage_policy->SetAllUnlimited(unlimited_quota);
  file_system_context_ = new FileSystemContext(
      FileSystemTaskRunners::CreateMockTaskRunners(),
      special_storage_policy,
      quota_manager_proxy,
      base_dir,
      CreateAllowFileAccessOptions());

  if (type_ == kFileSystemTypeTest) {
    file_system_context_->RegisterMountPointProvider(
        type_,
        new TestMountPointProvider(
            file_system_context_->task_runners()->file_task_runner(),
            base_dir));
  }

  // Prepare the origin's root directory.
  FileSystemMountPointProvider* mount_point_provider =
      file_system_context_->GetMountPointProvider(type_);
  mount_point_provider->GetFileSystemRootPathOnFileThread(
      origin_, type_, FilePath(), true /* create */);

  if (file_util)
    file_util_ = file_util;
  else
    file_util_ = mount_point_provider->GetFileUtil(type_);

  DCHECK(file_util_);

  // Initialize the usage cache file.
  FilePath usage_cache_path = GetUsageCachePath();
  if (!usage_cache_path.empty())
    FileSystemUsageCache::UpdateUsage(usage_cache_path, 0);
}

void LocalFileSystemTestOriginHelper::TearDown() {
  file_system_context_ = NULL;
  MessageLoop::current()->RunAllPending();
}

FilePath LocalFileSystemTestOriginHelper::GetOriginRootPath() const {
  return file_system_context_->GetMountPointProvider(type_)->
      GetFileSystemRootPathOnFileThread(
          origin_, type_, FilePath(), false);
}

FilePath LocalFileSystemTestOriginHelper::GetLocalPath(const FilePath& path) {
  DCHECK(file_util_);
  FilePath local_path;
  scoped_ptr<FileSystemOperationContext> context(NewOperationContext());
  file_util_->GetLocalFilePath(context.get(), CreateURL(path), &local_path);
  return local_path;
}

FilePath LocalFileSystemTestOriginHelper::GetLocalPathFromASCII(
    const std::string& path) {
  return GetLocalPath(FilePath().AppendASCII(path));
}

FilePath LocalFileSystemTestOriginHelper::GetUsageCachePath() const {
  if (type_ != kFileSystemTypeTemporary &&
      type_ != kFileSystemTypePersistent)
    return FilePath();
  return file_system_context_->
      sandbox_provider()->GetUsageCachePathForOriginAndType(origin_, type_);
}

FileSystemURL LocalFileSystemTestOriginHelper::CreateURL(const FilePath& path)
    const {
  return FileSystemURL(origin_, type_, path);
}

base::PlatformFileError LocalFileSystemTestOriginHelper::SameFileUtilCopy(
    FileSystemOperationContext* context,
    const FileSystemURL& src,
    const FileSystemURL& dest) const {
  return FileUtilHelper::Copy(context, file_util(), file_util(), src, dest);
}

base::PlatformFileError LocalFileSystemTestOriginHelper::SameFileUtilMove(
    FileSystemOperationContext* context,
    const FileSystemURL& src,
    const FileSystemURL& dest) const {
  return FileUtilHelper::Move(context, file_util(), file_util(), src, dest);
}

int64 LocalFileSystemTestOriginHelper::GetCachedOriginUsage() const {
  return file_system_context_->GetQuotaUtil(type_)->GetOriginUsageOnFileThread(
      file_system_context_, origin_, type_);
}

int64 LocalFileSystemTestOriginHelper::ComputeCurrentOriginUsage() const {
  int64 size = file_util::ComputeDirectorySize(GetOriginRootPath());
  if (file_util::PathExists(GetUsageCachePath()))
    size -= FileSystemUsageCache::kUsageFileSize;
  return size;
}

int64
LocalFileSystemTestOriginHelper::ComputeCurrentDirectoryDatabaseUsage() const {
  return file_util::ComputeDirectorySize(
      GetOriginRootPath().AppendASCII("Paths"));
}

LocalFileSystemOperation* LocalFileSystemTestOriginHelper::NewOperation() {
  DCHECK(file_system_context_.get());
  DCHECK(file_util_);
  scoped_ptr<FileSystemOperationContext> operation_context(
      NewOperationContext());
  LocalFileSystemOperation* operation = static_cast<LocalFileSystemOperation*>(
      file_system_context_->CreateFileSystemOperation(
          CreateURL(FilePath()), NULL));
  operation->set_override_file_util(file_util_);
  return operation;
}

FileSystemOperationContext*
LocalFileSystemTestOriginHelper::NewOperationContext() {
  DCHECK(file_system_context_.get());
  FileSystemOperationContext* context =
    new FileSystemOperationContext(file_system_context_.get());
  context->set_update_observers(
      *file_system_context_->GetUpdateObservers(type_));
  return context;
}

}  // namespace fileapi
