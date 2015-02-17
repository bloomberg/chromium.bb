// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "tools/gn/source_dir.h"
#include "tools/gn/source_file.h"

TEST(SourceDir, ResolveRelativeFile) {
  SourceDir base("//base/");
#if defined(OS_WIN)
  base::StringPiece source_root("C:/source/root");
#else
  base::StringPiece source_root("/source/root");
#endif

  // Empty input is an error.
  EXPECT_TRUE(base.ResolveRelativeFile("", source_root) == SourceFile());

  // These things are directories, so should be an error.
  EXPECT_TRUE(base.ResolveRelativeFile("//foo/bar/", source_root) ==
              SourceFile());
  EXPECT_TRUE(base.ResolveRelativeFile("bar/", source_root) ==
              SourceFile());

  // Absolute paths should be passed unchanged.
  EXPECT_TRUE(base.ResolveRelativeFile("//foo",source_root) ==
              SourceFile("//foo"));
  EXPECT_TRUE(base.ResolveRelativeFile("/foo", source_root) ==
              SourceFile("/foo"));

  // Basic relative stuff.
  EXPECT_TRUE(base.ResolveRelativeFile("foo", source_root) ==
              SourceFile("//base/foo"));
  EXPECT_TRUE(base.ResolveRelativeFile("./foo", source_root) ==
              SourceFile("//base/foo"));
  EXPECT_TRUE(base.ResolveRelativeFile("../foo", source_root) ==
              SourceFile("//foo"));

  // If the given relative path points outside the source root, we
  // expect an absolute path.
#if defined(OS_WIN)
  EXPECT_TRUE(base.ResolveRelativeFile("../../foo", source_root) ==
              SourceFile("/C:/source/foo"));
#else
  EXPECT_TRUE(base.ResolveRelativeFile("../../foo", source_root) ==
              SourceFile("/source/foo"));
#endif

#if defined(OS_WIN)
  // Note that we don't canonicalize the backslashes to forward slashes.
  // This could potentially be changed in the future which would mean we should
  // just change the expected result.
  EXPECT_TRUE(base.ResolveRelativeFile("C:\\foo\\bar.txt", source_root) ==
              SourceFile("/C:/foo/bar.txt"));
#endif
}

TEST(SourceDir, ResolveRelativeDir) {
  SourceDir base("//base/");
#if defined(OS_WIN)
  base::StringPiece source_root("C:/source/root");
#else
  base::StringPiece source_root("/source/root");
#endif

  // Empty input is an error.
  EXPECT_TRUE(base.ResolveRelativeDir("", source_root) == SourceDir());

  // Absolute paths should be passed unchanged.
  EXPECT_TRUE(base.ResolveRelativeDir("//foo", source_root) ==
              SourceDir("//foo/"));
  EXPECT_TRUE(base.ResolveRelativeDir("/foo", source_root) ==
              SourceDir("/foo/"));

  // Basic relative stuff.
  EXPECT_TRUE(base.ResolveRelativeDir("foo", source_root) ==
              SourceDir("//base/foo/"));
  EXPECT_TRUE(base.ResolveRelativeDir("./foo", source_root) ==
              SourceDir("//base/foo/"));
  EXPECT_TRUE(base.ResolveRelativeDir("../foo", source_root) ==
              SourceDir("//foo/"));

  // If the given relative path points outside the source root, we
  // expect an absolute path.
#if defined(OS_WIN)
  EXPECT_TRUE(base.ResolveRelativeDir("../../foo", source_root) ==
              SourceDir("/C:/source/foo/"));
#else
  EXPECT_TRUE(base.ResolveRelativeDir("../../foo", source_root) ==
              SourceDir("/source/foo/"));
#endif

#if defined(OS_WIN)
  // Canonicalize the existing backslashes to forward slashes and add a
  // leading slash if necessary.
  EXPECT_TRUE(base.ResolveRelativeDir("\\C:\\foo") == SourceDir("/C:/foo/"));
  EXPECT_TRUE(base.ResolveRelativeDir("C:\\foo") == SourceDir("/C:/foo/"));
#endif
}
