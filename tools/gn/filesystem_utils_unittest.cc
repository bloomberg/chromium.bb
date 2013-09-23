// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "tools/gn/filesystem_utils.h"

TEST(FilesystemUtils, FileExtensionOffset) {
  EXPECT_EQ(std::string::npos, FindExtensionOffset(""));
  EXPECT_EQ(std::string::npos, FindExtensionOffset("foo/bar/baz"));
  EXPECT_EQ(4u, FindExtensionOffset("foo."));
  EXPECT_EQ(4u, FindExtensionOffset("f.o.bar"));
  EXPECT_EQ(std::string::npos, FindExtensionOffset("foo.bar/"));
  EXPECT_EQ(std::string::npos, FindExtensionOffset("foo.bar/baz"));
}

TEST(FilesystemUtils, FindExtension) {
  std::string input;
  EXPECT_EQ("", FindExtension(&input).as_string());
  input = "foo/bar/baz";
  EXPECT_EQ("", FindExtension(&input).as_string());
  input = "foo.";
  EXPECT_EQ("", FindExtension(&input).as_string());
  input = "f.o.bar";
  EXPECT_EQ("bar", FindExtension(&input).as_string());
  input = "foo.bar/";
  EXPECT_EQ("", FindExtension(&input).as_string());
  input = "foo.bar/baz";
  EXPECT_EQ("", FindExtension(&input).as_string());
}

TEST(FilesystemUtils, FindFilenameOffset) {
  EXPECT_EQ(0u, FindFilenameOffset(""));
  EXPECT_EQ(0u, FindFilenameOffset("foo"));
  EXPECT_EQ(4u, FindFilenameOffset("foo/"));
  EXPECT_EQ(4u, FindFilenameOffset("foo/bar"));
}

TEST(FilesystemUtils, RemoveFilename) {
  std::string s;

  RemoveFilename(&s);
  EXPECT_STREQ("", s.c_str());

  s = "foo";
  RemoveFilename(&s);
  EXPECT_STREQ("", s.c_str());

  s = "/";
  RemoveFilename(&s);
  EXPECT_STREQ("/", s.c_str());

  s = "foo/bar";
  RemoveFilename(&s);
  EXPECT_STREQ("foo/", s.c_str());

  s = "foo/bar/baz.cc";
  RemoveFilename(&s);
  EXPECT_STREQ("foo/bar/", s.c_str());
}

TEST(FilesystemUtils, FindDir) {
  std::string input;
  EXPECT_EQ("", FindDir(&input));
  input = "/";
  EXPECT_EQ("/", FindDir(&input));
  input = "foo/";
  EXPECT_EQ("foo/", FindDir(&input));
  input = "foo/bar/baz";
  EXPECT_EQ("foo/bar/", FindDir(&input));
}

TEST(FilesystemUtils, IsPathAbsolute) {
  EXPECT_TRUE(IsPathAbsolute("/foo/bar"));
  EXPECT_TRUE(IsPathAbsolute("/"));
  EXPECT_FALSE(IsPathAbsolute(""));
  EXPECT_FALSE(IsPathAbsolute("//"));
  EXPECT_FALSE(IsPathAbsolute("//foo/bar"));

#if defined(OS_WIN)
  EXPECT_TRUE(IsPathAbsolute("C:/foo"));
  EXPECT_TRUE(IsPathAbsolute("C:/"));
  EXPECT_TRUE(IsPathAbsolute("C:\\foo"));
  EXPECT_TRUE(IsPathAbsolute("C:\\"));
  EXPECT_TRUE(IsPathAbsolute("/C:/foo"));
  EXPECT_TRUE(IsPathAbsolute("/C:\\foo"));
#endif
}

TEST(FilesystemUtils, MakeAbsolutePathRelativeIfPossible) {
  std::string dest;

#if defined(OS_WIN)
  EXPECT_TRUE(MakeAbsolutePathRelativeIfPossible("C:\\base", "C:\\base\\foo",
                                                 &dest));
  EXPECT_EQ("//foo", dest);
  EXPECT_TRUE(MakeAbsolutePathRelativeIfPossible("C:\\base", "/C:/base/foo",
                                                 &dest));
  EXPECT_EQ("//foo", dest);
  EXPECT_TRUE(MakeAbsolutePathRelativeIfPossible("c:\\base", "C:\\base\\foo\\",
                                                 &dest));
  EXPECT_EQ("//foo\\", dest);

  EXPECT_FALSE(MakeAbsolutePathRelativeIfPossible("C:\\base", "C:\\ba", &dest));
  EXPECT_FALSE(MakeAbsolutePathRelativeIfPossible("C:\\base",
                                                  "C:\\/notbase/foo",
                                                  &dest));
#else

  EXPECT_TRUE(MakeAbsolutePathRelativeIfPossible("/base", "/base/foo/", &dest));
  EXPECT_EQ("//foo/", dest);
  EXPECT_TRUE(MakeAbsolutePathRelativeIfPossible("/base", "/base/foo", &dest));
  EXPECT_EQ("//foo", dest);
  EXPECT_TRUE(MakeAbsolutePathRelativeIfPossible("/base/", "/base/foo/",
                                                 &dest));
  EXPECT_EQ("//foo/", dest);

  EXPECT_FALSE(MakeAbsolutePathRelativeIfPossible("/base", "/ba", &dest));
  EXPECT_FALSE(MakeAbsolutePathRelativeIfPossible("/base", "/notbase/foo",
                                                  &dest));
#endif
}

