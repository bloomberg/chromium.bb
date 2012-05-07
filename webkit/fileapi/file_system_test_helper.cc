// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/file_system_test_helper.h"

#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "googleurl/src/gurl.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_operation.h"
#include "webkit/fileapi/file_system_operation_context.h"
#include "webkit/fileapi/file_system_usage_cache.h"
#include "webkit/fileapi/file_system_util.h"
#include "webkit/fileapi/file_util_helper.h"
#include "webkit/fileapi/mock_file_system_options.h"
#include "webkit/fileapi/sandbox_mount_point_provider.h"
#include "webkit/quota/mock_special_storage_policy.h"

namespace fileapi {

FileSystemTestOriginHelper::FileSystemTestOriginHelper(
    const GURL& origin, FileSystemType type)
    : origin_(origin), type_(type), file_util_(NULL) {
}

FileSystemTestOriginHelper::FileSystemTestOriginHelper()
    : origin_(GURL("http://foo.com")),
      type_(kFileSystemTypeTemporary),
      file_util_(NULL) {
}

FileSystemTestOriginHelper::~FileSystemTestOriginHelper() {
}

void FileSystemTestOriginHelper::SetUp(
    const FilePath& base_dir, FileSystemFileUtil* file_util) {
  SetUp(base_dir, false, NULL, file_util);
}

void FileSystemTestOriginHelper::SetUp(
    FileSystemContext* file_system_context, FileSystemFileUtil* file_util) {
  DCHECK(file_system_context->sandbox_provider());

  file_util_ = file_util;
  file_system_context_ = file_system_context;
  if (!file_util_)
    file_util_ = file_system_context->sandbox_provider()->GetFileUtil();
  DCHECK(file_util_);

  // Prepare the origin's root directory.
  file_system_context_->GetMountPointProvider(type_)->
      GetFileSystemRootPathOnFileThread(
          origin_, type_, FilePath(), true /* create */);

  // Initialize the usage cache file.
  FilePath usage_cache_path = file_system_context_->
      sandbox_provider()->GetUsageCachePathForOriginAndType(origin_, type_);
  FileSystemUsageCache::UpdateUsage(usage_cache_path, 0);
}

void FileSystemTestOriginHelper::SetUp(
    const FilePath& base_dir,
    bool unlimited_quota,
    quota::QuotaManagerProxy* quota_manager_proxy,
    FileSystemFileUtil* file_util) {
  scoped_refptr<quota::MockSpecialStoragePolicy> special_storage_policy =
      new quota::MockSpecialStoragePolicy;
  special_storage_policy->SetAllUnlimited(unlimited_quota);
  file_system_context_ = new FileSystemContext(
      base::MessageLoopProxy::current(),
      base::MessageLoopProxy::current(),
      special_storage_policy,
      quota_manager_proxy,
      base_dir,
      CreateAllowFileAccessOptions());

  DCHECK(file_system_context_->sandbox_provider());

  // Prepare the origin's root directory.
  FileSystemMountPointProvider* mount_point_provider =
      file_system_context_->GetMountPointProvider(type_);
  mount_point_provider->GetFileSystemRootPathOnFileThread(
      origin_, type_, FilePath(), true /* create */);

  if (file_util)
    file_util_ = file_util;
  else
    file_util_ = mount_point_provider->GetFileUtil();

  DCHECK(file_util_);

  // Initialize the usage cache file.  This code assumes that we're either using
  // OFSFU or we've mocked it out in the sandbox provider.
  FilePath usage_cache_path = file_system_context_->
      sandbox_provider()->GetUsageCachePathForOriginAndType(origin_, type_);
  FileSystemUsageCache::UpdateUsage(usage_cache_path, 0);
}

void FileSystemTestOriginHelper::TearDown() {
  file_system_context_ = NULL;
  MessageLoop::current()->RunAllPending();
}

FilePath FileSystemTestOriginHelper::GetOriginRootPath() const {
  return file_system_context_->GetMountPointProvider(type_)->
      GetFileSystemRootPathOnFileThread(
          origin_, type_, FilePath(), false);
}

FilePath FileSystemTestOriginHelper::GetLocalPath(const FilePath& path) {
  DCHECK(file_util_);
  FilePath local_path;
  scoped_ptr<FileSystemOperationContext> context(NewOperationContext());
  file_util_->GetLocalFilePath(context.get(), CreatePath(path), &local_path);
  return local_path;
}

FilePath FileSystemTestOriginHelper::GetLocalPathFromASCII(
    const std::string& path) {
  return GetLocalPath(FilePath().AppendASCII(path));
}

GURL FileSystemTestOriginHelper::GetURLForPath(const FilePath& path) const {
  return GURL(GetFileSystemRootURI(origin_, type_).spec() +
              path.MaybeAsASCII());
}

FilePath FileSystemTestOriginHelper::GetUsageCachePath() const {
  return file_system_context_->
      sandbox_provider()->GetUsageCachePathForOriginAndType(origin_, type_);
}

FileSystemPath FileSystemTestOriginHelper::CreatePath(
    const FilePath& path) const {
  return FileSystemPath(origin_, type_, path);
}

base::PlatformFileError FileSystemTestOriginHelper::SameFileUtilCopy(
    FileSystemOperationContext* context,
    const FileSystemPath& src,
    const FileSystemPath& dest) const {
  return FileUtilHelper::Copy(context, file_util(), file_util(), src, dest);
}

base::PlatformFileError FileSystemTestOriginHelper::SameFileUtilMove(
    FileSystemOperationContext* context,
    const FileSystemPath& src,
    const FileSystemPath& dest) const {
  return FileUtilHelper::Move(context, file_util(), file_util(), src, dest);
}

int64 FileSystemTestOriginHelper::GetCachedOriginUsage() const {
  return FileSystemUsageCache::GetUsage(GetUsageCachePath());
}

bool FileSystemTestOriginHelper::RevokeUsageCache() const {
  return file_util::Delete(GetUsageCachePath(), false);
}

int64 FileSystemTestOriginHelper::ComputeCurrentOriginUsage() const {
  int64 size = file_util::ComputeDirectorySize(GetOriginRootPath());
  if (file_util::PathExists(GetUsageCachePath()))
    size -= FileSystemUsageCache::kUsageFileSize;
  return size;
}

int64 FileSystemTestOriginHelper::ComputeCurrentDirectoryDatabaseUsage() const {
  return file_util::ComputeDirectorySize(
      GetOriginRootPath().AppendASCII("Paths"));
}

FileSystemOperation* FileSystemTestOriginHelper::NewOperation() {
  DCHECK(file_system_context_.get());
  DCHECK(file_util_);
  FileSystemOperation* operation =
    new FileSystemOperation(file_system_context_.get());
  operation->set_override_file_util(file_util_);
  return operation;
}

FileSystemOperationContext* FileSystemTestOriginHelper::NewOperationContext() {
  DCHECK(file_system_context_.get());
  FileSystemOperationContext* context =
    new FileSystemOperationContext(file_system_context_.get());
  return context;
}

}  // namespace fileapi
