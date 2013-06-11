// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/browser/fileapi/sandbox_mount_point_provider.h"

#include <set>

#include "base/basictypes.h"
#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/message_loop/message_loop_proxy.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/browser/fileapi/file_system_mount_point_provider.h"
#include "webkit/browser/fileapi/file_system_url.h"
#include "webkit/browser/fileapi/mock_file_system_options.h"
#include "webkit/common/fileapi/file_system_util.h"

// PS stands for path separator.
#if defined(FILE_PATH_USES_WIN_SEPARATORS)
#define PS  "\\"
#else
#define PS  "/"
#endif

namespace fileapi {

namespace {

const struct RootPathTest {
  fileapi::FileSystemType type;
  const char* origin_url;
  const char* expected_path;
} kRootPathTestCases[] = {
  { fileapi::kFileSystemTypeTemporary, "http://foo:1/",
    "000" PS "t" },
  { fileapi::kFileSystemTypePersistent, "http://foo:1/",
    "000" PS "p" },
  { fileapi::kFileSystemTypeTemporary, "http://bar.com/",
    "001" PS "t" },
  { fileapi::kFileSystemTypePersistent, "http://bar.com/",
    "001" PS "p" },
  { fileapi::kFileSystemTypeTemporary, "https://foo:2/",
    "002" PS "t" },
  { fileapi::kFileSystemTypePersistent, "https://foo:2/",
    "002" PS "p" },
  { fileapi::kFileSystemTypeTemporary, "https://bar.com/",
    "003" PS "t" },
  { fileapi::kFileSystemTypePersistent, "https://bar.com/",
    "003" PS "p" },
};

const struct RootPathFileURITest {
  fileapi::FileSystemType type;
  const char* origin_url;
  const char* expected_path;
  const char* virtual_path;
} kRootPathFileURITestCases[] = {
  { fileapi::kFileSystemTypeTemporary, "file:///",
    "000" PS "t", NULL },
  { fileapi::kFileSystemTypePersistent, "file:///",
    "000" PS "p", NULL },
};

FileSystemURL CreateFileSystemURL(const char* path) {
  const GURL kOrigin("http://foo/");
  return FileSystemURL::CreateForTest(
      kOrigin, kFileSystemTypeTemporary, base::FilePath::FromUTF8Unsafe(path));
}

void DidOpenFileSystem(base::PlatformFileError* error_out,
                       base::PlatformFileError error) {
  *error_out = error;
}

}  // namespace

class SandboxMountPointProviderTest : public testing::Test {
 protected:
  virtual void SetUp() {
    ASSERT_TRUE(data_dir_.CreateUniqueTempDir());
  }

  void SetUpNewProvider(const FileSystemOptions& options) {
    provider_.reset(
        new SandboxMountPointProvider(NULL,
                                      base::MessageLoopProxy::current().get(),
                                      data_dir_.path(),
                                      options,
                                      NULL));
  }

  SandboxMountPointProvider::OriginEnumerator* CreateOriginEnumerator() const {
    return provider_->CreateOriginEnumerator();
  }

  void CreateOriginTypeDirectory(const GURL& origin,
                                 fileapi::FileSystemType type) {
    base::FilePath target = provider_->
        GetBaseDirectoryForOriginAndType(origin, type, true);
    ASSERT_TRUE(!target.empty());
    ASSERT_TRUE(file_util::DirectoryExists(target));
  }

  bool GetRootPath(const GURL& origin_url,
                   fileapi::FileSystemType type,
                   OpenFileSystemMode mode,
                   base::FilePath* root_path) {
    base::PlatformFileError error = base::PLATFORM_FILE_OK;
    provider_->OpenFileSystem(
        origin_url, type, mode,
        base::Bind(&DidOpenFileSystem, &error));
    base::MessageLoop::current()->RunUntilIdle();
    if (error != base::PLATFORM_FILE_OK)
      return false;
    base::FilePath returned_root_path =
        provider_->GetBaseDirectoryForOriginAndType(
            origin_url, type, false /* create */);
    if (root_path)
      *root_path = returned_root_path;
    return !returned_root_path.empty();
  }

  base::FilePath file_system_path() const {
    return data_dir_.path().Append(
        SandboxMountPointProvider::kFileSystemDirectory);
  }

