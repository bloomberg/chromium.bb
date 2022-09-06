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

#ifndef SRC_TINT_AST_TYPE_DECL_H_
#define SRC_TINT_AST_TYPE_DECL_H_

#include <string>

#include "src/tint/ast/type.h"

namespace tint::ast {

/// The base class for type declarations.
class TypeDecl : public Castable<TypeDecl, Node> {
  public:
    /// Create a new struct statement
    /// @param pid the identifier of the program that owns this node
    /// @param src the source of this node for the import statement
    /// @param name The name of the structure
    TypeDecl(ProgramID pid, const Source& src, Symbol name);
    /// Move constructor
    TypeDecl(TypeDecl&&);

    ~TypeDecl() override;

    /// The name of the type declaration
    const Symbol name;
};

}  // namespace tint::ast

#endif  // SRC_TINT_AST_TYPE_DECL_H_
