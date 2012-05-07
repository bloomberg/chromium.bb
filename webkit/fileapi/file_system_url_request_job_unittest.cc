// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// NOTE: These tests are run as part of "unit_tests" (in chrome/test/unit)
// rather than as part of test_shell_tests because they rely on being able
// to instantiate a MessageLoop of type TYPE_IO.  test_shell_tests uses
// TYPE_UI, which URLRequest doesn't allow.
//

#include "webkit/fileapi/file_system_url_request_job.h"

#include <string>

#include "base/bind.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/format_macros.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "base/platform_file.h"
#include "base/scoped_temp_dir.h"
#include "base/string_piece.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "net/base/load_flags.h"
#include "net/base/mime_util.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/http/http_request_headers.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_file_util.h"
#include "webkit/fileapi/file_system_operation_context.h"
#include "webkit/fileapi/mock_file_system_options.h"
#include "webkit/fileapi/sandbox_mount_point_provider.h"
#include "webkit/quota/mock_special_storage_policy.h"

namespace fileapi {
namespace {

// We always use the TEMPORARY FileSystem in this test.
const char kFileSystemURLPrefix[] = "filesystem:http://remote/temporary/";
const char kTestFileData[] = "0123456789";

void FillBuffer(char* buffer, size_t len) {
  static bool called = false;
  if (!called) {
    called = true;
    int seed = static_cast<int>(base::Time::Now().ToInternalValue());
    srand(seed);
  }

  for (size_t i = 0; i < len; i++) {
    buffer[i] = static_cast<char>(rand());
    if (!buffer[i])
      buffer[i] = 'g';
  }
}

}  // namespace

class FileSystemURLRequestJobTest : public testing::Test {
 protected:
  FileSystemURLRequestJobTest()
    : message_loop_(MessageLoop::TYPE_IO),  // simulate an IO thread
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
  }

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    special_storage_policy_ = new quota::MockSpecialStoragePolicy;
    // We use the main thread so that we can get the root path synchronously.
    // TODO(adamk): Run this on the FILE thread we've created as well.
    file_system_context_ =
        new FileSystemContext(
            base::MessageLoopProxy::current(),
            base::MessageLoopProxy::current(),
            special_storage_policy_, NULL,
            temp_dir_.path(),
            CreateDisallowFileAccessOptions());

    file_system_context_->sandbox_provider()->ValidateFileSystemRoot(
        GURL("http://remote/"), kFileSystemTypeTemporary, true,  // create
        base::Bind(&FileSystemURLRequestJobTest::OnValidateFileSystem,
                   weak_factory_.GetWeakPtr()));
    MessageLoop::current()->RunAllPending();

