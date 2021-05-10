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

#ifndef SRC_AST_STRUCT_MEMBER_DECORATION_H_
#define SRC_AST_STRUCT_MEMBER_DECORATION_H_

#include <vector>

#include "src/ast/decoration.h"

namespace tint {
namespace ast {

/// A decoration attached to a struct member
class StructMemberDecoration
    : public Castable<StructMemberDecoration, Decoration> {
 public:
  /// The kind of decoration that this type represents
  static constexpr const DecorationKind Kind = DecorationKind::kStructMember;

  ~StructMemberDecoration() override;

  /// @return the decoration kind
  DecorationKind GetKind() const override;

 protected:
  /// Constructor
  /// @param source the source of this decoration
  explicit StructMemberDecoration(const Source& source);
};

/// A list of struct member decorations
using StructMemberDecorationList = std::vector<StructMemberDecoration*>;

}  // namespace ast
}  // namespace tint

#endif  // SRC_AST_STRUCT_MEMBER_DECORATION_H_
