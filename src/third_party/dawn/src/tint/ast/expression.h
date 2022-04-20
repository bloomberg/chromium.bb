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

#ifndef SRC_TINT_AST_EXPRESSION_H_
#define SRC_TINT_AST_EXPRESSION_H_

#include <string>
#include <vector>

#include "src/tint/ast/node.h"
#include "src/tint/sem/type.h"

namespace tint::ast {

/// Base expression class
class Expression : public Castable<Expression, Node> {
 public:
  ~Expression() override;

 protected:
  /// Constructor
  /// @param pid the identifier of the program that owns this node
  /// @param src the source of this node
  Expression(ProgramID pid, const Source& src);
  /// Move constructor
  Expression(Expression&&);
};

/// A list of expressions
using ExpressionList = std::vector<const Expression*>;

}  // namespace tint::ast

#endif  // SRC_TINT_AST_EXPRESSION_H_
