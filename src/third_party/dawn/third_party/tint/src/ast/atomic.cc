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

#include "src/ast/atomic.h"

#include "src/program_builder.h"

TINT_INSTANTIATE_TYPEINFO(tint::ast::Atomic);

namespace tint {
namespace ast {

Atomic::Atomic(ProgramID program_id, const Source& source, Type* const subtype)
    : Base(program_id, source), subtype_(subtype) {}

std::string Atomic::type_name() const {
  std::ostringstream out;
  out << "__atomic" << subtype_->type_name();
  return out.str();
}

std::string Atomic::FriendlyName(const SymbolTable& symbols) const {
  std::ostringstream out;
  out << "atomic<" << subtype_->FriendlyName(symbols) << ">";
  return out.str();
}

Atomic::Atomic(Atomic&&) = default;

Atomic::~Atomic() = default;

Atomic* Atomic::Clone(CloneContext* ctx) const {
  // Clone arguments outside of create() call to have deterministic ordering
  auto src = ctx->Clone(source());
  auto* ty = ctx->Clone(type());
  return ctx->dst->create<Atomic>(src, ty);
}

}  // namespace ast
}  // namespace tint
