// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/file_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "base/scoped_temp_dir.h"
#include "base/sys_string_conversions.h"
#include "base/utf_string_conversions.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/fileapi/file_system_context.h"
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
    "chrome-extension__0" PS "External" },
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
#if defined(OS_CHROMEOS)
  { fileapi::kFileSystemTypeExternal, "chrome-extension://foo/",
    "chrome-extension__0" PS "External", "testing" },
#endif
};

const struct CheckValidPathTest {
  FilePath::StringType path;
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

const char* const kPathToVirtualPathTestCases[] = {
  "",
  "a",
  "a" PS "b",
  "a" PS "b" PS "c",
};

const struct IsRestrictedNameTest {
  FilePath::StringType name;
  bool expected_dangerous;
} kIsRestrictedNameTestCases[] = {

  // Names that contain strings that used to be restricted, but are now allowed.
  { FILE_PATH_LITERAL("con"), false, },
  { FILE_PATH_LITERAL("Con.txt"), false, },
  { FILE_PATH_LITERAL("Prn.png"), false, },
  { FILE_PATH_LITERAL("AUX"), false, },
  { FILE_PATH_LITERAL("nUl."), false, },
  { FILE_PATH_LITERAL("coM1"), false, },
  { FILE_PATH_LITERAL("COM3.com"), false, },
  { FILE_PATH_LITERAL("cOM7"), false, },
  { FILE_PATH_LITERAL("com9"), false, },
  { FILE_PATH_LITERAL("lpT1"), false, },
  { FILE_PATH_LITERAL("LPT4.com"), false, },
  { FILE_PATH_LITERAL("lPT8"), false, },
  { FILE_PATH_LITERAL("lPT9"), false, },
  { FILE_PATH_LITERAL("com1."), false, },

  // Similar cases that have always been allowed.
  { FILE_PATH_LITERAL("con3"), false, },
  { FILE_PATH_LITERAL("PrnImage.png"), false, },
  { FILE_PATH_LITERAL("AUXX"), false, },
  { FILE_PATH_LITERAL("NULL"), false, },
  { FILE_PATH_LITERAL("coM0"), false, },
  { FILE_PATH_LITERAL("COM.com"), false, },
  { FILE_PATH_LITERAL("lpT0"), false, },
  { FILE_PATH_LITERAL("LPT.com"), false, },

  // Ends with period or whitespace--used to be banned, now OK.
  { FILE_PATH_LITERAL("b "), false, },
  { FILE_PATH_LITERAL("b\t"), false, },
  { FILE_PATH_LITERAL("b\n"), false, },
  { FILE_PATH_LITERAL("b\r\n"), false, },
  { FILE_PATH_LITERAL("b."), false, },
  { FILE_PATH_LITERAL("b.."), false, },

  // Similar cases that have always been allowed.
  { FILE_PATH_LITERAL("b c"), false, },
  { FILE_PATH_LITERAL("b\tc"), false, },
  { FILE_PATH_LITERAL("b\nc"), false, },
  { FILE_PATH_LITERAL("b\r\nc"), false, },
  { FILE_PATH_LITERAL("b c d e f"), false, },
  { FILE_PATH_LITERAL("b.c"), false, },
  { FILE_PATH_LITERAL("b..c"), false, },

  // Name that has restricted chars in it.
  { FILE_PATH_LITERAL("\\"), true, },
  { FILE_PATH_LITERAL("/"), true, },
  { FILE_PATH_LITERAL("a\\b"), true, },
  { FILE_PATH_LITERAL("a/b"), true, },
  { FILE_PATH_LITERAL("ab\\"), true, },
  { FILE_PATH_LITERAL("ab/"), true, },
  { FILE_PATH_LITERAL("\\ab"), true, },
  { FILE_PATH_LITERAL("/ab"), true, },
  { FILE_PATH_LITERAL("ab/.txt"), true, },
  { FILE_PATH_LITERAL("ab\\.txt"), true, },

  // Names that contain chars that were formerly restricted, now OK.
  { FILE_PATH_LITERAL("a<b"), false, },
  { FILE_PATH_LITERAL("a>b"), false, },
  { FILE_PATH_LITERAL("a:b"), false, },
  { FILE_PATH_LITERAL("a?b"), false, },
  { FILE_PATH_LITERAL("a|b"), false, },
  { FILE_PATH_LITERAL("ab<.txt"), false, },
  { FILE_PATH_LITERAL("ab>.txt"), false, },
  { FILE_PATH_LITERAL("ab:.txt"), false, },
  { FILE_PATH_LITERAL("ab?.txt"), false, },
  { FILE_PATH_LITERAL("ab|.txt"), false, },
  { FILE_PATH_LITERAL("<ab"), false, },
  { FILE_PATH_LITERAL(">ab"), false, },
  { FILE_PATH_LITERAL(":ab"), false, },
  { FILE_PATH_LITERAL("?ab"), false, },
  { FILE_PATH_LITERAL("|ab"), false, },

  // Names that are restricted still.
  { FILE_PATH_LITERAL(".."), true, },
  { FILE_PATH_LITERAL("."), true, },

  // Similar but safe cases.
  { FILE_PATH_LITERAL(" ."), false, },
  { FILE_PATH_LITERAL(". "), false, },
  { FILE_PATH_LITERAL(" . "), false, },
  { FILE_PATH_LITERAL(" .."), false, },
  { FILE_PATH_LITERAL(".. "), false, },
  { FILE_PATH_LITERAL(" .. "), false, },
  { FILE_PATH_LITERAL("b."), false, },
  { FILE_PATH_LITERAL(".b"), false, },
};

FileSystemOptions CreateRootPathTestOptions() {
  std::vector<std::string> additional_allowed_schemes;
  additional_allowed_schemes.push_back("file");
#if defined(OS_CHROMEOS)
  additional_allowed_schemes.push_back("chrome-extension");
#endif
  return FileSystemOptions(FileSystemOptions::PROFILE_MODE_NORMAL,
                           additional_allowed_schemes);
}

}  // namespace

