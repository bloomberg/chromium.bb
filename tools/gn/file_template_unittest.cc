// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sstream>

#include "testing/gtest/include/gtest/gtest.h"
#include "tools/gn/escape.h"
#include "tools/gn/file_template.h"
#include "tools/gn/test_with_scope.h"

TEST(FileTemplate, Static) {
  TestWithScope setup;

  std::vector<std::string> templates;
  templates.push_back("something_static");
  FileTemplate t(setup.settings(), templates,
                 FileTemplate::OUTPUT_ABSOLUTE, SourceDir("//"));
  EXPECT_FALSE(t.has_substitutions());

  std::vector<std::string> result;
  t.Apply(SourceFile("//foo/bar"), &result);
  ASSERT_EQ(1u, result.size());
  EXPECT_EQ("something_static", result[0]);
}

TEST(FileTemplate, Typical) {
  TestWithScope setup;

  std::vector<std::string> templates;
  templates.push_back("foo/{{source_name_part}}.cc");
  templates.push_back("foo/{{source_name_part}}.h");
  FileTemplate t(setup.settings(), templates,
                 FileTemplate::OUTPUT_ABSOLUTE, SourceDir("//"));
  EXPECT_TRUE(t.has_substitutions());

  std::vector<std::string> result;
  t.Apply(SourceFile("//sources/ha.idl"), &result);
  ASSERT_EQ(2u, result.size());
  EXPECT_EQ("foo/ha.cc", result[0]);
  EXPECT_EQ("foo/ha.h", result[1]);
}

TEST(FileTemplate, Weird) {
  TestWithScope setup;

  std::vector<std::string> templates;
  templates.push_back("{{{source}}{{source}}{{");
  FileTemplate t(setup.settings(), templates,
                 FileTemplate::OUTPUT_RELATIVE,
                 setup.settings()->build_settings()->build_dir());
  EXPECT_TRUE(t.has_substitutions());

  std::vector<std::string> result;
  t.Apply(SourceFile("//foo/lalala.c"), &result);
  ASSERT_EQ(1u, result.size());
  EXPECT_EQ("{../../foo/lalala.c../../foo/lalala.c{{", result[0]);
}

TEST(FileTemplate, NinjaExpansions) {
  TestWithScope setup;

  std::vector<std::string> templates;
  templates.push_back("-i");
  templates.push_back("{{source}}");
  templates.push_back("--out=foo bar\"{{source_name_part}}\".o");
  templates.push_back("");  // Test empty string.

  FileTemplate t(setup.settings(), templates,
                 FileTemplate::OUTPUT_RELATIVE,
                 setup.settings()->build_settings()->build_dir());

  std::ostringstream out;
  t.WriteWithNinjaExpansions(out);

#if defined(OS_WIN)
  // The third argument should get quoted since it contains a space, and the
  // embedded quotes should get shell escaped.
  EXPECT_EQ(
      " -i ${source} \"--out=foo$ bar\\\"${source_name_part}\\\".o\" \"\"",
      out.str());
#else
  // On Posix we don't need to quote the whole thing and can escape all
  // individual bad chars.
  EXPECT_EQ(
      " -i ${source} --out=foo\\$ bar\\\"${source_name_part}\\\".o \"\"",
      out.str());
#endif
}

TEST(FileTemplate, NinjaVariables) {
  TestWithScope setup;

  std::vector<std::string> templates;
  templates.push_back("-i");
  templates.push_back("{{source}}");
  templates.push_back("--out=foo bar\"{{source_name_part}}\".o");
  templates.push_back("{{source_file_part}}");
  templates.push_back("{{source_dir}}");
  templates.push_back("{{source_root_relative_dir}}");
  templates.push_back("{{source_gen_dir}}");
  templates.push_back("{{source_out_dir}}");

  FileTemplate t(setup.settings(), templates,
                 FileTemplate::OUTPUT_RELATIVE,
                 setup.settings()->build_settings()->build_dir());

  std::ostringstream out;
  EscapeOptions options;
  options.mode = ESCAPE_NINJA_COMMAND;
  t.WriteNinjaVariablesForSubstitution(out, SourceFile("//foo/bar.txt"),
                                       options);

  // Just the variables used above should be written.
  EXPECT_EQ(
      "  source = ../../foo/bar.txt\n"
      "  source_name_part = bar\n"
      "  source_file_part = bar.txt\n"
      "  source_dir = ../../foo\n"
      "  source_root_rel_dir = foo\n"
      "  source_gen_dir = gen/foo\n"
      "  source_out_dir = obj/foo\n",
      out.str());
}

