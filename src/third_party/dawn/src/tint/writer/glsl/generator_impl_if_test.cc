// Copyright 2021 The Tint Authors.
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

#include "src/tint/writer/glsl/test_helper.h"

namespace tint::writer::glsl {
namespace {

using GlslGeneratorImplTest_If = TestHelper;

TEST_F(GlslGeneratorImplTest_If, Emit_If) {
  Global("cond", ty.bool_(), ast::StorageClass::kPrivate);

  auto* cond = Expr("cond");
  auto* body = Block(Return());
  auto* i = If(cond, body);
  WrapInFunction(i);

  GeneratorImpl& gen = Build();

  gen.increment_indent();
  ASSERT_TRUE(gen.EmitStatement(i)) << gen.error();
  EXPECT_EQ(gen.result(), R"(  if (cond) {
    return;
  }
)");
}

TEST_F(GlslGeneratorImplTest_If, Emit_IfWithElseIf) {
  Global("cond", ty.bool_(), ast::StorageClass::kPrivate);
  Global("else_cond", ty.bool_(), ast::StorageClass::kPrivate);

  auto* else_cond = Expr("else_cond");
  auto* else_body = Block(Return());

  auto* cond = Expr("cond");
  auto* body = Block(Return());
  auto* i = If(
      cond, body,
      ast::ElseStatementList{create<ast::ElseStatement>(else_cond, else_body)});
  WrapInFunction(i);

  GeneratorImpl& gen = Build();

  gen.increment_indent();

  ASSERT_TRUE(gen.EmitStatement(i)) << gen.error();
  EXPECT_EQ(gen.result(), R"(  if (cond) {
    return;
  } else {
    if (else_cond) {
      return;
    }
  }
)");
}

TEST_F(GlslGeneratorImplTest_If, Emit_IfWithElse) {
  Global("cond", ty.bool_(), ast::StorageClass::kPrivate);

  auto* else_body = Block(Return());

  auto* cond = Expr("cond");
  auto* body = Block(Return());
  auto* i = If(
      cond, body,
      ast::ElseStatementList{create<ast::ElseStatement>(nullptr, else_body)});
  WrapInFunction(i);

  GeneratorImpl& gen = Build();

  gen.increment_indent();

  ASSERT_TRUE(gen.EmitStatement(i)) << gen.error();
  EXPECT_EQ(gen.result(), R"(  if (cond) {
    return;
  } else {
    return;
  }
)");
}

TEST_F(GlslGeneratorImplTest_If, Emit_IfWithMultiple) {
  Global("cond", ty.bool_(), ast::StorageClass::kPrivate);
  Global("else_cond", ty.bool_(), ast::StorageClass::kPrivate);

  auto* else_cond = Expr("else_cond");

  auto* else_body = Block(Return());

  auto* else_body_2 = Block(Return());

  auto* cond = Expr("cond");
  auto* body = Block(Return());
  auto* i = If(cond, body,
               ast::ElseStatementList{
                   create<ast::ElseStatement>(else_cond, else_body),
                   create<ast::ElseStatement>(nullptr, else_body_2),
               });
  WrapInFunction(i);

  GeneratorImpl& gen = Build();

  gen.increment_indent();

  ASSERT_TRUE(gen.EmitStatement(i)) << gen.error();
  EXPECT_EQ(gen.result(), R"(  if (cond) {
    return;
  } else {
    if (else_cond) {
      return;
    } else {
      return;
    }
  }
)");
}

}  // namespace
}  // namespace tint::writer::glsl
