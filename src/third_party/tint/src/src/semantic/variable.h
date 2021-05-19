// Copyright 2021 The Tint Authors.
//
// Licensed under the Apache License, Version 2.0(the "License");
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

#ifndef SRC_SEMANTIC_VARIABLE_H_
#define SRC_SEMANTIC_VARIABLE_H_

#include <vector>

#include "src/ast/storage_class.h"
#include "src/semantic/expression.h"

namespace tint {

// Forward declarations
namespace ast {
class IdentifierExpression;
class Variable;
}  // namespace ast
namespace type {
class Type;
}  // namespace type

namespace semantic {

class VariableUser;

/// Variable holds the semantic information for variables.
class Variable : public Castable<Variable, Node> {
 public:
  /// Constructor
  /// @param declaration the AST declaration node
  /// @param type the variable type
  /// @param storage_class the variable storage class
  Variable(const ast::Variable* declaration,
           type::Type* type,
           ast::StorageClass storage_class);

  /// Destructor
  ~Variable() override;

  /// @returns the AST declaration node
  const ast::Variable* Declaration() const { return declaration_; }

  /// @returns the type for the variable
  type::Type* Type() const { return type_; }

  /// @returns the storage class for the variable
  ast::StorageClass StorageClass() const { return storage_class_; }

  /// @returns the expressions that use the variable
  const std::vector<const VariableUser*>& Users() const { return users_; }

  /// @param user the user to add
  void AddUser(const VariableUser* user) { users_.emplace_back(user); }

 private:
  const ast::Variable* const declaration_;
  type::Type* const type_;
  ast::StorageClass const storage_class_;
  std::vector<const VariableUser*> users_;
};

/// VariableUser holds the semantic information for an identifier expression
/// node that resolves to a variable.
class VariableUser : public Castable<VariableUser, Expression> {
 public:
  /// Constructor
  /// @param declaration the AST identifier node
  /// @param type the resolved type of the expression
  /// @param statement the statement that owns this expression
  /// @param variable the semantic variable
  VariableUser(ast::IdentifierExpression* declaration,
               type::Type* type,
               Statement* statement,
               semantic::Variable* variable);

  /// @returns the variable that this expression refers to
  const semantic::Variable* Variable() const { return variable_; }

 private:
  semantic::Variable const* const variable_;
};

}  // namespace semantic
}  // namespace tint

#endif  // SRC_SEMANTIC_VARIABLE_H_
