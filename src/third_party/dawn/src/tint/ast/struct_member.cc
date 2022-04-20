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

#include "src/tint/ast/struct_member.h"

#include "src/tint/program_builder.h"

TINT_INSTANTIATE_TYPEINFO(tint::ast::StructMember);

namespace tint::ast {

StructMember::StructMember(ProgramID pid,
                           const Source& src,
                           const Symbol& sym,
                           const ast::Type* ty,
                           AttributeList attrs)
    : Base(pid, src), symbol(sym), type(ty), attributes(std::move(attrs)) {
  TINT_ASSERT(AST, type);
  TINT_ASSERT(AST, symbol.IsValid());
  TINT_ASSERT_PROGRAM_IDS_EQUAL_IF_VALID(AST, symbol, program_id);
  for (auto* attr : attributes) {
    TINT_ASSERT(AST, attr);
    TINT_ASSERT_PROGRAM_IDS_EQUAL_IF_VALID(AST, attr, program_id);
  }
}

StructMember::StructMember(StructMember&&) = default;

StructMember::~StructMember() = default;

const StructMember* StructMember::Clone(CloneContext* ctx) const {
  // Clone arguments outside of create() call to have deterministic ordering
  auto src = ctx->Clone(source);
  auto sym = ctx->Clone(symbol);
  auto* ty = ctx->Clone(type);
  auto attrs = ctx->Clone(attributes);
  return ctx->dst->create<StructMember>(src, sym, ty, attrs);
}

}  // namespace tint::ast
