// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/file_system_util.h"

#include "base/file_path.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/fileapi/file_system_types.h"

namespace fileapi {
namespace {

class FileSystemUtilTest : public testing::Test {
 protected:
  bool CrackFileSystemURL(const char* url) {
    return fileapi::CrackFileSystemURL(
        GURL(url), &origin_url_, &type_, &file_path_);
  }

  GURL origin_url_;
  FileSystemType type_;
  FilePath file_path_;
};

TEST_F(FileSystemUtilTest, ParsePersistent) {
  ASSERT_TRUE(CrackFileSystemURL(
      "filesystem:http://chromium.org/persistent/directory/file"));
  EXPECT_EQ("http://chromium.org/", origin_url_.spec());
  EXPECT_EQ(kFileSystemTypePersistent, type_);
  EXPECT_EQ(FILE_PATH_LITERAL("file"),
      VirtualPath::BaseName(file_path_).value());
  EXPECT_EQ(FILE_PATH_LITERAL("directory"), file_path_.DirName().value());
}

TEST_F(FileSystemUtilTest, ParseTemporary) {
  ASSERT_TRUE(CrackFileSystemURL(
      "filesystem:http://chromium.org/temporary/directory/file"));
  EXPECT_EQ("http://chromium.org/", origin_url_.spec());
  EXPECT_EQ(kFileSystemTypeTemporary, type_);
  EXPECT_EQ(FILE_PATH_LITERAL("file"),
      VirtualPath::BaseName(file_path_).value());
  EXPECT_EQ(FILE_PATH_LITERAL("directory"), file_path_.DirName().value());
}

TEST_F(FileSystemUtilTest, EnsureFilePathIsRelative) {
  ASSERT_TRUE(CrackFileSystemURL(
      "filesystem:http://chromium.org/temporary/////directory/file"));
  EXPECT_EQ("http://chromium.org/", origin_url_.spec());
  EXPECT_EQ(kFileSystemTypeTemporary, type_);
  EXPECT_EQ(FILE_PATH_LITERAL("file"),
      VirtualPath::BaseName(file_path_).value());
  EXPECT_EQ(FILE_PATH_LITERAL("directory"), file_path_.DirName().value());
  EXPECT_FALSE(file_path_.IsAbsolute());
}

TEST_F(FileSystemUtilTest, RejectBadSchemes) {
  EXPECT_FALSE(CrackFileSystemURL("http://chromium.org/"));
  EXPECT_FALSE(CrackFileSystemURL("https://chromium.org/"));
  EXPECT_FALSE(CrackFileSystemURL("file:///foo/bar"));
  EXPECT_FALSE(CrackFileSystemURL("foobar:///foo/bar"));
}

TEST_F(FileSystemUtilTest, UnescapePath) {
  ASSERT_TRUE(CrackFileSystemURL(
      "filesystem:http://chromium.org/persistent/%7Echromium/space%20bar"));
  EXPECT_EQ(FILE_PATH_LITERAL("space bar"),
      VirtualPath::BaseName(file_path_).value());
  EXPECT_EQ(FILE_PATH_LITERAL("~chromium"), file_path_.DirName().value());
}

TEST_F(FileSystemUtilTest, RejectBadType) {
  EXPECT_FALSE(CrackFileSystemURL("filesystem:http://c.org/foobar/file"));
}

TEST_F(FileSystemUtilTest, RejectMalformedURL) {
  EXPECT_FALSE(CrackFileSystemURL("filesystem:///foobar/file"));
  EXPECT_FALSE(CrackFileSystemURL("filesystem:foobar/file"));
}

TEST_F(FileSystemUtilTest, GetTempFileSystemRootURI) {
  GURL origin_url("http://chromium.org");
  fileapi::FileSystemType type = fileapi::kFileSystemTypeTemporary;
  GURL uri = GURL("filesystem:http://chromium.org/temporary/");
  EXPECT_EQ(uri, GetFileSystemRootURI(origin_url, type));
}

TEST_F(FileSystemUtilTest, GetPersistentFileSystemRootURI) {
  GURL origin_url("http://chromium.org");
  fileapi::FileSystemType type = fileapi::kFileSystemTypePersistent;
  GURL uri = GURL("filesystem:http://chromium.org/persistent/");
  EXPECT_EQ(uri, GetFileSystemRootURI(origin_url, type));
}

TEST_F(FileSystemUtilTest, VirtualPathBaseName) {
  struct test_data {
    const FilePath::StringType path;
    const FilePath::StringType base_name;
  } test_cases[] = {
    { FILE_PATH_LITERAL("foo/bar"), FILE_PATH_LITERAL("bar") },
    { FILE_PATH_LITERAL("foo/b:bar"), FILE_PATH_LITERAL("b:bar") },
    { FILE_PATH_LITERAL(""), FILE_PATH_LITERAL("") },
    { FILE_PATH_LITERAL("/"), FILE_PATH_LITERAL("/") },
    { FILE_PATH_LITERAL("foo//////bar"), FILE_PATH_LITERAL("bar") },
    { FILE_PATH_LITERAL("foo/bar/"), FILE_PATH_LITERAL("bar") },
    { FILE_PATH_LITERAL("foo/bar/////"), FILE_PATH_LITERAL("bar") },
    { FILE_PATH_LITERAL("/bar/////"), FILE_PATH_LITERAL("bar") },
    { FILE_PATH_LITERAL("bar/////"), FILE_PATH_LITERAL("bar") },
    { FILE_PATH_LITERAL("bar/"), FILE_PATH_LITERAL("bar") },
    { FILE_PATH_LITERAL("/bar"), FILE_PATH_LITERAL("bar") },
    { FILE_PATH_LITERAL("////bar"), FILE_PATH_LITERAL("bar") },
    { FILE_PATH_LITERAL("bar"), FILE_PATH_LITERAL("bar") }
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_cases); ++i) {
    FilePath input = FilePath(test_cases[i].path);
    FilePath base_name = VirtualPath::BaseName(input);
    EXPECT_EQ(test_cases[i].base_name, base_name.value());
  }
}

TEST_F(FileSystemUtilTest, VirtualPathGetComponents) {
  struct test_data {
    const FilePath::StringType path;
    size_t count;
    const FilePath::StringType components[3];
  } test_cases[] = {
    { FILE_PATH_LITERAL("foo/bar"),
      2,
      { FILE_PATH_LITERAL("foo"), FILE_PATH_LITERAL("bar") } },
    { FILE_PATH_LITERAL("foo"),
      1,
      { FILE_PATH_LITERAL("foo") } },
    { FILE_PATH_LITERAL("foo////bar"),
      2,
      { FILE_PATH_LITERAL("foo"), FILE_PATH_LITERAL("bar") } },
    { FILE_PATH_LITERAL("foo/c:bar"),
      2,
      { FILE_PATH_LITERAL("foo"), FILE_PATH_LITERAL("c:bar") } },
    { FILE_PATH_LITERAL("c:foo/bar"),
      2,
      { FILE_PATH_LITERAL("c:foo"), FILE_PATH_LITERAL("bar") } },
    { FILE_PATH_LITERAL("foo/bar"),
      2,
      { FILE_PATH_LITERAL("foo"), FILE_PATH_LITERAL("bar") } },
    { FILE_PATH_LITERAL("/foo/bar"),
      2,
      { FILE_PATH_LITERAL("foo"), FILE_PATH_LITERAL("bar") } },
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_cases); ++i) {
    FilePath input = FilePath(test_cases[i].path);
    std::vector<FilePath::StringType> components;
    VirtualPath::GetComponents(input, &components);
    EXPECT_EQ(test_cases[i].count, components.size());
    for (size_t j = 0; j < components.size(); ++j)
      EXPECT_EQ(test_cases[i].components[j], components[j]);
  }
}

