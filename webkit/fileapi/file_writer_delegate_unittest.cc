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
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_operation.h"
#include "webkit/fileapi/file_system_operation_context.h"
#include "webkit/fileapi/file_system_path_manager.h"
#include "webkit/fileapi/file_system_usage_cache.h"
#include "webkit/fileapi/file_writer_delegate.h"
#include "webkit/fileapi/quota_file_util.h"
#include "webkit/quota/quota_manager.h"

using quota::QuotaClient;
using quota::QuotaManager;
using quota::QuotaManagerProxy;
using quota::StorageType;

namespace fileapi {

namespace {

class MockFileSystemPathManager : public FileSystemPathManager {
 public:
  MockFileSystemPathManager(const FilePath& filesystem_path)
      : FileSystemPathManager(base::MessageLoopProxy::CreateForCurrentThread(),
                              filesystem_path, NULL, false, true),
        test_filesystem_path_(filesystem_path) {}

  virtual FilePath ValidateFileSystemRootAndGetPathOnFileThread(
      const GURL& origin_url,
      FileSystemType type,
      const FilePath& virtual_path,
      bool create) {
    return test_filesystem_path_;
  }

 private:
  FilePath test_filesystem_path_;
};

class MockQuotaManager : public QuotaManager {
 public:
  MockQuotaManager(const FilePath& filesystem_path)
      : QuotaManager(false /* is_incognito */,
                     filesystem_path,
                     base::MessageLoopProxy::CreateForCurrentThread(),
                     base::MessageLoopProxy::CreateForCurrentThread()),
        usage_(0) {}
  int64 usage() const { return usage_; }

 protected:
  virtual void NotifyStorageModified(QuotaClient::ID client_id,
                                     const GURL& origin,
                                     StorageType type,
                                     int64 delta) {
    DCHECK(client_id == QuotaClient::kFileSystem);
    usage_ += delta;
  }

 private:
  int64 usage_;
};

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

static const char* g_content = NULL;

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
  MockDispatcher(Result* result) : result_(result) { }

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

class FileWriterDelegateTest : public PlatformTest {
 public:
  FileWriterDelegateTest()
      : loop_(MessageLoop::TYPE_IO) {}

 protected:
  virtual void SetUp();
  virtual void TearDown();

  int64 GetCachedUsage() {
    return FileSystemUsageCache::GetUsage(usage_file_path_);
  }

  int64 GetManagerUsage() const {
    DCHECK(quota_manager_.get());
    return quota_manager_->usage();
  }

  FileSystemContext* file_system_context() const {
    return file_system_context_.get();
  }

  static net::URLRequest::ProtocolFactory Factory;

  scoped_ptr<FileWriterDelegate> file_writer_delegate_;
  scoped_ptr<net::URLRequest> request_;
  scoped_ptr<FileSystemOperationContext> context_;
  scoped_ptr<Result> result_;

  scoped_refptr<FileSystemContext> file_system_context_;
  scoped_refptr<MockQuotaManager> quota_manager_;

  MessageLoop loop_;

