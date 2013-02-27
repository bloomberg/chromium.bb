// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "googleurl/src/gurl.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_job.h"
#include "net/url_request/url_request_job_factory_impl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/blob/blob_data.h"
#include "webkit/blob/blob_storage_controller.h"
#include "webkit/blob/blob_url_request_job.h"
#include "webkit/blob/mock_blob_url_request_context.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_file_util.h"
#include "webkit/fileapi/file_system_util.h"
#include "webkit/fileapi/local_file_system_operation.h"
#include "webkit/fileapi/local_file_system_test_helper.h"
#include "webkit/fileapi/local_file_util.h"
#include "webkit/fileapi/mock_file_change_observer.h"
#include "webkit/quota/quota_manager.h"

using quota::QuotaManager;
using webkit_blob::MockBlobURLRequestContext;
using webkit_blob::ScopedTextBlob;

namespace fileapi {

namespace {

void AssertStatusEq(base::PlatformFileError expected,
                    base::PlatformFileError actual) {
  ASSERT_EQ(expected, actual);
}

class MockQuotaManager : public QuotaManager {
 public:
  MockQuotaManager(const base::FilePath& base_dir, int64 quota)
      : QuotaManager(false /* is_incognito */, base_dir,
                     base::MessageLoopProxy::current(),
                     base::MessageLoopProxy::current(),
                     NULL /* special_storage_policy */),
        usage_(0),
        quota_(quota) {}

  virtual void GetUsageAndQuota(
      const GURL& origin, quota::StorageType type,
      const GetUsageAndQuotaCallback& callback) OVERRIDE {
    callback.Run(quota::kQuotaStatusOk, usage_, quota_);
  }

  void set_usage(int64 usage) { usage_ = usage; }
  void set_quota(int64 quota) { quota_ = quota; }

 protected:
  virtual ~MockQuotaManager() {}

 private:
  int64 usage_;
  int64 quota_;
};

}  // namespace

class LocalFileSystemOperationWriteTest
    : public testing::Test,
      public base::SupportsWeakPtr<LocalFileSystemOperationWriteTest> {
 public:
  LocalFileSystemOperationWriteTest()
      : test_helper_(GURL("http://example.com"), kFileSystemTypeTest),
        loop_(MessageLoop::TYPE_IO),
        status_(base::PLATFORM_FILE_OK),
        cancel_status_(base::PLATFORM_FILE_ERROR_FAILED),
        bytes_written_(0),
        complete_(false),
        url_request_context_(test_helper_.file_system_context()) {
    change_observers_ = MockFileChangeObserver::CreateList(&change_observer_);
  }

  virtual void SetUp() {
    ASSERT_TRUE(dir_.CreateUniqueTempDir());
    base::FilePath base_dir = dir_.path().AppendASCII("filesystem");

    quota_manager_ = new MockQuotaManager(base_dir, 1024);
    test_helper_.SetUp(base_dir,
                      false /* unlimited quota */,
                      quota_manager_->proxy());
    virtual_path_ = base::FilePath(FILE_PATH_LITERAL("temporary file"));

    NewOperation()->CreateFile(
        URLForPath(virtual_path_), true /* exclusive */,
        base::Bind(&AssertStatusEq, base::PLATFORM_FILE_OK));
  }

  virtual void TearDown() {
    quota_manager_ = NULL;
    test_helper_.TearDown();
  }

  LocalFileSystemOperation* NewOperation() {
    LocalFileSystemOperation* operation = test_helper_.NewOperation();
    operation->operation_context()->set_change_observers(change_observers());
    return operation;
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
    return test_helper_.CreateURL(path);
  }

  // Callback function for recording test results.
  FileSystemOperation::WriteCallback RecordWriteCallback() {
    return base::Bind(&LocalFileSystemOperationWriteTest::DidWrite,
                      AsWeakPtr());
  }

  FileSystemOperation::StatusCallback RecordCancelCallback() {
    return base::Bind(&LocalFileSystemOperationWriteTest::DidCancel,
                      AsWeakPtr());
  }

  void DidWrite(base::PlatformFileError status, int64 bytes, bool complete) {
    if (status == base::PLATFORM_FILE_OK) {
      add_bytes_written(bytes, complete);
      if (complete)
        MessageLoop::current()->Quit();
    } else {
      EXPECT_FALSE(complete_);
      EXPECT_EQ(status_, base::PLATFORM_FILE_OK);
      complete_ = true;
      status_ = status;
      if (MessageLoop::current()->is_running())
        MessageLoop::current()->Quit();
    }
  }

  void DidCancel(base::PlatformFileError status) {
    cancel_status_ = status;
  }

  FileSystemFileUtil* file_util() {
    return test_helper_.file_util();
  }

  scoped_refptr<MockQuotaManager> quota_manager_;
  LocalFileSystemTestOriginHelper test_helper_;

  MessageLoop loop_;

  base::ScopedTempDir dir_;
  base::FilePath virtual_path_;

  // For post-operation status.
  base::PlatformFileError status_;
  base::PlatformFileError cancel_status_;
  int64 bytes_written_;
  bool complete_;

  MockBlobURLRequestContext url_request_context_;

 private:
  MockFileChangeObserver change_observer_;
  ChangeObserverList change_observers_;

  DISALLOW_COPY_AND_ASSIGN(LocalFileSystemOperationWriteTest);
};

TEST_F(LocalFileSystemOperationWriteTest, TestWriteSuccess) {
  const GURL blob_url("blob:success");
  ScopedTextBlob blob(url_request_context_, blob_url, "Hello, world!\n");

  NewOperation()->Write(
      &url_request_context_, URLForPath(virtual_path_), blob_url,
      0, RecordWriteCallback());
  MessageLoop::current()->Run();

  EXPECT_EQ(14, bytes_written());
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());
  EXPECT_TRUE(complete());

