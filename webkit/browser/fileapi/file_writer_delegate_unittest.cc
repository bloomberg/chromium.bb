// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/scoped_temp_dir.h"
#include "base/message_loop/message_loop.h"
#include "net/base/io_buffer.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_job.h"
#include "net/url_request/url_request_status.h"
#include "testing/platform_test.h"
#include "url/gurl.h"
#include "webkit/browser/fileapi/file_system_context.h"
#include "webkit/browser/fileapi/file_system_file_util.h"
#include "webkit/browser/fileapi/file_system_operation_context.h"
#include "webkit/browser/fileapi/file_system_quota_util.h"
#include "webkit/browser/fileapi/file_writer_delegate.h"
#include "webkit/browser/fileapi/mock_file_system_context.h"
#include "webkit/browser/fileapi/sandbox_file_stream_writer.h"

namespace fileapi {

namespace {

const GURL kOrigin("http://example.com");
const FileSystemType kFileSystemType = kFileSystemTypeTest;

const char kData[] = "The quick brown fox jumps over the lazy dog.\n";
const int kDataSize = ARRAYSIZE_UNSAFE(kData) - 1;

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
        base::MessageLoop::current()->Quit();
    } else {
      EXPECT_EQ(base::PLATFORM_FILE_OK, status_);
      status_ = status;
      base::MessageLoop::current()->Quit();
    }
  }

 private:
  // For post-operation status.
  base::PlatformFileError status_;
  int64 bytes_written_;
  FileWriterDelegate::WriteProgressStatus write_status_;
};

}  // namespace (anonymous)

class FileWriterDelegateTest : public PlatformTest {
 public:
  FileWriterDelegateTest()
      : loop_(base::MessageLoop::TYPE_IO) {}

 protected:
  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

  FileSystemFileUtil* file_util() {
    return file_system_context_->GetFileUtil(kFileSystemType);
  }

  int64 usage() {
    return file_system_context_->GetQuotaUtil(kFileSystemType)
        ->GetOriginUsageOnFileThread(
              file_system_context_.get(), kOrigin, kFileSystemType);
  }

  int64 GetFileSizeOnDisk(const char* test_file_path) {
    // There might be in-flight flush/write.
    base::MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(&base::DoNothing));
    base::MessageLoop::current()->RunUntilIdle();

    FileSystemURL url = GetFileSystemURL(test_file_path);
    base::PlatformFileInfo file_info;
    base::FilePath platform_path;
    EXPECT_EQ(base::PLATFORM_FILE_OK,
              file_util()->GetFileInfo(NewOperationContext().get(), url,
                                       &file_info, &platform_path));
    return file_info.size;
  }

  FileSystemURL GetFileSystemURL(const char* file_name) const {
    return file_system_context_->CreateCrackedFileSystemURL(
        kOrigin, kFileSystemType, base::FilePath().FromUTF8Unsafe(file_name));
  }

  scoped_ptr<FileSystemOperationContext> NewOperationContext() {
    FileSystemOperationContext* context =
        new FileSystemOperationContext(file_system_context_.get());
    context->set_update_observers(
        *file_system_context_->GetUpdateObservers(kFileSystemType));
    context->set_root_path(dir_.path());
    return make_scoped_ptr(context);
  }

  FileWriterDelegate* CreateWriterDelegate(
      const char* test_file_path,
      int64 offset,
      int64 allowed_growth) {
    SandboxFileStreamWriter* writer = new SandboxFileStreamWriter(
        file_system_context_.get(),
        GetFileSystemURL(test_file_path),
        offset,
        *file_system_context_->GetUpdateObservers(kFileSystemType));
    writer->set_default_quota(allowed_growth);
    return new FileWriterDelegate(scoped_ptr<FileStreamWriter>(writer));
  }

  FileWriterDelegate::DelegateWriteCallback GetWriteCallback(Result* result) {
    return base::Bind(&Result::DidWrite, base::Unretained(result));
  }

  // Creates and sets up a FileWriterDelegate for writing the given |blob_url|,
  // and creates a new FileWriterDelegate for the file.
  void PrepareForWrite(const char* test_file_path,
                       const GURL& blob_url,
                       int64 offset,
                       int64 allowed_growth) {
    file_writer_delegate_.reset(
        CreateWriterDelegate(test_file_path, offset, allowed_growth));
    request_.reset(empty_context_.CreateRequest(
        blob_url, file_writer_delegate_.get()));
  }

  static net::URLRequest::ProtocolFactory Factory;

  // This should be alive until the very end of this instance.
  base::MessageLoop loop_;

  scoped_refptr<FileSystemContext> file_system_context_;

  net::URLRequestContext empty_context_;
  scoped_ptr<FileWriterDelegate> file_writer_delegate_;
  scoped_ptr<net::URLRequest> request_;

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
    base::MessageLoop::current()->PostTask(
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

  file_system_context_ = CreateFileSystemContextForTesting(
      NULL, dir_.path());

  bool created = false;
  scoped_ptr<FileSystemOperationContext> context = NewOperationContext();
  context->set_allowed_bytes_growth(kint64max);
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
  file_system_context_ = NULL;
  base::MessageLoop::current()->RunUntilIdle();
}

