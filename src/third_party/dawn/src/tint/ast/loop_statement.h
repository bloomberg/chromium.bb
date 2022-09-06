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

#ifndef SRC_TINT_AST_LOOP_STATEMENT_H_
#define SRC_TINT_AST_LOOP_STATEMENT_H_

#include "src/tint/ast/block_statement.h"

namespace tint::ast {

/// A loop statement
class LoopStatement final : public Castable<LoopStatement, Statement> {
  public:
    /// Constructor
    /// @param program_id the identifier of the program that owns this node
    /// @param source the loop statement source
    /// @param body the body statements
    /// @param continuing the continuing statements
    LoopStatement(ProgramID program_id,
                  const Source& source,
                  const BlockStatement* body,
                  const BlockStatement* continuing);
    /// Move constructor
    LoopStatement(LoopStatement&&);
    ~LoopStatement() override;

    /// Clones this node and all transitive child nodes using the `CloneContext`
    /// `ctx`.
    /// @param ctx the clone context
    /// @return the newly cloned node
    const LoopStatement* Clone(CloneContext* ctx) const override;

    /// The loop body
    const BlockStatement* const body;

    /// The continuing statements
    const BlockStatement* const continuing;
};

}  // namespace tint::ast

#endif  // SRC_TINT_AST_LOOP_STATEMENT_H_
