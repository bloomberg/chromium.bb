// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_loop_proxy.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_job.h"
#include "net/url_request/url_request_job_factory_impl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "webkit/browser/blob/blob_storage_controller.h"
#include "webkit/browser/blob/blob_url_request_job.h"
#include "webkit/browser/blob/mock_blob_url_request_context.h"
#include "webkit/browser/fileapi/file_system_context.h"
#include "webkit/browser/fileapi/file_system_file_util.h"
#include "webkit/browser/fileapi/file_system_operation_context.h"
#include "webkit/browser/fileapi/file_system_operation_runner.h"
#include "webkit/browser/fileapi/local_file_util.h"
#include "webkit/browser/fileapi/mock_file_change_observer.h"
#include "webkit/browser/fileapi/mock_file_system_context.h"
#include "webkit/browser/fileapi/test_file_system_backend.h"
#include "webkit/browser/quota/mock_quota_manager.h"
#include "webkit/common/blob/blob_data.h"
#include "webkit/common/fileapi/file_system_util.h"

using webkit_blob::MockBlobURLRequestContext;
using webkit_blob::ScopedTextBlob;

namespace fileapi {

namespace {

const GURL kOrigin("http://example.com");
const FileSystemType kFileSystemType = kFileSystemTypeTest;

void AssertStatusEq(base::PlatformFileError expected,
                    base::PlatformFileError actual) {
  ASSERT_EQ(expected, actual);
}

}  // namespace

class FileSystemOperationImplWriteTest
    : public testing::Test {
 public:
  FileSystemOperationImplWriteTest()
      : loop_(base::MessageLoop::TYPE_IO),
        status_(base::PLATFORM_FILE_OK),
        cancel_status_(base::PLATFORM_FILE_ERROR_FAILED),
        bytes_written_(0),
        complete_(false),
        weak_factory_(this) {
    change_observers_ = MockFileChangeObserver::CreateList(&change_observer_);
  }

  virtual void SetUp() {
    ASSERT_TRUE(dir_.CreateUniqueTempDir());

    quota_manager_ =
        new quota::MockQuotaManager(false /* is_incognito */,
                                    dir_.path(),
                                    base::MessageLoopProxy::current().get(),
                                    base::MessageLoopProxy::current().get(),
                                    NULL /* special storage policy */);
    virtual_path_ = base::FilePath(FILE_PATH_LITERAL("temporary file"));

    file_system_context_ = CreateFileSystemContextForTesting(
        quota_manager_->proxy(), dir_.path());
    url_request_context_.reset(
        new MockBlobURLRequestContext(file_system_context_.get()));

    file_system_context_->operation_runner()->CreateFile(
        URLForPath(virtual_path_), true /* exclusive */,
        base::Bind(&AssertStatusEq, base::PLATFORM_FILE_OK));

    static_cast<TestFileSystemBackend*>(
        file_system_context_->GetFileSystemBackend(kFileSystemType))
        ->AddFileChangeObserver(change_observer());
  }

  virtual void TearDown() {
    quota_manager_ = NULL;
    file_system_context_ = NULL;
    base::MessageLoop::current()->RunUntilIdle();
  }

  base::PlatformFileError status() const { return status_; }
  base::PlatformFileError cancel_status() const { return cancel_status_; }
  void add_bytes_written(int64 bytes, bool complete) {
    bytes_written_ += bytes;
    EXPECT_FALSE(complete_);
    complete_ = complete;
  }
  int64 bytes_written() const { return bytes_written_; }
  bool complete() const { return complete_; }

 protected:
  const ChangeObserverList& change_observers() const {
    return change_observers_;
  }

  MockFileChangeObserver* change_observer() {
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

  void DidWrite(base::PlatformFileError status, int64 bytes, bool complete) {
    if (status == base::PLATFORM_FILE_OK) {
      add_bytes_written(bytes, complete);
      if (complete)
        base::MessageLoop::current()->Quit();
    } else {
      EXPECT_FALSE(complete_);
      EXPECT_EQ(status_, base::PLATFORM_FILE_OK);
      complete_ = true;
      status_ = status;
      if (base::MessageLoop::current()->is_running())
        base::MessageLoop::current()->Quit();
    }
  }

  void DidCancel(base::PlatformFileError status) {
    cancel_status_ = status;
  }

  const MockBlobURLRequestContext& url_request_context() const {
    return *url_request_context_;
  }

  scoped_refptr<FileSystemContext> file_system_context_;
  scoped_refptr<quota::MockQuotaManager> quota_manager_;

  base::MessageLoop loop_;

  base::ScopedTempDir dir_;
  base::FilePath virtual_path_;

  // For post-operation status.
  base::PlatformFileError status_;
  base::PlatformFileError cancel_status_;
  int64 bytes_written_;
  bool complete_;

  scoped_ptr<MockBlobURLRequestContext> url_request_context_;

  MockFileChangeObserver change_observer_;
  ChangeObserverList change_observers_;

  base::WeakPtrFactory<FileSystemOperationImplWriteTest> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(FileSystemOperationImplWriteTest);
};

TEST_F(FileSystemOperationImplWriteTest, TestWriteSuccess) {
  const GURL blob_url("blob:success");
  ScopedTextBlob blob(url_request_context(), blob_url, "Hello, world!\n");

  file_system_context_->operation_runner()->Write(
      &url_request_context(), URLForPath(virtual_path_), blob_url,
      0, RecordWriteCallback());
  base::MessageLoop::current()->Run();

  EXPECT_EQ(14, bytes_written());
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());
  EXPECT_TRUE(complete());

  EXPECT_EQ(1, change_observer()->get_and_reset_modify_file_count());
}

TEST_F(FileSystemOperationImplWriteTest, TestWriteZero) {
  GURL blob_url("blob:zero");
  scoped_refptr<webkit_blob::BlobData> blob_data(new webkit_blob::BlobData());

  url_request_context().blob_storage_controller()
      ->AddFinishedBlob(blob_url, blob_data.get());

  file_system_context_->operation_runner()->Write(
      &url_request_context(), URLForPath(virtual_path_),
      blob_url, 0, RecordWriteCallback());
  base::MessageLoop::current()->Run();

  url_request_context().blob_storage_controller()->RemoveBlob(blob_url);

  EXPECT_EQ(0, bytes_written());
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());
  EXPECT_TRUE(complete());

