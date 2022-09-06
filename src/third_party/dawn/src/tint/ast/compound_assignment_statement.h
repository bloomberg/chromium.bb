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

#ifndef SRC_TINT_AST_COMPOUND_ASSIGNMENT_STATEMENT_H_
#define SRC_TINT_AST_COMPOUND_ASSIGNMENT_STATEMENT_H_

#include "src/tint/ast/binary_expression.h"
#include "src/tint/ast/expression.h"
#include "src/tint/ast/statement.h"

namespace tint::ast {

/// A compound assignment statement
class CompoundAssignmentStatement final : public Castable<CompoundAssignmentStatement, Statement> {
  public:
    /// Constructor
    /// @param program_id the identifier of the program that owns this node
    /// @param source the compound assignment statement source
    /// @param lhs the left side of the expression
    /// @param rhs the right side of the expression
    /// @param op the binary operator
    CompoundAssignmentStatement(ProgramID program_id,
                                const Source& source,
                                const Expression* lhs,
                                const Expression* rhs,
                                BinaryOp op);
    /// Move constructor
    CompoundAssignmentStatement(CompoundAssignmentStatement&&);
    ~CompoundAssignmentStatement() override;

    /// Clones this node and all transitive child nodes using the `CloneContext`
    /// `ctx`.
    /// @param ctx the clone context
    /// @return the newly cloned node
    const CompoundAssignmentStatement* Clone(CloneContext* ctx) const override;

    /// left side expression
    const Expression* const lhs;

    /// right side expression
    const Expression* const rhs;

    /// the binary operator
    const BinaryOp op;
};

}  // namespace tint::ast

#endif  // SRC_TINT_AST_COMPOUND_ASSIGNMENT_STATEMENT_H_
