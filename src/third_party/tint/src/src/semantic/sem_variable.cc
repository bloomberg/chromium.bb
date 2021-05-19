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

#include "src/semantic/variable.h"

#include "src/ast/identifier_expression.h"

TINT_INSTANTIATE_TYPEINFO(tint::semantic::Variable);
TINT_INSTANTIATE_TYPEINFO(tint::semantic::VariableUser);

namespace tint {
namespace semantic {

Variable::Variable(const ast::Variable* declaration,
                   type::Type* type,
                   ast::StorageClass storage_class)
    : declaration_(declaration), type_(type), storage_class_(storage_class) {}

Variable::~Variable() = default;

VariableUser::VariableUser(ast::IdentifierExpression* declaration,
                           type::Type* type,
                           Statement* statement,
                           semantic::Variable* variable)
    : Base(declaration, type, statement), variable_(variable) {}

}  // namespace semantic
}  // namespace tint
