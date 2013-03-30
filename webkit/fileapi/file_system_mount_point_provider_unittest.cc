// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "base/strings/sys_string_conversions.h"
#include "base/utf_string_conversions.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/fileapi/external_mount_points.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_task_runners.h"
#include "webkit/fileapi/file_system_url.h"
#include "webkit/fileapi/file_system_util.h"
#include "webkit/fileapi/mock_file_system_options.h"
#include "webkit/fileapi/sandbox_mount_point_provider.h"
#include "webkit/quota/mock_special_storage_policy.h"

#if defined(OS_CHROMEOS)
#include "webkit/chromeos/fileapi/cros_mount_point_provider.h"
#endif

namespace fileapi {

namespace {

// PS stands for path separator.
#if defined(FILE_PATH_USES_WIN_SEPARATORS)
#define PS  "\\"
#else
#define PS  "/"
#endif

struct RootPathTestCase {
  fileapi::FileSystemType type;
  const char* origin_url;
  const char* expected_path;
};

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
#if defined(OS_CHROMEOS)
  { fileapi::kFileSystemTypeExternal, "chrome-extension://foo/",
    "chrome-extension__0" /* unused, only for logging */ },
#endif
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

const struct CheckValidPathTest {
  base::FilePath::StringType path;
  bool expected_valid;
} kCheckValidPathTestCases[] = {
  { FILE_PATH_LITERAL("//tmp/foo.txt"), false, },
  { FILE_PATH_LITERAL("//etc/hosts"), false, },
  { FILE_PATH_LITERAL("foo.txt"), true, },
  { FILE_PATH_LITERAL("a/b/c"), true, },
  // Any paths that includes parent references are considered invalid.
  { FILE_PATH_LITERAL(".."), false, },
  { FILE_PATH_LITERAL("tmp/.."), false, },
  { FILE_PATH_LITERAL("a/b/../c/.."), false, },
};

// For External filesystem.
const base::FilePath::CharType kMountPoint[] = FILE_PATH_LITERAL("/tmp/testing");
const base::FilePath::CharType kRootPath[] = FILE_PATH_LITERAL("/tmp");
const base::FilePath::CharType kVirtualPath[] = FILE_PATH_LITERAL("testing");

}  // namespace

class FileSystemMountPointProviderTest : public testing::Test {
 public:
  FileSystemMountPointProviderTest()
      : ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
  }

  virtual void SetUp() {
    ASSERT_TRUE(data_dir_.CreateUniqueTempDir());
    special_storage_policy_ = new quota::MockSpecialStoragePolicy;
  }

 protected:
  void SetupNewContext(const FileSystemOptions& options) {
    scoped_refptr<ExternalMountPoints> external_mount_points(
        ExternalMountPoints::CreateRefCounted());
    file_system_context_ = new FileSystemContext(
        FileSystemTaskRunners::CreateMockTaskRunners(),
        external_mount_points.get(),
        special_storage_policy_,
        NULL,
        data_dir_.path(),
        options);
#if defined(OS_CHROMEOS)
    base::FilePath mount_point_path = base::FilePath(kMountPoint);
    external_mount_points->RegisterFileSystem(
        mount_point_path.BaseName().AsUTF8Unsafe(),
        kFileSystemTypeNativeLocal,
        mount_point_path);
#endif
  }

  FileSystemMountPointProvider* provider(FileSystemType type) {
    DCHECK(file_system_context_);
    return file_system_context_->GetMountPointProvider(type);
  }

  bool GetRootPath(const GURL& origin_url,
                   fileapi::FileSystemType type,
                   bool create,
                   base::FilePath* root_path) {
    base::FilePath virtual_path = base::FilePath();
    if (type == kFileSystemTypeExternal)
      virtual_path = base::FilePath(kVirtualPath);
    FileSystemURL url = file_system_context_->CreateCrackedFileSystemURL(
        origin_url, type, virtual_path);
    base::FilePath returned_root_path =
        provider(type)->GetFileSystemRootPathOnFileThread(url, create);
    if (root_path)
      *root_path = returned_root_path;
    return !returned_root_path.empty();
  }

  base::FilePath data_path() const { return data_dir_.path(); }
  base::FilePath file_system_path() const {
    return data_dir_.path().Append(
        SandboxMountPointProvider::kFileSystemDirectory);
  }
  FileSystemContext* file_system_context() const {
    return file_system_context_.get();
  }

 private:
  base::ScopedTempDir data_dir_;
  MessageLoop message_loop_;
  base::WeakPtrFactory<FileSystemMountPointProviderTest> weak_factory_;

  scoped_refptr<quota::SpecialStoragePolicy> special_storage_policy_;
  scoped_refptr<FileSystemContext> file_system_context_;

  DISALLOW_COPY_AND_ASSIGN(FileSystemMountPointProviderTest);
};

