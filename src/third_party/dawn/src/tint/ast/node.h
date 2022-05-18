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

#ifndef SRC_TINT_AST_NODE_H_
#define SRC_TINT_AST_NODE_H_

#include <string>

#include "src/tint/clone_context.h"

// Forward declarations
namespace tint {
class CloneContext;
}  // namespace tint
namespace tint::sem {
class Type;
class Info;
}  // namespace tint::sem

namespace tint::ast {

/// AST base class node
class Node : public Castable<Node, Cloneable> {
  public:
    ~Node() override;

    /// The identifier of the program that owns this node
    const ProgramID program_id;

    /// The node source data
    const Source source;

  protected:
    /// Create a new node
    /// @param pid the identifier of the program that owns this node
    /// @param src the input source for the node
    Node(ProgramID pid, const Source& src);
    /// Move constructor
    Node(Node&&);

  private:
    Node(const Node&) = delete;
};

}  // namespace tint::ast

namespace tint {

/// @param node a pointer to an AST node
/// @returns the ProgramID of the given AST node.
inline ProgramID ProgramIDOf(const ast::Node* node) {
    return node ? node->program_id : ProgramID();
}

}  // namespace tint

#endif  // SRC_TINT_AST_NODE_H_