  EXPECT_EQ(1, change_observer()->get_and_reset_modify_file_count());
}

TEST_F(LocalFileSystemOperationWriteTest, TestWriteZero) {
  GURL blob_url("blob:zero");
  scoped_refptr<webkit_blob::BlobData> blob_data(new webkit_blob::BlobData());

  url_request_context_.blob_storage_controller()->AddFinishedBlob(
      blob_url, blob_data);

  NewOperation()->Write(
      &url_request_context_, URLForPath(virtual_path_),
      blob_url, 0, RecordWriteCallback());
  MessageLoop::current()->Run();

  url_request_context_.blob_storage_controller()->RemoveBlob(blob_url);

  EXPECT_EQ(0, bytes_written());
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());
  EXPECT_TRUE(complete());

  EXPECT_EQ(1, change_observer()->get_and_reset_modify_file_count());
}

TEST_F(LocalFileSystemOperationWriteTest, TestWriteInvalidBlobUrl) {
  NewOperation()->Write(
      &url_request_context_, URLForPath(virtual_path_),
      GURL("blob:invalid"), 0, RecordWriteCallback());
  MessageLoop::current()->Run();

  EXPECT_EQ(0, bytes_written());
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_FAILED, status());
  EXPECT_TRUE(complete());

  EXPECT_EQ(0, change_observer()->get_and_reset_modify_file_count());
}

TEST_F(LocalFileSystemOperationWriteTest, TestWriteInvalidFile) {
  GURL blob_url("blob:writeinvalidfile");
  ScopedTextBlob blob(url_request_context_, blob_url, "It\'ll not be written.");

  NewOperation()->Write(
      &url_request_context_,
      URLForPath(base::FilePath(FILE_PATH_LITERAL("nonexist"))),
      blob_url, 0, RecordWriteCallback());
  MessageLoop::current()->Run();

  EXPECT_EQ(0, bytes_written());
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND, status());
  EXPECT_TRUE(complete());

  EXPECT_EQ(1, change_observer()->get_and_reset_modify_file_count());
}

