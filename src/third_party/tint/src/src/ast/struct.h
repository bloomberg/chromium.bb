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

#ifndef SRC_AST_STRUCT_H_
#define SRC_AST_STRUCT_H_

#include <ostream>
#include <string>
#include <utility>

#include "src/ast/node.h"
#include "src/ast/struct_decoration.h"
#include "src/ast/struct_member.h"

namespace tint {
namespace ast {

/// A struct statement.
class Struct : public Node {
 public:
  /// Create a new empty struct statement
  Struct();
  /// Create a new struct statement
  /// @param members The struct members
  explicit Struct(StructMemberList members);
  /// Create a new struct statement
  /// @param decorations The struct decorations
  /// @param members The struct members
  Struct(StructDecorationList decorations, StructMemberList members);
  /// Create a new struct statement
  /// @param source The input source for the import statement
  /// @param members The struct members
  Struct(const Source& source, StructMemberList members);
  /// Create a new struct statement
  /// @param source The input source for the import statement
  /// @param decorations The struct decorations
  /// @param members The struct members
  Struct(const Source& source,
         StructDecorationList decorations,
         StructMemberList members);
  /// Move constructor
  Struct(Struct&&);

  ~Struct() override;

  /// Sets the struct decoration
  /// @param decos the list of decorations to set
  void set_decorations(StructDecorationList decos) {
    decorations_ = std::move(decos);
  }
  /// @returns the struct decorations
  const StructDecorationList& decorations() const { return decorations_; }

  /// Sets the struct members
  /// @param members the members to set
  void set_members(StructMemberList members) { members_ = std::move(members); }
  /// @returns the members
  const StructMemberList& members() const { return members_; }

  /// Returns the struct member with the given name or nullptr if non exists.
  /// @param name the name of the member
  /// @returns the struct member or nullptr if not found
  StructMember* get_member(const std::string& name) const;

  /// @returns true if the struct is block decorated
  bool IsBlockDecorated() const;

  /// @returns true if the node is valid
  bool IsValid() const override;

  /// Writes a representation of the node to the output stream
  /// @param out the stream to write to
  /// @param indent number of spaces to indent the node when writing
  void to_str(std::ostream& out, size_t indent) const override;

 private:
  Struct(const Struct&) = delete;

  StructDecorationList decorations_;
  StructMemberList members_;
};

}  // namespace ast
}  // namespace tint

#endif  // SRC_AST_STRUCT_H_