TEST_F(FileSystemMountPointProviderTest, GetRootPathCreateAndExamine) {
  std::vector<base::FilePath> returned_root_path(
      ARRAYSIZE_UNSAFE(kRootPathTestCases));
  SetupNewContext(CreateAllowFileAccessOptions());

  // Create a new root directory.
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kRootPathTestCases); ++i) {
    SCOPED_TRACE(testing::Message() << "RootPath (create) #" << i << " "
                 << kRootPathTestCases[i].expected_path);

    base::FilePath root_path;
    EXPECT_TRUE(GetRootPath(GURL(kRootPathTestCases[i].origin_url),
                            kRootPathTestCases[i].type,
                            true /* create */, &root_path));

    if (kRootPathTestCases[i].type != kFileSystemTypeExternal) {
      base::FilePath expected = file_system_path().AppendASCII(
          kRootPathTestCases[i].expected_path);
      EXPECT_EQ(expected.value(), root_path.value());
      EXPECT_TRUE(file_util::DirectoryExists(root_path));
    } else {
      // External file system root path is virtual one and does not match
      // anything from the actual file system.
      EXPECT_EQ(kRootPath, root_path.value());
    }
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
                            false /* create */, &root_path));
    ASSERT_TRUE(returned_root_path.size() > i);
    EXPECT_EQ(returned_root_path[i].value(), root_path.value());
  }
}

TEST_F(FileSystemMountPointProviderTest,
       GetRootPathCreateAndExamineWithNewProvider) {
  std::vector<base::FilePath> returned_root_path(
      ARRAYSIZE_UNSAFE(kRootPathTestCases));
  SetupNewContext(CreateAllowFileAccessOptions());

  GURL origin_url("http://foo.com:1/");

  base::FilePath root_path1;
  EXPECT_TRUE(GetRootPath(origin_url,
                          kFileSystemTypeTemporary, true, &root_path1));

  SetupNewContext(CreateDisallowFileAccessOptions());
  base::FilePath root_path2;
  EXPECT_TRUE(GetRootPath(origin_url,
                          kFileSystemTypeTemporary, false, &root_path2));

  EXPECT_EQ(root_path1.value(), root_path2.value());
}

TEST_F(FileSystemMountPointProviderTest, GetRootPathGetWithoutCreate) {
  SetupNewContext(CreateDisallowFileAccessOptions());

  // Try to get a root directory without creating.
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kRootPathTestCases); ++i) {
    SCOPED_TRACE(testing::Message() << "RootPath (create=false) #" << i << " "
                 << kRootPathTestCases[i].expected_path);
    // External type does not check the directory existence.
    if (kRootPathTestCases[i].type != kFileSystemTypeExternal) {
      EXPECT_FALSE(GetRootPath(GURL(kRootPathTestCases[i].origin_url),
                              kRootPathTestCases[i].type,
                              false /* create */, NULL));
    }
  }
}

TEST_F(FileSystemMountPointProviderTest, GetRootPathInIncognito) {
  SetupNewContext(CreateIncognitoFileSystemOptions());

  // Try to get a root directory.
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kRootPathTestCases); ++i) {
    SCOPED_TRACE(testing::Message() << "RootPath (incognito) #" << i << " "
                 << kRootPathTestCases[i].expected_path);
    // External type does not change the behavior in incognito mode.
    if (kRootPathTestCases[i].type != kFileSystemTypeExternal) {
      EXPECT_FALSE(GetRootPath(GURL(kRootPathTestCases[i].origin_url),
                              kRootPathTestCases[i].type,
                              true /* create */, NULL));
    }
  }
}

TEST_F(FileSystemMountPointProviderTest, GetRootPathFileURI) {
  SetupNewContext(CreateDisallowFileAccessOptions());
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kRootPathFileURITestCases); ++i) {
    SCOPED_TRACE(testing::Message() << "RootPathFileURI (disallow) #"
                 << i << " " << kRootPathFileURITestCases[i].expected_path);
    EXPECT_FALSE(GetRootPath(GURL(kRootPathFileURITestCases[i].origin_url),
                             kRootPathFileURITestCases[i].type,
                             true /* create */, NULL));
  }
}

TEST_F(FileSystemMountPointProviderTest, GetRootPathFileURIWithAllowFlag) {
  SetupNewContext(CreateAllowFileAccessOptions());
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kRootPathFileURITestCases); ++i) {
    SCOPED_TRACE(testing::Message() << "RootPathFileURI (allow) #"
                 << i << " " << kRootPathFileURITestCases[i].expected_path);
    base::FilePath root_path;
    EXPECT_TRUE(GetRootPath(GURL(kRootPathFileURITestCases[i].origin_url),
                            kRootPathFileURITestCases[i].type,
                            true /* create */, &root_path));
    base::FilePath expected = file_system_path().AppendASCII(
        kRootPathFileURITestCases[i].expected_path);
    EXPECT_EQ(expected.value(), root_path.value());
    EXPECT_TRUE(file_util::DirectoryExists(root_path));
  }
}

}  // namespace fileapi
