// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/external_mount_points.h"

#include <string>

#include "base/file_path.h"
#include "testing/gtest/include/gtest/gtest.h"

#define FPL FILE_PATH_LITERAL

#if defined(FILE_PATH_USES_DRIVE_LETTERS)
#define DRIVE FPL("C:")
#else
#define DRIVE
#endif

namespace {

TEST(ExternalMountPointsTest, AddMountPoint) {
  scoped_refptr<fileapi::ExternalMountPoints> mount_points(
      fileapi::ExternalMountPoints::CreateRefCounted());

  struct TestCase {
    // The mount point's name.
    const char* const name;
    // The mount point's path.
    const FilePath::CharType* const path;
    // Whether the mount point registration should succeed.
    bool success;
    // Path returned by GetRegisteredPath. NULL if the method is expected to
    // fail.
    const FilePath::CharType* const registered_path;
  };

  const TestCase kTestCases[] = {
    // Valid mount point.
    { "test", DRIVE FPL("/foo/test"), true, DRIVE FPL("/foo/test") },
    // Valid mount point with only one path component.
    { "bbb", DRIVE FPL("/bbb"), true, DRIVE FPL("/bbb") },
    // Existing mount point path is substring of the mount points path.
    { "test11", DRIVE FPL("/foo/test11"), true, DRIVE FPL("/foo/test11") },
    // Path substring of an existing path.
    { "test1", DRIVE FPL("/foo/test1"), true, DRIVE FPL("/foo/test1") },
    // Empty mount point name and path.
    { "", DRIVE FPL(""), false, NULL },
    // Empty mount point name.
    { "", DRIVE FPL("/ddd"), false, NULL },
    // Empty mount point path.
    { "empty_path", FPL(""), true, FPL("") },
    // Name different from path's base name.
    { "not_base_name", DRIVE FPL("/x/y/z"), true, DRIVE FPL("/x/y/z") },
    // References parent.
    { "invalid", DRIVE FPL("../foo/invalid"), false, NULL },
    // Relative path.
    { "relative", DRIVE FPL("foo/relative"), false, NULL },
    // Existing mount point path.
    { "path_exists", DRIVE FPL("/foo/test"), false, NULL },
    // Mount point with the same name exists.
    { "test", DRIVE FPL("/foo/a/test_name_exists"), false,
      DRIVE FPL("/foo/test") },
    // Child of an existing mount point.
    { "a1", DRIVE FPL("/foo/test/a"), false, NULL },
    // Parent of an existing mount point.
    { "foo1", DRIVE FPL("/foo"), false, NULL },
    // Bit bigger depth.
    { "g", DRIVE FPL("/foo/a/b/c/d/e/f/g"), true,
      DRIVE FPL("/foo/a/b/c/d/e/f/g") },
    // Sibling mount point (with similar name) exists.
    { "ff", DRIVE FPL("/foo/a/b/c/d/e/ff"), true,
       DRIVE FPL("/foo/a/b/c/d/e/ff") },
    // Lexicographically last among existing mount points.
    { "yyy", DRIVE FPL("/zzz/yyy"), true, DRIVE FPL("/zzz/yyy") },
    // Parent of the lexicographically last mount point.
    { "zzz1", DRIVE FPL("/zzz"), false, NULL },
    // Child of the lexicographically last mount point.
    { "xxx1", DRIVE FPL("/zzz/yyy/xxx"), false, NULL },
    // Lexicographically first among existing mount points.
    { "b", DRIVE FPL("/a/b"), true, DRIVE FPL("/a/b") },
    // Parent of lexicographically first mount point.
    { "a2", DRIVE FPL("/a"), false, NULL },
    // Child of lexicographically last mount point.
    { "c1", DRIVE FPL("/a/b/c"), false, NULL },
    // Parent to all of the mount points.
    { "root", DRIVE FPL("/"), false, NULL },
    // Path contains .. component.
    { "funky", DRIVE FPL("/tt/fun/../funky"), false, NULL },
    // Windows separators.
#if defined(FILE_PATH_USES_WIN_SEPARATORS)
    { "win", DRIVE FPL("\\try\\separators\\win"), true,
      DRIVE FPL("\\try\\separators\\win") },
    { "win1", DRIVE FPL("\\try/separators\\win1"), true,
      DRIVE FPL("\\try/separators\\win1") },
    { "win2", DRIVE FPL("\\try/separators\\win"), false, NULL },
#else
    { "win", DRIVE FPL("\\separators\\win"), false, NULL },
    { "win1", DRIVE FPL("\\try/separators\\win1"), false, NULL },
#endif
    // Win separators, but relative path.
    { "win2", DRIVE FPL("try\\separators\\win2"), false, NULL },
  };

  // Test adding mount points.
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kTestCases); ++i) {
    EXPECT_EQ(kTestCases[i].success,
              mount_points->RegisterFileSystem(
                  kTestCases[i].name,
                  fileapi::kFileSystemTypeNativeLocal,
                  FilePath(kTestCases[i].path)))
        << "Adding mount point: " << kTestCases[i].name << " with path "
        << kTestCases[i].path;
  }

  // Test that final mount point presence state is as expected.
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kTestCases); ++i) {
    FilePath found_path;
    EXPECT_EQ(kTestCases[i].registered_path != NULL,
              mount_points->GetRegisteredPath(kTestCases[i].name, &found_path))
        << "Test case: " << i;

    if (kTestCases[i].registered_path) {
      FilePath expected_path(kTestCases[i].registered_path);
      EXPECT_EQ(expected_path.NormalizePathSeparators(), found_path);
    }
  }
}

