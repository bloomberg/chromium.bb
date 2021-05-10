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

#include "src/ast/variable_decl_statement.h"

#include "src/clone_context.h"
#include "src/program_builder.h"

TINT_INSTANTIATE_CLASS_ID(tint::ast::VariableDeclStatement);

namespace tint {
namespace ast {

VariableDeclStatement::VariableDeclStatement(const Source& source,
                                             Variable* variable)
    : Base(source), variable_(variable) {}

VariableDeclStatement::VariableDeclStatement(VariableDeclStatement&&) = default;

VariableDeclStatement::~VariableDeclStatement() = default;

VariableDeclStatement* VariableDeclStatement::Clone(CloneContext* ctx) const {
  // Clone arguments outside of create() call to have deterministic ordering
  auto src = ctx->Clone(source());
  auto* var = ctx->Clone(variable());
  return ctx->dst->create<VariableDeclStatement>(src, var);
}

bool VariableDeclStatement::IsValid() const {
  return variable_ != nullptr && variable_->IsValid();
}

void VariableDeclStatement::to_str(const semantic::Info& sem,
                                   std::ostream& out,
                                   size_t indent) const {
  make_indent(out, indent);
  out << "VariableDeclStatement{" << std::endl;
  variable_->to_str(sem, out, indent + 2);
  make_indent(out, indent);
  out << "}" << std::endl;
}

}  // namespace ast
}  // namespace tint
