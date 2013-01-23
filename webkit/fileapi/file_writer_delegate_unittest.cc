// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/files/scoped_temp_dir.h"
#include "base/message_loop.h"
#include "googleurl/src/gurl.h"
#include "net/base/io_buffer.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_job.h"
#include "net/url_request/url_request_status.h"
#include "testing/platform_test.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_writer_delegate.h"
#include "webkit/fileapi/local_file_system_operation.h"
#include "webkit/fileapi/local_file_system_test_helper.h"
#include "webkit/fileapi/sandbox_file_stream_writer.h"
#include "webkit/quota/quota_manager.h"

namespace fileapi {

namespace {

class Result {
 public:
  Result()
      : status_(base::PLATFORM_FILE_OK),
        bytes_written_(0),
        write_status_(FileWriterDelegate::SUCCESS_IO_PENDING) {}

  base::PlatformFileError status() const { return status_; }
  int64 bytes_written() const { return bytes_written_; }
  FileWriterDelegate::WriteProgressStatus write_status() const {
    return write_status_;
  }

  void DidWrite(base::PlatformFileError status, int64 bytes,
                FileWriterDelegate::WriteProgressStatus write_status) {
    write_status_ = write_status;
    if (status == base::PLATFORM_FILE_OK) {
      bytes_written_ += bytes;
      if (write_status_ != FileWriterDelegate::SUCCESS_IO_PENDING)
        MessageLoop::current()->Quit();
    } else {
      EXPECT_EQ(base::PLATFORM_FILE_OK, status_);
      status_ = status;
      MessageLoop::current()->Quit();
    }
  }

 private:
  // For post-operation status.
  base::PlatformFileError status_;
  int64 bytes_written_;
  FileWriterDelegate::WriteProgressStatus write_status_;
};

const char kData[] = "The quick brown fox jumps over the lazy dog.\n";
const int kDataSize = ARRAYSIZE_UNSAFE(kData) - 1;

}  // namespace (anonymous)

class FileWriterDelegateTest : public PlatformTest {
 public:
  FileWriterDelegateTest()
      : loop_(MessageLoop::TYPE_IO),
        test_helper_(GURL("http://example.com"), kFileSystemTypeTest) {}

 protected:
  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

  FileSystemFileUtil* file_util() {
    return test_helper_.file_util();
  }

  int64 ComputeCurrentOriginUsage() {
    return test_helper_.ComputeCurrentOriginUsage();
  }

  FileSystemURL GetFileSystemURL(const char* file_name) const {
    return test_helper_.CreateURLFromUTF8(file_name);
  }

  FileWriterDelegate* CreateWriterDelegate(
      const char* test_file_path,
      int64 offset,
      int64 allowed_growth,
      Result* result) {
    SandboxFileStreamWriter* writer = new SandboxFileStreamWriter(
        test_helper_.file_system_context(),
        GetFileSystemURL(test_file_path),
        offset,
        *test_helper_.file_system_context()->GetUpdateObservers(
            test_helper_.type()));
    writer->set_default_quota(allowed_growth);
    return new FileWriterDelegate(
        base::Bind(&Result::DidWrite, base::Unretained(result)),
        scoped_ptr<FileStreamWriter>(writer));
  }

  // Creates and sets up a FileWriterDelegate for writing the given |blob_url|,
  // and creates a new FileWriterDelegate for the file.
  void PrepareForWrite(const GURL& blob_url,
                       int64 offset,
                       int64 allowed_growth) {
    result_.reset(new Result());
    file_writer_delegate_.reset(
        CreateWriterDelegate("test", offset, allowed_growth, result_.get()));
    request_.reset(empty_context_.CreateRequest(
        blob_url, file_writer_delegate_.get()));
  }

  static net::URLRequest::ProtocolFactory Factory;

  // This should be alive until the very end of this instance.
  MessageLoop loop_;

  net::URLRequestContext empty_context_;
  scoped_ptr<FileWriterDelegate> file_writer_delegate_;
  scoped_ptr<net::URLRequest> request_;
  scoped_ptr<Result> result_;
  LocalFileSystemTestOriginHelper test_helper_;

  base::ScopedTempDir dir_;

  static const char* content_;
};

const char* FileWriterDelegateTest::content_ = NULL;

namespace {

static std::string g_content;

class FileWriterDelegateTestJob : public net::URLRequestJob {
 public:
  FileWriterDelegateTestJob(net::URLRequest* request,
                            net::NetworkDelegate* network_delegate,
                            const std::string& content)
      : net::URLRequestJob(request, network_delegate),
        content_(content),
        remaining_bytes_(content.length()),
        cursor_(0) {
  }

  virtual void Start() OVERRIDE {
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&FileWriterDelegateTestJob::NotifyHeadersComplete, this));
  }

