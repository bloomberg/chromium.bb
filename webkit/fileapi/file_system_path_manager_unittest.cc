// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/file_system_path_manager.h"

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/scoped_ptr.h"
#include "base/scoped_temp_dir.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"

using fileapi::FileSystemPathManager;

namespace {

// PS stands for path separator.
#if defined(FILE_PATH_USES_WIN_SEPARATORS)
#define PS  "\\"
#else
#define PS  "/"
#endif

const FilePath::CharType kTestDataPath[] = FILE_PATH_LITERAL(
        "//tmp/TestingProfilePath");

const struct RootPathTest {
  fileapi::FileSystemType type;
  bool off_the_record;
  const char* origin_url;
  bool expect_root_path;
  const char* expected_path;
} kRootPathTestCases[] = {
  { fileapi::kFileSystemTypeTemporary, false, "http://host:1/",
    true, "FileSystem" PS "http_host_1" PS "Temporary" },
  { fileapi::kFileSystemTypePersistent,  false, "http://host:2/",
    true, "FileSystem" PS "http_host_2" PS "Persistent" },
  { fileapi::kFileSystemTypeTemporary, true,  "http://host:3/",
    false, "" },
  { fileapi::kFileSystemTypePersistent,  true,  "http://host:4/",
    false, "" },
  // We disallow file:// URIs to access filesystem.
  { fileapi::kFileSystemTypeTemporary, false, "file:///some/path", false, "" },
  { fileapi::kFileSystemTypePersistent, false, "file:///some/path", false, "" },
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

const struct IsRestrictedNameTest {
  FilePath::StringType name;
  bool expected_dangerous;
} kIsRestrictedNameTestCases[] = {
  // Name that has restricted names in it.
  { FILE_PATH_LITERAL("con"), true, },
  { FILE_PATH_LITERAL("Con.txt"), true, },
  { FILE_PATH_LITERAL("Prn.png"), true, },
  { FILE_PATH_LITERAL("AUX"), true, },
  { FILE_PATH_LITERAL("nUl."), true, },
  { FILE_PATH_LITERAL("coM1"), true, },
  { FILE_PATH_LITERAL("COM3.com"), true, },
  { FILE_PATH_LITERAL("cOM7"), true, },
  { FILE_PATH_LITERAL("com9"), true, },
  { FILE_PATH_LITERAL("lpT1"), true, },
  { FILE_PATH_LITERAL("LPT4.com"), true, },
  { FILE_PATH_LITERAL("lPT8"), true, },
  { FILE_PATH_LITERAL("lPT9"), true, },
  // Similar but safe cases.
  { FILE_PATH_LITERAL("con3"), false, },
  { FILE_PATH_LITERAL("PrnImage.png"), false, },
  { FILE_PATH_LITERAL("AUXX"), false, },
  { FILE_PATH_LITERAL("NULL"), false, },
  { FILE_PATH_LITERAL("coM0"), false, },
  { FILE_PATH_LITERAL("COM.com"), false, },
  { FILE_PATH_LITERAL("lpT0"), false, },
  { FILE_PATH_LITERAL("LPT.com"), false, },

  // Ends with period or whitespace.
  { FILE_PATH_LITERAL("b "), true, },
  { FILE_PATH_LITERAL("b\t"), true, },
  { FILE_PATH_LITERAL("b\n"), true, },
  { FILE_PATH_LITERAL("b\r\n"), true, },
  { FILE_PATH_LITERAL("b."), true, },
  { FILE_PATH_LITERAL("b.."), true, },
  // Similar but safe cases.
  { FILE_PATH_LITERAL("b c"), false, },
  { FILE_PATH_LITERAL("b\tc"), false, },
  { FILE_PATH_LITERAL("b\nc"), false, },
  { FILE_PATH_LITERAL("b\r\nc"), false, },
  { FILE_PATH_LITERAL("b c d e f"), false, },
  { FILE_PATH_LITERAL("b.c"), false, },
  { FILE_PATH_LITERAL("b..c"), false, },

  // Name that has restricted chars in it.
  { FILE_PATH_LITERAL("a\\b"), true, },
  { FILE_PATH_LITERAL("a/b"), true, },
  { FILE_PATH_LITERAL("a<b"), true, },
  { FILE_PATH_LITERAL("a>b"), true, },
  { FILE_PATH_LITERAL("a:b"), true, },
  { FILE_PATH_LITERAL("a?b"), true, },
  { FILE_PATH_LITERAL("a|b"), true, },
  { FILE_PATH_LITERAL("ab\\"), true, },
  { FILE_PATH_LITERAL("ab/.txt"), true, },
  { FILE_PATH_LITERAL("ab<.txt"), true, },
  { FILE_PATH_LITERAL("ab>.txt"), true, },
  { FILE_PATH_LITERAL("ab:.txt"), true, },
  { FILE_PATH_LITERAL("ab?.txt"), true, },
  { FILE_PATH_LITERAL("ab|.txt"), true, },
  { FILE_PATH_LITERAL("\\ab"), true, },
  { FILE_PATH_LITERAL("/ab"), true, },
  { FILE_PATH_LITERAL("<ab"), true, },
  { FILE_PATH_LITERAL(">ab"), true, },
  { FILE_PATH_LITERAL(":ab"), true, },
  { FILE_PATH_LITERAL("?ab"), true, },
  { FILE_PATH_LITERAL("|ab"), true, },
};

}  // namespace

class FileSystemPathManagerTest : public testing::Test {
 public:
  FileSystemPathManagerTest()
      : data_path_(kTestDataPath) {
  }

