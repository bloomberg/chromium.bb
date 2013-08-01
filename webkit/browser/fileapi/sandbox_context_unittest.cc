// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/browser/fileapi/sandbox_context.h"

#include "base/basictypes.h"
#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_loop_proxy.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "webkit/browser/fileapi/file_system_url.h"
#include "webkit/browser/fileapi/mock_file_system_options.h"

namespace fileapi {

namespace {

FileSystemURL CreateFileSystemURL(const char* path) {
  const GURL kOrigin("http://foo/");
  return FileSystemURL::CreateForTest(
      kOrigin, kFileSystemTypeTemporary, base::FilePath::FromUTF8Unsafe(path));
}

}  // namespace

class SandboxContextTest : public testing::Test {
 protected:
  virtual void SetUp() {
    ASSERT_TRUE(data_dir_.CreateUniqueTempDir());
    context_.reset(new SandboxContext(
        NULL /* quota_manager_proxy */,
        base::MessageLoopProxy::current().get(),
        data_dir_.path(),
        NULL /* special_storage_policy */,
        CreateAllowFileAccessOptions()));
  }

  base::ScopedTempDir data_dir_;
  base::MessageLoop message_loop_;
  scoped_ptr<SandboxContext> context_;
};

TEST_F(SandboxContextTest, IsAccessValid) {
  // Normal case.
  EXPECT_TRUE(context_->IsAccessValid(CreateFileSystemURL("a")));

  // Access to a path with parent references ('..') should be disallowed.
  EXPECT_FALSE(context_->IsAccessValid(CreateFileSystemURL("a/../b")));

  // Access from non-allowed scheme should be disallowed.
  EXPECT_FALSE(context_->IsAccessValid(
      FileSystemURL::CreateForTest(
          GURL("unknown://bar"), kFileSystemTypeTemporary,
          base::FilePath::FromUTF8Unsafe("foo"))));

  // Access with restricted name should be disallowed.
  EXPECT_FALSE(context_->IsAccessValid(CreateFileSystemURL(".")));
  EXPECT_FALSE(context_->IsAccessValid(CreateFileSystemURL("..")));

  // This is also disallowed due to Windows XP parent path handling.
  EXPECT_FALSE(context_->IsAccessValid(CreateFileSystemURL("...")));

  // These are identified as unsafe cases due to weird path handling
  // on Windows.
  EXPECT_FALSE(context_->IsAccessValid(CreateFileSystemURL(" ..")));
  EXPECT_FALSE(context_->IsAccessValid(CreateFileSystemURL(".. ")));

  // Similar but safe cases.
  EXPECT_TRUE(context_->IsAccessValid(CreateFileSystemURL(" .")));
  EXPECT_TRUE(context_->IsAccessValid(CreateFileSystemURL(". ")));
  EXPECT_TRUE(context_->IsAccessValid(CreateFileSystemURL("b.")));
  EXPECT_TRUE(context_->IsAccessValid(CreateFileSystemURL(".b")));

  // A path that looks like a drive letter.
  EXPECT_TRUE(context_->IsAccessValid(CreateFileSystemURL("c:")));
}

}  // namespace fileapi