class FileSystemMountPointProviderTest : public testing::Test {
 public:
  FileSystemMountPointProviderTest()
      : ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
  }

  void SetUp() {
    ASSERT_TRUE(data_dir_.CreateUniqueTempDir());
    root_path_callback_status_ = false;
    root_path_.clear();
    file_system_name_.clear();
    special_storage_policy_ = new quota::MockSpecialStoragePolicy;
  }

 protected:
  void SetupNewContext(const FileSystemOptions& options) {
    file_system_context_ = new FileSystemContext(
        base::MessageLoopProxy::current(),
        base::MessageLoopProxy::current(),
        special_storage_policy_,
        NULL,
        data_dir_.path(),
        options);
#if defined(OS_CHROMEOS)
    fileapi::ExternalFileSystemMountPointProvider* ext_provider =
        file_system_context_->external_provider();
    ext_provider->AddMountPoint(FilePath("/tmp/testing"));
#endif
  }

  FileSystemMountPointProvider* provider(FileSystemType type) {
    DCHECK(file_system_context_);
    return file_system_context_->GetMountPointProvider(type);
  }

  void OnGetRootPath(bool success,
                     const FilePath& root_path,
                     const std::string& name) {
    root_path_callback_status_ = success;
    root_path_ = root_path;
    file_system_name_ = name;
  }

  bool GetRootPath(const GURL& origin_url,
                   fileapi::FileSystemType type,
                   bool create,
                   FilePath* root_path) {
    provider(type)->ValidateFileSystemRootAndGetURL(
        origin_url, type, create,
        base::Bind(&FileSystemMountPointProviderTest::OnGetRootPath,
                   weak_factory_.GetWeakPtr()));
    MessageLoop::current()->RunAllPending();
    if (root_path)
      *root_path = root_path_;
    return root_path_callback_status_;
  }

  FilePath data_path() { return data_dir_.path(); }
  FilePath file_system_path() {
    return data_dir_.path().Append(
        SandboxMountPointProvider::kNewFileSystemDirectory);
  }
  FilePath external_file_system_path() {
    return FilePath::FromUTF8Unsafe(fileapi::kExternalDir);
  }
  FilePath external_file_path_root() {
    return FilePath::FromUTF8Unsafe("/tmp");
  }

 private:
  ScopedTempDir data_dir_;
  base::WeakPtrFactory<FileSystemMountPointProviderTest> weak_factory_;

  bool root_path_callback_status_;
  FilePath root_path_;
  std::string file_system_name_;

  scoped_refptr<quota::SpecialStoragePolicy> special_storage_policy_;
  scoped_refptr<FileSystemContext> file_system_context_;

  DISALLOW_COPY_AND_ASSIGN(FileSystemMountPointProviderTest);
};