  virtual bool ReadRawData(net::IOBuffer* buf,
                           int buf_size,
                           int *bytes_read) OVERRIDE {
    if (remaining_bytes_ < buf_size)
      buf_size = static_cast<int>(remaining_bytes_);

    for (int i = 0; i < buf_size; ++i)
      buf->data()[i] = content_[cursor_++];
    remaining_bytes_ -= buf_size;

    SetStatus(net::URLRequestStatus());
    *bytes_read = buf_size;
    return true;
  }

  virtual int GetResponseCode() const OVERRIDE {
    return 200;
  }

 protected:
  virtual ~FileWriterDelegateTestJob() {}

 private:
  std::string content_;
  int remaining_bytes_;
  int cursor_;
};

}  // namespace (anonymous)

// static
net::URLRequestJob* FileWriterDelegateTest::Factory(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate,
    const std::string& scheme) {
  return new FileWriterDelegateTestJob(
      request, network_delegate, FileWriterDelegateTest::content_);
}

void FileWriterDelegateTest::SetUp() {
  ASSERT_TRUE(dir_.CreateUniqueTempDir());
  FilePath base_dir = dir_.path().AppendASCII("filesystem");
  test_helper_.SetUp(base_dir);

  scoped_ptr<FileSystemOperationContext> context(
      test_helper_.NewOperationContext());
  context->set_allowed_bytes_growth(kint64max);
  bool created = false;
  base::PlatformFileError error = file_util()->EnsureFileExists(
      context.get(),
      GetFileSystemURL("test"),
      &created);
  ASSERT_EQ(base::PLATFORM_FILE_OK, error);
  ASSERT_TRUE(created);
  net::URLRequest::Deprecated::RegisterProtocolFactory("blob", &Factory);
}

void FileWriterDelegateTest::TearDown() {
  net::URLRequest::Deprecated::RegisterProtocolFactory("blob", NULL);
  test_helper_.TearDown();
}

// FileWriterDelegateTest.WriteSuccessWithoutQuotaLimit is flaky on windows
// http://crbug.com/130401
#if defined(OS_WIN)
#define MAYBE_WriteSuccessWithoutQuotaLimit \
    DISABLED_WriteSuccessWithoutQuotaLimit
#else
#define MAYBE_WriteSuccessWithoutQuotaLimit \
    WriteSuccessWithoutQuotaLimit
#endif
TEST_F(FileWriterDelegateTest, MAYBE_WriteSuccessWithoutQuotaLimit) {
  const GURL kBlobURL("blob:nolimit");
  content_ = kData;

  PrepareForWrite(kBlobURL, 0, quota::QuotaManager::kNoLimit);

  ASSERT_EQ(0, test_helper_.GetCachedOriginUsage());
  file_writer_delegate_->Start(request_.Pass());
  MessageLoop::current()->Run();

  ASSERT_EQ(FileWriterDelegate::SUCCESS_COMPLETED, result_->write_status());
  file_writer_delegate_.reset();

  ASSERT_EQ(kDataSize, test_helper_.GetCachedOriginUsage());
  EXPECT_EQ(ComputeCurrentOriginUsage(), test_helper_.GetCachedOriginUsage());
  EXPECT_EQ(kDataSize, result_->bytes_written());
  EXPECT_EQ(base::PLATFORM_FILE_OK, result_->status());
}

// FileWriterDelegateTest.WriteSuccessWithJustQuota is flaky on windows
// http://crbug.com/130401
#if defined(OS_WIN)
#define MAYBE_WriteSuccessWithJustQuota DISABLED_WriteSuccessWithJustQuota
#else
#define MAYBE_WriteSuccessWithJustQuota WriteSuccessWithJustQuota
#endif

TEST_F(FileWriterDelegateTest, MAYBE_WriteSuccessWithJustQuota) {
  const GURL kBlobURL("blob:just");
  content_ = kData;
  const int64 kAllowedGrowth = kDataSize;
  PrepareForWrite(kBlobURL, 0, kAllowedGrowth);

  ASSERT_EQ(0, test_helper_.GetCachedOriginUsage());
  file_writer_delegate_->Start(request_.Pass());
  MessageLoop::current()->Run();
  ASSERT_EQ(FileWriterDelegate::SUCCESS_COMPLETED, result_->write_status());
  file_writer_delegate_.reset();

  ASSERT_EQ(kAllowedGrowth, test_helper_.GetCachedOriginUsage());
  EXPECT_EQ(ComputeCurrentOriginUsage(), test_helper_.GetCachedOriginUsage());

  EXPECT_EQ(kAllowedGrowth, result_->bytes_written());
  EXPECT_EQ(base::PLATFORM_FILE_OK, result_->status());
}

