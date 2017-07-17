// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_job.h"
#include "net/url_request/url_request_job_factory_impl.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/blob/blob_url_request_job.h"
#include "storage/browser/fileapi/file_system_context.h"
#include "storage/browser/fileapi/file_system_file_util.h"
#include "storage/browser/fileapi/file_system_operation_context.h"
#include "storage/browser/fileapi/file_system_operation_runner.h"
#include "storage/browser/fileapi/local_file_util.h"
#include "storage/browser/test/mock_blob_url_request_context.h"
#include "storage/browser/test/mock_file_change_observer.h"
#include "storage/browser/test/mock_quota_manager.h"
#include "storage/browser/test/test_file_system_backend.h"
#include "storage/browser/test/test_file_system_context.h"
#include "storage/common/fileapi/file_system_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using storage::FileSystemOperation;
using storage::FileSystemOperationRunner;
using storage::FileSystemURL;
using content::MockBlobURLRequestContext;
using content::ScopedTextBlob;

namespace content {

namespace {

const GURL kOrigin("http://example.com");
const storage::FileSystemType kFileSystemType = storage::kFileSystemTypeTest;

void AssertStatusEq(base::File::Error expected, base::File::Error actual) {
  ASSERT_EQ(expected, actual);
}

}  // namespace

class FileSystemOperationImplWriteTest : public testing::Test {
 public:
  FileSystemOperationImplWriteTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::IO),
        status_(base::File::FILE_OK),
        cancel_status_(base::File::FILE_ERROR_FAILED),
        bytes_written_(0),
        complete_(false),
        weak_factory_(this) {
    change_observers_ =
        storage::MockFileChangeObserver::CreateList(&change_observer_);
  }

  void SetUp() override {
    ASSERT_TRUE(dir_.CreateUniqueTempDir());

    quota_manager_ =
        new MockQuotaManager(false /* is_incognito */, dir_.GetPath(),
                             base::ThreadTaskRunnerHandle::Get().get(),
                             base::ThreadTaskRunnerHandle::Get().get(),
                             NULL /* special storage policy */);
    virtual_path_ = base::FilePath(FILE_PATH_LITERAL("temporary file"));

    file_system_context_ = CreateFileSystemContextForTesting(
        quota_manager_->proxy(), dir_.GetPath());
    url_request_context_.reset(
        new MockBlobURLRequestContext(file_system_context_.get()));

    file_system_context_->operation_runner()->CreateFile(
        URLForPath(virtual_path_), true /* exclusive */,
        base::Bind(&AssertStatusEq, base::File::FILE_OK));

    static_cast<TestFileSystemBackend*>(
        file_system_context_->GetFileSystemBackend(kFileSystemType))
        ->AddFileChangeObserver(change_observer());
  }

  void TearDown() override {
    quota_manager_ = NULL;
    file_system_context_ = NULL;
    base::RunLoop().RunUntilIdle();
  }

  base::File::Error status() const { return status_; }
  base::File::Error cancel_status() const { return cancel_status_; }
  void add_bytes_written(int64_t bytes, bool complete) {
    bytes_written_ += bytes;
    EXPECT_FALSE(complete_);
    complete_ = complete;
  }
  int64_t bytes_written() const { return bytes_written_; }
  bool complete() const { return complete_; }

 protected:
  const storage::ChangeObserverList& change_observers() const {
    return change_observers_;
  }

  storage::MockFileChangeObserver* change_observer() {
    return &change_observer_;
  }

  FileSystemURL URLForPath(const base::FilePath& path) const {
    return file_system_context_->CreateCrackedFileSystemURL(
        kOrigin, kFileSystemType, path);
  }

  // Callback function for recording test results.
  FileSystemOperation::WriteCallback RecordWriteCallback() {
    return base::Bind(&FileSystemOperationImplWriteTest::DidWrite,
                      weak_factory_.GetWeakPtr());
  }

  FileSystemOperation::StatusCallback RecordCancelCallback() {
    return base::Bind(&FileSystemOperationImplWriteTest::DidCancel,
                      weak_factory_.GetWeakPtr());
  }

