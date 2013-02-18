// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/sandbox_mount_point_provider.h"

#include <set>

#include "base/basictypes.h"
#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/fileapi/file_system_mount_point_provider.h"
#include "webkit/fileapi/file_system_url.h"
#include "webkit/fileapi/file_system_util.h"
#include "webkit/fileapi/mock_file_system_options.h"

namespace fileapi {

namespace {

FileSystemURL CreateFileSystemURL(const char* path) {
  const GURL kOrigin("http://foo/");
  return FileSystemURL::CreateForTest(
      kOrigin, kFileSystemTypeTemporary, base::FilePath::FromUTF8Unsafe(path));
}

}  // namespace

class SandboxMountPointProviderOriginEnumeratorTest : public testing::Test {
 public:
  virtual void SetUp() {
    ASSERT_TRUE(data_dir_.CreateUniqueTempDir());
    sandbox_provider_.reset(
        new SandboxMountPointProvider(
            NULL,
            base::MessageLoopProxy::current(),
            data_dir_.path(),
            CreateAllowFileAccessOptions()));
  }

  SandboxMountPointProvider::OriginEnumerator* CreateEnumerator() const {
    return sandbox_provider_->CreateOriginEnumerator();
  }

 protected:
  void CreateOriginTypeDirectory(const GURL& origin,
                                 fileapi::FileSystemType type) {
    base::FilePath target = sandbox_provider_->
        GetBaseDirectoryForOriginAndType(origin, type, true);
    ASSERT_TRUE(!target.empty());
    ASSERT_TRUE(file_util::DirectoryExists(target));
  }

