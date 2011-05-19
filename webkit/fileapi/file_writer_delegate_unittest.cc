// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// NOTE: These tests are run as part of "unit_tests" (in chrome/test/unit)
// rather than as part of test_shell_tests because they rely on being able
// to instantiate a MessageLoop of type TYPE_IO.  test_shell_tests uses
// TYPE_UI, which URLRequest doesn't allow.
//

#include <string>
#include <vector>

#include "base/file_util_proxy.h"
#include "base/message_loop.h"
#include "base/scoped_temp_dir.h"
#include "googleurl/src/gurl.h"
#include "net/base/io_buffer.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_job.h"
#include "net/url_request/url_request_status.h"
#include "testing/platform_test.h"
#include "webkit/fileapi/file_system_callback_dispatcher.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_operation.h"
#include "webkit/fileapi/file_system_operation_context.h"
#include "webkit/fileapi/file_system_path_manager.h"
#include "webkit/fileapi/file_system_test_helper.h"
#include "webkit/fileapi/file_system_usage_cache.h"
#include "webkit/fileapi/file_writer_delegate.h"
#include "webkit/fileapi/quota_file_util.h"
#include "webkit/fileapi/sandbox_mount_point_provider.h"

namespace fileapi {

namespace {

class Result {
 public:
  Result()
      : status_(base::PLATFORM_FILE_OK),
        bytes_written_(0),
        complete_(false) {}

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

 private:
  // For post-operation status.
  base::PlatformFileError status_;
  int64 bytes_written_;
  bool complete_;
};

}  // namespace (anonymous)

class FileWriterDelegateTest : public PlatformTest {
 public:
  FileWriterDelegateTest()
      : loop_(MessageLoop::TYPE_IO) {}

 protected:
  virtual void SetUp();
  virtual void TearDown();

  int64 GetCachedUsage() {
    return FileSystemUsageCache::GetUsage(test_helper_.GetUsageCachePath());
  }

  FileSystemOperation* CreateNewOperation(Result* result, int64 quota);

  static net::URLRequest::ProtocolFactory Factory;

  scoped_ptr<FileWriterDelegate> file_writer_delegate_;
  scoped_ptr<net::URLRequest> request_;
  scoped_ptr<Result> result_;
  FileSystemTestOriginHelper test_helper_;

  MessageLoop loop_;

  ScopedTempDir dir_;
  FilePath file_path_;
  PlatformFile file_;

  static const char* content_;
};
const char* FileWriterDelegateTest::content_ = NULL;

namespace {

static std::string g_content;

class FileWriterDelegateTestJob : public net::URLRequestJob {
 public:
  FileWriterDelegateTestJob(net::URLRequest* request,
                            const std::string& content)
      : net::URLRequestJob(request),
        content_(content),
        remaining_bytes_(content.length()),
        cursor_(0) {
  }

  void Start() {
    MessageLoop::current()->PostTask(FROM_HERE, NewRunnableMethod(
        this, &FileWriterDelegateTestJob::NotifyHeadersComplete));
  }

  bool ReadRawData(net::IOBuffer* buf, int buf_size, int *bytes_read) {
    if (remaining_bytes_ < buf_size)
      buf_size = static_cast<int>(remaining_bytes_);

    for (int i = 0; i < buf_size; ++i)
      buf->data()[i] = content_[cursor_++];
    remaining_bytes_ -= buf_size;

    SetStatus(net::URLRequestStatus());
    *bytes_read = buf_size;
    return true;
  }

 private:
  std::string content_;
  int remaining_bytes_;
  int cursor_;
};

class MockDispatcher : public FileSystemCallbackDispatcher {
 public:
  explicit MockDispatcher(Result* result) : result_(result) {}

  virtual void DidFail(base::PlatformFileError status) {
    result_->set_failure_status(status);
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
    result_->add_bytes_written(bytes, complete);
    if (complete)
      MessageLoop::current()->Quit();
  }