TEST(FilesystemUtils, InvertDir) {
  EXPECT_TRUE(InvertDir(SourceDir()) == "");
  EXPECT_TRUE(InvertDir(SourceDir("/")) == "");
  EXPECT_TRUE(InvertDir(SourceDir("//")) == "");

  EXPECT_TRUE(InvertDir(SourceDir("//foo/bar")) == "../../");
  EXPECT_TRUE(InvertDir(SourceDir("/foo/bar/")) == "../../");
}

TEST(FilesystemUtils, NormalizePath) {
  std::string input;

  NormalizePath(&input);
  EXPECT_EQ("", input);

  input = "foo/bar.txt";
  NormalizePath(&input);
  EXPECT_EQ("foo/bar.txt", input);

  input = ".";
  NormalizePath(&input);
  EXPECT_EQ("", input);

  input = "..";
  NormalizePath(&input);
  EXPECT_EQ("..", input);

  input = "foo//bar";
  NormalizePath(&input);
  EXPECT_EQ("foo/bar", input);

  input = "//foo";
  NormalizePath(&input);
  EXPECT_EQ("//foo", input);

  input = "foo/..//bar";
  NormalizePath(&input);
  EXPECT_EQ("bar", input);

  input = "foo/../../bar";
  NormalizePath(&input);
  EXPECT_EQ("../bar", input);

  input = "/../foo";  // Don't go aboe the root dir.
  NormalizePath(&input);
  EXPECT_EQ("/foo", input);

  input = "//../foo";  // Don't go aboe the root dir.
  NormalizePath(&input);
  EXPECT_EQ("//foo", input);

  input = "../foo";
  NormalizePath(&input);
  EXPECT_EQ("../foo", input);

  input = "..";
  NormalizePath(&input);
  EXPECT_EQ("..", input);

  input = "./././.";
  NormalizePath(&input);
  EXPECT_EQ("", input);

  input = "../../..";
  NormalizePath(&input);
  EXPECT_EQ("../../..", input);

  input = "../";
  NormalizePath(&input);
  EXPECT_EQ("../", input);
}

TEST(FilesystemUtils, RebaseSourceAbsolutePath) {
  // Degenerate case.
  EXPECT_EQ(".", RebaseSourceAbsolutePath("//", SourceDir("//")));
  EXPECT_EQ(".",
            RebaseSourceAbsolutePath("//foo/bar/", SourceDir("//foo/bar/")));

  // Going up the tree.
  EXPECT_EQ("../foo",
            RebaseSourceAbsolutePath("//foo", SourceDir("//bar/")));
  EXPECT_EQ("../foo/",
            RebaseSourceAbsolutePath("//foo/", SourceDir("//bar/")));
  EXPECT_EQ("../../foo",
            RebaseSourceAbsolutePath("//foo", SourceDir("//bar/moo")));
  EXPECT_EQ("../../foo/",
            RebaseSourceAbsolutePath("//foo/", SourceDir("//bar/moo")));

  // Going down the tree.
  EXPECT_EQ("foo/bar",
            RebaseSourceAbsolutePath("//foo/bar", SourceDir("//")));
  EXPECT_EQ("foo/bar/",
            RebaseSourceAbsolutePath("//foo/bar/", SourceDir("//")));

  // Going up and down the tree.
  EXPECT_EQ("../../foo/bar",
            RebaseSourceAbsolutePath("//foo/bar", SourceDir("//a/b/")));
  EXPECT_EQ("../../foo/bar/",
            RebaseSourceAbsolutePath("//foo/bar/", SourceDir("//a/b/")));

  // Sharing prefix.
  EXPECT_EQ("foo",
            RebaseSourceAbsolutePath("//a/foo", SourceDir("//a/")));
  EXPECT_EQ("foo/",
            RebaseSourceAbsolutePath("//a/foo/", SourceDir("//a/")));
  EXPECT_EQ("foo",
            RebaseSourceAbsolutePath("//a/b/foo", SourceDir("//a/b/")));
  EXPECT_EQ("foo/",
            RebaseSourceAbsolutePath("//a/b/foo/", SourceDir("//a/b/")));
  EXPECT_EQ("foo/bar",
            RebaseSourceAbsolutePath("//a/b/foo/bar", SourceDir("//a/b/")));
  EXPECT_EQ("foo/bar/",
            RebaseSourceAbsolutePath("//a/b/foo/bar/", SourceDir("//a/b/")));

  // One could argue about this case. Since the input doesn't have a slash it
  // would normally not be treated like a directory and we'd go up, which is
  // simpler. However, since it matches the output directory's name, we could
  // potentially infer that it's the same and return "." for this.
  EXPECT_EQ("../bar",
            RebaseSourceAbsolutePath("//foo/bar", SourceDir("//foo/bar/")));
}
