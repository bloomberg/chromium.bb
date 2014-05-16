// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "tools/gn/input_file.h"
#include "tools/gn/parse_tree.h"
#include "tools/gn/scope.h"
#include "tools/gn/test_with_scope.h"

TEST(ParseTree, Accessor) {
  TestWithScope setup;

  // Make a pretend parse node with proper tracking that we can blame for the
  // given value.
  InputFile input_file(SourceFile("//foo"));
  Token base_token(Location(&input_file, 1, 1), Token::IDENTIFIER, "a");
  Token member_token(Location(&input_file, 1, 1), Token::IDENTIFIER, "b");

  AccessorNode accessor;
  accessor.set_base(base_token);

  scoped_ptr<IdentifierNode> member_identifier(
      new IdentifierNode(member_token));
  accessor.set_member(member_identifier.Pass());

  // The access should fail because a is not defined.
  Err err;
  Value result = accessor.Execute(setup.scope(), &err);
  EXPECT_TRUE(err.has_error());
  EXPECT_EQ(Value::NONE, result.type());

  // Define a as a Scope. It should still fail because b isn't defined.
  err = Err();
  setup.scope()->SetValue("a",
      Value(NULL, scoped_ptr<Scope>(new Scope(setup.scope()))), NULL);
  result = accessor.Execute(setup.scope(), &err);
  EXPECT_TRUE(err.has_error());
  EXPECT_EQ(Value::NONE, result.type());

  // Define b, accessor should succeed now.
  const int64 kBValue = 42;
  err = Err();
  setup.scope()->GetMutableValue("a", false)->scope_value()->SetValue(
      "b", Value(NULL, kBValue), NULL);
  result = accessor.Execute(setup.scope(), &err);
  EXPECT_FALSE(err.has_error());
  ASSERT_EQ(Value::INTEGER, result.type());
  EXPECT_EQ(kBValue, result.int_value());
}

TEST(ParseTree, BlockUnusedVars) {
  TestWithScope setup;

  // Printing both values should be OK.
  TestParseInput input_all_used(
      "{\n"
      "  a = 12\n"
      "  b = 13\n"
      "  print(\"$a $b\")\n"
      "}");
  EXPECT_FALSE(input_all_used.has_error());

  Err err;
  input_all_used.parsed()->Execute(setup.scope(), &err);
  EXPECT_FALSE(err.has_error());

  // Skipping one should throw an unused var error.
  TestParseInput input_unused(
      "{\n"
      "  a = 12\n"
      "  b = 13\n"
      "  print(\"$a\")\n"
      "}");
  EXPECT_FALSE(input_unused.has_error());

  input_unused.parsed()->Execute(setup.scope(), &err);
  EXPECT_TRUE(err.has_error());
}