TEST_F(FileSystemUtilTest, GetIsolatedFileSystemName) {
  GURL origin_url("http://foo");
  std::string fsname1 = GetIsolatedFileSystemName(origin_url, "bar");
  EXPECT_EQ("http_foo_0:Isolated_bar", fsname1);
}

TEST_F(FileSystemUtilTest, CrackIsolatedFileSystemName) {
  std::string fsid;
  EXPECT_TRUE(CrackIsolatedFileSystemName("foo:Isolated_bar", &fsid));
  EXPECT_EQ("bar", fsid);
  EXPECT_TRUE(CrackIsolatedFileSystemName("foo:isolated_bar", &fsid));
  EXPECT_EQ("bar", fsid);
  EXPECT_TRUE(CrackIsolatedFileSystemName("foo:Isolated__bar", &fsid));
  EXPECT_EQ("_bar", fsid);
  EXPECT_TRUE(CrackIsolatedFileSystemName("foo::Isolated_bar", &fsid));
  EXPECT_EQ("bar", fsid);
}

TEST_F(FileSystemUtilTest, RejectBadIsolatedFileSystemName) {
  std::string fsid;
  EXPECT_FALSE(CrackIsolatedFileSystemName("foobar", &fsid));
  EXPECT_FALSE(CrackIsolatedFileSystemName("foo:_bar", &fsid));
  EXPECT_FALSE(CrackIsolatedFileSystemName("foo:Isolatedbar", &fsid));
  EXPECT_FALSE(CrackIsolatedFileSystemName("fooIsolatedbar", &fsid));
  EXPECT_FALSE(CrackIsolatedFileSystemName("foo:Persistent", &fsid));
  EXPECT_FALSE(CrackIsolatedFileSystemName("foo:Temporary", &fsid));
  EXPECT_FALSE(CrackIsolatedFileSystemName("foo:External", &fsid));
  EXPECT_FALSE(CrackIsolatedFileSystemName(":Isolated_bar", &fsid));
  EXPECT_FALSE(CrackIsolatedFileSystemName("foo:Isolated_", &fsid));
}

}  // namespace (anonymous)
}  // namespace fileapi
