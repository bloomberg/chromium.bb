// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "tools/gn/filesystem_utils.h"
#include "tools/gn/target.h"

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

TEST(FilesystemUtils, FindLastDirComponent) {
  SourceDir empty;
  EXPECT_EQ("", FindLastDirComponent(empty));

  SourceDir root("/");
  EXPECT_EQ("", FindLastDirComponent(root));

  SourceDir srcroot("//");
  EXPECT_EQ("", FindLastDirComponent(srcroot));

  SourceDir regular1("//foo/");
  EXPECT_EQ("foo", FindLastDirComponent(regular1));

  SourceDir regular2("//foo/bar/");
  EXPECT_EQ("bar", FindLastDirComponent(regular2));
}

TEST(FilesystemUtils, EnsureStringIsInOutputDir) {
  SourceDir output_dir("//out/Debug/");

  // Some outside.
  Err err;
  EXPECT_FALSE(EnsureStringIsInOutputDir(output_dir, "//foo", NULL, &err));
  EXPECT_TRUE(err.has_error());
  err = Err();
  EXPECT_FALSE(EnsureStringIsInOutputDir(output_dir, "//out/Debugit", NULL,
                                         &err));
  EXPECT_TRUE(err.has_error());

  // Some inside.
  err = Err();
  EXPECT_TRUE(EnsureStringIsInOutputDir(output_dir, "//out/Debug/", NULL,
                                        &err));
  EXPECT_FALSE(err.has_error());
  EXPECT_TRUE(EnsureStringIsInOutputDir(output_dir, "//out/Debug/foo", NULL,
                                        &err));
  EXPECT_FALSE(err.has_error());

  // Pattern but no template expansions are allowed.
  EXPECT_FALSE(EnsureStringIsInOutputDir(output_dir, "{{source_gen_dir}}",
                                         NULL, &err));
  EXPECT_TRUE(err.has_error());
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
  EXPECT_TRUE(InvertDir(SourceDir("//foo\\bar")) == "../../");
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

  input = "//../foo";  // Don't go above the root dir.
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

  // Backslash normalization.
  input = "foo\\..\\..\\bar";
  NormalizePath(&input);
  EXPECT_EQ("../bar", input);
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

TEST(FilesystemUtils, DirectoryWithNoLastSlash) {
  EXPECT_EQ("", DirectoryWithNoLastSlash(SourceDir()));
  EXPECT_EQ("/.", DirectoryWithNoLastSlash(SourceDir("/")));
  EXPECT_EQ("//.", DirectoryWithNoLastSlash(SourceDir("//")));
  EXPECT_EQ("//foo", DirectoryWithNoLastSlash(SourceDir("//foo/")));
  EXPECT_EQ("/bar", DirectoryWithNoLastSlash(SourceDir("/bar/")));
}

TEST(FilesystemUtils, SourceDirForPath) {
#if defined(OS_WIN)
  base::FilePath root(L"C:\\source\\foo\\");
  EXPECT_EQ("/C:/foo/bar/", SourceDirForPath(root,
            base::FilePath(L"C:\\foo\\bar")).value());
  EXPECT_EQ("/", SourceDirForPath(root,
            base::FilePath(L"/")).value());
  EXPECT_EQ("//", SourceDirForPath(root,
            base::FilePath(L"C:\\source\\foo")).value());
  EXPECT_EQ("//bar/", SourceDirForPath(root,
            base::FilePath(L"C:\\source\\foo\\bar\\")). value());
  EXPECT_EQ("//bar/baz/", SourceDirForPath(root,
            base::FilePath(L"C:\\source\\foo\\bar\\baz")).value());

  // Should be case-and-slash-insensitive.
  EXPECT_EQ("//baR/", SourceDirForPath(root,
            base::FilePath(L"c:/SOURCE\\Foo/baR/")).value());

  // Some "weird" Windows paths.
  EXPECT_EQ("/foo/bar/", SourceDirForPath(root,
            base::FilePath(L"/foo/bar/")).value());
  EXPECT_EQ("/C:/foo/bar/", SourceDirForPath(root,
            base::FilePath(L"C:foo/bar/")).value());

  // Also allow absolute GN-style Windows paths.
  EXPECT_EQ("/C:/foo/bar/", SourceDirForPath(root,
            base::FilePath(L"/C:/foo/bar")).value());
  EXPECT_EQ("//bar/", SourceDirForPath(root,
            base::FilePath(L"/C:/source/foo/bar")).value());

#else
  base::FilePath root("/source/foo/");
  EXPECT_EQ("/foo/bar/", SourceDirForPath(root,
            base::FilePath("/foo/bar/")).value());
  EXPECT_EQ("/", SourceDirForPath(root,
            base::FilePath("/")).value());
  EXPECT_EQ("//", SourceDirForPath(root,
            base::FilePath("/source/foo")).value());
  EXPECT_EQ("//bar/", SourceDirForPath(root,
            base::FilePath("/source/foo/bar/")).value());
  EXPECT_EQ("//bar/baz/", SourceDirForPath(root,
            base::FilePath("/source/foo/bar/baz/")).value());

  // Should be case-sensitive.
  EXPECT_EQ("/SOURCE/foo/bar/", SourceDirForPath(root,
            base::FilePath("/SOURCE/foo/bar/")).value());
#endif
}

TEST(FilesystemUtils, GetToolchainDirs) {
  BuildSettings build_settings;
  build_settings.SetBuildDir(SourceDir("//out/Debug/"));

  // The default toolchain.
  Settings default_settings(&build_settings, "");
  Label default_toolchain_label(SourceDir("//toolchain/"), "default");
  default_settings.set_toolchain_label(default_toolchain_label);
  default_settings.set_default_toolchain_label(default_toolchain_label);

  // Default toolchain out dir.
  EXPECT_EQ("//out/Debug/",
            GetToolchainOutputDir(&default_settings).value());
  EXPECT_EQ("//out/Debug/",
            GetToolchainOutputDir(&build_settings, default_toolchain_label,
                                  true).value());

  // Default toolchain gen dir.
  EXPECT_EQ("//out/Debug/gen/",
            GetToolchainGenDir(&default_settings).value());
  EXPECT_EQ("gen/",
            GetToolchainGenDirAsOutputFile(&default_settings).value());
  EXPECT_EQ("//out/Debug/gen/",
            GetToolchainGenDir(&build_settings, default_toolchain_label,
                               true).value());

  // Check a secondary toolchain.
  Settings other_settings(&build_settings, "two/");
  Label other_toolchain_label(SourceDir("//toolchain/"), "two");
  default_settings.set_toolchain_label(other_toolchain_label);
  default_settings.set_default_toolchain_label(default_toolchain_label);

  // Secondary toolchain out dir.
  EXPECT_EQ("//out/Debug/two/",
            GetToolchainOutputDir(&other_settings).value());
  EXPECT_EQ("//out/Debug/two/",
            GetToolchainOutputDir(&build_settings, other_toolchain_label,
                                  false).value());

  // Secondary toolchain gen dir.
  EXPECT_EQ("//out/Debug/two/gen/",
            GetToolchainGenDir(&other_settings).value());
  EXPECT_EQ("two/gen/",
            GetToolchainGenDirAsOutputFile(&other_settings).value());
  EXPECT_EQ("//out/Debug/two/gen/",
            GetToolchainGenDir(&build_settings, other_toolchain_label,
                               false).value());
}

TEST(FilesystemUtils, GetOutDirForSourceDir) {
  BuildSettings build_settings;
  build_settings.SetBuildDir(SourceDir("//out/Debug/"));

  // Test the default toolchain.
  Settings default_settings(&build_settings, "");
  EXPECT_EQ("//out/Debug/obj/",
            GetOutputDirForSourceDir(
                &default_settings, SourceDir("//")).value());
  EXPECT_EQ("obj/",
            GetOutputDirForSourceDirAsOutputFile(
                &default_settings, SourceDir("//")).value());

  EXPECT_EQ("//out/Debug/obj/foo/bar/",
            GetOutputDirForSourceDir(
                &default_settings, SourceDir("//foo/bar/")).value());
  EXPECT_EQ("obj/foo/bar/",
            GetOutputDirForSourceDirAsOutputFile(
                &default_settings, SourceDir("//foo/bar/")).value());

  // Secondary toolchain.
  Settings other_settings(&build_settings, "two/");
  EXPECT_EQ("//out/Debug/two/obj/",
            GetOutputDirForSourceDir(
                &other_settings, SourceDir("//")).value());
  EXPECT_EQ("two/obj/",
            GetOutputDirForSourceDirAsOutputFile(
                &other_settings, SourceDir("//")).value());

  EXPECT_EQ("//out/Debug/two/obj/foo/bar/",
            GetOutputDirForSourceDir(&other_settings,
                                     SourceDir("//foo/bar/")).value());
  EXPECT_EQ("two/obj/foo/bar/",
            GetOutputDirForSourceDirAsOutputFile(
                &other_settings, SourceDir("//foo/bar/")).value());
}

TEST(FilesystemUtils, GetGenDirForSourceDir) {
  BuildSettings build_settings;
  build_settings.SetBuildDir(SourceDir("//out/Debug/"));

  // Test the default toolchain.
  Settings default_settings(&build_settings, "");
  EXPECT_EQ("//out/Debug/gen/",
            GetGenDirForSourceDir(
                &default_settings, SourceDir("//")).value());
  EXPECT_EQ("gen/",
            GetGenDirForSourceDirAsOutputFile(
                &default_settings, SourceDir("//")).value());

  EXPECT_EQ("//out/Debug/gen/foo/bar/",
            GetGenDirForSourceDir(
                &default_settings, SourceDir("//foo/bar/")).value());
  EXPECT_EQ("gen/foo/bar/",
            GetGenDirForSourceDirAsOutputFile(
                &default_settings, SourceDir("//foo/bar/")).value());

  // Secondary toolchain.
  Settings other_settings(&build_settings, "two/");
  EXPECT_EQ("//out/Debug/two/gen/",
            GetGenDirForSourceDir(
                &other_settings, SourceDir("//")).value());
  EXPECT_EQ("two/gen/",
            GetGenDirForSourceDirAsOutputFile(
                &other_settings, SourceDir("//")).value());

  EXPECT_EQ("//out/Debug/two/gen/foo/bar/",
            GetGenDirForSourceDir(
                &other_settings, SourceDir("//foo/bar/")).value());
  EXPECT_EQ("two/gen/foo/bar/",
            GetGenDirForSourceDirAsOutputFile(
                &other_settings, SourceDir("//foo/bar/")).value());
}

TEST(FilesystemUtils, GetTargetDirs) {
  BuildSettings build_settings;
  build_settings.SetBuildDir(SourceDir("//out/Debug/"));
  Settings settings(&build_settings, "");

  Target a(&settings, Label(SourceDir("//foo/bar/"), "baz"));
  EXPECT_EQ("//out/Debug/obj/foo/bar/", GetTargetOutputDir(&a).value());
  EXPECT_EQ("obj/foo/bar/", GetTargetOutputDirAsOutputFile(&a).value());
  EXPECT_EQ("//out/Debug/gen/foo/bar/", GetTargetGenDir(&a).value());
  EXPECT_EQ("gen/foo/bar/", GetTargetGenDirAsOutputFile(&a).value());
}

// Tests handling of output dirs when build dir is the same as the root.
TEST(FilesystemUtils, GetDirForEmptyBuildDir) {
  BuildSettings build_settings;
  build_settings.SetBuildDir(SourceDir("//"));
  Settings settings(&build_settings, "");

  EXPECT_EQ("//", GetToolchainOutputDir(&settings).value());
  EXPECT_EQ("//gen/", GetToolchainGenDir(&settings).value());
  EXPECT_EQ("gen/", GetToolchainGenDirAsOutputFile(&settings).value());
  EXPECT_EQ("//obj/",
            GetOutputDirForSourceDir(&settings, SourceDir("//")).value());
  EXPECT_EQ("obj/",
            GetOutputDirForSourceDirAsOutputFile(
                &settings, SourceDir("//")).value());
  EXPECT_EQ("gen/",
            GetGenDirForSourceDirAsOutputFile(
                &settings, SourceDir("//")).value());
}
