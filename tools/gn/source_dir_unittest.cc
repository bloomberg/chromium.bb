// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "tools/gn/source_dir.h"
#include "tools/gn/source_file.h"

TEST(SourceDir, ResolveRelativeFile) {
  SourceDir base("//base/");

  // Empty input is an error.
  EXPECT_TRUE(base.ResolveRelativeFile("") == SourceFile());

  // These things are directories, so should be an error.
  EXPECT_TRUE(base.ResolveRelativeFile("//foo/bar/") == SourceFile());
  EXPECT_TRUE(base.ResolveRelativeFile("bar/") == SourceFile());

  // Absolute paths should be passed unchanged.
  EXPECT_TRUE(base.ResolveRelativeFile("//foo") == SourceFile("//foo"));
  EXPECT_TRUE(base.ResolveRelativeFile("/foo") == SourceFile("/foo"));

  // Basic relative stuff.
  EXPECT_TRUE(base.ResolveRelativeFile("foo") == SourceFile("//base/foo"));
  EXPECT_TRUE(base.ResolveRelativeFile("./foo") == SourceFile("//base/foo"));
  EXPECT_TRUE(base.ResolveRelativeFile("../foo") == SourceFile("//foo"));
  EXPECT_TRUE(base.ResolveRelativeFile("../../foo") == SourceFile("//foo"));

#if defined(OS_WIN)
  // Note that we don't canonicalize the backslashes to forward slashes.
  // This could potentially be changed in the future which would mean we should
  // just change the expected result.
  EXPECT_TRUE(base.ResolveRelativeFile("C:\\foo\\bar.txt") ==
              SourceFile("/C:/foo/bar.txt"));
#endif
}

TEST(SourceDir, ResolveRelativeDir) {
  SourceDir base("//base/");

  // Empty input is an error.
  EXPECT_TRUE(base.ResolveRelativeDir("") == SourceDir());

  // Absolute paths should be passed unchanged.
  EXPECT_TRUE(base.ResolveRelativeDir("//foo") == SourceDir("//foo/"));
  EXPECT_TRUE(base.ResolveRelativeDir("/foo") == SourceDir("/foo/"));

  // Basic relative stuff.
  EXPECT_TRUE(base.ResolveRelativeDir("foo") == SourceDir("//base/foo/"));
  EXPECT_TRUE(base.ResolveRelativeDir("./foo") == SourceDir("//base/foo/"));
  EXPECT_TRUE(base.ResolveRelativeDir("../foo") == SourceDir("//foo/"));
  EXPECT_TRUE(base.ResolveRelativeDir("../../foo/") == SourceDir("//foo/"));

#if defined(OS_WIN)
  // Note that we don't canonicalize the existing backslashes to forward
  // slashes. This could potentially be changed in the future which would mean
  // we should just change the expected result.
  EXPECT_TRUE(base.ResolveRelativeDir("C:\\foo") == SourceDir("/C:/foo/"));
#endif
}