    net::URLRequest::Deprecated::RegisterProtocolFactory(
        "filesystem", &FileSystemURLRequestJobFactory);
  }

  virtual void TearDown() OVERRIDE {
    net::URLRequest::Deprecated::RegisterProtocolFactory("filesystem", NULL);
    ClearUnusedJob();
    if (pending_job_) {
      pending_job_->Kill();
      pending_job_ = NULL;
    }
    // FileReader posts a task to close the file in destructor.
    MessageLoop::current()->RunAllPending();
  }

  void OnValidateFileSystem(base::PlatformFileError result) {
    ASSERT_EQ(base::PLATFORM_FILE_OK, result);
  }

  void TestRequestHelper(const GURL& url,
                         const net::HttpRequestHeaders* headers,
                         bool run_to_completion) {
    delegate_.reset(new TestDelegate());
    // Make delegate_ exit the MessageLoop when the request is done.
    delegate_->set_quit_on_complete(true);
    delegate_->set_quit_on_redirect(true);
    request_.reset(new net::URLRequest(url, delegate_.get()));
    if (headers)
      request_->SetExtraRequestHeaders(*headers);
    ASSERT_TRUE(!job_);
    job_ = new FileSystemURLRequestJob(
        request_.get(),
        file_system_context_.get());
    pending_job_ = job_;

    request_->Start();
    ASSERT_TRUE(request_->is_pending());  // verify that we're starting async
    if (run_to_completion)
      MessageLoop::current()->Run();
  }

  void TestRequest(const GURL& url) {
    TestRequestHelper(url, NULL, true);
  }

  void TestRequestWithHeaders(const GURL& url,
                              const net::HttpRequestHeaders* headers) {
    TestRequestHelper(url, headers, true);
  }

  void TestRequestNoRun(const GURL& url) {
    TestRequestHelper(url, NULL, false);
  }

  void CreateDirectory(const base::StringPiece& dir_name) {
    FileSystemFileUtil* file_util = file_system_context_->
        sandbox_provider()->GetFileUtil();
    FileSystemPath path(GURL("http://remote"),
                        kFileSystemTypeTemporary,
                        FilePath().AppendASCII(dir_name));

    FileSystemOperationContext context(file_system_context_);
    context.set_allowed_bytes_growth(1024);

    ASSERT_EQ(base::PLATFORM_FILE_OK, file_util->CreateDirectory(
        &context,
        path,
        false /* exclusive */,
        false /* recursive */));
  }

  void WriteFile(const base::StringPiece& file_name,
                 const char* buf, int buf_size) {
    FileSystemFileUtil* file_util = file_system_context_->
        sandbox_provider()->GetFileUtil();
    FileSystemPath path(GURL("http://remote"),
                        kFileSystemTypeTemporary,
                        FilePath().AppendASCII(file_name));

    FileSystemOperationContext context(file_system_context_);
    context.set_allowed_bytes_growth(1024);

    base::PlatformFile handle = base::kInvalidPlatformFileValue;
    bool created = false;
    ASSERT_EQ(base::PLATFORM_FILE_OK, file_util->CreateOrOpen(
        &context,
        path,
        base::PLATFORM_FILE_CREATE | base::PLATFORM_FILE_WRITE,
        &handle,
        &created));
    EXPECT_TRUE(created);
    ASSERT_NE(base::kInvalidPlatformFileValue, handle);
    ASSERT_EQ(buf_size,
        base::WritePlatformFile(handle, 0 /* offset */, buf, buf_size));
    base::ClosePlatformFile(handle);
  }

  GURL CreateFileSystemURL(const std::string& path) {
    return GURL(kFileSystemURLPrefix + path);
  }

  static net::URLRequestJob* FileSystemURLRequestJobFactory(
      net::URLRequest* request,
      const std::string& scheme) {
    DCHECK(job_);
    net::URLRequestJob* temp = job_;
    job_ = NULL;
    return temp;
  }

  static void ClearUnusedJob() {
    if (job_) {
      scoped_refptr<net::URLRequestJob> deleter = job_;
      job_ = NULL;
    }
  }

  // Put the message loop at the top, so that it's the last thing deleted.
  MessageLoop message_loop_;

  ScopedTempDir temp_dir_;
  scoped_refptr<quota::MockSpecialStoragePolicy> special_storage_policy_;
  scoped_refptr<FileSystemContext> file_system_context_;
  base::WeakPtrFactory<FileSystemURLRequestJobTest> weak_factory_;

  // NOTE: order matters, request must die before delegate
  scoped_ptr<TestDelegate> delegate_;
  scoped_ptr<net::URLRequest> request_;

  scoped_refptr<net::URLRequestJob> pending_job_;
  static net::URLRequestJob* job_;
};

// static
net::URLRequestJob* FileSystemURLRequestJobTest::job_ = NULL;

