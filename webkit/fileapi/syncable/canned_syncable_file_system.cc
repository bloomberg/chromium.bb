// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/syncable/canned_syncable_file_system.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "base/message_loop_proxy.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_operation_context.h"
#include "webkit/fileapi/file_system_task_runners.h"
#include "webkit/fileapi/file_system_util.h"
#include "webkit/fileapi/isolated_context.h"
#include "webkit/fileapi/local_file_system_operation.h"
#include "webkit/fileapi/mock_file_system_options.h"
#include "webkit/quota/mock_special_storage_policy.h"
#include "webkit/quota/quota_manager.h"

using base::PlatformFileError;
using quota::QuotaManager;

namespace fileapi {

CannedSyncableFileSystem::CannedSyncableFileSystem(
    const GURL& origin, const std::string& service)
    : service_name_(service),
      test_helper_(origin, kFileSystemTypeSyncable),
      result_(base::PLATFORM_FILE_OK),
      sync_status_(SYNC_STATUS_OK),
      weak_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
}

CannedSyncableFileSystem::~CannedSyncableFileSystem() {}

void CannedSyncableFileSystem::SetUp() {
  ASSERT_TRUE(data_dir_.CreateUniqueTempDir());

  scoped_refptr<quota::SpecialStoragePolicy> storage_policy =
      new quota::MockSpecialStoragePolicy();

  quota_manager_ = new QuotaManager(
      false /* is_incognito */,
      data_dir_.path(),
      base::MessageLoopProxy::current(),
      base::MessageLoopProxy::current(),
      storage_policy);

  file_system_context_ = new FileSystemContext(
      FileSystemTaskRunners::CreateMockTaskRunners(),
      storage_policy,
      quota_manager_->proxy(),
      data_dir_.path(),
      CreateAllowFileAccessOptions());

  test_helper_.SetUp(file_system_context_.get(), NULL);
}

void CannedSyncableFileSystem::TearDown() {
  quota_manager_ = NULL;
  test_helper_.TearDown();
}

FileSystemURL CannedSyncableFileSystem::URL(const std::string& path) const {
  return FileSystemURL(GURL(root_url_.spec() + path));
}

PlatformFileError CannedSyncableFileSystem::OpenFileSystem() {
  file_system_context_->OpenSyncableFileSystem(
      service_name_,
      test_helper_.origin(), test_helper_.type(),
      true /* create */,
      base::Bind(&CannedSyncableFileSystem::DidOpenFileSystem,
                  weak_factory_.GetWeakPtr()));
  MessageLoop::current()->RunAllPending();
  return result_;
}

PlatformFileError CannedSyncableFileSystem::CreateDirectory(
    const FileSystemURL& url) {
  result_ = base::PLATFORM_FILE_ERROR_FAILED;
  test_helper_.NewOperation()->CreateDirectory(
      url, false /* exclusive */, false /* recursive */,
      base::Bind(&CannedSyncableFileSystem::StatusCallback,
                  weak_factory_.GetWeakPtr()));
  MessageLoop::current()->RunAllPending();
  return result_;
}

PlatformFileError CannedSyncableFileSystem::CreateFile(
    const FileSystemURL& url) {
  result_ = base::PLATFORM_FILE_ERROR_FAILED;
  test_helper_.NewOperation()->CreateFile(
      url, false /* exclusive */,
      base::Bind(&CannedSyncableFileSystem::StatusCallback,
                  weak_factory_.GetWeakPtr()));
  MessageLoop::current()->RunAllPending();
  return result_;
}

PlatformFileError CannedSyncableFileSystem::TruncateFile(
    const FileSystemURL& url, int64 size) {
  result_ = base::PLATFORM_FILE_ERROR_FAILED;
  test_helper_.NewOperation()->Truncate(
      url, size,
      base::Bind(&CannedSyncableFileSystem::StatusCallback,
                  weak_factory_.GetWeakPtr()));
  MessageLoop::current()->RunAllPending();
  return result_;
}

PlatformFileError CannedSyncableFileSystem::Remove(
    const FileSystemURL& url, bool recursive) {
  result_ = base::PLATFORM_FILE_ERROR_FAILED;
  test_helper_.NewOperation()->Remove(
      url, recursive,
      base::Bind(&CannedSyncableFileSystem::StatusCallback,
                  weak_factory_.GetWeakPtr()));
  MessageLoop::current()->RunAllPending();
  return result_;
}

PlatformFileError CannedSyncableFileSystem::DeleteFileSystem() {
  file_system_context_->DeleteFileSystem(
      test_helper_.origin(), test_helper_.type(),
      base::Bind(&CannedSyncableFileSystem::StatusCallback,
                 weak_factory_.GetWeakPtr()));
  MessageLoop::current()->RunAllPending();
  return result_;
}

void CannedSyncableFileSystem::DidOpenFileSystem(
    PlatformFileError result, const std::string& name, const GURL& root) {
  result_ = result;
  root_url_ = root;
}

void CannedSyncableFileSystem::StatusCallback(PlatformFileError result) {
  result_ = result;
}

FileSystemOperationContext* CannedSyncableFileSystem::NewOperationContext() {
  FileSystemOperationContext* context = test_helper_.NewOperationContext();
  context->set_allowed_bytes_growth(kint64max);
  return context;
}

}  // namespace fileapi
