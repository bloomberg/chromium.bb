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

#include "src/tint/ast/for_loop_statement.h"

#include "src/tint/program_builder.h"

TINT_INSTANTIATE_TYPEINFO(tint::ast::ForLoopStatement);

namespace tint::ast {

ForLoopStatement::ForLoopStatement(ProgramID pid,
                                   const Source& src,
                                   const Statement* init,
                                   const Expression* cond,
                                   const Statement* cont,
                                   const BlockStatement* b)
    : Base(pid, src),
      initializer(init),
      condition(cond),
      continuing(cont),
      body(b) {
  TINT_ASSERT(AST, body);

  TINT_ASSERT_PROGRAM_IDS_EQUAL_IF_VALID(AST, initializer, program_id);
  TINT_ASSERT_PROGRAM_IDS_EQUAL_IF_VALID(AST, condition, program_id);
  TINT_ASSERT_PROGRAM_IDS_EQUAL_IF_VALID(AST, continuing, program_id);
  TINT_ASSERT_PROGRAM_IDS_EQUAL_IF_VALID(AST, body, program_id);
}

ForLoopStatement::ForLoopStatement(ForLoopStatement&&) = default;

ForLoopStatement::~ForLoopStatement() = default;

const ForLoopStatement* ForLoopStatement::Clone(CloneContext* ctx) const {
  // Clone arguments outside of create() call to have deterministic ordering
  auto src = ctx->Clone(source);

  auto* init = ctx->Clone(initializer);
  auto* cond = ctx->Clone(condition);
  auto* cont = ctx->Clone(continuing);
  auto* b = ctx->Clone(body);
  return ctx->dst->create<ForLoopStatement>(src, init, cond, cont, b);
}

}  // namespace tint::ast
