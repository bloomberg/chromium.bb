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
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/http/http_request_headers.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_path_manager.h"

namespace fileapi {
namespace {

// We always use the TEMPORARY FileSystem in this test.
static const char kFileSystemURLPrefix[] = "filesystem:http://remote/temporary/";

class TestSpecialStoragePolicy : public quota::SpecialStoragePolicy {
 public:
  virtual bool IsStorageProtected(const GURL& origin) {
    return false;
  }

  virtual bool IsStorageUnlimited(const GURL& origin) {
    return true;
  }

  virtual bool IsFileHandler(const std::string& extension_id) {
    return true;
  }
};

class FileSystemDirURLRequestJobTest : public testing::Test {
 protected:
  FileSystemDirURLRequestJobTest()
    : message_loop_(MessageLoop::TYPE_IO),  // simulate an IO thread
      ALLOW_THIS_IN_INITIALIZER_LIST(callback_factory_(this)) {
  }

  virtual void SetUp() {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    file_thread_proxy_ = base::MessageLoopProxy::CreateForCurrentThread();

    special_storage_policy_ = new TestSpecialStoragePolicy();
    file_system_context_ =
        new FileSystemContext(
            base::MessageLoopProxy::CreateForCurrentThread(),
            base::MessageLoopProxy::CreateForCurrentThread(),
            special_storage_policy_, NULL,
            FilePath(), false /* is_incognito */,
            false, true,
            new FileSystemPathManager(
                    file_thread_proxy_, temp_dir_.path(),
                    NULL, false, false));

    file_system_context_->path_manager()->ValidateFileSystemRootAndGetURL(
        GURL("http://remote/"), kFileSystemTypeTemporary, true,  // create
        callback_factory_.NewCallback(
            &FileSystemDirURLRequestJobTest::OnGetRootPath));
    MessageLoop::current()->RunAllPending();

    net::URLRequest::RegisterProtocolFactory(
        "filesystem", &FileSystemDirURLRequestJobFactory);
  }

  virtual void TearDown() {
    // NOTE: order matters, request must die before delegate
    request_.reset(NULL);
    delegate_.reset(NULL);

    net::URLRequest::RegisterProtocolFactory("filesystem", NULL);
  }

  void OnGetRootPath(bool success, const FilePath& root_path,
                     const std::string& name) {
    ASSERT_TRUE(success);
    root_path_ = root_path;
  }

  void TestRequestHelper(const GURL& url, bool run_to_completion) {
    delegate_.reset(new TestDelegate());
    delegate_->set_quit_on_redirect(true);
    request_.reset(new net::URLRequest(url, delegate_.get()));
    job_ = new FileSystemDirURLRequestJob(request_.get(),
                                          file_system_context_.get(),
                                          file_thread_proxy_);

    request_->Start();
    ASSERT_TRUE(request_->is_pending());  // verify that we're starting async
    if (run_to_completion)
      MessageLoop::current()->Run();
  }

  void TestRequest(const GURL& url) {
    TestRequestHelper(url, true);
  }

  void TestRequestNoRun(const GURL& url) {
    TestRequestHelper(url, false);
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
  scoped_refptr<TestSpecialStoragePolicy> special_storage_policy_;
  scoped_refptr<FileSystemContext> file_system_context_;
  scoped_refptr<base::MessageLoopProxy> file_thread_proxy_;

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
  EXPECT_EQ(net::ERR_FILE_NOT_FOUND, request_->status().os_error());
}

class QuitNowTask : public Task {
 public:
  virtual void Run() {
    MessageLoop::current()->QuitNow();
  }
};

TEST_F(FileSystemDirURLRequestJobTest, Cancel) {
  CreateDirectory("foo");
  TestRequestNoRun(CreateFileSystemURL("foo/"));
  // Run StartAsync() and only StartAsync().
  MessageLoop::current()->PostTask(FROM_HERE, new QuitNowTask);
  MessageLoop::current()->Run();

  request_.reset();
  MessageLoop::current()->RunAllPending();
  // If we get here, success! we didn't crash!
}

}  // namespace (anonymous)
}  // namespace fileapi