 private:
  Result* result_;
};

}  // namespace (anonymous)

// static
net::URLRequestJob* FileWriterDelegateTest::Factory(
    net::URLRequest* request,
    const std::string& scheme) {
  return new FileWriterDelegateTestJob(
      request, FileWriterDelegateTest::content_);
}

void FileWriterDelegateTest::SetUp() {
  ASSERT_TRUE(dir_.CreateUniqueTempDir());
  FilePath base_dir = dir_.path().AppendASCII("filesystem");
  test_helper_.SetUp(base_dir, QuotaFileUtil::GetInstance());

  FilePath filesystem_dir = test_helper_.GetOriginRootPath();

  ASSERT_TRUE(file_util::CreateTemporaryFileInDir(
      filesystem_dir, &file_path_));
  bool created;
  base::PlatformFileError error_code;
  file_ = base::CreatePlatformFile(
      file_path_,
      base::PLATFORM_FILE_OPEN | base::PLATFORM_FILE_WRITE |
          base::PLATFORM_FILE_ASYNC,
      &created, &error_code);
  ASSERT_EQ(base::PLATFORM_FILE_OK, error_code);

  result_.reset(new Result());

  net::URLRequest::RegisterProtocolFactory("blob", &Factory);
}

void FileWriterDelegateTest::TearDown() {
  net::URLRequest::RegisterProtocolFactory("blob", NULL);
  result_.reset();
  base::ClosePlatformFile(file_);
  test_helper_.TearDown();
}

FileSystemOperation* FileWriterDelegateTest::CreateNewOperation(
    Result* result, int64 quota) {
  FileSystemOperation* operation = test_helper_.NewOperation(
      new MockDispatcher(result));
  FileSystemOperationContext* context =
      operation->file_system_operation_context();
  context->set_allowed_bytes_growth(quota);
  return operation;
}

TEST_F(FileWriterDelegateTest, WriteSuccessWithoutQuotaLimit) {
  GURL blob_url("blob:nolimit");
  content_ = "The quick brown fox jumps over the lazy dog.\n";
  file_writer_delegate_.reset(new FileWriterDelegate(
      CreateNewOperation(result_.get(), QuotaFileUtil::kNoLimit),
      0, base::MessageLoopProxy::CreateForCurrentThread()));
  request_.reset(new net::URLRequest(blob_url, file_writer_delegate_.get()));

  ASSERT_EQ(0, GetCachedUsage());
  file_writer_delegate_->Start(file_, request_.get());
  MessageLoop::current()->Run();
  ASSERT_EQ(45, GetCachedUsage());

  EXPECT_EQ(45, result_->bytes_written());
  EXPECT_EQ(base::PLATFORM_FILE_OK, result_->status());
  EXPECT_TRUE(result_->complete());

  file_writer_delegate_.reset();
}

TEST_F(FileWriterDelegateTest, WriteSuccessWithJustQuota) {
  GURL blob_url("blob:just");
  content_ = "The quick brown fox jumps over the lazy dog.\n";
  file_writer_delegate_.reset(new FileWriterDelegate(
      CreateNewOperation(result_.get(), 45),
      0, base::MessageLoopProxy::CreateForCurrentThread()));
  request_.reset(new net::URLRequest(blob_url, file_writer_delegate_.get()));

  ASSERT_EQ(0, GetCachedUsage());
  file_writer_delegate_->Start(file_, request_.get());
  MessageLoop::current()->Run();
  ASSERT_EQ(45, GetCachedUsage());

  file_writer_delegate_.reset();

  EXPECT_EQ(45, result_->bytes_written());
  EXPECT_EQ(base::PLATFORM_FILE_OK, result_->status());
  EXPECT_TRUE(result_->complete());
}

TEST_F(FileWriterDelegateTest, WriteFailureByQuota) {
  GURL blob_url("blob:failure");
  content_ = "The quick brown fox jumps over the lazy dog.\n";
  file_writer_delegate_.reset(new FileWriterDelegate(
      CreateNewOperation(result_.get(), 44),
      0, base::MessageLoopProxy::CreateForCurrentThread()));
  request_.reset(new net::URLRequest(blob_url, file_writer_delegate_.get()));

  ASSERT_EQ(0, GetCachedUsage());
  file_writer_delegate_->Start(file_, request_.get());
  MessageLoop::current()->Run();
  ASSERT_EQ(44, GetCachedUsage());

  file_writer_delegate_.reset();

  EXPECT_EQ(44, result_->bytes_written());
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NO_SPACE, result_->status());
  EXPECT_TRUE(result_->complete());
}

