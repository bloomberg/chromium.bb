// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/syncable/canned_syncable_file_system.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/file_util.h"
#include "base/message_loop_proxy.h"
#include "base/single_thread_task_runner.h"
#include "base/task_runner_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/blob/mock_blob_url_request_context.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_operation_context.h"
#include "webkit/fileapi/file_system_task_runners.h"
#include "webkit/fileapi/file_system_util.h"
#include "webkit/fileapi/isolated_context.h"
#include "webkit/fileapi/local_file_system_operation.h"
#include "webkit/fileapi/mock_file_system_options.h"
#include "webkit/fileapi/syncable/local_file_change_tracker.h"
#include "webkit/fileapi/syncable/local_file_sync_context.h"
#include "webkit/quota/mock_special_storage_policy.h"
#include "webkit/quota/quota_manager.h"

using base::PlatformFileError;
using quota::QuotaManager;
using webkit_blob::MockBlobURLRequestContext;
using webkit_blob::ScopedTextBlob;

namespace fileapi {

namespace {

void Empty() {}

void Quit() { MessageLoop::current()->Quit(); }

template <typename R>
void AssignAndQuit(base::TaskRunner* original_task_runner,
                   R* result_out, R result) {
  DCHECK(result_out);
  *result_out = result;
  original_task_runner->PostTask(FROM_HERE, base::Bind(&Quit));
}

template <typename R>
R RunOnThread(
    base::SingleThreadTaskRunner* task_runner,
    const tracked_objects::Location& location,
    const base::Callback<void(const base::Callback<void(R)>& callback)>& task) {
  R result;
  task_runner->PostTask(
      location,
      base::Bind(task, base::Bind(&AssignAndQuit<R>,
                                  base::MessageLoopProxy::current(),
                                  &result)));
  MessageLoop::current()->Run();
  return result;
}

void RunOnThread(base::SingleThreadTaskRunner* task_runner,
                 const tracked_objects::Location& location,
                 const base::Closure& task) {
  task_runner->PostTaskAndReply(
      location, task,
      base::Bind(base::IgnoreResult(
          base::Bind(&base::MessageLoopProxy::PostTask,
                     base::MessageLoopProxy::current(),
                     FROM_HERE, base::Bind(&Quit)))));
  MessageLoop::current()->Run();
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

class WriteHelper {
 public:
  WriteHelper() : bytes_written_(0) {}
  WriteHelper(MockBlobURLRequestContext* request_context,
              const GURL& blob_url,
              const std::string& blob_data)
      : bytes_written_(0),
        request_context_(request_context),
        blob_data_(new ScopedTextBlob(*request_context, blob_url, blob_data)) {}

  ~WriteHelper() {
    if (request_context_)
      MessageLoop::current()->DeleteSoon(FROM_HERE, request_context_.release());
  }

  void DidWrite(const base::Callback<void(int64 result)>& completion_callback,
                PlatformFileError error, int64 bytes, bool complete) {
    if (error == base::PLATFORM_FILE_OK) {
      bytes_written_ += bytes;
      if (!complete)
        return;
    }
    completion_callback.Run(error == base::PLATFORM_FILE_OK
                            ? bytes_written_ : static_cast<int64>(error));
  }

 private:
  int64 bytes_written_;
  scoped_ptr<MockBlobURLRequestContext> request_context_;
  scoped_ptr<ScopedTextBlob> blob_data_;

  DISALLOW_COPY_AND_ASSIGN(WriteHelper);
};

void DidGetUsageAndQuota(const quota::StatusCallback& callback,
                         int64* usage_out, int64* quota_out,
                         quota::QuotaStatusCode status,
                         int64 usage, int64 quota) {
  *usage_out = usage;
  *quota_out = quota;
  callback.Run(status);
}

}  // namespace

CannedSyncableFileSystem::CannedSyncableFileSystem(
    const GURL& origin, const std::string& service,
    base::SingleThreadTaskRunner* io_task_runner,
    base::SingleThreadTaskRunner* file_task_runner)
    : service_name_(service),
      test_helper_(origin, kFileSystemTypeSyncable),
      result_(base::PLATFORM_FILE_OK),
      sync_status_(SYNC_STATUS_OK),
      io_task_runner_(io_task_runner),
      file_task_runner_(file_task_runner),
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
      make_scoped_ptr(new FileSystemTaskRunners(
          io_task_runner_,
          file_task_runner_,
          file_task_runner_)),
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
  MessageLoop::current()->Run();
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
  return RunOnThread<PlatformFileError>(
      io_task_runner_,
      FROM_HERE,
      base::Bind(&CannedSyncableFileSystem::DoCreateDirectory,
                 base::Unretained(this), url));
}

PlatformFileError CannedSyncableFileSystem::CreateFile(
    const FileSystemURL& url) {
  return RunOnThread<PlatformFileError>(
      io_task_runner_,
      FROM_HERE,
      base::Bind(&CannedSyncableFileSystem::DoCreateFile,
                 base::Unretained(this), url));
}

PlatformFileError CannedSyncableFileSystem::Copy(
    const FileSystemURL& src_url, const FileSystemURL& dest_url) {
  return RunOnThread<PlatformFileError>(
      io_task_runner_,
      FROM_HERE,
      base::Bind(&CannedSyncableFileSystem::DoCopy,
                 base::Unretained(this), src_url, dest_url));
}

PlatformFileError CannedSyncableFileSystem::Move(
    const FileSystemURL& src_url, const FileSystemURL& dest_url) {
  return RunOnThread<PlatformFileError>(
      io_task_runner_,
      FROM_HERE,
      base::Bind(&CannedSyncableFileSystem::DoMove,
                 base::Unretained(this), src_url, dest_url));
}

PlatformFileError CannedSyncableFileSystem::TruncateFile(
    const FileSystemURL& url, int64 size) {
  return RunOnThread<PlatformFileError>(
      io_task_runner_,
      FROM_HERE,
      base::Bind(&CannedSyncableFileSystem::DoTruncateFile,
                 base::Unretained(this), url, size));
}

PlatformFileError CannedSyncableFileSystem::Remove(
    const FileSystemURL& url, bool recursive) {
  return RunOnThread<PlatformFileError>(
      io_task_runner_,
      FROM_HERE,
      base::Bind(&CannedSyncableFileSystem::DoRemove,
                 base::Unretained(this), url, recursive));
}

PlatformFileError CannedSyncableFileSystem::FileExists(
    const FileSystemURL& url) {
  return RunOnThread<PlatformFileError>(
      io_task_runner_,
      FROM_HERE,
      base::Bind(&CannedSyncableFileSystem::DoFileExists,
                 base::Unretained(this), url));
}

PlatformFileError CannedSyncableFileSystem::DirectoryExists(
    const FileSystemURL& url) {
  return RunOnThread<PlatformFileError>(
      io_task_runner_,
      FROM_HERE,
      base::Bind(&CannedSyncableFileSystem::DoDirectoryExists,
                 base::Unretained(this), url));
}

int64 CannedSyncableFileSystem::Write(
    net::URLRequestContext* url_request_context,
    const FileSystemURL& url, const GURL& blob_url) {
  return RunOnThread<int64>(
      io_task_runner_,
      FROM_HERE,
      base::Bind(&CannedSyncableFileSystem::DoWrite,
                 base::Unretained(this), url_request_context, url, blob_url));
}

int64 CannedSyncableFileSystem::WriteString(
    const FileSystemURL& url, const std::string& data) {
  return RunOnThread<int64>(
      io_task_runner_,
      FROM_HERE,
      base::Bind(&CannedSyncableFileSystem::DoWriteString,
                 base::Unretained(this), url, data));
}

PlatformFileError CannedSyncableFileSystem::DeleteFileSystem() {
  EXPECT_TRUE(is_filesystem_set_up_);
  return RunOnThread<PlatformFileError>(
      io_task_runner_,
      FROM_HERE,
      base::Bind(&FileSystemContext::DeleteFileSystem,
                 file_system_context_,
                 test_helper_.origin(),
                 test_helper_.type()));
}

quota::QuotaStatusCode CannedSyncableFileSystem::GetUsageAndQuota(
    int64* usage, int64* quota) {
  return RunOnThread<quota::QuotaStatusCode>(
      io_task_runner_,
      FROM_HERE,
      base::Bind(&CannedSyncableFileSystem::DoGetUsageAndQuota,
                 base::Unretained(this), usage, quota));
}

void CannedSyncableFileSystem::GetChangedURLsInTracker(
    std::vector<FileSystemURL>* urls) {
  return RunOnThread(
      file_task_runner_,
      FROM_HERE,
      base::Bind(&LocalFileChangeTracker::GetChangedURLs,
                 base::Unretained(file_system_context_->change_tracker()),
                 urls));
}

void CannedSyncableFileSystem::FinalizeSyncForURLInTracker(
    const FileSystemURL& url) {
  return RunOnThread(
      file_task_runner_,
      FROM_HERE,
      base::Bind(&LocalFileChangeTracker::FinalizeSyncForURL,
                 base::Unretained(file_system_context_->change_tracker()),
                 url));
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

void CannedSyncableFileSystem::DoFileExists(
    const FileSystemURL& url, const StatusCallback& callback) {
  EXPECT_TRUE(is_filesystem_opened_);
  NewOperation()->FileExists(url, callback);
}

void CannedSyncableFileSystem::DoDirectoryExists(
    const FileSystemURL& url, const StatusCallback& callback) {
  EXPECT_TRUE(is_filesystem_opened_);
  NewOperation()->DirectoryExists(url, callback);
}

void CannedSyncableFileSystem::DoWrite(
    net::URLRequestContext* url_request_context,
    const FileSystemURL& url, const GURL& blob_url,
    const WriteCallback& callback) {
  EXPECT_TRUE(is_filesystem_opened_);
  WriteHelper* helper = new WriteHelper;
  NewOperation()->Write(url_request_context, url, blob_url, 0,
                        base::Bind(&WriteHelper::DidWrite,
                                   base::Owned(helper), callback));
}

void CannedSyncableFileSystem::DoWriteString(
    const FileSystemURL& url,
    const std::string& data,
    const WriteCallback& callback) {
  MockBlobURLRequestContext* url_request_context(
      new MockBlobURLRequestContext(file_system_context_));
  const GURL blob_url(std::string("blob:") + data);
  WriteHelper* helper = new WriteHelper(url_request_context, blob_url, data);
  NewOperation()->Write(url_request_context, url, blob_url, 0,
                        base::Bind(&WriteHelper::DidWrite,
                                   base::Owned(helper), callback));
}

void CannedSyncableFileSystem::DoGetUsageAndQuota(
    int64* usage,
    int64* quota,
    const quota::StatusCallback& callback) {
  quota_manager_->GetUsageAndQuota(
      test_helper_.origin(),
      test_helper_.storage_type(),
      base::Bind(&DidGetUsageAndQuota, callback, usage, quota));
}

void CannedSyncableFileSystem::DidOpenFileSystem(
    PlatformFileError result, const std::string& name, const GURL& root) {
  result_ = result;
  root_url_ = root;
  is_filesystem_opened_ = true;
  MessageLoop::current()->Quit();
}

void CannedSyncableFileSystem::DidInitializeFileSystemContext(
    SyncStatusCode status) {
  sync_status_ = status;
  MessageLoop::current()->Quit();
}

}  // namespace fileapi