TEST(ExternalMountPointsTest, GetVirtualPath) {
  scoped_refptr<fileapi::ExternalMountPoints> mount_points(
      fileapi::ExternalMountPoints::CreateRefCounted());

  mount_points->RegisterFileSystem("c",
                                   fileapi::kFileSystemTypeNativeLocal,
                                   FilePath(DRIVE FPL("/a/b/c")));
  // Note that "/a/b/c" < "/a/b/c(1)" < "/a/b/c/".
  mount_points->RegisterFileSystem("c(1)",
                                   fileapi::kFileSystemTypeNativeLocal,
                                   FilePath(DRIVE FPL("/a/b/c(1)")));
  mount_points->RegisterFileSystem("x",
                                   fileapi::kFileSystemTypeNativeLocal,
                                   FilePath(DRIVE FPL("/z/y/x")));
  mount_points->RegisterFileSystem("o",
                                   fileapi::kFileSystemTypeNativeLocal,
                                   FilePath(DRIVE FPL("/m/n/o")));
  // A mount point whose name does not match its path base name.
  mount_points->RegisterFileSystem("mount",
                                   fileapi::kFileSystemTypeNativeLocal,
                                   FilePath(DRIVE FPL("/root/foo")));
  // A mount point with an empty path.
  mount_points->RegisterFileSystem("empty_path",
                                   fileapi::kFileSystemTypeNativeLocal,
                                   FilePath(FPL("")));

  struct TestCase {
    const FilePath::CharType* const local_path;
    bool success;
    const FilePath::CharType* const virtual_path;
  };

  const TestCase kTestCases[] = {
    // Empty path.
    { FPL(""), false, FPL("") },
    // No registered mount point (but is parent to a mount point).
    { DRIVE FPL("/a/b"), false, FPL("") },
    // No registered mount point (but is parent to a mount point).
    { DRIVE FPL("/z/y"), false, FPL("") },
    // No registered mount point (but is parent to a mount point).
    { DRIVE FPL("/m/n"), false, FPL("") },
    // No registered mount point.
    { DRIVE FPL("/foo/mount"), false, FPL("") },
    // An existing mount point path is substring.
    { DRIVE FPL("/a/b/c1"), false, FPL("") },
    // No leading /.
    { DRIVE FPL("a/b/c"), false, FPL("") },
    // Sibling to a root path.
    { DRIVE FPL("/a/b/d/e"), false, FPL("") },
    // Sibling to a root path.
    { DRIVE FPL("/z/y/v/u"), false, FPL("") },
    // Sibling to a root path.
    { DRIVE FPL("/m/n/p/q"), false, FPL("") },
    // Mount point root path.
    { DRIVE FPL("/a/b/c"), true, FPL("c") },
    // Mount point root path.
    { DRIVE FPL("/z/y/x"), true, FPL("x") },
    // Mount point root path.
    { DRIVE FPL("/m/n/o"), true, FPL("o") },
    // Mount point child path.
    { DRIVE FPL("/a/b/c/d/e"), true, FPL("c/d/e") },
    // Mount point child path.
    { DRIVE FPL("/z/y/x/v/u"), true, FPL("x/v/u") },
    // Mount point child path.
    { DRIVE FPL("/m/n/o/p/q"), true, FPL("o/p/q") },
    // Name doesn't match mount point path base name.
    { DRIVE FPL("/root/foo/a/b/c"), true, FPL("mount/a/b/c") },
    { DRIVE FPL("/root/foo"), true, FPL("mount") },
    // Mount point contains character whose ASCII code is smaller than file path
    // separator's.
    { DRIVE FPL("/a/b/c(1)/d/e"), true, FPL("c(1)/d/e") },
#if defined(FILE_PATH_USES_WIN_SEPARATORS)
    // Path with win separators mixed in.
    { DRIVE FPL("/a\\b\\c/d"), true, FPL("c/d") },
#endif
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kTestCases); ++i) {
    // Initialize virtual path with a value.
    FilePath virtual_path(DRIVE FPL("/mount"));
    FilePath local_path(kTestCases[i].local_path);
    EXPECT_EQ(kTestCases[i].success,
              mount_points->GetVirtualPath(local_path, &virtual_path))
        << "Resolving " << kTestCases[i].local_path;

    // There are no guarantees for |virtual_path| value if |GetVirtualPath|
    // fails.
    if (!kTestCases[i].success)
      continue;

    FilePath expected_virtual_path(kTestCases[i].virtual_path);
    EXPECT_EQ(expected_virtual_path.NormalizePathSeparators(), virtual_path)
        << "Resolving " << kTestCases[i].local_path;
  }
}

}  // namespace