// Tests in isolation different types of substitutions and that the right
// things are generated.
TEST(FileTemplate, Substitutions) {
  TestWithScope setup;

  // Call to get substitutions relative to the build dir.
  #define GetRelSubst(str, what) \
      FileTemplate::GetSubstitution( \
          setup.settings(), \
          SourceFile(str), \
          FileTemplate::Subrange::what, \
          FileTemplate::OUTPUT_RELATIVE, \
          setup.settings()->build_settings()->build_dir())

  // Call to get absolute directory substitutions.
  #define GetAbsSubst(str, what) \
      FileTemplate::GetSubstitution( \
          setup.settings(), \
          SourceFile(str), \
          FileTemplate::Subrange::what, \
          FileTemplate::OUTPUT_ABSOLUTE, \
          SourceDir())

  // Try all possible templates with a normal looking string.
  EXPECT_EQ("../../foo/bar/baz.txt", GetRelSubst("//foo/bar/baz.txt", SOURCE));
  EXPECT_EQ("//foo/bar/baz.txt", GetAbsSubst("//foo/bar/baz.txt", SOURCE));

  EXPECT_EQ("baz", GetRelSubst("//foo/bar/baz.txt", NAME_PART));
  EXPECT_EQ("baz", GetAbsSubst("//foo/bar/baz.txt", NAME_PART));

  EXPECT_EQ("baz.txt", GetRelSubst("//foo/bar/baz.txt", FILE_PART));
  EXPECT_EQ("baz.txt", GetAbsSubst("//foo/bar/baz.txt", FILE_PART));

  EXPECT_EQ("../../foo/bar", GetRelSubst("//foo/bar/baz.txt", SOURCE_DIR));
  EXPECT_EQ("//foo/bar", GetAbsSubst("//foo/bar/baz.txt", SOURCE_DIR));

  EXPECT_EQ("foo/bar", GetRelSubst("//foo/bar/baz.txt", ROOT_RELATIVE_DIR));
  EXPECT_EQ("foo/bar", GetAbsSubst("//foo/bar/baz.txt", ROOT_RELATIVE_DIR));

  EXPECT_EQ("gen/foo/bar", GetRelSubst("//foo/bar/baz.txt", SOURCE_GEN_DIR));
  EXPECT_EQ("//out/Debug/gen/foo/bar",
            GetAbsSubst("//foo/bar/baz.txt", SOURCE_GEN_DIR));

  EXPECT_EQ("obj/foo/bar", GetRelSubst("//foo/bar/baz.txt", SOURCE_OUT_DIR));
  EXPECT_EQ("//out/Debug/obj/foo/bar",
            GetAbsSubst("//foo/bar/baz.txt", SOURCE_OUT_DIR));

  // Operations on an absolute path.
  EXPECT_EQ("/baz.txt", GetRelSubst("/baz.txt", SOURCE));
  EXPECT_EQ("/.", GetRelSubst("/baz.txt", SOURCE_DIR));
  EXPECT_EQ("gen", GetRelSubst("/baz.txt", SOURCE_GEN_DIR));
  EXPECT_EQ("obj", GetRelSubst("/baz.txt", SOURCE_OUT_DIR));

  EXPECT_EQ(".", GetRelSubst("//baz.txt", ROOT_RELATIVE_DIR));

  #undef GetAbsSubst
  #undef GetRelSubst
}
