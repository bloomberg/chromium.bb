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

#include "src/type/access_control_type.h"

#include "src/program_builder.h"

TINT_INSTANTIATE_TYPEINFO(tint::type::AccessControl);

namespace tint {
namespace type {

AccessControl::AccessControl(ast::AccessControl access, Type* subtype)
    : access_(access), subtype_(subtype) {
  TINT_ASSERT(subtype_);
  TINT_ASSERT(!subtype_->Is<AccessControl>());
}

AccessControl::AccessControl(AccessControl&&) = default;

AccessControl::~AccessControl() = default;

std::string AccessControl::type_name() const {
  std::string name = "__access_control_";
  switch (access_) {
    case ast::AccessControl::kReadOnly:
      name += "read_only";
      break;
    case ast::AccessControl::kWriteOnly:
      name += "write_only";
      break;
    case ast::AccessControl::kReadWrite:
      name += "read_write";
      break;
  }
  return name + subtype_->type_name();
}

std::string AccessControl::FriendlyName(const SymbolTable& symbols) const {
  std::ostringstream out;
  out << "[[access(";
  switch (access_) {
    case ast::AccessControl::kReadOnly:
      out << "read";
      break;
    case ast::AccessControl::kWriteOnly:
      out << "write";
      break;
    case ast::AccessControl::kReadWrite:
      out << "read_write";
      break;
  }
  out << ")]] " << subtype_->FriendlyName(symbols);
  return out.str();
}

AccessControl* AccessControl::Clone(CloneContext* ctx) const {
  // Clone arguments outside of create() call to have deterministic ordering
  auto* ty = ctx->Clone(type());
  return ctx->dst->create<AccessControl>(access_, ty);
}

}  // namespace type
}  // namespace tint
