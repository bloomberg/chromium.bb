// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// NOTE: These tests are run as part of "unit_tests" (in chrome/test/unit)
// rather than as part of test_shell_tests because they rely on being able
// to instantiate a MessageLoop of type TYPE_IO.  test_shell_tests uses
// TYPE_UI, which URLRequest doesn't allow.
//

#include "webkit/fileapi/file_system_url_request_job.h"

#include "build/build_config.h"

#include <string>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/format_macros.h"
#include "base/message_loop.h"
#include "base/platform_file.h"
#include "base/scoped_temp_dir.h"
#include "base/string_piece.h"
#include "base/stringprintf.h"
#include "base/threading/thread.h"
#include "base/utf_string_conversions.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/http/http_request_headers.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/fileapi/file_system_path_manager.h"

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

class FileSystemURLRequestJobTest : public testing::Test {
 protected:
  FileSystemURLRequestJobTest()
    : message_loop_(MessageLoop::TYPE_IO),  // simulate an IO thread
      ALLOW_THIS_IN_INITIALIZER_LIST(callback_factory_(this)) {
  }

  virtual void SetUp() {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    // We use the main thread so that we can get the root path synchronously.
    // TODO(adamk): Run this on the FILE thread we've created as well.
    path_manager_.reset(new FileSystemPathManager(
        base::MessageLoopProxy::CreateForCurrentThread(),
        temp_dir_.path(), false, false));

    path_manager_->GetFileSystemRootPath(
        GURL("http://remote/"), kFileSystemTypeTemporary, true,  // create
        callback_factory_.NewCallback(
            &FileSystemURLRequestJobTest::OnGetRootPath));
    MessageLoop::current()->RunAllPending();

    file_thread_.reset(
        new base::Thread("FileSystemURLRequestJobTest FILE Thread"));
    base::Thread::Options options(MessageLoop::TYPE_IO, 0);
    file_thread_->StartWithOptions(options);

    net::URLRequest::RegisterProtocolFactory("filesystem",
                                             &FileSystemURLRequestJobFactory);
  }

  virtual void TearDown() {
    // NOTE: order matters, request must die before delegate
    request_.reset(NULL);
    delegate_.reset(NULL);

    file_thread_.reset(NULL);
    net::URLRequest::RegisterProtocolFactory("filesystem", NULL);
  }

  void OnGetRootPath(bool success, const FilePath& root_path,
                     const std::string& name) {
    ASSERT_TRUE(success);
    origin_root_path_ = root_path;
  }

  void TestRequest(const GURL& url) {
    TestRequestWithHeaders(url, NULL);
  }

  void TestRequestWithHeaders(const GURL& url,
                              const net::HttpRequestHeaders* headers) {
    delegate_.reset(new TestDelegate());
    // Make delegate_ exit the MessageLoop when the request is done.
    delegate_->set_quit_on_complete(true);
    delegate_->set_quit_on_redirect(true);
    request_.reset(new net::URLRequest(url, delegate_.get()));
    if (headers)
      request_->SetExtraRequestHeaders(*headers);
    job_ = new FileSystemURLRequestJob(request_.get(), path_manager_.get(),
                                       file_thread_->message_loop_proxy());

    request_->Start();
    ASSERT_TRUE(request_->is_pending());  // verify that we're starting async
    MessageLoop::current()->Run();
  }

  void WriteFile(const base::StringPiece file_name,
                 const char* buf, int buf_size) {
    FilePath path = origin_root_path_.AppendASCII(file_name);
    ASSERT_EQ(buf_size, file_util::WriteFile(path, buf, buf_size));
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

  ScopedTempDir temp_dir_;
  FilePath origin_root_path_;
  scoped_ptr<net::URLRequest> request_;
  scoped_ptr<TestDelegate> delegate_;
  scoped_ptr<FileSystemPathManager> path_manager_;
  scoped_ptr<base::Thread> file_thread_;

  MessageLoop message_loop_;
  base::ScopedCallbackFactory<FileSystemURLRequestJobTest> callback_factory_;

  static net::URLRequestJob* job_;
};

// static
net::URLRequestJob* FileSystemURLRequestJobTest::job_ = NULL;

TEST_F(FileSystemURLRequestJobTest, FileTest) {
  WriteFile("file1.dat", kTestFileData, arraysize(kTestFileData) - 1);
  TestRequest(CreateFileSystemURL("file1.dat"));

  ASSERT_FALSE(request_->is_pending());
  EXPECT_EQ(1, delegate_->response_started_count());
  EXPECT_FALSE(delegate_->received_data_before_response());
  EXPECT_EQ(kTestFileData, delegate_->data_received());
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
  ASSERT_FALSE(request_->is_pending());
  EXPECT_TRUE(delegate_->request_failed());
  EXPECT_EQ(net::ERR_REQUEST_RANGE_NOT_SATISFIABLE,
            request_->status().os_error());
}

TEST_F(FileSystemURLRequestJobTest, RangeOutOfBounds) {
  WriteFile("file1.dat", kTestFileData, arraysize(kTestFileData) - 1);
  net::HttpRequestHeaders headers;
  headers.SetHeader(net::HttpRequestHeaders::kRange, "bytes=500-1000");
  TestRequestWithHeaders(CreateFileSystemURL("file1.dat"), &headers);

  ASSERT_FALSE(request_->is_pending());
  EXPECT_TRUE(delegate_->request_failed());
  EXPECT_EQ(net::ERR_REQUEST_RANGE_NOT_SATISFIABLE,
            request_->status().os_error());
}

TEST_F(FileSystemURLRequestJobTest, FileDirRedirect) {
  ASSERT_TRUE(file_util::CreateDirectory(origin_root_path_.AppendASCII("dir")));
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
  EXPECT_EQ(net::ERR_INVALID_URL, request_->status().os_error());
}

TEST_F(FileSystemURLRequestJobTest, NoSuchRoot) {
  TestRequest(GURL("filesystem:http://remote/persistent/somefile"));
  ASSERT_FALSE(request_->is_pending());
  EXPECT_TRUE(delegate_->request_failed());
  EXPECT_EQ(net::ERR_FILE_NOT_FOUND, request_->status().os_error());
}

TEST_F(FileSystemURLRequestJobTest, NoSuchFile) {
  TestRequest(CreateFileSystemURL("somefile"));
  ASSERT_FALSE(request_->is_pending());
  EXPECT_TRUE(delegate_->request_failed());
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND, request_->status().os_error());
}

}  // namespace (anonymous)
}  // namespace fileapi
