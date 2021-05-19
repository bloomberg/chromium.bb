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

#ifndef SRC_AST_STRUCT_MEMBER_SIZE_DECORATION_H_
#define SRC_AST_STRUCT_MEMBER_SIZE_DECORATION_H_

#include <stddef.h>

#include "src/ast/decoration.h"

namespace tint {
namespace ast {

/// A struct member size decoration
class StructMemberSizeDecoration
    : public Castable<StructMemberSizeDecoration, Decoration> {
 public:
  /// constructor
  /// @param source the source of this decoration
  /// @param size the size value
  StructMemberSizeDecoration(const Source& source, uint32_t size);
  ~StructMemberSizeDecoration() override;

  /// @returns the size value
  uint32_t size() const { return size_; }

  /// Outputs the decoration to the given stream
  /// @param sem the semantic info for the program
  /// @param out the stream to write to
  /// @param indent number of spaces to indent the node when writing
  void to_str(const semantic::Info& sem,
              std::ostream& out,
              size_t indent) const override;

  /// Clones this node and all transitive child nodes using the `CloneContext`
  /// `ctx`.
  /// @param ctx the clone context
  /// @return the newly cloned node
  StructMemberSizeDecoration* Clone(CloneContext* ctx) const override;

 private:
  uint32_t const size_;
};

}  // namespace ast
}  // namespace tint

#endif  // SRC_AST_STRUCT_MEMBER_SIZE_DECORATION_H_
