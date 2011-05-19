// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// NOTE: These tests are run as part of "unit_tests" (in chrome/test/unit)
// rather than as part of test_shell_tests because they rely on being able
// to instantiate a MessageLoop of type TYPE_IO.  test_shell_tests uses
// TYPE_UI, which URLRequest doesn't allow.
//

#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "base/scoped_temp_dir.h"
#include "googleurl/src/gurl.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_job.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/blob/blob_data.h"
#include "webkit/blob/blob_storage_controller.h"
#include "webkit/blob/blob_url_request_job.h"
#include "webkit/fileapi/file_system_callback_dispatcher.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_file_util.h"
#include "webkit/fileapi/file_system_operation.h"
#include "webkit/fileapi/file_system_path_manager.h"
#include "webkit/fileapi/file_system_test_helper.h"
#include "webkit/fileapi/file_system_util.h"
#include "webkit/fileapi/local_file_system_file_util.h"
#include "webkit/quota/quota_manager.h"

using quota::QuotaManager;

namespace fileapi {

namespace {

class MockQuotaManager : public QuotaManager {
 public:
  MockQuotaManager(const FilePath& base_dir, int64 quota)
      : QuotaManager(false /* is_incognito */, base_dir,
                     base::MessageLoopProxy::CreateForCurrentThread(),
                     base::MessageLoopProxy::CreateForCurrentThread()),
        usage_(0),
        quota_(quota) {}

  virtual void GetUsageAndQuota(const GURL& origin, quota::StorageType type,
                                GetUsageAndQuotaCallback* callback) {
    callback->Run(quota::kQuotaStatusOk, usage_, quota_);
    delete callback;
  }

  void set_usage(int64 usage) { usage_ = usage; }
  void set_quota(int64 quota) { quota_ = quota; }

 private:
  int64 usage_;
  int64 quota_;
};

}  // namespace (anonymous)

class FileSystemOperationWriteTest : public testing::Test {
 public:
  FileSystemOperationWriteTest()
      : loop_(MessageLoop::TYPE_IO),
        status_(base::PLATFORM_FILE_OK),
        bytes_written_(0),
        complete_(false) {}

  FileSystemOperation* operation();

  void set_failure_status(base::PlatformFileError status) {
    EXPECT_FALSE(complete_);
    EXPECT_EQ(status_, base::PLATFORM_FILE_OK);
    EXPECT_NE(status, base::PLATFORM_FILE_OK);
    complete_ = true;
    status_ = status;
  }
  base::PlatformFileError status() const { return status_; }
  void add_bytes_written(int64 bytes, bool complete) {
    bytes_written_ += bytes;
    EXPECT_FALSE(complete_);
    complete_ = complete;
  }
  int64 bytes_written() const { return bytes_written_; }
  bool complete() const { return complete_; }

  virtual void SetUp();
  virtual void TearDown();

 protected:
  GURL URLForPath(const FilePath& path) const {
    return test_helper_.GetURLForPath(path);
  }

  scoped_refptr<MockQuotaManager> quota_manager_;
  FileSystemTestOriginHelper test_helper_;

  MessageLoop loop_;

  ScopedTempDir dir_;
  FilePath filesystem_dir_;
  FilePath file_;
  FilePath virtual_path_;

  // For post-operation status.
  base::PlatformFileError status_;
  int64 bytes_written_;
  bool complete_;

  DISALLOW_COPY_AND_ASSIGN(FileSystemOperationWriteTest);
};

namespace {

class TestURLRequestContext : public net::URLRequestContext {
 public:
  webkit_blob::BlobStorageController* blob_storage_controller() {
    return &blob_storage_controller_;
  }

 private:
  webkit_blob::BlobStorageController blob_storage_controller_;
};

static net::URLRequestJob* BlobURLRequestJobFactory(net::URLRequest* request,
                                                    const std::string& scheme) {
  webkit_blob::BlobStorageController* blob_storage_controller =
      static_cast<TestURLRequestContext*>(request->context())->
          blob_storage_controller();
  return new webkit_blob::BlobURLRequestJob(
      request,
      blob_storage_controller->GetBlobDataFromUrl(request->url()),
      base::MessageLoopProxy::CreateForCurrentThread());
}

class MockDispatcher : public FileSystemCallbackDispatcher {
 public:
  MockDispatcher(FileSystemOperationWriteTest* test) : test_(test) { }

  virtual void DidFail(base::PlatformFileError status) {
    test_->set_failure_status(status);
    MessageLoop::current()->Quit();
  }

  virtual void DidSucceed() {
    ADD_FAILURE();
  }

  virtual void DidReadMetadata(
      const base::PlatformFileInfo& info,
      const FilePath& platform_path) {
    ADD_FAILURE();
  }

  virtual void DidReadDirectory(
      const std::vector<base::FileUtilProxy::Entry>& entries,
      bool /* has_more */) {
    ADD_FAILURE();
  }

  virtual void DidOpenFileSystem(const std::string&, const GURL&) {
    ADD_FAILURE();
  }

  virtual void DidWrite(int64 bytes, bool complete) {
    test_->add_bytes_written(bytes, complete);
    if (complete)
      MessageLoop::current()->Quit();
  }