  base::ScopedTempDir data_dir_;
  base::MessageLoop message_loop_;
  scoped_ptr<SandboxMountPointProvider> provider_;
};

TEST_F(SandboxMountPointProviderTest, Empty) {
  SetUpNewProvider(CreateAllowFileAccessOptions());
  scoped_ptr<SandboxMountPointProvider::OriginEnumerator> enumerator(
      CreateOriginEnumerator());
  ASSERT_TRUE(enumerator->Next().is_empty());
}

TEST_F(SandboxMountPointProviderTest, EnumerateOrigins) {
  SetUpNewProvider(CreateAllowFileAccessOptions());
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
      CreateOriginEnumerator());
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

TEST_F(SandboxMountPointProviderTest, AccessPermissions) {
  SetUpNewProvider(CreateAllowFileAccessOptions());

  // Any access should be allowed in sandbox directory.
  EXPECT_EQ(FILE_PERMISSION_ALWAYS_ALLOW,
            provider_->GetPermissionPolicy(CreateFileSystemURL("foo"),
                                         kReadFilePermissions));
  EXPECT_EQ(FILE_PERMISSION_ALWAYS_ALLOW,
            provider_->GetPermissionPolicy(CreateFileSystemURL("foo"),
                                         kWriteFilePermissions));
  EXPECT_EQ(FILE_PERMISSION_ALWAYS_ALLOW,
            provider_->GetPermissionPolicy(CreateFileSystemURL("foo"),
                                         kCreateFilePermissions));

  // Access to a path with parent references ('..') should be disallowed.
  EXPECT_EQ(FILE_PERMISSION_ALWAYS_DENY,
            provider_->GetPermissionPolicy(CreateFileSystemURL("a/../b"),
                                         kReadFilePermissions));

  // Access from non-allowed scheme should be disallowed.
  EXPECT_EQ(FILE_PERMISSION_ALWAYS_DENY,
            provider_->GetPermissionPolicy(
                FileSystemURL::CreateForTest(
                    GURL("unknown://bar"), kFileSystemTypeTemporary,
                    base::FilePath::FromUTF8Unsafe("foo")),
                kReadFilePermissions));

  // Access for non-sandbox type should be disallowed.
  EXPECT_EQ(FILE_PERMISSION_ALWAYS_DENY,
            provider_->GetPermissionPolicy(
                FileSystemURL::CreateForTest(
                    GURL("http://foo/"), kFileSystemTypeTest,
                    base::FilePath::FromUTF8Unsafe("foo")),
                kReadFilePermissions));

  // Write access to the root folder should be restricted.
  EXPECT_EQ(FILE_PERMISSION_ALWAYS_DENY,
            provider_->GetPermissionPolicy(CreateFileSystemURL(""),
                                           kWriteFilePermissions));
  EXPECT_EQ(FILE_PERMISSION_ALWAYS_DENY,
            provider_->GetPermissionPolicy(CreateFileSystemURL("/"),
                                           kWriteFilePermissions));
  EXPECT_EQ(FILE_PERMISSION_ALWAYS_DENY,
            provider_->GetPermissionPolicy(CreateFileSystemURL("/"),
                                           kCreateFilePermissions));

  // Create access with restricted name should be disallowed.
  EXPECT_EQ(FILE_PERMISSION_ALWAYS_DENY,
            provider_->GetPermissionPolicy(CreateFileSystemURL(".."),
                                           kCreateFilePermissions));
  EXPECT_EQ(FILE_PERMISSION_ALWAYS_DENY,
            provider_->GetPermissionPolicy(CreateFileSystemURL("."),
                                           kCreateFilePermissions));

  // Similar but safe cases.
  EXPECT_EQ(FILE_PERMISSION_ALWAYS_ALLOW,
            provider_->GetPermissionPolicy(CreateFileSystemURL(" ."),
                                           kCreateFilePermissions));
  EXPECT_EQ(FILE_PERMISSION_ALWAYS_ALLOW,
            provider_->GetPermissionPolicy(CreateFileSystemURL(". "),
                                           kCreateFilePermissions));
  EXPECT_EQ(FILE_PERMISSION_ALWAYS_DENY,
            provider_->GetPermissionPolicy(CreateFileSystemURL(" .."),
                                           kCreateFilePermissions));
  EXPECT_EQ(FILE_PERMISSION_ALWAYS_DENY,
            provider_->GetPermissionPolicy(CreateFileSystemURL(".. "),
                                           kCreateFilePermissions));
  EXPECT_EQ(FILE_PERMISSION_ALWAYS_ALLOW,
            provider_->GetPermissionPolicy(CreateFileSystemURL("b."),
                                           kCreateFilePermissions));
  EXPECT_EQ(FILE_PERMISSION_ALWAYS_ALLOW,
            provider_->GetPermissionPolicy(CreateFileSystemURL(".b"),
                                           kCreateFilePermissions));

  // A path that looks like a drive letter.
  EXPECT_EQ(FILE_PERMISSION_ALWAYS_ALLOW,
            provider_->GetPermissionPolicy(CreateFileSystemURL("c:"),
                                           kCreateFilePermissions));
}

TEST_F(SandboxMountPointProviderTest, GetRootPathCreateAndExamine) {
  std::vector<base::FilePath> returned_root_path(
      ARRAYSIZE_UNSAFE(kRootPathTestCases));
  SetUpNewProvider(CreateAllowFileAccessOptions());

  // Create a new root directory.
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kRootPathTestCases); ++i) {
    SCOPED_TRACE(testing::Message() << "RootPath (create) #" << i << " "
                 << kRootPathTestCases[i].expected_path);

    base::FilePath root_path;
    EXPECT_TRUE(GetRootPath(GURL(kRootPathTestCases[i].origin_url),
                            kRootPathTestCases[i].type,
                            OPEN_FILE_SYSTEM_CREATE_IF_NONEXISTENT,
                            &root_path));

    base::FilePath expected = file_system_path().AppendASCII(
        kRootPathTestCases[i].expected_path);
    EXPECT_EQ(expected.value(), root_path.value());
    EXPECT_TRUE(file_util::DirectoryExists(root_path));
    ASSERT_TRUE(returned_root_path.size() > i);
    returned_root_path[i] = root_path;
  }

  // Get the root directory with create=false and see if we get the
  // same directory.
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kRootPathTestCases); ++i) {
    SCOPED_TRACE(testing::Message() << "RootPath (get) #" << i << " "
                 << kRootPathTestCases[i].expected_path);

    base::FilePath root_path;
    EXPECT_TRUE(GetRootPath(GURL(kRootPathTestCases[i].origin_url),
                            kRootPathTestCases[i].type,
                            OPEN_FILE_SYSTEM_FAIL_IF_NONEXISTENT,
                            &root_path));
    ASSERT_TRUE(returned_root_path.size() > i);
    EXPECT_EQ(returned_root_path[i].value(), root_path.value());
  }
}

