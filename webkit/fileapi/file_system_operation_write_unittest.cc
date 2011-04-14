// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// NOTE: These tests are run as part of "unit_tests" (in chrome/test/unit)
// rather than as part of test_shell_tests because they rely on being able
// to instantiate a MessageLoop of type TYPE_IO.  test_shell_tests uses
// TYPE_UI, which URLRequest doesn't allow.
//

#include "base/message_loop.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_temp_dir.h"
#include "base/message_loop.h"
#include "googleurl/src/gurl.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_job.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/blob/blob_data.h"
#include "webkit/blob/blob_storage_controller.h"
#include "webkit/blob/blob_url_request_job.h"
#include "webkit/fileapi/file_system_callback_dispatcher.h"
#include "webkit/fileapi/file_system_file_util.h"
#include "webkit/fileapi/file_system_operation.h"

namespace fileapi {

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
  GURL URLForRelativePath(const std::string& path) const {
    // Only the path will actually get used.
    return GURL("file://").Resolve(file_.value()).Resolve(path);
  }

  GURL URLForPath(const FilePath& path) const {
    // Only the path will actually get used.
    return GURL("file://").Resolve(path.value());
  }

  MessageLoop loop_;

  ScopedTempDir dir_;
  FilePath file_;

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
  file_util::CreateTemporaryFileInDir(dir_.path(), &file_);
  net::URLRequest::RegisterProtocolFactory("blob", &BlobURLRequestJobFactory);
}

void FileSystemOperationWriteTest::TearDown() {
  net::URLRequest::RegisterProtocolFactory("blob", NULL);
}

FileSystemOperation* FileSystemOperationWriteTest::operation() {
  FileSystemOperation* operation = new FileSystemOperation(
      new MockDispatcher(this),
      base::MessageLoopProxy::CreateForCurrentThread(),
      NULL,
      FileSystemFileUtil::GetInstance());
  operation->file_system_operation_context()->set_src_type(
      kFileSystemTypeTemporary);
  operation->file_system_operation_context()->set_dest_type(
      kFileSystemTypeTemporary);
  return operation;
}

TEST_F(FileSystemOperationWriteTest, TestWriteSuccess) {
  GURL blob_url("blob:success");
  scoped_refptr<webkit_blob::BlobData> blob_data(new webkit_blob::BlobData());
  blob_data->AppendData("Hello, world!\n");

  scoped_refptr<TestURLRequestContext> url_request_context(
      new TestURLRequestContext());
  url_request_context->blob_storage_controller()->
      RegisterBlobUrl(blob_url, blob_data);

  operation()->Write(url_request_context, URLForPath(file_), blob_url, 0);
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

  operation()->Write(url_request_context, URLForPath(file_), blob_url, 0);
  MessageLoop::current()->Run();

  url_request_context->blob_storage_controller()->UnregisterBlobUrl(blob_url);

  EXPECT_EQ(0, bytes_written());
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());
  EXPECT_TRUE(complete());
}

TEST_F(FileSystemOperationWriteTest, TestWriteInvalidBlobUrl) {
  scoped_refptr<TestURLRequestContext> url_request_context(
      new TestURLRequestContext());

  operation()->Write(url_request_context, URLForPath(file_),
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
                     URLForRelativePath("nonexist"), blob_url, 0);
  MessageLoop::current()->Run();

  url_request_context->blob_storage_controller()->UnregisterBlobUrl(blob_url);

  EXPECT_EQ(0, bytes_written());
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND, status());
  EXPECT_TRUE(complete());
}

TEST_F(FileSystemOperationWriteTest, TestWriteDir) {
  GURL blob_url("blob:writedir");
  scoped_refptr<webkit_blob::BlobData> blob_data(new webkit_blob::BlobData());
  blob_data->AppendData("It\'ll not be written, too.");

  scoped_refptr<TestURLRequestContext> url_request_context(
      new TestURLRequestContext());
  url_request_context->blob_storage_controller()->
      RegisterBlobUrl(blob_url, blob_data);

  operation()->Write(url_request_context, URLForPath(dir_.path()), blob_url, 0);
  MessageLoop::current()->Run();

  url_request_context->blob_storage_controller()->UnregisterBlobUrl(blob_url);

  EXPECT_EQ(0, bytes_written());
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_ACCESS_DENIED, status());
  EXPECT_TRUE(complete());
}

// TODO(ericu,dmikurube): Add tests for Cancel.

}  // namespace fileapi
