// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/local_file_system_test_helper.h"

#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "googleurl/src/gurl.h"
#include "webkit/fileapi/external_mount_points.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_file_util.h"
#include "webkit/fileapi/file_system_operation_context.h"
#include "webkit/fileapi/file_system_task_runners.h"
#include "webkit/fileapi/file_system_url.h"
#include "webkit/fileapi/file_system_usage_cache.h"
#include "webkit/fileapi/file_system_util.h"
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

void LocalFileSystemTestOriginHelper::SetUp(const base::FilePath& base_dir) {
  SetUp(base_dir, false, NULL);
}

void LocalFileSystemTestOriginHelper::SetUp(
    FileSystemContext* file_system_context) {
  file_system_context_ = file_system_context;

  SetUpFileUtil();

  // Prepare the origin's root directory.
  file_system_context_->GetMountPointProvider(type_)->
      GetFileSystemRootPathOnFileThread(CreateURL(base::FilePath()),
                                        true /* create */);

  // Initialize the usage cache file.
  base::FilePath usage_cache_path = GetUsageCachePath();
  if (!usage_cache_path.empty())
    FileSystemUsageCache::UpdateUsage(usage_cache_path, 0);
}

void LocalFileSystemTestOriginHelper::SetUp(
    const base::FilePath& base_dir,
    bool unlimited_quota,
    quota::QuotaManagerProxy* quota_manager_proxy) {
  scoped_refptr<quota::MockSpecialStoragePolicy> special_storage_policy =
      new quota::MockSpecialStoragePolicy;
  special_storage_policy->SetAllUnlimited(unlimited_quota);
  file_system_context_ = new FileSystemContext(
      FileSystemTaskRunners::CreateMockTaskRunners(),
      ExternalMountPoints::CreateRefCounted().get(),
      special_storage_policy,
      quota_manager_proxy,
      base_dir,
      CreateAllowFileAccessOptions());

  SetUpFileUtil();

  // Prepare the origin's root directory.
  FileSystemMountPointProvider* mount_point_provider =
      file_system_context_->GetMountPointProvider(type_);
  mount_point_provider->GetFileSystemRootPathOnFileThread(CreateURL(base::FilePath()),
                                                          true /* create */);

  // Initialize the usage cache file.
  base::FilePath usage_cache_path = GetUsageCachePath();
  if (!usage_cache_path.empty())
    FileSystemUsageCache::UpdateUsage(usage_cache_path, 0);
}

void LocalFileSystemTestOriginHelper::TearDown() {
  file_system_context_ = NULL;
  MessageLoop::current()->RunUntilIdle();
}

base::FilePath LocalFileSystemTestOriginHelper::GetOriginRootPath() const {
  return file_system_context_->GetMountPointProvider(type_)->
      GetFileSystemRootPathOnFileThread(CreateURL(base::FilePath()), false);
}

base::FilePath LocalFileSystemTestOriginHelper::GetLocalPath(const base::FilePath& path) {
  DCHECK(file_util_);
  base::FilePath local_path;
  scoped_ptr<FileSystemOperationContext> context(NewOperationContext());
  file_util_->GetLocalFilePath(context.get(), CreateURL(path), &local_path);
  return local_path;
}

base::FilePath LocalFileSystemTestOriginHelper::GetLocalPathFromASCII(
    const std::string& path) {
  return GetLocalPath(base::FilePath().AppendASCII(path));
}

base::FilePath LocalFileSystemTestOriginHelper::GetUsageCachePath() const {
  if (type_ != kFileSystemTypeTemporary &&
      type_ != kFileSystemTypePersistent)
    return base::FilePath();
  return file_system_context_->
      sandbox_provider()->GetUsageCachePathForOriginAndType(origin_, type_);
}

FileSystemURL LocalFileSystemTestOriginHelper::CreateURL(const base::FilePath& path)
    const {
  return file_system_context_->CreateCrackedFileSystemURL(origin_, type_, path);
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
          CreateURL(base::FilePath()), NULL));
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

void LocalFileSystemTestOriginHelper::SetUpFileUtil() {
  DCHECK(file_system_context_);
  if (type_ == kFileSystemTypeTest) {
    file_system_context_->RegisterMountPointProvider(
        type_,
        new TestMountPointProvider(
            file_system_context_->task_runners()->file_task_runner(),
            file_system_context_->partition_path()));
  }
  file_util_ = file_system_context_->GetFileUtil(type_);
  DCHECK(file_util_);
}

}  // namespace fileapi
