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

#include "src/tint/ast/if_statement.h"

#include "src/tint/program_builder.h"

TINT_INSTANTIATE_TYPEINFO(tint::ast::IfStatement);

namespace tint::ast {

IfStatement::IfStatement(ProgramID pid,
                         const Source& src,
                         const Expression* cond,
                         const BlockStatement* b,
                         ElseStatementList else_stmts)
    : Base(pid, src),
      condition(cond),
      body(b),
      else_statements(std::move(else_stmts)) {
  TINT_ASSERT(AST, condition);
  TINT_ASSERT_PROGRAM_IDS_EQUAL_IF_VALID(AST, condition, program_id);
  TINT_ASSERT(AST, body);
  TINT_ASSERT_PROGRAM_IDS_EQUAL_IF_VALID(AST, body, program_id);
  for (auto* el : else_statements) {
    TINT_ASSERT(AST, el);
    TINT_ASSERT_PROGRAM_IDS_EQUAL_IF_VALID(AST, el, program_id);
  }
}

IfStatement::IfStatement(IfStatement&&) = default;

IfStatement::~IfStatement() = default;

const IfStatement* IfStatement::Clone(CloneContext* ctx) const {
  // Clone arguments outside of create() call to have deterministic ordering
  auto src = ctx->Clone(source);
  auto* cond = ctx->Clone(condition);
  auto* b = ctx->Clone(body);
  auto el = ctx->Clone(else_statements);
  return ctx->dst->create<IfStatement>(src, cond, b, el);
}

}  // namespace tint::ast