  base::ScopedTempDir data_dir_;
  MessageLoop message_loop_;
  scoped_ptr<SandboxMountPointProvider> sandbox_provider_;
};

TEST_F(SandboxMountPointProviderOriginEnumeratorTest, Empty) {
  scoped_ptr<SandboxMountPointProvider::OriginEnumerator> enumerator(
      CreateEnumerator());
  ASSERT_TRUE(enumerator->Next().is_empty());
}

TEST_F(SandboxMountPointProviderOriginEnumeratorTest, EnumerateOrigins) {
  const char* temporary_origins[] = {
    "http://www.bar.com/",
    "http://www.foo.com/",
    "http://www.foo.com:1/",
    "http://www.example.com:8080/",
    "http://www.google.com:80/",
  };
  const char* persistent_origins[] = {
    "http://www.bar.com/",
    "http://www.foo.com:8080/",
    "http://www.foo.com:80/",
  };
  size_t temporary_size = ARRAYSIZE_UNSAFE(temporary_origins);
  size_t persistent_size = ARRAYSIZE_UNSAFE(persistent_origins);
  std::set<GURL> temporary_set, persistent_set;
  for (size_t i = 0; i < temporary_size; ++i) {
    CreateOriginTypeDirectory(GURL(temporary_origins[i]),
        fileapi::kFileSystemTypeTemporary);
    temporary_set.insert(GURL(temporary_origins[i]));
  }
  for (size_t i = 0; i < persistent_size; ++i) {
    CreateOriginTypeDirectory(GURL(persistent_origins[i]),
        kFileSystemTypePersistent);
    persistent_set.insert(GURL(persistent_origins[i]));
  }

  scoped_ptr<SandboxMountPointProvider::OriginEnumerator> enumerator(
      CreateEnumerator());
  size_t temporary_actual_size = 0;
  size_t persistent_actual_size = 0;
  GURL current;
  while (!(current = enumerator->Next()).is_empty()) {
    SCOPED_TRACE(testing::Message() << "EnumerateOrigin " << current.spec());
    if (enumerator->HasFileSystemType(kFileSystemTypeTemporary)) {
      ASSERT_TRUE(temporary_set.find(current) != temporary_set.end());
      ++temporary_actual_size;
    }
    if (enumerator->HasFileSystemType(kFileSystemTypePersistent)) {
      ASSERT_TRUE(persistent_set.find(current) != persistent_set.end());
      ++persistent_actual_size;
    }
  }

  EXPECT_EQ(temporary_size, temporary_actual_size);
  EXPECT_EQ(persistent_size, persistent_actual_size);
}

TEST(SandboxMountPointProviderTest, AccessPermissions) {
  MessageLoop message_loop_;
  SandboxMountPointProvider provider(
      NULL, base::MessageLoopProxy::current(), base::FilePath(),
      CreateAllowFileAccessOptions());

  // Any access should be allowed in sandbox directory.
  EXPECT_EQ(FILE_PERMISSION_ALWAYS_ALLOW,
            provider.GetPermissionPolicy(CreateFileSystemURL("foo"),
                                         kReadFilePermissions));
  EXPECT_EQ(FILE_PERMISSION_ALWAYS_ALLOW,
            provider.GetPermissionPolicy(CreateFileSystemURL("foo"),
                                         kWriteFilePermissions));
  EXPECT_EQ(FILE_PERMISSION_ALWAYS_ALLOW,
            provider.GetPermissionPolicy(CreateFileSystemURL("foo"),
                                         kCreateFilePermissions));

  // Access to a path with parent references ('..') should be disallowed.
  EXPECT_EQ(FILE_PERMISSION_ALWAYS_DENY,
            provider.GetPermissionPolicy(CreateFileSystemURL("a/../b"),
                                         kReadFilePermissions));

  // Access from non-allowed scheme should be disallowed.
  EXPECT_EQ(FILE_PERMISSION_ALWAYS_DENY,
            provider.GetPermissionPolicy(
                FileSystemURL::CreateForTest(
                    GURL("unknown://bar"), kFileSystemTypeTemporary,
                    base::FilePath::FromUTF8Unsafe("foo")),
                kReadFilePermissions));

  // Access for non-sandbox type should be disallowed.
  EXPECT_EQ(FILE_PERMISSION_ALWAYS_DENY,
            provider.GetPermissionPolicy(
                FileSystemURL::CreateForTest(
                    GURL("http://foo/"), kFileSystemTypeTest,
                    base::FilePath::FromUTF8Unsafe("foo")),
                kReadFilePermissions));

  // Write access to the root folder should be restricted.
  EXPECT_EQ(FILE_PERMISSION_ALWAYS_DENY,
            provider.GetPermissionPolicy(CreateFileSystemURL(""),
                                         kWriteFilePermissions));
  EXPECT_EQ(FILE_PERMISSION_ALWAYS_DENY,
            provider.GetPermissionPolicy(CreateFileSystemURL("/"),
                                         kWriteFilePermissions));
  EXPECT_EQ(FILE_PERMISSION_ALWAYS_DENY,
            provider.GetPermissionPolicy(CreateFileSystemURL("/"),
                                         kCreateFilePermissions));

  // Create access with restricted name should be disallowed.
  EXPECT_EQ(FILE_PERMISSION_ALWAYS_DENY,
            provider.GetPermissionPolicy(CreateFileSystemURL(".."),
                                         kCreateFilePermissions));
  EXPECT_EQ(FILE_PERMISSION_ALWAYS_DENY,
            provider.GetPermissionPolicy(CreateFileSystemURL("."),
                                         kCreateFilePermissions));

  // Similar but safe cases.
  EXPECT_EQ(FILE_PERMISSION_ALWAYS_ALLOW,
            provider.GetPermissionPolicy(CreateFileSystemURL(" ."),
                                         kCreateFilePermissions));
  EXPECT_EQ(FILE_PERMISSION_ALWAYS_ALLOW,
            provider.GetPermissionPolicy(CreateFileSystemURL(". "),
                                         kCreateFilePermissions));
  EXPECT_EQ(FILE_PERMISSION_ALWAYS_ALLOW,
            provider.GetPermissionPolicy(CreateFileSystemURL(" .."),
                                         kCreateFilePermissions));
  EXPECT_EQ(FILE_PERMISSION_ALWAYS_ALLOW,
            provider.GetPermissionPolicy(CreateFileSystemURL(".. "),
                                         kCreateFilePermissions));
  EXPECT_EQ(FILE_PERMISSION_ALWAYS_ALLOW,
            provider.GetPermissionPolicy(CreateFileSystemURL("b."),
                                         kCreateFilePermissions));
  EXPECT_EQ(FILE_PERMISSION_ALWAYS_ALLOW,
            provider.GetPermissionPolicy(CreateFileSystemURL(".b"),
                                         kCreateFilePermissions));

  // A path that looks like a drive letter.
  EXPECT_EQ(FILE_PERMISSION_ALWAYS_ALLOW,
            provider.GetPermissionPolicy(CreateFileSystemURL("c:"),
                                         kCreateFilePermissions));
}

}  // namespace fileapi
