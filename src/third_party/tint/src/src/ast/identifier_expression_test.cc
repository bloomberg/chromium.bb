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

#include "src/ast/identifier_expression.h"

#include "gtest/gtest.h"

namespace tint {
namespace ast {
namespace {

using IdentifierExpressionTest = testing::Test;

TEST_F(IdentifierExpressionTest, Creation) {
  IdentifierExpression i("ident");
  EXPECT_EQ(i.name(), "ident");
}

TEST_F(IdentifierExpressionTest, Creation_WithSource) {
  IdentifierExpression i(Source{Source::Location{20, 2}}, "ident");
  EXPECT_EQ(i.name(), "ident");

  auto src = i.source();
  EXPECT_EQ(src.range.begin.line, 20u);
  EXPECT_EQ(src.range.begin.column, 2u);
}

TEST_F(IdentifierExpressionTest, IsIdentifier) {
  IdentifierExpression i("ident");
  EXPECT_TRUE(i.IsIdentifier());
}

TEST_F(IdentifierExpressionTest, IsValid) {
  IdentifierExpression i("ident");
  EXPECT_TRUE(i.IsValid());
}

TEST_F(IdentifierExpressionTest, IsValid_BlankName) {
  IdentifierExpression i("");
  EXPECT_FALSE(i.IsValid());
}

TEST_F(IdentifierExpressionTest, ToStr) {
  IdentifierExpression i("ident");
  std::ostringstream out;
  i.to_str(out, 2);
  EXPECT_EQ(out.str(), R"(  Identifier{ident}
)");
}

}  // namespace
}  // namespace ast
}  // namespace tint
