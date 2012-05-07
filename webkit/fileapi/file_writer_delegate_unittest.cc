// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/file_util_proxy.h"
#include "base/message_loop.h"
#include "base/scoped_temp_dir.h"
#include "googleurl/src/gurl.h"
#include "net/base/io_buffer.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_job.h"
#include "net/url_request/url_request_status.h"
#include "testing/platform_test.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_operation.h"
#include "webkit/fileapi/file_system_operation_context.h"
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

  base::PlatformFileError status() const { return status_; }
  void add_bytes_written(int64 bytes, bool complete) {
    bytes_written_ += bytes;
    EXPECT_FALSE(complete_);
    complete_ = complete;
  }
  int64 bytes_written() const { return bytes_written_; }
  bool complete() const { return complete_; }

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
      MessageLoop::current()->Quit();
    }
  }

 private:
  // For post-operation status.
  base::PlatformFileError status_;
  int64 bytes_written_;
  bool complete_;
};

const char kData[] = "The quick brown fox jumps over the lazy dog.\n";
const int kDataSize = ARRAYSIZE_UNSAFE(kData) - 1;

}  // namespace (anonymous)

class FileWriterDelegateTest : public PlatformTest {
 public:
  FileWriterDelegateTest()
      : loop_(MessageLoop::TYPE_IO),
        file_(base::kInvalidPlatformFileValue) {}

 protected:
  virtual void SetUp();
  virtual void TearDown();

  virtual void SetUpTestHelper(const FilePath& base_dir) {
    quota_file_util_.reset(QuotaFileUtil::CreateDefault());
    test_helper_.SetUp(base_dir, quota_file_util_.get());
  }

  int64 ComputeCurrentOriginUsage() {
    base::FlushPlatformFile(file_);
    return test_helper_.ComputeCurrentOriginUsage();
  }

  // Creates and sets up a FileWriterDelegate for writing the given |blob_url|
  // to a file (file_) from |offset| with |allowed_growth| quota setting.
  void PrepareForWrite(const GURL& blob_url,
                       int64 offset,
                       int64 allowed_growth) {
    bool created;
    base::PlatformFileError error_code;
    file_ = base::CreatePlatformFile(
        file_path_,
        base::PLATFORM_FILE_OPEN | base::PLATFORM_FILE_WRITE |
            base::PLATFORM_FILE_ASYNC,
        &created, &error_code);
    ASSERT_EQ(base::PLATFORM_FILE_OK, error_code);

    result_.reset(new Result());
    file_writer_delegate_.reset(new FileWriterDelegate(
        CreateNewOperation(result_.get(), allowed_growth),
        test_helper_.CreatePath(file_path_),
        offset));
    request_.reset(new net::URLRequest(blob_url, file_writer_delegate_.get()));
  }

  FileSystemOperation* CreateNewOperation(Result* result, int64 quota);

  static net::URLRequest::ProtocolFactory Factory;

  scoped_ptr<QuotaFileUtil> quota_file_util_;
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
    const std::string& scheme) {
  return new FileWriterDelegateTestJob(
      request, FileWriterDelegateTest::content_);
}

void FileWriterDelegateTest::SetUp() {
  ASSERT_TRUE(dir_.CreateUniqueTempDir());
  FilePath base_dir = dir_.path().AppendASCII("filesystem");
  SetUpTestHelper(base_dir);
  ASSERT_TRUE(file_util::CreateTemporaryFileInDir(
      test_helper_.GetOriginRootPath(), &file_path_));
  net::URLRequest::Deprecated::RegisterProtocolFactory("blob", &Factory);
}

void FileWriterDelegateTest::TearDown() {
  net::URLRequest::Deprecated::RegisterProtocolFactory("blob", NULL);
  base::ClosePlatformFile(file_);
  test_helper_.TearDown();
}

FileSystemOperation* FileWriterDelegateTest::CreateNewOperation(
    Result* result, int64 quota) {
  FileSystemOperation* operation = test_helper_.NewOperation();
  operation->set_write_callback(base::Bind(&Result::DidWrite,
                                           base::Unretained(result)));
  FileSystemOperationContext* context =
      operation->file_system_operation_context();
  context->set_allowed_bytes_growth(quota);
  return operation;
}

TEST_F(FileWriterDelegateTest, WriteSuccessWithoutQuotaLimit) {
  const GURL kBlobURL("blob:nolimit");
  content_ = kData;

  PrepareForWrite(kBlobURL, 0, QuotaFileUtil::kNoLimit);

  ASSERT_EQ(0, test_helper_.GetCachedOriginUsage());
  file_writer_delegate_->Start(file_, request_.get());
  MessageLoop::current()->Run();
  ASSERT_EQ(kDataSize, test_helper_.GetCachedOriginUsage());
  EXPECT_EQ(ComputeCurrentOriginUsage(), test_helper_.GetCachedOriginUsage());

  EXPECT_EQ(kDataSize, result_->bytes_written());
  EXPECT_EQ(base::PLATFORM_FILE_OK, result_->status());
  EXPECT_TRUE(result_->complete());

  file_writer_delegate_.reset();
}