  ScopedTempDir dir_;
  FilePath filesystem_dir_;
  FilePath usage_file_path_;
  FilePath file_path_;
  PlatformFile file_;
};

// static
net::URLRequestJob* FileWriterDelegateTest::Factory(
    net::URLRequest* request,
    const std::string& scheme) {
  return new FileWriterDelegateTestJob(request, g_content);
}

void FileWriterDelegateTest::SetUp() {
  ASSERT_TRUE(dir_.CreateUniqueTempDir());
  filesystem_dir_ = dir_.path().AppendASCII("filesystem");
  file_util::CreateDirectory(filesystem_dir_);
  ASSERT_TRUE(file_util::CreateTemporaryFileInDir(filesystem_dir_,
                                                  &file_path_));

  quota_manager_ = new MockQuotaManager(filesystem_dir_);
  file_system_context_ =
      new FileSystemContext(base::MessageLoopProxy::CreateForCurrentThread(),
                            base::MessageLoopProxy::CreateForCurrentThread(),
                            NULL, quota_manager_->proxy(),
                            FilePath(), false /* is_incognito */,
                            true, true,
                            new MockFileSystemPathManager(filesystem_dir_)),

  context_.reset(new FileSystemOperationContext(
      file_system_context_.get(), NULL));

  usage_file_path_ =
      filesystem_dir_.AppendASCII(FileSystemUsageCache::kUsageFileName);
  FileSystemUsageCache::UpdateUsage(usage_file_path_, 0);

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
  context_.reset();
  file_system_context_ = NULL;
  quota_manager_ = NULL;
  // Make sure objects that need to be freed on a particular thread go away.
  MessageLoop::current()->RunAllPending();
}

TEST_F(FileWriterDelegateTest, WriteSuccessWithoutQuotaLimit) {
  GURL blob_url("blob:nolimit");
  g_content = "The quick brown fox jumps over the lazy dog.\n";
  file_writer_delegate_.reset(new FileWriterDelegate(
      new FileSystemOperation(new MockDispatcher(result_.get()),
                              NULL, file_system_context(),
                              QuotaFileUtil::GetInstance()),
      0, base::MessageLoopProxy::CreateForCurrentThread()));
  request_.reset(new net::URLRequest(blob_url, file_writer_delegate_.get()));

  ASSERT_EQ(0, GetCachedUsage());
  ASSERT_EQ(0, GetManagerUsage());
  context_->set_allowed_bytes_growth(QuotaFileUtil::kNoLimit);
  file_writer_delegate_->Start(file_, request_.get(), *context_);
  MessageLoop::current()->Run();
  ASSERT_EQ(45, GetCachedUsage());
  ASSERT_EQ(45, GetManagerUsage());

  EXPECT_EQ(45, result_->bytes_written());
  EXPECT_EQ(base::PLATFORM_FILE_OK, result_->status());
  EXPECT_TRUE(result_->complete());

  file_writer_delegate_.reset(NULL);
}

TEST_F(FileWriterDelegateTest, WriteSuccessWithJustQuota) {
  GURL blob_url("blob:just");
  g_content = "The quick brown fox jumps over the lazy dog.\n";
  file_writer_delegate_.reset(new FileWriterDelegate(
      new FileSystemOperation(new MockDispatcher(result_.get()),
                              NULL, file_system_context(),
                              QuotaFileUtil::GetInstance()),
      0, base::MessageLoopProxy::CreateForCurrentThread()));
  request_.reset(new net::URLRequest(blob_url, file_writer_delegate_.get()));

  ASSERT_EQ(0, GetCachedUsage());
  context_->set_allowed_bytes_growth(45);
  file_writer_delegate_->Start(file_, request_.get(), *context_);
  MessageLoop::current()->Run();
  ASSERT_EQ(45, GetCachedUsage());
  ASSERT_EQ(45, GetManagerUsage());

  file_writer_delegate_.reset(NULL);

  EXPECT_EQ(45, result_->bytes_written());
  EXPECT_EQ(base::PLATFORM_FILE_OK, result_->status());
  EXPECT_TRUE(result_->complete());
}

TEST_F(FileWriterDelegateTest, WriteFailureByQuota) {
  GURL blob_url("blob:failure");
  g_content = "The quick brown fox jumps over the lazy dog.\n";
  file_writer_delegate_.reset(new FileWriterDelegate(
      new FileSystemOperation(new MockDispatcher(result_.get()),
                              NULL, file_system_context(),
                              QuotaFileUtil::GetInstance()),
      0, base::MessageLoopProxy::CreateForCurrentThread()));
  request_.reset(new net::URLRequest(blob_url, file_writer_delegate_.get()));

  ASSERT_EQ(0, GetCachedUsage());
  context_->set_allowed_bytes_growth(44);
  file_writer_delegate_->Start(file_, request_.get(), *context_);
  MessageLoop::current()->Run();
  ASSERT_EQ(44, GetCachedUsage());
  ASSERT_EQ(44, GetManagerUsage());

  file_writer_delegate_.reset(NULL);

  EXPECT_EQ(44, result_->bytes_written());
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NO_SPACE, result_->status());
  EXPECT_TRUE(result_->complete());
}

TEST_F(FileWriterDelegateTest, WriteZeroBytesSuccessfullyWithZeroQuota) {
  GURL blob_url("blob:zero");
  g_content = "";
  file_writer_delegate_.reset(new FileWriterDelegate(
      new FileSystemOperation(new MockDispatcher(result_.get()),
                              NULL, file_system_context(),
                              QuotaFileUtil::GetInstance()),
      0, base::MessageLoopProxy::CreateForCurrentThread()));
  request_.reset(new net::URLRequest(blob_url, file_writer_delegate_.get()));

  ASSERT_EQ(0, GetCachedUsage());
  context_->set_allowed_bytes_growth(0);
  file_writer_delegate_->Start(file_, request_.get(), *context_);
  MessageLoop::current()->Run();
  ASSERT_EQ(0, GetCachedUsage());
  ASSERT_EQ(0, GetManagerUsage());

  file_writer_delegate_.reset(NULL);

  EXPECT_EQ(0, result_->bytes_written());
  EXPECT_EQ(base::PLATFORM_FILE_OK, result_->status());
  EXPECT_TRUE(result_->complete());
}

TEST_F(FileWriterDelegateTest, WriteSuccessWithoutQuotaLimitConcurrent) {
  scoped_ptr<FileSystemOperationContext> context2;
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

  context2.reset(new FileSystemOperationContext(
      file_system_context(), NULL));
  result2.reset(new Result());

  GURL blob_url("blob:nolimitconcurrent");
  GURL blob_url2("blob:nolimitconcurrent2");
  g_content = "The quick brown fox jumps over the lazy dog.\n";
  file_writer_delegate_.reset(new FileWriterDelegate(
      new FileSystemOperation(new MockDispatcher(result_.get()),
                              NULL, file_system_context(),
                              QuotaFileUtil::GetInstance()),
      0, base::MessageLoopProxy::CreateForCurrentThread()));
  file_writer_delegate2.reset(new FileWriterDelegate(
      new FileSystemOperation(new MockDispatcher(result2.get()),
                              NULL, file_system_context(),
                              QuotaFileUtil::GetInstance()),
      0, base::MessageLoopProxy::CreateForCurrentThread()));
  request_.reset(new net::URLRequest(blob_url, file_writer_delegate_.get()));
  request2.reset(new net::URLRequest(blob_url2, file_writer_delegate2.get()));

  ASSERT_EQ(0, GetCachedUsage());
  context_->set_allowed_bytes_growth(QuotaFileUtil::kNoLimit);
  context2->set_allowed_bytes_growth(QuotaFileUtil::kNoLimit);
  file_writer_delegate_->Start(file_, request_.get(), *context_);
  file_writer_delegate2->Start(file2, request2.get(), *context2);
  MessageLoop::current()->Run();
  if (!result_->complete() || !result2->complete())
    MessageLoop::current()->Run();
  ASSERT_EQ(90, GetCachedUsage());
  ASSERT_EQ(90, GetManagerUsage());

  file_writer_delegate_.reset(NULL);

  EXPECT_EQ(45, result_->bytes_written());
  EXPECT_EQ(base::PLATFORM_FILE_OK, result_->status());
  EXPECT_TRUE(result_->complete());
  EXPECT_EQ(45, result2->bytes_written());
  EXPECT_EQ(base::PLATFORM_FILE_OK, result2->status());
  EXPECT_TRUE(result2->complete());
}

}  // namespace fileapi
