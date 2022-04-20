// Copyright 2022 The Tint Authors.
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

#include "src/tint/ast/increment_decrement_statement.h"

#include "src/tint/program_builder.h"

TINT_INSTANTIATE_TYPEINFO(tint::ast::IncrementDecrementStatement);

namespace tint::ast {

IncrementDecrementStatement::IncrementDecrementStatement(ProgramID pid,
                                                         const Source& src,
                                                         const Expression* l,
                                                         bool inc)
    : Base(pid, src), lhs(l), increment(inc) {
  TINT_ASSERT_PROGRAM_IDS_EQUAL_IF_VALID(AST, lhs, program_id);
}

IncrementDecrementStatement::IncrementDecrementStatement(
    IncrementDecrementStatement&&) = default;

IncrementDecrementStatement::~IncrementDecrementStatement() = default;

const IncrementDecrementStatement* IncrementDecrementStatement::Clone(
    CloneContext* ctx) const {
  // Clone arguments outside of create() call to have deterministic ordering
  auto src = ctx->Clone(source);
  auto* l = ctx->Clone(lhs);
  return ctx->dst->create<IncrementDecrementStatement>(src, l, increment);
}

}  // namespace tint::ast
