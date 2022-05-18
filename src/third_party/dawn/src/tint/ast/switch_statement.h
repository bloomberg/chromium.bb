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

#ifndef SRC_TINT_AST_SWITCH_STATEMENT_H_
#define SRC_TINT_AST_SWITCH_STATEMENT_H_

#include "src/tint/ast/case_statement.h"
#include "src/tint/ast/expression.h"

namespace tint::ast {

/// A switch statement
class SwitchStatement final : public Castable<SwitchStatement, Statement> {
  public:
    /// Constructor
    /// @param pid the identifier of the program that owns this node
    /// @param src the source of this node
    /// @param condition the switch condition
    /// @param body the switch body
    SwitchStatement(ProgramID pid,
                    const Source& src,
                    const Expression* condition,
                    CaseStatementList body);
    /// Move constructor
    SwitchStatement(SwitchStatement&&);
    ~SwitchStatement() override;

    /// @returns true if this is a default statement
    bool IsDefault() const { return condition == nullptr; }

    /// Clones this node and all transitive child nodes using the `CloneContext`
    /// `ctx`.
    /// @param ctx the clone context
    /// @return the newly cloned node
    const SwitchStatement* Clone(CloneContext* ctx) const override;

    /// The switch condition or nullptr if none set
    const Expression* const condition;

    /// The Switch body
    const CaseStatementList body;
    SwitchStatement(const SwitchStatement&) = delete;
};

}  // namespace tint::ast

#endif  // SRC_TINT_AST_SWITCH_STATEMENT_H_
