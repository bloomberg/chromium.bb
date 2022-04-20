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

#include "src/tint/ast/struct_member_size_attribute.h"

#include <string>

#include "src/tint/clone_context.h"
#include "src/tint/program_builder.h"

TINT_INSTANTIATE_TYPEINFO(tint::ast::StructMemberSizeAttribute);

namespace tint::ast {

StructMemberSizeAttribute::StructMemberSizeAttribute(ProgramID pid,
                                                     const Source& src,
                                                     uint32_t sz)
    : Base(pid, src), size(sz) {}

StructMemberSizeAttribute::~StructMemberSizeAttribute() = default;

std::string StructMemberSizeAttribute::Name() const {
  return "size";
}

const StructMemberSizeAttribute* StructMemberSizeAttribute::Clone(
    CloneContext* ctx) const {
  // Clone arguments outside of create() call to have deterministic ordering
  auto src = ctx->Clone(source);
  return ctx->dst->create<StructMemberSizeAttribute>(src, size);
}

}  // namespace tint::ast
