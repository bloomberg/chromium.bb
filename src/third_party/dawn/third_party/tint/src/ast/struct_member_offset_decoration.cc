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

#include "src/ast/struct_member_offset_decoration.h"

#include <string>

#include "src/program_builder.h"

TINT_INSTANTIATE_TYPEINFO(tint::ast::StructMemberOffsetDecoration);

namespace tint {
namespace ast {

StructMemberOffsetDecoration::StructMemberOffsetDecoration(ProgramID program_id,
                                                           const Source& source,
                                                           uint32_t offset)
    : Base(program_id, source), offset_(offset) {}

StructMemberOffsetDecoration::~StructMemberOffsetDecoration() = default;

std::string StructMemberOffsetDecoration::name() const {
  return "offset";
}

void StructMemberOffsetDecoration::to_str(const sem::Info&,
                                          std::ostream& out,
                                          size_t indent) const {
  make_indent(out, indent);
  out << "offset " << std::to_string(offset_);
}

StructMemberOffsetDecoration* StructMemberOffsetDecoration::Clone(
    CloneContext* ctx) const {
  // Clone arguments outside of create() call to have deterministic ordering
  auto src = ctx->Clone(source());
  return ctx->dst->create<StructMemberOffsetDecoration>(src, offset_);
}

}  // namespace ast
}  // namespace tint