TEST_F(FileWriterDelegateTest, WriteSuccessWithoutQuotaLimit) {
  const GURL kBlobURL("blob:nolimit");
  content_ = kData;

  PrepareForWrite("test", kBlobURL, 0, kint64max);

  Result result;
  ASSERT_EQ(0, usage());
  file_writer_delegate_->Start(request_.Pass(), GetWriteCallback(&result));
  base::MessageLoop::current()->Run();

  ASSERT_EQ(FileWriterDelegate::SUCCESS_COMPLETED, result.write_status());
  file_writer_delegate_.reset();

  ASSERT_EQ(kDataSize, usage());
  EXPECT_EQ(GetFileSizeOnDisk("test"), usage());
  EXPECT_EQ(kDataSize, result.bytes_written());
  EXPECT_EQ(base::PLATFORM_FILE_OK, result.status());
}

TEST_F(FileWriterDelegateTest, WriteSuccessWithJustQuota) {
  const GURL kBlobURL("blob:just");
  content_ = kData;
  const int64 kAllowedGrowth = kDataSize;
  PrepareForWrite("test", kBlobURL, 0, kAllowedGrowth);

  Result result;
  ASSERT_EQ(0, usage());
  file_writer_delegate_->Start(request_.Pass(), GetWriteCallback(&result));
  base::MessageLoop::current()->Run();
  ASSERT_EQ(FileWriterDelegate::SUCCESS_COMPLETED, result.write_status());
  file_writer_delegate_.reset();

  ASSERT_EQ(kAllowedGrowth, usage());
  EXPECT_EQ(GetFileSizeOnDisk("test"), usage());

  EXPECT_EQ(kAllowedGrowth, result.bytes_written());
  EXPECT_EQ(base::PLATFORM_FILE_OK, result.status());
}

TEST_F(FileWriterDelegateTest, DISABLED_WriteFailureByQuota) {
  const GURL kBlobURL("blob:failure");
  content_ = kData;
  const int64 kAllowedGrowth = kDataSize - 1;
  PrepareForWrite("test", kBlobURL, 0, kAllowedGrowth);

  Result result;
  ASSERT_EQ(0, usage());
  file_writer_delegate_->Start(request_.Pass(), GetWriteCallback(&result));
  base::MessageLoop::current()->Run();
  ASSERT_EQ(FileWriterDelegate::ERROR_WRITE_STARTED, result.write_status());
  file_writer_delegate_.reset();

  ASSERT_EQ(kAllowedGrowth, usage());
  EXPECT_EQ(GetFileSizeOnDisk("test"), usage());

  EXPECT_EQ(kAllowedGrowth, result.bytes_written());
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NO_SPACE, result.status());
  ASSERT_EQ(FileWriterDelegate::ERROR_WRITE_STARTED, result.write_status());
}