  void DidWrite(base::File::Error status, int64_t bytes, bool complete) {
    if (status == base::File::FILE_OK) {
      add_bytes_written(bytes, complete);
      if (complete)
        base::MessageLoop::current()->QuitWhenIdle();
    } else {
      EXPECT_FALSE(complete_);
      EXPECT_EQ(status_, base::File::FILE_OK);
      complete_ = true;
      status_ = status;
      if (base::RunLoop::IsRunningOnCurrentThread())
        base::MessageLoop::current()->QuitWhenIdle();
    }
  }

  void DidCancel(base::File::Error status) { cancel_status_ = status; }

  const MockBlobURLRequestContext& url_request_context() const {
    return *url_request_context_;
  }

  base::test::ScopedTaskEnvironment scoped_task_environment_;

  scoped_refptr<storage::FileSystemContext> file_system_context_;
  scoped_refptr<MockQuotaManager> quota_manager_;

  base::ScopedTempDir dir_;
  base::FilePath virtual_path_;

  // For post-operation status.
  base::File::Error status_;
  base::File::Error cancel_status_;
  int64_t bytes_written_;
  bool complete_;

  std::unique_ptr<MockBlobURLRequestContext> url_request_context_;

  storage::MockFileChangeObserver change_observer_;
  storage::ChangeObserverList change_observers_;

  base::WeakPtrFactory<FileSystemOperationImplWriteTest> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(FileSystemOperationImplWriteTest);
};

TEST_F(FileSystemOperationImplWriteTest, TestWriteSuccess) {
  ScopedTextBlob blob(url_request_context(), "blob-id:success",
                      "Hello, world!\n");
  file_system_context_->operation_runner()->Write(
      &url_request_context(), URLForPath(virtual_path_),
      blob.GetBlobDataHandle(), 0, RecordWriteCallback());
  base::RunLoop().Run();

  EXPECT_EQ(14, bytes_written());
  EXPECT_EQ(base::File::FILE_OK, status());
  EXPECT_TRUE(complete());

  EXPECT_EQ(1, change_observer()->get_and_reset_modify_file_count());
}

TEST_F(FileSystemOperationImplWriteTest, TestWriteZero) {
  ScopedTextBlob blob(url_request_context(), "blob_id:zero", "");
  file_system_context_->operation_runner()->Write(
      &url_request_context(), URLForPath(virtual_path_),
      blob.GetBlobDataHandle(), 0, RecordWriteCallback());
  base::RunLoop().Run();

  EXPECT_EQ(0, bytes_written());
  EXPECT_EQ(base::File::FILE_OK, status());
  EXPECT_TRUE(complete());

  EXPECT_EQ(1, change_observer()->get_and_reset_modify_file_count());
}

TEST_F(FileSystemOperationImplWriteTest, TestWriteInvalidBlobUrl) {
  std::unique_ptr<storage::BlobDataHandle> null_handle;
  file_system_context_->operation_runner()->Write(
      &url_request_context(), URLForPath(virtual_path_), std::move(null_handle),
      0, RecordWriteCallback());
  base::RunLoop().Run();

  EXPECT_EQ(0, bytes_written());
  EXPECT_EQ(base::File::FILE_ERROR_FAILED, status());
  EXPECT_TRUE(complete());

  EXPECT_EQ(0, change_observer()->get_and_reset_modify_file_count());
}

TEST_F(FileSystemOperationImplWriteTest, TestWriteInvalidFile) {
  ScopedTextBlob blob(url_request_context(), "blob_id:writeinvalidfile",
                      "It\'ll not be written.");
  file_system_context_->operation_runner()->Write(
      &url_request_context(),
      URLForPath(base::FilePath(FILE_PATH_LITERAL("nonexist"))),
      blob.GetBlobDataHandle(), 0, RecordWriteCallback());
  base::RunLoop().Run();

  EXPECT_EQ(0, bytes_written());
  EXPECT_EQ(base::File::FILE_ERROR_NOT_FOUND, status());
  EXPECT_TRUE(complete());

  EXPECT_EQ(1, change_observer()->get_and_reset_modify_file_count());
}