TEST_F(LocalFileSystemOperationWriteTest, TestWriteDir) {
  base::FilePath virtual_dir_path(FILE_PATH_LITERAL("d"));
  NewOperation()->CreateDirectory(
      URLForPath(virtual_dir_path),
      true /* exclusive */, false /* recursive */,
      base::Bind(&AssertStatusEq, base::PLATFORM_FILE_OK));

  GURL blob_url("blob:writedir");
  ScopedTextBlob blob(url_request_context_, blob_url,
                      "It\'ll not be written, too.");

  NewOperation()->Write(&url_request_context_, URLForPath(virtual_dir_path),
                        blob_url, 0, RecordWriteCallback());
  MessageLoop::current()->Run();

  EXPECT_EQ(0, bytes_written());
  // TODO(kinuko): This error code is platform- or fileutil- dependent
  // right now.  Make it return PLATFORM_FILE_ERROR_NOT_A_FILE in every case.
  EXPECT_TRUE(status() == base::PLATFORM_FILE_ERROR_NOT_A_FILE ||
              status() == base::PLATFORM_FILE_ERROR_ACCESS_DENIED ||
              status() == base::PLATFORM_FILE_ERROR_FAILED);
  EXPECT_TRUE(complete());

  EXPECT_EQ(1, change_observer()->get_and_reset_modify_file_count());
}

TEST_F(LocalFileSystemOperationWriteTest, TestWriteFailureByQuota) {
  GURL blob_url("blob:success");
  ScopedTextBlob blob(url_request_context_, blob_url, "Hello, world!\n");

  quota_manager_->set_quota(10);
  NewOperation()->Write(
      &url_request_context_, URLForPath(virtual_path_), blob_url,
      0, RecordWriteCallback());
  MessageLoop::current()->Run();

  EXPECT_EQ(10, bytes_written());
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NO_SPACE, status());
  EXPECT_TRUE(complete());

  EXPECT_EQ(1, change_observer()->get_and_reset_modify_file_count());
}

TEST_F(LocalFileSystemOperationWriteTest, TestImmediateCancelSuccessfulWrite) {
  GURL blob_url("blob:success");
  ScopedTextBlob blob(url_request_context_, blob_url, "Hello, world!\n");

  FileSystemOperation* write_operation = NewOperation();
  write_operation->Write(&url_request_context_, URLForPath(virtual_path_),
                         blob_url, 0, RecordWriteCallback());
  write_operation->Cancel(RecordCancelCallback());
  // We use RunAllPendings() instead of Run() here, because we won't dispatch
  // callbacks after Cancel() is issued (so no chance to Quit) nor do we need
  // to run another write cycle.
  MessageLoop::current()->RunUntilIdle();

  // Issued Cancel() before receiving any response from Write(),
  // so nothing should have happen.
  EXPECT_EQ(0, bytes_written());
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_ABORT, status());
  EXPECT_EQ(base::PLATFORM_FILE_OK, cancel_status());
  EXPECT_TRUE(complete());

  EXPECT_EQ(0, change_observer()->get_and_reset_modify_file_count());
}

TEST_F(LocalFileSystemOperationWriteTest, TestImmediateCancelFailingWrite) {
  GURL blob_url("blob:writeinvalidfile");
  ScopedTextBlob blob(url_request_context_, blob_url, "It\'ll not be written.");

  FileSystemOperation* write_operation = NewOperation();
  write_operation->Write(
      &url_request_context_,
      URLForPath(base::FilePath(FILE_PATH_LITERAL("nonexist"))),
      blob_url, 0, RecordWriteCallback());
  write_operation->Cancel(RecordCancelCallback());
  // We use RunAllPendings() instead of Run() here, because we won't dispatch
  // callbacks after Cancel() is issued (so no chance to Quit) nor do we need
  // to run another write cycle.
  MessageLoop::current()->RunUntilIdle();

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