TEST_F(FileWriterDelegateTest, DISABLED_WriteFailureByQuota) {
  const GURL kBlobURL("blob:failure");
  content_ = kData;
  const int64 kAllowedGrowth = kDataSize - 1;
  PrepareForWrite(kBlobURL, 0, kAllowedGrowth);

  ASSERT_EQ(0, test_helper_.GetCachedOriginUsage());
  file_writer_delegate_->Start(request_.Pass());
  MessageLoop::current()->Run();
  ASSERT_EQ(FileWriterDelegate::ERROR_WRITE_STARTED, result_->write_status());
  file_writer_delegate_.reset();

  ASSERT_EQ(kAllowedGrowth, test_helper_.GetCachedOriginUsage());
  EXPECT_EQ(ComputeCurrentOriginUsage(), test_helper_.GetCachedOriginUsage());

  EXPECT_EQ(kAllowedGrowth, result_->bytes_written());
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NO_SPACE, result_->status());
  ASSERT_EQ(FileWriterDelegate::ERROR_WRITE_STARTED, result_->write_status());
}

TEST_F(FileWriterDelegateTest, WriteZeroBytesSuccessfullyWithZeroQuota) {
  const GURL kBlobURL("blob:zero");
  content_ = "";
  int64 kAllowedGrowth = 0;
  PrepareForWrite(kBlobURL, 0, kAllowedGrowth);

  ASSERT_EQ(0, test_helper_.GetCachedOriginUsage());
  file_writer_delegate_->Start(request_.Pass());
  MessageLoop::current()->Run();
  ASSERT_EQ(FileWriterDelegate::SUCCESS_COMPLETED, result_->write_status());
  file_writer_delegate_.reset();

  ASSERT_EQ(kAllowedGrowth, test_helper_.GetCachedOriginUsage());
  EXPECT_EQ(ComputeCurrentOriginUsage(), test_helper_.GetCachedOriginUsage());

  EXPECT_EQ(kAllowedGrowth, result_->bytes_written());
  EXPECT_EQ(base::PLATFORM_FILE_OK, result_->status());
  ASSERT_EQ(FileWriterDelegate::SUCCESS_COMPLETED, result_->write_status());
}

#if defined(OS_WIN)
// See http://crbug.com/129264
#define MAYBE_WriteSuccessWithoutQuotaLimitConcurrent \
    DISABLED_WriteSuccessWithoutQuotaLimitConcurrent
#else
#define MAYBE_WriteSuccessWithoutQuotaLimitConcurrent \
    WriteSuccessWithoutQuotaLimitConcurrent
#endif

TEST_F(FileWriterDelegateTest, MAYBE_WriteSuccessWithoutQuotaLimitConcurrent) {
  scoped_ptr<FileWriterDelegate> file_writer_delegate2;
  scoped_ptr<net::URLRequest> request2;
  scoped_ptr<Result> result2;

  scoped_ptr<FileSystemOperationContext> context(
      test_helper_.NewOperationContext());
  bool created = false;
  file_util()->EnsureFileExists(context.get(),
                                GetFileSystemURL("test2"),
                                &created);
  ASSERT_TRUE(created);

  const GURL kBlobURL("blob:nolimitconcurrent");
  const GURL kBlobURL2("blob:nolimitconcurrent2");
  content_ = kData;

  PrepareForWrite(kBlobURL, 0, quota::QuotaManager::kNoLimit);

  // Credate another FileWriterDelegate for concurrent write.
  result2.reset(new Result());
  file_writer_delegate2.reset(CreateWriterDelegate(
      "test2", 0, quota::QuotaManager::kNoLimit, result2.get()));
  request2.reset(empty_context_.CreateRequest(
      kBlobURL2, file_writer_delegate2.get()));

  ASSERT_EQ(0, test_helper_.GetCachedOriginUsage());
  file_writer_delegate_->Start(request_.Pass());
  file_writer_delegate2->Start(request2.Pass());
  MessageLoop::current()->Run();
  if (result_->write_status() == FileWriterDelegate::SUCCESS_IO_PENDING ||
      result2->write_status() == FileWriterDelegate::SUCCESS_IO_PENDING)
    MessageLoop::current()->Run();

  ASSERT_EQ(FileWriterDelegate::SUCCESS_COMPLETED, result_->write_status());
  ASSERT_EQ(FileWriterDelegate::SUCCESS_COMPLETED, result2->write_status());
  file_writer_delegate_.reset();
  file_writer_delegate2.reset();

  ASSERT_EQ(kDataSize * 2, test_helper_.GetCachedOriginUsage());
  EXPECT_EQ(ComputeCurrentOriginUsage(), test_helper_.GetCachedOriginUsage());

  EXPECT_EQ(kDataSize, result_->bytes_written());
  EXPECT_EQ(base::PLATFORM_FILE_OK, result_->status());
  EXPECT_EQ(kDataSize, result2->bytes_written());
  EXPECT_EQ(base::PLATFORM_FILE_OK, result2->status());
}

