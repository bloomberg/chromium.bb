// Copyright 2020 The Tint Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "src/reader/wgsl/parser_impl_test_helper.h"

namespace tint {
namespace reader {
namespace wgsl {
namespace {

TEST_F(ParserImplTest, ArgumentExpressionList_Parses) {
  auto p = parser("a");
  auto e = p->expect_argument_expression_list();
  ASSERT_FALSE(p->has_error()) << p->error();
  ASSERT_FALSE(e.errored);

  ASSERT_EQ(e.value.size(), 1u);
  ASSERT_TRUE(e.value[0]->Is<ast::IdentifierExpression>());
}

TEST_F(ParserImplTest, ArgumentExpressionList_ParsesMultiple) {
  auto p = parser("a, -33, 1+2");
  auto e = p->expect_argument_expression_list();
  ASSERT_FALSE(p->has_error()) << p->error();
  ASSERT_FALSE(e.errored);

  ASSERT_EQ(e.value.size(), 3u);
  ASSERT_TRUE(e.value[0]->Is<ast::IdentifierExpression>());
  ASSERT_TRUE(e.value[1]->Is<ast::ConstructorExpression>());
  ASSERT_TRUE(e.value[2]->Is<ast::BinaryExpression>());
}

TEST_F(ParserImplTest, ArgumentExpressionList_HandlesMissingExpression) {
  auto p = parser("a, ");
  auto e = p->expect_argument_expression_list();
  ASSERT_TRUE(p->has_error());
  ASSERT_TRUE(e.errored);
  EXPECT_EQ(p->error(), "1:4: unable to parse argument expression after comma");
}

TEST_F(ParserImplTest, ArgumentExpressionList_HandlesInvalidExpression) {
  auto p = parser("if(a) {}");
  auto e = p->expect_argument_expression_list();
  ASSERT_TRUE(p->has_error());
  ASSERT_TRUE(e.errored);
  EXPECT_EQ(p->error(), "1:1: unable to parse argument expression");
}

}  // namespace
}  // namespace wgsl
}  // namespace reader
}  // namespace tint