TEST_F(FileWriterDelegateTest, WriteZeroBytesSuccessfullyWithZeroQuota) {
  const GURL kBlobURL("blob:zero");
  content_ = "";
  int64 kAllowedGrowth = 0;
  PrepareForWrite("test", kBlobURL, 0, kAllowedGrowth);

  Result result;
  ASSERT_EQ(0, usage());
  file_writer_delegate_->Start(request_.Pass(), GetWriteCallback(&result));
  base::MessageLoop::current()->Run();
  ASSERT_EQ(FileWriterDelegate::SUCCESS_COMPLETED, result.write_status());
  file_writer_delegate_.reset();

  ASSERT_EQ(kAllowedGrowth, usage());
  EXPECT_EQ(GetFileSizeOnDisk("test"), usage());

  EXPECT_EQ(kAllowedGrowth, result.bytes_written());
  EXPECT_EQ(base::PLATFORM_FILE_OK, result.status());
  ASSERT_EQ(FileWriterDelegate::SUCCESS_COMPLETED, result.write_status());
}

TEST_F(FileWriterDelegateTest, WriteSuccessWithoutQuotaLimitConcurrent) {
  scoped_ptr<FileWriterDelegate> file_writer_delegate2;
  scoped_ptr<net::URLRequest> request2;

  bool created = false;
  file_util()->EnsureFileExists(NewOperationContext().get(),
                                GetFileSystemURL("test2"),
                                &created);
  ASSERT_TRUE(created);

  const GURL kBlobURL("blob:nolimitconcurrent");
  const GURL kBlobURL2("blob:nolimitconcurrent2");
  content_ = kData;

  PrepareForWrite("test", kBlobURL, 0, kint64max);

  // Credate another FileWriterDelegate for concurrent write.
  file_writer_delegate2.reset(CreateWriterDelegate("test2", 0, kint64max));
  request2.reset(empty_context_.CreateRequest(
      kBlobURL2, file_writer_delegate2.get()));

  Result result, result2;
  ASSERT_EQ(0, usage());
  file_writer_delegate_->Start(request_.Pass(), GetWriteCallback(&result));
  file_writer_delegate2->Start(request2.Pass(), GetWriteCallback(&result2));
  base::MessageLoop::current()->Run();
  if (result.write_status() == FileWriterDelegate::SUCCESS_IO_PENDING ||
      result2.write_status() == FileWriterDelegate::SUCCESS_IO_PENDING)
    base::MessageLoop::current()->Run();

  ASSERT_EQ(FileWriterDelegate::SUCCESS_COMPLETED, result.write_status());
  ASSERT_EQ(FileWriterDelegate::SUCCESS_COMPLETED, result2.write_status());
  file_writer_delegate_.reset();
  file_writer_delegate2.reset();

  ASSERT_EQ(kDataSize * 2, usage());
  EXPECT_EQ(GetFileSizeOnDisk("test") + GetFileSizeOnDisk("test2"), usage());

  EXPECT_EQ(kDataSize, result.bytes_written());
  EXPECT_EQ(base::PLATFORM_FILE_OK, result.status());
  EXPECT_EQ(kDataSize, result2.bytes_written());
  EXPECT_EQ(base::PLATFORM_FILE_OK, result2.status());
}

