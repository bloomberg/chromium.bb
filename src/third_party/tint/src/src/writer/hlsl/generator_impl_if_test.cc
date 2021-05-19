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

#include "src/writer/hlsl/test_helper.h"

namespace tint {
namespace writer {
namespace hlsl {
namespace {

using HlslGeneratorImplTest_If = TestHelper;

TEST_F(HlslGeneratorImplTest_If, Emit_If) {
  auto* cond = Expr("cond");
  auto* body = create<ast::BlockStatement>(ast::StatementList{
      create<ast::ReturnStatement>(),
  });
  auto* i = create<ast::IfStatement>(cond, body, ast::ElseStatementList{});

  GeneratorImpl& gen = Build();

  gen.increment_indent();
  ASSERT_TRUE(gen.EmitStatement(out, i)) << gen.error();
  EXPECT_EQ(result(), R"(  if (cond) {
    return;
  }
)");
}

TEST_F(HlslGeneratorImplTest_If, Emit_IfWithElseIf) {
  auto* else_cond = Expr("else_cond");
  auto* else_body = create<ast::BlockStatement>(ast::StatementList{
      create<ast::ReturnStatement>(),
  });

  auto* cond = Expr("cond");
  auto* body = create<ast::BlockStatement>(ast::StatementList{
      create<ast::ReturnStatement>(),
  });
  auto* i = create<ast::IfStatement>(
      cond, body,
      ast::ElseStatementList{create<ast::ElseStatement>(else_cond, else_body)});

  GeneratorImpl& gen = Build();

  gen.increment_indent();

  ASSERT_TRUE(gen.EmitStatement(out, i)) << gen.error();
  EXPECT_EQ(result(), R"(  if (cond) {
    return;
  } else {
    if (else_cond) {
      return;
    }
  }
)");
}

TEST_F(HlslGeneratorImplTest_If, Emit_IfWithElse) {
  auto* else_body = create<ast::BlockStatement>(ast::StatementList{
      create<ast::ReturnStatement>(),
  });

  auto* cond = Expr("cond");
  auto* body = create<ast::BlockStatement>(ast::StatementList{
      create<ast::ReturnStatement>(),
  });
  auto* i = create<ast::IfStatement>(
      cond, body,
      ast::ElseStatementList{create<ast::ElseStatement>(nullptr, else_body)});

  GeneratorImpl& gen = Build();

  gen.increment_indent();

  ASSERT_TRUE(gen.EmitStatement(out, i)) << gen.error();
  EXPECT_EQ(result(), R"(  if (cond) {
    return;
  } else {
    return;
  }
)");
}

TEST_F(HlslGeneratorImplTest_If, Emit_IfWithMultiple) {
  auto* else_cond = Expr("else_cond");

  auto* else_body = create<ast::BlockStatement>(ast::StatementList{
      create<ast::ReturnStatement>(),
  });

  auto* else_body_2 = create<ast::BlockStatement>(ast::StatementList{
      create<ast::ReturnStatement>(),
  });

  auto* cond = Expr("cond");
  auto* body = create<ast::BlockStatement>(ast::StatementList{
      create<ast::ReturnStatement>(),
  });
  auto* i = create<ast::IfStatement>(
      cond, body,
      ast::ElseStatementList{
          create<ast::ElseStatement>(else_cond, else_body),
          create<ast::ElseStatement>(nullptr, else_body_2),
      });

  GeneratorImpl& gen = Build();

  gen.increment_indent();

  ASSERT_TRUE(gen.EmitStatement(out, i)) << gen.error();
  EXPECT_EQ(result(), R"(  if (cond) {
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
}  // namespace hlsl
}  // namespace writer
}  // namespace tint
