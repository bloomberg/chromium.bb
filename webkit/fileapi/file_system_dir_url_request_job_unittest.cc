// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// NOTE: These tests are run as part of "unit_tests" (in chrome/test/unit)
// rather than as part of test_shell_tests because they rely on being able
// to instantiate a MessageLoop of type TYPE_IO.  test_shell_tests uses
// TYPE_UI, which URLRequest doesn't allow.
//

#include "webkit/fileapi/file_system_dir_url_request_job.h"

#include "build/build_config.h"

#include <string>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/format_macros.h"
#include "base/message_loop.h"
#include "base/platform_file.h"
#include "base/scoped_temp_dir.h"
#include "base/string_piece.h"
#include "base/threading/thread.h"
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
static const char kFileSystemURLPrefix[] = "filesystem:http://remote/temporary/";

class FileSystemDirURLRequestJobTest : public testing::Test {
 protected:
  FileSystemDirURLRequestJobTest()
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
            &FileSystemDirURLRequestJobTest::OnGetRootPath));
    MessageLoop::current()->RunAllPending();

    file_thread_.reset(
        new base::Thread("FileSystemDirURLRequestJobTest FILE Thread"));
    base::Thread::Options options(MessageLoop::TYPE_IO, 0);
    file_thread_->StartWithOptions(options);

    net::URLRequest::RegisterProtocolFactory(
        "filesystem", &FileSystemDirURLRequestJobFactory);
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
    root_path_ = root_path;
  }

  void TestRequest(const GURL& url) {
    delegate_.reset(new TestDelegate());
    delegate_->set_quit_on_redirect(true);
    request_.reset(new net::URLRequest(url, delegate_.get()));
    job_ = new FileSystemDirURLRequestJob(request_.get(), path_manager_.get(),
                                          file_thread_->message_loop_proxy());

    request_->Start();
    ASSERT_TRUE(request_->is_pending());  // verify that we're starting async
    MessageLoop::current()->Run();
  }

  void CreateDirectory(const base::StringPiece dir_name) {
    FilePath path = root_path_.AppendASCII(dir_name);
    ASSERT_TRUE(file_util::CreateDirectory(path));
  }

  GURL CreateFileSystemURL(const std::string path) {
    return GURL(kFileSystemURLPrefix + path);
  }

  static net::URLRequestJob* FileSystemDirURLRequestJobFactory(
      net::URLRequest* request,
      const std::string& scheme) {
    DCHECK(job_);
    net::URLRequestJob* temp = job_;
    job_ = NULL;
    return temp;
  }

  ScopedTempDir temp_dir_;
  FilePath root_path_;
  scoped_ptr<net::URLRequest> request_;
  scoped_ptr<TestDelegate> delegate_;
  scoped_ptr<FileSystemPathManager> path_manager_;
  scoped_ptr<base::Thread> file_thread_;

  MessageLoop message_loop_;
  base::ScopedCallbackFactory<FileSystemDirURLRequestJobTest> callback_factory_;

  static net::URLRequestJob* job_;
};

// static
net::URLRequestJob* FileSystemDirURLRequestJobTest::job_ = NULL;

// TODO(adamk): Write tighter tests once we've decided on a format for directory
// listing responses.
TEST_F(FileSystemDirURLRequestJobTest, DirectoryListing) {
  CreateDirectory("foo");
  CreateDirectory("foo/bar");
  CreateDirectory("foo/bar/baz");

  TestRequest(CreateFileSystemURL("foo/bar/"));

  ASSERT_FALSE(request_->is_pending());
  EXPECT_EQ(1, delegate_->response_started_count());
  EXPECT_FALSE(delegate_->received_data_before_response());
  EXPECT_GT(delegate_->bytes_received(), 0);
}

TEST_F(FileSystemDirURLRequestJobTest, InvalidURL) {
  TestRequest(GURL("filesystem:/foo/bar/baz"));
  ASSERT_FALSE(request_->is_pending());
  EXPECT_TRUE(delegate_->request_failed());
  ASSERT_FALSE(request_->status().is_success());
  EXPECT_EQ(net::ERR_INVALID_URL, request_->status().os_error());
}

TEST_F(FileSystemDirURLRequestJobTest, NoSuchRoot) {
  TestRequest(GURL("filesystem:http://remote/persistent/somedir/"));
  ASSERT_FALSE(request_->is_pending());
  ASSERT_FALSE(request_->status().is_success());
  EXPECT_EQ(net::ERR_FILE_NOT_FOUND, request_->status().os_error());
}

TEST_F(FileSystemDirURLRequestJobTest, NoSuchDirectory) {
  TestRequest(CreateFileSystemURL("somedir/"));
  ASSERT_FALSE(request_->is_pending());
  ASSERT_FALSE(request_->status().is_success());
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND, request_->status().os_error());
}

}  // namespace (anonymous)
}  // namespace fileapi