TEST_F(FileSystemMountPointProviderTest, GetRootPathCreateAndExamine) {
  std::vector<FilePath> returned_root_path(
      ARRAYSIZE_UNSAFE(kRootPathTestCases));
  SetupNewContext(CreateRootPathTestOptions());

  // Create a new root directory.
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kRootPathTestCases); ++i) {
    SCOPED_TRACE(testing::Message() << "RootPath (create) #" << i << " "
                 << kRootPathTestCases[i].expected_path);

    FilePath root_path;
    EXPECT_TRUE(GetRootPath(GURL(kRootPathTestCases[i].origin_url),
                            kRootPathTestCases[i].type,
                            true /* create */, &root_path));

    if (kRootPathTestCases[i].type != fileapi::kFileSystemTypeExternal) {
      FilePath expected = file_system_path().AppendASCII(
          kRootPathTestCases[i].expected_path);
      EXPECT_EQ(expected.value(), root_path.value());
      EXPECT_TRUE(file_util::DirectoryExists(root_path));
    } else {
      // External file system root path is virtual one and does not match
      // anything from the actual file system.
      EXPECT_EQ(external_file_system_path().value(),
                root_path.value());
    }
    ASSERT_TRUE(returned_root_path.size() > i);
    returned_root_path[i] = root_path;
  }

  // Get the root directory with create=false and see if we get the
  // same directory.
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kRootPathTestCases); ++i) {
    SCOPED_TRACE(testing::Message() << "RootPath (get) #" << i << " "
                 << kRootPathTestCases[i].expected_path);

    FilePath root_path;
    EXPECT_TRUE(GetRootPath(GURL(kRootPathTestCases[i].origin_url),
                            kRootPathTestCases[i].type,
                            false /* create */, &root_path));
    ASSERT_TRUE(returned_root_path.size() > i);
    EXPECT_EQ(returned_root_path[i].value(), root_path.value());
  }
}

TEST_F(FileSystemMountPointProviderTest,
       GetRootPathCreateAndExamineWithNewProvider) {
  std::vector<FilePath> returned_root_path(
      ARRAYSIZE_UNSAFE(kRootPathTestCases));
  SetupNewContext(CreateAllowFileAccessOptions());

  GURL origin_url("http://foo.com:1/");

  FilePath root_path1;
  EXPECT_TRUE(GetRootPath(origin_url,
                          kFileSystemTypeTemporary, true, &root_path1));

  SetupNewContext(CreateDisallowFileAccessOptions());
  FilePath root_path2;
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
    EXPECT_FALSE(GetRootPath(GURL(kRootPathTestCases[i].origin_url),
                             kRootPathTestCases[i].type,
                             false /* create */, NULL));
  }
}

TEST_F(FileSystemMountPointProviderTest, GetRootPathInIncognito) {
  SetupNewContext(CreateIncognitoFileSystemOptions());

  // Try to get a root directory.
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kRootPathTestCases); ++i) {
    SCOPED_TRACE(testing::Message() << "RootPath (incognito) #" << i << " "
                 << kRootPathTestCases[i].expected_path);
    EXPECT_FALSE(GetRootPath(GURL(kRootPathTestCases[i].origin_url),
                             kRootPathTestCases[i].type,
                             true /* create */, NULL));
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
  SetupNewContext(CreateRootPathTestOptions());
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kRootPathFileURITestCases); ++i) {
    SCOPED_TRACE(testing::Message() << "RootPathFileURI (allow) #"
                 << i << " " << kRootPathFileURITestCases[i].expected_path);
    FilePath root_path;
    EXPECT_TRUE(GetRootPath(GURL(kRootPathFileURITestCases[i].origin_url),
                            kRootPathFileURITestCases[i].type,
                            true /* create */, &root_path));
    if (kRootPathFileURITestCases[i].type != fileapi::kFileSystemTypeExternal) {
      FilePath expected = file_system_path().AppendASCII(
          kRootPathFileURITestCases[i].expected_path);
      EXPECT_EQ(expected.value(), root_path.value());
      EXPECT_TRUE(file_util::DirectoryExists(root_path));
    } else {
      EXPECT_EQ(external_file_path_root().value(), root_path.value());
    }
  }
}

TEST_F(FileSystemMountPointProviderTest, IsRestrictedName) {
  SetupNewContext(CreateDisallowFileAccessOptions());
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kIsRestrictedNameTestCases); ++i) {
    SCOPED_TRACE(testing::Message() << "IsRestrictedName #" << i << " "
                 << kIsRestrictedNameTestCases[i].name);
    FilePath name(kIsRestrictedNameTestCases[i].name);
    EXPECT_EQ(kIsRestrictedNameTestCases[i].expected_dangerous,
              provider(kFileSystemTypeTemporary)->IsRestrictedFileName(name));
  }
}

}  // namespace fileapi
