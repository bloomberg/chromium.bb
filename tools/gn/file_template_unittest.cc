// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sstream>

#include "testing/gtest/include/gtest/gtest.h"
#include "tools/gn/escape.h"
#include "tools/gn/file_template.h"

TEST(FileTemplate, Static) {
  std::vector<std::string> templates;
  templates.push_back("something_static");
  FileTemplate t(templates);
  EXPECT_FALSE(t.has_substitutions());

  std::vector<std::string> result;
  t.ApplyString("", &result);
  ASSERT_EQ(1u, result.size());
  EXPECT_EQ("something_static", result[0]);

  t.ApplyString("lalala", &result);
  ASSERT_EQ(1u, result.size());
  EXPECT_EQ("something_static", result[0]);
}

TEST(FileTemplate, Typical) {
  std::vector<std::string> templates;
  templates.push_back("foo/{{source_name_part}}.cc");
  templates.push_back("foo/{{source_name_part}}.h");
  FileTemplate t(templates);
  EXPECT_TRUE(t.has_substitutions());

  std::vector<std::string> result;
  t.ApplyString("sources/ha.idl", &result);
  ASSERT_EQ(2u, result.size());
  EXPECT_EQ("foo/ha.cc", result[0]);
  EXPECT_EQ("foo/ha.h", result[1]);
}

TEST(FileTemplate, Weird) {
  std::vector<std::string> templates;
  templates.push_back("{{{source}}{{source}}{{");
  FileTemplate t(templates);
  EXPECT_TRUE(t.has_substitutions());

  std::vector<std::string> result;
  t.ApplyString("foo/lalala.c", &result);
  ASSERT_EQ(1u, result.size());
  EXPECT_EQ("{foo/lalala.cfoo/lalala.c{{", result[0]);
}

TEST(FileTemplate, NinjaExpansions) {
  std::vector<std::string> templates;
  templates.push_back("-i");
  templates.push_back("{{source}}");
  templates.push_back("--out=foo bar\"{{source_name_part}}\".o");

  FileTemplate t(templates);

  std::ostringstream out;
  t.WriteWithNinjaExpansions(out);

  // The third argument should get quoted since it contains a space, and the
  // embedded quotes should get shell escaped.
  EXPECT_EQ(
      " -i ${source} \"--out=foo$ bar\\\"${source_name_part}\\\".o\"",
      out.str());
}

TEST(FileTemplate, NinjaVariables) {
  std::vector<std::string> templates;
  templates.push_back("-i");
  templates.push_back("{{source}}");
  templates.push_back("--out=foo bar\"{{source_name_part}}\".o");

  FileTemplate t(templates);

  std::ostringstream out;
  EscapeOptions options;
  options.mode = ESCAPE_NINJA_SHELL;
  t.WriteNinjaVariablesForSubstitution(out, "../../foo/bar.txt", options);

  // Just the variables used above should be written.
  EXPECT_EQ(
      "  source = ../../foo/bar.txt\n"
      "  source_name_part = bar\n",
      out.str());
}