  FileSystemPathManager* GetNewPathManager(bool incognito) {
    path_manager_.reset(new FileSystemPathManager(
        data_path_, incognito, false /* allow_file_access */));
    return path_manager_.get();
  }

 private:
  FilePath data_path_;
  scoped_ptr<FileSystemPathManager> path_manager_;
};

TEST_F(FileSystemPathManagerTest, GetRootPath) {
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kRootPathTestCases); ++i) {
    SCOPED_TRACE(testing::Message() << "RootPath #" << i << " "
                 << kRootPathTestCases[i].expected_path);

    FileSystemPathManager* manager = GetNewPathManager(
        kRootPathTestCases[i].off_the_record);

    FilePath root_path;
    bool result = manager->GetFileSystemRootPath(
            GURL(kRootPathTestCases[i].origin_url),
            kRootPathTestCases[i].type,
            &root_path, NULL);
    EXPECT_EQ(kRootPathTestCases[i].expect_root_path, result);
    if (result) {
      FilePath expected = FilePath(kTestDataPath).AppendASCII(
          kRootPathTestCases[i].expected_path);
      EXPECT_EQ(expected.value(), root_path.value());
    }
  }
}

TEST_F(FileSystemPathManagerTest, CheckValidPath) {
  FileSystemPathManager* manager = GetNewPathManager(false);
  FilePath root_path;
  EXPECT_TRUE(manager->GetFileSystemRootPath(
      GURL("http://foo.com/"), fileapi::kFileSystemTypePersistent,
      &root_path, NULL));

  // The root path must be valid, but upper directories or directories
  // that are not in our temporary or persistent directory must be
  // evaluated invalid.
  EXPECT_TRUE(manager->CheckValidFileSystemPath(root_path));
  EXPECT_FALSE(manager->CheckValidFileSystemPath(root_path.DirName()));
  EXPECT_FALSE(manager->CheckValidFileSystemPath(
      root_path.DirName().DirName()));
  EXPECT_FALSE(manager->CheckValidFileSystemPath(
      root_path.DirName().AppendASCII("ArbitraryName")));

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kCheckValidPathTestCases); ++i) {
    SCOPED_TRACE(testing::Message() << "CheckValidPath #" << i << " "
                 << kCheckValidPathTestCases[i].path);
    FilePath path(kCheckValidPathTestCases[i].path);
    if (!path.IsAbsolute())
      path = root_path.Append(path);
    EXPECT_EQ(kCheckValidPathTestCases[i].expected_valid,
              manager->CheckValidFileSystemPath(path));
  }
}

TEST_F(FileSystemPathManagerTest, IsRestrictedName) {
  FileSystemPathManager* manager = GetNewPathManager(false);
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kIsRestrictedNameTestCases); ++i) {
    SCOPED_TRACE(testing::Message() << "IsRestrictedName #" << i << " "
                 << kIsRestrictedNameTestCases[i].name);
    FilePath name(kIsRestrictedNameTestCases[i].name);
    EXPECT_EQ(kIsRestrictedNameTestCases[i].expected_dangerous,
              manager->IsRestrictedFileName(name));
  }
}
