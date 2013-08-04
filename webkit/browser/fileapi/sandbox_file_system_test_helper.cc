// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/browser/fileapi/sandbox_file_system_test_helper.h"

#include "base/file_util.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_loop_proxy.h"
#include "url/gurl.h"
#include "webkit/browser/fileapi/file_system_context.h"
#include "webkit/browser/fileapi/file_system_file_util.h"
#include "webkit/browser/fileapi/file_system_operation_context.h"
#include "webkit/browser/fileapi/file_system_operation_runner.h"
#include "webkit/browser/fileapi/file_system_url.h"
#include "webkit/browser/fileapi/file_system_usage_cache.h"
#include "webkit/browser/fileapi/mock_file_system_context.h"
#include "webkit/browser/fileapi/sandbox_file_system_backend.h"
#include "webkit/browser/quota/mock_special_storage_policy.h"
#include "webkit/common/fileapi/file_system_util.h"

namespace fileapi {

SandboxFileSystemTestHelper::SandboxFileSystemTestHelper(
    const GURL& origin, FileSystemType type)
    : origin_(origin), type_(type), file_util_(NULL) {
}

SandboxFileSystemTestHelper::SandboxFileSystemTestHelper()
    : origin_(GURL("http://foo.com")),
      type_(kFileSystemTypeTemporary),
      file_util_(NULL) {
}

SandboxFileSystemTestHelper::~SandboxFileSystemTestHelper() {
}

void SandboxFileSystemTestHelper::SetUp(const base::FilePath& base_dir) {
  SetUp(base_dir, NULL);
}

void SandboxFileSystemTestHelper::SetUp(
    FileSystemContext* file_system_context) {
  file_system_context_ = file_system_context;

  SetUpFileSystem();
}

void SandboxFileSystemTestHelper::SetUp(
    const base::FilePath& base_dir,
    quota::QuotaManagerProxy* quota_manager_proxy) {
  file_system_context_ = CreateFileSystemContextForTesting(
      quota_manager_proxy, base_dir);

  SetUpFileSystem();
}

void SandboxFileSystemTestHelper::TearDown() {
  file_system_context_ = NULL;
  base::MessageLoop::current()->RunUntilIdle();
}

base::FilePath SandboxFileSystemTestHelper::GetOriginRootPath() {
  return file_system_context_->sandbox_context()->
      GetBaseDirectoryForOriginAndType(origin_, type_, false);
}

base::FilePath SandboxFileSystemTestHelper::GetLocalPath(
    const base::FilePath& path) {
  DCHECK(file_util_);
  base::FilePath local_path;
  scoped_ptr<FileSystemOperationContext> context(NewOperationContext());
  file_util_->GetLocalFilePath(context.get(), CreateURL(path), &local_path);
  return local_path;
}

base::FilePath SandboxFileSystemTestHelper::GetLocalPathFromASCII(
    const std::string& path) {
  return GetLocalPath(base::FilePath().AppendASCII(path));
}

base::FilePath SandboxFileSystemTestHelper::GetUsageCachePath() const {
  return file_system_context_->
      sandbox_context()->GetUsageCachePathForOriginAndType(origin_, type_);
}

FileSystemURL SandboxFileSystemTestHelper::CreateURL(
    const base::FilePath& path) const {
  return file_system_context_->CreateCrackedFileSystemURL(origin_, type_, path);
}

int64 SandboxFileSystemTestHelper::GetCachedOriginUsage() const {
  return file_system_context_->GetQuotaUtil(type_)
      ->GetOriginUsageOnFileThread(file_system_context_.get(), origin_, type_);
}

int64 SandboxFileSystemTestHelper::ComputeCurrentOriginUsage() {
  usage_cache()->CloseCacheFiles();
  int64 size = base::ComputeDirectorySize(GetOriginRootPath());
  if (base::PathExists(GetUsageCachePath()))
    size -= FileSystemUsageCache::kUsageFileSize;
  return size;
}

int64
SandboxFileSystemTestHelper::ComputeCurrentDirectoryDatabaseUsage() {
  return base::ComputeDirectorySize(
      GetOriginRootPath().AppendASCII("Paths"));
}

FileSystemOperationRunner* SandboxFileSystemTestHelper::operation_runner() {
  return file_system_context_->operation_runner();
}

FileSystemOperationContext*
SandboxFileSystemTestHelper::NewOperationContext() {
  DCHECK(file_system_context_.get());
  FileSystemOperationContext* context =
    new FileSystemOperationContext(file_system_context_.get());
  context->set_update_observers(
      *file_system_context_->GetUpdateObservers(type_));
  return context;
}

void SandboxFileSystemTestHelper::AddFileChangeObserver(
    FileChangeObserver* observer) {
  file_system_context_->sandbox_backend()->
      AddFileChangeObserver(type_, observer, NULL);
}

FileSystemUsageCache* SandboxFileSystemTestHelper::usage_cache() {
  return file_system_context()->sandbox_context()->usage_cache();
}

void SandboxFileSystemTestHelper::SetUpFileSystem() {
  DCHECK(file_system_context_.get());
  DCHECK(file_system_context_->sandbox_backend()->CanHandleType(type_));

  file_util_ = file_system_context_->GetFileUtil(type_);
  DCHECK(file_util_);

  // Prepare the origin's root directory.
  file_system_context_->sandbox_context()->
      GetBaseDirectoryForOriginAndType(origin_, type_, true /* create */);

  // Initialize the usage cache file.
  base::FilePath usage_cache_path = GetUsageCachePath();
  if (!usage_cache_path.empty())
    usage_cache()->UpdateUsage(usage_cache_path, 0);
}

}  // namespace fileapi