TEST_F(FileWriterDelegateTest, WriteZeroBytesSuccessfullyWithZeroQuota) {
  GURL blob_url("blob:zero");
  content_ = "";
  file_writer_delegate_.reset(new FileWriterDelegate(
      CreateNewOperation(result_.get(), 0),
      0, base::MessageLoopProxy::CreateForCurrentThread()));
  request_.reset(new net::URLRequest(blob_url, file_writer_delegate_.get()));

  ASSERT_EQ(0, GetCachedUsage());
  file_writer_delegate_->Start(file_, request_.get());
  MessageLoop::current()->Run();
  ASSERT_EQ(0, GetCachedUsage());

  file_writer_delegate_.reset();

  EXPECT_EQ(0, result_->bytes_written());
  EXPECT_EQ(base::PLATFORM_FILE_OK, result_->status());
  EXPECT_TRUE(result_->complete());
}

TEST_F(FileWriterDelegateTest, WriteSuccessWithoutQuotaLimitConcurrent) {
  scoped_ptr<FileWriterDelegate> file_writer_delegate2;
  scoped_ptr<net::URLRequest> request2;
  scoped_ptr<Result> result2;

  PlatformFile file2;
  bool created;
  base::PlatformFileError error_code;
  file2 = base::CreatePlatformFile(
      file_path_,
      base::PLATFORM_FILE_OPEN | base::PLATFORM_FILE_WRITE |
          base::PLATFORM_FILE_ASYNC,
      &created, &error_code);
  ASSERT_EQ(base::PLATFORM_FILE_OK, error_code);

  result2.reset(new Result());

  GURL blob_url("blob:nolimitconcurrent");
  GURL blob_url2("blob:nolimitconcurrent2");
  content_ = "The quick brown fox jumps over the lazy dog.\n";
  file_writer_delegate_.reset(new FileWriterDelegate(
      CreateNewOperation(result_.get(), QuotaFileUtil::kNoLimit),
      0, base::MessageLoopProxy::CreateForCurrentThread()));
  file_writer_delegate2.reset(new FileWriterDelegate(
      CreateNewOperation(result2.get(), QuotaFileUtil::kNoLimit),
      0, base::MessageLoopProxy::CreateForCurrentThread()));
  request_.reset(new net::URLRequest(blob_url, file_writer_delegate_.get()));
  request2.reset(new net::URLRequest(blob_url2, file_writer_delegate2.get()));

  ASSERT_EQ(0, GetCachedUsage());
  file_writer_delegate_->Start(file_, request_.get());
  file_writer_delegate2->Start(file2, request2.get());
  MessageLoop::current()->Run();
  if (!result_->complete() || !result2->complete())
    MessageLoop::current()->Run();
  ASSERT_EQ(90, GetCachedUsage());

  file_writer_delegate_.reset();

  EXPECT_EQ(45, result_->bytes_written());
  EXPECT_EQ(base::PLATFORM_FILE_OK, result_->status());
  EXPECT_TRUE(result_->complete());
  EXPECT_EQ(45, result2->bytes_written());
  EXPECT_EQ(base::PLATFORM_FILE_OK, result2->status());
  EXPECT_TRUE(result2->complete());
}

}  // namespace fileapi