 private:
  FileSystemOperationWriteTest* test_;
};

}  // namespace (anonymous)

void FileSystemOperationWriteTest::SetUp() {
  ASSERT_TRUE(dir_.CreateUniqueTempDir());
  FilePath base_dir = dir_.path().AppendASCII("filesystem");

  quota_manager_ = new MockQuotaManager(base_dir, 1024);
  test_helper_.SetUp(base_dir,
                     false /* incognito */,
                     false /* unlimited quota */,
                     quota_manager_->proxy(),
                     LocalFileSystemFileUtil::GetInstance());
  filesystem_dir_ = test_helper_.GetOriginRootPath();

  ASSERT_TRUE(file_util::CreateTemporaryFileInDir(filesystem_dir_, &file_));
  virtual_path_ = file_.BaseName();

  net::URLRequest::RegisterProtocolFactory("blob", &BlobURLRequestJobFactory);
}

void FileSystemOperationWriteTest::TearDown() {
  net::URLRequest::RegisterProtocolFactory("blob", NULL);
  quota_manager_ = NULL;
  test_helper_.TearDown();
}

FileSystemOperation* FileSystemOperationWriteTest::operation() {
  return test_helper_.NewOperation(new MockDispatcher(this));
}

TEST_F(FileSystemOperationWriteTest, TestWriteSuccess) {
  GURL blob_url("blob:success");
  scoped_refptr<webkit_blob::BlobData> blob_data(new webkit_blob::BlobData());
  blob_data->AppendData("Hello, world!\n");

  scoped_refptr<TestURLRequestContext> url_request_context(
      new TestURLRequestContext());
  url_request_context->blob_storage_controller()->
      RegisterBlobUrl(blob_url, blob_data);

  operation()->Write(url_request_context, URLForPath(virtual_path_), blob_url,
                     0);
  MessageLoop::current()->Run();

  url_request_context->blob_storage_controller()->UnregisterBlobUrl(blob_url);

  EXPECT_EQ(14, bytes_written());
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());
  EXPECT_TRUE(complete());
}

TEST_F(FileSystemOperationWriteTest, TestWriteZero) {
  GURL blob_url("blob:zero");
  scoped_refptr<webkit_blob::BlobData> blob_data(new webkit_blob::BlobData());
  blob_data->AppendData("");

  scoped_refptr<TestURLRequestContext> url_request_context(
      new TestURLRequestContext());
  url_request_context->blob_storage_controller()->
      RegisterBlobUrl(blob_url, blob_data);

  operation()->Write(url_request_context, URLForPath(virtual_path_),
                     blob_url, 0);
  MessageLoop::current()->Run();

  url_request_context->blob_storage_controller()->UnregisterBlobUrl(blob_url);

  EXPECT_EQ(0, bytes_written());
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());
  EXPECT_TRUE(complete());
}

TEST_F(FileSystemOperationWriteTest, TestWriteInvalidBlobUrl) {
  scoped_refptr<TestURLRequestContext> url_request_context(
      new TestURLRequestContext());

  operation()->Write(url_request_context, URLForPath(virtual_path_),
      GURL("blob:invalid"), 0);
  MessageLoop::current()->Run();

  EXPECT_EQ(0, bytes_written());
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());
  EXPECT_TRUE(complete());
}

TEST_F(FileSystemOperationWriteTest, TestWriteInvalidFile) {
  GURL blob_url("blob:writeinvalidfile");
  scoped_refptr<webkit_blob::BlobData> blob_data(new webkit_blob::BlobData());
  blob_data->AppendData("It\'ll not be written.");

  scoped_refptr<TestURLRequestContext> url_request_context(
      new TestURLRequestContext());
  url_request_context->blob_storage_controller()->
      RegisterBlobUrl(blob_url, blob_data);

  operation()->Write(url_request_context,
                     URLForPath(FilePath(FILE_PATH_LITERAL("nonexist"))),
                     blob_url, 0);
  MessageLoop::current()->Run();

  url_request_context->blob_storage_controller()->UnregisterBlobUrl(blob_url);

  EXPECT_EQ(0, bytes_written());
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND, status());
  EXPECT_TRUE(complete());
}

TEST_F(FileSystemOperationWriteTest, TestWriteDir) {
  FilePath subdir;
  ASSERT_TRUE(file_util::CreateTemporaryDirInDir(filesystem_dir_,
                                                 FILE_PATH_LITERAL("d"),
                                                 &subdir));
  FilePath virtual_subdir_path = subdir.BaseName();

  GURL blob_url("blob:writedir");
  scoped_refptr<webkit_blob::BlobData> blob_data(new webkit_blob::BlobData());
  blob_data->AppendData("It\'ll not be written, too.");

  scoped_refptr<TestURLRequestContext> url_request_context(
      new TestURLRequestContext());
  url_request_context->blob_storage_controller()->
      RegisterBlobUrl(blob_url, blob_data);

  operation()->Write(url_request_context, URLForPath(virtual_subdir_path),
                     blob_url, 0);
  MessageLoop::current()->Run();

  url_request_context->blob_storage_controller()->UnregisterBlobUrl(blob_url);

  EXPECT_EQ(0, bytes_written());
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_ACCESS_DENIED, status());
  EXPECT_TRUE(complete());
}

TEST_F(FileSystemOperationWriteTest, TestWriteFailureByQuota) {
  GURL blob_url("blob:success");
  scoped_refptr<webkit_blob::BlobData> blob_data(new webkit_blob::BlobData());
  blob_data->AppendData("Hello, world!\n");

  scoped_refptr<TestURLRequestContext> url_request_context(
      new TestURLRequestContext());
  url_request_context->blob_storage_controller()->
      RegisterBlobUrl(blob_url, blob_data);

  quota_manager_->set_quota(10);
  operation()->Write(url_request_context, URLForPath(virtual_path_), blob_url,
                     0);
  MessageLoop::current()->Run();

  url_request_context->blob_storage_controller()->UnregisterBlobUrl(blob_url);

  EXPECT_EQ(10, bytes_written());
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NO_SPACE, status());
  EXPECT_TRUE(complete());
}

// TODO(ericu,dmikurube): Add tests for Cancel.

}  // namespace fileapi
