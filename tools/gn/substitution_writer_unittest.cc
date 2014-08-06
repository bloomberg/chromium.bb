// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sstream>

#include "testing/gtest/include/gtest/gtest.h"
#include "tools/gn/err.h"
#include "tools/gn/escape.h"
#include "tools/gn/substitution_pattern.h"
#include "tools/gn/substitution_writer.h"
#include "tools/gn/test_with_scope.h"

TEST(SubstitutionWriter, ApplyPatternToSource) {
  TestWithScope setup;

  SubstitutionPattern pattern;
  Err err;
  ASSERT_TRUE(pattern.Parse("{{source_gen_dir}}/{{source_name_part}}.tmp",
                            NULL, &err));

  SourceFile result = SubstitutionWriter::ApplyPatternToSource(
      setup.settings(), pattern, SourceFile("//foo/bar/myfile.txt"));
  ASSERT_EQ("//out/Debug/gen/foo/bar/myfile.tmp", result.value());
}

TEST(SubstitutionWriter, ApplyPatternToSourceAsOutputFile) {
  TestWithScope setup;

  SubstitutionPattern pattern;
  Err err;
  ASSERT_TRUE(pattern.Parse("{{source_gen_dir}}/{{source_name_part}}.tmp",
                            NULL, &err));

  OutputFile result = SubstitutionWriter::ApplyPatternToSourceAsOutputFile(
      setup.settings(), pattern, SourceFile("//foo/bar/myfile.txt"));
  ASSERT_EQ("gen/foo/bar/myfile.tmp", result.value());
}

TEST(SubstutitionWriter, WriteNinjaVariablesForSource) {
  TestWithScope setup;

  std::vector<SubstitutionType> types;
  types.push_back(SUBSTITUTION_SOURCE);
  types.push_back(SUBSTITUTION_SOURCE_NAME_PART);
  types.push_back(SUBSTITUTION_SOURCE_DIR);

  EscapeOptions options;
  options.mode = ESCAPE_NONE;

  std::ostringstream out;
  SubstitutionWriter::WriteNinjaVariablesForSource(
      setup.settings(), SourceFile("//foo/bar/baz.txt"), types, options, out);

  // The "source" should be skipped since that will expand to $in which is
  // implicit.
  EXPECT_EQ(
      "  source_name_part = baz\n"
      "  source_dir = ../../foo/bar\n",
      out.str());
}

TEST(SubstitutionWriter, WriteWithNinjaVariables) {
  Err err;
  SubstitutionPattern pattern;
  ASSERT_TRUE(pattern.Parse(
      "-i {{source}} --out=bar\"{{source_name_part}}\".o",
      NULL, &err));
  EXPECT_FALSE(err.has_error());

  EscapeOptions options;
  options.mode = ESCAPE_NONE;

  std::ostringstream out;
  SubstitutionWriter::WriteWithNinjaVariables(pattern, options, out);

  EXPECT_EQ(
      "-i ${in} --out=bar\"${source_name_part}\".o",
      out.str());
}

// Tests in isolation different types of substitutions and that the right
// things are generated.
TEST(SubstutitionWriter, Substitutions) {
  TestWithScope setup;

  // Call to get substitutions relative to the build dir.
  #define GetRelSubst(str, what) \
      SubstitutionWriter::GetSourceSubstitution( \
          setup.settings(), \
          SourceFile(str), \
          what, \
          SubstitutionWriter::OUTPUT_RELATIVE, \
          setup.settings()->build_settings()->build_dir())

  // Call to get absolute directory substitutions.
  #define GetAbsSubst(str, what) \
      SubstitutionWriter::GetSourceSubstitution( \
          setup.settings(), \
          SourceFile(str), \
          what, \
          SubstitutionWriter::OUTPUT_ABSOLUTE, \
          SourceDir())

  // Try all possible templates with a normal looking string.
  EXPECT_EQ("../../foo/bar/baz.txt",
            GetRelSubst("//foo/bar/baz.txt", SUBSTITUTION_SOURCE));
  EXPECT_EQ("//foo/bar/baz.txt",
            GetAbsSubst("//foo/bar/baz.txt", SUBSTITUTION_SOURCE));

  EXPECT_EQ("baz",
            GetRelSubst("//foo/bar/baz.txt", SUBSTITUTION_SOURCE_NAME_PART));
  EXPECT_EQ("baz",
            GetAbsSubst("//foo/bar/baz.txt", SUBSTITUTION_SOURCE_NAME_PART));

  EXPECT_EQ("baz.txt",
            GetRelSubst("//foo/bar/baz.txt", SUBSTITUTION_SOURCE_FILE_PART));
  EXPECT_EQ("baz.txt",
            GetAbsSubst("//foo/bar/baz.txt", SUBSTITUTION_SOURCE_FILE_PART));

  EXPECT_EQ("../../foo/bar",
            GetRelSubst("//foo/bar/baz.txt", SUBSTITUTION_SOURCE_DIR));
  EXPECT_EQ("//foo/bar",
            GetAbsSubst("//foo/bar/baz.txt", SUBSTITUTION_SOURCE_DIR));

  EXPECT_EQ("foo/bar", GetRelSubst("//foo/bar/baz.txt",
                                   SUBSTITUTION_SOURCE_ROOT_RELATIVE_DIR));
  EXPECT_EQ("foo/bar", GetAbsSubst("//foo/bar/baz.txt",
                                   SUBSTITUTION_SOURCE_ROOT_RELATIVE_DIR));

  EXPECT_EQ("gen/foo/bar",
            GetRelSubst("//foo/bar/baz.txt", SUBSTITUTION_SOURCE_GEN_DIR));
  EXPECT_EQ("//out/Debug/gen/foo/bar",
            GetAbsSubst("//foo/bar/baz.txt", SUBSTITUTION_SOURCE_GEN_DIR));

  EXPECT_EQ("obj/foo/bar",
            GetRelSubst("//foo/bar/baz.txt", SUBSTITUTION_SOURCE_OUT_DIR));
  EXPECT_EQ("//out/Debug/obj/foo/bar",
            GetAbsSubst("//foo/bar/baz.txt", SUBSTITUTION_SOURCE_OUT_DIR));

  // Operations on an absolute path.
  EXPECT_EQ("/baz.txt", GetRelSubst("/baz.txt", SUBSTITUTION_SOURCE));
  EXPECT_EQ("/.", GetRelSubst("/baz.txt", SUBSTITUTION_SOURCE_DIR));
  EXPECT_EQ("gen", GetRelSubst("/baz.txt", SUBSTITUTION_SOURCE_GEN_DIR));
  EXPECT_EQ("obj", GetRelSubst("/baz.txt", SUBSTITUTION_SOURCE_OUT_DIR));

  EXPECT_EQ(".",
            GetRelSubst("//baz.txt", SUBSTITUTION_SOURCE_ROOT_RELATIVE_DIR));

  #undef GetAbsSubst
  #undef GetRelSubst
}