TEST_F(SandboxMountPointProviderTest,
       GetRootPathCreateAndExamineWithNewProvider) {
  std::vector<base::FilePath> returned_root_path(
      ARRAYSIZE_UNSAFE(kRootPathTestCases));
  SetUpNewProvider(CreateAllowFileAccessOptions());

  GURL origin_url("http://foo.com:1/");

  base::FilePath root_path1;
  EXPECT_TRUE(GetRootPath(origin_url, kFileSystemTypeTemporary,
                          OPEN_FILE_SYSTEM_CREATE_IF_NONEXISTENT,
                          &root_path1));

  SetUpNewProvider(CreateDisallowFileAccessOptions());
  base::FilePath root_path2;
  EXPECT_TRUE(GetRootPath(origin_url, kFileSystemTypeTemporary,
                          OPEN_FILE_SYSTEM_FAIL_IF_NONEXISTENT,
                          &root_path2));

  EXPECT_EQ(root_path1.value(), root_path2.value());
}

TEST_F(SandboxMountPointProviderTest, GetRootPathGetWithoutCreate) {
  SetUpNewProvider(CreateDisallowFileAccessOptions());

  // Try to get a root directory without creating.
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kRootPathTestCases); ++i) {
    SCOPED_TRACE(testing::Message() << "RootPath (create=false) #" << i << " "
                 << kRootPathTestCases[i].expected_path);
    EXPECT_FALSE(GetRootPath(GURL(kRootPathTestCases[i].origin_url),
                             kRootPathTestCases[i].type,
                             OPEN_FILE_SYSTEM_FAIL_IF_NONEXISTENT,
                             NULL));
  }
}

TEST_F(SandboxMountPointProviderTest, GetRootPathInIncognito) {
  SetUpNewProvider(CreateIncognitoFileSystemOptions());

  // Try to get a root directory.
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kRootPathTestCases); ++i) {
    SCOPED_TRACE(testing::Message() << "RootPath (incognito) #" << i << " "
                 << kRootPathTestCases[i].expected_path);
    EXPECT_FALSE(
        GetRootPath(GURL(kRootPathTestCases[i].origin_url),
                    kRootPathTestCases[i].type,
                    OPEN_FILE_SYSTEM_CREATE_IF_NONEXISTENT,
                    NULL));
  }
}

TEST_F(SandboxMountPointProviderTest, GetRootPathFileURI) {
  SetUpNewProvider(CreateDisallowFileAccessOptions());
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kRootPathFileURITestCases); ++i) {
    SCOPED_TRACE(testing::Message() << "RootPathFileURI (disallow) #"
                 << i << " " << kRootPathFileURITestCases[i].expected_path);
    EXPECT_FALSE(
        GetRootPath(GURL(kRootPathFileURITestCases[i].origin_url),
                    kRootPathFileURITestCases[i].type,
                    OPEN_FILE_SYSTEM_CREATE_IF_NONEXISTENT,
                    NULL));
  }
}

TEST_F(SandboxMountPointProviderTest, GetRootPathFileURIWithAllowFlag) {
  SetUpNewProvider(CreateAllowFileAccessOptions());
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kRootPathFileURITestCases); ++i) {
    SCOPED_TRACE(testing::Message() << "RootPathFileURI (allow) #"
                 << i << " " << kRootPathFileURITestCases[i].expected_path);
    base::FilePath root_path;
    EXPECT_TRUE(GetRootPath(GURL(kRootPathFileURITestCases[i].origin_url),
                            kRootPathFileURITestCases[i].type,
                            OPEN_FILE_SYSTEM_CREATE_IF_NONEXISTENT,
                            &root_path));
    base::FilePath expected = file_system_path().AppendASCII(
        kRootPathFileURITestCases[i].expected_path);
    EXPECT_EQ(expected.value(), root_path.value());
    EXPECT_TRUE(file_util::DirectoryExists(root_path));
  }
}

}  // namespace fileapi