TEST_F(FileWriterDelegateTest, WritesWithQuotaAndOffset) {
  const GURL kBlobURL("blob:failure-with-updated-quota");
  content_ = kData;

  // Writing kDataSize (=45) bytes data while allowed_growth is 100.
  int64 offset = 0;
  int64 allowed_growth = 100;
  ASSERT_LT(kDataSize, allowed_growth);
  PrepareForWrite("test", kBlobURL, offset, allowed_growth);

  {
    Result result;
    ASSERT_EQ(0, usage());
    file_writer_delegate_->Start(request_.Pass(), GetWriteCallback(&result));
    base::MessageLoop::current()->Run();
    ASSERT_EQ(FileWriterDelegate::SUCCESS_COMPLETED, result.write_status());
    file_writer_delegate_.reset();

    ASSERT_EQ(kDataSize, usage());
    EXPECT_EQ(GetFileSizeOnDisk("test"), usage());
    EXPECT_EQ(kDataSize, result.bytes_written());
    EXPECT_EQ(base::PLATFORM_FILE_OK, result.status());
  }

  // Trying to overwrite kDataSize bytes data while allowed_growth is 20.
  offset = 0;
  allowed_growth = 20;
  PrepareForWrite("test", kBlobURL, offset, allowed_growth);

  {
    Result result;
    file_writer_delegate_->Start(request_.Pass(), GetWriteCallback(&result));
    base::MessageLoop::current()->Run();
    EXPECT_EQ(kDataSize, usage());
    EXPECT_EQ(GetFileSizeOnDisk("test"), usage());
    EXPECT_EQ(kDataSize, result.bytes_written());
    EXPECT_EQ(base::PLATFORM_FILE_OK, result.status());
    ASSERT_EQ(FileWriterDelegate::SUCCESS_COMPLETED, result.write_status());
  }

  // Trying to write kDataSize bytes data from offset 25 while
  // allowed_growth is 55.
  offset = 25;
  allowed_growth = 55;
  PrepareForWrite("test", kBlobURL, offset, allowed_growth);

  {
    Result result;
    file_writer_delegate_->Start(request_.Pass(), GetWriteCallback(&result));
    base::MessageLoop::current()->Run();
    ASSERT_EQ(FileWriterDelegate::SUCCESS_COMPLETED, result.write_status());
    file_writer_delegate_.reset();

    EXPECT_EQ(offset + kDataSize, usage());
    EXPECT_EQ(GetFileSizeOnDisk("test"), usage());
    EXPECT_EQ(kDataSize, result.bytes_written());
    EXPECT_EQ(base::PLATFORM_FILE_OK, result.status());
  }

  // Trying to overwrite 45 bytes data while allowed_growth is -20.
  offset = 0;
  allowed_growth = -20;
  PrepareForWrite("test", kBlobURL, offset, allowed_growth);
  int64 pre_write_usage = GetFileSizeOnDisk("test");

  {
    Result result;
    file_writer_delegate_->Start(request_.Pass(), GetWriteCallback(&result));
    base::MessageLoop::current()->Run();
    ASSERT_EQ(FileWriterDelegate::SUCCESS_COMPLETED, result.write_status());
    file_writer_delegate_.reset();

    EXPECT_EQ(pre_write_usage, usage());
    EXPECT_EQ(GetFileSizeOnDisk("test"), usage());
    EXPECT_EQ(kDataSize, result.bytes_written());
    EXPECT_EQ(base::PLATFORM_FILE_OK, result.status());
  }

  // Trying to overwrite 45 bytes data with offset pre_write_usage - 20,
  // while allowed_growth is 10.
  const int kOverlap = 20;
  offset = pre_write_usage - kOverlap;
  allowed_growth = 10;
  PrepareForWrite("test", kBlobURL, offset, allowed_growth);

  {
    Result result;
    file_writer_delegate_->Start(request_.Pass(), GetWriteCallback(&result));
    base::MessageLoop::current()->Run();
    ASSERT_EQ(FileWriterDelegate::ERROR_WRITE_STARTED, result.write_status());
    file_writer_delegate_.reset();

    EXPECT_EQ(pre_write_usage + allowed_growth, usage());
    EXPECT_EQ(GetFileSizeOnDisk("test"), usage());
    EXPECT_EQ(kOverlap + allowed_growth, result.bytes_written());
    EXPECT_EQ(base::PLATFORM_FILE_ERROR_NO_SPACE, result.status());
  }
}

}  // namespace fileapi
