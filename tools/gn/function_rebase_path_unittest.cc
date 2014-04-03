// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "tools/gn/functions.h"
#include "tools/gn/parse_tree.h"
#include "tools/gn/test_with_scope.h"

namespace {

std::string RebaseOne(Scope* scope,
                      const char* input,
                      const char* to_dir,
                      const char* from_dir) {
  std::vector<Value> args;
  args.push_back(Value(NULL, input));
  args.push_back(Value(NULL, to_dir));
  args.push_back(Value(NULL, from_dir));

  Err err;
  FunctionCallNode function;
  Value result = functions::RunRebasePath(scope, &function, args, &err);
  bool is_string = result.type() == Value::STRING;
  EXPECT_TRUE(is_string);

  return result.string_value();
}

}  // namespace

TEST(RebasePath, Strings) {
  TestWithScope setup;
  setup.build_settings()->SetBuildDir(SourceDir("//out/Debug/"));
  Scope* scope = setup.scope();
  scope->set_source_dir(SourceDir("//tools/gn/"));

  // Build-file relative paths.
  EXPECT_EQ("../../tools/gn", RebaseOne(scope, ".", "//out/Debug", "."));
  EXPECT_EQ("../../tools/gn/", RebaseOne(scope, "./", "//out/Debug", "."));
  EXPECT_EQ("../../tools/gn/foo", RebaseOne(scope, "foo", "//out/Debug", "."));
  EXPECT_EQ("../..", RebaseOne(scope, "../..", "//out/Debug", "."));
  EXPECT_EQ("../../", RebaseOne(scope, "../../", "//out/Debug", "."));

  // We don't allow going above the root source dir.
  EXPECT_EQ("../..", RebaseOne(scope, "../../..", "//out/Debug", "."));

  // Source-absolute input paths.
  EXPECT_EQ("./", RebaseOne(scope, "//", "//", "//"));
  EXPECT_EQ("foo", RebaseOne(scope, "//foo", "//", "//"));
  EXPECT_EQ("foo/", RebaseOne(scope, "//foo/", "//", "//"));
  EXPECT_EQ("../../foo/bar", RebaseOne(scope, "//foo/bar", "//out/Debug", "."));
  EXPECT_EQ("./", RebaseOne(scope, "//foo/", "//foo/", "//"));
  // Thie one is technically correct but could be simplified to "." if
  // necessary.
  EXPECT_EQ("../foo", RebaseOne(scope, "//foo", "//foo", "//"));

  // Test slash conversion.
  EXPECT_EQ("foo/bar", RebaseOne(scope, "foo/bar", ".", "."));
  EXPECT_EQ("foo/bar", RebaseOne(scope, "foo\\bar", ".", "."));

  // Test system path output.
#if defined(OS_WIN)
  setup.build_settings()->SetRootPath(base::FilePath(L"C:/source"));
  EXPECT_EQ("C:/source", RebaseOne(scope, ".", "", "//"));
  EXPECT_EQ("C:/source/", RebaseOne(scope, "//", "", "//"));
  EXPECT_EQ("C:/source/foo", RebaseOne(scope, "foo", "", "//"));
  EXPECT_EQ("C:/source/foo/", RebaseOne(scope, "foo/", "", "//"));
  EXPECT_EQ("C:/source/tools/gn/foo", RebaseOne(scope, "foo", "", "."));
#else
  setup.build_settings()->SetRootPath(base::FilePath("/source"));
  EXPECT_EQ("/source", RebaseOne(scope, ".", "", "//"));
  EXPECT_EQ("/source/", RebaseOne(scope, "//", "", "//"));
  EXPECT_EQ("/source/foo", RebaseOne(scope, "foo", "", "//"));
  EXPECT_EQ("/source/foo/", RebaseOne(scope, "foo/", "", "//"));
  EXPECT_EQ("/source/tools/gn/foo", RebaseOne(scope, "foo", "", "."));
#endif
}

// Test list input.
TEST(RebasePath, List) {
  TestWithScope setup;
  setup.build_settings()->SetBuildDir(SourceDir("//out/Debug/"));
  setup.scope()->set_source_dir(SourceDir("//tools/gn/"));

  std::vector<Value> args;
  args.push_back(Value(NULL, Value::LIST));
  args[0].list_value().push_back(Value(NULL, "foo.txt"));
  args[0].list_value().push_back(Value(NULL, "bar.txt"));
  args.push_back(Value(NULL, "//out/Debug/"));
  args.push_back(Value(NULL, "."));

  Err err;
  FunctionCallNode function;
  Value ret = functions::RunRebasePath(setup.scope(), &function, args, &err);
  EXPECT_FALSE(err.has_error());

  ASSERT_EQ(Value::LIST, ret.type());
  ASSERT_EQ(2u, ret.list_value().size());

  EXPECT_EQ("../../tools/gn/foo.txt", ret.list_value()[0].string_value());
  EXPECT_EQ("../../tools/gn/bar.txt", ret.list_value()[1].string_value());
}

TEST(RebasePath, Errors) {
  TestWithScope setup;
  setup.build_settings()->SetBuildDir(SourceDir("//out/Debug/"));

  // No arg input should issue an error.
  Err err;
  std::vector<Value> args;
  FunctionCallNode function;
  Value ret = functions::RunRebasePath(setup.scope(), &function, args, &err);
  EXPECT_TRUE(err.has_error());
}