TEST_F(FileWriterDelegateTest, WriteSuccessWithJustQuota) {
  const GURL kBlobURL("blob:just");
  content_ = kData;
  const int64 kAllowedGrowth = kDataSize;
  PrepareForWrite(kBlobURL, 0, kAllowedGrowth);

  ASSERT_EQ(0, test_helper_.GetCachedOriginUsage());
  file_writer_delegate_->Start(file_, request_.get());
  MessageLoop::current()->Run();
  ASSERT_EQ(kAllowedGrowth, test_helper_.GetCachedOriginUsage());
  EXPECT_EQ(ComputeCurrentOriginUsage(), test_helper_.GetCachedOriginUsage());

  file_writer_delegate_.reset();

  EXPECT_EQ(kAllowedGrowth, result_->bytes_written());
  EXPECT_EQ(base::PLATFORM_FILE_OK, result_->status());
  EXPECT_TRUE(result_->complete());
}

TEST_F(FileWriterDelegateTest, WriteFailureByQuota) {
  const GURL kBlobURL("blob:failure");
  content_ = kData;
  const int64 kAllowedGrowth = kDataSize - 1;
  PrepareForWrite(kBlobURL, 0, kAllowedGrowth);

  ASSERT_EQ(0, test_helper_.GetCachedOriginUsage());
  file_writer_delegate_->Start(file_, request_.get());
  MessageLoop::current()->Run();
  ASSERT_EQ(kAllowedGrowth, test_helper_.GetCachedOriginUsage());
  EXPECT_EQ(ComputeCurrentOriginUsage(), test_helper_.GetCachedOriginUsage());

  file_writer_delegate_.reset();

  EXPECT_EQ(kAllowedGrowth, result_->bytes_written());
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NO_SPACE, result_->status());
  EXPECT_TRUE(result_->complete());
}

TEST_F(FileWriterDelegateTest, WriteZeroBytesSuccessfullyWithZeroQuota) {
  const GURL kBlobURL("blob:zero");
  content_ = "";
  int64 kAllowedGrowth = 0;
  PrepareForWrite(kBlobURL, 0, kAllowedGrowth);

  ASSERT_EQ(0, test_helper_.GetCachedOriginUsage());
  file_writer_delegate_->Start(file_, request_.get());
  MessageLoop::current()->Run();
  ASSERT_EQ(kAllowedGrowth, test_helper_.GetCachedOriginUsage());
  EXPECT_EQ(ComputeCurrentOriginUsage(), test_helper_.GetCachedOriginUsage());

  file_writer_delegate_.reset();

  EXPECT_EQ(kAllowedGrowth, result_->bytes_written());
  EXPECT_EQ(base::PLATFORM_FILE_OK, result_->status());
  EXPECT_TRUE(result_->complete());
}

TEST_F(FileWriterDelegateTest, WriteSuccessWithoutQuotaLimitConcurrent) {
  scoped_ptr<FileWriterDelegate> file_writer_delegate2;
  scoped_ptr<net::URLRequest> request2;
  scoped_ptr<Result> result2;

  FilePath file_path2;
  PlatformFile file2;
  bool created;
  base::PlatformFileError error_code;
  ASSERT_TRUE(file_util::CreateTemporaryFileInDir(
      test_helper_.GetOriginRootPath(), &file_path2));
  file2 = base::CreatePlatformFile(
      file_path2,
      base::PLATFORM_FILE_OPEN | base::PLATFORM_FILE_WRITE |
          base::PLATFORM_FILE_ASYNC,
      &created, &error_code);
  ASSERT_EQ(base::PLATFORM_FILE_OK, error_code);

  const GURL kBlobURL("blob:nolimitconcurrent");
  const GURL kBlobURL2("blob:nolimitconcurrent2");
  content_ = kData;

  PrepareForWrite(kBlobURL, 0, QuotaFileUtil::kNoLimit);

  // Credate another FileWriterDelegate for concurrent write.
  result2.reset(new Result());
  file_writer_delegate2.reset(new FileWriterDelegate(
      CreateNewOperation(result2.get(), QuotaFileUtil::kNoLimit),
      test_helper_.CreatePath(file_path2), 0));
  request2.reset(new net::URLRequest(kBlobURL2, file_writer_delegate2.get()));

  ASSERT_EQ(0, test_helper_.GetCachedOriginUsage());
  file_writer_delegate_->Start(file_, request_.get());
  file_writer_delegate2->Start(file2, request2.get());
  MessageLoop::current()->Run();
  if (!result_->complete() || !result2->complete())
    MessageLoop::current()->Run();

  ASSERT_EQ(kDataSize * 2, test_helper_.GetCachedOriginUsage());
  base::FlushPlatformFile(file2);
  EXPECT_EQ(ComputeCurrentOriginUsage(), test_helper_.GetCachedOriginUsage());

  file_writer_delegate_.reset();

  EXPECT_EQ(kDataSize, result_->bytes_written());
  EXPECT_EQ(base::PLATFORM_FILE_OK, result_->status());
  EXPECT_TRUE(result_->complete());
  EXPECT_EQ(kDataSize, result2->bytes_written());
  EXPECT_EQ(base::PLATFORM_FILE_OK, result2->status());
  EXPECT_TRUE(result2->complete());

  base::ClosePlatformFile(file2);
}