#if defined(OS_WIN)
// See http://crbug.com/129264
#define MAYBE_WritesWithQuotaAndOffset \
    DISABLED_WritesWithQuotaAndOffset
#else
#define MAYBE_WritesWithQuotaAndOffset \
    WritesWithQuotaAndOffset
#endif

TEST_F(FileWriterDelegateTest, MAYBE_WritesWithQuotaAndOffset) {
  const GURL kBlobURL("blob:failure-with-updated-quota");
  content_ = kData;

  // Writing kDataSize (=45) bytes data while allowed_growth is 100.
  int64 offset = 0;
  int64 allowed_growth = 100;
  ASSERT_LT(kDataSize, allowed_growth);
  PrepareForWrite(kBlobURL, offset, allowed_growth);

  ASSERT_EQ(0, test_helper_.GetCachedOriginUsage());
  file_writer_delegate_->Start(request_.Pass());
  MessageLoop::current()->Run();
  ASSERT_EQ(FileWriterDelegate::SUCCESS_COMPLETED, result_->write_status());
  file_writer_delegate_.reset();

  ASSERT_EQ(kDataSize, test_helper_.GetCachedOriginUsage());
  EXPECT_EQ(ComputeCurrentOriginUsage(), test_helper_.GetCachedOriginUsage());
  EXPECT_EQ(kDataSize, result_->bytes_written());
  EXPECT_EQ(base::PLATFORM_FILE_OK, result_->status());

  // Trying to overwrite kDataSize bytes data while allowed_growth is 20.
  offset = 0;
  allowed_growth = 20;
  PrepareForWrite(kBlobURL, offset, allowed_growth);

  file_writer_delegate_->Start(request_.Pass());
  MessageLoop::current()->Run();
  EXPECT_EQ(kDataSize, test_helper_.GetCachedOriginUsage());
  EXPECT_EQ(ComputeCurrentOriginUsage(), test_helper_.GetCachedOriginUsage());
  EXPECT_EQ(kDataSize, result_->bytes_written());
  EXPECT_EQ(base::PLATFORM_FILE_OK, result_->status());
  ASSERT_EQ(FileWriterDelegate::SUCCESS_COMPLETED, result_->write_status());

  // Trying to write kDataSize bytes data from offset 25 while
  // allowed_growth is 55.
  offset = 25;
  allowed_growth = 55;
  PrepareForWrite(kBlobURL, offset, allowed_growth);

  file_writer_delegate_->Start(request_.Pass());
  MessageLoop::current()->Run();
  ASSERT_EQ(FileWriterDelegate::SUCCESS_COMPLETED, result_->write_status());
  file_writer_delegate_.reset();

  EXPECT_EQ(offset + kDataSize, test_helper_.GetCachedOriginUsage());
  EXPECT_EQ(ComputeCurrentOriginUsage(), test_helper_.GetCachedOriginUsage());
  EXPECT_EQ(kDataSize, result_->bytes_written());
  EXPECT_EQ(base::PLATFORM_FILE_OK, result_->status());

  // Trying to overwrite 45 bytes data while allowed_growth is -20.
  offset = 0;
  allowed_growth = -20;
  PrepareForWrite(kBlobURL, offset, allowed_growth);

  int64 pre_write_usage = ComputeCurrentOriginUsage();
  file_writer_delegate_->Start(request_.Pass());
  MessageLoop::current()->Run();
  ASSERT_EQ(FileWriterDelegate::SUCCESS_COMPLETED, result_->write_status());
  file_writer_delegate_.reset();

  EXPECT_EQ(pre_write_usage, test_helper_.GetCachedOriginUsage());
  EXPECT_EQ(ComputeCurrentOriginUsage(), test_helper_.GetCachedOriginUsage());
  EXPECT_EQ(kDataSize, result_->bytes_written());
  EXPECT_EQ(base::PLATFORM_FILE_OK, result_->status());

  // Trying to overwrite 45 bytes data with offset pre_write_usage - 20,
  // while allowed_growth is 10.
  const int kOverlap = 20;
  offset = pre_write_usage - kOverlap;
  allowed_growth = 10;
  PrepareForWrite(kBlobURL, offset, allowed_growth);

  file_writer_delegate_->Start(request_.Pass());
  MessageLoop::current()->Run();
  ASSERT_EQ(FileWriterDelegate::ERROR_WRITE_STARTED, result_->write_status());
  file_writer_delegate_.reset();

  EXPECT_EQ(pre_write_usage + allowed_growth,
            test_helper_.GetCachedOriginUsage());
  EXPECT_EQ(ComputeCurrentOriginUsage(), test_helper_.GetCachedOriginUsage());
  EXPECT_EQ(kOverlap + allowed_growth, result_->bytes_written());
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NO_SPACE, result_->status());
}

}  // namespace fileapi
