// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/syncable/canned_syncable_file_system.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "base/message_loop_proxy.h"
#include "base/single_thread_task_runner.h"
#include "base/task_runner_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_operation_context.h"
#include "webkit/fileapi/file_system_task_runners.h"
#include "webkit/fileapi/file_system_util.h"
#include "webkit/fileapi/isolated_context.h"
#include "webkit/fileapi/local_file_system_operation.h"
#include "webkit/fileapi/mock_file_system_options.h"
#include "webkit/fileapi/syncable/local_file_sync_context.h"
#include "webkit/quota/mock_special_storage_policy.h"
#include "webkit/quota/quota_manager.h"

using base::PlatformFileError;
using quota::QuotaManager;

namespace fileapi {

namespace {

typedef CannedSyncableFileSystem::StatusCallback StatusCallback;

void Empty() {}

void Quit() { MessageLoop::current()->Quit(); }

void AssignAndQuit(base::TaskRunner* original_task_runner,
                   PlatformFileError* result_out,
                   PlatformFileError result) {
  DCHECK(result_out);
  *result_out = result;
  original_task_runner->PostTask(FROM_HERE, base::Bind(&Quit));
}

PlatformFileError RunOnThread(
    base::SingleThreadTaskRunner* task_runner,
    const tracked_objects::Location& location,
    const base::Callback<void(const StatusCallback& callback)>& task) {
  PlatformFileError result = base::PLATFORM_FILE_ERROR_FAILED;
  task_runner->PostTask(
      location,
      base::Bind(task, base::Bind(&AssignAndQuit,
                                  base::MessageLoopProxy::current(),
                                  &result)));
  MessageLoop::current()->Run();
  return result;
}

void EnsureRunningOn(base::SingleThreadTaskRunner* runner) {
  EXPECT_TRUE(runner->RunsTasksOnCurrentThread());
}

void VerifySameTaskRunner(
    base::SingleThreadTaskRunner* runner1,
    base::SingleThreadTaskRunner* runner2) {
  ASSERT_TRUE(runner1 != NULL);
  ASSERT_TRUE(runner2 != NULL);
  runner1->PostTask(FROM_HERE,
                    base::Bind(&EnsureRunningOn, make_scoped_refptr(runner2)));
}

}  // namespace

CannedSyncableFileSystem::CannedSyncableFileSystem(
    const GURL& origin, const std::string& service,
    base::SingleThreadTaskRunner* io_task_runner)
    : service_name_(service),
      test_helper_(origin, kFileSystemTypeSyncable),
      result_(base::PLATFORM_FILE_OK),
      sync_status_(SYNC_STATUS_OK),
      io_task_runner_(io_task_runner),
      is_filesystem_set_up_(false),
      is_filesystem_opened_(false) {
}

CannedSyncableFileSystem::~CannedSyncableFileSystem() {}

void CannedSyncableFileSystem::SetUp() {
  ASSERT_FALSE(is_filesystem_set_up_);
  ASSERT_TRUE(data_dir_.CreateUniqueTempDir());

  scoped_refptr<quota::SpecialStoragePolicy> storage_policy =
      new quota::MockSpecialStoragePolicy();

  quota_manager_ = new QuotaManager(
      false /* is_incognito */,
      data_dir_.path(),
      io_task_runner_,
      base::MessageLoopProxy::current(),
      storage_policy);

  file_system_context_ = new FileSystemContext(
      FileSystemTaskRunners::CreateMockTaskRunners(),
      storage_policy,
      quota_manager_->proxy(),
      data_dir_.path(),
      CreateAllowFileAccessOptions());

  test_helper_.SetUp(file_system_context_.get(), NULL);
  is_filesystem_set_up_ = true;
}

void CannedSyncableFileSystem::TearDown() {
  quota_manager_ = NULL;
  test_helper_.TearDown();

  io_task_runner_->PostTaskAndReply(
      FROM_HERE, base::Bind(&Empty), base::Bind(&Quit));
  MessageLoop::current()->Run();
}

FileSystemURL CannedSyncableFileSystem::URL(const std::string& path) const {
  EXPECT_TRUE(is_filesystem_set_up_);
  EXPECT_TRUE(is_filesystem_opened_);
  return FileSystemURL(GURL(root_url_.spec() + path));
}

PlatformFileError CannedSyncableFileSystem::OpenFileSystem() {
  EXPECT_TRUE(is_filesystem_set_up_);
  EXPECT_FALSE(is_filesystem_opened_);
  file_system_context_->OpenSyncableFileSystem(
      service_name_,
      test_helper_.origin(), test_helper_.type(),
      true /* create */,
      base::Bind(&CannedSyncableFileSystem::DidOpenFileSystem,
                 base::Unretained(this)));
  MessageLoop::current()->RunAllPending();
  return result_;
}

SyncStatusCode CannedSyncableFileSystem::MaybeInitializeFileSystemContext(
    LocalFileSyncContext* sync_context) {
  DCHECK(sync_context);
  sync_status_ = SYNC_STATUS_UNKNOWN;
  VerifySameTaskRunner(io_task_runner_, sync_context->io_task_runner_);
  sync_context->MaybeInitializeFileSystemContext(
      test_helper_.origin(),
      file_system_context_,
      base::Bind(&CannedSyncableFileSystem::DidInitializeFileSystemContext,
                 base::Unretained(this)));
  MessageLoop::current()->Run();
  return sync_status_;
}

PlatformFileError CannedSyncableFileSystem::CreateDirectory(
    const FileSystemURL& url) {
  return RunOnThread(io_task_runner_,
                     FROM_HERE,
                     base::Bind(&CannedSyncableFileSystem::DoCreateDirectory,
                                base::Unretained(this), url));
}

PlatformFileError CannedSyncableFileSystem::CreateFile(
    const FileSystemURL& url) {
  return RunOnThread(io_task_runner_,
                     FROM_HERE,
                     base::Bind(&CannedSyncableFileSystem::DoCreateFile,
                                base::Unretained(this), url));
}

PlatformFileError CannedSyncableFileSystem::Copy(
    const FileSystemURL& src_url, const FileSystemURL& dest_url) {
  return RunOnThread(io_task_runner_,
                     FROM_HERE,
                     base::Bind(&CannedSyncableFileSystem::DoCopy,
                                base::Unretained(this), src_url, dest_url));
}

PlatformFileError CannedSyncableFileSystem::Move(
    const FileSystemURL& src_url, const FileSystemURL& dest_url) {
  return RunOnThread(io_task_runner_,
                     FROM_HERE,
                     base::Bind(&CannedSyncableFileSystem::DoMove,
                                base::Unretained(this), src_url, dest_url));
}

PlatformFileError CannedSyncableFileSystem::TruncateFile(
    const FileSystemURL& url, int64 size) {
  return RunOnThread(io_task_runner_,
                     FROM_HERE,
                     base::Bind(&CannedSyncableFileSystem::DoTruncateFile,
                                base::Unretained(this), url, size));
}

PlatformFileError CannedSyncableFileSystem::Remove(
    const FileSystemURL& url, bool recursive) {
  return RunOnThread(io_task_runner_,
                     FROM_HERE,
                     base::Bind(&CannedSyncableFileSystem::DoRemove,
                                base::Unretained(this), url, recursive));
}

PlatformFileError CannedSyncableFileSystem::DeleteFileSystem() {
  EXPECT_TRUE(is_filesystem_set_up_);
  return RunOnThread(io_task_runner_,
                     FROM_HERE,
                     base::Bind(&FileSystemContext::DeleteFileSystem,
                                file_system_context_,
                                test_helper_.origin(),
                                test_helper_.type()));
}

FileSystemOperation* CannedSyncableFileSystem::NewOperation() {
  return file_system_context_->CreateFileSystemOperation(URL(""), NULL);
}

void CannedSyncableFileSystem::DoCreateDirectory(
    const FileSystemURL& url,
    const StatusCallback& callback) {
  EXPECT_TRUE(is_filesystem_opened_);
  NewOperation()->CreateDirectory(
      url, false /* exclusive */, false /* recursive */, callback);
}

void CannedSyncableFileSystem::DoCreateFile(
    const FileSystemURL& url,
    const StatusCallback& callback) {
  EXPECT_TRUE(is_filesystem_opened_);
  NewOperation()->CreateFile(url, false /* exclusive */, callback);
}

void CannedSyncableFileSystem::DoCopy(
    const FileSystemURL& src_url,
    const FileSystemURL& dest_url,
    const StatusCallback& callback) {
  EXPECT_TRUE(is_filesystem_opened_);
  NewOperation()->Copy(src_url, dest_url, callback);
}

void CannedSyncableFileSystem::DoMove(
    const FileSystemURL& src_url,
    const FileSystemURL& dest_url,
    const StatusCallback& callback) {
  EXPECT_TRUE(is_filesystem_opened_);
  NewOperation()->Move(src_url, dest_url, callback);
}

void CannedSyncableFileSystem::DoTruncateFile(
    const FileSystemURL& url, int64 size,
    const StatusCallback& callback) {
  EXPECT_TRUE(is_filesystem_opened_);
  NewOperation()->Truncate(url, size, callback);
}

void CannedSyncableFileSystem::DoRemove(
    const FileSystemURL& url, bool recursive,
    const StatusCallback& callback) {
  EXPECT_TRUE(is_filesystem_opened_);
  NewOperation()->Remove(url, recursive, callback);
}

void CannedSyncableFileSystem::DidOpenFileSystem(
    PlatformFileError result, const std::string& name, const GURL& root) {
  result_ = result;
  root_url_ = root;
  is_filesystem_opened_ = true;
}

void CannedSyncableFileSystem::DidInitializeFileSystemContext(
    SyncStatusCode status) {
  sync_status_ = status;
  MessageLoop::current()->Quit();
}

}  // namespace fileapi