namespace {

TEST_F(FileSystemURLRequestJobTest, FileTest) {
  WriteFile("file1.dat", kTestFileData, arraysize(kTestFileData) - 1);
  TestRequest(CreateFileSystemURL("file1.dat"));

  ASSERT_FALSE(request_->is_pending());
  EXPECT_EQ(1, delegate_->response_started_count());
  EXPECT_FALSE(delegate_->received_data_before_response());
  EXPECT_EQ(kTestFileData, delegate_->data_received());
  EXPECT_EQ(200, request_->GetResponseCode());
  std::string cache_control;
  request_->GetResponseHeaderByName("cache-control", &cache_control);
  EXPECT_EQ("no-cache", cache_control);
}

TEST_F(FileSystemURLRequestJobTest, FileTestFullSpecifiedRange) {
  const size_t buffer_size = 4000;
  scoped_array<char> buffer(new char[buffer_size]);
  FillBuffer(buffer.get(), buffer_size);
  WriteFile("bigfile", buffer.get(), buffer_size);

  const size_t first_byte_position = 500;
  const size_t last_byte_position = buffer_size - first_byte_position;
  std::string partial_buffer_string(buffer.get() + first_byte_position,
                                    buffer.get() + last_byte_position + 1);

  net::HttpRequestHeaders headers;
  headers.SetHeader(net::HttpRequestHeaders::kRange,
                    base::StringPrintf(
                         "bytes=%" PRIuS "-%" PRIuS,
                         first_byte_position, last_byte_position));
  TestRequestWithHeaders(CreateFileSystemURL("bigfile"), &headers);

  ASSERT_FALSE(request_->is_pending());
  EXPECT_EQ(1, delegate_->response_started_count());
  EXPECT_FALSE(delegate_->received_data_before_response());
  EXPECT_TRUE(partial_buffer_string == delegate_->data_received());
}

TEST_F(FileSystemURLRequestJobTest, FileTestHalfSpecifiedRange) {
  const size_t buffer_size = 4000;
  scoped_array<char> buffer(new char[buffer_size]);
  FillBuffer(buffer.get(), buffer_size);
  WriteFile("bigfile", buffer.get(), buffer_size);

  const size_t first_byte_position = 500;
  std::string partial_buffer_string(buffer.get() + first_byte_position,
                                    buffer.get() + buffer_size);

  net::HttpRequestHeaders headers;
  headers.SetHeader(net::HttpRequestHeaders::kRange,
                    base::StringPrintf("bytes=%" PRIuS "-",
                                       first_byte_position));
  TestRequestWithHeaders(CreateFileSystemURL("bigfile"), &headers);
  ASSERT_FALSE(request_->is_pending());
  EXPECT_EQ(1, delegate_->response_started_count());
  EXPECT_FALSE(delegate_->received_data_before_response());
  // Don't use EXPECT_EQ, it will print out a lot of garbage if check failed.
  EXPECT_TRUE(partial_buffer_string == delegate_->data_received());
}


TEST_F(FileSystemURLRequestJobTest, FileTestMultipleRangesNotSupported) {
  WriteFile("file1.dat", kTestFileData, arraysize(kTestFileData) - 1);
  net::HttpRequestHeaders headers;
  headers.SetHeader(net::HttpRequestHeaders::kRange,
                    "bytes=0-5,10-200,200-300");
  TestRequestWithHeaders(CreateFileSystemURL("file1.dat"), &headers);
  EXPECT_TRUE(delegate_->request_failed());
  EXPECT_EQ(net::ERR_REQUEST_RANGE_NOT_SATISFIABLE,
            request_->status().error());
}

TEST_F(FileSystemURLRequestJobTest, RangeOutOfBounds) {
  WriteFile("file1.dat", kTestFileData, arraysize(kTestFileData) - 1);
  net::HttpRequestHeaders headers;
  headers.SetHeader(net::HttpRequestHeaders::kRange, "bytes=500-1000");
  TestRequestWithHeaders(CreateFileSystemURL("file1.dat"), &headers);

  ASSERT_FALSE(request_->is_pending());
  EXPECT_TRUE(delegate_->request_failed());
  EXPECT_EQ(net::ERR_REQUEST_RANGE_NOT_SATISFIABLE,
            request_->status().error());
}

TEST_F(FileSystemURLRequestJobTest, FileDirRedirect) {
  CreateDirectory("dir");
  TestRequest(CreateFileSystemURL("dir"));

  EXPECT_EQ(1, delegate_->received_redirect_count());
  EXPECT_TRUE(request_->status().is_success());
  EXPECT_FALSE(delegate_->request_failed());

  // We've deferred the redirect; now cancel the request to avoid following it.
  request_->Cancel();
  MessageLoop::current()->Run();
}

TEST_F(FileSystemURLRequestJobTest, InvalidURL) {
  TestRequest(GURL("filesystem:/foo/bar/baz"));
  ASSERT_FALSE(request_->is_pending());
  EXPECT_TRUE(delegate_->request_failed());
  EXPECT_EQ(net::ERR_INVALID_URL, request_->status().error());
}

TEST_F(FileSystemURLRequestJobTest, NoSuchRoot) {
  TestRequest(GURL("filesystem:http://remote/persistent/somefile"));
  ASSERT_FALSE(request_->is_pending());
  EXPECT_TRUE(delegate_->request_failed());
  EXPECT_EQ(net::ERR_FILE_NOT_FOUND, request_->status().error());
}

TEST_F(FileSystemURLRequestJobTest, NoSuchFile) {
  TestRequest(CreateFileSystemURL("somefile"));
  ASSERT_FALSE(request_->is_pending());
  EXPECT_TRUE(delegate_->request_failed());
  EXPECT_EQ(net::ERR_FILE_NOT_FOUND, request_->status().error());
}

TEST_F(FileSystemURLRequestJobTest, Cancel) {
  WriteFile("file1.dat", kTestFileData, arraysize(kTestFileData) - 1);
  TestRequestNoRun(CreateFileSystemURL("file1.dat"));

  // Run StartAsync() and only StartAsync().
  MessageLoop::current()->DeleteSoon(FROM_HERE, request_.release());
  MessageLoop::current()->RunAllPending();
  // If we get here, success! we didn't crash!
}

TEST_F(FileSystemURLRequestJobTest, GetMimeType) {
  const char kFilename[] = "hoge.html";

  std::string mime_type_direct;
  FilePath::StringType extension =
      FilePath().AppendASCII(kFilename).Extension();
  if (!extension.empty())
    extension = extension.substr(1);
  EXPECT_TRUE(net::GetWellKnownMimeTypeFromExtension(
      extension, &mime_type_direct));

  TestRequest(CreateFileSystemURL(kFilename));

  std::string mime_type_from_job;
  request_->GetMimeType(&mime_type_from_job);
  EXPECT_EQ(mime_type_direct, mime_type_from_job);
}

}  // namespace
}  // namespace fileapi
