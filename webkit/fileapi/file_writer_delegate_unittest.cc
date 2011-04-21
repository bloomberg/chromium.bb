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

#include "base/file_util_proxy.h"
#include "base/memory/scoped_temp_dir.h"
#include "base/message_loop.h"
#include "googleurl/src/gurl.h"
#include "net/base/io_buffer.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_job.h"
#include "net/url_request/url_request_status.h"
#include "testing/platform_test.h"
#include "webkit/fileapi/file_system_callback_dispatcher.h"
#include "webkit/fileapi/file_system_operation.h"
#include "webkit/fileapi/file_writer_delegate.h"
#include "webkit/fileapi/quota_file_util.h"

namespace fileapi {

class FileWriterDelegateTest : public PlatformTest {
 public:
  FileWriterDelegateTest()
      : loop_(MessageLoop::TYPE_IO),
        status_(base::PLATFORM_FILE_OK),
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

 protected:
  virtual void SetUp();
  virtual void TearDown();

  static net::URLRequest::ProtocolFactory Factory;

  scoped_ptr<FileWriterDelegate> file_writer_delegate_;
  scoped_ptr<net::URLRequest> request_;

  MessageLoop loop_;

  ScopedTempDir dir_;
  FilePath file_path_;
  PlatformFile file_;

  // For post-operation status.
  base::PlatformFileError status_;
  int64 bytes_written_;
  bool complete_;
};

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
  MockDispatcher(FileWriterDelegateTest* test) : test_(test) { }

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
  FileWriterDelegateTest* test_;
};

}  // namespace (anonymous)

// static
net::URLRequestJob* FileWriterDelegateTest::Factory(
    net::URLRequest* request,
    const std::string& scheme) {
  return new FileWriterDelegateTestJob(request, g_content);
}

void FileWriterDelegateTest::SetUp() {
  ASSERT_TRUE(dir_.CreateUniqueTempDir());
  ASSERT_TRUE(file_util::CreateTemporaryFileInDir(dir_.path(), &file_path_));

  bool created;
  base::PlatformFileError error_code;
  file_ = base::CreatePlatformFile(
      file_path_,
      base::PLATFORM_FILE_OPEN | base::PLATFORM_FILE_WRITE |
          base::PLATFORM_FILE_ASYNC,
      &created, &error_code);
  ASSERT_EQ(base::PLATFORM_FILE_OK, error_code);

  net::URLRequest::RegisterProtocolFactory("blob", &Factory);
}

void FileWriterDelegateTest::TearDown() {
  net::URLRequest::RegisterProtocolFactory("blob", NULL);
  base::ClosePlatformFile(file_);
}

TEST_F(FileWriterDelegateTest, WriteSuccessWithoutQuotaLimit) {
  GURL blob_url("blob:nolimit");
  g_content = std::string("The quick brown fox jumps over the lazy dog.\n");
  file_writer_delegate_.reset(new FileWriterDelegate(
      new FileSystemOperation(new MockDispatcher(this), NULL, NULL,
                              QuotaFileUtil::GetInstance()), 0));
  request_.reset(new net::URLRequest(blob_url, file_writer_delegate_.get()));

  file_writer_delegate_->Start(file_, request_.get(), QuotaFileUtil::kNoLimit,
      base::MessageLoopProxy::CreateForCurrentThread());
  MessageLoop::current()->Run();

  file_writer_delegate_.reset(NULL);

  EXPECT_EQ(45, bytes_written());
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());
  EXPECT_TRUE(complete());
}

TEST_F(FileWriterDelegateTest, WriteSuccessWithJustQuota) {
  GURL blob_url("blob:just");
  g_content = std::string("The quick brown fox jumps over the lazy dog.\n");
  file_writer_delegate_.reset(new FileWriterDelegate(
      new FileSystemOperation(new MockDispatcher(this), NULL, NULL,
                              QuotaFileUtil::GetInstance()), 0));
  request_.reset(new net::URLRequest(blob_url, file_writer_delegate_.get()));

  file_writer_delegate_->Start(file_, request_.get(), 45,
      base::MessageLoopProxy::CreateForCurrentThread());
  MessageLoop::current()->Run();

  file_writer_delegate_.reset(NULL);

  EXPECT_EQ(45, bytes_written());
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());
  EXPECT_TRUE(complete());
}

TEST_F(FileWriterDelegateTest, WriteFailureByQuota) {
  GURL blob_url("blob:failure");
  g_content = std::string("The quick brown fox jumps over the lazy dog.\n");
  file_writer_delegate_.reset(new FileWriterDelegate(
      new FileSystemOperation(new MockDispatcher(this), NULL, NULL,
                              QuotaFileUtil::GetInstance()), 0));
  request_.reset(new net::URLRequest(blob_url, file_writer_delegate_.get()));

  file_writer_delegate_->Start(file_, request_.get(), 44,
      base::MessageLoopProxy::CreateForCurrentThread());
  MessageLoop::current()->Run();

  file_writer_delegate_.reset(NULL);

  EXPECT_EQ(44, bytes_written());
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NO_SPACE, status());
  EXPECT_TRUE(complete());
}

TEST_F(FileWriterDelegateTest, WriteZeroBytesSuccessfullyWithZeroQuota) {
  GURL blob_url("blob:zero");
  g_content = std::string("");
  file_writer_delegate_.reset(new FileWriterDelegate(
      new FileSystemOperation(new MockDispatcher(this), NULL, NULL,
                              QuotaFileUtil::GetInstance()), 0));
  request_.reset(new net::URLRequest(blob_url, file_writer_delegate_.get()));

  file_writer_delegate_->Start(file_, request_.get(), 0,
      base::MessageLoopProxy::CreateForCurrentThread());
  MessageLoop::current()->Run();

  file_writer_delegate_.reset(NULL);

  EXPECT_EQ(0, bytes_written());
  EXPECT_EQ(base::PLATFORM_FILE_OK, status());
  EXPECT_TRUE(complete());
}

}  // namespace fileapi
