// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "tools/gn/test_with_scope.h"

TEST(Template, Basic) {
  TestWithScope setup;
  TestParseInput input(
      "template(\"foo\") {\n"
      "  print(target_name)\n"
      "  print(invoker.bar)\n"
      "}\n"
      "foo(\"lala\") {\n"
      "  bar = 42\n"
      "}");
  ASSERT_FALSE(input.has_error());

  Err err;
  input.parsed()->Execute(setup.scope(), &err);
  ASSERT_FALSE(err.has_error()) << err.message();

  EXPECT_EQ("lala\n42\n", setup.print_output());
}

TEST(Template, UnusedTargetNameShouldThrowError) {
  TestWithScope setup;
  TestParseInput input(
      "template(\"foo\") {\n"
      "  print(invoker.bar)\n"
      "}\n"
      "foo(\"lala\") {\n"
      "  bar = 42\n"
      "}");
  ASSERT_FALSE(input.has_error());

  Err err;
  input.parsed()->Execute(setup.scope(), &err);
  EXPECT_TRUE(err.has_error());
}

TEST(Template, UnusedInvokerShouldThrowError) {
  TestWithScope setup;
  TestParseInput input(
      "template(\"foo\") {\n"
      "  print(target_name)\n"
      "}\n"
      "foo(\"lala\") {\n"
      "  bar = 42\n"
      "}");
  ASSERT_FALSE(input.has_error());

  Err err;
  input.parsed()->Execute(setup.scope(), &err);
  EXPECT_TRUE(err.has_error());
}

TEST(Template, UnusedVarInInvokerShouldThrowError) {
  TestWithScope setup;
  TestParseInput input(
      "template(\"foo\") {\n"
      "  print(target_name)\n"
      "  print(invoker.bar)\n"
      "}\n"
      "foo(\"lala\") {\n"
      "  bar = 42\n"
      "  baz = [ \"foo\" ]\n"
      "}");
  ASSERT_FALSE(input.has_error());

  Err err;
  input.parsed()->Execute(setup.scope(), &err);
  EXPECT_TRUE(err.has_error());
}
