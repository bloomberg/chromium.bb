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

#include "src/ast/scalar_constructor_expression.h"

#include "src/clone_context.h"
#include "src/program_builder.h"

TINT_INSTANTIATE_CLASS_ID(tint::ast::ScalarConstructorExpression);

namespace tint {
namespace ast {

ScalarConstructorExpression::ScalarConstructorExpression(const Source& source,
                                                         Literal* litearl)
    : Base(source), literal_(litearl) {}

ScalarConstructorExpression::ScalarConstructorExpression(
    ScalarConstructorExpression&&) = default;

ScalarConstructorExpression::~ScalarConstructorExpression() = default;

ScalarConstructorExpression* ScalarConstructorExpression::Clone(
    CloneContext* ctx) const {
  // Clone arguments outside of create() call to have deterministic ordering
  auto src = ctx->Clone(source());
  auto* lit = ctx->Clone(literal());
  return ctx->dst->create<ScalarConstructorExpression>(src, lit);
}

bool ScalarConstructorExpression::IsValid() const {
  return literal_ != nullptr;
}

void ScalarConstructorExpression::to_str(const semantic::Info& sem,
                                         std::ostream& out,
                                         size_t indent) const {
  make_indent(out, indent);
  out << "ScalarConstructor[" << result_type_str(sem) << "]{"
      << literal_->to_str(sem) << "}" << std::endl;
}

}  // namespace ast
}  // namespace tint