TEST_F(FileWriterDelegateTest, WritesWithQuotaAndOffset) {
  const GURL kBlobURL("blob:failure-with-updated-quota");
  content_ = kData;

  // Writing kDataSize (=45) bytes data while allowed_growth is 100.
  int64 offset = 0;
  int64 allowed_growth = 100;
  ASSERT_LT(kDataSize, allowed_growth);
  PrepareForWrite(kBlobURL, offset, allowed_growth);

  ASSERT_EQ(0, test_helper_.GetCachedOriginUsage());
  file_writer_delegate_->Start(file_, request_.get());
  MessageLoop::current()->Run();
  ASSERT_EQ(kDataSize, test_helper_.GetCachedOriginUsage());
  EXPECT_EQ(ComputeCurrentOriginUsage(), test_helper_.GetCachedOriginUsage());
  EXPECT_EQ(kDataSize, result_->bytes_written());
  EXPECT_EQ(base::PLATFORM_FILE_OK, result_->status());
  EXPECT_TRUE(result_->complete());

  // Trying to overwrite kDataSize bytes data while allowed_growth is 20.
  offset = 0;
  allowed_growth = 20;
  PrepareForWrite(kBlobURL, offset, allowed_growth);

  file_writer_delegate_->Start(file_, request_.get());
  MessageLoop::current()->Run();
  EXPECT_EQ(kDataSize, test_helper_.GetCachedOriginUsage());
  EXPECT_EQ(ComputeCurrentOriginUsage(), test_helper_.GetCachedOriginUsage());
  EXPECT_EQ(kDataSize, result_->bytes_written());
  EXPECT_EQ(base::PLATFORM_FILE_OK, result_->status());
  EXPECT_TRUE(result_->complete());

  // Trying to write kDataSize bytes data from offset 25 while
  // allowed_growth is 55.
  offset = 25;
  allowed_growth = 55;
  PrepareForWrite(kBlobURL, offset, allowed_growth);

  file_writer_delegate_->Start(file_, request_.get());
  MessageLoop::current()->Run();
  EXPECT_EQ(offset + kDataSize, test_helper_.GetCachedOriginUsage());
  EXPECT_EQ(ComputeCurrentOriginUsage(), test_helper_.GetCachedOriginUsage());
  EXPECT_EQ(kDataSize, result_->bytes_written());
  EXPECT_EQ(base::PLATFORM_FILE_OK, result_->status());
  EXPECT_TRUE(result_->complete());

  // Trying to overwrite 45 bytes data while allowed_growth is -20.
  offset = 0;
  allowed_growth = -20;
  PrepareForWrite(kBlobURL, offset, allowed_growth);

  int64 pre_write_usage = ComputeCurrentOriginUsage();
  file_writer_delegate_->Start(file_, request_.get());
  MessageLoop::current()->Run();
  EXPECT_EQ(pre_write_usage, test_helper_.GetCachedOriginUsage());
  EXPECT_EQ(ComputeCurrentOriginUsage(), test_helper_.GetCachedOriginUsage());
  EXPECT_EQ(kDataSize, result_->bytes_written());
  EXPECT_EQ(base::PLATFORM_FILE_OK, result_->status());
  EXPECT_TRUE(result_->complete());

  // Trying to overwrite 45 bytes data with offset pre_write_usage - 20,
  // while allowed_growth is 10.
  const int kOverlap = 20;
  offset = pre_write_usage - kOverlap;
  allowed_growth = 10;
  PrepareForWrite(kBlobURL, offset, allowed_growth);

  file_writer_delegate_->Start(file_, request_.get());
  MessageLoop::current()->Run();
  EXPECT_EQ(pre_write_usage + allowed_growth,
            test_helper_.GetCachedOriginUsage());
  EXPECT_EQ(ComputeCurrentOriginUsage(), test_helper_.GetCachedOriginUsage());
  EXPECT_EQ(kOverlap + allowed_growth, result_->bytes_written());
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NO_SPACE, result_->status());
  EXPECT_TRUE(result_->complete());
}

class FileWriterDelegateUnlimitedTest : public FileWriterDelegateTest {
 protected:
  virtual void SetUpTestHelper(const FilePath& path) OVERRIDE;
};

}  // namespace fileapi
