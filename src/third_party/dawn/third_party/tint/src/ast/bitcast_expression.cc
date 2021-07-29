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

#include "src/ast/bitcast_expression.h"

#include "src/program_builder.h"

TINT_INSTANTIATE_TYPEINFO(tint::ast::BitcastExpression);

namespace tint {
namespace ast {

BitcastExpression::BitcastExpression(ProgramID program_id,
                                     const Source& source,
                                     ast::Type* type,
                                     Expression* expr)
    : Base(program_id, source), type_(type), expr_(expr) {
  TINT_ASSERT(AST, type_);
  TINT_ASSERT(AST, expr_);
  TINT_ASSERT_PROGRAM_IDS_EQUAL_IF_VALID(AST, expr, program_id);
}

BitcastExpression::BitcastExpression(BitcastExpression&&) = default;
BitcastExpression::~BitcastExpression() = default;

BitcastExpression* BitcastExpression::Clone(CloneContext* ctx) const {
  // Clone arguments outside of create() call to have deterministic ordering
  auto src = ctx->Clone(source());
  auto* ty = ctx->Clone(type());
  auto* e = ctx->Clone(expr_);
  return ctx->dst->create<BitcastExpression>(src, ty, e);
}

void BitcastExpression::to_str(const sem::Info& sem,
                               std::ostream& out,
                               size_t indent) const {
  make_indent(out, indent);
  out << "Bitcast[" << result_type_str(sem) << "]<" << type_->type_name()
      << ">{" << std::endl;
  expr_->to_str(sem, out, indent + 2);
  make_indent(out, indent);
  out << "}" << std::endl;
}

}  // namespace ast
}  // namespace tint