  EXPECT_EQ(1, change_observer()->get_and_reset_modify_file_count());
}

TEST_F(FileSystemOperationImplWriteTest, TestWriteInvalidBlobUrl) {
  file_system_context_->operation_runner()->Write(
      &url_request_context(), URLForPath(virtual_path_),
      GURL("blob:invalid"), 0, RecordWriteCallback());
  base::MessageLoop::current()->Run();

  EXPECT_EQ(0, bytes_written());
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_FAILED, status());
  EXPECT_TRUE(complete());

  EXPECT_EQ(0, change_observer()->get_and_reset_modify_file_count());
}

TEST_F(FileSystemOperationImplWriteTest, TestWriteInvalidFile) {
  GURL blob_url("blob:writeinvalidfile");
  ScopedTextBlob blob(url_request_context(), blob_url,
                      "It\'ll not be written.");

  file_system_context_->operation_runner()->Write(
      &url_request_context(),
      URLForPath(base::FilePath(FILE_PATH_LITERAL("nonexist"))),
      blob_url, 0, RecordWriteCallback());
  base::MessageLoop::current()->Run();

  EXPECT_EQ(0, bytes_written());
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND, status());
  EXPECT_TRUE(complete());

  EXPECT_EQ(1, change_observer()->get_and_reset_modify_file_count());
}

