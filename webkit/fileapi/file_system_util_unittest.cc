// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
  EXPECT_EQ(FILE_PATH_LITERAL("directory/file"), file_path_.value());
}

TEST_F(FileSystemUtilTest, ParseTemporary) {
  ASSERT_TRUE(CrackFileSystemURL(
      "filesystem:http://chromium.org/temporary/directory/file"));
  EXPECT_EQ("http://chromium.org/", origin_url_.spec());
  EXPECT_EQ(kFileSystemTypeTemporary, type_);
  EXPECT_EQ(FILE_PATH_LITERAL("directory/file"), file_path_.value());
}

TEST_F(FileSystemUtilTest, EnsureFilePathIsRelative) {
  ASSERT_TRUE(CrackFileSystemURL(
      "filesystem:http://chromium.org/temporary/////directory/file"));
  EXPECT_EQ("http://chromium.org/", origin_url_.spec());
  EXPECT_EQ(kFileSystemTypeTemporary, type_);
  EXPECT_EQ(FILE_PATH_LITERAL("directory/file"), file_path_.value());
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
  EXPECT_EQ(FILE_PATH_LITERAL("~chromium/space bar"), file_path_.value());
}

TEST_F(FileSystemUtilTest, RejectBadType) {
  EXPECT_FALSE(CrackFileSystemURL("filesystem:http://c.org/foobar/file"));
}

TEST_F(FileSystemUtilTest, RejectMalformedURL) {
  EXPECT_FALSE(CrackFileSystemURL("filesystem:///foobar/file"));
  EXPECT_FALSE(CrackFileSystemURL("filesystem:foobar/file"));
}

}  // namespace (anonymous)
}  // namespace fileapi