TEST_F(FileSystemOperationImplWriteTest, TestWriteDir) {
  base::FilePath virtual_dir_path(FILE_PATH_LITERAL("d"));
  file_system_context_->operation_runner()->CreateDirectory(
      URLForPath(virtual_dir_path), true /* exclusive */, false /* recursive */,
      base::Bind(&AssertStatusEq, base::File::FILE_OK));

  ScopedTextBlob blob(url_request_context(), "blob:writedir",
                      "It\'ll not be written, too.");
  file_system_context_->operation_runner()->Write(
      &url_request_context(), URLForPath(virtual_dir_path),
      blob.GetBlobDataHandle(), 0, RecordWriteCallback());
  base::RunLoop().Run();

  EXPECT_EQ(0, bytes_written());
  // TODO(kinuko): This error code is platform- or fileutil- dependent
  // right now.  Make it return File::FILE_ERROR_NOT_A_FILE in every case.
  EXPECT_TRUE(status() == base::File::FILE_ERROR_NOT_A_FILE ||
              status() == base::File::FILE_ERROR_ACCESS_DENIED ||
              status() == base::File::FILE_ERROR_FAILED);
  EXPECT_TRUE(complete());

  EXPECT_EQ(1, change_observer()->get_and_reset_modify_file_count());
}

TEST_F(FileSystemOperationImplWriteTest, TestWriteFailureByQuota) {
  ScopedTextBlob blob(url_request_context(), "blob:success", "Hello, world!\n");
  quota_manager_->SetQuota(
      kOrigin, FileSystemTypeToQuotaStorageType(kFileSystemType), 10);
  file_system_context_->operation_runner()->Write(
      &url_request_context(), URLForPath(virtual_path_),
      blob.GetBlobDataHandle(), 0, RecordWriteCallback());
  base::RunLoop().Run();

  EXPECT_EQ(10, bytes_written());
  EXPECT_EQ(base::File::FILE_ERROR_NO_SPACE, status());
  EXPECT_TRUE(complete());

  EXPECT_EQ(1, change_observer()->get_and_reset_modify_file_count());
}

TEST_F(FileSystemOperationImplWriteTest, TestImmediateCancelSuccessfulWrite) {
  ScopedTextBlob blob(url_request_context(), "blob:success", "Hello, world!\n");
  FileSystemOperationRunner::OperationID id =
      file_system_context_->operation_runner()->Write(
          &url_request_context(), URLForPath(virtual_path_),
          blob.GetBlobDataHandle(), 0, RecordWriteCallback());
  file_system_context_->operation_runner()->Cancel(id, RecordCancelCallback());
  // We use RunAllPendings() instead of Run() here, because we won't dispatch
  // callbacks after Cancel() is issued (so no chance to Quit) nor do we need
  // to run another write cycle.
  base::RunLoop().RunUntilIdle();

  // Issued Cancel() before receiving any response from Write(),
  // so nothing should have happen.
  EXPECT_EQ(0, bytes_written());
  EXPECT_EQ(base::File::FILE_ERROR_ABORT, status());
  EXPECT_EQ(base::File::FILE_OK, cancel_status());
  EXPECT_TRUE(complete());

  EXPECT_EQ(0, change_observer()->get_and_reset_modify_file_count());
}

TEST_F(FileSystemOperationImplWriteTest, TestImmediateCancelFailingWrite) {
  ScopedTextBlob blob(url_request_context(), "blob:writeinvalidfile",
                      "It\'ll not be written.");
  FileSystemOperationRunner::OperationID id =
      file_system_context_->operation_runner()->Write(
          &url_request_context(),
          URLForPath(base::FilePath(FILE_PATH_LITERAL("nonexist"))),
          blob.GetBlobDataHandle(), 0, RecordWriteCallback());
  file_system_context_->operation_runner()->Cancel(id, RecordCancelCallback());
  // We use RunAllPendings() instead of Run() here, because we won't dispatch
  // callbacks after Cancel() is issued (so no chance to Quit) nor do we need
  // to run another write cycle.
  base::RunLoop().RunUntilIdle();

  // Issued Cancel() before receiving any response from Write(),
  // so nothing should have happen.
  EXPECT_EQ(0, bytes_written());
  EXPECT_EQ(base::File::FILE_ERROR_ABORT, status());
  EXPECT_EQ(base::File::FILE_OK, cancel_status());
  EXPECT_TRUE(complete());

  EXPECT_EQ(0, change_observer()->get_and_reset_modify_file_count());
}

// TODO(ericu,dmikurube,kinuko): Add more tests for cancel cases.

}  // namespace content