TEST_F(FileSystemOperationImplWriteTest, TestWriteDir) {
  base::FilePath virtual_dir_path(FILE_PATH_LITERAL("d"));
  file_system_context_->operation_runner()->CreateDirectory(
      URLForPath(virtual_dir_path),
      true /* exclusive */, false /* recursive */,
      base::Bind(&AssertStatusEq, base::PLATFORM_FILE_OK));

  GURL blob_url("blob:writedir");
  ScopedTextBlob blob(url_request_context(), blob_url,
                      "It\'ll not be written, too.");

  file_system_context_->operation_runner()->Write(
      &url_request_context(), URLForPath(virtual_dir_path),
                        blob_url, 0, RecordWriteCallback());
  base::MessageLoop::current()->Run();

  EXPECT_EQ(0, bytes_written());
  // TODO(kinuko): This error code is platform- or fileutil- dependent
  // right now.  Make it return PLATFORM_FILE_ERROR_NOT_A_FILE in every case.
  EXPECT_TRUE(status() == base::PLATFORM_FILE_ERROR_NOT_A_FILE ||
              status() == base::PLATFORM_FILE_ERROR_ACCESS_DENIED ||
              status() == base::PLATFORM_FILE_ERROR_FAILED);
  EXPECT_TRUE(complete());

  EXPECT_EQ(1, change_observer()->get_and_reset_modify_file_count());
}

TEST_F(FileSystemOperationImplWriteTest, TestWriteFailureByQuota) {
  GURL blob_url("blob:success");
  ScopedTextBlob blob(url_request_context(), blob_url, "Hello, world!\n");

  quota_manager_->SetQuota(
      kOrigin, FileSystemTypeToQuotaStorageType(kFileSystemType), 10);
  file_system_context_->operation_runner()->Write(
      &url_request_context(), URLForPath(virtual_path_), blob_url,
      0, RecordWriteCallback());
  base::MessageLoop::current()->Run();

  EXPECT_EQ(10, bytes_written());
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NO_SPACE, status());
  EXPECT_TRUE(complete());

  EXPECT_EQ(1, change_observer()->get_and_reset_modify_file_count());
}

TEST_F(FileSystemOperationImplWriteTest, TestImmediateCancelSuccessfulWrite) {
  GURL blob_url("blob:success");
  ScopedTextBlob blob(url_request_context(), blob_url, "Hello, world!\n");

  FileSystemOperationRunner::OperationID id =
      file_system_context_->operation_runner()->Write(
          &url_request_context(), URLForPath(virtual_path_),
          blob_url, 0, RecordWriteCallback());
  file_system_context_->operation_runner()->Cancel(id, RecordCancelCallback());
  // We use RunAllPendings() instead of Run() here, because we won't dispatch
  // callbacks after Cancel() is issued (so no chance to Quit) nor do we need
  // to run another write cycle.
  base::MessageLoop::current()->RunUntilIdle();

  // Issued Cancel() before receiving any response from Write(),
  // so nothing should have happen.
  EXPECT_EQ(0, bytes_written());
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_ABORT, status());
  EXPECT_EQ(base::PLATFORM_FILE_OK, cancel_status());
  EXPECT_TRUE(complete());

  EXPECT_EQ(0, change_observer()->get_and_reset_modify_file_count());
}

TEST_F(FileSystemOperationImplWriteTest, TestImmediateCancelFailingWrite) {
  GURL blob_url("blob:writeinvalidfile");
  ScopedTextBlob blob(url_request_context(), blob_url,
                      "It\'ll not be written.");

  FileSystemOperationRunner::OperationID id =
      file_system_context_->operation_runner()->Write(
          &url_request_context(),
          URLForPath(base::FilePath(FILE_PATH_LITERAL("nonexist"))),
          blob_url, 0, RecordWriteCallback());
  file_system_context_->operation_runner()->Cancel(id, RecordCancelCallback());
  // We use RunAllPendings() instead of Run() here, because we won't dispatch
  // callbacks after Cancel() is issued (so no chance to Quit) nor do we need
  // to run another write cycle.
  base::MessageLoop::current()->RunUntilIdle();

  // Issued Cancel() before receiving any response from Write(),
  // so nothing should have happen.
  EXPECT_EQ(0, bytes_written());
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_ABORT, status());
  EXPECT_EQ(base::PLATFORM_FILE_OK, cancel_status());
  EXPECT_TRUE(complete());

  EXPECT_EQ(0, change_observer()->get_and_reset_modify_file_count());
}

// TODO(ericu,dmikurube,kinuko): Add more tests for cancel cases.

}  // namespace fileapi
