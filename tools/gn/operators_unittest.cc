// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "tools/gn/operators.h"
#include "tools/gn/parse_tree.h"
#include "tools/gn/pattern.h"
#include "tools/gn/test_with_scope.h"

namespace {

bool IsValueIntegerEqualing(const Value& v, int64 i) {
  if (v.type() != Value::INTEGER)
    return false;
  return v.int_value() == i;
}

bool IsValueStringEqualing(const Value& v, const char* s) {
  if (v.type() != Value::STRING)
    return false;
  return v.string_value() == s;
}

}  // namespace

TEST(Operators, SourcesAppend) {
  Err err;
  TestWithScope setup;

  // Set up "sources" with an empty list.
  const char sources[] = "sources";
  setup.scope()->SetValue(sources, Value(NULL, Value::LIST), NULL);

  // Set up the operator.
  BinaryOpNode node;
  const char token_value[] = "+=";
  Token op(Location(), Token::PLUS_EQUALS, token_value);
  node.set_op(op);

  // Append to the sources variable.
  Token identifier_token(Location(), Token::IDENTIFIER, sources);
  node.set_left(scoped_ptr<ParseNode>(new IdentifierNode(identifier_token)));

  // Set up the filter on the scope to remove everything ending with "rm"
  scoped_ptr<PatternList> pattern_list(new PatternList);
  pattern_list->Append(Pattern("*rm"));
  setup.scope()->set_sources_assignment_filter(pattern_list.Pass());

  // Append an integer.
  const char integer_value[] = "5";
  Token integer(Location(), Token::INTEGER, integer_value);
  node.set_right(scoped_ptr<ParseNode>(new LiteralNode(integer)));
  node.Execute(setup.scope(), &err);
  EXPECT_FALSE(err.has_error());

  // Append a string that doesn't match the pattern, it should get appended.
  const char string_1_value[] = "\"good\"";
  Token string_1(Location(), Token::STRING, string_1_value);
  node.set_right(scoped_ptr<ParseNode>(new LiteralNode(string_1)));
  node.Execute(setup.scope(), &err);
  EXPECT_FALSE(err.has_error());

  // Append a string that does match the pattern, it should be a no-op.
  const char string_2_value[] = "\"foo-rm\"";
  Token string_2(Location(), Token::STRING, string_2_value);
  node.set_right(scoped_ptr<ParseNode>(new LiteralNode(string_2)));
  node.Execute(setup.scope(), &err);
  EXPECT_FALSE(err.has_error());

  // Append a list with the two strings from above.
  ListNode list;
  list.append_item(scoped_ptr<ParseNode>(new LiteralNode(string_1)));
  list.append_item(scoped_ptr<ParseNode>(new LiteralNode(string_2)));
  ExecuteBinaryOperator(setup.scope(), &node, node.left(), &list, &err);
  EXPECT_FALSE(err.has_error());

  // The sources variable in the scope should now have: [ 5, "good", "good" ]
  const Value* value = setup.scope()->GetValue(sources);
  ASSERT_TRUE(value);
  ASSERT_EQ(Value::LIST, value->type());
  ASSERT_EQ(3u, value->list_value().size());
  EXPECT_TRUE(IsValueIntegerEqualing(value->list_value()[0], 5));
  EXPECT_TRUE(IsValueStringEqualing(value->list_value()[1], "good"));
  EXPECT_TRUE(IsValueStringEqualing(value->list_value()[2], "good"));
}
