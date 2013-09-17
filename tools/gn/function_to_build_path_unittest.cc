// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "tools/gn/functions.h"
#include "tools/gn/parse_tree.h"
#include "tools/gn/test_with_scope.h"

namespace {

void ExpectCallEqualsString(Scope* scope,
                            const std::string& input,
                            const std::string& expected) {
  std::vector<Value> args;
  args.push_back(Value(NULL, input));

  Err err;
  FunctionCallNode function;
  Value result = functions::RunToBuildPath(scope, &function, args, &err);

  EXPECT_FALSE(err.has_error());
  EXPECT_EQ(Value::STRING, result.type());
  EXPECT_EQ(expected, result.string_value());
}

}  // namespace

TEST(ToBuildPath, Strings) {
  TestWithScope setup;
  setup.build_settings()->SetBuildDir(SourceDir("//out/Debug/"));

  Scope* scope = setup.scope();
  scope->set_source_dir(SourceDir("//tools/gn/"));

  // Absolute system paths are unchanged.
  ExpectCallEqualsString(scope, "/", "/");
  ExpectCallEqualsString(scope, "/foo", "/foo");
  ExpectCallEqualsString(scope, "/foo/", "/foo");
  ExpectCallEqualsString(scope, "/foo/bar.txt", "/foo/bar.txt");

  // Build-file relative paths.
  ExpectCallEqualsString(scope, ".", "../../tools/gn");
  ExpectCallEqualsString(scope, "foo.txt", "../../tools/gn/foo.txt");
  ExpectCallEqualsString(scope, "..", "../../tools");
  ExpectCallEqualsString(scope, "../", "../../tools");
  ExpectCallEqualsString(scope, "../foo.txt", "../../tools/foo.txt");

  // Source-root paths.
  ExpectCallEqualsString(scope, "//", "../..");
  ExpectCallEqualsString(scope, "//foo/", "../../foo");
  ExpectCallEqualsString(scope, "//foo.txt", "../../foo.txt");
  ExpectCallEqualsString(scope, "//foo/bar.txt", "../../foo/bar.txt");

  // Source-root paths. It might be nice if we detected the strings start
  // with the build dir and collapse, but this is the current behavior.
  ExpectCallEqualsString(scope, "//out/Debug/",
                         "../../out/Debug");  // Could be ".".
  ExpectCallEqualsString(scope, "//out/Debug/foo/",
                         "../../out/Debug/foo");  // Could be "foo".
}

// Test list input.
TEST(ToBuildPath, List) {
  TestWithScope setup;
  setup.build_settings()->SetBuildDir(SourceDir("//out/Debug/"));
  setup.scope()->set_source_dir(SourceDir("//tools/gn/"));

  std::vector<Value> args;
  args.push_back(Value(NULL, Value::LIST));
  args[0].list_value().push_back(Value(NULL, "foo.txt"));
  args[0].list_value().push_back(Value(NULL, "bar.txt"));

  Err err;
  FunctionCallNode function;
  Value ret = functions::RunToBuildPath(setup.scope(), &function, args, &err);
  EXPECT_FALSE(err.has_error());

  ASSERT_EQ(Value::LIST, ret.type());
  ASSERT_EQ(2u, ret.list_value().size());

  EXPECT_EQ("../../tools/gn/foo.txt", ret.list_value()[0].string_value());
  EXPECT_EQ("../../tools/gn/bar.txt", ret.list_value()[1].string_value());
}

TEST(ToBuildPath, Errors) {
  TestWithScope setup;
  setup.build_settings()->SetBuildDir(SourceDir("//out/Debug/"));

  // No arg input should issue an error.
  Err err;
  std::vector<Value> args;
  FunctionCallNode function;
  Value ret = functions::RunToBuildPath(setup.scope(), &function, args, &err);
  EXPECT_TRUE(err.has_error());

  // One arg int input.
  args.push_back(Value(NULL, static_cast<int64>(5)));
  err = Err();
  ret = functions::RunToBuildPath(setup.scope(), &function, args, &err);
  EXPECT_TRUE(err.has_error());

  // Two arg string input.
  args.clear();
  args.push_back(Value(NULL, "hello"));
  args.push_back(Value(NULL, "world"));
  err = Err();
  ret = functions::RunToBuildPath(setup.scope(), &function, args, &err);
  EXPECT_TRUE(err.has_error());
}
