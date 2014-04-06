// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "tools/gn/functions.h"
#include "tools/gn/parse_tree.h"
#include "tools/gn/test_with_scope.h"
#include "tools/gn/value.h"

TEST(Functions, Defined) {
  TestWithScope setup;

  FunctionCallNode function_call;
  Err err;

  // Test an undefined identifier.
  Token undefined_token(Location(), Token::IDENTIFIER, "undef");
  ListNode args_list_identifier_undefined;
  args_list_identifier_undefined.append_item(
      scoped_ptr<ParseNode>(new IdentifierNode(undefined_token)));
  Value result = functions::RunDefined(setup.scope(), &function_call,
                                       &args_list_identifier_undefined, &err);
  ASSERT_EQ(Value::BOOLEAN, result.type());
  EXPECT_FALSE(result.boolean_value());

  // Define a value that's itself a scope value.
  const char kDef[] = "def";  // Defined variable name.
  setup.scope()->SetValue(kDef,
      Value(NULL, scoped_ptr<Scope>(new Scope(setup.scope()))),
      NULL);

  // Test the defined identifier.
  Token defined_token(Location(), Token::IDENTIFIER, kDef);
  ListNode args_list_identifier_defined;
  args_list_identifier_defined.append_item(
      scoped_ptr<ParseNode>(new IdentifierNode(defined_token)));
  result = functions::RunDefined(setup.scope(), &function_call,
                                 &args_list_identifier_defined, &err);
  ASSERT_EQ(Value::BOOLEAN, result.type());
  EXPECT_TRUE(result.boolean_value());

  // Should also work by passing an accessor node so you can do
  // "defined(def.foo)" to see if foo is defined on the def scope.
  scoped_ptr<AccessorNode> undef_accessor(new AccessorNode);
  undef_accessor->set_base(defined_token);
  undef_accessor->set_member(scoped_ptr<IdentifierNode>(
      new IdentifierNode(undefined_token)));
  ListNode args_list_accessor_defined;
  args_list_accessor_defined.append_item(undef_accessor.PassAs<ParseNode>());
  result = functions::RunDefined(setup.scope(), &function_call,
                                 &args_list_accessor_defined, &err);
  ASSERT_EQ(Value::BOOLEAN, result.type());
  EXPECT_FALSE(result.boolean_value());
}
