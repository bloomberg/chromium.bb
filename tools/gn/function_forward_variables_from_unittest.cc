// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "tools/gn/scheduler.h"
#include "tools/gn/test_with_scope.h"

TEST(FunctionForwardVariablesFrom, List) {
  Scheduler scheduler;
  TestWithScope setup;

  // Defines a template and copy the two x and y, and z values out.
  TestParseInput input(
    "template(\"a\") {\n"
    "  forward_variables_from(invoker, [\"x\", \"y\", \"z\"])\n"
    "  assert(!defined(z))\n"  // "z" should still be undefined.
    "  print(\"$target_name, $x, $y\")\n"
    "}\n"
    "a(\"target\") {\n"
    "  x = 1\n"
    "  y = 2\n"
    "}\n");

  ASSERT_FALSE(input.has_error());

  Err err;
  input.parsed()->Execute(setup.scope(), &err);
  ASSERT_FALSE(err.has_error()) << err.message();

  EXPECT_EQ("target, 1, 2\n", setup.print_output());
  setup.print_output().clear();
}

TEST(FunctionForwardVariablesFrom, ErrorCases) {
  Scheduler scheduler;
  TestWithScope setup;

  // Type check the source scope.
  TestParseInput invalid_source(
    "template(\"a\") {\n"
    "  forward_variables_from(42, [\"x\"])\n"
    "  print(\"$target_name\")\n"  // Prevent unused var error.
    "}\n"
    "a(\"target\") {\n"
    "}\n");
  ASSERT_FALSE(invalid_source.has_error());
  Err err;
  invalid_source.parsed()->Execute(setup.scope(), &err);
  EXPECT_TRUE(err.has_error());
  EXPECT_EQ("Expected an identifier for the scope.", err.message());

  // Type check the list. We need to use a new template name each time since
  // all of these invocations are executing in sequence in the same scope.
  TestParseInput invalid_list(
    "template(\"b\") {\n"
    "  forward_variables_from(invoker, 42)\n"
    "  print(\"$target_name\")\n"
    "}\n"
    "b(\"target\") {\n"
    "}\n");
  ASSERT_FALSE(invalid_list.has_error());
  err = Err();
  invalid_list.parsed()->Execute(setup.scope(), &err);
  EXPECT_TRUE(err.has_error());
  EXPECT_EQ("Not a valid list of variables to copy.", err.message());

  // Programmatic values should error.
  TestParseInput prog(
    "template(\"c\") {\n"
    "  forward_variables_from(invoker, [\"root_out_dir\"])\n"
    "  print(\"$target_name\")\n"
    "}\n"
    "c(\"target\") {\n"
    "}\n");
  ASSERT_FALSE(prog.has_error());
  err = Err();
  prog.parsed()->Execute(setup.scope(), &err);
  EXPECT_TRUE(err.has_error());
  EXPECT_EQ("This value can't be forwarded.", err.message());
}

TEST(FunctionForwardVariablesFrom, Star) {
  Scheduler scheduler;
  TestWithScope setup;

  // Defines a template and copy the two x and y values out. The "*" behavior
  // should clobber existing variables with the same name.
  TestParseInput input(
    "template(\"a\") {\n"
    "  x = 1000000\n"  // Should be clobbered.
    "  forward_variables_from(invoker, \"*\")\n"
    "  print(\"$target_name, $x, $y\")\n"
    "}\n"
    "a(\"target\") {\n"
    "  x = 1\n"
    "  y = 2\n"
    "}\n");

  ASSERT_FALSE(input.has_error());

  Err err;
  input.parsed()->Execute(setup.scope(), &err);
  ASSERT_FALSE(err.has_error()) << err.message();

  EXPECT_EQ("target, 1, 2\n", setup.print_output());
  setup.print_output().clear();
}
